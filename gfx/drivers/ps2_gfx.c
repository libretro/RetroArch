/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <pspkernel.h>
#include <pspdisplay.h>
#include <malloc.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>

#include <retro_assert.h>
#include <retro_inline.h>
#include <retro_math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../defines/ps2_defines.h"

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

typedef struct __attribute__((packed)) ps2_vertex
{
   float u,v;
   float x,y,z;

} ps2_vertex_t;

typedef struct __attribute__((packed)) ps2_sprite
{
   ps2_vertex_t v0;
   ps2_vertex_t v1;
} ps2_sprite_t;

typedef struct ps2_menu_frame
{
   void* dList;
   void* frame;
   ps2_sprite_t* frame_coords;

   bool active;

   PspGeContext context_storage;
} ps2_menu_frame_t;

typedef struct ps2_video
{
   void* main_dList;
   void* frame_dList;
   void* draw_buffer;
   void* texture;
   ps2_sprite_t* frame_coords;
   int tex_filter;

   bool vsync;
   bool rgb32;
   int bpp_log2;

   ps2_menu_frame_t menu;

   video_viewport_t vp;

   unsigned rotation;
   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;
   bool hw_render;
} ps2_video_t;

// both row and column count need to be a power of 2
#define PSP_FRAME_ROWS_COUNT     4
#define PSP_FRAME_COLUMNS_COUNT  16
#define PSP_FRAME_SLICE_COUNT    (PSP_FRAME_ROWS_COUNT * PSP_FRAME_COLUMNS_COUNT)
#define PSP_FRAME_VERTEX_COUNT   (PSP_FRAME_SLICE_COUNT * 2)

static INLINE void ps2_set_screen_coords (ps2_sprite_t* framecoords,
      int x, int y, int width, int height, unsigned rotation)
{
   int i;
   float x0, y0, step_x, step_y;
   int current_column = 0;

   if (rotation == 0)
   {
      x0 = x;
      y0 = y;
      step_x = ((float) width)  / PSP_FRAME_COLUMNS_COUNT;
      step_y = ((float) height) / PSP_FRAME_ROWS_COUNT;

      for (i=0; i < PSP_FRAME_SLICE_COUNT; i++)
      {
         framecoords[i].v0.x = x0;
         framecoords[i].v0.y = y0;

         framecoords[i].v1.x = (x0 += step_x);
         framecoords[i].v1.y = y0 + step_y;

         if (++current_column == PSP_FRAME_COLUMNS_COUNT)
         {
            x0 = x;
            y0 += step_y;
            current_column = 0;
         }
      }
   }
   else if (rotation == 1) /* 90° */
   {
      x0 = x + width;
      y0 = y;
      step_x = -((float) width) / PSP_FRAME_ROWS_COUNT;
      step_y = ((float) height)  / PSP_FRAME_COLUMNS_COUNT;

      for (i=0; i < PSP_FRAME_SLICE_COUNT; i++)
      {
         framecoords[i].v0.x = x0;
         framecoords[i].v0.y = y0;

         framecoords[i].v1.x = x0 + step_x;
         framecoords[i].v1.y = (y0 += step_y);

         if (++current_column == PSP_FRAME_COLUMNS_COUNT)
         {
            y0 = y;
            x0 += step_x;
            current_column = 0;
         }
      }
   }
   else if (rotation == 2) /* 180° */
   {
      x0 = x + width;
      y0 = y + height;
      step_x = -((float) width)  / PSP_FRAME_COLUMNS_COUNT;
      step_y = -((float) height) / PSP_FRAME_ROWS_COUNT;

      for (i=0; i < PSP_FRAME_SLICE_COUNT; i++)
      {
         framecoords[i].v0.x = x0;
         framecoords[i].v0.y = y0;

         framecoords[i].v1.x = (x0 += step_x);
         framecoords[i].v1.y = y0 + step_y;

         if (++current_column == PSP_FRAME_COLUMNS_COUNT)
         {
            x0 = x + width;
            y0 += step_y;
            current_column = 0;
         }
      }
   }
   else /* 270° */
   {
      x0 = x;
      y0 = y + height;
      step_x = ((float) width)  / PSP_FRAME_ROWS_COUNT;
      step_y = -((float) height) / PSP_FRAME_COLUMNS_COUNT;

      for (i=0; i < PSP_FRAME_SLICE_COUNT; i++)
      {
         framecoords[i].v0.x = x0;
         framecoords[i].v0.y = y0;
         framecoords[i].v1.x = x0 + step_x;
         framecoords[i].v1.y = (y0 += step_y);

         if (++current_column == PSP_FRAME_COLUMNS_COUNT)
         {
            y0 = y + height;
            x0 += step_x;
            current_column = 0;
         }
      }
   }
}

