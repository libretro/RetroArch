/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* DONE:
 * - Context creation (mostly)
 * TODO: 
 * - Shader code
 * - Texture reinitialization (16bpp support, etc)
 * - Viewports
 * - Implement video frame logic inbetween Begin/End
 * - Actually run and test this to make sure it does work
 */

#include "../psp/sdk_defines.h"
#include "../general.h"
#include "../driver.h"

#define MALLOC_PARAMS_FRAGMENT_FLAG (1 << 0)
#define MALLOC_PARAMS_VERTEX_FLAG   (1 << 1)

#define GXM_ALIGN(x, a)					(((x) + ((a) - 1)) & ~((a) - 1))

#define DISPLAY_BUFFER_COUNT 3
#define DISPLAY_BUFFER_SIZE (GXM_ALIGN(PSP_PITCH_PIXELS * PSP_FB_HEIGHT * 4, 1024 * 1024))
#define DISPLAY_MAX_PENDING_SWAPS 2

typedef struct vita_video
{
   SceGxmContext *gxm_ctx;
   void             *context_host_mem;
   void             *disp_buf_data[DISPLAY_BUFFER_COUNT];
   SceUID             disp_buf_uid[DISPLAY_BUFFER_COUNT];
   SceGxmColorSurface disp_surface[DISPLAY_BUFFER_COUNT];
   SceGxmSyncObject *disp_buf_sync[DISPLAY_BUFFER_COUNT];
   SceGxmShaderPatcher *shader_patcher;
   SceGxmRenderTarget *rt;
   SceUID vid_rb_uid;
   SceUID vtx_rb_uid;
   SceUID fp_rb_uid;
   SceUID patcher_buf_id;
   SceUID patcher_vertex_usse_uid;
   SceUID patcher_fragment_usse_uid;
   SceUID shader_patcher;
   SceUID fp_usse_rb_uid;
   SceUID patcher_buf_uid;
   unsigned disp_back_buf_index;
   unsigned disp_front_buf_index;
} vita_video_t;

typedef struct
{
   uint32_t *address;
} DisplayData;

static void *malloc_gpu(SceKernelMemBlockType type, uint32_t size, 
      uint32_t attribs, SceUID *uid, uint32_t params, uint32_t *offset)
{
   int ret = SCE_OK;

   if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RWDATA)
      size = GXM_ALIGN(size, 262144);
   else
      size = GXM_ALIGN(size, 4096);

   if(((params & MALLOC_PARAMS_FRAGMENT_FLAG) == MALLOC_PARAMS_FRAGMENT_FLAG) ||
         ((params & MALLOC_PARAMS_VERTEX_FLAG) == MALLOC_PARAMS_VERTEX_FLAG))
      type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE;

   *uid = sceKernelAllocMemBlock("basic", type, size, NULL);

   if (uid != SCE_OK)
      goto error;

   void *mem = NULL;

   ret = sceKernelGetMemBlockBase(*uid, &mem);

   if (ret != SCE_OK)
      goto error;

   if((params & MALLOC_PARAMS_FRAGMENT_FLAG) == MALLOC_PARAMS_FRAGMENT_FLAG)
      ret = sceGxmMapFragmentUsseMemory(mem, size, offset);
   else if((params & MALLOC_PARAMS_VERTEX_FLAG) == MALLOC_PARAMS_VERTEX_FLAG)
      ret = sceGxmMapVertexUsseMemory(mem, size, offset);
   else
      ret = sceGxmMapMemory(mem, size, attribs);

   if (ret != SCE_OK)
      goto error;

   return mem;
error:
   RARCH_ERR("Error during GPU memory allocation.\n");
   return NULL;
}

static void free_gpu(SceUID uid, uint32_t params)
{
   int ret = SCE_OK;

   void *mem = NULL;
   ret = sceKernelGetMemBlockBase(uid, &mem);

   if (ret != SCE_OK)
      goto error;

   if((params & MALLOC_PARAMS_FRAGMENT_FLAG) == MALLOC_PARAMS_FRAGMENT_FLAG)
      ret = sceGxmUnmapFragmentUsseMemory(mem);
   else if((params & MALLOC_PARAMS_VERTEX_FLAG) == MALLOC_PARAMS_VERTEX_FLAG)
      ret = sceGxmUnmapVertexUsseMemory(mem);
   else
      ret = sceGxmUnmapMemory(mem);

   if (ret != SCE_OK)
      goto error;

   ret = sceKernelFreeMemBlock(uid);

   if (ret != SCE_OK)
      goto error;

error:
   RARCH_ERR("Error during GPU memory deallocation.\n");
}

