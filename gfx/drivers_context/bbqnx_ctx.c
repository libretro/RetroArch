/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <bps/screen.h>
#include <bps/navigator.h>
#include <bps/event.h>
#include <screen/screen.h>
#include <sys/platform.h>

#include "../../driver.h"
#include "../../general.h"
#include "../../runloop.h"
#include "../video_monitor.h"
#include "../common/gl_common.h"

#include "../image/image.h"

#define WINDOW_BUFFERS 2

static bool g_use_hw_ctx;
static EGLContext g_egl_hw_ctx;
static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_egl_config;
static bool g_resize;

screen_context_t screen_ctx;
screen_window_t screen_win;
static screen_display_t screen_disp;

static enum gfx_ctx_api g_api;

static void gfx_ctx_qnx_set_swap_interval(void *data, unsigned interval)
{
   (void)data;
   eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_qnx_destroy(void *data)
{
   (void)data;

   if (g_egl_dpy)
   {
      if (g_egl_ctx)
      {
         eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(g_egl_dpy, g_egl_ctx);
      }

      if (g_egl_hw_ctx)
         eglDestroyContext(g_egl_dpy, g_egl_hw_ctx);

      if (g_egl_surf)
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      eglTerminate(g_egl_dpy);
   }

   /* Be as careful as possible in deinit. */

   g_egl_ctx     = NULL;
   g_egl_hw_ctx  = NULL;
   g_egl_surf    = NULL;
   g_egl_dpy     = NULL;
   g_egl_config  = 0;
   g_resize      = false;
}

static void gfx_ctx_qnx_get_video_size(void *data, unsigned *width, unsigned *height)
{
   EGLint gl_width, gl_height;

   (void)data;

   *width  = 0;
   *height = 0;

   if (!g_egl_dpy)
      return;

   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);
   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &gl_height);
   *width  = gl_width;
   *height = gl_height;
}

static bool gfx_ctx_qnx_init(void *data)
{
   EGLint num_config;
   EGLint egl_version_major, egl_version_minor;
   EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
   const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_NONE
   };
   int angle, size[2];
   int usage, format = SCREEN_FORMAT_RGBX8888;

   /* Create a screen context that will be used to 
    * create an EGL surface to receive libscreen events */

   RARCH_LOG("Initializing screen context...\n");
   if (!screen_ctx)
   {
      screen_create_context(&screen_ctx, 0);

      if (screen_request_events(screen_ctx) != BPS_SUCCESS)
      {
         RARCH_ERR("screen_request_events failed.\n");
         goto screen_error;
      }

      if (navigator_request_events(0) != BPS_SUCCESS)
      {
         RARCH_ERR("navigator_request_events failed.\n");
         goto screen_error;
      }

      if (navigator_rotation_lock(false) != BPS_SUCCESS)
      {
         RARCH_ERR("navigator_location_lock failed.\n");
         goto screen_error;
      }
   }

   usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_ROTATION;

   RARCH_LOG("Initializing context\n");

   if ((g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY)
   {
      RARCH_ERR("eglGetDisplay failed.\n");
      goto error;
   }

   if (!eglInitialize(g_egl_dpy, &egl_version_major, &egl_version_minor))
   {
      RARCH_ERR("eglInitialize failed.\n");
      goto error;
   }

   if (!eglBindAPI(EGL_OPENGL_ES_API))
   {
      RARCH_ERR("eglBindAPI failed.\n");
      goto error;
   }

   RARCH_LOG("[BLACKBERRY QNX/EGL]: EGL version: %d.%d\n", egl_version_major, egl_version_minor);

   if (!eglChooseConfig(g_egl_dpy, attribs, &g_egl_config, 1, &num_config))
      goto error;

   g_egl_ctx = eglCreateContext(g_egl_dpy, g_egl_config, EGL_NO_CONTEXT, context_attributes);

   if (g_egl_ctx == EGL_NO_CONTEXT)
      goto error;

   if (g_use_hw_ctx)
   {
      g_egl_hw_ctx = eglCreateContext(g_egl_dpy, g_egl_config, g_egl_ctx,
            context_attributes);
      RARCH_LOG("[Android/EGL]: Created shared context: %p.\n", (void*)g_egl_hw_ctx);

      if (g_egl_hw_ctx == EGL_NO_CONTEXT)
         goto error;
   }

   if(!screen_win)
   {
      if (screen_create_window(&screen_win, screen_ctx))
      {
	     RARCH_ERR("screen_create_window failed:.\n");
	     goto error;
      }
   }

   if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_FORMAT, &format))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_FORMAT] failed.\n");
      goto error;
   }

   if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_USAGE] failed.\n");
      goto error;
   }

   if (screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_DISPLAY, (void **)&screen_disp))
   {
      RARCH_ERR("screen_get_window_property_pv [SCREEN_PROPERTY_DISPLAY] failed.\n");
      goto error;
   }

   int screen_resolution[2];

   if (screen_get_display_property_iv(screen_disp, SCREEN_PROPERTY_SIZE, screen_resolution))
   {
      RARCH_ERR("screen_get_window_property_iv [SCREEN_PROPERTY_SIZE] failed.\n");
      goto error;
   }