static INLINE void ps2_set_tex_coords (ps2_sprite_t* framecoords,
      int width, int height)
{
   int i;
   int current_column     = 0;
   float u0               = 0;
   float v0               = 0;
   float step_u           = ((float) width)  / PSP_FRAME_COLUMNS_COUNT;
   float step_v           = ((float) height) / PSP_FRAME_ROWS_COUNT;

   for (i=0; i < PSP_FRAME_SLICE_COUNT; i++)
   {
      framecoords[i].v0.u = u0;
      framecoords[i].v0.v = v0;
      u0                 += step_u;
      framecoords[i].v1.u = u0;
      framecoords[i].v1.v = v0 + step_v;

      if (++current_column == PSP_FRAME_COLUMNS_COUNT)
      {
         u0               = 0;
         v0              += step_v;
         current_column   = 0;
      }
   }
}

static void ps2_update_viewport(ps2_video_t* ps2,
      video_frame_info_t *video_info);

static void ps2_on_vblank(u32 sub, ps2_video_t *ps2)
{
   if (ps2)
      ps2->vblank_not_reached = false;
}

static void *ps2_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   /* TODO : add ASSERT() checks or use main RAM if
    * VRAM is too low for desired video->input_scale. */

   int pixel_format, lut_pixel_format, lut_block_count;
   unsigned int red_shift, color_mask;
   void *ps2input           = NULL;
   void *displayBuffer      = NULL;
   void *LUT_r              = NULL;
   void *LUT_b              = NULL;
   ps2_video_t *ps2        = (ps2_video_t*)calloc(1, sizeof(ps2_video_t));

   if (!ps2)
      return NULL;

   sceGuInit();

   ps2->vp.x                = 0;
   ps2->vp.y                = 0;
   ps2->vp.width            = SCEGU_SCR_WIDTH;
   ps2->vp.height           = SCEGU_SCR_HEIGHT;
   ps2->vp.full_width       = SCEGU_SCR_WIDTH;
   ps2->vp.full_height      = SCEGU_SCR_HEIGHT;

   /* Make sure anything using uncached pointers reserves
    * whole cachelines (memory address and size need to be a multiple of 64)
    * so it isn't overwritten by an unlucky cache writeback.
    *
    * This includes display lists since the Gu library uses
    * uncached pointers to write to them. */

   /* Allocate more space if bigger display lists are needed. */
   ps2->main_dList          = memalign(64, 256);

   ps2->frame_dList         = memalign(64, 256);
   ps2->menu.dList          = memalign(64, 256);
   ps2->menu.frame          = memalign(16,  2 * 480 * 272);
   ps2->frame_coords        = memalign(64,
         (((PSP_FRAME_SLICE_COUNT * sizeof(ps2_sprite_t)) + 63) & ~63));
   ps2->menu.frame_coords   = memalign(64,
         (((PSP_FRAME_SLICE_COUNT * sizeof(ps2_sprite_t)) + 63) & ~63));

   memset(ps2->frame_coords, 0,
         PSP_FRAME_SLICE_COUNT * sizeof(ps2_sprite_t));
   memset(ps2->menu.frame_coords, 0,
         PSP_FRAME_SLICE_COUNT * sizeof(ps2_sprite_t));

   sceKernelDcacheWritebackInvalidateAll();
   ps2->frame_coords        = TO_UNCACHED_PTR(ps2->frame_coords);
   ps2->menu.frame_coords   = TO_UNCACHED_PTR(ps2->menu.frame_coords);

   ps2->frame_coords->v0.x  = 60;
   ps2->frame_coords->v0.y  = 0;
   ps2->frame_coords->v0.u  = 0;
   ps2->frame_coords->v0.v  = 0;

   ps2->frame_coords->v1.x  = 420;
   ps2->frame_coords->v1.y  = SCEGU_SCR_HEIGHT;
   ps2->frame_coords->v1.u  = 256;
   ps2->frame_coords->v1.v  = 240;

   ps2->vsync               = video->vsync;
   ps2->rgb32               = video->rgb32;

   if(ps2->rgb32)
   {
      u32 i;
      uint32_t* LUT_r_local = (uint32_t*)(SCEGU_VRAM_BP32_2);
      uint32_t* LUT_b_local = (uint32_t*)(SCEGU_VRAM_BP32_2) + (1 << 8);

      red_shift             = 8 + 8;
      color_mask            = 0xFF;
      lut_block_count       = (1 << 8) / 8;

      ps2->texture          = (void*)(LUT_b_local + (1 << 8));
      ps2->draw_buffer      = SCEGU_VRAM_BP32_0;
      ps2->bpp_log2         = 2;

      pixel_format          = GU_PSM_8888;
      lut_pixel_format      = GU_PSM_T32;

      displayBuffer         = SCEGU_VRAM_BP32_1;

      for (i = 0; i < (1 << 8); i++)
      {
         LUT_r_local[i]     = i;
         LUT_b_local[i]     = i << (8 + 8);
      }

      LUT_r                 = (void*)LUT_r_local;
      LUT_b                 = (void*)LUT_b_local;

   }
   else
   {
      u16 i;
      uint16_t* LUT_r_local = (uint16_t*)(SCEGU_VRAM_BP_2);
      uint16_t* LUT_b_local = (uint16_t*)(SCEGU_VRAM_BP_2) + (1 << 5);

      red_shift             = 6 + 5;
      color_mask            = 0x1F;
      lut_block_count       = (1 << 5) / 8;

      ps2->texture          = (void*)(LUT_b_local + (1 << 5));
      ps2->draw_buffer      = SCEGU_VRAM_BP_0;
      ps2->bpp_log2         = 1;

      pixel_format          =
         (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_0RGB1555)
         ? GU_PSM_5551 : GU_PSM_5650 ;

      lut_pixel_format      = GU_PSM_T16;

      displayBuffer         = SCEGU_VRAM_BP_1;

      for (i = 0; i < (1 << 5); i++)
      {
         LUT_r_local[i]     = i;
         LUT_b_local[i]     = i << (5 + 6);
      }

      LUT_r                 = (void*)LUT_r_local;
      LUT_b                 = (void*)LUT_b_local;

   }

   ps2->tex_filter = video->smooth? GU_LINEAR : GU_NEAREST;

   /* TODO: check if necessary. */
   sceDisplayWaitVblankStart();

   sceGuDisplay(GU_FALSE);

   sceGuStart(GU_DIRECT, ps2->main_dList);

   sceGuDrawBuffer(pixel_format, TO_GU_POINTER(ps2->draw_buffer),
         SCEGU_VRAM_WIDTH);
   sceGuDispBuffer(SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT,
         TO_GU_POINTER(displayBuffer), SCEGU_VRAM_WIDTH);
   sceGuClearColor(0);
   sceGuScissor(0, 0, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);
   sceGuEnable(GU_SCISSOR_TEST);
   sceGuTexFilter(ps2->tex_filter, ps2->tex_filter);
   sceGuTexWrap (GU_CLAMP, GU_CLAMP);
   sceGuEnable(GU_TEXTURE_2D);
   sceGuDisable(GU_DEPTH_TEST);
   sceGuCallMode(GU_FALSE);

   sceGuFinish();
   sceGuSync(0, 0);

   /* TODO : check if necessary */
   sceDisplayWaitVblankStart();
   sceGuDisplay(GU_TRUE);

   ps2DebugScreenSetColorMode(pixel_format);
   ps2DebugScreenSetBase(ps2->draw_buffer);

   /* fill frame_dList : */
   sceGuStart(GU_CALL, ps2->frame_dList);

   sceGuTexMode(pixel_format, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
   sceGuEnable(GU_BLEND);

   /* green only */
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0x0000FF00, 0xFFFFFFFF);

   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF |
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         (void*)(ps2->frame_coords));

   /* restore */
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xFFFFFFFF, 0xFFFFFFFF);

   sceGuTexMode(lut_pixel_format, 0, 0, GU_FALSE);

   sceGuClutMode(pixel_format, red_shift, color_mask, 0);
   sceGuClutLoad(lut_block_count, LUT_r);

   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF |
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         (void*)(ps2->frame_coords));

   sceGuClutMode(pixel_format, 0, color_mask, 0);
   sceGuClutLoad(lut_block_count, LUT_b);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF |
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         (void*)(ps2->frame_coords));

   sceGuFinish();

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      ps2input             = input_ps2.init(settings->arrays.input_joypad_driver);
      *input               = ps2input ? &input_ps2 : NULL;
      *input_data          = ps2input;
   }

   ps2->vblank_not_reached = true;
   sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0, ps2_on_vblank, ps2);
   sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);

   ps2->keep_aspect        = true;
   ps2->should_resize      = true;
   ps2->hw_render          = false;

   return ps2;
}

