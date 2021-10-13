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
#include <retro_math.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "video_driver.h"
#include "video_filter.h"
#include "video_display_server.h"

#ifdef HAVE_THREADS
#include "video_thread_wrapper.h"
#endif

#include "../frontend/frontend_driver.h"
#include "../ui/ui_companion_driver.h"
#include "../file_path_special.h"
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

static const video_display_server_t dispserv_null = {
   NULL, /* init */
   NULL, /* destroy */
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   NULL, /* get_flags */
   "null"
};

static void *video_null_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   *input       = NULL;
   *input_data = NULL;

   frontend_driver_install_signal_handler();

   return (void*)-1;
}

static bool video_null_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   return true;
}

static void video_null_free(void *data) { }
static void video_null_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool video_null_alive(void *data) { return frontend_driver_get_signal_handler_state() != 1; }
static bool video_null_focus(void *data) { return true; }
static bool video_null_has_windowed(void *data) { return true; }
static bool video_null_suppress_screensaver(void *data, bool enable) { return false; }
static bool video_null_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

video_driver_t video_null = {
   video_null_init,
   video_null_frame,
   video_null_set_nonblock_state,
   video_null_alive,
   video_null_focus,
   video_null_suppress_screensaver,
   video_null_has_windowed,
   video_null_set_shader,
   video_null_free,
   "null",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
  NULL, /* get_poke_interface */
};

const video_driver_t *video_drivers[] = {
#ifdef __PSL1GHT__
   &video_gcm,
#endif
#ifdef HAVE_VITA2D
   &video_vita2d,
#endif
#ifdef HAVE_OPENGL
   &video_gl2,
#endif
#if defined(HAVE_OPENGL_CORE)
   &video_gl_core,
#endif
#ifdef HAVE_OPENGL1
   &video_gl1,
#endif
#ifdef HAVE_VULKAN
   &video_vulkan,
#endif
#ifdef HAVE_METAL
   &video_metal,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(HAVE_D3D12)
   &video_d3d12,
#endif
#if defined(HAVE_D3D11)
   &video_d3d11,
#endif
#if defined(HAVE_D3D10)
   &video_d3d10,
#endif
#if defined(HAVE_D3D9)
   &video_d3d9,
#endif
#if defined(HAVE_D3D8)
   &video_d3d8,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef PS2
   &video_ps2,
#endif
#ifdef _3DS
   &video_ctr,
#endif
#ifdef SWITCH
   &video_switch,
#endif
#ifdef HAVE_ODROIDGO2
   &video_oga,
#endif
#if defined(HAVE_SDL) && !defined(HAVE_SDL_DINGUX)
   &video_sdl,
#endif
#ifdef HAVE_SDL2
   &video_sdl2,
#endif
#ifdef HAVE_SDL_DINGUX
#if defined(RS90) || defined(MIYOO)
   &video_sdl_rs90,
#else
   &video_sdl_dingux,
#endif
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef WIIU
   &video_wiiu,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
#ifdef HAVE_EXYNOS
   &video_exynos,
#endif
#ifdef HAVE_DISPMANX
   &video_dispmanx,
#endif
#ifdef HAVE_SUNXI
   &video_sunxi,
#endif
#ifdef HAVE_PLAIN_DRM
   &video_drm,
#endif
#ifdef HAVE_XSHM
   &video_xshm,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &video_gdi,
#endif
#endif
#ifdef DJGPP
   &video_vga,
#endif
#ifdef HAVE_FPGA
   &video_fpga,
#endif
#ifdef HAVE_SIXEL
   &video_sixel,
#endif
#ifdef HAVE_CACA
   &video_caca,
#endif
#ifdef HAVE_NETWORK_VIDEO
   &video_network,
#endif
   &video_null,
   NULL,
};

static video_driver_state_t video_driver_st = { 0 };
static const video_display_server_t *current_display_server = 
&dispserv_null;

