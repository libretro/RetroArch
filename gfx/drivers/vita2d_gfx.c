/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Sergi Granell (xerpi)
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/display.h>

#include <retro_inline.h>
#include <encodings/utf.h>
#include <string/stdstring.h>
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"
#include "../video_driver.h"

#include <defines/psp_defines.h>

#include "../../driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../configuration.h"

/* Forward declaration
   TODO/FIXME - check if this custom memcpy is genuinely more efficient */
extern void *memcpy_neon(void *dst, const void *src, size_t n);

/* Defines */

#define RGBA8(r,g,b,a) ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((g)&0xFF)<<8) | (((r)&0xFF)<<0))
#define ALIGN(x, a)  (((x) + ((a) - 1)) & ~((a) - 1))
#define DISPLAY_COLOR_FORMAT   SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define DISPLAY_PIXEL_FORMAT   SCE_DISPLAY_PIXELFORMAT_A8B8G8R8
#define DISPLAY_BUFFER_COUNT   3
#define DISPLAY_MAX_PENDING_SWAPS   2
#define DEFAULT_TEMP_POOL_SIZE     (1 * 1024 * 1024)
#define GXM_TEX_MAX_SIZE 4096

/* Types */

typedef enum
{
   GXM_VIDEO_MODE_960x544 = 0,
   GXM_VIDEO_MODE_1280x720
} gxm_video_mode_t;

typedef struct gxm_video_mode_data
{
   int width;
   int height;
   int stride;
} gxm_video_mode_data_t;

typedef struct gxm_color_vertex
{
   float x;
   float y;
   float z;
   unsigned int color;
} gxm_color_vertex_t;

typedef struct gxm_texture_vertex
{
   float x;
   float y;
   float z;
   float u;
   float v;
} gxm_texture_vertex_t;

typedef struct gxm_texture_tint_vertex
{
   float x;
   float y;
   float z;
   float u;
   float v;
   float r;
   float g;
   float b;
   float a;
} gxm_texture_tint_vertex_t;

typedef struct gxm_texture
{
   SceGxmTexture gxm_tex;
   SceUID data_UID;
   SceUID palette_UID;
   SceGxmRenderTarget *gxm_rtgt;
   SceGxmColorSurface gxm_sfc;
   SceGxmDepthStencilSurface gxm_sfd;
   SceUID depth_UID;
} gxm_texture_t;

typedef struct gxm_display_data
{
   void *address;
} gxm_display_data_t;

typedef struct gxm_fragment_programs
{
   SceGxmFragmentProgram *color;
   SceGxmFragmentProgram *texture;
   SceGxmFragmentProgram *textureTint;
} gxm_fragment_programs_t;

typedef struct vita_menu_frame
{
   gxm_texture_t *texture;
   int width;
   int height;
   bool active;
} vita_menu_t;

#ifdef HAVE_OVERLAY
struct vita_overlay_data
{
   gxm_texture_t *tex;
   float x;
   float y;
   float w;
   float h;
   float tex_x;
   float tex_y;
   float tex_w;
   float tex_h;
   float alpha_mod;
   float width;
   float height;
};
#endif

typedef struct vita_video
{
   gxm_texture_t *texture;
   SceGxmTextureFormat format;
   int width;
   int height;
   SceGxmTextureFilter tex_filter;

   video_viewport_t vp;

   math_matrix_4x4 mvp, mvp_no_rot;

   vita_menu_t menu;

#ifdef HAVE_OVERLAY
   struct vita_overlay_data *overlay;
   unsigned overlays;
#endif
   unsigned video_width;
   unsigned video_height;
   unsigned rotation;

#ifdef HAVE_OVERLAY
   bool overlay_enable;
   bool overlay_full_screen;
#endif
   bool fullscreen;
   bool vsync;
   bool rgb32;
   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;
} vita_video_t;

typedef struct
{
   vita_video_t *vita;
   gxm_texture_t *texture;
   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;
} vita_font_t;

/* Shader program blobs */

extern const SceGxmProgram color_v_gxp;
extern const SceGxmProgram color_f_gxp;
extern const SceGxmProgram texture_v_gxp;
extern const SceGxmProgram texture_f_gxp;
extern const SceGxmProgram texture_tint_v_gxp;
extern const SceGxmProgram texture_tint_f_gxp;

/* File-scope state */

static gxm_video_mode_data_t video_mode_data;
static gxm_video_mode_t video_mode_initial;
static SceGxmMultisampleMode current_msaa = SCE_GXM_MULTISAMPLE_4X;

static const SceGxmProgram *const colorVertexProgramGxp         = &color_v_gxp;
static const SceGxmProgram *const colorFragmentProgramGxp       = &color_f_gxp;
static const SceGxmProgram *const textureVertexProgramGxp       = &texture_v_gxp;
static const SceGxmProgram *const textureFragmentProgramGxp     = &texture_f_gxp;
static const SceGxmProgram *const textureTintVertexProgramGxp   = &texture_tint_v_gxp;
static const SceGxmProgram *const textureTintFragmentProgramGxp = &texture_tint_f_gxp;

static int gxm_initialized = 0;
static unsigned int clear_color_u = 0xFF000000;
static int clip_rect_x_min = 0;
static int clip_rect_y_min = 0;
static int clip_rect_x_max = 0;
static int clip_rect_y_max = 0;
static int vblank_wait = 1;
static int drawing = 0;
static int clipping_enabled = 0;

static SceUID vdmRingBufferUid;
static SceUID vertexRingBufferUid;
static SceUID fragmentRingBufferUid;
static SceUID fragmentUsseRingBufferUid;

static SceGxmContextParams contextParams;
static SceGxmRenderTarget *renderTarget = NULL;
static SceUID displayBufferUid[DISPLAY_BUFFER_COUNT];
static void *displayBufferData[DISPLAY_BUFFER_COUNT];
static SceGxmColorSurface displaySurface[DISPLAY_BUFFER_COUNT];
static SceGxmSyncObject *displayBufferSync[DISPLAY_BUFFER_COUNT];
static SceUID depthBufferUid;
static SceUID stencilBufferUid;
static SceGxmDepthStencilSurface depthSurface;
static void *depthBufferData = NULL;
static void *stencilBufferData = NULL;

static unsigned int backBufferIndex = 0;
static unsigned int frontBufferIndex = 0;

static SceGxmShaderPatcher *shaderPatcher = NULL;

static SceGxmShaderPatcherId colorVertexProgramId;
static SceGxmShaderPatcherId colorFragmentProgramId;
static SceGxmShaderPatcherId textureVertexProgramId;
static SceGxmShaderPatcherId textureFragmentProgramId;
static SceGxmShaderPatcherId textureTintVertexProgramId;
static SceGxmShaderPatcherId textureTintFragmentProgramId;

static SceUID patcherBufferUid;
static SceUID patcherVertexUsseUid;
static SceUID patcherFragmentUsseUid;

static SceUID linearIndicesUid;
static uint16_t *linearIndices = NULL;

static float gxm_ortho_matrix[4*4];
static SceGxmContext *gxm_context = NULL;
static SceGxmVertexProgram *gxm_colorVertexProgram = NULL;
static SceGxmFragmentProgram *gxm_colorFragmentProgram = NULL;
static SceGxmVertexProgram *gxm_textureVertexProgram = NULL;
static SceGxmFragmentProgram *gxm_textureFragmentProgram = NULL;
static SceGxmVertexProgram *gxm_textureTintVertexProgram = NULL;
static SceGxmFragmentProgram *gxm_textureTintFragmentProgram = NULL;
static const SceGxmProgramParameter *gxm_colorWvpParam = NULL;
static const SceGxmProgramParameter *gxm_textureWvpParam = NULL;
static const SceGxmProgramParameter *gxm_textureTintWvpParam = NULL;


static struct
{
   gxm_fragment_programs_t blend_mode_normal;
   gxm_fragment_programs_t blend_mode_add;
} gxm_fragmentPrograms;

/* Temporary memory pool */
static void *pool_addr = NULL;
static SceUID poolUid;
static unsigned int pool_index = 0;
static unsigned int pool_size = 0;

static SceKernelMemBlockType MemBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW;

static const float gxm_vertexes[8]   = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float gxm_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float gxm_colors[16]    = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

/* Forward declarations */

static void *gpu_alloc(SceKernelMemBlockType type, unsigned int size,
      unsigned int alignment, unsigned int attribs, SceUID *uid);
static void gpu_free(SceUID uid);
static void *vertex_usse_alloc(unsigned int size, SceUID *uid,
      unsigned int *usse_offset);
static void vertex_usse_free(SceUID uid);
static void gxm_render_overlay(void *data);
static void gxm_set_projection(vita_video_t *vita,
      struct video_ortho *ortho, bool allow_rotate);


static void *gpu_alloc(SceKernelMemBlockType type, unsigned int size,
      unsigned int alignment, unsigned int attribs, SceUID *uid)
{
   void *mem;

   if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
      size = ALIGN(size, 256*1024);
   else
      size = ALIGN(size, 4*1024);

   *uid = sceKernelAllocMemBlock("gpu_mem", type, size, NULL);

   if (*uid < 0)
      return NULL;

   if (sceKernelGetMemBlockBase(*uid, &mem) < 0)
      return NULL;

   if (sceGxmMapMemory(mem, size, attribs) < 0)
      return NULL;

   return mem;
}

static void gpu_free(SceUID uid)
{
   void *mem = NULL;
   if (sceKernelGetMemBlockBase(uid, &mem) < 0)
      return;
   sceGxmUnmapMemory(mem);
   sceKernelFreeMemBlock(uid);
}