#if 0
#define DISPLAY_FPS
#endif

static bool ps2_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
#ifdef DISPLAY_FPS
   uint32_t diff;
   static uint64_t currentTick,lastTick;
   static int frames;
   static float fps                        = 0.0;
#endif
   ps2_video_t *ps2                       = (ps2_video_t*)data;

   if (!width || !height)
      return false;

   if (((uint32_t)frame&0x04000000) || (frame == RETRO_HW_FRAME_BUFFER_VALID))
      ps2->hw_render = true;
   else if (frame)
      ps2->hw_render = false;

   if (!ps2->hw_render)
      sceGuSync(0, 0); /* let the core decide when to sync when HW_RENDER */

   ps2DebugScreenSetBase(ps2->draw_buffer);

   ps2DebugScreenSetXY(0,0);

   if (video_info->fps_show)
   {
      ps2DebugScreenSetXY(68 - strlen(video_info->fps_text) - 1,0);
      ps2DebugScreenPuts(video_info->fps_text);
      ps2DebugScreenSetXY(0,1);
   }

   if (msg)
      ps2DebugScreenPuts(msg);

   if ((ps2->vsync)&&(ps2->vblank_not_reached))
      sceDisplayWaitVblankStart();

   ps2->vblank_not_reached = true;

#ifdef DISPLAY_FPS
   frames++;
   sceRtcGetCurrentTick(&currentTick);
   diff = currentTick - lastTick;
   if(diff > 1000000)
   {
      fps = (float)frames * 1000000.0 / diff;
      lastTick = currentTick;
      frames = 0;
   }

   ps2DebugScreenSetXY(0,0);
   ps2DebugScreenPrintf("%f", fps);