struct retro_hw_render_callback *video_driver_get_hw_context(void)
{
   video_driver_state_t *video_st         = &video_driver_st;
   return VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);
}

video_driver_state_t *video_state_get_ptr(void)
{
   return &video_driver_st;
}

#ifdef HAVE_THREADS
void *video_thread_get_ptr(video_driver_state_t *video_st)
{
   const thread_video_t *thr   = (const thread_video_t*)video_st->data;
   if (thr)
      return thr->driver_data;
   return NULL;
}
#endif

/**
 * video_driver_get_ptr:
 *
 * Use this if you need the real video driver
 * and driver data pointers.
 *
 * Returns: video driver's userdata.
 **/
void *video_driver_get_ptr(void)
{
   video_driver_state_t *video_st         = &video_driver_st;
   return VIDEO_DRIVER_GET_PTR_INTERNAL(video_st);
}

void *video_driver_get_data(void)
{
   return video_driver_st.data;
}

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

static bool get_metrics_null(void *data, enum display_metric_types type,
      float *value) { return false; }

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

struct retro_system_av_info *video_viewport_get_system_av_info(void)
{
   return &video_driver_st.av_info;
}

void video_driver_gpu_record_deinit(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (video_st->record_gpu_buffer)
      free(video_st->record_gpu_buffer);
   video_st->record_gpu_buffer = NULL;
}

void recording_dump_frame(
      const void *data, unsigned width,
      unsigned height, size_t pitch, bool is_idle)
{
   struct record_video_data ffemu_data;
   video_driver_state_t 
	   *video_st   = &video_driver_st;
   recording_state_t 
      *record_st       = recording_state_get_ptr();

   ffemu_data.data     = data;
   ffemu_data.width    = width;
   ffemu_data.height   = height;
   ffemu_data.pitch    = (int)pitch;
   ffemu_data.is_dupe  = false;

   if (video_st->record_gpu_buffer)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_WARN("[recording] %s \n",
               msg_hash_to_str(MSG_VIEWPORT_SIZE_CALCULATION_FAILED));
         video_driver_gpu_record_deinit();
         recording_dump_frame(
               data, width, height, pitch, is_idle);
         return;
      }

      /* User has resized. We kinda have a problem now. */
      if (  vp.width  != record_st->gpu_width ||
            vp.height != record_st->gpu_height)
      {
         RARCH_WARN("[recording] %s\n",
               msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE));

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         return;
      }

      /* Big bottleneck.
       * Since we might need to do read-backs asynchronously,
       * it might take 3-4 times before this returns true. */
      if (!video_driver_read_viewport(video_st->record_gpu_buffer, is_idle))
         return;

      ffemu_data.pitch  = (int)(record_st->gpu_width * 3);
      ffemu_data.width  = (unsigned)record_st->gpu_width;
      ffemu_data.height = (unsigned)record_st->gpu_height;
      ffemu_data.data   = video_st->record_gpu_buffer + (ffemu_data.height - 1) * ffemu_data.pitch;

      ffemu_data.pitch  = -ffemu_data.pitch;
   }
   else
      ffemu_data.is_dupe = !data;

   record_st->driver->push_video(record_st->data, &ffemu_data);
}

const char *video_display_server_get_ident(void)
{
   if (!current_display_server)
      return FILE_PATH_UNKNOWN;
   return current_display_server->ident;
}

void* video_display_server_init(enum rarch_display_type type)
{
   video_driver_state_t *video_st = &video_driver_st;
   video_display_server_destroy();

   switch (type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
         current_display_server = &dispserv_win32;
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         current_display_server = &dispserv_x11;
#endif
         break;
      default:
#if defined(ANDROID)
         current_display_server = &dispserv_android;
#else
         current_display_server = &dispserv_null;
#endif
         break;
   }

   if (current_display_server)
   {
      if (current_display_server->init)
         video_st->current_display_server_data = current_display_server->init();

      if (!string_is_empty(current_display_server->ident))
      {
         RARCH_LOG("[Video]: Found display server: %s\n",
               current_display_server->ident);
      }
   }

   video_st->initial_screen_orientation =
      video_display_server_get_screen_orientation();
   video_st->current_screen_orientation =
      video_st->initial_screen_orientation;

   return video_st->current_display_server_data;
}

