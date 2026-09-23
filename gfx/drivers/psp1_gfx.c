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

#include <retro_inline.h>
#include <retro_math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <defines/psp_defines.h>

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
#define SCEGU_VRAM_SIZE       (2 * 1024 * 1024)
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
   /* The GE writes this, so it owns its cache lines rather than
    * sharing them with fields the CPU touches every frame. */
   PspGeContext* context_storage;

   bool active;
} psp1_menu_frame_t;

typedef struct psp1_video
{
   psp1_menu_frame_t menu;
   video_viewport_t vp;
   void* main_dList;
   void* frame_dList;
   void* blit_dList;
   void* draw_buffer;
   void* texture;
   psp1_sprite_t *frame_coords;
   int tex_filter;
   int bpp_log2;
   unsigned rotation;
   /* VRAM left for the core frame, after the display buffers and the
    * colour tables. */
   unsigned texture_size;
   /* Source geometry the texture coordinates were built for,
    * (width << 16) | height. */
   unsigned tex_geom;
   /* Capacity of blit_dList, in command words. */
   unsigned blit_dList_words;
   bool vsync;
   bool rgb32;
   /* Cleared from interrupt context by psp_on_vblank(). */
   volatile bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;
   bool hw_render;
} psp1_video_t;

/* Both row and column count need to be a power of 2. The split decides
 * the shape of each slice, which is what the GE's texture cache sees;
 * a build can pick another one to compare against. */
#ifndef PSP_FRAME_ROWS_COUNT
#define PSP_FRAME_ROWS_COUNT     4
#endif
#ifndef PSP_FRAME_COLUMNS_COUNT
#define PSP_FRAME_COLUMNS_COUNT  16
#endif
#define PSP_FRAME_SLICE_COUNT    (PSP_FRAME_ROWS_COUNT * PSP_FRAME_COLUMNS_COUNT)
#define PSP_FRAME_VERTEX_COUNT   (PSP_FRAME_SLICE_COUNT * 2)