static void *vertex_usse_alloc(unsigned int size, SceUID *uid,
      unsigned int *usse_offset)
{
   void *mem = NULL;

   size = ALIGN(size, 4096);
   *uid = sceKernelAllocMemBlock("vertex_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

   if (sceKernelGetMemBlockBase(*uid, &mem) < 0)
      return NULL;
   if (sceGxmMapVertexUsseMemory(mem, size, usse_offset) < 0)
      return NULL;

   return mem;
}

static void vertex_usse_free(SceUID uid)
{
   void *mem = NULL;
   if (sceKernelGetMemBlockBase(uid, &mem) < 0)
      return;
   sceGxmUnmapVertexUsseMemory(mem);
   sceKernelFreeMemBlock(uid);
}

static void *fragment_usse_alloc(unsigned int size, SceUID *uid,
      unsigned int *usse_offset)
{
   void *mem = NULL;

   size = ALIGN(size, 4096);
   *uid = sceKernelAllocMemBlock("fragment_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

   if (sceKernelGetMemBlockBase(*uid, &mem) < 0)
      return NULL;
   if (sceGxmMapFragmentUsseMemory(mem, size, usse_offset) < 0)
      return NULL;

   return mem;
}

static void fragment_usse_free(SceUID uid)
{
   void *mem = NULL;
   if (sceKernelGetMemBlockBase(uid, &mem) < 0)
      return;
   sceGxmUnmapFragmentUsseMemory(mem);
   sceKernelFreeMemBlock(uid);
}

static void matrix_init_orthographic(float *m, float left, float right,
      float bottom, float top, float near, float far)
{
   m[0x0] = 2.0f/(right-left);
   m[0x4] = 0.0f;
   m[0x8] = 0.0f;
   m[0xC] = -(right+left)/(right-left);

   m[0x1] = 0.0f;
   m[0x5] = 2.0f/(top-bottom);
   m[0x9] = 0.0f;
   m[0xD] = -(top+bottom)/(top-bottom);

   m[0x2] = 0.0f;
   m[0x6] = 0.0f;
   m[0xA] = -2.0f/(far-near);
   m[0xE] = (far+near)/(far-near);

   m[0x3] = 0.0f;
   m[0x7] = 0.0f;
   m[0xB] = 0.0f;
   m[0xF] = 1.0f;
}


/* Static functions */

static void *patcher_host_alloc(void *user_data, unsigned int size)
{
   return malloc(size);
}

static void patcher_host_free(void *user_data, void *mem)
{
   free(mem);
}

static int gxm_switch_video_mode(gxm_video_mode_t video_mode)
{
   SceGxmRenderTargetParams renderTargetParams;
   int i, x, y;

   if(video_mode > video_mode_initial)
      return -1;
   switch (video_mode)
   {
      case GXM_VIDEO_MODE_960x544:
         video_mode_data.width = 960;
         video_mode_data.height = 544;
         video_mode_data.stride = 960;
         break;

      case GXM_VIDEO_MODE_1280x720:
         video_mode_data.width = 1280;
         video_mode_data.height = 720;
         video_mode_data.stride = 1280;
         break;

      default:
         return -1;
         break;
   }

   clip_rect_x_max = video_mode_data.width;
   clip_rect_y_max = video_mode_data.height;

   if (renderTarget != NULL)
   {
      sceGxmDestroyRenderTarget(renderTarget);

      for (i = 0; i < DISPLAY_BUFFER_COUNT; i++)
                {
         /* clear the buffer then deallocate */
         memset(displayBufferData[i], 0,
               video_mode_data.height*video_mode_data.stride*4);
         gpu_free(displayBufferUid[i]);
      }
   }

   /* allocate memory and sync objects for display buffers */
   for (i = 0; i < DISPLAY_BUFFER_COUNT; i++)
        {
      /* allocate memory for display */
      displayBufferData[i] = gpu_alloc(
            SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
            4*video_mode_data.stride*video_mode_data.height,
            SCE_GXM_COLOR_SURFACE_ALIGNMENT,
            SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
            &displayBufferUid[i]);

      /* memset the buffer to black */
      for (y = 0; y < video_mode_data.height; y++)
                {
         unsigned int *row = (unsigned int *)displayBufferData[i] + y*video_mode_data.stride;
         for (x = 0; x < video_mode_data.width; x++)
            row[x] = 0xff000000;
      }

      /* initialize a color surface for this display buffer */
      sceGxmColorSurfaceInit(
            &displaySurface[i],
            DISPLAY_COLOR_FORMAT,
            SCE_GXM_COLOR_SURFACE_LINEAR,
            (current_msaa == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
            video_mode_data.width,
            video_mode_data.height,
            video_mode_data.stride,
            displayBufferData[i]);

   }

   /* set up parameters */
   memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
   renderTargetParams.flags      = 0;
   renderTargetParams.width      = video_mode_data.width;
   renderTargetParams.height      = video_mode_data.height;
   renderTargetParams.scenesPerFrame     = 1;
   renderTargetParams.multisampleMode   = current_msaa;
   renderTargetParams.multisampleLocations     = 0;
   renderTargetParams.driverMemBlock     = -1; /* Invalid UID */

   /* create the render target */
   sceGxmCreateRenderTarget(&renderTargetParams, &renderTarget);

   matrix_init_orthographic(gxm_ortho_matrix, 0.0f,
         video_mode_data.width, video_mode_data.height, 0.0f, 0.0f, 1.0f);

   return 0;
}

static void gxm_display_callback(const void *callback_data)
{
   SceDisplayFrameBuf framebuf;
   const gxm_display_data_t *display_data = (const gxm_display_data_t *)callback_data;

   memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
   framebuf.size        = sizeof(SceDisplayFrameBuf);
   framebuf.base        = display_data->address;
   framebuf.pitch       = video_mode_data.stride;
   framebuf.pixelformat = DISPLAY_PIXEL_FORMAT;
   framebuf.width       = video_mode_data.width;
   framebuf.height      = video_mode_data.height;
   if (sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_NEXTFRAME)
         == SCE_DISPLAY_ERROR_INVALID_RESOLUTION)
   {
      if (video_mode_initial)
         gxm_switch_video_mode(GXM_VIDEO_MODE_960x544);
   }

   if (vblank_wait)
      sceDisplayWaitVblankStart();
}

static void gxm_free_fragment_programs(gxm_fragment_programs_t *out)
{
   sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->color);
   sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->texture);
   sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->textureTint);
}

static void gxm_make_fragment_programs(gxm_fragment_programs_t *out,
   const SceGxmBlendInfo *blend_info, SceGxmMultisampleMode msaa)
{
   int err = sceGxmShaderPatcherCreateFragmentProgram(
      shaderPatcher,
      colorFragmentProgramId,
      SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
      msaa,
      blend_info,
      colorVertexProgramGxp,
      &out->color);

   err = sceGxmShaderPatcherCreateFragmentProgram(
      shaderPatcher,
      textureFragmentProgramId,
      SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
      msaa,
      blend_info,
      textureVertexProgramGxp,
      &out->texture);

   err = sceGxmShaderPatcherCreateFragmentProgram(
      shaderPatcher,
      textureTintFragmentProgramId,
      SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
      msaa,
      blend_info,
      textureTintVertexProgramGxp,
      &out->textureTint);
}

static void gxm_set_blend_mode_add(int enable)
{
   gxm_fragment_programs_t *in = enable ? &gxm_fragmentPrograms.blend_mode_add
       : &gxm_fragmentPrograms.blend_mode_normal;

   gxm_colorFragmentProgram = in->color;
   gxm_textureFragmentProgram = in->texture;
   gxm_textureTintFragmentProgram = in->textureTint;
}

