/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <pspkernel.h>
#include <pspdisplay.h>
#include <malloc.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>

#include "sdk_defines.h"
#include "../../general.h"
#include "../../driver.h"
#include "../gfx_common.h"

#ifndef SCEGU_SCR_WIDTH
#define SCEGU_SCR_WIDTH 480
#endif

#ifndef SCEGU_SCR_HEIGHT
#define SCEGU_SCR_HEIGHT 272
#endif

#ifndef SCEGU_VRAM_WIDTH
#define SCEGU_VRAM_WIDTH 512
#endif

/* Frame buffer */
#define SCEGU_VRAM_TOP        (0x44000000)
/* 16bit mode */
#define SCEGU_VRAM_BUFSIZE    (SCEGU_VRAM_WIDTH*SCEGU_SCR_HEIGHT*2)
#define SCEGU_VRAM_BP_0       ((void *)(SCEGU_VRAM_TOP))
#define SCEGU_VRAM_BP_1       ((void *)(SCEGU_VRAM_TOP+SCEGU_VRAM_BUFSIZE))
#define SCEGU_VRAM_BP_2       ((void *)(SCEGU_VRAM_TOP+(SCEGU_VRAM_BUFSIZE*2)))
/* 32bit mode */
#define SCEGU_VRAM_BUFSIZE32  (SCEGU_VRAM_WIDTH*SCEGU_SCR_HEIGHT*4)
#define SCEGU_VRAM_BP32_0     ((void *)(SCEGU_VRAM_TOP))
#define SCEGU_VRAM_BP32_1     ((void *)(SCEGU_VRAM_TOP+SCEGU_VRAM_BUFSIZE32))
#define SCEGU_VRAM_BP32_2     ((void *)(SCEGU_VRAM_TOP+(SCEGU_VRAM_BUFSIZE32*2)))

#define TO_UNCACHED_PTR(ptr)  ((void *)((uint32_t)(ptr)|0x40000000))
#define TO_CACHED_PTR(ptr)    ((void *)((uint32_t)(ptr)&~0x40000000))

#define FROM_GU_POINTER(ptr)  ((void *)((uint32_t)(ptr)|0x44000000))
#define TO_GU_POINTER(ptr)    ((void *)((uint32_t)(ptr)&~0x44000000))



typedef struct __attribute__((packed)) psp1_vertex
{
   float u,v;
   float x,y,z;

} psp1_vertex_t;

typedef struct __attribute__((packed)) psp1_sprite
{
   psp1_vertex_t v0;
   psp1_vertex_t v1;

} psp1_sprite_t;

typedef struct psp1_menu_frame
{
   void* dList;
   void* frame;
   psp1_sprite_t* frame_coords;

   bool active;

   PspGeContext context_storage;

} psp1_menu_frame_t;

typedef struct psp1_video
{
   void* main_dList;
   void* frame_dList;
   void* draw_buffer;
   void* texture;
   psp1_sprite_t* frame_coords;
   int tex_filter;

   bool vsync;
   bool rgb32;
   int bpp_log2;


   psp1_menu_frame_t menu;

   rarch_viewport_t vp;

   unsigned rotation;
   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;
   bool hw_render;

} psp1_video_t;

// both row and column count need to be a power of 2
#define PSP_FRAME_ROWS_COUNT     4
#define PSP_FRAME_COLUMNS_COUNT  16
#define PSP_FRAME_SLICE_COUNT    (PSP_FRAME_ROWS_COUNT * PSP_FRAME_COLUMNS_COUNT)
#define PSP_FRAME_VERTEX_COUNT   (PSP_FRAME_SLICE_COUNT * 2)