void video_display_server_destroy(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   const enum rotation initial_screen_orientation = video_st->initial_screen_orientation;
   const enum rotation current_screen_orientation = video_st->current_screen_orientation;

   if (initial_screen_orientation != current_screen_orientation)
      video_display_server_set_screen_orientation(initial_screen_orientation);

   if (current_display_server)
      if (video_st->current_display_server_data)
         current_display_server->destroy(video_st->current_display_server_data);
}

bool video_display_server_set_window_opacity(unsigned opacity)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_window_opacity)
      return current_display_server->set_window_opacity(
            video_st->current_display_server_data, opacity);
   return false;
}

bool video_display_server_set_window_progress(int progress, bool finished)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_window_progress)
      return current_display_server->set_window_progress(
            video_st->current_display_server_data, progress, finished);
   return false;
}

bool video_display_server_set_window_decorations(bool on)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_window_decorations)
      return current_display_server->set_window_decorations(
            video_st->current_display_server_data, on);
   return false;
}

bool video_display_server_set_resolution(unsigned width, unsigned height,
      int int_hz, float hz, int center, int monitor_index, int xoffset, int padjust)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_resolution)
      return current_display_server->set_resolution(
            video_st->current_display_server_data, width, height, int_hz,
            hz, center, monitor_index, xoffset, padjust);
   return false;
}

bool video_display_server_has_resolution_list(void)
{
   return (current_display_server
         && current_display_server->get_resolution_list);
}

void *video_display_server_get_resolution_list(unsigned *size)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (video_display_server_has_resolution_list())
      return current_display_server->get_resolution_list(
            video_st->current_display_server_data, size);
   return NULL;
}

bool video_display_server_has_refresh_rate(float hz)
{
   unsigned i, size            = 0;
   bool rate_exists            = false;

   struct video_display_config *video_list = (struct video_display_config*)
         video_display_server_get_resolution_list(&size);

   if (video_list)
   {
   video_driver_state_t *video_st    = &video_driver_st;
      unsigned video_driver_width    = video_st->width;
      unsigned video_driver_height   = video_st->height;

      for (i = 0; i < size && !rate_exists; i++)
      {
         if (video_list[i].width       == video_driver_width &&
             video_list[i].height      == video_driver_height &&
             video_list[i].refreshrate == floor(hz))
            rate_exists = true;
      }

      free(video_list);
   }

   return rate_exists;
}

bool video_display_server_set_refresh_rate(float hz)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_resolution)
      return current_display_server->set_resolution(
            video_st->current_display_server_data, 0, 0, (int)hz,
            hz, 0, 0, 0, 0);
   return false;
}

void video_display_server_restore_refresh_rate(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   settings_t *settings        = config_get_ptr();
   float refresh_rate_original = video_st->video_refresh_rate_original;
   float refresh_rate_current  = settings->floats.video_refresh_rate;

   if (!refresh_rate_original || refresh_rate_current == refresh_rate_original)
      return;

   video_monitor_set_refresh_rate(refresh_rate_original);
   video_display_server_set_refresh_rate(refresh_rate_original);
}

const char *video_display_server_get_output_options(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->get_output_options)
      return current_display_server->get_output_options(
            video_st->current_display_server_data);
   return NULL;
}

void video_display_server_set_screen_orientation(enum rotation rotation)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_screen_orientation)
   {
      RARCH_LOG("[Video]: Setting screen orientation to %d.\n", rotation);
      video_st->current_screen_orientation    = rotation;
      current_display_server->set_screen_orientation(video_st->current_display_server_data, rotation);
   }
}