static INLINE void psp_set_screen_coords (psp1_sprite_t* framecoords,
      int x, int y, int width, int height, unsigned rotation)
{
   int i;
   float x0, y0, step_x, step_y;
   int current_column = 0;

   if (rotation == 0)
   {
      x0     = x;
      y0     = y;
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
      x0     = x + width;
      y0     = y;
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
      x0     = x + width;
      y0     = y + height;
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
      x0     = x;
      y0     = y + height;
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

static INLINE void psp_set_tex_coords (psp1_sprite_t* framecoords,
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

static void psp_update_viewport(psp1_video_t* psp)
{
   psp->vp.full_dims   = VIDEO_SCALE_PACK(SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);
   video_driver_update_viewport(&psp->vp, false, psp->keep_aspect, true);

   /* Ensure even dimensions */
   {
      unsigned vp_w = VIDEO_SCALE_W(psp->vp.dims);
      unsigned vp_h = VIDEO_SCALE_H(psp->vp.dims);
      psp->vp.dims  = VIDEO_SCALE_PACK(vp_w + (vp_w & 0x1),
            vp_h + (vp_h & 0x1));

      psp_set_screen_coords(psp->frame_coords, VIDEO_POS_X(psp->vp.pos),
            VIDEO_POS_Y(psp->vp.pos), VIDEO_SCALE_W(psp->vp.dims),
            VIDEO_SCALE_H(psp->vp.dims), psp->rotation);
   }

   psp->should_resize = false;
}


static void psp_on_vblank(int sub, void *arg)
{
   psp1_video_t *psp = (psp1_video_t*)arg;

   (void)sub;

   if (psp)
      psp->vblank_not_reached = false;
}

static void psp_free(void *data);

/* The GE's block transfer takes a source stride that is a multiple of 8
 * and no wider than 1024. A core outside that is copied a row at a
 * time, where the stride is not read. */
static bool psp_build_row_blit(psp1_video_t *psp, const void *frame,
      unsigned width, unsigned height, unsigned pitch, unsigned dest_stride,
      void *dest, int psm)
{
   unsigned y;
   unsigned words = height * 8 + 16;

   if (psp->blit_dList_words < words)
   {
      void *list;

      sceGuSync(0, 0);
      if (psp->blit_dList)
         free(psp->blit_dList);
      psp->blit_dList       = NULL;
      psp->blit_dList_words = 0;

      if (!(list = memalign(64, ((words * 4) + 63) & ~63)))
         return false;

      psp->blit_dList       = list;
      psp->blit_dList_words = words;
   }

   sceGuStart(GU_CALL, psp->blit_dList);

   for (y = 0; y < height; y++)
   {
      const u8 *row = (const u8*)frame + y * pitch;

      sceGuCopyImage(psm, ((u32)row & 0xF) >> psp->bpp_log2, 0,
            width, 1, dest_stride, (void*)((u32)row & ~0xF),
            0, y, dest_stride, dest);
   }

   sceGuFinish();

   return true;
}

static void *psp_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   /* TODO : add ASSERT() checks or use main RAM if
    * VRAM is too low for desired video->input_scale. */

   int pixel_format, lut_pixel_format, lut_block_count;
   unsigned int red_shift, color_mask;
   void *pspinput           = NULL;
   void *displayBuffer      = NULL;
   void *LUT_r              = NULL;
   void *LUT_b              = NULL;
   psp1_video_t *psp        = (psp1_video_t*)calloc(1, sizeof(psp1_video_t));

   if (!psp)
      return NULL;

   sceGuInit();

   psp->vp.pos              = VIDEO_POS_PACK(0, 0);
   psp->vp.dims             = VIDEO_SCALE_PACK(SCEGU_SCR_WIDTH,
         SCEGU_SCR_HEIGHT);
   psp->vp.full_dims        = VIDEO_SCALE_PACK(SCEGU_SCR_WIDTH,
         SCEGU_SCR_HEIGHT);

   /* Make sure anything using uncached pointers reserves
    * whole cachelines (memory address and size need to be a multiple of 64)
    * so it isn't overwritten by an unlucky cache writeback.
    *
    * This includes display lists since the Gu library uses
    * uncached pointers to write to them. */

   /* Allocate more space if bigger display lists are needed. */
   psp->main_dList          = memalign(64, 256);

   psp->frame_dList         = memalign(64, 256);
   psp->menu.dList          = memalign(64, 256);
   psp->menu.frame          = memalign(64,
         2 * SCEGU_SCR_WIDTH * SCEGU_SCR_HEIGHT);
   psp->menu.context_storage = (PspGeContext*)memalign(64,
         sizeof(PspGeContext));
   psp->frame_coords        = memalign(64,
         (((PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t)) + 63) & ~63));
   psp->menu.frame_coords   = memalign(64,
         (((PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t)) + 63) & ~63));

   if (     !psp->main_dList
         || !psp->frame_dList
         || !psp->menu.dList
         || !psp->menu.frame
         || !psp->menu.context_storage
         || !psp->frame_coords
         || !psp->menu.frame_coords)
   {
      psp_free(psp);
      return NULL;
   }

   memset(psp->frame_coords, 0,
         PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t));
   memset(psp->menu.frame_coords, 0,
         PSP_FRAME_SLICE_COUNT * sizeof(psp1_sprite_t));

   sceKernelDcacheWritebackInvalidateAll();
   psp->frame_coords        = TO_UNCACHED_PTR(psp->frame_coords);
   psp->menu.frame_coords   = TO_UNCACHED_PTR(psp->menu.frame_coords);

   psp->frame_coords->v0.x  = 60;
   psp->frame_coords->v0.y  = 0;
   psp->frame_coords->v0.u  = 0;
   psp->frame_coords->v0.v  = 0;

   psp->frame_coords->v1.x  = 420;
   psp->frame_coords->v1.y  = SCEGU_SCR_HEIGHT;
   psp->frame_coords->v1.u  = 256;
   psp->frame_coords->v1.v  = 240;

   psp->vsync               = video->vsync;
   psp->rgb32               = video->rgb32;

   if (psp->rgb32)
   {
      u32 i;
      uint32_t* LUT_r_local = (uint32_t*)(SCEGU_VRAM_BP32_2);
      uint32_t* LUT_b_local = (uint32_t*)(SCEGU_VRAM_BP32_2) + (1 << 8);

      red_shift             = 8 + 8;
      color_mask            = 0xFF;
      lut_block_count       = (1 << 8) / 8;

      psp->texture          = (void*)(LUT_b_local + (1 << 8));
      psp->draw_buffer      = SCEGU_VRAM_BP32_0;
      psp->bpp_log2         = 2;

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
      video_driver_state_t *video_st = video_state_get_ptr();
      uint16_t* LUT_r_local = (uint16_t*)(SCEGU_VRAM_BP_2);
      uint16_t* LUT_b_local = (uint16_t*)(SCEGU_VRAM_BP_2) + (1 << 5);

      red_shift             = 6 + 5;
      color_mask            = 0x1F;
      lut_block_count       = (1 << 5) / 8;

      psp->texture          = (void*)(LUT_b_local + (1 << 5));
      psp->draw_buffer      = SCEGU_VRAM_BP_0;
      psp->bpp_log2         = 1;

      pixel_format          =
         (video_st->pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555)
         ? GU_PSM_5551 : GU_PSM_5650;

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

   psp->texture_size = (SCEGU_VRAM_TOP + SCEGU_VRAM_SIZE)
      - (uint32_t)psp->texture;

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
      settings_t *settings = config_get_ptr();
      pspinput             = input_driver_init_wrap(&input_psp,
            settings->arrays.input_joypad_driver);
      *input               = pspinput ? &input_psp : NULL;
      *input_data          = pspinput;
   }

   psp->vblank_not_reached = true;
   sceKernelRegisterSubIntrHandler(PSP_VBLANK_INT, 0,
         (void*)psp_on_vblank, psp);
   sceKernelEnableSubIntr(PSP_VBLANK_INT, 0);

   psp->keep_aspect        = true;
   psp->should_resize      = true;
   psp->hw_render          = false;

   return psp;
}

