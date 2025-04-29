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
#include "../../frontend/drivers/platform_emscripten.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct
{
   EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
   unsigned fb_width;
   unsigned fb_height;
} emscripten_ctx_data_t;

static void gfx_ctx_emscripten_webgl_swap_interval(void *data, int interval)
{
   platform_emscripten_set_main_loop_interval(interval);
}

static void gfx_ctx_emscripten_webgl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   int input_width;
   int input_height;
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

   platform_emscripten_get_canvas_size(&input_width, &input_height);

   *resize = (emscripten->fb_width != input_width || emscripten->fb_height != input_height);
   *width  = emscripten->fb_width  = (unsigned)input_width;
   *height = emscripten->fb_height = (unsigned)input_height;
   *quit   = false;
}

/* https://github.com/emscripten-core/emscripten/issues/17816#issuecomment-1249719343 */
static void gfx_ctx_emscripten_webgl_swap_buffers(void *data)
{
   (void)data;
}

static void gfx_ctx_emscripten_webgl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

   if (!emscripten)
      return;

   *width  = emscripten->fb_width;
   *height = emscripten->fb_height;
}

static bool gfx_ctx_emscripten_webgl_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   switch (type)
   {
      // there is no way to get the actual DPI in emscripten, so return a standard value instead.
      // this is needed for menu touch/pointer swipe scrolling to work.
      case DISPLAY_METRIC_DPI:
         *value = 150.0f;
         break;

      default:
         *value = 0.0f;
         return false;
   }

   return true;
}

static void gfx_ctx_emscripten_webgl_destroy(void *data)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;

   if (!emscripten)
      return;

   emscripten_webgl_destroy_context(emscripten->ctx);

   free(data);
}

static void *gfx_ctx_emscripten_webgl_init(void *video_driver)
{
   int width, height;
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)
      calloc(1, sizeof(*emscripten));

   EmscriptenWebGLContextAttributes attrs = {0};
   emscripten_webgl_init_context_attributes(&attrs);
   attrs.alpha = false;
   attrs.depth = true;
   attrs.stencil = true;
   attrs.antialias = false;
   attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
#ifdef HAVE_OPENGLES3
   attrs.majorVersion = 2;
#else
   attrs.majorVersion = 1;
#endif
   attrs.minorVersion = 0;
   attrs.enableExtensionsByDefault = true;
   attrs.explicitSwapControl = false;
   attrs.renderViaOffscreenBackBuffer = false;
   attrs.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;

   if (!emscripten)
      return NULL;

   emscripten->ctx = emscripten_webgl_create_context("#canvas", &attrs);
   if (!emscripten->ctx)
   {
      RARCH_ERR("[EMSCRIPTEN/WebGL]: Failed to initialize webgl\n");
      goto error;
   }
   emscripten_webgl_get_drawing_buffer_size(emscripten->ctx, &width, &height);
   emscripten_webgl_make_context_current(emscripten->ctx);
   emscripten->fb_width = (unsigned)width;
   emscripten->fb_height = (unsigned)height;
   RARCH_LOG("[EMSCRIPTEN/WebGL]: Dimensions: %ux%u\n", emscripten->fb_width, emscripten->fb_height);

   return emscripten;

error:
   gfx_ctx_emscripten_webgl_destroy(video_driver);
   return NULL;
}

static bool gfx_ctx_emscripten_webgl_set_canvas_size(int width, int height)
{
#ifdef NO_CANVAS_RESIZE
   return false;
#endif
   double dpr = platform_emscripten_get_dpr();
   EMSCRIPTEN_RESULT r = emscripten_set_element_css_size("#canvas", (double)width / dpr, (double)height / dpr);
   RARCH_LOG("[EMSCRIPTEN/WebGL]: set canvas size to %d, %d\n", width, height);

   if (r != EMSCRIPTEN_RESULT_SUCCESS)
   {
      RARCH_ERR("[EMSCRIPTEN/WebGL]: error resizing canvas: %d\n", r);
      return false;
   }
   return true;
}

static bool gfx_ctx_emscripten_webgl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;
   if (!emscripten || !emscripten->ctx)
      return false;

   if (width != 0 && height != 0)
   { 
      if (!gfx_ctx_emscripten_webgl_set_canvas_size(width, height))
         return false;
   }
   emscripten->fb_width = width;
   emscripten->fb_height = height;

   return true;
}

bool gfx_ctx_emscripten_webgl_set_resize(void *data, unsigned width, unsigned height)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;
   if (!emscripten || !emscripten->ctx)
      return false;
   return gfx_ctx_emscripten_webgl_set_canvas_size(width, height);
}

static enum gfx_ctx_api gfx_ctx_emscripten_webgl_get_api(void *data) { return GFX_CTX_OPENGL_ES_API; }

static bool gfx_ctx_emscripten_webgl_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return true;
}

static void gfx_ctx_emscripten_webgl_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   void *rwebinput = input_driver_init_wrap(&input_rwebinput, name);
   *input          = rwebinput ? &input_rwebinput : NULL;
   *input_data     = rwebinput;
}

static bool gfx_ctx_emscripten_webgl_has_focus(void *data)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;
   return emscripten && emscripten->ctx;
}

static bool gfx_ctx_emscripten_webgl_suppress_screensaver(void *data, bool enable) { return false; }

static float gfx_ctx_emscripten_webgl_translate_aspect(void *data,
      unsigned width, unsigned height) { return (float)width / height; }

static void gfx_ctx_emscripten_webgl_bind_hw_render(void *data, bool enable)
{
   emscripten_ctx_data_t *emscripten = (emscripten_ctx_data_t*)data;
   emscripten_webgl_make_context_current(emscripten->ctx);
}

static uint32_t gfx_ctx_emscripten_webgl_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
   return flags;
}

static void gfx_ctx_emscripten_webgl_set_flags(void *data, uint32_t flags) { }

static gfx_ctx_proc_t gfx_ctx_emscripten_webgl_get_proc_address(const char *symbol)
{
   return emscripten_webgl_get_proc_address(symbol);
}

const gfx_ctx_driver_t gfx_ctx_emscripten_webgl = {
   gfx_ctx_emscripten_webgl_init,
   gfx_ctx_emscripten_webgl_destroy,
   gfx_ctx_emscripten_webgl_get_api,
   gfx_ctx_emscripten_webgl_bind_api,
   gfx_ctx_emscripten_webgl_swap_interval,
   gfx_ctx_emscripten_webgl_set_video_mode,
   gfx_ctx_emscripten_webgl_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_emscripten_webgl_get_metrics,
   gfx_ctx_emscripten_webgl_translate_aspect,
   NULL, /* update_title */
   gfx_ctx_emscripten_webgl_check_window,
   gfx_ctx_emscripten_webgl_set_resize,
   gfx_ctx_emscripten_webgl_has_focus,
   gfx_ctx_emscripten_webgl_suppress_screensaver,
   false,
   gfx_ctx_emscripten_webgl_swap_buffers,
   gfx_ctx_emscripten_webgl_input_driver,
   gfx_ctx_emscripten_webgl_get_proc_address,
   NULL,
   NULL,
   NULL,
   "webgl_emscripten",
   gfx_ctx_emscripten_webgl_get_flags,
   gfx_ctx_emscripten_webgl_set_flags,
   gfx_ctx_emscripten_webgl_bind_hw_render,
   NULL, /* get_context_data */
   NULL  /* make_current */
};
