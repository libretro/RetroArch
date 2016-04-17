/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel
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

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <3ds.h>

#include <retro_inline.h>

#include "ctr_gu.h"

#include "../../command_event.h"
#include "../../general.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../performance.h"

#define CTR_TOP_FRAMEBUFFER_WIDTH    400
#define CTR_TOP_FRAMEBUFFER_HEIGHT   240
#define CTR_GPU_FRAMEBUFFER         ((void*)0x1F119400)
#define CTR_GPU_DEPTHBUFFER         ((void*)0x1F370800)

extern const u8 ctr_sprite_shbin[];
extern const u32 ctr_sprite_shbin_size;

typedef struct
{
   float v;
   float u;
   float y;
   float x;
} ctr_scale_vector_t;

typedef struct
{
   s16 x0, y0, x1, y1;
   s16 u0, v0, u1, v1;
} ctr_vertex_t;

typedef struct ctr_video
{
   struct
   {
      uint32_t* display_list;
      int display_list_size;
      void* texture_linear;
      void* texture_swizzled;
      int texture_width;
      int texture_height;
      ctr_scale_vector_t scale_vector;
      ctr_vertex_t* frame_coords;
   }menu;

   uint32_t* display_list;
   int display_list_size;
   void* texture_linear;
   void* texture_swizzled;
   int texture_width;
   int texture_height;

   ctr_scale_vector_t scale_vector;
   ctr_vertex_t* frame_coords;

   DVLB_s*         dvlb;
   shaderProgram_s shader;

   video_viewport_t vp;

   bool rgb32;
   bool vsync;
   bool smooth;
   bool menu_texture_enable;
   unsigned rotation;
   bool keep_aspect;
   bool should_resize;
   bool lcd_buttom_on;

   void* empty_framebuffer;

   aptHookCookie lcd_aptHook;
} ctr_video_t;

static INLINE void ctr_set_scale_vector(ctr_scale_vector_t* vec,
      int viewport_width, int viewport_height,
      int texture_width, int texture_height)
{
   vec->x = -2.0 / viewport_width;
   vec->y = -2.0 / viewport_height;
   vec->u =  1.0 / texture_width;
   vec->v = -1.0 / texture_height;
}

static INLINE void ctr_set_screen_coords(ctr_video_t * ctr)
{
   if (ctr->rotation == 0)
   {
      ctr->frame_coords->x0 = ctr->vp.x;
      ctr->frame_coords->y0 = ctr->vp.y;
      ctr->frame_coords->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y1 = ctr->vp.y + ctr->vp.height;
   }
   else if (ctr->rotation == 1) /* 90° */
   {
      ctr->frame_coords->x1 = ctr->vp.x;
      ctr->frame_coords->y0 = ctr->vp.y;
      ctr->frame_coords->x0 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y1 = ctr->vp.y + ctr->vp.height;
   }
   else if (ctr->rotation == 2) /* 180° */
   {
      ctr->frame_coords->x1 = ctr->vp.x;
      ctr->frame_coords->y1 = ctr->vp.y;
      ctr->frame_coords->x0 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y0 = ctr->vp.y + ctr->vp.height;
   }
   else /* 270° */
   {
      ctr->frame_coords->x0 = ctr->vp.x;
      ctr->frame_coords->y1 = ctr->vp.y;
      ctr->frame_coords->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y0 = ctr->vp.y + ctr->vp.height;
   }
}