static int gxm_init_internal(unsigned int temp_pool_size,
      SceGxmMultisampleMode msaa, gxm_video_mode_t video_mode)
{
   uint32_t idx;
   SceGxmInitializeParams initializeParams;
   void *fragmentUsseRingBuffer;
   unsigned int fragmentUsseRingBufferOffset;
   void *fragmentRingBuffer;
   void *vertexRingBuffer;
   void *vdmRingBuffer;
   gxm_display_data_t displayData;
   unsigned int depthStrideInSamples;
   unsigned int sampleCount;
   unsigned int alignedHeight;
   unsigned int alignedWidth;
   SceGxmShaderPatcherParams patcherParams;
   void *patcherFragmentUsse;
   unsigned int patcherFragmentUsseOffset;
   void *patcherVertexUsse;
   unsigned int patcherVertexUsseOffset;
   void *patcherBuffer;
   unsigned int patcherFragmentUsseSize;
   unsigned int patcherVertexUsseSize;
   unsigned int patcherBufferSize;
   SceGxmVertexStream colorVertexStreams[1];
   SceGxmVertexAttribute colorVertexAttributes[2];
   const SceGxmProgramParameter *paramColorColorAttribute;
   const SceGxmProgramParameter *paramColorPositionAttribute;
   SceGxmVertexStream textureVertexStreams[1];
   SceGxmVertexAttribute textureVertexAttributes[2];
   const SceGxmProgramParameter *paramTextureTexcoordAttribute;
   const SceGxmProgramParameter *paramTexturePositionAttribute;
   SceGxmVertexStream textureTintVertexStreams[1];
   SceGxmVertexAttribute textureTintVertexAttributes[3];
   const SceGxmProgramParameter *paramTextureTintColorAttribute;
   const SceGxmProgramParameter *paramTextureTintTexcoordAttribute;
   const SceGxmProgramParameter *paramTextureTintPositionAttribute;
   /* Blend modes. Fields are in SceGxmBlendInfo declaration order:
    * colorMask, colorFunc, alphaFunc, colorSrc, colorDst, alphaSrc,
    * alphaDst.
    */
   static const SceGxmBlendInfo blend_info = {
      SCE_GXM_COLOR_MASK_ALL,
      SCE_GXM_BLEND_FUNC_ADD,
      SCE_GXM_BLEND_FUNC_ADD,
      SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
      SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
      SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
   };
   static const SceGxmBlendInfo blend_info_add = {
      SCE_GXM_COLOR_MASK_ALL,
      SCE_GXM_BLEND_FUNC_ADD,
      SCE_GXM_BLEND_FUNC_ADD,
      SCE_GXM_BLEND_FACTOR_ONE,
      SCE_GXM_BLEND_FACTOR_ONE,
      SCE_GXM_BLEND_FACTOR_ONE,
      SCE_GXM_BLEND_FACTOR_ONE
   };
   int err;
   unsigned int i;

   if (gxm_initialized)
      return 1;

   video_mode_initial = video_mode;
   current_msaa = msaa;

   memset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
   initializeParams.flags        = 0;
   initializeParams.displayQueueMaxPendingCount = DISPLAY_MAX_PENDING_SWAPS;
   initializeParams.displayQueueCallback   = gxm_display_callback;
   initializeParams.displayQueueCallbackDataSize   = sizeof(gxm_display_data_t);
   initializeParams.parameterBufferSize     = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

   err = sceGxmInitialize(&initializeParams);

   /* allocate ring buffer memory using default sizes */
   vdmRingBuffer = gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
      SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
      4,
      SCE_GXM_MEMORY_ATTRIB_READ,
      &vdmRingBufferUid);

   vertexRingBuffer = gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
      SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
      4,
      SCE_GXM_MEMORY_ATTRIB_READ,
      &vertexRingBufferUid);

   fragmentRingBuffer = gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
      SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
      4,
      SCE_GXM_MEMORY_ATTRIB_READ,
      &fragmentRingBufferUid);

   fragmentUsseRingBuffer = fragment_usse_alloc(
      SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
      &fragmentUsseRingBufferUid,
      &fragmentUsseRingBufferOffset);

   memset(&contextParams, 0, sizeof(SceGxmContextParams));
   contextParams.hostMem         = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
   contextParams.hostMemSize      = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
   contextParams.vdmRingBufferMem      = vdmRingBuffer;
   contextParams.vdmRingBufferMemSize   = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
   contextParams.vertexRingBufferMem     = vertexRingBuffer;
   contextParams.vertexRingBufferMemSize   = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
   contextParams.fragmentRingBufferMem    = fragmentRingBuffer;
   contextParams.fragmentRingBufferMemSize     = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
   contextParams.fragmentUsseRingBufferMem     = fragmentUsseRingBuffer;
   contextParams.fragmentUsseRingBufferMemSize  = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
   contextParams.fragmentUsseRingBufferOffset   = fragmentUsseRingBufferOffset;

   err = sceGxmCreateContext(&contextParams, &gxm_context);
   err = gxm_switch_video_mode(video_mode);
   if (err < 0)
      return err;

   displayData.address = displayBufferData[0];
   gxm_display_callback(&displayData);

   /* allocate memory and sync objects for display buffers */
   /* create a sync object that we will associate with this buffer */
   for (i = 0; i < DISPLAY_BUFFER_COUNT; i++)
      err = sceGxmSyncObjectCreate(&displayBufferSync[i]);

   /* compute the memory footprint of the depth buffer */
   alignedWidth = ALIGN(video_mode_data.width, SCE_GXM_TILE_SIZEX);
   alignedHeight = ALIGN(video_mode_data.height, SCE_GXM_TILE_SIZEY);
   sampleCount = alignedWidth*alignedHeight;
   depthStrideInSamples = alignedWidth;
   if (current_msaa == SCE_GXM_MULTISAMPLE_4X)
   {
      /* samples increase in X and Y */
      sampleCount          *= 4;
      depthStrideInSamples *= 2;
   }
   else if (current_msaa == SCE_GXM_MULTISAMPLE_2X)
      /* samples increase in Y only */
      sampleCount *= 2;

   /* allocate the depth buffer */
   depthBufferData = gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
      4*sampleCount,
      SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
      SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
      &depthBufferUid);

   /* allocate the stencil buffer */
   stencilBufferData = gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
      4*sampleCount,
      SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
      SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
      &stencilBufferUid);

   /* create the SceGxmDepthStencilSurface structure */
   err = sceGxmDepthStencilSurfaceInit(
      &depthSurface,
      SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
      SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
      depthStrideInSamples,
      depthBufferData,
      stencilBufferData);

   /* Set the stencil test reference. This is assumed to stay at 1 from
    * here on, for region clipping.
    */
   sceGxmSetFrontStencilRef(gxm_context, 1);
   /* Set the stencil function. Not strictly needed: the set-clip-rectangle
    * path has to call this at the start of every scene anyway.
    */
   sceGxmSetFrontStencilFunc(
      gxm_context,
      SCE_GXM_STENCIL_FUNC_ALWAYS,
      SCE_GXM_STENCIL_OP_KEEP,
      SCE_GXM_STENCIL_OP_KEEP,
      SCE_GXM_STENCIL_OP_KEEP,
      0xFF,
      0xFF);

   /* set buffer sizes for this sample */
   patcherBufferSize = 64*1024;
   patcherVertexUsseSize = 64*1024;
   patcherFragmentUsseSize = 64*1024;

   /* allocate memory for buffers and USSE code */
   patcherBuffer = gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
      patcherBufferSize,
      4,
      SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
      &patcherBufferUid);

   patcherVertexUsse = vertex_usse_alloc(
      patcherVertexUsseSize,
      &patcherVertexUsseUid,
      &patcherVertexUsseOffset);

   patcherFragmentUsse = fragment_usse_alloc(
      patcherFragmentUsseSize,
      &patcherFragmentUsseUid,
      &patcherFragmentUsseOffset);

   /* create a shader patcher */
   memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
   patcherParams.userData      = NULL;
   patcherParams.hostAllocCallback   = &patcher_host_alloc;
   patcherParams.hostFreeCallback     = &patcher_host_free;
   patcherParams.bufferAllocCallback   = NULL;
   patcherParams.bufferFreeCallback = NULL;
   patcherParams.bufferMem      = patcherBuffer;
   patcherParams.bufferMemSize     = patcherBufferSize;
   patcherParams.vertexUsseAllocCallback  = NULL;
   patcherParams.vertexUsseFreeCallback   = NULL;
   patcherParams.vertexUsseMem     = patcherVertexUsse;
   patcherParams.vertexUsseMemSize   = patcherVertexUsseSize;
   patcherParams.vertexUsseOffset     = patcherVertexUsseOffset;
   patcherParams.fragmentUsseAllocCallback   = NULL;
   patcherParams.fragmentUsseFreeCallback = NULL;
   patcherParams.fragmentUsseMem    = patcherFragmentUsse;
   patcherParams.fragmentUsseMemSize   = patcherFragmentUsseSize;
   patcherParams.fragmentUsseOffset = patcherFragmentUsseOffset;

   err = sceGxmShaderPatcherCreate(&patcherParams, &shaderPatcher);

   /* check the shaders */
   err = sceGxmProgramCheck(colorVertexProgramGxp);
   err = sceGxmProgramCheck(colorFragmentProgramGxp);
   err = sceGxmProgramCheck(textureVertexProgramGxp);
   err = sceGxmProgramCheck(textureFragmentProgramGxp);
   err = sceGxmProgramCheck(textureTintVertexProgramGxp);
   err = sceGxmProgramCheck(textureTintFragmentProgramGxp);

   /* register programs with the patcher */
   err = sceGxmShaderPatcherRegisterProgram(shaderPatcher,
         colorVertexProgramGxp, &colorVertexProgramId);
   err = sceGxmShaderPatcherRegisterProgram(shaderPatcher,
         colorFragmentProgramGxp, &colorFragmentProgramId);
   err = sceGxmShaderPatcherRegisterProgram(shaderPatcher,
         textureVertexProgramGxp, &textureVertexProgramId);
   err = sceGxmShaderPatcherRegisterProgram(shaderPatcher,
         textureFragmentProgramGxp, &textureFragmentProgramId);
   err = sceGxmShaderPatcherRegisterProgram(shaderPatcher,
         textureTintVertexProgramGxp, &textureTintVertexProgramId);
   err = sceGxmShaderPatcherRegisterProgram(shaderPatcher,
         textureTintFragmentProgramGxp, &textureTintFragmentProgramId);

   /* Allocate a 64k * 2 bytes = 128 KiB buffer and store all possible */
   /* 16-bit indices in linear ascending order, so we can use this for */
   /* all drawing operations where we don't want to use indexing. */
   linearIndices = (uint16_t *)gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
      UINT16_MAX*sizeof(uint16_t),
      sizeof(uint16_t),
      SCE_GXM_MEMORY_ATTRIB_READ,
      &linearIndicesUid);

        /* Range of i must be greater than uint16_t, this doesn't endless-loop */
   for (idx = 0; idx <= UINT16_MAX; ++idx)
      linearIndices[idx] = idx;

   paramColorPositionAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aPosition");
   paramColorColorAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aColor");

   /* create color vertex format */
   /* x,y,z: 3 float 32 bits */
   colorVertexAttributes[0].streamIndex = 0;
   colorVertexAttributes[0].offset = 0;
   colorVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
   colorVertexAttributes[0].componentCount = 3; /* (x, y, z) */
   colorVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorPositionAttribute);
   /* color: 4 unsigned char  = 32 bits */
   colorVertexAttributes[1].streamIndex = 0;
   colorVertexAttributes[1].offset = 12; /* (x, y, z) * 4 = 12 bytes */
   colorVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
   colorVertexAttributes[1].componentCount = 4; /* (color) */
   colorVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorColorAttribute);
   /* 16 bit (short) indices */
   colorVertexStreams[0].stride = sizeof(gxm_color_vertex_t);
   colorVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

   /* create color shaders */
   err = sceGxmShaderPatcherCreateVertexProgram(
      shaderPatcher,
      colorVertexProgramId,
      colorVertexAttributes,
      2,
      colorVertexStreams,
      1,
      &gxm_colorVertexProgram);

   paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");

   paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");

   /* create texture vertex format */
   /* x,y,z: 3 float 32 bits */
   textureVertexAttributes[0].streamIndex = 0;
   textureVertexAttributes[0].offset = 0;
   textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
   textureVertexAttributes[0].componentCount = 3; /* (x, y, z) */
   textureVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexturePositionAttribute);
   /* u,v: 2 floats 32 bits */
   textureVertexAttributes[1].streamIndex = 0;
   textureVertexAttributes[1].offset = 12; /* (x, y, z) * 4 = 12 bytes */
   textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
   textureVertexAttributes[1].componentCount = 2; /* (u, v) */
   textureVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTexcoordAttribute);
   /* 16 bit (short) indices */
   textureVertexStreams[0].stride = sizeof(gxm_texture_vertex_t);
   textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

   /* create texture shaders */
   err = sceGxmShaderPatcherCreateVertexProgram(
      shaderPatcher,
      textureVertexProgramId,
      textureVertexAttributes,
      2,
      textureVertexStreams,
      1,
      &gxm_textureVertexProgram);

   paramTextureTintPositionAttribute = sceGxmProgramFindParameterByName(textureTintVertexProgramGxp, "aPosition");
   paramTextureTintTexcoordAttribute = sceGxmProgramFindParameterByName(textureTintVertexProgramGxp, "aTexcoord");
   paramTextureTintColorAttribute = sceGxmProgramFindParameterByName(textureTintVertexProgramGxp, "aColor");

   /* create texture vertex format */
   /* x,y,z: 3 float 32 bits */
   textureTintVertexAttributes[0].streamIndex = 0;
   textureTintVertexAttributes[0].offset = 0;
   textureTintVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
   textureTintVertexAttributes[0].componentCount = 3; /* (x, y, z) */
   textureTintVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTintPositionAttribute);
   /* u,v: 2 floats 32 bits */
   textureTintVertexAttributes[1].streamIndex = 0;
   textureTintVertexAttributes[1].offset = 12; /* (x, y, z) * 4 = 12 bytes */
   textureTintVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
   textureTintVertexAttributes[1].componentCount = 2; /* (u, v) */
   textureTintVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTintTexcoordAttribute);
   /* r,g,b,a: 4 floats 32 bits */
   textureTintVertexAttributes[2].streamIndex = 0;
   textureTintVertexAttributes[2].offset = 20; /* (u, v) * 4 = 8 bytes */
   textureTintVertexAttributes[2].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
   textureTintVertexAttributes[2].componentCount = 4; /* (r, g, b, a) */
   textureTintVertexAttributes[2].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTintColorAttribute);
   /* 16 bit (short) indices */
   textureTintVertexStreams[0].stride = sizeof(gxm_texture_tint_vertex_t);
   textureTintVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

   /* create texture shaders */
   err = sceGxmShaderPatcherCreateVertexProgram(
      shaderPatcher,
      textureTintVertexProgramId,
      textureTintVertexAttributes,
      3,
      textureTintVertexStreams,
      1,
      &gxm_textureTintVertexProgram);

   /* Create variations of the fragment program based on blending mode */
   gxm_make_fragment_programs(&gxm_fragmentPrograms.blend_mode_normal,
         &blend_info, current_msaa);
   gxm_make_fragment_programs(&gxm_fragmentPrograms.blend_mode_add,
         &blend_info_add, current_msaa);

   /* Default to "normal" blending mode (non-additive) */
   gxm_set_blend_mode_add(0);

   /* find vertex uniforms by name and cache parameter information */

   gxm_colorWvpParam = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "wvp");

   gxm_textureWvpParam = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");

   gxm_textureTintWvpParam = sceGxmProgramFindParameterByName(textureTintVertexProgramGxp, "wvp");

   /* Allocate memory for the memory pool */
   pool_size = temp_pool_size;
   pool_addr = gpu_alloc(
      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
      pool_size,
      sizeof(void *),
      SCE_GXM_MEMORY_ATTRIB_READ,
      &poolUid);


   matrix_init_orthographic(gxm_ortho_matrix, 0.0f,
         video_mode_data.width, video_mode_data.height, 0.0f, 0.0f, 1.0f);

   backBufferIndex = 0;
   frontBufferIndex = 0;

   gxm_initialized = 1;
   return 1;
}

static void gxm_set_viewport(int x, int y, int width, int height)
{
   float vh = video_mode_data.height;
   float sw = width  / 2.;
   float sh = height / 2.;
   float x_scale = sw;
   float x_port = x + sw;
   float y_scale = -(sh);
   float y_port = vh - y - sh;
   sceGxmSetViewport(gxm_context, x_port, x_scale, y_port, y_scale,
         -0.5f, 0.5f);
}

static void *gxm_pool_memalign(unsigned int size, unsigned int alignment)
{
   unsigned int new_index = (pool_index + alignment - 1) & ~(alignment - 1);
   if ((new_index + size) < pool_size)
   {
      void *addr = (void *)((unsigned int)pool_addr + new_index);
      pool_index = new_index + size;
      return addr;
   }
   return NULL;
}