static bool psp_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned width = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   unsigned dest_stride    = 0;
   bool     rows_at_a_time  = false;
   psp1_video_t *psp  = (psp1_video_t*)data;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (!width || !height)
      return false;

   if (((uint32_t)frame&0x04000000) || (frame == RETRO_HW_FRAME_BUFFER_VALID))
      psp->hw_render = true;
   else if (frame)
      psp->hw_render = false;

   if (!psp->hw_render)
   {
      unsigned max_height;

      /* No side of a GE transfer reaches past 1023. */
      if (width > 1023)
         width = 1023;
      if (height > 1023)
         height = 1023;

      /* The destination stride is ours, and has to be a multiple of 8. */
      dest_stride = (width + 7) & ~7u;

      /* The blit below runs to the end of VRAM if the core frame is
       * larger than what is left for it. */
      max_height  = (psp->texture_size >> psp->bpp_log2) / dest_stride;
      if (height > max_height)
         height = max_height;
      if (!height)
         return false;

      if (frame && ((pitch >> psp->bpp_log2) > 1024
               || ((pitch >> psp->bpp_log2) & 0x7)))
      {
         if (!psp_build_row_blit(psp, frame, width, height, pitch,
                  dest_stride, psp->texture,
                  psp->rgb32? GU_PSM_8888 : GU_PSM_5650))
            return false;
         rows_at_a_time = true;
      }

      sceGuSync(0, 0); /* let the core decide when to sync when HW_RENDER */
   }

   if (msg)
   {
      /* The CPU writes the framebuffer here, so the GE has to be off
       * it even where the core drives its own rendering. */
      if (psp->hw_render)
         sceGuSync(0, 0);

      pspDebugScreenSetBase(psp->draw_buffer);
      pspDebugScreenSetXY(0,0);
      pspDebugScreenPuts(msg);
   }

   if ((psp->vsync) && (psp->vblank_not_reached))
      sceDisplayWaitVblankStart();

   psp->vblank_not_reached = true;

   psp->draw_buffer = FROM_GU_POINTER(sceGuSwapBuffers());

   if (psp->should_resize)
      psp_update_viewport(psp);

   /* frame_coords is uncached, so every store here goes to memory. */
   if (psp->tex_geom != ((width << 16) | height))
   {
      psp_set_tex_coords(psp->frame_coords, width, height);
      psp->tex_geom = (width << 16) | height;
   }

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

         if (rows_at_a_time)
            sceGuCallList(psp->blit_dList);
         else
            sceGuCopyImage(psp->rgb32? GU_PSM_8888 : GU_PSM_5650,
                  ((u32)frame & 0xF) >> psp->bpp_log2,
                  0, width, height, pitch >> psp->bpp_log2,
                  (void*)((u32)frame & ~0xF), 0, 0, dest_stride,
                  psp->texture);
      }
      sceGuTexImage(0, next_pow2(width), next_pow2(height), dest_stride,
            psp->texture);
      sceGuCallList(psp->frame_dList);
   }

   sceGuFinish();

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (psp->menu.active)
   {
      sceGuSendList(GU_TAIL, psp->menu.dList, psp->menu.context_storage);
      sceGuSync(0, 0);
   }

   return true;
}