static void ctr_update_viewport(ctr_video_t* ctr)
{
   int x                = 0;
   int y                = 0;
   float device_aspect  = ((float)ctr->vp.full_width) / ctr->vp.full_height;
   float width          = ctr->vp.full_width;
   float height         = ctr->vp.full_height;
   settings_t *settings = config_get_ptr();

   float desired_aspect = video_driver_get_aspect_ratio();
   if(ctr->rotation & 0x1)
      desired_aspect = 1.0 / desired_aspect;

   if (settings->video.scale_integer)
   {
      video_viewport_get_scaled_integer(&ctr->vp, ctr->vp.full_width,
            ctr->vp.full_height, desired_aspect, ctr->keep_aspect);
   }
   else if (ctr->keep_aspect)
   {
#if defined(HAVE_MENU)
      if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         struct video_viewport *custom = video_viewport_get_custom();

         if (custom)
         {
            x      = custom->x;
            y      = custom->y;
            width  = custom->width;
            height = custom->height;
         }
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x     = (int)roundf(width * (0.5f - delta));
            width = (unsigned)roundf(2.0f * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y      = (int)roundf(height * (0.5f - delta));
            height = (unsigned)roundf(2.0f * height * delta);
         }
      }

      ctr->vp.x      = x;
      ctr->vp.y      = y;
      ctr->vp.width  = width;
      ctr->vp.height = height;
   }
   else
   {
      ctr->vp.x = ctr->vp.y = 0;
      ctr->vp.width = width;
      ctr->vp.height = height;
   }

   ctr_set_screen_coords(ctr);

   ctr->should_resize = false;

}