#endif

   ps2->draw_buffer = FROM_GU_POINTER(sceGuSwapBuffers());

   if (ps2->should_resize)
      ps2_update_viewport(ps2, video_info);

   ps2_set_tex_coords(ps2->frame_coords, width, height);

   sceGuStart(GU_DIRECT, ps2->main_dList);

   sceGuTexFilter(ps2->tex_filter, ps2->tex_filter);
   sceGuClear(GU_COLOR_BUFFER_BIT);

   /* frame in VRAM ? texture/palette was
    * set in core so draw directly */
   if (ps2->hw_render)
      sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF |
            GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
            (void*)(ps2->frame_coords));
   else
   {
      if (frame)
      {
         sceKernelDcacheWritebackRange(frame,pitch * height);
         sceGuCopyImage(ps2->rgb32? GU_PSM_8888 : GU_PSM_5650, ((u32)frame & 0xF) >> ps2->bpp_log2,
               0, width, height, pitch >> ps2->bpp_log2,
               (void*)((u32)frame & ~0xF), 0, 0, width, ps2->texture);
      }
      sceGuTexImage(0, next_pow2(width), next_pow2(height), width, ps2->texture);
      sceGuCallList(ps2->frame_dList);
   }

   sceGuFinish();

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   if(ps2->menu.active)
   {
      sceGuSendList(GU_TAIL, ps2->menu.dList, &(ps2->menu.context_storage));
      sceGuSync(0, 0);
   }

   return true;
}

static void ps2_set_nonblock_state(void *data, bool toggle)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      ps2->vsync = !toggle;
}

static bool ps2_alive(void *data)
{
   (void)data;
   return true;
}

static bool ps2_focus(void *data)
{
   (void)data;
   return true;
}

static bool ps2_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void ps2_free(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if(!(ps2) || !(ps2->main_dList))
      return;

   sceDisplayWaitVblankStart();
   sceGuDisplay(GU_FALSE);
   sceGuTerm();

   if (ps2->main_dList)
      free(ps2->main_dList);
   if (ps2->frame_dList)
      free(ps2->frame_dList);
   if (ps2->frame_coords)
      free(TO_CACHED_PTR(ps2->frame_coords));
   if (ps2->menu.frame_coords)
      free(TO_CACHED_PTR(ps2->menu.frame_coords));
   if (ps2->menu.dList)
      free(ps2->menu.dList);
   if (ps2->menu.frame)
      free(ps2->menu.frame);

   free(data);

   sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
   sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT,0);
}