static void psp_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   if (psp)
      psp->vsync = !toggle;
}

static bool psp_alive(void *data) { return true; }
static bool psp_focus(void *data) { return true; }
static bool psp_suppress_screensaver(void *data, bool enable) { return false; }

static void psp_free(void *data)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   if (!psp)
      return;

   /* psp_on_vblank() writes through its own copy of @psp, so it has to
    * be gone before anything it touches is. */
   sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
   sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);

   sceDisplayWaitVblankStart();
   sceGuDisplay(GU_FALSE);
   sceGuTerm();

   if (psp->main_dList)
      free(psp->main_dList);
   if (psp->frame_dList)
      free(psp->frame_dList);
   if (psp->blit_dList)
      free(psp->blit_dList);
   if (psp->frame_coords)
      free(TO_CACHED_PTR(psp->frame_coords));
   if (psp->menu.frame_coords)
      free(TO_CACHED_PTR(psp->menu.frame_coords));
   if (psp->menu.dList)
      free(psp->menu.dList);
   if (psp->menu.frame)
      free(psp->menu.frame);
   if (psp->menu.context_storage)
      free(psp->menu.context_storage);

   free(psp);
}

static void psp_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned dims, float alpha)
{
   unsigned max_height, dest_stride, src_stride;
   bool     rows_at_a_time = false;
   psp1_video_t *psp = (psp1_video_t*)data;

   if (!psp || !frame || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return;

   /* No side of a GE transfer reaches past 1023. */
   if (VIDEO_SCALE_W(dims) > 1023)
      VIDEO_SCALE_PUT_W(dims, 1023);
   if (VIDEO_SCALE_H(dims) > 1023)
      VIDEO_SCALE_PUT_H(dims, 1023);

   dest_stride = (VIDEO_SCALE_W(dims) + 7) & ~7u;
   src_stride  = VIDEO_SCALE_W(dims);

   /* psp->menu.frame holds one screen of 4444. */
   max_height  = (SCEGU_SCR_WIDTH * SCEGU_SCR_HEIGHT) / dest_stride;
   if (VIDEO_SCALE_H(dims) > max_height)
      VIDEO_SCALE_PUT_H(dims, max_height);
   if (!VIDEO_SCALE_H(dims))
      return;

   psp_set_screen_coords(psp->menu.frame_coords, 0, 0,
         SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT, 0);
   psp_set_tex_coords(psp->menu.frame_coords, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));

   sceKernelDcacheWritebackRange(frame, src_stride * VIDEO_SCALE_H(dims) * 2);

   /* The menu pushes a texture between frames, so the GE may still be
    * on the list psp_frame() left it. */
   sceGuSync(0, 0);

   if (src_stride > 1024 || (src_stride & 0x7))
   {
      if (!psp_build_row_blit(psp, frame, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), src_stride * 2,
               dest_stride, psp->menu.frame, GU_PSM_4444))
         return;
      rows_at_a_time = true;
   }

   sceGuStart(GU_DIRECT, psp->main_dList);
   if (rows_at_a_time)
      sceGuCallList(psp->blit_dList);
   else
      sceGuCopyImage(GU_PSM_4444, 0, 0, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), src_stride,
            (void*)frame, 0, 0, dest_stride, psp->menu.frame);
   sceGuFinish();

   sceGuStart(GU_SEND, psp->menu.dList);
   sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
   sceGuTexFilter(GU_LINEAR, GU_LINEAR);
   sceGuTexImage(0, next_pow2(VIDEO_SCALE_W(dims)), next_pow2(VIDEO_SCALE_H(dims)), dest_stride,
         psp->menu.frame);
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
   psp1_video_t *psp = (psp1_video_t*)data;

   (void)full_screen;

   if (psp)
      psp->menu.active = state;
}