static void ctr_lcd_aptHook(APT_HookType hook, void* param)
{
   ctr_video_t *ctr  = (ctr_video_t*)param;
   if(!ctr)
      return;

   if((hook == APTHOOK_ONSUSPEND) || (hook == APTHOOK_ONRESTORE))
   {
      Handle lcd_handle;
      u8 not_2DS;
      CFGU_GetModelNintendo2DS(&not_2DS);
      if(not_2DS && srvGetServiceHandle(&lcd_handle, "gsp::Lcd") >= 0)
      {
         u32 *cmdbuf = getThreadCommandBuffer();
         cmdbuf[0] = ((hook == APTHOOK_ONSUSPEND) || ctr->lcd_buttom_on)? 0x00110040: 0x00120040;
         cmdbuf[1] = 2;
         svcSendSyncRequest(lcd_handle);
         svcCloseHandle(lcd_handle);
      }
   }
}
static void* ctr_init(const video_info_t* video,
      const input_driver_t** input, void** input_data)
{
   float refresh_rate;
   void* ctrinput   = NULL;
   ctr_video_t* ctr = (ctr_video_t*)linearAlloc(sizeof(ctr_video_t));

   if (!ctr)
      return NULL;

//   gfxInitDefault();
//   gfxSet3D(false);

   memset(ctr, 0, sizeof(ctr_video_t));


   ctr->vp.x                = 0;
   ctr->vp.y                = 0;
   ctr->vp.width            = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->vp.height           = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->vp.full_width       = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->vp.full_height      = CTR_TOP_FRAMEBUFFER_HEIGHT;


   ctr->display_list_size = 0x400;
   ctr->display_list = linearAlloc(ctr->display_list_size * sizeof(uint32_t));
   GPU_Reset(NULL, ctr->display_list, ctr->display_list_size);

   ctr->rgb32 = video->rgb32;
   ctr->texture_width = video->input_scale * RARCH_SCALE_BASE;
   ctr->texture_height = video->input_scale * RARCH_SCALE_BASE;
   ctr->texture_linear =
         linearMemAlign(ctr->texture_width * ctr->texture_height * (ctr->rgb32? 4:2), 128);
   ctr->texture_swizzled =
         linearMemAlign(ctr->texture_width * ctr->texture_height * (ctr->rgb32? 4:2), 128);

   ctr->frame_coords = linearAlloc(sizeof(ctr_vertex_t));
   ctr->frame_coords->x0 = 0;
   ctr->frame_coords->y0 = 0;
   ctr->frame_coords->x1 = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->frame_coords->y1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->frame_coords->u0 = 0;
   ctr->frame_coords->v0 = 0;
   ctr->frame_coords->u1 = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->frame_coords->v1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   GSPGPU_FlushDataCache(ctr->frame_coords, sizeof(ctr_vertex_t));

   ctr->menu.texture_width = 512;
   ctr->menu.texture_height = 512;
   ctr->menu.texture_linear =
         linearMemAlign(ctr->menu.texture_width * ctr->menu.texture_height * sizeof(uint16_t), 128);
   ctr->menu.texture_swizzled =
         linearMemAlign(ctr->menu.texture_width * ctr->menu.texture_height * sizeof(uint16_t), 128);

   ctr->menu.frame_coords = linearAlloc(sizeof(ctr_vertex_t));

   ctr->menu.frame_coords->x0 = 40;
   ctr->menu.frame_coords->y0 = 0;
   ctr->menu.frame_coords->x1 = CTR_TOP_FRAMEBUFFER_WIDTH - 40;
   ctr->menu.frame_coords->y1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->menu.frame_coords->u0 = 0;
   ctr->menu.frame_coords->v0 = 0;
   ctr->menu.frame_coords->u1 = CTR_TOP_FRAMEBUFFER_WIDTH - 80;
   ctr->menu.frame_coords->v1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   GSPGPU_FlushDataCache(ctr->menu.frame_coords, sizeof(ctr_vertex_t));

   ctr_set_scale_vector(&ctr->scale_vector,
                        CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                        ctr->texture_width, ctr->texture_height);
   ctr_set_scale_vector(&ctr->menu.scale_vector,
                        CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                        ctr->menu.texture_width, ctr->menu.texture_height);

   ctr->dvlb = DVLB_ParseFile((u32*)ctr_sprite_shbin, ctr_sprite_shbin_size);
   ctrGuSetVshGsh(&ctr->shader, ctr->dvlb, 2, 2);
   shaderProgramUse(&ctr->shader);

   GPU_SetViewport(VIRT_TO_PHYS(CTR_GPU_DEPTHBUFFER),
                   VIRT_TO_PHYS(CTR_GPU_FRAMEBUFFER),
                   0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT, CTR_TOP_FRAMEBUFFER_WIDTH);

//      GPU_SetViewport(NULL,
//                      VIRT_TO_PHYS(CTR_GPU_FRAMEBUFFER),
//                      0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT, CTR_TOP_FRAMEBUFFER_WIDTH);

   GPU_DepthMap(-1.0f, 0.0f);
   GPU_SetFaceCulling(GPU_CULL_NONE);
   GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
   GPU_SetStencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
   GPU_SetBlendingColor(0, 0, 0, 0);
//      GPU_SetDepthTestAndWriteMask(true, GPU_GREATER, GPU_WRITE_ALL);
   GPU_SetDepthTestAndWriteMask(false, GPU_ALWAYS, GPU_WRITE_ALL);
   //   GPU_SetDepthTestAndWriteMask(true, GPU_ALWAYS, GPU_WRITE_ALL);

   GPUCMD_AddMaskedWrite(GPUREG_EARLYDEPTH_TEST1, 0x1, 0);
   GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);

   GPU_SetAlphaBlending(GPU_BLEND_ADD, GPU_BLEND_ADD,
                        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
   GPU_SetAlphaTest(false, GPU_ALWAYS, 0x00);

   GPU_SetTextureEnable(GPU_TEXUNIT0);

   GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
   GPU_SetTexEnv(1, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(2, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(3, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(4, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(5, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);

   ctrGuSetAttributeBuffers(2,
                            VIRT_TO_PHYS(ctr->menu.frame_coords),
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0 |
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 4,
                            sizeof(ctr_vertex_t));
   GPUCMD_Finalize();
   ctrGuFlushAndRun(true);
   gspWaitForEvent(GSPGPU_EVENT_P3D, false);

   if (input && input_data)
   {
      ctrinput = input_ctr.init();
      *input = ctrinput ? &input_ctr : NULL;
      *input_data = ctrinput;
   }

   ctr->keep_aspect   = true;
   ctr->should_resize = true;
   ctr->smooth        = video->smooth;
   ctr->vsync         = video->vsync;
   ctr->lcd_buttom_on = true;

   ctr->empty_framebuffer = linearAlloc(320 * 240 * 2);
   memset(ctr->empty_framebuffer, 0, 320 * 240 * 2);

   refresh_rate = (32730.0 * 8192.0) / 4481134.0;

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);
   aptHook(&ctr->lcd_aptHook, ctr_lcd_aptHook, ctr);

   return ctr;
}

static bool ctr_frame(void* data, const void* frame,
      unsigned width, unsigned height, 
      uint64_t frame_count,
      unsigned pitch, const char* msg)
{
   uint32_t diff;
   static uint64_t currentTick,lastTick;
   ctr_video_t       *ctr  = (ctr_video_t*)data;
   settings_t   *settings  = config_get_ptr();
   static float        fps = 0.0;
   static int total_frames = 0;
   static int       frames = 0;
   static struct retro_perf_counter ctrframe_f = {0};
   uint32_t state_tmp;
   touchPosition state_tmp_touch;

   extern bool select_pressed;

   if (!width || !height)
   {
      gspWaitForEvent(GSPGPU_EVENT_VBlank0, true);
      return true;
   }

   if(!aptMainLoop())
   {
      event_cmd_ctl(EVENT_CMD_QUIT, NULL);
      return true;
   }

   if (select_pressed)
   {
      event_cmd_ctl(EVENT_CMD_QUIT, NULL);
      return true;
   }

   state_tmp = hidKeysDown();
   hidTouchRead(&state_tmp_touch);
   if((state_tmp & KEY_TOUCH) && (state_tmp_touch.py < 120))
   {
      Handle lcd_handle;
      u8 not_2DS;
      extern PrintConsole* currentConsole;

      gfxBottomFramebuffers[0] = ctr->lcd_buttom_on ? (u8*)ctr->empty_framebuffer:
                                                      (u8*)currentConsole->frameBuffer;

      CFGU_GetModelNintendo2DS(&not_2DS);
      if(not_2DS && srvGetServiceHandle(&lcd_handle, "gsp::Lcd") >= 0)
      {
         u32 *cmdbuf = getThreadCommandBuffer();
         cmdbuf[0] = ctr->lcd_buttom_on? 0x00120040:  0x00110040;
         cmdbuf[1] = 2;
         svcSendSyncRequest(lcd_handle);
         svcCloseHandle(lcd_handle);
      }

      ctr->lcd_buttom_on = !ctr->lcd_buttom_on;
   }

   svcWaitSynchronization(gspEvents[GSPGPU_EVENT_P3D], 20000000);
   svcClearEvent(gspEvents[GSPGPU_EVENT_P3D]);
   svcWaitSynchronization(gspEvents[GSPGPU_EVENT_PPF], 20000000);
   svcClearEvent(gspEvents[GSPGPU_EVENT_PPF]);

   frames++;

   if (ctr->vsync)
      svcWaitSynchronization(gspEvents[GSPGPU_EVENT_VBlank0], U64_MAX);

   svcClearEvent(gspEvents[GSPGPU_EVENT_VBlank0]);

   currentTick = svcGetSystemTick();
   diff        = currentTick - lastTick;
   if(diff > CTR_CPU_TICKS_PER_SECOND)
   {
      fps = (float)frames * ((float) CTR_CPU_TICKS_PER_SECOND / (float) diff);
      lastTick = currentTick;
      frames = 0;
   }


//#define CTR_INSPECT_MEMORY_USAGE

#ifdef CTR_INSPECT_MEMORY_USAGE
   uint32_t ctr_get_stack_usage(void);
   void ctr_linear_get_stats(void);
   extern u32 __linear_heap_size;
   extern u32 __heap_size;

   MemInfo mem_info;
   PageInfo page_info;
   u32 query_addr = 0x08000000;
   printf(PRINTFPOS(0,0));
   while (query_addr < 0x40000000)
   {
      svcQueryMemory(&mem_info, &page_info, query_addr);
      printf("0x%08X --> 0x%08X (0x%08X) \n", mem_info.base_addr, mem_info.base_addr + mem_info.size, mem_info.size);
      query_addr = mem_info.base_addr + mem_info.size;
      if(query_addr == 0x1F000000)
         query_addr = 0x30000000;
   }
//   static u32* dummy_pointer;
//   if(total_frames == 500)
//      dummy_pointer = malloc(0x2000000);
//   if(total_frames == 1000)
//      free(dummy_pointer);


   printf("========================================");
   printf("0x%08X 0x%08X 0x%08X\n", __heap_size, gpuCmdBufOffset, (__linear_heap_size - linearSpaceFree()));
   printf("fps: %8.4f frames: %i (%X)\n", fps, total_frames++, (__linear_heap_size - linearSpaceFree()));
   printf("========================================");
   u32 app_memory = *((u32*)0x1FF80040);
   u64 mem_used;
   svcGetSystemInfo(&mem_used, 0, 1);
   printf("total mem : 0x%08X          \n", app_memory);
   printf("used: 0x%08X free: 0x%08X      \n", (u32)mem_used, app_memory - (u32)mem_used);
   static u32 stack_usage = 0;
   extern u32 __stack_bottom;
   if(!(total_frames & 0x3F))
      stack_usage = ctr_get_stack_usage();
   printf("stack total:0x%08X used: 0x%08X\n", 0x10000000 - __stack_bottom, stack_usage);

   printf("========================================");
   ctr_linear_get_stats();
   printf("========================================");

#else
   printf(PRINTFPOS(29,0)"fps: %8.4f frames: %i\r", fps, total_frames++);
#endif
   fflush(stdout);

   rarch_perf_init(&ctrframe_f, "ctrframe_f");
   retro_perf_start(&ctrframe_f);

   if (ctr->should_resize)
      ctr_update_viewport(ctr);

   ctrGuSetMemoryFill(true, (u32*)CTR_GPU_FRAMEBUFFER, 0x00000000,
                    (u32*)(CTR_GPU_FRAMEBUFFER + CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT * sizeof(uint32_t)),
                    0x201, (u32*)CTR_GPU_DEPTHBUFFER, 0x00000000,
                    (u32*)(CTR_GPU_DEPTHBUFFER + CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT * sizeof(uint32_t)),
                    0x201);

   GPUCMD_SetBufferOffset(0);

   if (width > ctr->texture_width)
      width = ctr->texture_width;
   if (height > ctr->texture_height)
      height = ctr->texture_height;

   if(frame)
   {
      if(((((u32)(frame)) >= 0x14000000 && ((u32)(frame)) < 0x40000000)) /* frame in linear memory */
         && !((u32)frame & 0x7F)                                         /* 128-byte aligned */
         && !(pitch & 0xF)                                               /* 16-byte aligned */
         && (pitch > 0x40))
      {
         /* can copy the buffer directly with the GPU */
//         GSPGPU_FlushDataCache(frame, pitch * height);
         ctrGuSetCommandList_First(true,(void*)frame, pitch * height,0,0,0,0);
         ctrGuCopyImage(true, frame, pitch / (ctr->rgb32? 4: 2), height, ctr->rgb32 ? CTRGU_RGBA8: CTRGU_RGB565, false,
                        ctr->texture_swizzled, ctr->texture_width, ctr->rgb32 ? CTRGU_RGBA8: CTRGU_RGB565,  true);
      }
      else
      {
         int i;
         uint8_t      *dst = (uint8_t*)ctr->texture_linear;
         const uint8_t *src = frame;

         for (i = 0; i < height; i++)
         {
            memcpy(dst, src, width * (ctr->rgb32? 4: 2));
            dst += ctr->texture_width * (ctr->rgb32? 4: 2);
            src += pitch;
         }
         GSPGPU_FlushDataCache(ctr->texture_linear,
                               ctr->texture_width * ctr->texture_height * (ctr->rgb32? 4: 2));

         ctrGuCopyImage(false, ctr->texture_linear, ctr->texture_width, ctr->texture_height, ctr->rgb32 ? CTRGU_RGBA8: CTRGU_RGB565, false,
                        ctr->texture_swizzled, ctr->texture_width, ctr->rgb32 ? CTRGU_RGBA8: CTRGU_RGB565,  true);

      }

      ctr->frame_coords->u0 = 0;
      ctr->frame_coords->v0 = 0;
      ctr->frame_coords->u1 = width;
      ctr->frame_coords->v1 = height;
      GSPGPU_FlushDataCache(ctr->frame_coords, sizeof(ctr_vertex_t));

      ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->frame_coords));
      ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->scale_vector, 1);
   }

   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(ctr->texture_swizzled), ctr->texture_width, ctr->texture_height,
                  (ctr->smooth? GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)  | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
                              : GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST)) |
                  GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                  ctr->rgb32 ? GPU_RGBA8: GPU_RGB565);

   /* ARGB --> RGBA */
   if (ctr->rgb32)
   {
      GPU_SetTexEnv(0,
                    GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
                    GPU_TEVSOURCES(GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, 0),
                    GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_G, 0, 0),
                    GPU_TEVOPERANDS(0, 0, 0),
                    GPU_MODULATE, GPU_MODULATE,
                    0x0000FF);
      GPU_SetTexEnv(1,
                    GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS),
                    GPU_TEVSOURCES(GPU_PREVIOUS, GPU_PREVIOUS, 0),
                    GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_B, 0, 0),
                    GPU_TEVOPERANDS(0, 0, 0),
                    GPU_MULTIPLY_ADD, GPU_MODULATE,
                    0x00FF00);
      GPU_SetTexEnv(2,
                    GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS),
                    GPU_TEVSOURCES(GPU_PREVIOUS, GPU_PREVIOUS, 0),
                    GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_ALPHA, 0, 0),
                    GPU_TEVOPERANDS(0, 0, 0),
                    GPU_MULTIPLY_ADD, GPU_MODULATE,
                    0xFF0000);
   }

   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);

   /* restore */
   if (ctr->rgb32)
   {
      GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
      GPU_SetTexEnv(1, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
      GPU_SetTexEnv(2, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   }

   if (ctr->menu_texture_enable)
   {

      GSPGPU_FlushDataCache(ctr->menu.texture_linear,
                            ctr->menu.texture_width * ctr->menu.texture_height * sizeof(uint16_t));

      ctrGuCopyImage(false, ctr->menu.texture_linear, ctr->menu.texture_width, ctr->menu.texture_height, CTRGU_RGBA4444,false,
                     ctr->menu.texture_swizzled, ctr->menu.texture_width, CTRGU_RGBA4444,  true);

      ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(ctr->menu.texture_swizzled), ctr->menu.texture_width, ctr->menu.texture_height,
                     GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
                     GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                     GPU_RGBA4);


      ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->menu.frame_coords));
      ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->menu.scale_vector, 1);
      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
   }

   GPU_FinishDrawing();
   GPUCMD_Finalize();
   ctrGuFlushAndRun(true);

   ctrGuDisplayTransfer(true, CTR_GPU_FRAMEBUFFER, 240,400, CTRGU_RGBA8,
                        gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 240,400,CTRGU_RGB8, CTRGU_MULTISAMPLE_NONE);

   gfxSwapBuffersGpu();
   retro_perf_stop(&ctrframe_f);

   return true;
}

