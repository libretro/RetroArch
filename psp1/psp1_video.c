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
#define SCEGU_VRAM_TOP        0x04000000
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

#define TO_UNCACHED_PTR(ptr)  ((void *)((uint32_t)ptr|0x40000000))
#define TO_CACHED_PTR(ptr)    ((void *)((uint32_t)ptr&~0x40000000))

typedef void (*psp1_copy_frame_func) (const void *in_frame, unsigned in_buffer_width,
                                      void *out_frame, unsigned out_buffer_width,
                                      unsigned width, unsigned height);


typedef struct psp1_rgui_frame
{
   bool rgb32;
   int pixel_format;
   int pixel_size;

   unsigned width;
   unsigned height;   
   
   unsigned buffer_width;
   unsigned buffer_Height;
   void* buffer;
   
   float alpha;
   uint32_t alpha_source;
   uint32_t alpha_dest;
   
   bool active;
      
   psp1_copy_frame_func copy_frame;      
   
} psp1_rgui_frame_t;

typedef struct psp1_video
{
   void* displayList;   
   void* frameBuffers[2];
   unsigned int drawBuffer_ID;
   bool vsync;
   
   bool rgb32;
   int pixel_format;
   int pixel_size;
   unsigned buffer_width;
   unsigned buffer_Height;
   void* buffer;
   
   psp1_copy_frame_func copy_frame;   
   
   psp1_rgui_frame_t rgui;
   
   // not implemented
   unsigned width;
   unsigned height;
   bool force_aspect;
   bool smooth;
   unsigned input_scale; // Maximum input size: RARCH_SCALE_BASE * input_scale
   int rotation; 
   
} psp1_video_t;


typedef struct psp1_vertex
{
   int16_t u,v;
   uint16_t color; // not used
   int16_t x,y,z;
} psp1_vertex_t;


static void copy_frame_ARGB8888_to_ABGR8888(const void *in_frame, unsigned in_buffer_width,
                                 void *out_frame, unsigned out_buffer_width,
                                 unsigned width, unsigned height)
{   
   int x,y;
   const uint32_t *in_p  = (const uint32_t*)in_frame; 
   uint32_t *out_p = (uint32_t*)out_frame; 
  
   for(y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         *out_p++ =((*in_p) & 0xFF00FF00) | (((*in_p) & 0xFF) << 16) | (((*in_p) & 0xFF0000) >> 16);
         in_p++;
      }
      in_p += in_buffer_width - width;
      out_p += out_buffer_width - width;
   }
}

#define RGB565_GREEN_MASK 0x7E0
#define RGB565_BLUE_MASK  0x1F
static void copy_frame_RGB5650_to_BGR5650(const void *in_frame, unsigned in_buffer_width,
                                          void *out_frame, unsigned out_buffer_width,
                                          unsigned width, unsigned height)
{   
   int x,y;
   const uint16_t *in_p  = (const uint16_t*)in_frame; 
   uint16_t *out_p = (uint16_t*)out_frame; 
   
   for(y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         *out_p++ =((*in_p) & RGB565_GREEN_MASK) | (((*in_p) & RGB565_BLUE_MASK) << 11) | ((*in_p)>>11);
         in_p++;
      }
      in_p += in_buffer_width - width;
      out_p += out_buffer_width - width;
   }
}

static void copy_frame_XBGR16(const void *in_frame, unsigned in_buffer_width,
                              void *out_frame, unsigned out_buffer_width,
                              unsigned width, unsigned height)
{   
   int x,y;
   const uint16_t *in_p  = (const uint16_t*)in_frame; 
   uint16_t *out_p = (uint16_t*)out_frame; 
   
   for(y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         *out_p++ =*in_p++;
      }
      in_p += in_buffer_width - width;
      out_p += out_buffer_width - width;
   }
}

