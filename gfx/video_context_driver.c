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
#if defined(HAVE_OPENDINGUX_FBDEV)
   &gfx_ctx_opendingux_fbdev,
#endif
#if defined(_WIN32) && defined(HAVE_OPENGL)
   &gfx_ctx_wgl,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && !defined(HAVE_OPENGLES)
#if defined(HAVE_OPENGL) || defined(HAVE_VULKAN)
   &gfx_ctx_x,
#endif
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
   &gfx_ctx_drm,
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
static bool gfx_ctl_find_prev_driver(void)
{
   settings_t *settings = config_get_ptr();
   int                i = find_gfx_ctx_driver_index(
         settings->video.context_driver);
   
   if (i > 0)
   {
      strlcpy(settings->video.context_driver,
            gfx_ctx_drivers[i - 1]->ident,
            sizeof(settings->video.context_driver));
      return true;
   }

   RARCH_WARN("Couldn't find any previous video context driver.\n");
   return false;
}

/**
 * find_next_context_driver:
 *
 * Finds next driver in graphics context driver array.
 **/
static bool gfx_ctl_find_next_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i = find_gfx_ctx_driver_index(settings->video.context_driver);

   if (i >= 0 && gfx_ctx_drivers[i + 1])
   {
      strlcpy(settings->video.context_driver,
            gfx_ctx_drivers[i + 1]->ident,
            sizeof(settings->video.context_driver));
      return true;
   }

   RARCH_WARN("Couldn't find any next video context driver.\n");
   return false;
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

      gfx_ctx_ctl(GFX_CTL_SET_VIDEO_CONTEXT_DATA, ctx_data);
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
   static const gfx_ctx_driver_t *current_video_context = NULL;
   static void *video_context_data                      = NULL;

   switch (state)
   {
      case GFX_CTL_CHECK_WINDOW:
         {
            uint64_t       *frame_count = NULL;
            gfx_ctx_size_t *size_data   = (gfx_ctx_size_t*)data;

            video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

            if (!video_context_data || !size_data)
               return false;
            if (!current_video_context || !current_video_context->check_window)
               return false;

            current_video_context->check_window(video_context_data,
                  size_data->quit,
                  size_data->resize,
                  size_data->width,
                  size_data->height, (unsigned int)*frame_count);
         }
         break;
      case GFX_CTL_FIND_PREV_DRIVER:
         if (!gfx_ctl_find_prev_driver())
            return false;
         break;
      case GFX_CTL_FIND_NEXT_DRIVER:
         if (!gfx_ctl_find_next_driver())
            return false;
         break;
      case GFX_CTL_IMAGE_BUFFER_INIT:
         if (!current_video_context || !current_video_context->image_buffer_init)
            return false;
         if (!current_video_context->image_buffer_init(video_context_data,
               (const video_info_t*)data))
            return false;
         break;
      case GFX_CTL_IMAGE_BUFFER_WRITE:
         {
            gfx_ctx_image_t *img = (gfx_ctx_image_t*)data;

            if (!current_video_context || !current_video_context->image_buffer_write)
               return false;
            if (!current_video_context->image_buffer_write(video_context_data,
                  img->frame, img->width, img->height, img->pitch,
                  img->rgb32, img->index, img->handle))
               return false;
         }
         break;
      case GFX_CTL_GET_VIDEO_OUTPUT_PREV:
         if (!current_video_context 
               || !current_video_context->get_video_output_prev)
            return false;
         current_video_context->get_video_output_prev(video_context_data);
         break;
      case GFX_CTL_GET_VIDEO_OUTPUT_NEXT:
         if (!current_video_context || 
               !current_video_context->get_video_output_next)
            return false;
         current_video_context->get_video_output_next(video_context_data);
         break;
      case GFX_CTL_BIND_HW_RENDER:
         {
            bool *enable = (bool*)data;
            if (!current_video_context || !current_video_context->bind_hw_render)
               return false;
            current_video_context->bind_hw_render(video_context_data, *enable);
         }
         break;
      case GFX_CTL_SET:
         if (!data)
            return false;
         current_video_context = (const gfx_ctx_driver_t*)data;
         break;
      case GFX_CTL_DESTROY:
         current_video_context = NULL;
         break;
      case GFX_CTL_UPDATE_WINDOW_TITLE:
         if (!current_video_context || !current_video_context->update_window_title)
            return false;
         current_video_context->update_window_title(video_context_data);
         break;
      case GFX_CTL_SWAP_BUFFERS:
         if (!current_video_context || !current_video_context->swap_buffers)
            return false;
         current_video_context->swap_buffers(video_context_data);
         break;
      case GFX_CTL_FOCUS:
         if (!video_context_data || !current_video_context->has_focus)
            return false;
         if (!current_video_context->has_focus(video_context_data))
            return false;
         break;
      case GFX_CTL_HAS_WINDOWED:
         if (!video_context_data)
            return false;
         if (!current_video_context->has_windowed(video_context_data))
            return false;
         break;
      case GFX_CTL_FREE:
         if (current_video_context->destroy)
            current_video_context->destroy(video_context_data);
         current_video_context = NULL;
         video_context_data    = NULL;
         break;
      case GFX_CTL_GET_VIDEO_OUTPUT_SIZE:
         {
            gfx_ctx_size_t *size_data = (gfx_ctx_size_t*)data;
            if (!size_data)
               return false;
            if (!current_video_context || !current_video_context->get_video_output_size)
               return false;
            current_video_context->get_video_output_size(video_context_data,
                  size_data->width, size_data->height);
         }
         break;
      case GFX_CTL_SWAP_INTERVAL:
         {
            unsigned *interval = (unsigned*)data;
            if (!current_video_context || !current_video_context->swap_interval)
               return false;
            current_video_context->swap_interval(video_context_data, *interval);
         }
         break;
      case GFX_CTL_PROC_ADDRESS_GET:
         {
            gfx_ctx_proc_address_t *proc = (gfx_ctx_proc_address_t*)data;
            if (!current_video_context || !current_video_context->get_proc_address)
               return false;

            proc->addr = current_video_context->get_proc_address(proc->sym);
         }
         break;
      case GFX_CTL_GET_METRICS:
         {
            gfx_ctx_metrics_t *metrics = (gfx_ctx_metrics_t*)data;
            if (!current_video_context || !current_video_context->get_metrics)
               return false;
            if (!current_video_context->get_metrics(video_context_data,
                  metrics->type,
                  metrics->value))
               return false;
         }
         break;
      case GFX_CTL_INPUT_DRIVER:
         {
            gfx_ctx_input_t *inp = (gfx_ctx_input_t*)data;
            if (!current_video_context || !current_video_context->input_driver)
               return false;
            current_video_context->input_driver(
                  video_context_data, inp->input, inp->input_data);
         }
         break;
      case GFX_CTL_SUPPRESS_SCREENSAVER:
         {
            bool *bool_data = (bool*)data;
            if (!video_context_data || !current_video_context)
               return false;
            if (!current_video_context->suppress_screensaver(
                  video_context_data, *bool_data))
               return false;
         }
         break;
      case GFX_CTL_IDENT_GET:
         {
            gfx_ctx_ident_t *ident = (gfx_ctx_ident_t*)data;
            ident->ident = NULL;
            if (current_video_context)
               ident->ident = current_video_context->ident;
         }
         break;
      case GFX_CTL_SET_VIDEO_MODE:
         {
            gfx_ctx_mode_t *mode_info = (gfx_ctx_mode_t*)data;
            if (!current_video_context || !current_video_context->set_video_mode)
               return false;
            if (!current_video_context->set_video_mode(
                  video_context_data, mode_info->width,
                  mode_info->height, mode_info->fullscreen))
               return false;
         }
         break;
      case GFX_CTL_SET_RESIZE:
         {
            gfx_ctx_mode_t *mode_info = (gfx_ctx_mode_t*)data;
            if (!current_video_context)
               return false;
            if (!current_video_context->set_resize(
                  video_context_data, mode_info->width, mode_info->height))
               return false;
         }
         break;
      case GFX_CTL_GET_VIDEO_SIZE:
         {
            gfx_ctx_mode_t *mode_info = (gfx_ctx_mode_t*)data;
            if (!current_video_context || !current_video_context->get_video_size)
               return false;
            current_video_context->get_video_size(video_context_data, &mode_info->width, &mode_info->height);
         }
         break;
      case GFX_CTL_GET_CONTEXT_DATA:
         {
            if (!current_video_context || !current_video_context->get_context_data)
               return false;
            *(void**)data = current_video_context->get_context_data(video_context_data);
         }
         break;
      case GFX_CTL_SHOW_MOUSE:
         {
            bool *bool_data = (bool*)data;
            if (!current_video_context || !current_video_context->show_mouse)
               return false;
            current_video_context->show_mouse(video_context_data, *bool_data);
         }
         break;
      case GFX_CTL_SET_VIDEO_CONTEXT_DATA:
         video_context_data = data;
         break;
      case GFX_CTL_GET_FLAGS:
         {
            gfx_ctx_flags_t *flags = (gfx_ctx_flags_t*)data;
            if (!flags)
               return false;
            if (!current_video_context || !current_video_context->get_flags)
               return false;
            flags->flags = current_video_context->get_flags(video_context_data);
         }
         break;
      case GFX_CTL_NONE:
      default:
         break;
   }

   return true;
}