static inline void psp_set_screen_coords (psp1_sprite_t* framecoords, 
      int x, int y, int width, int height)
{
   int i;
   int current_column = 0;

   float x0 = x;
   float y0 = y;
   float step_x = ((float) width)  / PSP_FRAME_COLUMNS_COUNT;
   float step_y = ((float) height) / PSP_FRAME_ROWS_COUNT;

   for (i=0; i < PSP_FRAME_SLICE_COUNT; i++)
   {
      framecoords[i].v0.x = x0;
      framecoords[i].v0.y = y0;

      x0 += step_x;

      framecoords[i].v1.x = x0;
      framecoords[i].v1.y = y0 + step_y;

      if (++current_column == PSP_FRAME_COLUMNS_COUNT)
      {
         x0 = x;
         y0 += step_y;
         current_column = 0;
      }
   }
}

static inline void psp_set_tex_coords (psp1_sprite_t* framecoords,
      int width, int height)
{
   int i;
   int current_column = 0;

   float u0 = 0;
   float v0 = 0;
   float step_u = ((float) width)  / PSP_FRAME_COLUMNS_COUNT;
   float step_v = ((float) height) / PSP_FRAME_ROWS_COUNT;

   for (i=0; i < PSP_FRAME_SLICE_COUNT; i++)
   {
      framecoords[i].v0.u = u0;
      framecoords[i].v0.v = v0;
      u0+=step_u;
      framecoords[i].v1.u = u0;
      framecoords[i].v1.v = v0 + step_v;

      if (++current_column == PSP_FRAME_COLUMNS_COUNT)
      {
         u0 = 0;
         v0 += step_v;
         current_column = 0;
      }
   }
}
static void psp_update_frame_coords(psp1_video_t* psp);
static void psp_update_viewport(psp1_video_t* psp);

static void psp_on_vblank(u32 sub, psp1_video_t *psp)
{
   psp->vblank_not_reached = false;
}