static void gxm_draw_rectangle(float x, float y, float w, float h,
      unsigned int color)
{
   void *vertexDefaultBuffer;
   gxm_color_vertex_t *vertices = (gxm_color_vertex_t *)
      gxm_pool_memalign(
      4 * sizeof(gxm_color_vertex_t), /* 4 vertices */
      sizeof(gxm_color_vertex_t));

   uint16_t *indices = (uint16_t *)gxm_pool_memalign(
      4 * sizeof(uint16_t), /* 4 indices */
      sizeof(uint16_t));

   vertices[0].x = x;
   vertices[0].y = y;
   vertices[0].z = +0.5f;
   vertices[0].color = color;

   vertices[1].x = x + w;
   vertices[1].y = y;
   vertices[1].z = +0.5f;
   vertices[1].color = color;

   vertices[2].x = x;
   vertices[2].y = y + h;
   vertices[2].z = +0.5f;
   vertices[2].color = color;

   vertices[3].x = x + w;
   vertices[3].y = y + h;
   vertices[3].z = +0.5f;
   vertices[3].color = color;

   indices[0] = 0;
   indices[1] = 1;
   indices[2] = 2;
   indices[3] = 3;

   sceGxmSetVertexProgram(gxm_context, gxm_colorVertexProgram);
   sceGxmSetFragmentProgram(gxm_context, gxm_colorFragmentProgram);

   sceGxmReserveVertexDefaultUniformBuffer(gxm_context,
         &vertexDefaultBuffer);
   sceGxmSetUniformDataF(vertexDefaultBuffer, gxm_colorWvpParam, 0, 16,
         gxm_ortho_matrix);

   sceGxmSetVertexStream(gxm_context, 0, vertices);
   sceGxmDraw(gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
         SCE_GXM_INDEX_FORMAT_U16, indices, 4);
}

static void gxm_set_clip_rectangle(int x_min, int y_min, int x_max, int y_max)
{
   gxm_set_viewport(0,0,video_mode_data.width,video_mode_data.height);
   clipping_enabled = 1;
   clip_rect_x_min = x_min;
   clip_rect_y_min = y_min;
   clip_rect_x_max = x_max;
   clip_rect_y_max = y_max;
   /* We can only draw during a scene, but we can cache the values 
    * since they're not going to have any visible effect till the 
    * scene starts anyways */
   if (drawing)
   {
      sceGxmSetFrontDepthWriteEnable(gxm_context,
         SCE_GXM_DEPTH_WRITE_DISABLED);
      /* clear the stencil buffer to 0 */
      sceGxmSetFrontStencilFunc(
         gxm_context,
         SCE_GXM_STENCIL_FUNC_NEVER,
         SCE_GXM_STENCIL_OP_ZERO,
         SCE_GXM_STENCIL_OP_ZERO,
         SCE_GXM_STENCIL_OP_ZERO,
         0xFF,
         0xFF);
      gxm_draw_rectangle(0, 0, video_mode_data.width,
            video_mode_data.height, 0);
      /* set the stencil to 1 in the desired region */
      sceGxmSetFrontStencilFunc(
         gxm_context,
         SCE_GXM_STENCIL_FUNC_NEVER,
         SCE_GXM_STENCIL_OP_REPLACE,
         SCE_GXM_STENCIL_OP_REPLACE,
         SCE_GXM_STENCIL_OP_REPLACE,
         0xFF,
         0xFF);
      gxm_draw_rectangle(x_min, y_min, x_max - x_min, y_max - y_min, 0);
      sceGxmSetFrontDepthWriteEnable(gxm_context,
         SCE_GXM_DEPTH_WRITE_ENABLED);
      if(clipping_enabled)
         /* set the stencil function to only accept pixels 
          * where the stencil is 1 */
         sceGxmSetFrontStencilFunc(
            gxm_context,
            SCE_GXM_STENCIL_FUNC_EQUAL,
            SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,
            0xFF,
            0xFF);
      else
         sceGxmSetFrontStencilFunc(
            gxm_context,
            SCE_GXM_STENCIL_FUNC_ALWAYS,
            SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,
            0xFF,
            0xFF);
   }
}


static int tex_format_to_bytespp(SceGxmTextureFormat format)
{
   switch (format & 0x9f000000U)
   {
   case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
   case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
   case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
      return 1;
   case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
   case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
   case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
   case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
   case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
   case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
   case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
      return 2;
   case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
   case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
      return 3;
   case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
   case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
   case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
   case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
   case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
   default:
      return 4;
   }
}

static void gxm_free_texture(gxm_texture_t *texture)
{
   if (texture)
   {
      if (texture->gxm_rtgt)
         sceGxmDestroyRenderTarget(texture->gxm_rtgt);
      if (texture->depth_UID)
         gpu_free(texture->depth_UID);
      if (texture->palette_UID)
         gpu_free(texture->palette_UID);
      gpu_free(texture->data_UID);
      free(texture);
   }
}

static gxm_texture_t *gxm_create_empty_texture_format(unsigned int w,
      unsigned int h, SceGxmTextureFormat format)
{
   gxm_texture_t *texture;
   void *texture_data;
   int tex_size;
   if (w > GXM_TEX_MAX_SIZE || h > GXM_TEX_MAX_SIZE)
      return NULL;

   texture = malloc(sizeof(*texture));
   if (!texture)
      return NULL;

   memset(texture, 0, sizeof(gxm_texture_t));

   tex_size =  w * h * tex_format_to_bytespp(format);

   /* Allocate a GPU buffer for the texture */
   texture_data = gpu_alloc(
      MemBlockType,
      tex_size,
      SCE_GXM_TEXTURE_ALIGNMENT,
      SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
      &texture->data_UID);

   if (!texture_data)
   {
      free(texture);
      return NULL;
   }

   /* Clear the texture */
   memset(texture_data, 0, tex_size);

   /* Create the gxm texture */
   sceGxmTextureInitLinear(
      &texture->gxm_tex,
      texture_data,
      format,
      w,
      h,
      0);

   if ((format & 0x9f000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8)
   {
      const int pal_size = 256 * sizeof(uint32_t);

      void *texture_palette = gpu_alloc(
         MemBlockType,
         pal_size,
         SCE_GXM_PALETTE_ALIGNMENT,
         SCE_GXM_MEMORY_ATTRIB_READ,
         &texture->palette_UID);

      if (!texture_palette)
      {
         texture->palette_UID = 0;
         gxm_free_texture(texture);
         return NULL;
      }

      memset(texture_palette, 0, pal_size);

      sceGxmTextureSetPalette(&texture->gxm_tex, texture_palette);
   }
   else
      texture->palette_UID = 0;

   return texture;
}

static unsigned int gxm_texture_get_stride(const gxm_texture_t *texture)
{
   return ((sceGxmTextureGetWidth(&texture->gxm_tex) + 7) & ~7)
      * tex_format_to_bytespp(sceGxmTextureGetFormat(&texture->gxm_tex));
}

static void gxm_texture_set_filters(gxm_texture_t *texture,
      SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter)
{
   sceGxmTextureSetMinFilter(&texture->gxm_tex, min_filter);
   sceGxmTextureSetMagFilter(&texture->gxm_tex, mag_filter);
}

static inline void draw_texture_scale_generic(const gxm_texture_t *texture,
      float x, float y, float x_scale, float y_scale)
{
   gxm_texture_vertex_t *vertices = (gxm_texture_vertex_t *)
      gxm_pool_memalign(
      4 * sizeof(gxm_texture_vertex_t), /* 4 vertices */
      sizeof(gxm_texture_vertex_t));

   const float w = x_scale * sceGxmTextureGetWidth(&texture->gxm_tex);
   const float h = y_scale * sceGxmTextureGetHeight(&texture->gxm_tex);

   vertices[0].x = x;
   vertices[0].y = y;
   vertices[0].z = +0.5f;
   vertices[0].u = 0.0f;
   vertices[0].v = 0.0f;

   vertices[1].x = x + w;
   vertices[1].y = y;
   vertices[1].z = +0.5f;
   vertices[1].u = 1.0f;
   vertices[1].v = 0.0f;

   vertices[2].x = x;
   vertices[2].y = y + h;
   vertices[2].z = +0.5f;
   vertices[2].u = 0.0f;
   vertices[2].v = 1.0f;

   vertices[3].x = x + w;
   vertices[3].y = y + h;
   vertices[3].z = +0.5f;
   vertices[3].u = 1.0f;
   vertices[3].v = 1.0f;

   /* Set the texture to the TEXUNIT0 */
   sceGxmSetFragmentTexture(gxm_context, 0, &texture->gxm_tex);

   sceGxmSetVertexStream(gxm_context, 0, vertices);
   sceGxmDraw(gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
         SCE_GXM_INDEX_FORMAT_U16, linearIndices, 4);
}

static void gxm_draw_texture_scale(const gxm_texture_t *texture,
      float x, float y, float x_scale, float y_scale)
{
   void *vertex_wvp_buffer;
   sceGxmSetVertexProgram(gxm_context, gxm_textureVertexProgram);
   sceGxmSetFragmentProgram(gxm_context, gxm_textureFragmentProgram);
   sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);
   sceGxmSetUniformDataF(vertex_wvp_buffer, gxm_textureWvpParam, 0, 16,
         gxm_ortho_matrix);
   draw_texture_scale_generic(texture, x, y, x_scale, y_scale);
}

static inline void draw_texture_tint_part_scale_generic(
      const gxm_texture_t *texture, float x, float y,
      float tex_x, float tex_y, float tex_w, float tex_h,
      float x_scale, float y_scale, unsigned int color)
{
   int n;
   gxm_texture_tint_vertex_t *vertices = (gxm_texture_tint_vertex_t *)
      gxm_pool_memalign(
      4 * sizeof(gxm_texture_tint_vertex_t), /* 4 vertices */
      sizeof(gxm_texture_tint_vertex_t));

   const float w = sceGxmTextureGetWidth(&texture->gxm_tex);
   const float h = sceGxmTextureGetHeight(&texture->gxm_tex);

   const float u0 = tex_x/w;
   const float v0 = tex_y/h;
   const float u1 = (tex_x+tex_w)/w;
   const float v1 = (tex_y+tex_h)/h;

   tex_w *= x_scale;
   tex_h *= y_scale;

   vertices[0].x = x;
   vertices[0].y = y;
   vertices[0].z = +0.5f;
   vertices[0].u = u0;
   vertices[0].v = v0;

   vertices[1].x = x + tex_w;
   vertices[1].y = y;
   vertices[1].z = +0.5f;
   vertices[1].u = u1;
   vertices[1].v = v0;

   vertices[2].x = x;
   vertices[2].y = y + tex_h;
   vertices[2].z = +0.5f;
   vertices[2].u = u0;
   vertices[2].v = v1;

   vertices[3].x = x + tex_w;
   vertices[3].y = y + tex_h;
   vertices[3].z = +0.5f;
   vertices[3].u = u1;
   vertices[3].v = v1;

   for (n = 0; n < 4; n++)
   {
      vertices[n].r = ((color >> 8*0) & 0xFF)/255.0f;
      vertices[n].g = ((color >> 8*1) & 0xFF)/255.0f;
      vertices[n].b = ((color >> 8*2) & 0xFF)/255.0f;
      vertices[n].a = ((color >> 8*3) & 0xFF)/255.0f;
   }

   /* Set the texture to the TEXUNIT0 */
   sceGxmSetFragmentTexture(gxm_context, 0, &texture->gxm_tex);

   sceGxmSetVertexStream(gxm_context, 0, vertices);
   sceGxmDraw(gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
         SCE_GXM_INDEX_FORMAT_U16, linearIndices, 4);
}

