/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2020 Google
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

#include <retro_inline.h>
#include <retro_math.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../command.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifndef HAVE_THREADS
#include "../../tasks/tasks_internal.h"
#endif

#include <defines/ps3_defines.h>

#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <ppu-types.h>

#define CB_SIZE		0x100000
#define HOST_SIZE	(32*1024*1024)

#include <ppu-lv2.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <time.h>
#include <math.h>

#define MAX_BUFFERS 2

typedef struct
{
   int height;
   int width;
   int id;
   uint32_t *ptr;
   /* Internal stuff */
   uint32_t offset;
} rsxBuffer;

typedef struct
{
   float v;
   float u;
   float y;
   float x;
} gcm_scale_vector_t;

typedef struct
{
   s16 x0, y0, x1, y1;
   s16 u0, v0, u1, v1;
} gcm_vertex_t;

typedef struct gcm_video
{
   video_viewport_t vp;
   rsxBuffer buffers[MAX_BUFFERS];
   rsxBuffer menuBuffers[MAX_BUFFERS];
   int currentBuffer, menuBuffer;
   gcmContextData *context;
   u16 width;
   u16 height;
   bool menu_frame_enable;
   bool rgb32;
   bool vsync;
   u32 depth_pitch;
   u32 depth_offset;
   u32 *depth_buffer;

   bool smooth;
   unsigned rotation;
   bool keep_aspect;
   bool should_resize;
   bool msg_rendering_enabled;
} gcm_video_t;

#ifndef HAVE_THREADS
static bool gcm_tasks_finder(retro_task_t *task,void *userdata)
{
   return task;
}
task_finder_data_t gcm_tasks_finder_data = {gcm_tasks_finder, NULL};
#endif

static int gcm_make_buffer(rsxBuffer * buffer, u16 width, u16 height, int id)
{
   int depth = sizeof(u32);
   int pitch = depth * width;
   int size = depth * width * height;

   buffer->ptr = (uint32_t*)rsxMemalign (64, size);
   if (!buffer->ptr)
      goto error;

   if (rsxAddressToOffset (buffer->ptr, &buffer->offset) != 0)
      goto error;

   /* Register the display buffer with the RSX */
   if (gcmSetDisplayBuffer (id, buffer->offset, pitch, width, height) != 0)
      goto error;

   buffer->width = width;
   buffer->height = height;
   buffer->id = id;

   return TRUE;

error:
   if (buffer->ptr)
      rsxFree (buffer->ptr);

   return FALSE;
}

static int gcm_flip(gcmContextData *context, s32 buffer)
{
   if (gcmSetFlip(context, buffer) == 0)
   {
      rsxFlushBuffer (context);
      /* Prevent the RSX from continuing until the flip has finished. */
      gcmSetWaitFlip (context);

      return TRUE;
   }
   return FALSE;
}

#define GCM_LABEL_INDEX		255

static void gcm_wait_rsx_idle(gcmContextData *context);

static void gcm_wait_flip(void)
{
  while (gcmGetFlipStatus() != 0)
    usleep (200);  /* Sleep, to not stress the cpu. */
  gcmResetFlipStatus();
}

