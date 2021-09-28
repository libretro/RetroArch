/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <math.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "video_driver.h"

#include "../ui/ui_companion_driver.h"
#include "../list_special.h"
#include "../retroarch.h"
#include "../verbosity.h"

typedef struct
{
   struct string_list *list;
   enum gfx_ctx_api api;
} gfx_api_gpu_map;

static gfx_api_gpu_map gpu_map[] = {
   { NULL,                   GFX_CTX_VULKAN_API     },
   { NULL,                   GFX_CTX_DIRECT3D10_API },
   { NULL,                   GFX_CTX_DIRECT3D11_API },
   { NULL,                   GFX_CTX_DIRECT3D12_API }
};

video_driver_t *hw_render_context_driver(
      enum retro_hw_context_type type, int major, int minor)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGL_CORE:
#ifdef HAVE_OPENGL_CORE
         return &video_gl_core;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_OPENGL:
#ifdef HAVE_OPENGL
         return &video_gl2;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_DIRECT3D:
#if defined(HAVE_D3D9)
         if (major == 9)
            return &video_d3d9;
#endif
#if defined(HAVE_D3D11)
         if (major == 11)
            return &video_d3d11;
#endif
         break;
      case RETRO_HW_CONTEXT_VULKAN:
#if defined(HAVE_VULKAN)
         return &video_vulkan;
#else
         break;
#endif
      default:
      case RETRO_HW_CONTEXT_NONE:
         break;
   }

   return NULL;
}

const char *hw_render_context_name(
      enum retro_hw_context_type type, int major, int minor)
{
#ifdef HAVE_OPENGL_CORE
   if (type == RETRO_HW_CONTEXT_OPENGL_CORE)
      return "glcore";
#endif
#ifdef HAVE_OPENGL
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      case RETRO_HW_CONTEXT_OPENGL:
#ifndef HAVE_OPENGL_CORE
      case RETRO_HW_CONTEXT_OPENGL_CORE:
#endif
         return "gl";
      default:
         break;
   }
#endif
#ifdef HAVE_VULKAN
   if (type == RETRO_HW_CONTEXT_VULKAN)
      return "vulkan";
#endif
#ifdef HAVE_D3D11
   if (type == RETRO_HW_CONTEXT_DIRECT3D && major == 11)
      return "d3d11";
#endif
#ifdef HAVE_D3D9
   if (type == RETRO_HW_CONTEXT_DIRECT3D && major == 9)
      return "d3d9";
#endif
   return "N/A";
}

enum retro_hw_context_type hw_render_context_type(const char *s)
{
#ifdef HAVE_OPENGL_CORE
   if (string_is_equal(s, "glcore"))
      return RETRO_HW_CONTEXT_OPENGL_CORE;
#endif
#ifdef HAVE_OPENGL
   if (string_is_equal(s, "gl"))
      return RETRO_HW_CONTEXT_OPENGL;
#endif
#ifdef HAVE_VULKAN
   if (string_is_equal(s, "vulkan"))
      return RETRO_HW_CONTEXT_VULKAN;
#endif
#ifdef HAVE_D3D11
   if (string_is_equal(s, "d3d11"))
      return RETRO_HW_CONTEXT_DIRECT3D;
#endif
#ifdef HAVE_D3D11
   if (string_is_equal(s, "d3d9"))
      return RETRO_HW_CONTEXT_DIRECT3D;
#endif
   return RETRO_HW_CONTEXT_NONE;
}

/* string list stays owned by the caller and must be available at
 * all times after the video driver is inited */
void video_driver_set_gpu_api_devices(
      enum gfx_ctx_api api, struct string_list *list)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
      {
         gpu_map[i].list = list;
         break;
      }
   }
}

struct string_list* video_driver_get_gpu_api_devices(enum gfx_ctx_api api)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
         return gpu_map[i].list;
   }

   return NULL;
}