static void gxm_draw_texture_tint_part_scale(
      const gxm_texture_t *texture, float x, float y,
      float tex_x, float tex_y, float tex_w, float tex_h,
      float x_scale, float y_scale, unsigned int color)
{
   void *vertex_wvp_buffer;
   sceGxmSetVertexProgram(gxm_context, gxm_textureTintVertexProgram);
   sceGxmSetFragmentProgram(gxm_context,
         gxm_textureTintFragmentProgram);
   sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);
   sceGxmSetUniformDataF(vertex_wvp_buffer, gxm_textureWvpParam, 0, 16,
         gxm_ortho_matrix);
   draw_texture_tint_part_scale_generic(texture, x, y, tex_x, tex_y, tex_w,
         tex_h, x_scale, y_scale, color);
}

static void gxm_draw_texture_scale_rotate(const gxm_texture_t *texture,
      float x, float y, float x_scale, float y_scale, float rad)
{
   float center_y_scaled;
   float center_x_scaled;
   float h;
   float w;
   int i;
   float s;
   float c;
   const float center_x = sceGxmTextureGetWidth(&texture->gxm_tex)  / 2.0f;
   const float center_y = sceGxmTextureGetHeight(&texture->gxm_tex) / 2.0f;
   void *vertex_wvp_buffer;
   gxm_texture_vertex_t *vertices;

   sceGxmSetVertexProgram(gxm_context, gxm_textureVertexProgram);
   sceGxmSetFragmentProgram(gxm_context, gxm_textureFragmentProgram);
   sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);
   sceGxmSetUniformDataF(vertex_wvp_buffer, gxm_textureWvpParam, 0, 16,
         gxm_ortho_matrix);

   vertices = (gxm_texture_vertex_t *)
      gxm_pool_memalign(
      4 * sizeof(gxm_texture_vertex_t), /* 4 vertices */
      sizeof(gxm_texture_vertex_t));

   w = x_scale * sceGxmTextureGetWidth(&texture->gxm_tex);
   h = y_scale * sceGxmTextureGetHeight(&texture->gxm_tex);
   center_x_scaled = x_scale * center_x;
   center_y_scaled = y_scale * center_y;

   vertices[0].x = -center_x_scaled;
   vertices[0].y = -center_y_scaled;
   vertices[0].z = +0.5f;
   vertices[0].u = 0.0f;
   vertices[0].v = 0.0f;

   vertices[1].x = -center_x_scaled + w;
   vertices[1].y = -center_y_scaled;
   vertices[1].z = +0.5f;
   vertices[1].u = 1.0f;
   vertices[1].v = 0.0f;

   vertices[2].x = -center_x_scaled;
   vertices[2].y = -center_y_scaled + h;
   vertices[2].z = +0.5f;
   vertices[2].u = 0.0f;
   vertices[2].v = 1.0f;

   vertices[3].x = -center_x_scaled + w;
   vertices[3].y = -center_y_scaled + h;
   vertices[3].z = +0.5f;
   vertices[3].u = 1.0f;
   vertices[3].v = 1.0f;

   c = cosf(rad);
   s = sinf(rad);
   for (i = 0; i < 4; ++i)
   {
      /* Rotate and translate */
      float _x      = vertices[i].x;
      float _y      = vertices[i].y;
      vertices[i].x = _x*c - _y*s + x + center_x_scaled;
      vertices[i].y = _x*s + _y*c + y + center_y_scaled;
   }

   /* Set the texture to the TEXUNIT0 */
   sceGxmSetFragmentTexture(gxm_context, 0, &texture->gxm_tex);

   sceGxmSetVertexStream(gxm_context, 0, vertices);
   sceGxmDraw(gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
         SCE_GXM_INDEX_FORMAT_U16, linearIndices, 4);
}

static void gxm_draw_array_textured_mat(const gxm_texture_t *texture,
      const gxm_texture_tint_vertex_t *vertices, size_t count, float *mat)
{
   void *vertex_wvp_buffer;
   sceGxmSetVertexProgram(gxm_context, gxm_textureTintVertexProgram);
   sceGxmSetFragmentProgram(gxm_context,
         gxm_textureTintFragmentProgram);

   sceGxmReserveVertexDefaultUniformBuffer(gxm_context, &vertex_wvp_buffer);
   sceGxmSetUniformDataF(vertex_wvp_buffer, gxm_textureWvpParam, 0, 16,
         mat);

   /* Set the texture to the TEXUNIT0 */
   sceGxmSetFragmentTexture(gxm_context, 0, &texture->gxm_tex);

   sceGxmSetVertexStream(gxm_context, 0, vertices);
   sceGxmDraw(gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP,
         SCE_GXM_INDEX_FORMAT_U16, linearIndices, count);
}

/*
 * DISPLAY DRIVER
 */

static const float *gfx_display_gxm_get_default_vertices(void)
{
   return &gxm_vertexes[0];
}

static const float *gfx_display_gxm_get_default_tex_coords(void)
{
   return &gxm_tex_coords[0];
}

static void *gfx_display_gxm_get_default_mvp(void *data)
{
   vita_video_t *vita = (vita_video_t*)data;
   if (!vita)
      return NULL;
   return &vita->mvp_no_rot;
}

static void gfx_display_gxm_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   gxm_texture_tint_vertex_t *vertices;
   unsigned i;
   gxm_texture_t *texture   = NULL;
   const float *vertex              = NULL;
   const float *tex_coord           = NULL;
   const float *color               = NULL;
   vita_video_t             *vita   = (vita_video_t*)data;

   if (!vita || !draw)
      return;

   texture            = (gxm_texture_t*)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!vertex)
      vertex          = &gxm_vertexes[0];
   if (!tex_coord)
      tex_coord       = &gxm_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = &gxm_tex_coords[0];
   if (!texture)
      return;
   if (!color)
      color           = &gxm_colors[0];

   gxm_set_viewport(draw->x, draw->y, draw->width, draw->height);
   vertices = (gxm_texture_tint_vertex_t *)
      gxm_pool_memalign(
         draw->coords->vertices * sizeof(gxm_texture_tint_vertex_t),
         sizeof(gxm_texture_tint_vertex_t));

   for (i = 0; i < draw->coords->vertices; i++)
   {
      vertices[i].x = *vertex++;
      vertices[i].y = *vertex++;
      vertices[i].z = 1.0f;
      vertices[i].u = *tex_coord++;
      vertices[i].v = *tex_coord++;
      vertices[i].r = *color++;
      vertices[i].g = *color++;
      vertices[i].b = *color++;
      vertices[i].a = *color++;
   }

   gxm_draw_array_textured_mat(texture, vertices, draw->coords->vertices,
         vita->mvp_no_rot.data);
}

static void gfx_display_gxm_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   gxm_set_clip_rectangle(x, y, x + width, y + height);
   sceGxmSetRegionClip(gxm_context, SCE_GXM_REGION_CLIP_OUTSIDE, x, y,
         x + width, y + height);
}

static void gfx_display_gxm_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   sceGxmSetRegionClip(gxm_context, SCE_GXM_REGION_CLIP_NONE, 0, 0,
         video_width, video_height);
   clipping_enabled = 0;
   sceGxmSetFrontStencilFunc(
         gxm_context,
         SCE_GXM_STENCIL_FUNC_ALWAYS,
         SCE_GXM_STENCIL_OP_KEEP,
         SCE_GXM_STENCIL_OP_KEEP,
         SCE_GXM_STENCIL_OP_KEEP,
         0xFF,
         0xFF);
}

gfx_display_ctx_driver_t gfx_display_ctx_gxm = {
   gfx_display_gxm_draw,
   NULL,                                        /* draw_pipeline */
   NULL,                                        /* blend_begin   */
   NULL,                                        /* blend_end     */
   gfx_display_gxm_get_default_mvp,
   gfx_display_gxm_get_default_vertices,
   gfx_display_gxm_get_default_tex_coords,
   FONT_DRIVER_RENDER_GXM,
   GFX_VIDEO_DRIVER_GXM,
   "vita2d",
   true,
   gfx_display_gxm_scissor_begin,
   gfx_display_gxm_scissor_end
};

/*
 * FONT DRIVER
 */