static gcmContextData *gcm_init_screen(gcm_video_t* gcm)
{
   /* Context to keep track of the RSX buffer. */
   gcmContextData              *context = NULL;
   static gcmContextData *saved_context = NULL;
   videoState state;
   videoConfiguration vconfig;
   videoResolution res; /* Screen Resolution */

   if (!saved_context)
   {
      /* Allocate a 1Mb buffer, alligned to a 1Mb boundary                          
       * to be our shared IO memory with the RSX. */
      void *host_addr = memalign (1024*1024, HOST_SIZE);

      if (!host_addr)
         goto error;

      /* Initialise Reality, which sets up the 
       * command buffer and shared I/O memory */
#ifdef NV40TCL_RENDER_ENABLE
      /* There was an api breakage on 2020-07-10, let's
       * workaround this by using one of the new defines */
      rsxInit (&context, CB_SIZE, HOST_SIZE, host_addr);
#else
      context = rsxInit (CB_SIZE, HOST_SIZE, host_addr);
#endif
      if (!context)
         goto error;
      saved_context = context;
   }
   else
      context = saved_context;

   /* Get the state of the display */
   if (videoGetState (0, 0, &state) != 0)
      goto error;

   /* Make sure display is enabled */
   if (state.state != 0)
      goto error;

   /* Get the current resolution */
   if (videoGetResolution (state.displayMode.resolution, &res) != 0)
      goto error;

   /* Configure the buffer format to xRGB */
   memset (&vconfig, 0, sizeof(videoConfiguration));
   vconfig.resolution = state.displayMode.resolution;
   vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
   vconfig.pitch      = res.width * sizeof(u32);
   vconfig.aspect     = state.displayMode.aspect;

   gcm->width         = res.width;
   gcm->height        = res.height;

   gcm_wait_rsx_idle(context);

   if (videoConfigure (0, &vconfig, NULL, 0) != 0)
      goto error;

   if (videoGetState (0, 0, &state) != 0)
      goto error;

   gcmSetFlipMode (GCM_FLIP_VSYNC); /* Wait for VSYNC to flip */

   gcm->depth_pitch = res.width * sizeof(u32);
   gcm->depth_buffer = (u32 *) rsxMemalign (64, (res.height * gcm->depth_pitch)* 2);
   rsxAddressToOffset (gcm->depth_buffer, &gcm->depth_offset);

   gcmResetFlipStatus();

   return context;

error:
#if 0
   if (context)
      rsxFinish (context, 0);

   if (gcm->host_addr)
      free (gcm->host_addr);
#endif

   return NULL;
}


static void waitFinish(gcmContextData *context, u32 sLabelVal)
{
  rsxSetWriteBackendLabel (context, GCM_LABEL_INDEX, sLabelVal);

  rsxFlushBuffer (context);

  while (*(vu32 *) gcmGetLabelAddress (GCM_LABEL_INDEX) != sLabelVal)
    usleep(30);

  sLabelVal++;
}

static void gcm_wait_rsx_idle(gcmContextData *context)
{
  u32 sLabelVal = 1;

  rsxSetWriteBackendLabel (context, GCM_LABEL_INDEX, sLabelVal);
  rsxSetWaitLabel (context, GCM_LABEL_INDEX, sLabelVal);

  sLabelVal++;

  waitFinish(context, sLabelVal);
}

static void* gcm_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   int i;
   gcm_video_t* gcm     = malloc(sizeof(gcm_video_t));

   if (!gcm)
      return NULL;

   memset(gcm, 0, sizeof(gcm_video_t));

   gcm->context = gcm_init_screen(gcm);

   for (i = 0; i < MAX_BUFFERS; i++)
     gcm_make_buffer(&gcm->buffers[i], gcm->width, gcm->height, i);

   for (i = 0; i < MAX_BUFFERS; i++)
     gcm_make_buffer(&gcm->menuBuffers[i], gcm->width, gcm->height, i + MAX_BUFFERS);

   gcm_flip(gcm->context, MAX_BUFFERS - 1);

   gcm->vp.x                = 0;
   gcm->vp.y                = 0;
   gcm->vp.width            = gcm->width;
   gcm->vp.height           = gcm->height;
   gcm->vp.full_width       = gcm->width;
   gcm->vp.full_height      = gcm->height;
   gcm->rgb32               = video->rgb32;
   video_driver_set_size(gcm->vp.width, gcm->vp.height);

   if (input && input_data)
   {
      void *ps3input       = input_driver_init_wrap(&input_ps3, ps3_joypad.ident);
      *input               = ps3input ? &input_ps3 : NULL;
      *input_data          = ps3input;
   }

   return gcm;
}

static void gcm_fill_black(uint32_t *dst, uint32_t *dst_end, size_t sz)
{
  if (sz > dst_end - dst)
    sz = dst_end - dst;
  memset (dst, 0, sz * 4);
}