static void psp_set_rotation(void *data, unsigned rotation)
{
   psp1_video_t *psp  = (psp1_video_t*)data;

   if (!psp)
      return;

   psp->rotation      = rotation;
   psp->should_resize = true;
}
static void psp_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   if (psp)
      psp->tex_filter = smooth? GU_LINEAR : GU_NEAREST;
}

static void psp_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   if (!psp)
      return;

   psp->keep_aspect   = true;
   psp->should_resize = true;
}

static void psp_apply_state_changes(void *data)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   if (psp)
      psp->should_resize = true;
}

static void psp_viewport_info(void *data, struct video_viewport *vp)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   if (psp)
      *vp = psp->vp;
}

static uint32_t psp_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

   return flags;
}

static const video_poke_interface_t psp_poke_interface = {
   psp_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   psp_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   psp_set_aspect_ratio,
   psp_apply_state_changes,
   psp_set_texture_frame,
   psp_set_texture_enable,
   NULL, /* set_osd_msg */
   NULL, /* show_mouse  */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void psp_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &psp_poke_interface;
}

static bool psp_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   void* src_buffer;
   int i, j, src_bufferwidth, src_pixelformat, x0, x1, y0, y1, width, height;
   uint8_t      *dst = buffer;
   psp1_video_t *psp = (psp1_video_t*)data;

   if (!psp || !buffer)
      return false;

   sceDisplayGetFrameBuf(&src_buffer, &src_bufferwidth, &src_pixelformat, PSP_DISPLAY_SETBUF_NEXTFRAME);

   if (!src_buffer)
      return false;

   /* The GE wrote this buffer, so read it through the uncached alias. */
   src_buffer = TO_UNCACHED_PTR(src_buffer);

   width     = VIDEO_SCALE_W(psp->vp.dims);
   height    = VIDEO_SCALE_H(psp->vp.dims);

   /* The caller sizes its buffer from the viewport and encodes all of
    * it, so anything the framebuffer does not cover reads as black. */
   memset(buffer, 0, (size_t)width * height * 3);

   x0        = (VIDEO_POS_X(psp->vp.pos) > 0)? VIDEO_POS_X(psp->vp.pos) : 0;
   y0        = (VIDEO_POS_Y(psp->vp.pos) > 0)? VIDEO_POS_Y(psp->vp.pos) : 0;
   x1        = ((VIDEO_POS_X(psp->vp.pos) + width)  < src_bufferwidth)? (VIDEO_POS_X(psp->vp.pos) + width): src_bufferwidth;
   y1        = ((VIDEO_POS_Y(psp->vp.pos) + height) < SCEGU_SCR_HEIGHT)? (VIDEO_POS_Y(psp->vp.pos) + height): SCEGU_SCR_HEIGHT;

