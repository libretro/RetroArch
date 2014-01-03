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

#include "../../driver.h"
#include "../../general.h"
#include "../gfx_common.h"
#include "../gl_common.h"

#include <EGL/egl.h>
#include <bps/screen.h>
#include <bps/navigator.h>
#include <bps/event.h>
#include <screen/screen.h>
#include <sys/platform.h>
#include <GLES2/gl2.h>

#include "../image.h"

#include "../fonts/gl_font.h"
#include <stdint.h>

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

#define WINDOW_BUFFERS 2

static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig egl_config;
static bool g_resize;

screen_context_t screen_ctx;
screen_window_t screen_win;
static screen_display_t screen_disp;

GLfloat _angle;

static enum gfx_ctx_api g_api;

static void gfx_ctx_set_swap_interval(unsigned interval)
{
   RARCH_LOG("gfx_ctx_set_swap_interval(%d).\n", interval);
   eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_destroy(void)
{
   RARCH_LOG("gfx_ctx_destroy().\n");
   eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(g_egl_dpy, g_egl_ctx);
   eglDestroySurface(g_egl_dpy, g_egl_surf);
   eglTerminate(g_egl_dpy);
   //eglReleaseThread();

   g_egl_dpy = EGL_NO_DISPLAY;
   g_egl_surf = EGL_NO_SURFACE;
   g_egl_ctx = EGL_NO_CONTEXT;
   egl_config   = 0;
   g_resize   = false;
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   if (g_egl_dpy)
   {
      EGLint gl_width, gl_height;
      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);
      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &gl_height);
      *width  = gl_width;
      *height = gl_height;
   }
   else
   {
      *width  = 0;
      *height = 0;
   }
}

static bool gfx_ctx_init(void)
{
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

   const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_NONE
   };
   EGLint num_config;
   EGLint egl_version_major, egl_version_minor;
   int format = SCREEN_FORMAT_RGBX8888;

   EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
   int usage;

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

   if (!eglChooseConfig(g_egl_dpy, attribs, &egl_config, 1, &num_config))
   {
      RARCH_ERR("eglChooseConfig failed.\n");
      goto error;
   }

   if ((g_egl_ctx = eglCreateContext(g_egl_dpy, egl_config, 0, context_attributes)) == EGL_NO_CONTEXT)
   {
      RARCH_ERR("eglCreateContext failed.\n");
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
   int angle = atoi(getenv("ORIENTATION"));

   screen_display_mode_t screen_mode;
   if (screen_get_display_property_pv(screen_disp, SCREEN_PROPERTY_MODE, (void**)&screen_mode))
   {
      RARCH_ERR("screen_get_display_property_pv [SCREEN_PROPERTY_MODE] failed.\n");
      goto error;
   }

   int size[2];
   if (screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, size))
   {
      RARCH_ERR("screen_get_window_property_iv [SCREEN_PROPERTY_BUFFER_SIZE] failed.\n");
      goto error;
   }

   int buffer_size[2] = {size[0], size[1]};

   if ((angle == 0) || (angle == 180)) {
      if (((screen_mode.width > screen_mode.height) && (size[0] < size[1])) ||
            ((screen_mode.width < screen_mode.height) && (size[0] > size[1]))) {
         buffer_size[1] = size[0];
         buffer_size[0] = size[1];
      }
   } else if ((angle == 90) || (angle == 270)){
      if (((screen_mode.width > screen_mode.height) && (size[0] > size[1])) ||
            ((screen_mode.width < screen_mode.height && size[0] < size[1]))) {
         buffer_size[1] = size[0];
         buffer_size[0] = size[1];
      }
   } else {
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

   if (!(g_egl_surf = eglCreateWindowSurface(g_egl_dpy, egl_config, screen_win, 0)))
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
   gfx_ctx_destroy();
screen_error:
   screen_stop_events(screen_ctx);
   return false;
}

static void gfx_ctx_swap_buffers(void)
{
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   *quit = false;

   unsigned new_width, new_height;
   gfx_ctx_get_video_size(&new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   // Check if we are exiting.
   if (g_extern.lifecycle_state & (1ULL << RARCH_QUIT_KEY))
      *quit = true;
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(void)
{
   char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show;
   gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps));

   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}


static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
}

static unsigned gfx_ctx_get_resolution_width(unsigned resolution_id)
{
   int gl_width;
   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);

   return gl_width;
}

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));
   gfx_ctx_proc_t ret;

   void *sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)major;
   (void)minor;
   g_api = api;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_has_focus(void)
{
   return true;
}

const gfx_ctx_driver_t gfx_ctx_bbqnx = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_set_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   NULL,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
   NULL,
   "blackberry_qnx",
};