static void gcm_blit_buffer(
      rsxBuffer *buffer, const void *frame, unsigned width,
      unsigned height, unsigned pitch, int rgb32, bool do_scaling)
{
   int i;
   uint32_t *dst;
   uint32_t *dst_end;
   int pre_clean;
   int scale = 1, xofs = 0, yofs = 0;

   if (width > buffer->width)
      width = buffer->width;
   if (height > buffer->height)
      height = buffer->height;

   if (do_scaling)
   {
      scale = buffer->width / width;
      if (scale > buffer->height / height)
         scale = buffer->height / height;
      if (scale >= 10)
         scale = 10;
      if (scale >= 1)
      {
         xofs = (buffer->width - width * scale) / 2;
         yofs = (buffer->height - height * scale) / 2;
      }
      else
         scale = 1;
   }

   /* TODO/FIXME: let RSX do the copy */
   pre_clean = xofs + buffer->width * yofs;
   dst       = buffer->ptr;
   dst_end   = buffer->ptr + buffer->width * buffer->height;

   memset(dst, 0, pre_clean * 4);
   dst      += pre_clean;

   if (scale == 1)
   {
      if (rgb32)
      {
         const uint8_t *src = frame;
         for (i = 0; i < height; i++)
         {
            memcpy(dst, src, width * 4);
            gcm_fill_black(dst + width, dst_end, buffer->width - width);
            dst += buffer->width;
            src += pitch;
         }
      }
      else
      {
         const uint16_t *src = frame;
         for (i = 0; i < height; i++)
         {
            for (int j = 0; j < width; j++, src++, dst++)
            {
               u16 rgb565 = *src;
               u8 r = ((rgb565 >> 8) & 0xf8);
               u8 g = ((rgb565 >> 3) & 0xfc);
               u8 b = ((rgb565 << 3) & 0xfc);
               *dst = (r<<16) | (g<<8) | b;
            }
            gcm_fill_black(dst, dst_end, buffer->width - width);

            dst += buffer->width - width;
            src += pitch / 2 - width;
         }
      }
   }
   else
   {
      if (rgb32)
      {
         const uint32_t *src = frame;
         for (i = 0; i < height; i++)
         {
            for (int j = 0; j < width; j++, src++)
            {
               u32 c = *src;
               for (int k = 0; k < scale; k++, dst++)
                  for (int l = 0; l < scale; l++)
                     dst[l * buffer->width] = c;
            }
            for (int l = 0; l < scale; l++)
               gcm_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

            dst += buffer->width * scale - width * scale;
            src += pitch / 4 - width;
         }
      } else {
         const uint16_t *src = frame;
         for (i = 0; i < height; i++)
         {
            for (int j = 0; j < width; j++, src++)
            {
               u16 rgb565 = *src;
               u8 r = ((rgb565 >> 8) & 0xf8);
               u8 g = ((rgb565 >> 3) & 0xfc);
               u8 b = ((rgb565 << 3) & 0xfc);
               u32 c = (r<<16) | (g<<8) | b;
               for (int k = 0; k < scale; k++, dst++)
                  for (int l = 0; l < scale; l++)
                     dst[l * buffer->width] = c;
            }
            for (int l = 0; l < scale; l++)
               gcm_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

            dst += buffer->width * scale - width * scale;
            src += pitch / 2 - width;
         }
      }
   }

   if (dst < dst_end)
      memset(dst, 0, 4 * (dst_end - dst));
}

static void gcm_update_screen(gcm_video_t *gcm)
{
   rsxBuffer *buffer = gcm->menu_frame_enable
      ? &gcm->menuBuffers[gcm->menuBuffer]
      : &gcm->buffers[gcm->currentBuffer];
   gcm_flip(gcm->context, buffer->id);
   if (gcm->vsync)
      gcm_wait_flip();
#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif
}

static bool gcm_frame(void* data, const void* frame,
      unsigned width, unsigned height,
      uint64_t frame_count,
      unsigned pitch, const char* msg, video_frame_info_t *video_info)
{
   gcm_video_t       *gcm         = (gcm_video_t*)data;
#ifdef HAVE_MENU
   bool statistics_show           = video_info->statistics_show;
   struct font_params *osd_params = (struct font_params*)
      &video_info->osd_stat_params;
#endif

   if(frame && width && height)
   {
      gcm->currentBuffer++;
      if (gcm->currentBuffer >= MAX_BUFFERS)
         gcm->currentBuffer = 0;
      gcm_blit_buffer(
            &gcm->buffers[gcm->currentBuffer], frame, width, height, pitch,
            gcm->rgb32, true);
   }

   /* TODO: translucid menu */
   gcm_update_screen(gcm);

   return true;

#ifdef HAVE_MENU
   if (statistics_show)
      if (osd_params)
         font_driver_render_msg(gcm,
               video_info->stat_text,
               osd_params, NULL);
#endif

   if (msg)
      font_driver_render_msg(gcm, msg, NULL, NULL);
   return true;
}

