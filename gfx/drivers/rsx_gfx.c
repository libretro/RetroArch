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

#define rsx_context_bind_hw_render(rsx, enable) \
   if (rsx->shared_context_use) \
      rsx->ctx_driver->bind_hw_render(rsx->ctx_data, enable)

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
} rsx_scale_vector_t;

typedef struct
{
   s16 x0, y0, x1, y1;
   s16 u0, v0, u1, v1;
} rsx_vertex_t;

typedef struct {
   video_viewport_t vp;
   rsxBuffer buffers[MAX_BUFFERS];
   rsxBuffer menuBuffers[MAX_BUFFERS];
   int currentBuffer, menuBuffer;
   gcmContextData* context;
   u16 width;
   u16 height;
   bool menu_frame_enable;
   bool rgb32;
   bool vsync;
   u32 depth_pitch;
   u32 depth_offset;
   u32* depth_buffer;

   bool smooth;
   unsigned rotation;
   bool keep_aspect;
   bool should_resize;
   bool msg_rendering_enabled;

   const shader_backend_t* shader;
   void* shader_data;
   void* renderchain_data;
   void* ctx_data;
   const gfx_ctx_driver_t* ctx_driver;
   bool shared_context_use;

   video_info_t video_info;
   struct video_tex_info tex_info;                    /* unsigned int alignment */
   struct video_tex_info prev_info[GFX_MAX_TEXTURES]; /* unsigned alignment */
   struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];   /* unsigned alignment */
} rsx_t;

static const gfx_ctx_driver_t* rsx_get_context(rsx_t* rsx)
{
   const gfx_ctx_driver_t* gfx_ctx = NULL;
   void* ctx_data = NULL;
   settings_t* settings = config_get_ptr();
   struct retro_hw_render_callback* hwr = video_driver_get_hw_context();
   
   bool video_shared_context = settings->bools.video_shared_context;
   enum gfx_ctx_api api = GFX_CTX_RSX_API;

   rsx->shared_context_use = video_shared_context && hwr->context_type != RETRO_HW_CONTEXT_NONE;
   
   if ((libretro_get_shared_context())
      && (hwr->context_type != RETRO_HW_CONTEXT_NONE))
      rsx->shared_context_use = true;

   gfx_ctx = video_context_driver_init_first(rsx,
      settings->arrays.video_context_driver,
      api, 1, 0, rsx->shared_context_use, &ctx_data);
   
   if (ctx_data)
      rsx->ctx_data = ctx_data;

   return gfx_ctx;
}

#ifndef HAVE_THREADS
static bool rsx_tasks_finder(retro_task_t *task,void *userdata)
{
   return task;
}
task_finder_data_t rsx_tasks_finder_data = {rsx_tasks_finder, NULL};
#endif

static int rsx_make_buffer(rsxBuffer * buffer, u16 width, u16 height, int id)
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

static int rsx_flip(gcmContextData *context, s32 buffer)
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

static void rsx_wait_rsx_idle(gcmContextData *context);

static void rsx_wait_flip(void)
{
  while (gcmGetFlipStatus() != 0)
    usleep (200);  /* Sleep, to not stress the cpu. */
  gcmResetFlipStatus();
}

static gcmContextData *rsx_init_screen(rsx_t* gcm)
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

   rsx_wait_rsx_idle(context);

   if (videoConfigure (0, &vconfig, NULL, 0) != 0)
      goto error;

   if (videoGetState (0, 0, &state) != 0)
      goto error;

   gcmSetFlipMode (GCM_FLIP_VSYNC); /* Wait for VSYNC to flip */

   gcm->depth_pitch = res.width * sizeof(u32);
   gcm->depth_buffer = (u32 *) rsxMemalign (64, (res.height * gcm->depth_pitch));  //Beware, if was (res.height * gcm->depth_pitch)*2
   
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

static void rsx_wait_rsx_idle(gcmContextData *context)
{
  u32 sLabelVal = 1;

  rsxSetWriteBackendLabel (context, GCM_LABEL_INDEX, sLabelVal);
  rsxSetWaitLabel (context, GCM_LABEL_INDEX, sLabelVal);

  sLabelVal++;

  waitFinish(context, sLabelVal);
}

static void* rsx_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   int i;
   rsx_t* rsx = malloc(sizeof(rsx_t));

   if (!rsx)
      return NULL;

   memset(rsx, 0, sizeof(rsx_t));

   rsx->context = rsx_init_screen(rsx);
   const gfx_ctx_driver_t* ctx_driver = rsx_get_context(rsx);

   if (!ctx_driver)
      return NULL;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);
   rsx->ctx_driver = ctx_driver;
   rsx->video_info = *video;

   for (i = 0; i < MAX_BUFFERS; i++)
      rsx_make_buffer(&rsx->buffers[i], rsx->width, rsx->height, i);

   for (i = 0; i < MAX_BUFFERS; i++)
      rsx_make_buffer(&rsx->menuBuffers[i], rsx->width, rsx->height, i + MAX_BUFFERS);

   rsx_flip(rsx->context, MAX_BUFFERS - 1);

   rsx->vp.x = 0;
   rsx->vp.y = 0;
   rsx->vp.width = rsx->width;
   rsx->vp.height = rsx->height;
   rsx->vp.full_width = rsx->width;
   rsx->vp.full_height = rsx->height;
   rsx->rgb32 = video->rgb32;
   video_driver_set_size(rsx->vp.width, rsx->vp.height);

   if (input && input_data)
   {
      void *ps3input       = input_driver_init_wrap(&input_ps3, ps3_joypad.ident);
      *input               = ps3input ? &input_ps3 : NULL;
      *input_data          = ps3input;
   }

   rsx_context_bind_hw_render(rsx, true);

   return rsx;
}