static void ps2_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   (void) rgb32;
   (void) alpha;

#ifdef DEBUG
   /* ps2->menu.frame buffer size is (480 * 272)*2 Bytes */
   retro_assert((width*height) < (480 * 272));
#endif

   ps2_set_screen_coords(ps2->menu.frame_coords, 0, 0,
         SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT, 0);
   ps2_set_tex_coords(ps2->menu.frame_coords, width, height);

   sceKernelDcacheWritebackRange(frame, width * height * 2);

   sceGuStart(GU_DIRECT, ps2->main_dList);
   sceGuCopyImage(GU_PSM_4444, 0, 0, width, height, width,
         (void*)frame, 0, 0, width, ps2->menu.frame);
   sceGuFinish();

   sceGuStart(GU_SEND, ps2->menu.dList);
   sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
   sceGuTexFilter(GU_LINEAR, GU_LINEAR);
   sceGuTexImage(0, next_pow2(width), next_pow2(height), width, ps2->menu.frame);
   sceGuEnable(GU_BLEND);

#if 0
   /* default blending */
   sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
#endif
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xF0F0F0F0, 0x0F0F0F0F);
;
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF |
         GU_TRANSFORM_2D, PSP_FRAME_VERTEX_COUNT, NULL,
         ps2->menu.frame_coords);
   sceGuFinish();

}

static void ps2_set_texture_enable(void *data, bool state, bool full_screen)
{
   (void) full_screen;

   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      ps2->menu.active = state;
}

static void ps2_update_viewport(ps2_video_t* ps2,
      video_frame_info_t *video_info)
{
   int x                = 0;
   int y                = 0;
   float device_aspect  = ((float)SCEGU_SCR_WIDTH) / SCEGU_SCR_HEIGHT;
   float width          = SCEGU_SCR_WIDTH;
   float height         = SCEGU_SCR_HEIGHT;
   settings_t *settings = config_get_ptr();

   if (settings->bools.video_scale_integer)
   {
      video_viewport_get_scaled_integer(&ps2->vp, SCEGU_SCR_WIDTH,
            SCEGU_SCR_HEIGHT, video_driver_get_aspect_ratio(), ps2->keep_aspect);
      width  = ps2->vp.width;
      height = ps2->vp.height;
   }
   else if (ps2->keep_aspect)
   {
#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         x      = video_info->custom_vp_x;
         y      = video_info->custom_vp_y;
         width  = video_info->custom_vp_width;
         height = video_info->custom_vp_height;
      }
      else
#endif
      {
         float delta;
         float desired_aspect = video_driver_get_aspect_ratio();

         if ((fabsf(device_aspect - desired_aspect) < 0.0001f)
               || (fabsf((16.0/9.0) - desired_aspect) < 0.02f))
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

      ps2->vp.x      = x;
      ps2->vp.y      = y;
      ps2->vp.width  = width;
      ps2->vp.height = height;
   }
   else
   {
      ps2->vp.x = ps2->vp.y = 0;
      ps2->vp.width = width;
      ps2->vp.height = height;
   }

   ps2->vp.width += ps2->vp.width&0x1;
   ps2->vp.height += ps2->vp.height&0x1;

   ps2_set_screen_coords(ps2->frame_coords, ps2->vp.x,
         ps2->vp.y, ps2->vp.width, ps2->vp.height, ps2->rotation);

   ps2->should_resize = false;

}

static void ps2_set_rotation(void *data, unsigned rotation)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (!ps2)
      return;

   ps2->rotation = rotation;
   ps2->should_resize = true;
}
static void ps2_set_filtering(void *data, unsigned index, bool smooth)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      ps2->tex_filter = smooth? GU_LINEAR : GU_NEAREST;
}

static void ps2_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(aspectratio_lut[aspect_ratio_idx].value);

   ps2->keep_aspect = true;
   ps2->should_resize = true;
}

static void ps2_apply_state_changes(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      ps2->should_resize = true;
}

static void ps2_viewport_info(void *data, struct video_viewport *vp)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      *vp = ps2->vp;
}

