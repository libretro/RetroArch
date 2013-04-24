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

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>

#include "../psp/sdk_defines.h"
#include "../general.h"
#include "../driver.h"

#ifndef SCEGU_SCR_WIDTH
#define SCEGU_SCR_WIDTH 480
#endif

#ifndef SCEGU_SCR_HEIGHT
#define SCEGU_SCR_HEIGHT 272
#endif

#ifndef SCEGU_VRAM_WIDTH
#define SCEGU_VRAM_WIDTH 512
#endif

typedef struct psp1_video
{
   bool rgb32;
   unsigned tex_w;
   unsigned tex_h;
} psp1_video_t;

static unsigned int __attribute__((aligned(16))) list[262144];


static void init_texture(void *data, const video_info_t *video)
{
   psp1_video_t *vid = (psp1_video_t*)data;

   sceGuInit();
   sceGuStart(GU_DIRECT, list);

   sceGuDrawBuffer(vid->rgb32 ? GU_PSM_8888 : GU_PSM_5650, (void*)0, SCEGU_VRAM_WIDTH);
   sceGuDispBuffer(SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT, (void*)0x88000, SCEGU_VRAM_WIDTH);
   sceGuClear(GU_COLOR_BUFFER_BIT);

   sceGuOffset(2048 - (SCEGU_SCR_WIDTH / 2), 2048 - (SCEGU_SCR_HEIGHT / 2));
   sceGuViewport(2048, 2048, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);

   /* FIXME - we will want to disable all this */
   sceGuScissor(0, 0, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);
   sceGuEnable(GU_SCISSOR_TEST);
   sceGuTexMode(vid->rgb32 ? GU_PSM_8888 : GU_PSM_5650, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
   sceGuTexFilter(GU_LINEAR, GU_LINEAR);
   sceGuEnable(GU_TEXTURE_2D);

   sceGuFrontFace(GU_CW);
   sceGuDisable(GU_BLEND);

   sceGuFinish();
   sceGuSync(0, 0);

   sceDisplayWaitVblankStart();
   sceGuDisplay(GU_TRUE);

   vid->rgb32 = video->rgb32;
}

static void *psp_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
   (void)video;

   if (driver.video_data)
   {
      psp1_video_t *vid = (psp1_video_t*)driver.video_data;

      /* Reinitialize textures here */
      //init_texture(vid, video);

      return driver.video_data;
   }

   psp1_video_t *vid = (psp1_video_t*)calloc(1, sizeof(psp1_video_t));

   if (!vid)
      goto error;

   init_texture(vid, video);

   vid->tex_w = 512;
   vid->tex_h = 512;

   return vid;
error:
   RARCH_ERR("PSP1 video could not be initialized.\n");
   return (void*)-1;
}

static bool psp_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;

   if(!frame)
      return true;

#if 0
   psp1_video_t *vid = (psp1_video_t*)data;

   sceKernelDcacheWritebackInvalidateAll(); 

   sceGuStart(GU_DIRECT, list);

   sceGumMatrixMode(GU_PROJECTION);
   sceGumLoadIdentity();
   sceGumPerspective(75.0f,16.0f/9.0f,0.5f,1000.0f);

   sceGumMatrixMode(GU_VIEW);
   sceGumLoadIdentity();

   sceGuClearColor(GU_COLOR(0.0f,0.0f,0.0f,1.0f));
   sceGuClearDepth(0);

   sceGuFinish(); 

   sceDisplayWaitVblankStart();

   void *frame_ptr = &frame;
   DisplaySetFrameBuf(frame_ptr, pitch, 
         vid->rgb32 ? PSP_DISPLAY_PIXEL_FORMAT_8888 : PSP_DISPLAY_PIXEL_FORMAT_565,
         PSP_DISPLAY_SETBUF_IMMEDIATE);
#endif

   return true;
}

static void psp_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
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
   (void)data;

   sceGuTerm();
}

#ifdef RARCH_CONSOLE
static void psp_start(void) {}
static void psp_restart(void) {}
#endif

static void psp_set_rotation(void *data, unsigned rotation)
{
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

#ifdef RARCH_CONSOLE
   psp_start,
   psp_restart,
   NULL,
   NULL,
#endif

   psp_set_rotation,
};