static void *gxm_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   unsigned int stride, pitch, j, k;
   const uint8_t         *frame32 = NULL;
   uint8_t                 *tex32 = NULL;
   const struct font_atlas *atlas = NULL;
   vita_font_t              *font = (vita_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->vita                     = (vita_video_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
      goto error;

   font->atlas   = font->font_driver->get_atlas(font->font_data);
   atlas         = font->atlas;

   if (!atlas)
      goto error;

   font->texture = gxm_create_empty_texture_format(
         atlas->width,
         atlas->height,
         SCE_GXM_TEXTURE_FORMAT_U8_R111);

   if (!font->texture)
      goto error;

   gxm_texture_set_filters(font->texture,
         SCE_GXM_TEXTURE_FILTER_POINT,
         SCE_GXM_TEXTURE_FILTER_LINEAR);

   stride  = gxm_texture_get_stride(font->texture);
   tex32   = sceGxmTextureGetData(&font->texture->gxm_tex);
   frame32 = atlas->buffer;
   pitch   = atlas->width;

   for (j = 0; j < atlas->height; j++)
      for (k = 0; k < atlas->width; k++)
         tex32[k + j * stride] = frame32[k + j*pitch];

   font->atlas->dirty = false;

   return font;

error:
   free(font);
   return NULL;
}

static void gxm_font_free(void *data, bool is_threaded)
{
   vita_font_t *font = (vita_font_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (gxm_initialized)
      sceGxmFinish(gxm_context);
   gxm_free_texture(font->texture);

   free(font);
}

static int gxm_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int delta_x       = 0;
   vita_font_t *font = (vita_font_t*)data;
   const struct font_glyph* (*get_glyph)(void*, uint32_t)
                     = font->font_driver->get_glyph;
   void *font_data   = font->font_data;

   if (!font)
      return 0;

   glyph_q = get_glyph(font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph = NULL;
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = get_glyph(font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void gxm_font_render_line(
      vita_video_t *vita,
      vita_font_t *font,
      const struct font_glyph* glyph_q,
      const char *msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width,
      unsigned height,
      int pre_x,
      unsigned text_align)
{
   int i;
   int x           = pre_x;
   int y           = roundf((1.0f - pos_y) * height);
   int delta_x     = 0;
   int delta_y     = 0;
   const char* msg_end = msg + msg_len;
   const struct font_glyph* (*get_glyph)(void*, uint32_t)
                     = font->font_driver->get_glyph;
   void *font_data   = font->font_data;

   /* For right/center alignment, compute width with a lightweight pass
    * that only accumulates advance_x — avoids the redundant glyph lookups
    * and atlas dirty checks that gxm_font_get_message_width 
    * would repeat. */
   if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
   {
      int width_accum      = 0;
      const char *scan     = msg;
      const char *scan_end = msg_end;
      while (scan < scan_end)
      {
         const struct font_glyph *glyph;
         uint32_t code       = utf8_walk(&scan);
         if (!(glyph = get_glyph(font_data, code)))
            if (!(glyph = glyph_q))
               continue;
         width_accum += glyph->advance_x;
      }

      if (text_align == TEXT_ALIGN_RIGHT)
         x -= (int)(width_accum * scale);
      else
         x -= (int)(width_accum * scale) / 2;
   }

   for (i = 0; i < msg_len; i++)
   {
      int j;
      int off_x, off_y, tex_x, tex_y, width, height;
      const struct font_glyph *glyph = NULL;
      const char *msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = get_glyph(font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      if (font->atlas->dirty)
      {
        unsigned int stride    = gxm_texture_get_stride(font->texture);
        uint8_t *tex32         = sceGxmTextureGetData(&font->texture->gxm_tex);
        const uint8_t *frame32 = font->atlas->buffer;
        unsigned int pitch     = font->atlas->width;
        /* Copy only the dirty rectangle tracked by the font
         * renderers, one row at a time */
        unsigned int x0        = font->atlas->dirty_x0;
        unsigned int y0        = font->atlas->dirty_y0;
        unsigned int x1        = font->atlas->dirty_x1;
        unsigned int y1        = font->atlas->dirty_y1;

        if (x1 <= x0 || y1 <= y0 || x1 > pitch || y1 > font->atlas->height)
        {
           x0 = 0;
           y0 = 0;
           x1 = pitch;
           y1 = font->atlas->height;
        }

        for (j = y0; j < y1; j++)
           memcpy(tex32 + x0 + j * stride,
                  frame32 + x0 + j * pitch, x1 - x0);

         font->atlas->dirty = false;
      }

      gxm_draw_texture_tint_part_scale(font->texture,
            x + (off_x + delta_x) * scale,
            y + (off_y + delta_y) * scale,
            tex_x, tex_y, width, height,
            scale,
            scale,
            color);

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void gxm_font_render_message(
      vita_video_t *vita,
      vita_font_t *font, const char *msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   int x                                  = roundf(pos_x * width);
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / vita->vp.height;
   for (;;)
   {
      size_t msg_len;
      const char *delim = msg;
      while (*delim && *delim != '\n')
         delim++;
      msg_len = delim - msg;
      /* Draw the line */
      gxm_font_render_line(vita, font, glyph_q, msg, msg_len,
            scale, color, pos_x, pos_y - (float)lines * line_height,
            width, height, x, text_align);
      if (!*delim)
         break;
      msg += msg_len + 1;
      lines++;
   }
}

static void gxm_set_viewport_wrapper(void *data, unsigned vp_width,
      unsigned vp_height, bool force_full, bool allow_rotate)
{
   struct video_ortho ortho  = {0, 1, 0, 1, -1, 1};
   vita_video_t *vita        = (vita_video_t*)data;

   vita->vp.full_width  = vp_width;
   vita->vp.full_height = vp_height;
   video_driver_update_viewport(&vita->vp, force_full,
   vita->keep_aspect, true);

   gxm_set_viewport(vita->vp.x, vita->vp.y, vita->vp.width,
   vita->vp.height);
   gxm_set_projection(vita, &ortho, allow_rotate);
}

static void gxm_font_render_msg(
      void *userdata,
      void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params)
{
   int drop_x, drop_y;
   unsigned color, r, g, b, alpha;
   enum text_alignment text_align;
   float x, y, scale, drop_mod, drop_alpha;
   bool full_screen                 = false;
   vita_video_t *vita               = (vita_video_t *)userdata;
   vita_font_t *font                = (vita_font_t *)data;
   unsigned width                   = vita->video_width;
   unsigned height                  = vita->video_height;

   if (!font || !msg || !*msg)
      return;

   if (params)
   {
      x                       = params->x;
      y                       = params->y;
      scale                   = params->scale;
      full_screen             = params->full_screen;
      text_align              = params->text_align;
      drop_x                  = params->drop_x;
      drop_y                  = params->drop_y;
      drop_mod                = params->drop_mod;
      drop_alpha              = params->drop_alpha;
      r              = FONT_COLOR_GET_RED(params->color);
      g              = FONT_COLOR_GET_GREEN(params->color);
      b              = FONT_COLOR_GET_BLUE(params->color);
      alpha               = FONT_COLOR_GET_ALPHA(params->color);
      color               = RGBA8(r,g,b,alpha);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      x                       = video_msg_pos_x;
      y                       = video_msg_pos_y;
      scale                   = 1.0f;
      full_screen             = true;
      text_align              = TEXT_ALIGN_LEFT;

      r                       = (video_msg_color_r * 255);
      g                       = (video_msg_color_g * 255);
      b                       = (video_msg_color_b * 255);
      alpha          = 255;
      color            = RGBA8(r,g,b,alpha);

      drop_x                  = -2;
      drop_y                  = -2;
      drop_mod                = 0.3f;
      drop_alpha              = 1.0f;
   }

   gxm_set_viewport_wrapper(vita, width, height, full_screen, false);

   if (drop_x || drop_y)
   {
      unsigned r_dark         = r * drop_mod;
      unsigned g_dark         = g * drop_mod;
      unsigned b_dark         = b * drop_mod;
      unsigned alpha_dark     = alpha * drop_alpha;
      unsigned color_dark     = RGBA8(r_dark,g_dark,b_dark,alpha_dark);

      gxm_font_render_message(vita, font, msg, scale, color_dark,
            x + scale * drop_x / width, y +
            scale * drop_y / height, width, height, text_align);
   }

   gxm_font_render_message(vita, font, msg, scale,
         color, x, y, width, height, text_align);
}

static const struct font_glyph *gxm_font_get_glyph(
      void *data, uint32_t code)
{
   vita_font_t *font = (vita_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_data, code);
   return NULL;
}

static bool gxm_font_get_line_metrics(void* data,
   struct font_line_metrics **metrics)
{
   vita_font_t *font = (vita_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t gxm_font = {
   gxm_font_init,
   gxm_font_free,
   gxm_font_render_msg,
   "vita2d",
   gxm_font_get_glyph,
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   gxm_font_get_message_width,
   gxm_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static void *gxm_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned int color;
   unsigned temp_width                    = PSP_FB_WIDTH;
   unsigned temp_height                   = PSP_FB_HEIGHT;
   vita_video_t *vita                     = (vita_video_t *)
      calloc(1, sizeof(vita_video_t));

   if (!vita)
      return NULL;

   *input             = NULL;
   *input_data        = NULL;

   gxm_init_internal((1 * 1024 * 1024), SCE_GXM_MULTISAMPLE_4X,
   (sceKernelGetModelForCDialog() == SCE_KERNEL_MODEL_VITATV)
   ? GXM_VIDEO_MODE_1280x720 : GXM_VIDEO_MODE_960x544 );
   /* TODO/FIXME - is this color just black? */
   color = RGBA8(0x00, 0x00, 0x00, 0xFF);
   clear_color_u  = color;
   vblank_wait    = video->vsync;

   if (video->rgb32)
      vita->format    = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB;
   else
      vita->format    = SCE_GXM_TEXTURE_FORMAT_R5G6B5;

   temp_width         = video_mode_data.width;
   temp_height        = video_mode_data.height;

   vita->fullscreen   = video->fullscreen;

   vita->texture      = NULL;
   vita->menu.texture = NULL;
   vita->menu.active  = 0;
   vita->menu.width   = 0;
   vita->menu.height  = 0;

   vita->vsync        = video->vsync;
   vita->rgb32        = video->rgb32;

   vita->tex_filter   = video->smooth
      ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;

   vita->video_width  = temp_width;
   vita->video_height = temp_height;

   video_driver_set_output_size(temp_width, temp_height);
   gxm_set_viewport_wrapper(vita, temp_width, temp_height, false, true);

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      void *pspinput       = input_driver_init_wrap(&input_psp,
            settings->arrays.input_joypad_driver);
      *input               = pspinput ? &input_psp : NULL;
      *input_data          = pspinput;
   }

   vita->keep_aspect        = true;
   vita->should_resize      = true;
#ifdef HAVE_OVERLAY
   vita->overlay_enable     = false;
#endif
   font_driver_init_osd(vita,
         video,
         false,
         video->is_threaded,
         FONT_DRIVER_RENDER_GXM);

   return vita;
}

#ifdef HAVE_OVERLAY
static void gxm_free_overlay(vita_video_t *vita)
{
   unsigned i;

   if (gxm_initialized)
      sceGxmFinish(gxm_context);

   for (i = 0; i < vita->overlays; i++)
      gxm_free_texture(vita->overlay[i].tex);
   free(vita->overlay);
   vita->overlay = NULL;
   vita->overlays = 0;
}
#endif

static void gxm_update_viewport(vita_video_t* vita)
{
   unsigned temp_width  = video_mode_data.width;
   unsigned temp_height = video_mode_data.height;
   bool is_rotated      = (vita->rotation == ORIENTATION_VERTICAL)
                       || (vita->rotation == ORIENTATION_FLIPPED_ROTATED);

   /* For rotated displays, swap dimensions before viewport calculation */
   if (is_rotated && vita->keep_aspect)
   {
      vita->vp.full_width  = temp_height;
      vita->vp.full_height = temp_width;
   }
   else
   {
      vita->vp.full_width  = temp_width;
      vita->vp.full_height = temp_height;
   }

   video_driver_update_viewport(&vita->vp, false, vita->keep_aspect, true);

   /* For rotated displays, swap x and y */
   if (is_rotated && vita->keep_aspect)
   {
      unsigned tmp = vita->vp.x;
      vita->vp.x   = vita->vp.y;
      vita->vp.y   = tmp;
   }

   /* Ensure even dimensions */
   vita->vp.width      += vita->vp.width & 0x1;
   vita->vp.height     += vita->vp.height & 0x1;

   vita->should_resize  = false;
}

static bool gxm_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   void *tex_p;
   gxm_display_data_t displayData;
   vita_video_t *vita                     = (vita_video_t *)data;
   unsigned temp_width                    = PSP_FB_WIDTH;
   unsigned temp_height                   = PSP_FB_HEIGHT;
#ifdef HAVE_MENU
   bool menu_is_alive                     = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                    = video_info->widgets_active;
#endif
   bool statistics_show                   = video_info->statistics_show;
   struct font_params *osd_params         = (struct font_params*)&video_info->osd_stat_params;

   if (frame)
   {
      if (!(vita->texture && sceGxmTextureGetData(&vita->texture->gxm_tex)==frame))
      {
         unsigned i;
         unsigned int stride;

         if ((width != vita->width || height != vita->height) && vita->texture)
         {
            if (gxm_initialized)
               sceGxmFinish(gxm_context);
            gxm_free_texture(vita->texture);
            vita->texture = NULL;
         }

         if (!vita->texture)
         {
            vita->width   = width;
            vita->height  = height;
            vita->texture = gxm_create_empty_texture_format(width, height,
                  vita->format);
            gxm_texture_set_filters(vita->texture, vita->tex_filter,
                  vita->tex_filter);
         }
         tex_p  = sceGxmTextureGetData(&vita->texture->gxm_tex);
         stride = gxm_texture_get_stride(vita->texture);

         if (vita->format == SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB)
         {
            const uint32_t *frame32;
            uint32_t *tex32;
            stride                     /= 4;
            pitch                      /= 4;
            tex32 = tex_p;
            frame32 = frame;

            for (i = 0; i < height; i++)
               memcpy_neon(&tex32[i*stride], &frame32[i*pitch],
                     pitch*sizeof(uint32_t));
         }
         else
         {
            const uint16_t *frame16;
            uint16_t *tex16;
            stride                 /= 2;
            pitch                  /= 2;
            tex16 = tex_p;
            frame16 = frame;

            for (i = 0; i < height; i++)
               memcpy_neon(&tex16[i*stride], &frame16[i*pitch],
                     width*sizeof(uint16_t));
         }
      }
   }

   if (vita->should_resize)
      gxm_update_viewport(vita);

   temp_width      = video_mode_data.width;
   temp_height     = video_mode_data.height;

   pool_index = 0;
   sceGxmBeginScene(
         gxm_context,
         0,
         renderTarget,
         NULL,
         NULL,
         displayBufferSync[backBufferIndex],
         &displaySurface[backBufferIndex],
         &depthSurface);

   drawing = 1;
   /* in the current way, the library keeps the region clip across scenes */
   if (clipping_enabled)
      gxm_set_clip_rectangle(clip_rect_x_min, clip_rect_y_min,
            clip_rect_x_max, clip_rect_y_max);

   gxm_draw_rectangle(0,0, temp_width, temp_height,
      clear_color_u);

   if (vita->texture)
   {
      if (vita->fullscreen)
         gxm_draw_texture_scale(vita->texture,
               0, 0,
               temp_width  / (float)vita->width,
               temp_height / (float)vita->height);
      else
      {
         const float radian = 270 * 0.0174532925f;
         const float rad = vita->rotation * radian;
         float scalex = vita->vp.width  / (float)vita->width;
         float scaley = vita->vp.height / (float)vita->height;
         gxm_draw_texture_scale_rotate(vita->texture,vita->vp.x,
               vita->vp.y, scalex, scaley, rad);
      }
   }

   if (vita->menu.active)
   {
#ifdef HAVE_MENU
      menu_driver_frame(menu_is_alive, video_info);
#endif

      if (vita->menu.texture)
      {
         if (vita->fullscreen)
            gxm_draw_texture_scale(vita->menu.texture,
                  0, 0,
                  temp_width  / (float)vita->menu.width,
                  temp_height / (float)vita->menu.height);
         else
         {
            if (vita->menu.width > vita->menu.height)
            {
               float scale = temp_height / (float)vita->menu.height;
               float w = vita->menu.width * scale;
               gxm_draw_texture_scale(vita->menu.texture,
                     temp_width / 2.0f - w/2.0f, 0.0f,
                     scale, scale);
            }
            else
            {
               float scale = temp_width / (float)vita->menu.width;
               float h = vita->menu.height * scale;
               gxm_draw_texture_scale(vita->menu.texture,
                     0.0f, temp_height / 2.0f - h/2.0f,
                     scale, scale);
            }
         }
      }
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(vita, video_info->stat_text, video_info->stat_text_len,
               osd_params, NULL);
   }

#ifdef HAVE_OVERLAY
   if (vita->overlay_enable)
      gxm_render_overlay(vita);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif
   if (msg && *msg)
      font_driver_render_msg(vita, msg, strlen(msg), NULL, NULL);

   sceGxmEndScene(gxm_context, NULL, NULL);
   drawing = 0;

   sceGxmPadHeartbeat(&displaySurface[backBufferIndex],
         displayBufferSync[backBufferIndex]);
   /* queue the display swap for this frame */
   displayData.address = displayBufferData[backBufferIndex];
   sceGxmDisplayQueueAddEntry(
      displayBufferSync[frontBufferIndex],   /* OLD fb */
      displayBufferSync[backBufferIndex], /* NEW fb */
      &displayData);

   /* update buffer indices */
   frontBufferIndex = backBufferIndex;
   backBufferIndex = (backBufferIndex + 1) % DISPLAY_BUFFER_COUNT;

   return true;
}

static void gxm_set_nonblock_state(void *data, bool toggle,
   bool c, unsigned d)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->vsync = !toggle;
      vblank_wait = vita->vsync;
   }
}

/* Stubs */
static bool gxm_alive(void *data)     { return true; }
static bool gxm_focus(void *data) { return true; }
static bool gxm_suppress_screensaver(void *a, bool b) { return false; }
static bool gxm_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void gxm_free(void *data)
{
   unsigned int i;
   vita_video_t *vita = (vita_video_t *)data;

   if (gxm_initialized)
   {
      /* wait until rendering is done */
      sceGxmFinish(gxm_context);
      /* clean up allocations */
      sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher,
            gxm_colorVertexProgram);
      sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher,
            gxm_textureVertexProgram);

      gxm_free_fragment_programs(&gxm_fragmentPrograms.blend_mode_normal);
      gxm_free_fragment_programs(&gxm_fragmentPrograms.blend_mode_add);

      gpu_free(linearIndicesUid);

      /* wait until display queue is finished before deallocating display buffers */
      sceGxmDisplayQueueFinish();

      /* clean up display queue */
      for (i = 0; i < DISPLAY_BUFFER_COUNT; i++)
      {
         /* clear the buffer then deallocate */
         memset(displayBufferData[i], 0,
               video_mode_data.height*video_mode_data.stride*4);
         gpu_free(displayBufferUid[i]);

         /* destroy the sync object */
         sceGxmSyncObjectDestroy(displayBufferSync[i]);
      }

      /* free the depth and stencil buffer */
      gpu_free(depthBufferUid);
      gpu_free(stencilBufferUid);

      /* unregister programs and destroy shader patcher */
      sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorFragmentProgramId);
      sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorVertexProgramId);
      sceGxmShaderPatcherUnregisterProgram(shaderPatcher,
            textureFragmentProgramId);
      sceGxmShaderPatcherUnregisterProgram(shaderPatcher,
            textureTintFragmentProgramId);
      sceGxmShaderPatcherUnregisterProgram(shaderPatcher,
            textureTintVertexProgramId);
      sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureVertexProgramId);

      sceGxmShaderPatcherDestroy(shaderPatcher);
      fragment_usse_free(patcherFragmentUsseUid);
      vertex_usse_free(patcherVertexUsseUid);
      gpu_free(patcherBufferUid);

      /* destroy the render target */
      sceGxmDestroyRenderTarget(renderTarget);
      renderTarget = NULL;

      /* destroy the gxm_context */
      sceGxmDestroyContext(gxm_context);
      fragment_usse_free(fragmentUsseRingBufferUid);
      gpu_free(fragmentRingBufferUid);
      gpu_free(vertexRingBufferUid);
      gpu_free(vdmRingBufferUid);
      free(contextParams.hostMem);

      gpu_free(poolUid);

      /* terminate libgxm */
      sceGxmTerminate();

      gxm_initialized = 0;
   }

   if (vita->menu.texture)
   {
      gxm_free_texture(vita->menu.texture);
      vita->menu.texture = NULL;
   }

   if (vita->texture)
   {
      gxm_free_texture(vita->texture);
      vita->texture = NULL;
   }

   font_driver_free_osd();
}

