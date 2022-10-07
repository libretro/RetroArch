/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <sys/system_properties.h>

#include <formats/image.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../frontend/drivers/platform_unix.h"
#include "../../verbosity.h"
#include "../../configuration.h"

#ifdef HAVE_OPENGLES
#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR                  0x0040
#endif
#endif

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
} android_ctx_data_t;

/* TODO/FIXME - static globals */
static enum gfx_ctx_api android_api           = GFX_CTX_NONE;
#ifdef HAVE_OPENGLES
static bool g_es3                             = false;
#endif

static void android_gfx_ctx_destroy(void *data)
{
   android_ctx_data_t *and         = (android_ctx_data_t*)data;

   if (!and)
      return;

#ifdef HAVE_EGL
   egl_destroy(&and->egl);
#endif

   free(data);
}

static void *android_gfx_ctx_init(void *video_driver)
{
#ifdef HAVE_OPENGLES
   EGLint n, major, minor;
   EGLint format;
#if 0
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   bool debug = hwr->debug_context;
#endif
   EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      EGL_NONE
   };
#endif
   struct android_app *android_app = (struct android_app*)g_android;
   android_ctx_data_t        *and  = (android_ctx_data_t*)
      calloc(1, sizeof(*and));

   if (!android_app || !and)
      return false;

#ifdef HAVE_OPENGLES
   if (g_es3)
      attribs[1] = EGL_OPENGL_ES3_BIT_KHR;
#endif

#ifdef HAVE_EGL
   RARCH_LOG("Android EGL: GLES version = %d.\n", g_es3 ? 3 : 2);

   if (!egl_init_context(&and->egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
            &major, &minor, &n, attribs, NULL))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_get_native_visual_id(&and->egl, &format))
      goto error;
#endif

   slock_lock(android_app->mutex);
   if (!android_app->window)
   {
      slock_unlock(android_app->mutex);
      android_gfx_ctx_destroy(and);
      return NULL;
   }

   ANativeWindow_setBuffersGeometry(android_app->window, 0, 0, format);

   slock_unlock(android_app->mutex);
   return and;

error:
   android_gfx_ctx_destroy(and);

   return NULL;
}

static void android_gfx_ctx_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   android_ctx_data_t *and  = (android_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_get_video_size(&and->egl, width, height);
#endif
}

static void android_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   unsigned new_width       = 0;
   unsigned new_height      = 0;
   android_ctx_data_t *and  = (android_ctx_data_t*)data;

   *quit                    = false;

#ifdef HAVE_EGL
   egl_get_video_size(&and->egl, &new_width, &new_height);
#endif

   if (new_width != *width || new_height != *height)
   {
      RARCH_LOG("[Android]: Resizing (%u x %u) -> (%u x %u).\n",
              *width, *height, new_width, new_height);

      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static bool android_gfx_ctx_set_resize(void *data,
      unsigned width, unsigned height) { return false; }

static bool android_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#if defined(HAVE_OPENGLES)
   struct android_app *android_app = (struct android_app*)g_android;
   android_ctx_data_t         *and = (android_ctx_data_t*)data;
#if defined(HAVE_EGL)
   EGLint     context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, g_es3 ? 3 : 2,
#if 0
      EGL_CONTEXT_FLAGS_KHR, debug ? EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR : 0,
#endif
      EGL_NONE
   };
#endif
#endif

   switch (android_api)
   {
      case GFX_CTX_OPENGL_API:
         break;
      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_OPENGLES) && defined(HAVE_EGL)
         if (!egl_create_context(&and->egl, context_attributes))
         {
            egl_report_error();
            return false;
         }

         if (!egl_create_surface(&and->egl, android_app->window))
            return false;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;
}

static void android_gfx_ctx_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   void *androidinput   = input_driver_init_wrap(&input_android, joypad_name);

   *input               = androidinput ? &input_android : NULL;
   *input_data          = androidinput;
}

static enum gfx_ctx_api android_gfx_ctx_get_api(void *data)
{
   return android_api;
}

static bool android_gfx_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   unsigned version;
   android_api = api;

#ifdef HAVE_OPENGLES
   version     = major * 100 + minor;
   if (version >= 300)
      g_es3 = true;
   if (api == GFX_CTX_OPENGL_ES_API)
      return true;
#endif

   return false;
}

static bool android_gfx_ctx_has_focus(void *data)
{
   bool                    focused = false;
   struct android_app *android_app = (struct android_app*)g_android;
   if (!android_app)
      return true;

   slock_lock(android_app->mutex);
   focused = !android_app->unfocused;
   slock_unlock(android_app->mutex);

   return focused;
}

static bool android_gfx_ctx_suppress_screensaver(void *data, bool enable) { return false; }

static bool android_gfx_ctx_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   static int dpi = -1;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
      case DISPLAY_METRIC_MM_HEIGHT:
         return false;
      case DISPLAY_METRIC_DPI:
         if (dpi == -1)
         {
            char density[PROP_VALUE_MAX];
            android_dpi_get_density(density, sizeof(density));
            if (string_is_empty(density))
               goto dpi_fallback;
            if ((dpi = atoi(density)) <= 0)
               goto dpi_fallback;
         }
         *value = (float)dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;

dpi_fallback:
   /* add a fallback in case the device doesn't report DPI.
    * Hopefully fixes issues with the moto G2. */
   dpi    = 90;
   *value = (float)dpi;
   return true;
}

static void android_gfx_ctx_swap_buffers(void *data)
{
   android_ctx_data_t *and  = (android_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_swap_buffers(&and->egl);
#endif
}

static void android_gfx_ctx_set_swap_interval(void *data, int swap_interval)
{
   android_ctx_data_t *and  = (android_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_set_swap_interval(&and->egl, swap_interval);
#endif
}

static void android_gfx_ctx_bind_hw_render(void *data, bool enable)
{
   android_ctx_data_t *and  = (android_ctx_data_t*)data;
#ifdef HAVE_EGL
   egl_bind_hw_render(&and->egl, enable);
#endif
}

static uint32_t android_gfx_ctx_get_flags(void *data)
{
   uint32_t flags = 0;

#ifdef HAVE_GLSL
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif

   return flags;
}

static void android_gfx_ctx_set_flags(void *data, uint32_t flags) { }

const gfx_ctx_driver_t gfx_ctx_android = {
   android_gfx_ctx_init,
   android_gfx_ctx_destroy,
   android_gfx_ctx_get_api,
   android_gfx_ctx_bind_api,
   android_gfx_ctx_set_swap_interval,
   android_gfx_ctx_set_video_mode,
   android_gfx_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   android_gfx_ctx_get_metrics,
   NULL,
   NULL, /* update_title */
   android_gfx_ctx_check_window,
   android_gfx_ctx_set_resize,
   android_gfx_ctx_has_focus,
   android_gfx_ctx_suppress_screensaver,
   false, /* has_windowed */
   android_gfx_ctx_swap_buffers,
   android_gfx_ctx_input_driver,
#ifdef HAVE_EGL
   egl_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   "egl_android",
   android_gfx_ctx_get_flags,
   android_gfx_ctx_set_flags,
   android_gfx_ctx_bind_hw_render,
   NULL,
   NULL
};
