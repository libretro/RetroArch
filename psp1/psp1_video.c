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

#include "../psp/sdk_defines.h"
#include "../general.h"
#include "../driver.h"
#include "../gfx/gfx_common.h"

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
   int16_t u,v;
   uint16_t color;
   int16_t x,y,z;

} psp1_vertex_t;

typedef struct __attribute__((packed)) psp1_sprite
{
   psp1_vertex_t v0;
   psp1_vertex_t v1;

} psp1_sprite_t;

typedef struct psp1_rgui_frame
{
   void* dList;
   void* frame;
   psp1_sprite_t* frame_coords;   

   bool active;

   PspGeContext context_storage;

} psp1_rgui_frame_t;

typedef struct psp1_video
{
   void* main_dList;
   void* frame_dList;
   void* draw_buffer;
   void* texture;
   psp1_sprite_t* frame_coords;

   bool vsync;
   bool rgb32;
   int bpp_log2;

   psp1_rgui_frame_t rgui;

   //not implemented
   unsigned rotation;

} psp1_video_t;


static void *psp_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   // to-do : add ASSERT() checks or use main RAM if VRAM is too low for desired video->input_scale
   void *pspinput;
   psp1_video_t *psp=driver.video_data;

   if (!psp)
   {
      // first time init
      psp = (psp1_video_t*)calloc(1, sizeof(psp1_video_t));

      if (!psp)
         goto error;

   }

   if(!psp->main_dList) // either first time init or psp_free was called
   {
      sceGuInit();


      psp->main_dList         = memalign(16, 256); // make sure to allocate more space if bigger display lists are needed.
      psp->frame_dList        = memalign(16, 256);
      psp->rgui.dList         = memalign(16, 256);
      psp->rgui.frame         = memalign(16,  2 * 480 * 272);
      psp->frame_coords       = memalign(64,  1 * sizeof(psp1_sprite_t));
      psp->rgui.frame_coords  = memalign(64, 16 * sizeof(psp1_sprite_t));

      memset(psp->frame_coords      , 0,  1 * sizeof(psp1_sprite_t));
      memset(psp->rgui.frame_coords , 0, 16 * sizeof(psp1_sprite_t));
      sceKernelDcacheWritebackInvalidateAll();
      psp->frame_coords       = TO_UNCACHED_PTR(psp->frame_coords);
      psp->rgui.frame_coords  = TO_UNCACHED_PTR(psp->rgui.frame_coords);;

      psp->frame_coords->v0.x = 60;
      psp->frame_coords->v0.y = 0;
      psp->frame_coords->v0.u = 0;
      psp->frame_coords->v0.v = 0;

      psp->frame_coords->v1.x = 420;
      psp->frame_coords->v1.y = SCEGU_SCR_HEIGHT;
      psp->frame_coords->v1.u = 256;
      psp->frame_coords->v1.v = 240;

   }

   psp->vsync = video->vsync;
   psp->rgb32 = video->rgb32;

   int pixel_format, lut_pixel_format, lut_block_count;
   unsigned int red_shift, color_mask;
   void *displayBuffer, *LUT_r, *LUT_b;

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

      pixel_format = GU_PSM_5650;
      lut_pixel_format = GU_PSM_T16;

      displayBuffer = SCEGU_VRAM_BP_1;

      for (u16 i = 0; i < (1 << 5); i++){
         LUT_r_local[i]= i;
         LUT_b_local[i]= i << (5 + 6);
      }

      LUT_r = (void*)LUT_r_local;
      LUT_b = (void*)LUT_b_local;

   }


   sceDisplayWaitVblankStart();   // TODO : check if necessary
   sceGuDisplay(GU_FALSE);

   sceGuStart(GU_DIRECT, psp->main_dList);

   sceGuDrawBuffer(pixel_format, TO_GU_POINTER(psp->draw_buffer), SCEGU_VRAM_WIDTH);
   sceGuDispBuffer(SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT, TO_GU_POINTER(displayBuffer), SCEGU_VRAM_WIDTH);
   sceGuClearColor(0);
   sceGuScissor(0, 0, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);
   sceGuEnable(GU_SCISSOR_TEST);
   sceGuTexFilter(GU_LINEAR, GU_LINEAR);  // TODO , move this to display list
   sceGuTexWrap (GU_CLAMP, GU_CLAMP);
   sceGuEnable(GU_TEXTURE_2D);
   sceGuDisable(GU_DEPTH_TEST);
   sceGuCallMode(GU_FALSE);

   sceGuFinish();
   sceGuSync(0, 0);

   sceDisplayWaitVblankStart();   // TODO : check if necessary
   sceGuDisplay(GU_TRUE);

   pspDebugScreenSetColorMode(pixel_format);
   pspDebugScreenSetBase(psp->draw_buffer);



   // fill frame_dList :

   sceGuStart(GU_CALL, psp->frame_dList);

   sceGuTexMode(pixel_format, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
   sceGuEnable(GU_BLEND);

   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0x0000FF00, 0xFFFFFFFF); // green only
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, (void*)(psp->frame_coords));
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xFFFFFFFF, 0xFFFFFFFF); // restore

   sceGuTexMode(lut_pixel_format, 0, 0, GU_FALSE);

   sceGuClutMode(pixel_format, red_shift, color_mask, 0);
   sceGuClutLoad(lut_block_count, LUT_r);

   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, (void*)(psp->frame_coords));

   sceGuClutMode(pixel_format, 0, color_mask, 0);
   sceGuClutLoad(lut_block_count, LUT_b);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, (void*)(psp->frame_coords));

   sceGuFinish();

   if (input && input_data)
   {
      pspinput = input_psp.init();
      *input = pspinput ? &input_psp : NULL;
      *input_data = pspinput;
   }


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

   sceGuSync(0, 0);

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

   if (psp->vsync)
      sceDisplayWaitVblankStart();



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

   psp->frame_coords->v0.x = (SCEGU_SCR_WIDTH - width * SCEGU_SCR_HEIGHT / height) / 2;