static void gxm_set_projection(vita_video_t *vita,
      struct video_ortho *ortho, bool allow_rotate)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;

   /* Calculate projection. */
   matrix_4x4_ortho(vita->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      vita->mvp = vita->mvp_no_rot;
      return;
   }

   radians                 = M_PI * vita->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(vita->mvp, rot, vita->mvp_no_rot);
}

static void gxm_set_rotation(void *data,
      unsigned rotation)
{
  vita_video_t *vita       = (vita_video_t*)data;
  struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

  if (!vita)
     return;

  vita->rotation           = rotation;
  vita->should_resize      = true;
  gxm_set_projection(vita, &ortho, true);
}

static void gxm_viewport_info(void *data,
      struct video_viewport *vp)
{
    vita_video_t *vita = (vita_video_t*)data;

    if (vita)
       *vp = vita->vp;
}

static void gxm_set_filtering(void *data, unsigned index,
   bool smooth, bool ctx_scaling)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->tex_filter = smooth
         ? SCE_GXM_TEXTURE_FILTER_LINEAR
         : SCE_GXM_TEXTURE_FILTER_POINT;
      gxm_texture_set_filters(vita->texture,vita->tex_filter,
            vita->tex_filter);
   }
}

static void gxm_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   vita_video_t *vita  = (vita_video_t*)data;

   if (!vita)
      return;
   vita->keep_aspect   = true;
   vita->should_resize = true;
}

static void gxm_apply_state_changes(void *data)
{
  vita_video_t *vita = (vita_video_t*)data;

  if (vita)
     vita->should_resize = true;
}

static void gxm_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   unsigned i;
   void *tex_p;
   unsigned int stride;
   vita_video_t *vita = (vita_video_t*)data;

   /* Recreate when either dimension changes, not both.  The original
    * condition used && between the width/height comparisons, so a
    * resize that only changed one dimension (e.g. 480x272 -> 480x544
    * when the menu toggles portrait mode, or a width-only change on
    * menu-size setting updates) would skip the destroy-recreate step;
    * the subsequent per-pixel write below would then index past the
    * old texture's allocation (new size > old) or leave stale border
    * pixels (new size < old). */
   if (     vita->menu.texture
         && (   width  != (unsigned)vita->menu.width
             || height != (unsigned)vita->menu.height))
   {
      if (gxm_initialized)
         sceGxmFinish(gxm_context);
      gxm_free_texture(vita->menu.texture);
      vita->menu.texture = NULL;
   }

   if (!vita->menu.texture)
   {
      if (rgb32)
         vita->menu.texture = gxm_create_empty_texture_format(width,
               height, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);
      else
         vita->menu.texture = gxm_create_empty_texture_format(
               width, height, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA);
      vita->menu.width      = width;
      vita->menu.height     = height;
   }

   gxm_texture_set_filters(vita->menu.texture,
         SCE_GXM_TEXTURE_FILTER_LINEAR,
         SCE_GXM_TEXTURE_FILTER_LINEAR);

   tex_p  = sceGxmTextureGetData(&vita->menu.texture->gxm_tex);
   stride = gxm_texture_get_stride(vita->menu.texture);

   /* Copy row-by-row via memcpy rather than pixel-by-pixel.  The
    * texture stride (destination) can differ from the source pitch
    * (width * bpp) for alignment reasons, so a single memcpy of the
    * whole buffer isn't safe when stride != width*bpp, but a per-row
    * memcpy handles both cases and is materially faster than the
    * previous nested loop with per-pixel indexing. */
   if (rgb32)
   {
      uint32_t       *tex32   = (uint32_t*)tex_p;
      const uint32_t *frame32 = (const uint32_t*)frame;
      size_t          rowlen  = (size_t)width * 4;

      stride /= 4;
      for (i = 0; i < height; i++)
         memcpy(tex32 + i * stride, frame32 + i * width, rowlen);
   }
   else
   {
      uint16_t       *tex16   = (uint16_t*)tex_p;
      const uint16_t *frame16 = (const uint16_t*)frame;
      size_t          rowlen  = (size_t)width * 2;

      stride /= 2;
      for (i = 0; i < height; i++)
         memcpy(tex16 + i * stride, frame16 + i * width, rowlen);
   }
}

static void gxm_set_texture_enable(void *data, bool state, bool full_screen)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->menu.active  = state;
}

static uintptr_t gxm_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   unsigned int stride, pitch, j;
   uint32_t             *tex32    = NULL;
   const uint32_t *frame32        = NULL;
   struct texture_image *image    = (struct texture_image*)data;
   gxm_texture_t *texture = gxm_create_empty_texture_format(
         image->width, image->height,
         SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);

   if (!texture)
      return 0;

   if (    (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR)
        || (filter_type == TEXTURE_FILTER_LINEAR))
      gxm_texture_set_filters(texture,
            SCE_GXM_TEXTURE_FILTER_LINEAR,
            SCE_GXM_TEXTURE_FILTER_LINEAR);

   stride                      = gxm_texture_get_stride(texture);
   stride                     /= 4;

   tex32                       = sceGxmTextureGetData(&texture->gxm_tex);
   frame32                     = image->pixels;
   pitch                       = image->width;

   for (j = 0; j < image->height; j++)
         memcpy_neon(
               &tex32[j*stride],
               &frame32[j*pitch],
               pitch * sizeof(uint32_t));

   return (uintptr_t)texture;
}