static void rsx_fill_black(uint32_t *dst, uint32_t *dst_end, size_t sz)
{
  if (sz > dst_end - dst)
    sz = dst_end - dst;
  memset (dst, 0, sz * 4);
}

static void rsx_blit_buffer(
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
            rsx_fill_black(dst + width, dst_end, buffer->width - width);
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
            rsx_fill_black(dst, dst_end, buffer->width - width);

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
               rsx_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

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
               rsx_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

            dst += buffer->width * scale - width * scale;
            src += pitch / 2 - width;
         }
      }
   }

   if (dst < dst_end)
      memset(dst, 0, 4 * (dst_end - dst));
}

static void rsx_update_screen(rsx_t* gcm)
{
   rsxBuffer *buffer = gcm->menu_frame_enable
      ? &gcm->menuBuffers[gcm->menuBuffer]
      : &gcm->buffers[gcm->currentBuffer];
   rsx_flip(gcm->context, buffer->id);
   if (gcm->vsync)
      rsx_wait_flip();
#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif
}

static bool rsx_frame(void* data, const void* frame,
      unsigned width, unsigned height,
      uint64_t frame_count,
      unsigned pitch, const char* msg, video_frame_info_t *video_info)
{
   rsx_t* gcm = (rsx_t*)data;
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
      rsx_blit_buffer(
            &gcm->buffers[gcm->currentBuffer], frame, width, height, pitch,
            gcm->rgb32, true);
   }

   /* TODO: translucid menu */
   rsx_update_screen(gcm);

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

static void rsx_set_nonblock_state(void* data, bool toggle,
      bool a, unsigned b)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      gcm->vsync = !toggle;
}

static bool rsx_alive(void* data)
{
   (void)data;
   return true;
}

static bool rsx_focus(void* data)
{
   (void)data;
   return true;
}

static bool rsx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void rsx_free(void* data)
{
   int i;
   rsx_t* gcm = (rsx_t*)data;

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

static void rsx_set_texture_frame(void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   rsx_t* gcm = (rsx_t*)data;
  int newBuffer    = gcm->menuBuffer + 1;

  if (newBuffer >= MAX_BUFFERS)
    newBuffer = 0;

  /* TODO: respect alpha */
  rsx_blit_buffer(&gcm->menuBuffers[newBuffer], frame, width, height,
	     width * (rgb32 ? 4 : 2), rgb32, true);
  gcm->menuBuffer = newBuffer;

  rsx_update_screen(gcm);
}

static void rsx_set_texture_enable(void* data, bool state, bool full_screen)
{
   rsx_t* gcm = (rsx_t*)data;

   if (!gcm)
     return;
   
   gcm->menu_frame_enable = state;

   rsx_update_screen(gcm);
}

static void rsx_set_rotation(void* data, unsigned rotation)
{
   rsx_t* gcm = (rsx_t*)data;

   if (!gcm)
      return;

   gcm->rotation = rotation;
   gcm->should_resize = true;
}
static void rsx_set_filtering(void* data, unsigned index, bool smooth)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      gcm->smooth = smooth;
}

static void rsx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   rsx_t* gcm = (rsx_t*)data;

   if(!gcm)
      return;

   gcm->keep_aspect   = true;
   gcm->should_resize = true;
}

static void rsx_apply_state_changes(void* data)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      gcm->should_resize = true;

}

static void rsx_viewport_info(void* data, struct video_viewport* vp)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      *vp = gcm->vp;
}

static void rsx_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm && gcm->msg_rendering_enabled)
      font_driver_render_msg(data, msg, params, font);
}

static uint32_t rsx_get_flags(void *data)
{
   uint32_t             flags   = 0;

   return flags;
}

static const video_poke_interface_t rsx_poke_interface = {
   rsx_get_flags,
   NULL,                                  /* load_texture */
   NULL,                                  /* unload_texture */
   NULL,
   NULL,
   rsx_set_filtering,
   NULL,                                  /* get_video_output_size */
   NULL,                                  /* get_video_output_prev */
   NULL,                                  /* get_video_output_next */
   NULL,                                  /* get_current_framebuffer */
   NULL,
   rsx_set_aspect_ratio,
   rsx_apply_state_changes,
   rsx_set_texture_frame,
   rsx_set_texture_enable,
   rsx_set_osd_msg,
   NULL,                   /* show_mouse */
   NULL,                   /* grab_mouse_toggle */
   NULL,                   /* get_current_shader */
   NULL,                   /* get_current_software_framebuffer */
   NULL,                   /* get_hw_render_interface */
   NULL,                   /* set_hdr_max_nits */
   NULL,                   /* set_hdr_paper_white_nits */
   NULL,                   /* set_hdr_contrast */
   NULL                    /* set_hdr_expand_gamut */
};

static void rsx_get_poke_interface(void* data,
      const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &rsx_poke_interface;
}

static bool rsx_set_shader(void* data,
      enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_gcm =
{
   rsx_init,
   rsx_frame,
   rsx_set_nonblock_state,
   rsx_alive,
   rsx_focus,
   rsx_suppress_screensaver,
   NULL, /* has_windowed */
   rsx_set_shader,
   rsx_free,
   "rsx",
   NULL, /* set_viewport */
   rsx_set_rotation,
   rsx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   rsx_get_poke_interface
};