static void *psp_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   void *pspinput;
   psp1_video_t *psp=driver.video_data;

   if (!psp)
   {
      // first time init
      psp = (psp1_video_t*)calloc(1, sizeof(psp1_video_t));
   
      if (!psp)
         goto error;
      
   }
   
   if(!psp->displayList) // either first time init or psp_free was called
   {
      sceGuInit();

      psp->displayList = memalign(16,1024*1024);   // TODO: use a better approximation of maximum display list size, 1MB is probably too much
      psp->buffer_width = 512;
      psp->buffer_Height = 512;
      psp->rgui.buffer_width = 512;
      psp->rgui.buffer_Height = 256;    
   }
   
   psp->width = video->width;
   psp->height = video->height;
   psp->vsync = video->vsync;
   psp->force_aspect = video->force_aspect;
   psp->smooth = video->smooth;
   psp->input_scale = video->input_scale;
   

   psp->rgb32 = video->rgb32;
   psp->pixel_format = psp->rgb32 ? GU_PSM_8888 : GU_PSM_5650;
   psp->pixel_size = psp->rgb32 ? 4 : 2;   
   psp->copy_frame = psp->rgb32 ? copy_frame_ARGB8888_to_ABGR8888 : copy_frame_RGB5650_to_BGR5650;
   
   if (psp->buffer)
      free(TO_CACHED_PTR(psp->buffer));
   psp->buffer = memalign(16, psp->buffer_width * psp->buffer_Height * psp->pixel_size);   
   psp->buffer = TO_UNCACHED_PTR(psp->buffer);

   //init rgui to 16-bit pixel format since it can't be empty when psp_set_texture_frame is called
   psp->rgui.rgb32 = video->rgb32;
   psp->rgui.pixel_format = psp->rgui.rgb32 ? GU_PSM_8888 : GU_PSM_4444;
   psp->rgui.pixel_size = psp->rgui.rgb32 ? 4 : 2;   
   psp->rgui.copy_frame = psp->rgui.rgb32 ? copy_frame_ARGB8888_to_ABGR8888 : copy_frame_XBGR16;
   psp->frameBuffers[0]=psp->rgui.rgb32 ? SCEGU_VRAM_BP32_0 : SCEGU_VRAM_BP_0;
   psp->frameBuffers[1]=psp->rgui.rgb32 ? SCEGU_VRAM_BP32_1 : SCEGU_VRAM_BP_1;
   psp->drawBuffer_ID=0;
   
   if (psp->rgui.buffer)
      free(TO_CACHED_PTR(psp->rgui.buffer));
   psp->rgui.buffer = memalign(16, psp->rgui.buffer_width * psp->rgui.buffer_Height * psp->rgui.pixel_size);     
   psp->rgui.buffer = TO_UNCACHED_PTR(psp->rgui.buffer);
   
   psp->rgui.active = false;
   psp->rgui.alpha = 0.0;
   psp->rgui.alpha_source = 0x00000000;
   psp->rgui.alpha_dest = 0xFFFFFFFF;
   
   // need to invalidate cache before using uncached pointers, to avoid unwanted cache writebacks. TODO: check up/downsides of using sceKernelDcacheInvalidateRange here instead
   sceKernelDcacheWritebackInvalidateAll();
   
   sceDisplayWaitVblankStart();   // TODO : check if necessary  
   sceGuDisplay(GU_FALSE);

   sceGuStart(GU_DIRECT, psp->displayList);

   sceGuDrawBuffer(psp->pixel_format, (void*)0, SCEGU_VRAM_WIDTH);
   sceGuDispBuffer(SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT, psp->rgb32 ? (void*) SCEGU_VRAM_BUFSIZE32 : (void*) SCEGU_VRAM_BUFSIZE, SCEGU_VRAM_WIDTH);
   sceGuClearColor(GU_COLOR(0.0f, 0.0f, 0.0f, 1.0f));
   sceGuScissor(0, 0, SCEGU_SCR_WIDTH, SCEGU_SCR_HEIGHT);
   sceGuEnable(GU_SCISSOR_TEST);
   sceGuTexMode(psp->pixel_format, 0, 0, GU_FALSE);
   sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
   sceGuTexFilter(GU_LINEAR, GU_LINEAR);  // TODO , move this to display list   
   sceGuTexWrap (GU_CLAMP, GU_CLAMP);
   
   sceGuEnable(GU_TEXTURE_2D);   
   sceGuDisable(GU_BLEND);
   sceGuDisable(GU_DEPTH_TEST); 
   sceGuFinish();
   sceGuSync(0, 0);
    
   sceDisplayWaitVblankStart();   // TODO : check if necessary     
   sceGuDisplay(GU_TRUE);
   
   pspDebugScreenSetColorMode(psp->pixel_format);
   pspDebugScreenSetBase(psp->frameBuffers[psp->drawBuffer_ID]);
   
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