static void vita_gfx_init_fbo(void *data, const video_info_t *video)
{
   vita_video_t *vid = (vita_video_t*)driver.video_data;

   SceGxmRenderTargetParams rtparams;
   memset(&rtparams, 0, sizeof(SceGxmRenderTargetParams));

   rtparams.flags                = 0;
   rtparams.width                = PSP_FB_WIDTH;
   rtparams.height               = PSP_FB_HEIGHT;
   rtparams.scenesPerFrame       = 1;
   rtparams.multisampleMode      = SCE_GXM_MULTISAMPLE_NONE;
   rtparams.multisampleLocations = 0;
   rtparams.hostMem              = NULL;
   rtparams.hostMemSize          = 0;
   rtparams.driverMemBlock       = -1;

   // compute size
   uint32_t host_mem_size, driver_mem_size;
   sceGxmGetRenderTargetMemSizes(&rtparams, &host_mem_size, &driver_mem_size);

   rtparams.hostMem        = malloc(host_mem_size);
   rtparams.hostMemSize    = host_mem_size;
   rtparams.driverMemBlock = sceKernelAllocMemBlock(
         "SampleRT",
         SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
         driver_mem_sie, NULL);

   int ret = sceGxmCreateRenderTarget(&rtparams, &vid->rt);

   return ret;
}