bool video_display_server_can_set_screen_orientation(void)
{
   return (current_display_server && current_display_server->set_screen_orientation);
}

enum rotation video_display_server_get_screen_orientation(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->get_screen_orientation)
      return current_display_server->get_screen_orientation(
            video_st->current_display_server_data);
   return ORIENTATION_NORMAL;
}

bool video_display_server_get_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!flags || !current_display_server || !current_display_server->get_flags)
      return false;
   flags->flags = current_display_server->get_flags(
         video_st->current_display_server_data);
   return true;
}

bool video_driver_started_fullscreen(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   return video_st->started_fullscreen;
}

bool video_driver_is_threaded(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   return VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
}

bool *video_driver_get_threaded(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
#if defined(__MACH__) && defined(__APPLE__)
   /* TODO/FIXME - force threaded video to disabled on Apple for now
    * until NSWindow/UIWindow concurrency issues are taken care of */
   video_st->threaded = false;
#endif
   return &video_st->threaded;
}

void video_driver_set_threaded(bool val)
{
   video_driver_state_t *video_st                 = &video_driver_st;
#if defined(__MACH__) && defined(__APPLE__)
   /* TODO/FIXME - force threaded video to disabled on Apple for now
    * until NSWindow/UIWindow concurrency issues are taken care of */
   video_st->threaded = false;
#else
   video_st->threaded = val;
#endif
}

const char *video_driver_get_ident(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!video_st->current_video)
      return NULL;
#ifdef HAVE_THREADS
   if (VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st))
   {
      const thread_video_t *thr   = (const thread_video_t*)video_st->data;
      if (!thr || !thr->driver)
         return NULL;
      return thr->driver->ident;
   }
#endif

   return video_st->current_video->ident;
}

void video_context_driver_reset(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!video_st->current_video_context.get_metrics)
      video_st->current_video_context.get_metrics = get_metrics_null;
}

bool video_context_driver_set(const gfx_ctx_driver_t *data)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!data)
      return false;
   video_st->current_video_context = *data;
   video_context_driver_reset();
   return true;
}

uintptr_t video_driver_get_current_framebuffer(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (     video_st->poke
         && video_st->poke->get_current_framebuffer)
      return video_st->poke->get_current_framebuffer(video_st->data);
   return 0;
}

retro_proc_address_t video_driver_get_proc_address(const char *sym)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (     video_st->poke
         && video_st->poke->get_proc_address)
      return video_st->poke->get_proc_address(video_st->data, sym);
   return NULL;
}

#ifdef HAVE_VIDEO_FILTER
void video_driver_filter_free(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (video_st->state_filter)
      rarch_softfilter_free(video_st->state_filter);
   video_st->state_filter   = NULL;

   if (video_st->state_buffer)
   {
#ifdef _3DS
      linearFree(video_st->state_buffer);
#else
      free(video_st->state_buffer);
#endif
   }
   video_st->state_buffer    = NULL;

   video_st->state_scale     = 0;
   video_st->state_out_bpp   = 0;
   video_st->state_out_rgb32 = false;
}

