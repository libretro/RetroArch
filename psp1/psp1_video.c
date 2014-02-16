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

/* Frame buffer */
#define SCEGU_VRAM_TOP        0x00000000
/* 16bit mode */
#define SCEGU_VRAM_BUFSIZE    (SCEGU_VRAM_WIDTH*SCEGU_SCR_HEIGHT*2)
#define SCEGU_VRAM_BP_0       (void *)(SCEGU_VRAM_TOP)
#define SCEGU_VRAM_BP_1       (void *)(SCEGU_VRAM_TOP+SCEGU_VRAM_BUFSIZE)
#define SCEGU_VRAM_BP_2       (void *)(SCEGU_VRAM_TOP+(SCEGU_VRAM_BUFSIZE*2))
/* 32bit mode */
#define SCEGU_VRAM_BUFSIZE32  (SCEGU_VRAM_WIDTH*SCEGU_SCR_HEIGHT*4)
#define SCEGU_VRAM_BP32_0     (void *)(SCEGU_VRAM_TOP)
#define SCEGU_VRAM_BP32_1     (void *)(SCEGU_VRAM_TOP+SCEGU_VRAM_BUFSIZE32)
#define SCEGU_VRAM_BP32_2     (void *)(SCEGU_VRAM_TOP+(SCEGU_VRAM_BUFSIZE32*2))

typedef struct psp1_video
{
   bool rgb32;
   unsigned tex_w;
   unsigned tex_h;

   /* RGUI data */
   int rgui_rotation;
   float rgui_alpha;
   bool rgui_active;
   bool rgui_rgb32;
} psp1_video_t;

static struct
{
   const void *frame;
   unsigned width;
   unsigned height;
} rgui_texture;

typedef struct psp1_vertex
{
   u16 u,v;
   u16 color;
   u16 x,y,z;
}psp1_vertex_t;


static unsigned int __attribute__((aligned(16))) list[262144];



static void init_texture(void *data, void *frame, unsigned width, unsigned height, bool rgb32)
{
   psp1_video_t *psp = (psp1_video_t*)data;
   
   sceGuStart(GU_DIRECT, list);

   sceGuDrawBuffer(rgb32 ? GU_PSM_8888 : GU_PSM_5650, (void*)0, SCEGU_VRAM_WIDTH);
   sceGuDispBuffer(width, height, frame, SCEGU_VRAM_WIDTH);
   sceGuClearColor(GU_COLOR(0.0f,0.0f,0.0f,1.0f));
   sceGuScissor(0, 0, width, height);
   sceGuEnable(GU_SCISSOR_TEST);
   sceGuTexMode(rgb32 ? GU_PSM_8888 : GU_PSM_5650, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
   sceGuTexFilter(GU_LINEAR, GU_LINEAR);      
   sceGuTexWrap ( GU_CLAMP,GU_CLAMP );
   sceGuEnable(GU_TEXTURE_2D);
   sceGuDisable(GU_BLEND);
   sceGuDisable (GU_DEPTH_TEST); 
   sceGuFinish();
   sceGuSync(0, 0);
    
   sceDisplayWaitVblankStart();
   
   sceGuDisplay(GU_TRUE);

   rgui_texture.frame = NULL;
}

static void *psp_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   void *pspinput;
   (void)video;


   if (driver.video_data)
   {
      psp1_video_t *psp = (psp1_video_t*)driver.video_data;

      /* Reinitialize textures here */
      psp->rgb32 = video->rgb32;
      init_texture(psp, SCEGU_VRAM_BP_2, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT, psp->rgb32);

      return driver.video_data;
   }

   psp1_video_t *psp = (psp1_video_t*)calloc(1, sizeof(psp1_video_t));

   if (!psp)
      goto error;

   sceGuInit();

   if (input && input_data)
   {
      pspinput = input_psp.init();
      *input = pspinput ? &input_psp : NULL;
      *input_data = pspinput;
   }

   psp->rgb32 = video->rgb32;
   init_texture(psp, SCEGU_VRAM_BP_2, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT, psp->rgb32);

   psp->tex_w = 512;
   psp->tex_h = 512;

   return psp;
error:
   RARCH_ERR("PSP1 video could not be initialized.\n");
   return (void*)-1;
}