static void ctr_set_nonblock_state(void* data, bool toggle)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->vsync = !toggle;
}

static bool ctr_alive(void* data)
{
   (void)data;
   return true;
}

static bool ctr_focus(void* data)
{
   (void)data;
   return true;
}

static bool ctr_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool ctr_has_windowed(void* data)
{
   (void)data;
   return false;
}

static void ctr_free(void* data)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   aptUnhook(&ctr->lcd_aptHook);
   shaderProgramFree(&ctr->shader);
   DVLB_Free(ctr->dvlb);
   linearFree(ctr->display_list);
   linearFree(ctr->texture_linear);
   linearFree(ctr->texture_swizzled);
   linearFree(ctr->frame_coords);
   linearFree(ctr->menu.texture_linear);
   linearFree(ctr->menu.texture_swizzled);
   linearFree(ctr->menu.frame_coords);
   linearFree(ctr->empty_framebuffer);
   linearFree(ctr);
   //   gfxExit();
}

static void ctr_set_texture_frame(void* data, const void* frame, bool rgb32,
                                  unsigned width, unsigned height, float alpha)
{
   int i;
   ctr_video_t* ctr = (ctr_video_t*)data;
   uint16_t* dst = (uint16_t*)ctr->menu.texture_linear;
   const uint16_t* src = frame;
   int line_width = width;

   (void)rgb32;
   (void)alpha;

   if (line_width > ctr->menu.texture_width)
      line_width = ctr->menu.texture_width;

   if (height > (unsigned)ctr->menu.texture_height)
      height = (unsigned)ctr->menu.texture_height;

   for (i = 0; i < height; i++)
   {
      memcpy(dst, src, line_width * sizeof(uint16_t));
      dst += ctr->menu.texture_width;
      src += width;
   }

   ctr->menu.frame_coords->x0 = (CTR_TOP_FRAMEBUFFER_WIDTH - width) / 2;
   ctr->menu.frame_coords->y0 = (CTR_TOP_FRAMEBUFFER_HEIGHT - height) / 2;
   ctr->menu.frame_coords->x1 = ctr->menu.frame_coords->x0 + width;
   ctr->menu.frame_coords->y1 = ctr->menu.frame_coords->y0 + height;
   ctr->menu.frame_coords->u0 = 0;
   ctr->menu.frame_coords->v0 = 0;
   ctr->menu.frame_coords->u1 = width;
   ctr->menu.frame_coords->v1 = height;
   GSPGPU_FlushDataCache(ctr->menu.frame_coords, sizeof(ctr_vertex_t));
}