static void disp_callback(const void *callback_data)
{
   int ret = SCE_OK;

#if defined(SN_TARGET_PSP2)
   SceDisplayFrameBuf framebuf;

   const DisplayData *display_data = (const DisplayData*)callback_data;

   memset(&framebuf, 0, sizeof(SceDisplayFrameBuf));

   framebuf.size        = sizeof(SceDisplayFrameBuf);
   framebuf.base        = display_data->address;
   framebuf.pitch       = PSP_PITCH_PIXELS;
   framebuf.pixelformat = PSP_DISPLAY_PIXELFORMAT_8888;
   framebuf.width       = PSP_FB_WIDTH;
   framebuf.height      = PSP_FB_HEIGHT;

   ret = DisplaySetFrameBuf(&framebuf, PSP_FB_WIDTH, PSP_DISPLAY_PIXELFORMAT_8888, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
#elif defined(PSP)
   ret = DisplaySetFrameBuf(&display_data->address, PSP_FB_WIDTH, PSP_DISPLAY_PIXELFORMAT_8888, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
#endif

   /* TODO - Don't bother with error checking for now in here */

   // Block until swap has occurred and the old buffer is no longer displayed
   ret = sceDisplayWaitSetFrameBuf();
}

static void *patcher_host_alloc(void *user_data, uint32_t size)
{
   (void)user_data;
   return malloc(size);
}

static void patcher_host_free(void *user_data, void *mem)
{
   (void)user_data;
   free(mem);
}

static int vita_gfx_init_shader_patcher(const video_info_t *video)
{
   ps2p_video_t *vid = (vita_video_t*)driver.video_data;

   SceGxmShaderPatcherParams patcher_params;
   uint32_t patcherVertexUsseOffset, patcherFragmentUsseOffset;

   memset(&patcher_params, 0, sizeof(SceGxmShaderPatcherParams));
   patcher_params.userData                = NULL;
   patcher_params.hostAllocCallback       = &patcher_host_alloc;
   patcher_params.hostFreeCallback        = &patcher_host_free;
   patcher_params.bufferAllocCallback     = NULL;
   patcher_params.bufferreeCallback       = NULL;
   patcher_params.bufferMem               = malloc_gpu(
         SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
         64 * 1024,
         SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
         &vid->patcher_buf_uid, 0, NULL);
   patcher_params.bufferMemSize           = 64 * 1024;
   patcher_params.vertexUsseAllocCallback = NULL;
   patcher_params.vertexUsseFreeCallback  = NULL;
   patcher_params.vertexUsseMem           = malloc_gpu(
         0,
         64 * 1024,
         0,
         &vid->patcher_vertex_usse_uid,
         MALLOC_PARAMS_VERTEX_FLAG,
         &patcherVertexUsseOffset);
   patcher_params.vertexUsseMemSize       = 64 * 1024;
   patcher_params.vertexUsseOffset        = patcherVertexUsseOffset;
   patcher_params.fragmentUsseAllocCallback = NULL;
   patcher_params.fragmentUsseFreeCallback  = NULL;
   patcher_params.fragmentUsseMem         = malloc_gpu(
         0,
         64 * 1024,
         0,
         &vid->patcher_fragment_usse_uid,
         MALLOC_PARAMS_FRAGMENT_FLAG,
         &patcherFragmentUsseOffset);
   patcher_params.fragmentUsseMemSize     = 64 * 1024;
   patcher_params.fragmentUsseOffset      = patcherFragmentUsseOffset;

   int ret = sceGxmShaderPatcherCreate(&patcher_params, &vid->shader_patcher);

   return ret;
}

static void vita_gfx_init_sync_objects(const video_info_t *video)
{
   vita_video_t *vid = (vita_video_t*)driver.video_data;

   for (unsigned i = 0; i < DISPLAY_BUFFER_COUNT; ++i)
   {
      vid->disp_buf_data[i] = malloc_gpu(
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
            DISPLAY_BUFFER_SIZE,
            SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
            &vid->disp_buf_uid[i], 0, NULL);

      int ret = sceGxmColorSurfaceInit(
            &vid->disp_surface[i],
            SCE_GXM_COLOR_FORMAT_A8B8G8R8, //TODO - Add toggle between 16bpp and 32bpp here
            SCE_GXM_COLOR_SURFACE_LINEAR,
            SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
            PSP_FB_WIDTH,
            PSP_FB_HEIGHT,
            PSP_PITCH_PIXELS,
            vid->disp_buf_data[i]);

      if(ret != SCE_OK)
      {
         RARCH_ERR("Initialization of color surface %d failed.\n", i);
      }
      else
      {
         ret = sceGxmSyncObjectCreate(&vid->disp_buffer_sync[i]);

         if(ret != SCE_OK)
            RARCH_ERR("Initialization of sync object %d failed.\n");
      }
   }
}

static void *vita_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
   (void)video;

   if (driver.video_data)
   {
      vita_video_t *vid = (vita_video_t*)driver.video_data;

      /* TODO - Reinitialize textures here */

      return driver.video_data;
   }

   vita_video_t *vid = (vita_video_t*)calloc(1, sizeof(vita_video_t));

   if (!vid)
      goto error;

   driver.video_data = vid;

   int ret;
   SceGxmInitializeParams params;
   memset(&params, 0, sizeof(SceGxmInitializeParams));

   params.flags                        = 0;
   params.displayQueueMaxPendingCount  = DISPLAY_MAX_PENDING_SWAPS;
   params.displayQueueCallback         = disp_callback;
   params.displayQueueCallbackDataSize = sizeof(DisplayData);
   params.parameterBufferSize          = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

   ret = sceGxmInitialize(&params);

   if(ret != SCE_OK)
      goto error;

   SceGxmContextParams ctx_params;
   memset(&ctx_params, 0, sizeof(SceGxmContextParams));

   uint32_t fp_usse_ring_buffer_offset;

   vid->context_host_mem = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);

   ctx_params.hostMem                       = vid->context_host_mem;
   ctx_params.hostMemSize                   = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
   ctx_params.vdmRingBufferMem              = malloc_gpu(
         SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
         SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
         SCE_GXM_MEMORY_ATTRIB_READ,
         &vid->rb_uid, 0, NULL);
   ctx_params.vdmRingBufferMemSize          = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
   ctx_params.vertexRingBufferMem           = malloc_gpu(
         SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
         SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
         SCE_GXM_MEMORY_ATTRIB_READ,
         &vid->vtx_rb_uid, 0, NULL);
   ctx_params.vertexRingBufferMemSize       = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
   ctx_params.fragmentRingBufferMem         = malloc_gpu(
         SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
         SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
         SCE_GXM_MEMORY_ATTRIB_READ,
         &vid->fp_rb_uid, 0, NULL);
   ctx_params.fragmentRingBufferMemSize     = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
   ctx_params.fragmentUsseRingBufferMem     = malloc_gpu(
         0,
         SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
         0,
         &vid->fp_usse_rb_uid,
         MALLOC_PARAMS_FRAGMENT_FLAG,
         &fp_usse_ring_buffer_offset);
   ctx_params.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
   ctx_params.fragmentUsseRingBufferOffset  = vid->fp_rb_uid;

   vid->gxm_ctx = NULL;

   ret = sceGxmCreateContext(&ctx_params, &vid->gxm_ctx);

   if (ret != SCE_OK)
      goto error;

   if((vita_gfx_init_fbo()) != SCE_OK)
      goto error;
   else
      RARCH_LOG("FBO initialized successfully.\n");

   if((vita_gfx_init_shader_patcher()) != SCE_OK)
      goto error;
   else
      RARCH_LOG("Shader patcher initialized successfully.\n");

   vita_gfx_init_sync_objects(video);

   /* Clear display buffer for first swap */
   memset(vid->disp_buf_data[vid>disp_front_buf_index], 0x00, DISPLAY_BUFFER_SIZE);

   /* Swap to the current front buffer with Vsync */
   disp_callback(NULL);

   return vid;
error:
   RARCH_ERR("Vita libgxm video could not be initialized.\n");
   return (void*)-1;
}

