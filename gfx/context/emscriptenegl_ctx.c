/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2012-2014 - Michael Lelli
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

// VideoCore context, for Rasperry Pi.

#include "../../driver.h"
#include "../gfx_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdint.h>
#include <unistd.h>

#include <emscripten/emscripten.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

static EGLContext g_egl_hw_ctx;
static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;

static bool g_inited;
static bool g_use_hw_ctx;

static unsigned g_fb_width;
static unsigned g_fb_height;

static void gfx_ctx_swap_interval(void *data, unsigned interval)
{
   (void)data;
   // no way to control vsync in WebGL
   (void)interval;
}

static void gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)data;
   (void)frame_count;
   int iWidth, iHeight, isFullscreen;

   emscripten_get_canvas_size(&iWidth, &iHeight, &isFullscreen);
   *width  = (unsigned) iWidth;
   *height = (unsigned) iHeight;

   if (*width != g_fb_width || *height != g_fb_height)
      *resize = true;
   else
      *resize = false;

   g_fb_width = (unsigned) iWidth;
   g_fb_height = (unsigned) iHeight;
   *quit   = false;
}

static void gfx_ctx_swap_buffers(void *data)
{
   (void)data;
   // no-op in emscripten, no way to force swap/wait for vsync in browsers
   //eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(void *data)
{
   (void)data;
   char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show;
   gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps));

   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static void gfx_ctx_get_video_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;
   *width  = g_fb_width;
   *height = g_fb_height;
}

static void gfx_ctx_destroy(void *data);

static bool gfx_ctx_init(void *data)
{
   (void)data;
   EGLint width;
   EGLint height;

   RARCH_LOG("[EMSCRIPTEN/EGL]: Initializing...\n");
   if (g_inited)
   {
      RARCH_LOG("[EMSCRIPTEN/EGL]: Attempted to re-initialize driver.\n");
      return true;
   }

   EGLint num_config;

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

   // get an EGL display connection
   g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!g_egl_dpy)
      goto error;

   // initialize the EGL display connection
   if (!eglInitialize(g_egl_dpy, NULL, NULL))
      goto error;

   // get an appropriate EGL frame buffer configuration
   if (!eglChooseConfig(g_egl_dpy, attribute_list, &g_config, 1, &num_config))
      goto error;

   // create an EGL rendering context
   g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, EGL_NO_CONTEXT, context_attributes);
   if (!g_egl_ctx)
      goto error;

   if (g_use_hw_ctx)
   {
      g_egl_hw_ctx = eglCreateContext(g_egl_dpy, g_config, g_egl_ctx,
            context_attributes);
      RARCH_LOG("[VC/EGL]: Created shared context: %p.\n", (void*)g_egl_hw_ctx);

      if (g_egl_hw_ctx == EGL_NO_CONTEXT)
         goto error;
   }

   // create an EGL window surface
   g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, 0, NULL);
   if (!g_egl_surf)
      goto error;

   // connect the context to the surface
   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
      goto error;

   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &width);
   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &height);
   g_fb_width = width;
   g_fb_height = height;
   RARCH_LOG("[EMSCRIPTEN/EGL]: Dimensions: %ux%u\n", width, height);

   return true;

error:
   gfx_ctx_destroy(data);
   return false;
}

static bool gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;
   if (g_inited)
      return false;

   g_inited = true;
   return true;
}

static bool gfx_ctx_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;
   switch (api)
   {
      case GFX_CTX_OPENGL_ES_API:
         return eglBindAPI(EGL_OPENGL_ES_API);
      default:
         return false;
   }
}

static void gfx_ctx_destroy(void *data)
{
   (void)data;
   if (g_egl_dpy)
   {
      eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      if (g_egl_ctx)
      {
         eglDestroyContext(g_egl_dpy, g_egl_ctx);
      }

      if (g_egl_surf)
      {
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      }

      eglTerminate(g_egl_dpy);
   }

   g_egl_ctx      = NULL;
   g_egl_surf     = NULL;
   g_egl_dpy      = NULL;
   g_config       = 0;
   g_inited       = false;
}

static void gfx_ctx_input_driver(void *data, const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;

   void *rwebinput = input_rwebinput.init();

   if (rwebinput)
   {
      *input      = &input_rwebinput;
      *input_data = rwebinput;
   }
}

static bool gfx_ctx_has_focus(void *data)
{
   (void)data;
   return g_inited;
}

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   return eglGetProcAddress(symbol);
}

static float gfx_ctx_translate_aspect(void *data, unsigned width, unsigned height)
{
   (void)data;
   return (float)width / height;
}

static bool gfx_ctx_init_egl_image_buffer(void *data, const video_info_t *video)
{
   (void)data;
   return false;
}

static bool gfx_ctx_write_egl_image(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   (void)data;
   return false;
}

static void gfx_ctx_bind_hw_render(void *data, bool enable)
{
   (void)data;
   g_use_hw_ctx = enable;
   if (g_egl_dpy && g_egl_surf)
      eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, enable ? g_egl_hw_ctx : g_egl_ctx);
}

const gfx_ctx_driver_t gfx_ctx_emscripten = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   gfx_ctx_translate_aspect,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
   NULL,
   "emscripten",
   gfx_ctx_bind_hw_render,
};