static void ctr_set_texture_enable(void* data, bool state, bool full_screen)
{
   (void) full_screen;

   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->menu_texture_enable = state;
}


static void ctr_set_rotation(void* data, unsigned rotation)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   ctr->rotation = rotation;
   ctr->should_resize = true;
}
static void ctr_set_filtering(void* data, unsigned index, bool smooth)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->smooth = smooth;
}

static void ctr_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   ctr_video_t *ctr = (ctr_video_t*)data;
   enum rarch_display_ctl_state cmd = RARCH_DISPLAY_CTL_NONE;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_SQUARE_PIXEL;
         break;

      case ASPECT_RATIO_CORE:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_CORE;
         break;

      case ASPECT_RATIO_CONFIG:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_CONFIG;
         break;

      default:
         break;
   }

   if (cmd != RARCH_DISPLAY_CTL_NONE)
      video_driver_ctl(cmd, NULL);

   video_driver_set_aspect_ratio_value(aspectratio_lut[aspect_ratio_idx].value);

   ctr->keep_aspect = true;
   ctr->should_resize = true;
}

static void ctr_apply_state_changes(void* data)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->should_resize = true;

}

static void ctr_viewport_info(void* data, struct video_viewport* vp)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      *vp = ctr->vp;
}

static const video_poke_interface_t ctr_poke_interface = {
   NULL,
   NULL,
   NULL,
   ctr_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL,
   ctr_set_aspect_ratio,
   ctr_apply_state_changes,
#ifdef HAVE_MENU
   ctr_set_texture_frame,
   ctr_set_texture_enable,
#endif
   NULL,
   NULL,
   NULL
};

static void ctr_get_poke_interface(void* data,
                                   const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &ctr_poke_interface;
}

static bool ctr_read_viewport(void* data, uint8_t* buffer)
{
   (void)data;
   (void)buffer;
   return false;
}

static bool ctr_set_shader(void* data,
                           enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_ctr =
{
   ctr_init,
   ctr_frame,
   ctr_set_nonblock_state,
   ctr_alive,
   ctr_focus,
   ctr_suppress_screensaver,
   ctr_has_windowed,
   ctr_set_shader,
   ctr_free,
   "ctr",
   NULL, /* set_viewport */
   ctr_set_rotation,
   ctr_viewport_info,
   ctr_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
   ctr_get_poke_interface
};