static inline void vita_gfx_swap(void)
{
   vita_video_t *vid = (vita_video_t*)driver.video_data;

   DisplayData display_data;

   display_data.address = vid->disp_buf_data[vid->disp_back_buf_index];

   /* queue swap for this frame */

   int ret = sceGxmDisplayQueueAddEntry(
         vid->disp_buffer_sync[vid->disp_front_buf_index],
         vid->disp_buffer_sync[vid->disp_back_buf_index],
         &display_data);

   vid->disp_front_buf_index = vid->disp_back_buf_index;
   vid->disp_back_buf_index  = (vid->disp_back_buf_index + 1) & DISPLAY_BUFFER_COUNT;
}

static bool vita_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   (void)data;
   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;

   vita_video_t *vid = (vita_video_t*)data;

   sceGxmBeginScene(vid->gcm_ctx, 0, vid->rt, NULL,
         NULL, vid->disp_buf_sync[vid->disp_back_buf_index]);

   /* TODO - code comes inbetween */

   sceGxmEndScene(vid->gxm_ctx, NULL, NULL);

   /* notify end of frame */
   sceGxmPadHeartBeat(&vid->disp_surface[vid->disp_back_buf_index], vid->disp_buf_sync[vid->disp_back_buf_index]);

   vita_gfx_swap();

   return true;
}

static void vita_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool vita_gfx_alive(void *data)
{
   (void)data;
   return true;
}

static bool vita_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static void vita_gfx_free(void *data)
{
   (void)data;
   void *hostmem;
   int ret;
   SceUID drivermemblock;

   /* TDO: error checking */

   vita_video_t *vid = (vita_video_t*)driver.video_data;

   sceGxmFinish(vid->gxm_ctx);

   ret = sceGxmRenderTargetGetHostMem(vid->rt, &hostmem);

   ret = sceGxmRenderTargetGetDriverMemBlock(vid->rt, &drivermemblock);

   ret = sceGxmDestroyRenderTarget(vid->rt);

   sceKernelFreeMemBlock(drivermemblock);
   free(hostmem);

   // wait until display queue is finished before deallocating display buffers
   int ret = sceGxmDisplayQueueFinish();

   for (int i = 0; i < DISPLAY_BUFFER_COUNT; ++i)
   {
      free_gpu(vid->disp_buf_uid[i], 0);
      ret = sceGxmSyncObjectDestroy(vid->disp_buf_sync[i]);
   }

   ret = sceGxmShaderPatcherDestroy(vid->shader_patcher);

   free_gpu(vid->patcher_fragment_usse_uid, MALLOC_PARAMS_FRAGMENT_FLAG);
   free_gpu(vid->patcher_vertex_usse_uid, MALLOC_PARAMS_VERTEX_FLAG);
   free_gpu(vid->patcher_buf_uid, 0);

   ret = sceGxmDestroyContext(vid->gxm_ctx);

   free_gpu(vid->fp_rb_uid, MALLOC_PARAMS_FRAGMENT_FLAG);
   free_gpu(vid->vtx_rb_uid, MALLOC_PARAMS_VERTEX_FLAG);
   free_gpu(vid->vid_rb_uid, 0);

   free(vid->context_host_mem, 0);

   sceGxmTerminate();
}

#ifdef RARCH_CONSOLE
static void vita_gfx_start(void) {}
static void vita_gfx_restart(void) {}
#endif

const video_driver_t video_vita = {
   vita_gfx_init,
   vita_gfx_frame,
   vita_gfx_set_nonblock_state,
   vita_gfx_alive,
   vita_gfx_focus,
   NULL,
   vita_gfx_free,
   "vita",

#ifdef RARCH_CONSOLE
   vita_gfx_start,
   vita_gfx_restart,
#endif
};