/**
 * video_driver_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool video_driver_translate_coord_viewport(
      struct video_viewport *vp,
      int mouse_x,           int mouse_y,
      int16_t *res_x,        int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y)
{
   int norm_vp_width         = (int)vp->width;
   int norm_vp_height        = (int)vp->height;
   int norm_full_vp_width    = (int)vp->full_width;
   int norm_full_vp_height   = (int)vp->full_height;
   int scaled_screen_x       = -0x8000; /* OOB */
   int scaled_screen_y       = -0x8000; /* OOB */
   int scaled_x              = -0x8000; /* OOB */
   int scaled_y              = -0x8000; /* OOB */
   if (norm_vp_width       <= 0 ||
       norm_vp_height      <= 0 ||
       norm_full_vp_width  <= 0 ||
       norm_full_vp_height <= 0)
      return false;

   if (mouse_x >= 0 && mouse_x <= norm_full_vp_width)
      scaled_screen_x = ((2 * mouse_x * 0x7fff)
            / norm_full_vp_width)  - 0x7fff;

   if (mouse_y >= 0 && mouse_y <= norm_full_vp_height)
      scaled_screen_y = ((2 * mouse_y * 0x7fff)
            / norm_full_vp_height) - 0x7fff;

   mouse_x           -= vp->x;
   mouse_y           -= vp->y;

   if (mouse_x >= 0 && mouse_x <= norm_vp_width)
      scaled_x        = ((2 * mouse_x * 0x7fff)
            / norm_vp_width) - 0x7fff;
   else
      scaled_x        = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_vp_height)
      scaled_y        = ((2 * mouse_y * 0x7fff)
            / norm_vp_height) - 0x7fff;

   *res_x             = scaled_x;
   *res_y             = scaled_y;
   *res_screen_x      = scaled_screen_x;
   *res_screen_y      = scaled_screen_y;
   return true;
}