static void *psp_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   /* to-do : add ASSERT() checks or use main RAM if 
    * VRAM is too low for desired video->input_scale. */
   void *pspinput;
   int pixel_format, lut_pixel_format, lut_block_count;
   unsigned int red_shift, color_mask;
   void *displayBuffer, *LUT_r, *LUT_b;
   psp1_video_t *psp  = (psp1_video_t*)calloc(1, sizeof(psp1_video_t));

   if (!psp)
      return NULL;

   sceGuInit();

   psp->vp.x           = 0;
   psp->vp.y           = 0;
   psp->vp.width       = SCEGU_SCR_WIDTH;
   psp->vp.height      = SCEGU_SCR_HEIGHT;
   psp->vp.full_width  = SCEGU_SCR_WIDTH;
   psp->vp.full_height = SCEGU_SCR_HEIGHT;

   /* Make sure anything using uncached pointers reserves 
    * whole cachelines (memory address and size need to be a multiple of 64)
    * so it isn't overwritten by an unlucky cache writeback.
    *
    * This includes display lists since the Gu library uses 
    * uncached pointers to write to them. */

   /* Allocate more space if bigger display lists are needed. */
   psp->main_dList         = memalign(64, 256);

   psp->frame_dList        = memalign(64, 256);
   psp->menu.dList         = memalign(64, 256);
   psp->menu.frame         = memalign(16,  2 * 480 * 272);
   psp->frame_coords       = memalign(64,
         (((PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t)) + 63) & ~63));
   psp->menu.frame_coords  = memalign(64,
         (((PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t)) + 63) & ~63));

   memset(psp->frame_coords, 0,
         PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t));
   memset(psp->menu.frame_coords, 0,
         PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t));

   sceKernelDcacheWritebackInvalidateAll();
   psp->frame_coords       = TO_UNCACHED_PTR(psp->frame_coords);
   psp->menu.frame_coords  = TO_UNCACHED_PTR(psp->menu.frame_coords);

   psp->frame_coords->v0.x = 60;
   psp->frame_coords->v0.y = 0;
   psp->frame_coords->v0.u = 0;
   psp->frame_coords->v0.v = 0;

   psp->frame_coords->v1.x = 420;
   psp->frame_coords->v1.y = SCEGU_SCR_HEIGHT;
   psp->frame_coords->v1.u = 256;
   psp->frame_coords->v1.v = 240;

   psp->vsync = video->vsync;
   psp->rgb32 = video->rgb32;

   if(psp->rgb32)
   {
      uint32_t* LUT_r_local = (uint32_t*)(SCEGU_VRAM_BP32_2);
      uint32_t* LUT_b_local = (uint32_t*)(SCEGU_VRAM_BP32_2) + (1 << 8);

      red_shift = 8 + 8;
      color_mask = 0xFF;
      lut_block_count = (1 << 8) / 8;

      psp->texture = (void*)(LUT_b_local + (1 << 8));
      psp->draw_buffer = SCEGU_VRAM_BP32_0;
      psp->bpp_log2 = 2;

      pixel_format = GU_PSM_8888;
      lut_pixel_format = GU_PSM_T32;

      displayBuffer = SCEGU_VRAM_BP32_1;

      for (u32 i=0; i < (1 << 8); i++){
         LUT_r_local[i]= i;
         LUT_b_local[i]= i << (8 + 8);
      }

      LUT_r = (void*)LUT_r_local;
      LUT_b = (void*)LUT_b_local;

   }
   else
   {
      uint16_t* LUT_r_local = (uint16_t*)(SCEGU_VRAM_BP_2);
      uint16_t* LUT_b_local = (uint16_t*)(SCEGU_VRAM_BP_2) + (1 << 5);

      red_shift = 6 + 5;
      color_mask = 0x1F;
      lut_block_count = (1 << 5) / 8;

      psp->texture = (void*)(LUT_b_local + (1 << 5));
      psp->draw_buffer = SCEGU_VRAM_BP_0;
      psp->bpp_log2 = 1;

      pixel_format = 
         (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555) 
         ? GU_PSM_5551 : GU_PSM_5650 ;

      lut_pixel_format = GU_PSM_T16;

      displayBuffer = SCEGU_VRAM_BP_1;

      for (u16 i = 0; i < (1 << 5); i++)
      {
         LUT_r_local[i]= i;
         LUT_b_local[i]= i << (5 + 6);
      }

      LUT_r = (void*)LUT_r_local;
      LUT_b = (void*)LUT_b_local;

   }

   psp->tex_filter = video->smooth? GU_LINEAR : GU_NEAREST;

   /* TODO: check if necessary. */
   sceDisplayWaitVblankStart();

   sceGuDisplay(GU_FALSE);

   sceGuStart(GU_DIRECT, psp->main_dList);

   sceGuDrawBuffer(pixel_format, TO_GU_POINTER(psp->draw_buffer),
         SCEGU_VRAM_WIDTH);
   sceGuDispBuffer(SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT,
         TO_GU_POINTER(displayBuffer), SCEGU_VRAM_WIDTH);
   sceGuClearColor(0);
   sceGuScissor(0, 0, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);
   sceGuEnable(GU_SCISSOR_TEST);
   sceGuTexFilter(psp->tex_filter, psp->tex_filter);
   sceGuTexWrap (GU_CLAMP, GU_CLAMP);
   sceGuEnable(GU_TEXTURE_2D);
   sceGuDisable(GU_DEPTH_TEST);
   sceGuCallMode(GU_FALSE);

   sceGuFinish();
   sceGuSync(0, 0);
 
   /* TODO : check if necessary */
   sceDisplayWaitVblankStart();
   sceGuDisplay(GU_TRUE);

   pspDebugScreenSetColorMode(pixel_format);
   pspDebugScreenSetBase(psp->draw_buffer);

   /* fill frame_dList : */
   sceGuStart(GU_CALL, psp->frame_dList);

   sceGuTexMode(pixel_format, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
   sceGuEnable(GU_BLEND);

   /* green only */
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0x0000FF00, 0xFFFFFFFF);

   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | 
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         (void*)(psp->frame_coords));
   
   /* restore */
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xFFFFFFFF, 0xFFFFFFFF);

   sceGuTexMode(lut_pixel_format, 0, 0, GU_FALSE);

   sceGuClutMode(pixel_format, red_shift, color_mask, 0);
   sceGuClutLoad(lut_block_count, LUT_r);

   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF |
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         (void*)(psp->frame_coords));

   sceGuClutMode(pixel_format, 0, color_mask, 0);
   sceGuClutLoad(lut_block_count, LUT_b);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | 
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         (void*)(psp->frame_coords));

   sceGuFinish();

   if (input && input_data)
   {
      pspinput = input_psp.init();
      *input = pspinput ? &input_psp : NULL;
      *input_data = pspinput;
   }

   psp->vblank_not_reached = true;
   sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, psp_on_vblank, psp);
   sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);

   psp->keep_aspect = true;
   psp->should_resize = true;
   psp->hw_render = false;

   return psp;
