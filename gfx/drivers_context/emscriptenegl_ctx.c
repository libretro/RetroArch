/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdint.h>
#include <unistd.h>

#include <emscripten/emscripten.h>

#include "../../driver.h"
#include "../../runloop.h"
#include "../video_context_driver.h"
#include "../common/egl_common.h"
#include "../common/gl_common.h"
#include "../video_monitor.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

static unsigned g_fb_width;
static unsigned g_fb_height;

static void gfx_ctx_emscripten_swap_interval(void *data, unsigned interval)
{
   (void)data;
   /* no way to control VSync in WebGL. */
   (void)interval;
}

static void gfx_ctx_emscripten_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   int iWidth, iHeight, isFullscreen;

   (void)data;
   (void)frame_count;

   emscripten_get_canvas_size(&iWidth, &iHeight, &isFullscreen);
   *width  = (unsigned) iWidth;
   *height = (unsigned) iHeight;
   *resize = false;

   if (*width != g_fb_width || *height != g_fb_height)
      *resize = true;

   g_fb_width = (unsigned) iWidth;
   g_fb_height = (unsigned) iHeight;
   *quit   = false;
}

static void gfx_ctx_emscripten_swap_buffers(void *data)
{
   (void)data;
   /* no-op in emscripten, no way to force swap/wait for VSync in browsers */
#if 0
   egl_swap_buffers(data);
#endif
}

static void gfx_ctx_emscripten_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_emscripten_update_window_title(void *data)
{
   char buf[128]        = {0};
   char buf_fps[128]    = {0};
   settings_t *settings = config_get_ptr();

   (void)data;

   video_monitor_get_fps(buf, sizeof(buf),
         buf_fps, sizeof(buf_fps));
   if (settings->fps_show)
      rarch_main_msg_queue_push(buf_fps, 1, 1, false);
}

static void gfx_ctx_emscripten_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   (void)data;
   *width  = g_fb_width;
   *height = g_fb_height;
}

static void gfx_ctx_emscripten_destroy(void *data);

static bool gfx_ctx_emscripten_init(void *data)
{
   EGLint width, height, n, major, minor;
   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   static const EGLint context_attributes[] =
   {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   (void)data;

   if (g_egl_inited)
   {
      RARCH_LOG("[EMSCRIPTEN/EGL]: Attempted to re-initialize driver.\n");
      return true;
   }

   if (!egl_init_context(EGL_DEFAULT_DISPLAY, &major, &minor,
            &n, attribute_list))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_context(context_attributes))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(0))
      goto error;

   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &width);
   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &height);
   g_fb_width = width;
   g_fb_height = height;
   RARCH_LOG("[EMSCRIPTEN/EGL]: Dimensions: %ux%u\n", width, height);

   return true;

error:
   gfx_ctx_emscripten_destroy(data);
   return false;
}

static bool gfx_ctx_emscripten_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;

   if (g_egl_inited)
      return false;

   g_egl_inited = true;
   return true;
}

static bool gfx_ctx_emscripten_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;

   switch (api)
   {
      case GFX_CTX_OPENGL_ES_API:
         return eglBindAPI(EGL_OPENGL_ES_API);
      default:
         break;
   }

   return false;
}

static void gfx_ctx_emscripten_destroy(void *data)
{
   egl_destroy(data);

   g_egl_inited       = false;
}

static void gfx_ctx_emscripten_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   void *rwebinput = NULL;

   (void)data;

   *input = NULL;

   rwebinput = input_rwebinput.init();

   if (!rwebinput)
      return;

   *input      = &input_rwebinput;
   *input_data = rwebinput;
}

static bool gfx_ctx_emscripten_has_focus(void *data)
{
   (void)data;

   return g_egl_inited;
}

static bool gfx_ctx_emscripten_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool gfx_ctx_emscripten_has_windowed(void *data)
{
   (void)data;

   /* TODO -verify. */
   return true;
}

static float gfx_ctx_emscripten_translate_aspect(void *data,
      unsigned width, unsigned height)
{
   (void)data;

   return (float)width / height;
}

static bool gfx_ctx_emscripten_init_egl_image_buffer(void *data,
      const video_info_t *video)
{
   (void)data;

   return false;
}

static bool gfx_ctx_emscripten_write_egl_image(void *data,
      const void *frame, unsigned width, unsigned height, unsigned pitch,
      bool rgb32, unsigned index, void **image_handle)
{
   (void)data;
   return false;
}

const gfx_ctx_driver_t gfx_ctx_emscripten = {
   gfx_ctx_emscripten_init,
   gfx_ctx_emscripten_destroy,
   gfx_ctx_emscripten_bind_api,
   gfx_ctx_emscripten_swap_interval,
   gfx_ctx_emscripten_set_video_mode,
   gfx_ctx_emscripten_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   gfx_ctx_emscripten_translate_aspect,
   gfx_ctx_emscripten_update_window_title,
   gfx_ctx_emscripten_check_window,
   gfx_ctx_emscripten_set_resize,
   gfx_ctx_emscripten_has_focus,
   gfx_ctx_emscripten_suppress_screensaver,
   gfx_ctx_emscripten_has_windowed,
   gfx_ctx_emscripten_swap_buffers,
   gfx_ctx_emscripten_input_driver,
   egl_get_proc_address,
   gfx_ctx_emscripten_init_egl_image_buffer,
   gfx_ctx_emscripten_write_egl_image,
   NULL,
   "emscripten",
   egl_bind_hw_render,
};
