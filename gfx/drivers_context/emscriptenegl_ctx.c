/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <stdlib.h>
#include <unistd.h>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   unsigned fb_width;
   unsigned fb_height;
} emscripten_ctx_data_t;

static int emscripten_initial_width;
static int emscripten_initial_height;
static enum gfx_ctx_api emscripten_api = GFX_CTX_NONE;

static void gfx_ctx_emscripten_swap_interval(void *data, int interval)
{
   (void)data;

   if (interval == 0)
      emscripten_set_main_loop_timing(EM_TIMING_SETIMMEDIATE, 0);
   else
      emscripten_set_main_loop_timing(EM_TIMING_RAF, interval);
}

static void gfx_ctx_emscripten_get_canvas_size(int *width, int *height)
{
   EmscriptenFullscreenChangeEvent fullscreen_status;
   bool  is_fullscreen = false;
   EMSCRIPTEN_RESULT r = emscripten_get_fullscreen_status(&fullscreen_status);

   if (r == EMSCRIPTEN_RESULT_SUCCESS)
   {
      if (fullscreen_status.isFullscreen)
      {
         is_fullscreen = true;
         *width = fullscreen_status.screenWidth;
         *height = fullscreen_status.screenHeight;
      }
   }

   if (!is_fullscreen)
   {
      r = emscripten_get_canvas_element_size("#canvas", width, height);

      if (r != EMSCRIPTEN_RESULT_SUCCESS)
      {
         RARCH_ERR("[EMSCRIPTEN/EGL]: Could not get screen dimensions: %d\n",
            r);
         *width = 800;
         *height = 600;
      }
   }
}

static void gfx_ctx_emscripten_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   EMSCRIPTEN_RESULT r;
   int input_width;
   int input_height;
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

   gfx_ctx_emscripten_get_canvas_size(&input_width, &input_height);

   if (input_width == 0 || input_height == 0)
   {
      input_width = emscripten_initial_width;
      input_height = emscripten_initial_height;
      emscripten->fb_width = emscripten->fb_height = 0;
   }

   *width      = (unsigned)input_width;
   *height     = (unsigned)input_height;
   *resize     = false;

   if (input_width != emscripten->fb_width ||
      input_height != emscripten->fb_height)
   {
      r = emscripten_set_canvas_element_size("#canvas",
         input_width, input_height);

      if (r != EMSCRIPTEN_RESULT_SUCCESS)
         RARCH_ERR("[EMSCRIPTEN/EGL]: error resizing canvas: %d\n", r);

      /* fix Module.requestFullscreen messing with the canvas size */
      r = emscripten_set_element_css_size("#canvas",
         (double)input_width, (double)input_height);

      if (r != EMSCRIPTEN_RESULT_SUCCESS)
         RARCH_ERR("[EMSCRIPTEN/EGL]: error resizing canvas css: %d\n", r);

      *resize  = true;
   }

   emscripten->fb_width  = (unsigned)input_width;
   emscripten->fb_height = (unsigned)input_height;
   *quit       = false;
}

static void gfx_ctx_emscripten_swap_buffers(void *data, void *data2)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

   /* doesn't really do anything in WebGL, but it might if we use WebGL workers
    * in the future */
#ifdef HAVE_EGL
   egl_swap_buffers(&emscripten->egl);
#endif
}

static void gfx_ctx_emscripten_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

   if (!emscripten)
      return;

   *width  = emscripten->fb_width;
   *height = emscripten->fb_height;
}

static void gfx_ctx_emscripten_destroy(void *data)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

   if (!emscripten)
      return;

#ifdef HAVE_EGL
   egl_destroy(&emscripten->egl);
#endif

   free(data);
}

static void *gfx_ctx_emscripten_init(video_frame_info_t *video_info,
   void *video_driver)
{
#ifdef HAVE_EGL
   unsigned width, height;
   EGLint major, minor;
   EGLint n;
   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   static const EGLint context_attributes[] =
   {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
#endif
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)
      calloc(1, sizeof(*emscripten));

   if (!emscripten)
      return NULL;

   (void)video_driver;

   if (emscripten_initial_width == 0 || emscripten_initial_height == 0)
      emscripten_get_canvas_element_size("#canvas",
         &emscripten_initial_width, &emscripten_initial_height);

#ifdef HAVE_EGL
   if (g_egl_inited)
   {
      RARCH_LOG("[EMSCRIPTEN/EGL]: Attempted to re-initialize driver.\n");
      return (void*)"emscripten";
   }

   if (!egl_init_context(&emscripten->egl, EGL_NONE,
      (void *)EGL_DEFAULT_DISPLAY, &major, &minor, &n, attribute_list, NULL))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_context(&emscripten->egl, context_attributes))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(&emscripten->egl, 0))
      goto error;

   egl_get_video_size(&emscripten->egl, &width, &height);

   emscripten->fb_width  = width;
   emscripten->fb_height = height;
   RARCH_LOG("[EMSCRIPTEN/EGL]: Dimensions: %ux%u\n", width, height);
#endif

   return emscripten;

error:
   gfx_ctx_emscripten_destroy(video_driver);
   return NULL;
}

static bool gfx_ctx_emscripten_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;

   if (g_egl_inited)
      return false;

   g_egl_inited = true;
   return true;
}

static enum gfx_ctx_api gfx_ctx_emscripten_get_api(void *data)
{
   return emscripten_api;
}

static bool gfx_ctx_emscripten_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;

   emscripten_api = api;

   switch (api)
   {
#ifdef HAVE_EGL
      case GFX_CTX_OPENGL_ES_API:
         return egl_bind_api(EGL_OPENGL_ES_API);
#endif
      default:
         break;
   }

   return false;
}

static void gfx_ctx_emscripten_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   void *rwebinput = input_rwebinput.init(name);

   *input      = rwebinput ? &input_rwebinput : NULL;
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

static gfx_ctx_proc_t gfx_ctx_emscripten_get_proc_address(const char *symbol)
{
#ifdef HAVE_EGL
   return egl_get_proc_address(symbol);
#else
   return NULL;
#endif
}

static void gfx_ctx_emscripten_bind_hw_render(void *data, bool enable)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_bind_hw_render(&emscripten->egl, enable);
#endif
}

static uint32_t gfx_ctx_emscripten_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
   return flags;
}

static void gfx_ctx_emscripten_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_emscripten = {
   gfx_ctx_emscripten_init,
   gfx_ctx_emscripten_destroy,
   gfx_ctx_emscripten_get_api,
   gfx_ctx_emscripten_bind_api,
   gfx_ctx_emscripten_swap_interval,
   gfx_ctx_emscripten_set_video_mode,
   gfx_ctx_emscripten_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   gfx_ctx_emscripten_translate_aspect,
   NULL, /* update_title */
   gfx_ctx_emscripten_check_window,
   NULL, /* set_resize */
   gfx_ctx_emscripten_has_focus,
   gfx_ctx_emscripten_suppress_screensaver,
   false,
   gfx_ctx_emscripten_swap_buffers,
   gfx_ctx_emscripten_input_driver,
   gfx_ctx_emscripten_get_proc_address,
   gfx_ctx_emscripten_init_egl_image_buffer,
   gfx_ctx_emscripten_write_egl_image,
   NULL,
   "emscripten",
   gfx_ctx_emscripten_get_flags,
   gfx_ctx_emscripten_set_flags,
   gfx_ctx_emscripten_bind_hw_render,
   NULL,
   NULL
};