error:
   RARCH_ERR("PSP1 video could not be initialized.\n");
   return (void*)-1;
}

#define DISPLAY_FPS

static bool psp_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   static char fps_txt[128], fps_text_buf[128];
   psp1_video_t *psp = (psp1_video_t*)data;

#ifdef DISPLAY_FPS
   static uint64_t currentTick,lastTick;
   static float fps=0.0;
   static int frames;
#endif

   if (!width || !height)
      return false;

   if (((uint32_t)frame&0x04000000) || (frame == RETRO_HW_FRAME_BUFFER_VALID))
      psp->hw_render = true;
   else if (frame)
      psp->hw_render = false;

   if (!psp->hw_render)
      sceGuSync(0, 0); /* let the core decide when to sync when HW_RENDER */

   pspDebugScreenSetBase(psp->draw_buffer);

   pspDebugScreenSetXY(0,0);

   if(g_settings.fps_show)
   {
      gfx_get_fps(fps_txt, sizeof(fps_txt), fps_text_buf, sizeof(fps_text_buf));
      pspDebugScreenSetXY(68 - strlen(fps_text_buf) - 1,0);
      pspDebugScreenPuts(fps_text_buf);
      pspDebugScreenSetXY(0,1);
   }
   else
      gfx_get_fps(fps_txt, sizeof(fps_txt), NULL, 0);

   if (msg)
      pspDebugScreenPuts(msg);

   if ((psp->vsync)&&(psp->vblank_not_reached))
      sceDisplayWaitVblankStart();

   psp->vblank_not_reached = true;

#ifdef DISPLAY_FPS
   frames++;
   sceRtcGetCurrentTick(&currentTick);
   uint32_t diff = currentTick - lastTick;
   if(diff > 1000000)
   {
      fps = (float)frames * 1000000.0 / diff;
      lastTick = currentTick;
      frames = 0;
   }

   pspDebugScreenSetXY(0,0);
   pspDebugScreenPrintf("%f", fps);
#endif

   psp->draw_buffer = FROM_GU_POINTER(sceGuSwapBuffers());
   g_extern.frame_count++;


   RARCH_PERFORMANCE_INIT(psp_frame_run);
   RARCH_PERFORMANCE_START(psp_frame_run);

   if (psp->should_resize)
      psp_update_viewport(psp);

   psp_set_tex_coords(psp->frame_coords, width, height);

   sceGuStart(GU_DIRECT, psp->main_dList);

   sceGuTexFilter(psp->tex_filter, psp->tex_filter);
   sceGuClear(GU_COLOR_BUFFER_BIT);

   /* frame in VRAM ? texture/palette was 
    * set in core so draw directly */
   if (psp->hw_render) 
      sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | 
            GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
            (void*)(psp->frame_coords));
   else
   {
      if (frame)
      {
         sceKernelDcacheWritebackRange(frame,pitch * height);
         sceGuCopyImage(GU_PSM_5650, ((u32)frame & 0xF) >> psp->bpp_log2,
               0, width, height, pitch >> psp->bpp_log2,
               (void*)((u32)frame & ~0xF), 0, 0, width, psp->texture);
      }
      sceGuTexImage(0, next_pow2(width), next_pow2(height), width, psp->texture);
      sceGuCallList(psp->frame_dList);
   }

   sceGuFinish();

   RARCH_PERFORMANCE_STOP(psp_frame_run);

   if(psp->menu.active)
   {
      sceGuSendList(GU_TAIL, psp->menu.dList, &(psp->menu.context_storage));
      sceGuSync(0, 0);
   }

   return true;
}