#ifndef HAVE_BB10
   angle = atoi(getenv("ORIENTATION"));

   screen_display_mode_t screen_mode;
   if (screen_get_display_property_pv(screen_disp, SCREEN_PROPERTY_MODE, (void**)&screen_mode))
   {
      RARCH_ERR("screen_get_display_property_pv [SCREEN_PROPERTY_MODE] failed.\n");
      goto error;
   }

   if (screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, size))
   {
      RARCH_ERR("screen_get_window_property_iv [SCREEN_PROPERTY_BUFFER_SIZE] failed.\n");
      goto error;
   }

   int buffer_size[2] = {size[0], size[1]};

   if ((angle == 0) || (angle == 180))
   {
      if (((screen_mode.width > screen_mode.height) && (size[0] < size[1])) ||
            ((screen_mode.width < screen_mode.height) && (size[0] > size[1])))
      {
         buffer_size[1] = size[0];
         buffer_size[0] = size[1];
      }
   }
   else if ((angle == 90) || (angle == 270))
   {
      if (((screen_mode.width > screen_mode.height) && (size[0] > size[1])) ||
            ((screen_mode.width < screen_mode.height && size[0] < size[1])))
      {
         buffer_size[1] = size[0];
         buffer_size[0] = size[1];
      }
   }
   else
   {
      RARCH_ERR("Navigator returned an unexpected orientation angle.\n");
      goto error;
   }


   if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, buffer_size))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_BUFFER_SIZE] failed.\n");
      goto error;
   }

   if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ROTATION, &angle))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_ROTATION] failed.\n");
      goto error;
   }
#endif

   if (screen_create_window_buffers(screen_win, WINDOW_BUFFERS))
   {
      RARCH_ERR("screen_create_window_buffers failed.\n");
      goto error;
   }

   if (!(g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_egl_config, screen_win, 0)))
   {
      RARCH_ERR("eglCreateWindowSurface failed.\n");
      goto error;
   }


   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
   {
      RARCH_ERR("eglMakeCurrent failed.\n");
      goto error;
   }

   return true;

error:
   RARCH_ERR("EGL error: %d.\n", eglGetError());
   gfx_ctx_qnx_destroy(data);
screen_error:
   screen_stop_events(screen_ctx);
   return false;
}

static void gfx_ctx_qnx_swap_buffers(void *data)
{
   (void)data;
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_qnx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   unsigned new_width, new_height;
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   (void)data;
   (void)frame_count;

   *quit = false;

   gfx_ctx_qnx_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   /* Check if we are exiting. */
   if (system->shutdown)
      *quit = true;
}

static void gfx_ctx_qnx_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_qnx_update_window_title(void *data)
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

static bool gfx_ctx_qnx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}


static void gfx_ctx_qnx_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}

static gfx_ctx_proc_t gfx_ctx_qnx_get_proc_address(const char *symbol)
{
   gfx_ctx_proc_t ret;
   void *sym__;

   retro_assert(sizeof(void*) == sizeof(void (*)(void)));

   sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

static bool gfx_ctx_qnx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;

   g_api = api;

   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_qnx_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_qnx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool gfx_ctx_qnx_has_windowed(void *data)
{
   (void)data;
   return false;
}

static void gfx_qnx_ctx_bind_hw_render(void *data, bool enable)
{
   (void)data;

   g_use_hw_ctx = enable;

   if (!g_egl_dpy)
      return;
   if (!g_egl_surf)
      return;

   eglMakeCurrent(g_egl_dpy, g_egl_surf,
         g_egl_surf, enable ? g_egl_hw_ctx : g_egl_ctx);
}

const gfx_ctx_driver_t gfx_ctx_bbqnx = {
   gfx_ctx_qnx_init,
   gfx_ctx_qnx_destroy,
   gfx_ctx_qnx_bind_api,
   gfx_ctx_qnx_set_swap_interval,
   gfx_ctx_qnx_set_video_mode,
   gfx_ctx_qnx_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   gfx_ctx_qnx_update_window_title,
   gfx_ctx_qnx_check_window,
   gfx_ctx_qnx_set_resize,
   gfx_ctx_qnx_has_focus,
   gfx_ctx_qnx_suppress_screensaver,
   gfx_ctx_qnx_has_windowed,
   gfx_ctx_qnx_swap_buffers,
   gfx_ctx_qnx_input_driver,
   gfx_ctx_qnx_get_proc_address,
   NULL,
   NULL,
   NULL,
   "blackberry_qnx",
   gfx_qnx_ctx_bind_hw_render,
};