void video_driver_init_filter(enum retro_pixel_format colfmt_int,
      settings_t *settings)
{
   unsigned pow2_x, pow2_y, maxsize;
   void *buf                            = NULL;
   video_driver_state_t *video_st       = &video_driver_st;
   struct retro_game_geometry *geom     = &video_st->av_info.geometry;
   unsigned width                       = geom->max_width;
   unsigned height                      = geom->max_height;
   /* Deprecated format. Gets pre-converted. */
   enum retro_pixel_format colfmt       =
      (colfmt_int == RETRO_PIXEL_FORMAT_0RGB1555) ?
      RETRO_PIXEL_FORMAT_RGB565 : colfmt_int;

   if (video_driver_is_hw_context())
   {
      RARCH_WARN("[Video]: Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   video_st->state_filter   = rarch_softfilter_new(
         settings->paths.path_softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!video_st->state_filter)
   {
      RARCH_ERR("[Video]: Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(
         video_st->state_filter,
         &width, &height);

   pow2_x                              = next_pow2(width);
   pow2_y                              = next_pow2(height);
   maxsize                             = MAX(pow2_x, pow2_y);

#ifdef _3DS
   /* On 3DS, video is disabled if the output resolution
    * exceeds 2048x2048. To avoid the user being presented
    * with a black screen, we therefore have to check that
    * the filter upscaling buffer fits within this limit. */
   if (maxsize >= 2048)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed."
            " Upscaling buffer exceeds hardware limitations.\n");
      video_driver_filter_free();
      return;
   }
#endif

   video_st->state_scale     = maxsize / RARCH_SCALE_BASE;
   video_st->state_out_rgb32 = rarch_softfilter_get_output_format(
         video_st->state_filter) == RETRO_PIXEL_FORMAT_XRGB8888;

   video_st->state_out_bpp   =
      video_st->state_out_rgb32
      ? sizeof(uint32_t)
      : sizeof(uint16_t);

   /* TODO: Aligned output. */
#ifdef _3DS
   buf = linearMemAlign(
         width * height * video_st->state_out_bpp, 0x80);
#else
   buf = malloc(
         width * height * video_st->state_out_bpp);
#endif
   if (!buf)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed.\n");
      video_driver_filter_free();
      return;
   }

   video_st->state_buffer    = buf;
}
#endif

void video_driver_free_hw_context(void)
{
   video_driver_state_t *video_st       = &video_driver_st;
   VIDEO_DRIVER_CONTEXT_LOCK(video_st);

   if (video_st->hw_render.context_destroy)
      video_st->hw_render.context_destroy();

   memset(&video_st->hw_render, 0, sizeof(video_st->hw_render));

   VIDEO_DRIVER_CONTEXT_UNLOCK(video_st);

   video_st->hw_render_context_negotiation = NULL;
}

void video_driver_free_internal(void)
{
   input_driver_state_t *input_st    = input_state_get_ptr();
   video_driver_state_t *video_st    = &video_driver_st;
#ifdef HAVE_THREADS
   bool        is_threaded           =
VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
#endif

#ifdef HAVE_VIDEO_LAYOUT
   video_layout_deinit();
#endif

   command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);

   if (!video_driver_is_video_cache_context())
      video_driver_free_hw_context();

   if (!(input_st->current_data == video_st->data))
   {
      if (input_st->current_driver)
         if (input_st->current_driver->free)
            input_st->current_driver->free(input_st->current_data);
      if (input_st->primary_joypad)
      {
         const input_device_driver_t *tmp   = input_st->primary_joypad;
         input_st->primary_joypad    = NULL;
         tmp->destroy();
      }
#ifdef HAVE_MFI
      if (input_st->secondary_joypad)
      {
         const input_device_driver_t *tmp   = input_st->secondary_joypad;
         input_st->secondary_joypad         = NULL;
         tmp->destroy();
      }
#endif
      input_st->keyboard_mapping_blocked    = false;
      input_st->current_data                = NULL;
   }

   if (     video_st->data
         && video_st->current_video
         && video_st->current_video->free)
      video_st->current_video->free(video_st->data);

   if (video_st->scaler_ptr)
      video_driver_pixel_converter_free(video_st->scaler_ptr);
   video_st->scaler_ptr = NULL;
#ifdef HAVE_VIDEO_FILTER
   video_driver_filter_free();
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   dir_free_shader(
         (struct rarch_dir_shader_list*)&video_st->dir_shader_list,
         config_get_ptr()->bools.video_shader_remember_last_dir);
#endif
#ifdef HAVE_THREADS
   if (is_threaded)
      return;
#endif

   video_monitor_compute_fps_statistics(video_st->frame_time_count);
}