/* Red is in the low bits of every display format, which is why the
 * frame is drawn through the CLUT passes psp_init() builds: blue comes
 * out of the high bits here. */

/* Bottom-up, from the start of the row the viewport puts this line on. */
#define PSP_VP_ROW(row) (buffer + ((size_t)(VIDEO_POS_Y(psp->vp.pos) + height - 1 - (row)) \
      * width + (size_t)(x0 - VIDEO_POS_X(psp->vp.pos))) * 3)

   switch(src_pixelformat)
   {
   case PSP_DISPLAY_PIXEL_FORMAT_565:
      for (j = y0; j < y1; j++)
      {
         uint16_t* src = (uint16_t*)src_buffer + src_bufferwidth * j + x0;
         dst           = PSP_VP_ROW(j);
         for (i = x0; i < x1; i++)
         {
            uint16_t s = *(src++);

            *(dst++) = (s >> 11) << 3;
            *(dst++) = ((s >> 5) << 2) &0xFF;
            *(dst++) = (s & 0x1F) << 3;
         }
      }
      return true;

   case PSP_DISPLAY_PIXEL_FORMAT_5551:
      for (j = y0; j < y1; j++)
      {
         uint16_t* src = (uint16_t*)src_buffer + src_bufferwidth * j + x0;
         dst           = PSP_VP_ROW(j);
         for (i = x0; i < x1; i++)
         {
            uint16_t s = *(src++);

            *(dst++) = ((s >> 10) << 3) &0xFF;
            *(dst++) = ((s >> 5) << 3) &0xFF;
            *(dst++) = (s & 0x1F) << 3;
         }
      }
      return true;

   case PSP_DISPLAY_PIXEL_FORMAT_4444:
      for (j = y0; j < y1; j++)
      {
         uint16_t* src = (uint16_t*)src_buffer + src_bufferwidth * j + x0;
         dst           = PSP_VP_ROW(j);
         for (i = x0; i < x1; i++)
         {
            uint16_t s = *(src++);

            *(dst++) = (s >> 4) & 0xF0;
            *(dst++) = s        & 0xF0;
            *(dst++) = (s << 4) & 0xF0;
         }
      }
      return true;

   case PSP_DISPLAY_PIXEL_FORMAT_8888:
      for (j = y0; j < y1; j++)
      {
         uint32_t* src = (uint32_t*)src_buffer + src_bufferwidth * j + x0;
         dst           = PSP_VP_ROW(j);
         for (i = x0; i < x1; i++)
         {
            uint32_t s = *(src++);

            *(dst++) = (s >> 16) & 0xFF;
            *(dst++) = (s >> 8 ) & 0xFF;
            *(dst++) = s & 0xFF;
         }
      }
      return true;
   }

#undef PSP_VP_ROW

   return false;
}

static bool psp_set_shader(void *data, enum rarch_shader_type type, const char *path) { return false; }

video_driver_t video_psp1 = {
   psp_init,
   psp_frame,
   psp_set_nonblock_state,
   psp_alive,
   psp_focus,
   psp_suppress_screensaver,
   NULL, /* has_windowed */
   psp_set_shader,
   psp_free,
   "psp1",
   NULL, /* set_viewport */
   psp_set_rotation,
   psp_viewport_info,
   psp_read_viewport,
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   psp_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
