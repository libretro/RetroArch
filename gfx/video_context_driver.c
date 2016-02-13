/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <string.h>

#include <string/stdstring.h>

#include "video_context_driver.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../general.h"
#include "../verbosity.h"

static const gfx_ctx_driver_t *gfx_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_D3D)
   &gfx_ctx_d3d,
#endif
#if defined(HAVE_VIDEOCORE)
   &gfx_ctx_videocore,
#endif
#if defined(HAVE_MALI_FBDEV)
   &gfx_ctx_mali_fbdev,
#endif
#if defined(HAVE_VIVANTE_FBDEV)
   &gfx_ctx_vivante_fbdev,
#endif
#if defined(_WIN32) && defined(HAVE_OPENGL)
   &gfx_ctx_wgl,
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && !defined(HAVE_OPENGLES)
   &gfx_ctx_glx,
#endif
#if defined(HAVE_WAYLAND) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
   &gfx_ctx_drm_egl,
#endif
#if defined(ANDROID)
   &gfx_ctx_android,
#endif
#if defined(__QNX__)
   &gfx_ctx_bbqnx,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
   &gfx_ctx_cocoagl,
#endif
#if defined(__APPLE__) && !defined(TARGET_IPHONE_SIMULATOR) && !defined(TARGET_OS_IPHONE)
   &gfx_ctx_cgl,
#endif
#if (defined(HAVE_SDL) || defined(HAVE_SDL2)) && defined(HAVE_OPENGL)
   &gfx_ctx_sdl_gl,
#endif
#ifdef EMSCRIPTEN
   &gfx_ctx_emscripten,
#endif
   &gfx_ctx_null,
   NULL
};

static const gfx_ctx_driver_t  *current_video_context;
static        void *video_context_data;

void gfx_ctx_set(const gfx_ctx_driver_t *ctx_driver)
{
   if (!ctx_driver)
      return;
   current_video_context = ctx_driver;
}

void gfx_ctx_destroy(const gfx_ctx_driver_t *ctx_driver)
{
   current_video_context = NULL;
}

const char *gfx_ctx_get_ident(void)
{
   const gfx_ctx_driver_t *ctx = current_video_context;

   if (!ctx)
      return NULL;
   return ctx->ident;
}

void gfx_ctx_update_window_title(void)
{
   if (current_video_context->update_window_title)
      current_video_context->update_window_title(video_context_data);
}

void gfx_ctx_get_video_output_size(unsigned *width, unsigned *height)
{
   if (!current_video_context || !current_video_context->get_video_output_size)
      return;
   current_video_context->get_video_output_size(video_context_data, width, height);
}

bool gfx_ctx_get_video_output_prev(void)
{
   if (!current_video_context 
         || !current_video_context->get_video_output_prev)
      return false;
   current_video_context->get_video_output_prev(video_context_data);
   return true;
}

bool gfx_ctx_get_video_output_next(void)
{
   if (!current_video_context || 
         !current_video_context->get_video_output_next)
      return false;
   current_video_context->get_video_output_next(video_context_data);
   return true;
}

void gfx_ctx_bind_hw_render(bool enable)
{
   if (current_video_context->bind_hw_render)
      current_video_context->bind_hw_render(video_context_data, enable);
}


bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!current_video_context || !current_video_context->set_video_mode)
      return false;
   return current_video_context->set_video_mode(
         video_context_data, width, height, fullscreen);
}

void gfx_ctx_translate_aspect(float *aspect,
      unsigned width, unsigned height)
{
   if (!current_video_context || !current_video_context->translate_aspect)
      return;

   *aspect = current_video_context->translate_aspect(
         video_context_data, width, height);
}

bool gfx_ctx_get_metrics(enum display_metric_types type, float *value)
{
   if (!current_video_context || !current_video_context->get_metrics)
      return false;
   return current_video_context->get_metrics(video_context_data, type,
         value);
}

bool gfx_ctx_image_buffer_init(const video_info_t* info)
{
   if (!current_video_context || !current_video_context->image_buffer_init)
      return false;
   return current_video_context->image_buffer_init(video_context_data, info);
}

bool gfx_ctx_image_buffer_write(const void *frame, unsigned width,
         unsigned height, unsigned pitch, bool rgb32,
         unsigned index, void **image_handle)
{
   if (!current_video_context || !current_video_context->image_buffer_write)
      return false;
   return current_video_context->image_buffer_write(video_context_data,
         frame, width, height, pitch,
         rgb32, index, image_handle);
}

retro_proc_address_t gfx_ctx_get_proc_address(const char *sym)
{
   return current_video_context->get_proc_address(sym);
}

void gfx_ctx_show_mouse(bool state)
{
   if (!video_context_data || !current_video_context->show_mouse)
      return;
   current_video_context->show_mouse(video_context_data, state);
}

bool gfx_ctx_has_windowed(void)
{
   if (!video_context_data)
      return false;
   return current_video_context->has_windowed(video_context_data);
}