static void psp_set_nonblock_state(void *data, bool toggle)
{
   psp1_video_t *psp = (psp1_video_t*)data;
   psp->vsync = !toggle;
}

static bool psp_alive(void *data)
{
   (void)data;
   return true;
}

static bool psp_focus(void *data)
{
   (void)data;
   return true;
}

static void psp_free(void *data)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   if(!(psp) || !(psp->main_dList))
      return;

   sceDisplayWaitVblankStart();
   sceGuDisplay(GU_FALSE);
   sceGuTerm();

   if (psp->main_dList)
      free(psp->main_dList);
   if (psp->frame_dList)
      free(psp->frame_dList);
   if (psp->frame_coords)
      free(TO_CACHED_PTR(psp->frame_coords));
   if (psp->menu.frame_coords)
      free(TO_CACHED_PTR(psp->menu.frame_coords));
   if (psp->menu.dList)
      free(psp->menu.dList);
   if (psp->menu.frame)
      free(psp->menu.frame);

   free(data);

   sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
   sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT,0);
}

static void psp_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha)
{
   (void) rgb32;
   (void) alpha;

   psp1_video_t *psp = (psp1_video_t*)data;

#ifdef DEBUG
   /* psp->menu.frame buffer size is (480 * 272)*2 Bytes */
   rarch_assert((width*height) < (480 * 272));
#endif

   psp_set_screen_coords(psp->menu.frame_coords, 0, 0,
         SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);
   psp_set_tex_coords(psp->menu.frame_coords, width, height);

   sceKernelDcacheWritebackRange(frame, width * height * 2);

   sceGuStart(GU_DIRECT, psp->main_dList);
   sceGuCopyImage(GU_PSM_4444, 0, 0, width, height, width,
         (void*)frame, 0, 0, width, psp->menu.frame);
   sceGuFinish();

   sceGuStart(GU_SEND, psp->menu.dList);
   sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
   sceGuTexFilter(GU_LINEAR, GU_LINEAR);
   sceGuTexImage(0, next_pow2(width), next_pow2(height), width, psp->menu.frame);
   sceGuEnable(GU_BLEND);

#if 0
   /* default blending */
   sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
#endif
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xF0F0F0F0, 0x0F0F0F0F);
;
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | 
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         psp->menu.frame_coords);
   sceGuFinish();

}

static void psp_set_texture_enable(void *data, bool state, bool full_screen)
{
   (void) full_screen;

   psp1_video_t *psp = (psp1_video_t*)data;
   psp->menu.active = state;

}

static inline void psp_update_frame_coords(psp1_video_t* psp)
{
   /* TODO: no rotation for now */
#if 0
   psp1_vertex_t *v0, *v1;

   if (psp->rotation & 0x2)
   {
      v0 = &psp->frame_coords->v1;
      v1 = &psp->frame_coords->v0;
   }
   else
   {
      v0 = &psp->frame_coords->v0;
      v1 = &psp->frame_coords->v1;
   }

   if (psp->rotation & 0x1)
   {
      v0->x = psp->vp.x + psp->vp.width;
      v1->x = psp->vp.x;
   }
   else
   {
      v0->x = psp->vp.x;
      v1->x = psp->vp.x + psp->vp.width;
   }

   v0->y = psp->vp.y;
   v1->y = psp->vp.y + psp->vp.height;
#endif

   psp_set_screen_coords(psp->frame_coords, psp->vp.x,
         psp->vp.y, psp->vp.width, psp->vp.height);
}

