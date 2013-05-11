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

#include "../../driver.h"
#include "../../general.h"
#include "../gfx_common.h"
#include "../gl_common.h"

#include <EGL/egl.h>
#include <android/looper.h>

#include "../../frontend/frontend_android.h"
#include "../image.h"

#include "../fonts/gl_font.h"
#include <stdint.h>

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;
static bool g_resize;

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

   g_egl_dpy = EGL_NO_DISPLAY;
   g_egl_surf = EGL_NO_SURFACE;
   g_egl_ctx = EGL_NO_CONTEXT;
   g_config   = 0;
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
   struct android_app *android_app = (struct android_app*)g_android;
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
   EGLint format;

   EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

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

   RARCH_LOG("[ANDROID/EGL]: EGL version: %d.%d\n", egl_version_major, egl_version_minor);

   if (!eglChooseConfig(g_egl_dpy, attribs, &g_config, 1, &num_config))
   {
      RARCH_ERR("eglChooseConfig failed.\n");
      goto error;
   }

   int var = eglGetConfigAttrib(g_egl_dpy, g_config, EGL_NATIVE_VISUAL_ID, &format);

   if (!var)
   {
      RARCH_ERR("eglGetConfigAttrib failed: %d.\n", var);
      goto error;
   }

   ANativeWindow_setBuffersGeometry(android_app->window, 0, 0, format);

   if (!(g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, android_app->window, 0)))
   {
      RARCH_ERR("eglCreateWindowSurface failed.\n");
      goto error;
   }

   if (!(g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, 0, context_attributes)))
   {
      RARCH_ERR("eglCreateContext failed.\n");
      goto error;
   }

   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
   {
      RARCH_ERR("eglMakeCurrent failed.\n");
      goto error;
   }

   ALooper *looper = ALooper_forThread();
   if (!looper)
      ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

   return true;

error:
   RARCH_ERR("EGL error: %d.\n", eglGetError());
   gfx_ctx_destroy();
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
   char buf[128];
#if 0
   if (gfx_get_fps(buf, sizeof(buf), false))
      RARCH_LOG("%s.\n", buf);
#else
   gfx_get_fps(buf, sizeof(buf), false);
#endif
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

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   g_api = api;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_has_focus(void)
{
   return true;
}

#ifdef HAVE_EGL
static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}
#endif

const gfx_ctx_driver_t gfx_ctx_android = {
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
#ifdef HAVE_EGL
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
#endif
   NULL,
   "android",
};