bool gfx_ctx_check_window(bool *quit, bool *resize,
      unsigned *width, unsigned *height)
{
   uint64_t       *frame_count;
   
   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

   if (!video_context_data)
      return false;
   
   current_video_context->check_window(video_context_data, quit,
         resize, width, height, (unsigned int)*frame_count);

   return true;
}

bool gfx_ctx_suppress_screensaver(bool enable)
{
   if (video_context_data && current_video_context)
      return current_video_context->suppress_screensaver(
            video_context_data, enable);
   return false;
}

void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   current_video_context->get_video_size(video_context_data, width, height);
}

void gfx_ctx_swap_interval(unsigned interval)
{
   if (current_video_context)
      current_video_context->swap_interval(video_context_data, interval);
}

bool gfx_ctx_set_resize(unsigned width, unsigned height)
{
   if (!current_video_context)
      return false;

   return current_video_context->set_resize(
         video_context_data, width, height);
}

void gfx_ctx_input_driver(
      const input_driver_t **input, void **input_data)
{
   if (current_video_context)
      current_video_context->input_driver(
            video_context_data, input, input_data);
}

/**
 * find_gfx_ctx_driver_index:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds graphics context driver index by @ident name.
 *
 * Returns: graphics context driver index if driver was found, otherwise
 * -1.
 **/
static int find_gfx_ctx_driver_index(const char *ident)
{
   unsigned i;
   for (i = 0; gfx_ctx_drivers[i]; i++)
      if (string_is_equal_noncase(ident, gfx_ctx_drivers[i]->ident))
         return i;
   return -1;
}

/**
 * find_prev_context_driver:
 *
 * Finds previous driver in graphics context driver array.
 **/
void find_prev_gfx_context_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i = find_gfx_ctx_driver_index(settings->video.context_driver);
   
   if (i > 0)
   {
      strlcpy(settings->video.context_driver, gfx_ctx_drivers[i - 1]->ident,
            sizeof(settings->video.context_driver));
   }
   else
      RARCH_WARN("Couldn't find any previous video context driver.\n");
}

/**
 * find_next_context_driver:
 *
 * Finds next driver in graphics context driver array.
 **/
void find_next_context_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i = find_gfx_ctx_driver_index(settings->video.context_driver);

   if (i >= 0 && gfx_ctx_drivers[i + 1])
   {
      strlcpy(settings->video.context_driver, gfx_ctx_drivers[i + 1]->ident,
            sizeof(settings->video.context_driver));
   }
   else
      RARCH_WARN("Couldn't find any next video context driver.\n");
}

/**
 * gfx_ctx_init:
 * @data                    : Input data.
 * @ctx                     : Graphics context driver to initialize.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Initialize graphics context driver.
 *
 * Returns: graphics context driver if successfully initialized, otherwise NULL.
 **/
static const gfx_ctx_driver_t *gfx_ctx_init(void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   settings_t *settings = config_get_ptr();

   if (ctx->bind_api(data, api, major, minor))
   {
      void *ctx_data = ctx->init(data);

      if (!ctx_data)
         return NULL;

      if (ctx->bind_hw_render)
         ctx->bind_hw_render(ctx_data,
               settings->video.shared_context && hw_render_ctx);

      video_context_data = ctx_data;
      return ctx;
   }

#ifndef _WIN32
   RARCH_WARN("Failed to bind API (#%u, version %u.%u) on context driver \"%s\".\n",
         (unsigned)api, major, minor, ctx->ident);
#endif

   return NULL;
}

/**
 * gfx_ctx_find_driver:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
static const gfx_ctx_driver_t *gfx_ctx_find_driver(void *data,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   int i = find_gfx_ctx_driver_index(ident);

   if (i >= 0)
      return gfx_ctx_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);

   for (i = 0; gfx_ctx_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx = 
         gfx_ctx_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);

      if (ctx)
         return ctx;
   }

   return NULL;
}

/**
 * gfx_ctx_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *gfx_ctx_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   return gfx_ctx_find_driver(data, ident, api,
         major, minor, hw_render_ctx);
}

bool gfx_ctx_ctl(enum gfx_ctx_ctl_state state, void *data)
{
   switch (state)
   {
      case GFX_CTL_SWAP_BUFFERS:
         if (!current_video_context || !current_video_context->swap_buffers)
            return false;
         current_video_context->swap_buffers(video_context_data);
         break;
      case GFX_CTL_FOCUS:
         if (!video_context_data || !current_video_context->has_focus)
            return false;
         return current_video_context->has_focus(video_context_data);
      case GFX_CTL_FREE:
         if (current_video_context->destroy)
            current_video_context->destroy(video_context_data);
         current_video_context = NULL;
         video_context_data    = NULL;
         break;
      case GFX_CTL_NONE:
      default:
         break;
   }

   return true;
}