//   psp->frame_coords->v0.y = 0;
//   psp->frame_coords->v0.u = 0;
//   psp->frame_coords->v0.v = 0;

   psp->frame_coords->v1.x = (SCEGU_SCR_WIDTH + width * SCEGU_SCR_HEIGHT / height) / 2;
//   psp->frame_coords->v1.y = SCEGU_SCR_HEIGHT;
   psp->frame_coords->v1.u = width;
   psp->frame_coords->v1.v = height;

   sceGuStart(GU_DIRECT, psp->main_dList);

   if ((uint32_t)frame&0x04000000) // frame in VRAM ? texture/palette was set in core so draw directly
   {
      sceGuClear(GU_COLOR_BUFFER_BIT);
      sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, (void*)(psp->frame_coords));
   }
   else
   {
      if (frame!=NULL)
      {
         sceKernelDcacheWritebackRange(frame,pitch * height);
         sceGuCopyImage(GU_PSM_5650, ((u32)frame & 0xF) >> psp->bpp_log2, 0, width, height, pitch >> psp->bpp_log2, (void*)((u32)frame & ~0xF), 0, 0, width, psp->texture);
      }

      sceGuClear(GU_COLOR_BUFFER_BIT);
      sceGuTexImage(0, next_pow2(width), next_pow2(height), width, psp->texture);
      sceGuCallList(psp->frame_dList);
   }

   sceGuFinish();

   if(psp->rgui.active)
   {
      sceGuSendList(GU_TAIL, psp->rgui.dList, &(psp->rgui.context_storage));
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
   if (psp->rgui.frame_coords)
      free(TO_CACHED_PTR(psp->rgui.frame_coords));
   if (psp->rgui.dList)
      free(psp->rgui.dList);
   if (psp->rgui.frame)
      free(psp->rgui.frame);

   memset(psp, 0, sizeof(psp1_video_t));

}

#ifdef HAVE_MENU
static void psp_restart(void) {}
#endif

static void psp_set_rotation(void *data, unsigned rotation)
{
   psp1_video_t *psp = (psp1_video_t*)data;
   psp->rotation = rotation;
}

static void psp_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha)
{
   (void) rgb32;
   (void) alpha;

   int i;
   psp1_video_t *psp = (psp1_video_t*)data;

#ifdef DEBUG
   rarch_assert((width*height) < (480 * 272));  // psp->rgui.frame buffer size is (480 * 272)*2 Bytes
#endif

   // rendering the rgui frame as a single sprite is slow
   // so we render it as 16 vertical stripes instead

   for (i=0;i<16;i++)
   {
      psp->rgui.frame_coords[i].v0.x = (i)   * SCEGU_SCR_WIDTH / 16 ;
      psp->rgui.frame_coords[i].v1.x = (i+1) * SCEGU_SCR_WIDTH / 16 ;

//      psp->rgui.frame_coords[i].v0.y = 0;
      psp->rgui.frame_coords[i].v1.y = SCEGU_SCR_HEIGHT ;


      psp->rgui.frame_coords[i].v0.u = (i)   * width / 16 ;
      psp->rgui.frame_coords[i].v1.u = (i+1) * width / 16 ;

//      psp->rgui.frame_coords[i].v0.v = 0;
      psp->rgui.frame_coords[i].v1.v = height;
   }


   sceKernelDcacheWritebackRange(frame,width * height * 2);

   sceGuStart(GU_DIRECT, psp->main_dList);
   sceGuCopyImage(GU_PSM_4444, 0, 0, width, height, width, (void*)frame, 0, 0, width, psp->rgui.frame);
   sceGuFinish();

   sceGuStart(GU_SEND, psp->rgui.dList);
   sceGuTexSync();
   sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
   sceGuTexImage(0, next_pow2(width), next_pow2(height), width, psp->rgui.frame);
   sceGuEnable(GU_BLEND);

//   sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); // default blending
   sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xF0F0F0F0, 0x0F0F0F0F);
;
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 32, NULL, psp->rgui.frame_coords);
   sceGuFinish();

}

static void psp_set_texture_enable(void *data, bool state, bool full_screen)
{
   (void) full_screen;

   psp1_video_t *psp = (psp1_video_t*)data;
   psp->rgui.active = state;

}

static const video_poke_interface_t psp_poke_interface = {
   NULL, /* set_filtering */
#ifdef HAVE_FBO
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
#endif
   NULL,
   NULL,
#ifdef HAVE_MENU
   psp_set_texture_frame,
   psp_set_texture_enable,
#endif
   NULL,
   NULL,
   NULL
};

static void psp_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &psp_poke_interface;
}

const video_driver_t video_psp1 = {
   psp_init,
   psp_frame,
   psp_set_nonblock_state,
   psp_alive,
   psp_focus,
   NULL,
   psp_free,
   "psp1",

#if defined(HAVE_MENU)
   psp_restart,
#endif

   psp_set_rotation,
   NULL,
   NULL,
#ifdef HAVE_OVERLAY
   NULL,
#endif
   psp_get_poke_interface
};