void video_monitor_compute_fps_statistics(uint64_t
      frame_time_count)
{
   double        avg_fps       = 0.0;
   double        stddev        = 0.0;
   unsigned        samples     = 0;

   if (frame_time_count <
         (2 * MEASURE_FRAME_TIME_SAMPLES_COUNT))
   {
      RARCH_LOG(
            "[Video]: Does not have enough samples for monitor refresh rate"
            " estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   if (video_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("[Video]: Average monitor Hz: %.6f Hz. (%.3f %% frame time"
            " deviation, based on %u last samples).\n",
            avg_fps, 100.0f * stddev, samples);
   }
}

void video_monitor_set_refresh_rate(float hz)
{
   char msg[128];
   settings_t        *settings = config_get_ptr();

   snprintf(msg, sizeof(msg),
         "Setting refresh rate to: %.3f Hz.", hz);
   if (settings->bools.notification_show_refresh_rate)
      runloop_msg_queue_push(msg, 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[Video]: %s\n", msg);

   configuration_set_float(settings,
         settings->floats.video_refresh_rate,
         hz);
}

void video_driver_force_fallback(const char *driver)
{
   settings_t *settings        = config_get_ptr();
   ui_msg_window_t *msg_window = NULL;

   configuration_set_string(settings,
         settings->arrays.video_driver,
         driver);

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
   /* UI companion driver is not inited yet, just call into it directly */
   msg_window = &ui_msg_window_win32;
#endif

   if (msg_window)
   {
      char text[PATH_MAX_LENGTH];
      ui_msg_window_state window_state;
      char *title          = strdup(msg_hash_to_str(MSG_ERROR));

      text[0]              = '\0';

      snprintf(text, sizeof(text),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK),
            driver);

      window_state.buttons = UI_MSG_WINDOW_OK;
      window_state.text    = strdup(text);
      window_state.title   = title;
      window_state.window  = NULL;

      msg_window->error(&window_state);

      free(title);
   }
   exit(1);
}

static bool video_context_driver_get_metrics_null(
      void *data, enum display_metric_types type,
      float *value) { return false; }

void video_context_driver_destroy(gfx_ctx_driver_t *ctx_driver)
{
   if (!ctx_driver)
      return;

   ctx_driver->init                       = NULL;
   ctx_driver->bind_api                   = NULL;
   ctx_driver->swap_interval              = NULL;
   ctx_driver->set_video_mode             = NULL;
   ctx_driver->get_video_size             = NULL;
   ctx_driver->get_video_output_size      = NULL;
   ctx_driver->get_video_output_prev      = NULL;
   ctx_driver->get_video_output_next      = NULL;
   ctx_driver->get_metrics                = 
      video_context_driver_get_metrics_null;
   ctx_driver->translate_aspect           = NULL;
   ctx_driver->update_window_title        = NULL;
   ctx_driver->check_window               = NULL;
   ctx_driver->set_resize                 = NULL;
   ctx_driver->suppress_screensaver       = NULL;
   ctx_driver->swap_buffers               = NULL;
   ctx_driver->input_driver               = NULL;
   ctx_driver->get_proc_address           = NULL;
   ctx_driver->image_buffer_init          = NULL;
   ctx_driver->image_buffer_write         = NULL;
   ctx_driver->show_mouse                 = NULL;
   ctx_driver->ident                      = NULL;
   ctx_driver->get_flags                  = NULL;
   ctx_driver->set_flags                  = NULL;
   ctx_driver->bind_hw_render             = NULL;
   ctx_driver->get_context_data           = NULL;
   ctx_driver->make_current               = NULL;
}

const gfx_ctx_driver_t *video_context_driver_init(
      bool core_set_shared_context,
      settings_t *settings,
      void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx,
      void **ctx_data)
{
   if (!ctx->bind_api(data, api, major, minor))
   {
      RARCH_WARN("Failed to bind API (#%u, version %u.%u)"
            " on context driver \"%s\".\n",
            (unsigned)api, major, minor, ctx->ident);

      return NULL;
   }

   if (!(*ctx_data = ctx->init(data)))
      return NULL;

   if (ctx->bind_hw_render)
   {
      bool  video_shared_context  =
         settings->bools.video_shared_context || core_set_shared_context;

      ctx->bind_hw_render(*ctx_data,
            video_shared_context && hw_render_ctx);
   }

   return ctx;
}

/**
 * config_get_video_driver_options:
 *
 * Get an enumerated list of all video driver names, separated by '|'.
 *
 * Returns: string listing of all video driver names, separated by '|'.
 **/
const char* config_get_video_driver_options(void)
{
   return char_list_new_special(STRING_LIST_VIDEO_DRIVERS, NULL);
}

void video_driver_pixel_converter_free(
      video_pixel_scaler_t *scalr)
{
   if (!scalr)
      return;

   if (scalr->scaler)
   {
      scaler_ctx_gen_reset(scalr->scaler);
      free(scalr->scaler);
   }
   if (scalr->scaler_out)
      free(scalr->scaler_out);

   scalr->scaler     = NULL;
   scalr->scaler_out = NULL;

   free(scalr);
}

video_pixel_scaler_t *video_driver_pixel_converter_init(
      const enum retro_pixel_format video_driver_pix_fmt,
      struct retro_hw_render_callback *hwr,
      unsigned size)
{
   void *scalr_out                      = NULL;
   video_pixel_scaler_t          *scalr = NULL;
   struct scaler_ctx        *scalr_ctx  = NULL;

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_pix_fmt != RETRO_PIXEL_FORMAT_0RGB1555)
      return NULL;

   /* No need to perform pixel conversion for HW rendering contexts. */
   if (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      return NULL;

   RARCH_WARN("[Video]: 0RGB1555 pixel format is deprecated,"
         " and will be slower. For 15/16-bit, RGB565"
         " format is preferred.\n");

   if (!(scalr = (video_pixel_scaler_t*)malloc(sizeof(*scalr))))
      goto error;

   scalr->scaler                            = NULL;
   scalr->scaler_out                        = NULL;

   if (!(scalr_ctx = (struct scaler_ctx*)calloc(1, sizeof(*scalr_ctx))))
      goto error;

   scalr->scaler                            = scalr_ctx;
   scalr->scaler->scaler_type               = SCALER_TYPE_POINT;
   scalr->scaler->in_fmt                    = SCALER_FMT_0RGB1555;
   /* TODO/FIXME: Pick either ARGB8888 or RGB565 depending on driver. */
   scalr->scaler->out_fmt                   = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(scalr_ctx))
      goto error;

   if (!(scalr_out = calloc(sizeof(uint16_t), size * size)))
      goto error;

   scalr->scaler_out                        = scalr_out;

   return scalr;

error:
   video_driver_pixel_converter_free(scalr);
#ifdef HAVE_VIDEO_FILTER
   video_driver_filter_free();
#endif
   return NULL;
}

struct video_viewport *video_viewport_get_custom(void)
{
   return &config_get_ptr()->video_viewport_custom;
}

bool video_driver_monitor_adjust_system_rates(
      float timing_skew_hz,
      float video_refresh_rate,
      bool vrr_runloop_enable,
      float audio_max_timing_skew,
      double input_fps)
{
   if (!vrr_runloop_enable)
   {
      float timing_skew                    = fabs(
            1.0f - input_fps / timing_skew_hz);
      /* We don't want to adjust pitch too much. If we have extreme cases,
       * just don't readjust at all. */
      if (timing_skew <= audio_max_timing_skew)
         return true;
      RARCH_LOG("[Video]: Timings deviate too much. Will not adjust."
            " (Display = %.2f Hz, Game = %.2f Hz)\n",
            video_refresh_rate,
            (float)input_fps);
   }
   return input_fps <= timing_skew_hz;
}

void video_driver_reset_custom_viewport(settings_t *settings)
{
   struct video_viewport *custom_vp = &settings->video_viewport_custom;

   custom_vp->width  = 0;
   custom_vp->height = 0;
   custom_vp->x      = 0;
   custom_vp->y      = 0;
}
