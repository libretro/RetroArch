/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

// KMS/DRM context, running without any window manager.
// Based on kmscube example by Rob Clark.

#include "../../driver.h"
#include "../gfx_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <bcm_host.h>

static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;

static volatile sig_atomic_t g_quit;
static bool g_inited;
static gfx_ctx_api g_api;

static unsigned g_fb_width; // Just use something for now.
static unsigned g_fb_height;

struct drm_fb
{
   struct gbm_bo *bo;
   uint32_t fb_id;
};

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static void gfx_ctx_set_swap_interval(unsigned interval, bool inited)
{
   eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;
   (void)width;
   (void)height;

   *resize = false;
   *quit   = g_quit;
}

static void gfx_ctx_swap_buffers(void)
{
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(bool reset)
{
   (void)reset;
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   *width  = g_fb_width;
   *height = g_fb_height;
}

static bool gfx_ctx_init(void)
{
   if (g_inited)
   {
      RARCH_ERR("[VC/EGL]: Attempted to re-initialize driver.\n");
      return false;
   }

   int32_t success;
   EGLBoolean result;
   EGLint num_config;

   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   DISPMANX_MODEINFO_T dispman_modeinfo;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

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

   bcm_host_init();

   // get an EGL display connection
   g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   rarch_assert(g_egl_dpy != EGL_NO_DISPLAY);

   // initialize the EGL display connection
   result = eglInitialize(g_egl_dpy, NULL, NULL);
   rarch_assert(result != EGL_FALSE);

   // get an appropriate EGL frame buffer configuration
   result = eglChooseConfig(g_egl_dpy, attribute_list, &g_config, 1, &num_config);
   rarch_assert(result != EGL_FALSE);

   // create an EGL rendering context
   g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, EGL_NO_CONTEXT, (driver.video == &video_gl) ? context_attributes : NULL);
   rarch_assert(g_egl_ctx != EGL_NO_CONTEXT);

   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &g_fb_width, &g_fb_height);
   rarch_assert(success >= 0);

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = g_fb_width;
   dst_rect.height = g_fb_height;

   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = g_fb_width << 16;
   src_rect.height = g_fb_height << 16;

   dispman_display = vc_dispmanx_display_open(0 /* LCD */);
   vc_dispmanx_display_get_info(dispman_display, &dispman_modeinfo);
   dispman_update = vc_dispmanx_update_start(0);

   dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
      0 /*layer*/, &dst_rect, 0 /*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0 /*clamp*/, DISPMANX_NO_ROTATE);

   nativewindow.element = dispman_element;
   nativewindow.width = g_fb_width;
   nativewindow.height = g_fb_height;
   vc_dispmanx_update_submit_sync(dispman_update);

   g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, &nativewindow, NULL);
   rarch_assert(g_egl_surf != EGL_NO_SURFACE);

   // connect the context to the surface
   result = eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx);
   rarch_assert(result != EGL_FALSE);

   return true;
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen)
{
   (void)bits;
   if (g_inited)
      return false;

   struct sigaction sa = {{0}};
   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   g_inited = true;
   return true;
}

static void gfx_ctx_destroy(void)
{
   if (g_egl_dpy)
   {
      if (g_egl_ctx)
      {
         eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(g_egl_dpy, g_egl_ctx);
      }

      if (g_egl_surf)
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      eglTerminate(g_egl_dpy);
   }

   g_egl_ctx  = NULL;
   g_egl_surf = NULL;
   g_egl_dpy  = NULL;
   g_config   = 0;
   g_inited   = false;
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   void *linuxinput = input_linuxraw.init();
   *input           = linuxinput ? &input_linuxraw : NULL;
   *input_data      = linuxinput;
}

static bool gfx_ctx_window_has_focus(void)
{
   return g_inited;
}

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   return eglGetProcAddress(symbol);
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   g_api = api;
   switch (api)
   {
      case GFX_CTX_OPENGL_API:
         return eglBindAPI(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
         return eglBindAPI(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
         return eglBindAPI(EGL_OPENVG_API);
      default:
         return false;
   }
}

const gfx_ctx_driver_t gfx_ctx_videocore = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
   "videocore",
};