static bool psp_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   psp1_vertex_t *v;
	
   psp1_video_t *psp = (psp1_video_t*)data;

   sceGuSync(0, 0);
   
   if (msg)  // TODO: fix flickering text
   {
      pspDebugScreenSetBase(psp->frameBuffers[psp->drawBuffer_ID]);
      pspDebugScreenSetXY(0,0);
      pspDebugScreenPuts(msg);
   }
   
   if (psp->vsync)
      sceDisplayWaitVblankStart();
   
   sceGuSwapBuffers();
   psp->drawBuffer_ID^=1;
   
   /* frame dupes. */   
   if (frame == NULL)
      return true;
   
   psp->copy_frame(frame, pitch / psp->pixel_size, psp->buffer, psp->buffer_width, width, height);
   
   sceGuStart(GU_DIRECT, psp->displayList);
   sceGuClear(GU_COLOR_BUFFER_BIT);
   
   v = (psp1_vertex_t*)sceGuGetMemory(4 * sizeof(psp1_vertex_t));
   
   v[0].x = (SCEGU_SCR_WIDTH - width * SCEGU_SCR_HEIGHT / height) / 2;
   v[0].y = 0;   
   v[0].u = 0;
   v[0].v = 0;   
   
   v[1].x = (SCEGU_SCR_WIDTH + width * SCEGU_SCR_HEIGHT / height) / 2;
   v[1].y = SCEGU_SCR_HEIGHT;
   v[1].u = width;
   v[1].v = height;
   
   sceGuTexMode(psp->pixel_format, 0, 0, GU_FALSE);
   sceGuTexImage(0, psp->buffer_width, psp->buffer_Height, psp->buffer_width, psp->buffer);   
//      sceGuTexFilter(GU_LINEAR, GU_LINEAR);
   
   sceGuDisable(GU_BLEND);
   sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_5650 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, v);
   
   if (psp->rgui.active)
   {
      v[2].x = 0;
      v[2].y = 0;   
      v[2].u = 0;
      v[2].v = 0;   
      
      v[3].x = SCEGU_SCR_WIDTH;
      v[3].y = SCEGU_SCR_HEIGHT;
      v[3].u = psp->rgui.width; 
      v[3].v = psp->rgui.height;
      
      sceGuTexMode(psp->rgui.pixel_format, 0, 0, GU_FALSE);
      sceGuTexImage(0, psp->rgui.buffer_width, psp->rgui.buffer_Height, psp->rgui.buffer_width, psp->rgui.buffer);   
//      sceGuTexFilter(GU_LINEAR, GU_LINEAR);
      
      sceGuEnable(GU_BLEND);
      sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, psp->rgui.alpha_source, psp->rgui.alpha_dest);
      sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_5650 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, v + 2);
   }
   sceGuFinish(); 
   
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
   
   if (psp->displayList)
      free(psp->displayList);
   if (psp->buffer)
      free(TO_CACHED_PTR(psp->buffer));
   if (psp->rgui.buffer)
      free(TO_CACHED_PTR(psp->rgui.buffer));
   
   memset(psp, 0, sizeof(psp1_video_t));
   
   sceGuTerm();
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
   psp1_video_t *psp = (psp1_video_t*)data;
   
   if (psp->rgui.rgb32 != rgb32)
   {
      psp->rgui.rgb32 = rgb32;
      psp->rgui.pixel_format = psp->rgui.rgb32 ? GU_PSM_8888 : GU_PSM_4444;
      psp->rgui.pixel_size = psp->rgui.rgb32 ? 4 : 2;   
      psp->rgui.copy_frame = psp->rgui.rgb32 ? copy_frame_ARGB8888_to_ABGR8888 : copy_frame_XBGR16;
      
      if (psp->rgui.buffer)
         free(TO_CACHED_PTR(psp->rgui.buffer));
      psp->rgui.buffer = memalign(16, psp->rgui.buffer_width * psp->rgui.buffer_Height * psp->rgui.pixel_size);        
      psp->rgui.buffer = TO_UNCACHED_PTR(psp->rgui.buffer);
      sceKernelDcacheWritebackInvalidateAll();
   }
   
   psp->rgui.width = width;
   psp->rgui.height = height;
   psp->rgui.copy_frame(frame, width, psp->rgui.buffer, psp->rgui.buffer_width, width, height);   
   
   psp->rgui.alpha = alpha; 
   uint32_t mask;
   mask = alpha*255.0;
   mask &= 0xFF;
   psp->rgui.alpha_source = (mask << 24) | (mask << 16) | (mask << 8) | mask;
   mask = 0xFF - mask;
   psp->rgui.alpha_dest = (mask << 24) | (mask << 16) | (mask << 8) | mask;

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