static void gxm_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   gxm_texture_t *texture = (gxm_texture_t*)handle;
   if (!texture)
      return;

   /* TODO: We really want to defer this deletion instead,
    * but this will do for now. */
   if (gxm_initialized)
      sceGxmFinish(gxm_context);
   gxm_free_texture(texture);

#if 0
   free(texture);
#endif
}

static bool gxm_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *framebuffer)
{
   vita_video_t *vita = (vita_video_t*)data;

   if (     !vita->texture
         || (vita->width  != framebuffer->width)
         || (vita->height != framebuffer->height))
   {
      if (vita->texture)
      {
         if (gxm_initialized)
            sceGxmFinish(gxm_context);
         gxm_free_texture(vita->texture);
         vita->texture = NULL;
      }

      vita->width   = framebuffer->width;
      vita->height  = framebuffer->height;
      vita->texture = gxm_create_empty_texture_format(
            vita->width, vita->height, vita->format);
      gxm_texture_set_filters(vita->texture,
            vita->tex_filter,vita->tex_filter);
   }

   framebuffer->data         = sceGxmTextureGetData(&vita->texture->gxm_tex);
   framebuffer->pitch        = gxm_texture_get_stride(vita->texture);
   framebuffer->format       = vita->rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888
      : RETRO_PIXEL_FORMAT_RGB565;
   framebuffer->memory_flags = 0;

   return true;
}

static uint32_t gxm_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

   return flags;
}

/* --- GPU-native BCn compressed-texture upload --- */
/* The Vita GPU (SGX543) samples UBC1/UBC2/UBC3, which are exactly
 * BC1/BC2/BC3 (DXT1/DXT3/DXT5).  Nothing above BC3 exists here (no UBC for
 * BC6H/BC7, and UBC4/UBC5 are single/two-channel only). */
static bool gxm_bc_to_ubc(enum texture_gpu_format fmt,
      SceGxmTextureFormat *out, unsigned *block_bytes)
{
   switch (fmt)
   {
      case TEXTURE_GPU_FORMAT_BC1:
         *out         = SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR;
         *block_bytes = 8;
         return true;
      case TEXTURE_GPU_FORMAT_BC2:
         *out         = SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR;
         *block_bytes = 16;
         return true;
      case TEXTURE_GPU_FORMAT_BC3:
         *out         = SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR;
         *block_bytes = 16;
         return true;
      default:
         break;
   }
   return false;
}

static bool gxm_supports_texture_format(void *data,
      enum texture_gpu_format fmt)
{
   unsigned bb;
   SceGxmTextureFormat gxm;
   return gxm_bc_to_ubc(fmt, &gxm, &bb);
}

static uintptr_t gxm_load_texture_compressed(void *video_data,
      const struct texture_compressed *tc, bool threaded,
      enum texture_filter_type filter_type)
{
   unsigned i;
   gxm_texture_t      *texture;
   void                *tex_data;
   SceGxmTextureFormat  gxm_fmt   = 0;
   unsigned             block_bytes = 16;
   size_t               total     = 0;
   size_t               off       = 0;
   int                  err;

   (void)video_data;
   (void)threaded;

   if (!tc || tc->num_mips == 0)
      return 0;
   if (!gxm_bc_to_ubc(tc->format, &gxm_fmt, &block_bytes))
      return 0;
   if (tc->mips[0].width > GXM_TEX_MAX_SIZE || tc->mips[0].height > GXM_TEX_MAX_SIZE)
      return 0;

   if (!(texture = malloc(sizeof(*texture))))
      return 0;
   memset(texture, 0, sizeof(*texture));

   for (i = 0; i < tc->num_mips; i++)
      total += tc->mips[i].size;

   tex_data = gpu_alloc(
         MemBlockType,
         total,
         SCE_GXM_TEXTURE_ALIGNMENT,
         SCE_GXM_MEMORY_ATTRIB_READ,
         &texture->data_UID);

   if (!tex_data)
   {
      free(texture);
      return 0;
   }

   /* Mip chain is copied contiguously; block-compressed layout is derived
    * by GXM from the UBC format + dimensions. */
   for (i = 0; i < tc->num_mips; i++)
   {
      memcpy((uint8_t*)tex_data + off, tc->mips[i].data, tc->mips[i].size);
      off += tc->mips[i].size;
   }

   /* Block-compressed data uses the linear texture type. GXM stores the
    * memory layout (linear/swizzled/tiled) as a property of the texture,
    * and a linear-typed texture is sampled as plain row-major blocks --
    * exactly the DDS block order copied above, so no Morton reorder is
    * needed. Verified against Vita3K (the reference emulator), which reads
    * linear/swizzled/tiled BCn per that type field; Sony's GXT tooling
    * emits the swizzled type, but that is a tooling choice, not a hardware
    * requirement. Should a GXM revision refuse InitLinear for a UBC format,
    * the err check below returns 0 and video_driver_texture_load falls back
    * to the CPU decode path. */
   err = sceGxmTextureInitLinear(
         &texture->gxm_tex,
         tex_data,
         gxm_fmt,
         tc->mips[0].width,
         tc->mips[0].height,
         (tc->num_mips > 0) ? (tc->num_mips - 1) : 0);

   if (err < 0)
   {
      gxm_free_texture(texture);
      return 0;
   }

   if (     filter_type == TEXTURE_FILTER_MIPMAP_LINEAR
         || filter_type == TEXTURE_FILTER_LINEAR)
      gxm_texture_set_filters(texture,
            SCE_GXM_TEXTURE_FILTER_LINEAR,
            SCE_GXM_TEXTURE_FILTER_LINEAR);

   return (uintptr_t)texture;
}

static const video_poke_interface_t vita_poke_interface = {
   gxm_get_flags,
   gxm_load_texture,
   gxm_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   gxm_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   gxm_set_aspect_ratio,
   gxm_apply_state_changes,
   gxm_set_texture_frame,
   gxm_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   gxm_get_current_sw_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   gxm_supports_texture_format,
   gxm_load_texture_compressed
};

static void gxm_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &vita_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool gxm_widgets_enabled(void *data) { return true; }
#endif

#ifdef HAVE_OVERLAY
static void gxm_overlay_tex_geom(void *data, unsigned image, float x,
      float y, float w, float h);
static void gxm_overlay_vertex_geom(void *data, unsigned image, float x,
      float y, float w, float h);

static bool gxm_overlay_load(void *data, const void *image_data, unsigned num_images)
{
   unsigned i,j,k;
   unsigned int stride, pitch;
   vita_video_t *vita = (vita_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)image_data;

   gxm_free_overlay(vita);
   vita->overlay = (struct vita_overlay_data*)calloc(num_images,
         sizeof(*vita->overlay));
   if (!vita->overlay)
      return false;

   vita->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      uint32_t *tex32;
      const uint32_t *frame32;
      struct vita_overlay_data *o = (struct vita_overlay_data*)&vita->overlay[i];
      o->width   = images[i].width;
      o->height  = images[i].height;
      o->tex     = gxm_create_empty_texture_format(o->width, o->height,
            SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
      gxm_texture_set_filters(o->tex, SCE_GXM_TEXTURE_FILTER_LINEAR,
            SCE_GXM_TEXTURE_FILTER_LINEAR);
      stride     = gxm_texture_get_stride(o->tex);
      stride    /= 4;
      tex32      = sceGxmTextureGetData(&o->tex->gxm_tex);
      frame32    = images[i].pixels;
      pitch      = o->width;
      for (j = 0; j < o->height; j++)
         for (k = 0; k < o->width; k++)
            tex32[k + j*stride] = frame32[k + j*pitch];

      gxm_overlay_tex_geom(vita, i, 0, 0, 1, 1); /* Default. Stretch to whole screen. */
      gxm_overlay_vertex_geom(vita, i, 0, 0, 1, 1);
      vita->overlay[i].alpha_mod = 1.0f;
   }

   return true;
}

static void gxm_overlay_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   vita_video_t          *vita = (vita_video_t*)data;
   struct vita_overlay_data *o = NULL;

   if (vita)
      o = (struct vita_overlay_data*)&vita->overlay[image];

   if (o)
   {
      o->tex_x = x;
      o->tex_y = y;
      o->tex_w = w*o->width;
      o->tex_h = h*o->height;
   }
}

static void gxm_overlay_vertex_geom(void *data, unsigned image,
         float x, float y, float w, float h)
{
   vita_video_t          *vita = (vita_video_t*)data;
   struct vita_overlay_data *o = NULL;

   /* Flipped, so we preserve top-down semantics. */
   /* y = 1.0f - y;
      h = -h;
    */

   if (vita)
      o = (struct vita_overlay_data*)&vita->overlay[image];

   if (o)
   {
      o->w = w * video_mode_data.width  / o->width;
      o->h = h * video_mode_data.height / o->height;
      o->x = video_mode_data.width  * (1 - w) / 2 + x;
      o->y = video_mode_data.height * (1 - h) / 2 + y;
   }
}

static void gxm_overlay_enable(void *data, bool state)
{
   vita_video_t *vita   = (vita_video_t*)data;
   vita->overlay_enable = state;
}

static void gxm_overlay_full_screen(void *data, bool enable)
{
   vita_video_t *vita        = (vita_video_t*)data;
   vita->overlay_full_screen = enable;
}

static void gxm_overlay_set_alpha(void *data, unsigned image, float mod)
{
   vita_video_t *vita             = (vita_video_t*)data;
   vita->overlay[image].alpha_mod = mod;
}

static void gxm_render_overlay(void *data)
{
   unsigned i;
   vita_video_t *vita = (vita_video_t*)data;

   for (i = 0; i < vita->overlays; i++)
      gxm_draw_texture_tint_part_scale(vita->overlay[i].tex,
            vita->overlay[i].x,
            vita->overlay[i].y,
            vita->overlay[i].tex_x,
            vita->overlay[i].tex_y,
            vita->overlay[i].tex_w,
            vita->overlay[i].tex_h,
            vita->overlay[i].w,
            vita->overlay[i].h,
            RGBA8(0xFF,0xFF,0xFF,(uint8_t)(vita->overlay[i].alpha_mod * 255.0f)));
}

static const video_overlay_interface_t gxm_overlay_interface = {
   gxm_overlay_enable,
   gxm_overlay_load,
   gxm_overlay_tex_geom,
   gxm_overlay_vertex_geom,
   gxm_overlay_full_screen,
   gxm_overlay_set_alpha,
};

static void gxm_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   *iface = &gxm_overlay_interface;
}
#endif

video_driver_t video_gxm = {
   gxm_gfx_init,
   gxm_frame,
   gxm_set_nonblock_state,
   gxm_alive,
   gxm_focus,
   gxm_suppress_screensaver,
   NULL, /* has_windowed */
   gxm_set_shader,
   gxm_free,
   "vita2d",
   gxm_set_viewport_wrapper,
   gxm_set_rotation,
   gxm_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   gxm_get_overlay_interface,
#endif
   gxm_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   gxm_widgets_enabled
#endif
};