#define RGB565_GREEN_MASK 0x7E0
#define RGB565_BLUE_MASK  0x1F

static bool psp_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   void *g_texture;
   psp1_vertex_t *v;
	int x,y;
   (void)msg;
	
   psp1_video_t *psp = (psp1_video_t*)data;
  
   /* Check if neither RGUI nor emulator framebuffer is to be displayed. */
   if(psp->rgui_active)
   {
      sceKernelDcacheWritebackInvalidateAll();
      void* frameBuffer;
      int bufferWidth,pixelFormat;
      sceDisplayGetFrameBuf(&frameBuffer, &bufferWidth, &pixelFormat, 0);
      sceGuStart(GU_DIRECT, list);
      sceGuClear(GU_COLOR_BUFFER_BIT);
      sceGuCopyImage(GU_PSM_5650,0,0,rgui_texture.width, rgui_texture.width,rgui_texture.width, rgui_texture.frame,
            (SCEGU_SCR_WIDTH-rgui_texture.width)/2,(SCEGU_SCR_HEIGHT-rgui_texture.height)/2,SCEGU_VRAM_WIDTH,frameBuffer);
      sceGuFinish();
      sceDisplayWaitVblankStart();
      return true;
   }
   
   if (frame == NULL)
      return true;
   
   g_texture = (void*)0x44110000; // video memory after draw+display buffers
   if (psp->rgb32)
   {
      pitch /= 4;
      u32 *out_p = (u32*)g_texture; 
      u32 *in_p  = (u32*)frame; 
      for(y = 0; y < height; y++)
      {
         for (x = 0; x < width; x++)
         {
            *out_p++ =((*in_p)&0xFF00FF00)|(((*in_p)&0xFF)<<16)|(((*in_p)&0xFF0000)>>16);
            in_p++;
         }
         in_p += pitch-width;
      }
   }
   else
   {
      pitch /= 2;
      u16 *out_p = (u16*)g_texture;
      u16 *in_p  = (u16*)frame; 
      for (y = 0; y < height; y++)
      {
         for (x = 0;x < width; x++)
         {
            *out_p++ =((*in_p) & RGB565_GREEN_MASK)|(((*in_p) & RGB565_BLUE_MASK) << 11) | ((*in_p)>>11);
            in_p++;
         }
         in_p += pitch-width;
     }
   }
   
   sceGuStart(GU_DIRECT, list);
   sceGuClear(GU_COLOR_BUFFER_BIT);
   v = (psp1_vertex_t*)sceGuGetMemory(2*sizeof(psp1_vertex_t));
   
   v[0].x = (SCEGU_SCR_WIDTH - width * SCEGU_SCR_HEIGHT / height) / 2;
   v[0].y = 0;   
   v[0].u = 0;
   v[0].v = 0;   
   
   v[1].x = (SCEGU_SCR_WIDTH + width * SCEGU_SCR_HEIGHT / height) / 2;
   v[1].y = SCEGU_SCR_HEIGHT;
   v[1].u = width;
   v[1].v = height;
   
   sceGuTexImage(0, 256, 256, width, g_texture);
   
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_5650 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, v);
   sceGuFinish(); 
   sceGuSync(0, 0);
   sceDisplayWaitVblankStart();
   sceGuSwapBuffers();
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

#ifdef HAVE_MENU
static void psp_restart(void) {}
#endif

static void psp_set_rotation(void *data, unsigned rotation)
{
   /* stub */
}

static void psp_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha)
{
   psp1_video_t *psp = (psp1_video_t*)data;

   psp->rgui_rgb32 = rgb32;
   psp->rgui_alpha = alpha;

   rgui_texture.width  = width;
   rgui_texture.height = height;
   rgui_texture.frame  = frame;
}

static void psp_set_texture_enable(void *data, bool state, bool full_screen)
{
   psp1_video_t *psp = (psp1_video_t*)data;
   psp->rgui_active = state;
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