static void gcm_set_nonblock_state(void* data, bool toggle,
      bool a, unsigned b)
{
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (gcm)
      gcm->vsync = !toggle;
}

static bool gcm_alive(void* data)
{
   (void)data;
   return true;
}

static bool gcm_focus(void* data)
{
   (void)data;
   return true;
}

static bool gcm_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void gcm_free(void* data)
{
   int i;
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (!gcm)
      return;

   gcmSetWaitFlip(gcm->context);
   for (i = 0; i < MAX_BUFFERS; i++)
     rsxFree(gcm->buffers[i].ptr);
   for (i = 0; i < MAX_BUFFERS; i++)
     rsxFree(gcm->menuBuffers[i].ptr);

#if 0
   rsxFinish(gcm->context, 1);
   free(gcm->host_addr);
#endif
   free (gcm);
}

static void gcm_set_texture_frame(void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
  gcm_video_t* gcm = (gcm_video_t*)data;
  int newBuffer    = gcm->menuBuffer + 1;

  if (newBuffer >= MAX_BUFFERS)
    newBuffer = 0;

  /* TODO: respect alpha */
  gcm_blit_buffer(&gcm->menuBuffers[newBuffer], frame, width, height,
	     width * (rgb32 ? 4 : 2), rgb32, true);
  gcm->menuBuffer = newBuffer;

  gcm_update_screen(gcm);
}

static void gcm_set_texture_enable(void* data, bool state, bool full_screen)
{
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (!gcm)
     return;
   
   gcm->menu_frame_enable = state;

   gcm_update_screen(gcm);
}

static void gcm_set_rotation(void* data, unsigned rotation)
{
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (!gcm)
      return;

   gcm->rotation = rotation;
   gcm->should_resize = true;
}
static void gcm_set_filtering(void* data, unsigned index, bool smooth)
{
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (gcm)
      gcm->smooth = smooth;
}

static void gcm_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   gcm_video_t *gcm = (gcm_video_t*)data;

   if(!gcm)
      return;

   gcm->keep_aspect   = true;
   gcm->should_resize = true;
}

static void gcm_apply_state_changes(void* data)
{
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (gcm)
      gcm->should_resize = true;

}

static void gcm_viewport_info(void* data, struct video_viewport* vp)
{
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (gcm)
      *vp = gcm->vp;
}

static void gcm_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   gcm_video_t* gcm = (gcm_video_t*)data;

   if (gcm && gcm->msg_rendering_enabled)
      font_driver_render_msg(data, msg, params, font);
}

static uint32_t gcm_get_flags(void *data)
{
   uint32_t             flags   = 0;

   return flags;
}

static const video_poke_interface_t gcm_poke_interface = {
   gcm_get_flags,
   NULL,                                  /* load_texture */
   NULL,                                  /* unload_texture */
   NULL,
   NULL,
   gcm_set_filtering,
   NULL,                                  /* get_video_output_size */
   NULL,                                  /* get_video_output_prev */
   NULL,                                  /* get_video_output_next */
   NULL,                                  /* get_current_framebuffer */
   NULL,
   gcm_set_aspect_ratio,
   gcm_apply_state_changes,
   gcm_set_texture_frame,
   gcm_set_texture_enable,
   gcm_set_osd_msg,
   NULL,                   /* show_mouse */
   NULL,                   /* grab_mouse_toggle */
   NULL,                   /* get_current_shader */
   NULL,                   /* get_current_software_framebuffer */
   NULL                    /* get_hw_render_interface */
};

static void gcm_get_poke_interface(void* data,
      const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &gcm_poke_interface;
}

static bool gcm_set_shader(void* data,
      enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_gcm =
{
   gcm_init,
   gcm_frame,
   gcm_set_nonblock_state,
   gcm_alive,
   gcm_focus,
   gcm_suppress_screensaver,
   NULL, /* has_windowed */
   gcm_set_shader,
   gcm_free,
   "gcm",
   NULL, /* set_viewport */
   gcm_set_rotation,
   gcm_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   gcm_get_poke_interface
};