static void psp_update_viewport(psp1_video_t* psp)
{
   int x, y;
   float device_aspect = SCEGU_SCR_WIDTH / SCEGU_SCR_HEIGHT;
   float width = SCEGU_SCR_WIDTH;
   float height = SCEGU_SCR_HEIGHT;
   x = 0;
   y = 0;

   if (g_settings.video.scale_integer)
   {
      gfx_scale_integer(&psp->vp, SCEGU_SCR_WIDTH,
            SCEGU_SCR_HEIGHT, g_extern.system.aspect_ratio, psp->keep_aspect);
      width  = psp->vp.width;
      height = psp->vp.height;
   }
   else if (psp->keep_aspect)
   {
      float desired_aspect = g_extern.system.aspect_ratio;
      float delta;

#if defined(HAVE_MENU)
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct rarch_viewport *custom =
            &g_extern.console.screen.viewports.custom_vp;

         x      = custom->x;
         y      = custom->y;
         width  = custom->width;
         height = custom->height;
      }
      else
#endif
      {
         if ((fabsf(device_aspect - desired_aspect) < 0.0001f)
               || (fabsf((16.0/9.0) - desired_aspect) < 0.0001f))
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

      psp->vp.x      = x;
      psp->vp.y      = y;
      psp->vp.width  = width;
      psp->vp.height = height;
   }
   else
   {
      psp->vp.x = psp->vp.y = 0;
      psp->vp.width = width;
      psp->vp.height = height;
   }

   psp_update_frame_coords(psp);

   psp->should_resize = false;

}

static void psp_set_rotation(void *data, unsigned rotation)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   psp->rotation = rotation;
   psp->should_resize = true;
}
static void psp_set_filtering(void *data, unsigned index, bool smooth)
{
   psp1_video_t *psp = (psp1_video_t*)data;
   psp->tex_filter = smooth? GU_LINEAR : GU_NEAREST;
}

static void psp_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   switch (aspectratio_index)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(
               g_extern.system.av_info.geometry.base_width, 
               g_extern.system.av_info.geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         gfx_set_core_viewport();
         break;

      case ASPECT_RATIO_CONFIG:
         gfx_set_config_viewport();
         break;

      default:
         break;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[aspectratio_index].value;

   psp->keep_aspect = true;
   psp->should_resize = true;
}

static void psp_apply_state_changes(void *data)
{
   psp1_video_t *psp = (psp1_video_t*)data;
   psp->should_resize = true;
}

static void psp_viewport_info(void *data, struct rarch_viewport *vp)
{
   psp1_video_t *psp = (psp1_video_t*)data;
   *vp = psp->vp;
}

static bool psp_read_viewport(void *data, uint8_t *buffer)
{
   return false;
}

static const video_poke_interface_t psp_poke_interface = {
   psp_set_filtering,
   psp_set_aspect_ratio,
   psp_apply_state_changes,
#ifdef HAVE_MENU
   psp_set_texture_frame,
   psp_set_texture_enable,
#endif
   NULL,
   NULL,
   NULL
};

static void psp_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &psp_poke_interface;
}

static bool psp_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return false;
}

static bool psp_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

const video_driver_t video_psp1 = {
   psp_init,
   psp_frame,
   psp_set_nonblock_state,
   psp_alive,
   psp_focus,
   psp_set_shader,
   psp_free,
   "psp1",

   psp_set_rotation,
   psp_viewport_info,
   psp_read_viewport,
#ifdef HAVE_OVERLAY
   NULL,
#endif
   psp_get_poke_interface
};