static const video_poke_interface_t ps2_poke_interface = {
   NULL,          /* get_flags  */
   NULL,          /* set_coords */
   NULL,          /* set_mvp */
   NULL,
   NULL,
   NULL,
   NULL, /* get_refresh_rate */
   ps2_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   ps2_set_aspect_ratio,
   ps2_apply_state_changes,
   ps2_set_texture_frame,
   ps2_set_texture_enable,
   NULL,                        /* set_osd_msg */
   NULL,                        /* show_mouse  */
   NULL,                        /* grab_mouse_toggle */
   NULL,                        /* get_current_shader */
   NULL,                        /* get_current_software_framebuffer */
   NULL                         /* get_hw_render_interface */
};

static void ps2_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &ps2_poke_interface;
}

static bool ps2_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   void* src_buffer;
   int i, j, src_bufferwidth, src_pixelformat, src_x, src_y, src_x_max, src_y_max;
   uint8_t* dst = buffer;
   ps2_video_t *ps2 = (ps2_video_t*)data;

   (void)data;
   (void)buffer;

   sceDisplayGetFrameBuf(&src_buffer, &src_bufferwidth, &src_pixelformat, PSP_DISPLAY_SETBUF_NEXTFRAME);

   src_x     = (ps2->vp.x > 0)? ps2->vp.x : 0;
   src_y     = (ps2->vp.y > 0)? ps2->vp.y : 0;
   src_x_max = ((ps2->vp.x + ps2->vp.width) < src_bufferwidth)? (ps2->vp.x + ps2->vp.width): src_bufferwidth;
   src_y_max = ((ps2->vp.y + ps2->vp.height) < SCEGU_SCR_HEIGHT)? (ps2->vp.y + ps2->vp.height): SCEGU_SCR_HEIGHT;

   switch(src_pixelformat)
   {
   case PSP_DISPLAY_PIXEL_FORMAT_565:
      for (j = (src_y_max - 1); j >= src_y ; j--)
      {
         uint16_t* src = (uint16_t*)src_buffer + src_bufferwidth * j + src_x;
         for (i = src_x; i < src_x_max; i++)
         {

            *(dst++) = ((*src) >> 11) << 3;
            *(dst++) = (((*src) >> 5) << 2) &0xFF;
            *(dst++) = ((*src) & 0x1F) << 3;
            src++;
         }
      }
      return true;

   case PSP_DISPLAY_PIXEL_FORMAT_5551:
      for (j = (src_y_max - 1); j >= src_y ; j--)
      {
         uint16_t* src = (uint16_t*)src_buffer + src_bufferwidth * j + src_x;
         for (i = src_x; i < src_x_max; i++)
         {

            *(dst++) = (((*src) >> 10) << 3) &0xFF;
            *(dst++) = (((*src) >> 5) << 3) &0xFF;
            *(dst++) = ((*src) & 0x1F) << 3;
            src++;
         }
      }
      return true;

   case PSP_DISPLAY_PIXEL_FORMAT_4444:
      for (j = (src_y_max - 1); j >= src_y ; j--)
      {
         uint16_t* src = (uint16_t*)src_buffer + src_bufferwidth * j + src_x;
         for (i = src_x; i < src_x_max; i++)
         {

            *(dst++) = ((*src) >> 4) & 0xF0;
            *(dst++) = (*src)        & 0xF0;
            *(dst++) = ((*src) << 4) & 0xF0;
            src++;
         }
      }
      return true;

   case PSP_DISPLAY_PIXEL_FORMAT_8888:
      for (j = (src_y_max - 1); j >= src_y ; j--)
      {
         uint32_t* src = (uint32_t*)src_buffer + src_bufferwidth * j + src_x;
         for (i = src_x; i < src_x_max; i++)
         {

            *(dst++) = ((*src) >> 16) & 0xFF;
            *(dst++) = ((*src) >> 8 ) & 0xFF;
            *(dst++) = (*src) & 0xFF;
            src++;
         }
      }
      return true;
   }


   return false;
}

static bool ps2_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_ps2 = {
   ps2_init,
   ps2_frame,
   ps2_set_nonblock_state,
   ps2_alive,
   ps2_focus,
   ps2_suppress_screensaver,
   NULL, /* has_windowed */
   ps2_set_shader,
   ps2_free,
   "ps2",
   NULL, /* set_viewport */
   ps2_set_rotation,
   ps2_viewport_info,
   ps2_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
   ps2_get_poke_interface
};
