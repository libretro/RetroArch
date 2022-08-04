/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andr�s Su�rez
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
#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <retro_math.h>

#include "../configuration.h"
#include "../list_special.h"
#include "../gfx/video_driver.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"

#include "record_driver.h"

static recording_state_t recording_state = {0};

static const record_driver_t record_null = {
   NULL, /* new */
   NULL, /* free */
   NULL, /* push_video */
   NULL, /* push_audio */
   NULL, /* finalize */
   "null",
};

const record_driver_t *record_drivers[] = {
#ifdef HAVE_FFMPEG
   &record_ffmpeg,
#endif
   &record_null,
   NULL,
};

recording_state_t *recording_state_get_ptr(void)
{
   return &recording_state;
}

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * @return string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void)
{
   return char_list_new_special(STRING_LIST_RECORD_DRIVERS, NULL);
}

#if 0
/* TODO/FIXME - not used apparently */
static void find_record_driver(const char *prefix,
      bool verbosity_enabled)
{
   settings_t *settings = config_get_ptr();
   int i                = (int)driver_find_index(
         "record_driver",
         settings->arrays.record_driver);

   if (i >= 0)
      recording_state.driver = (const record_driver_t*)record_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;

         RARCH_ERR("[Recording]: Couldn't find any %s named \"%s\".\n", prefix,
               settings->arrays.record_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; record_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", record_drivers[d].ident);
         RARCH_WARN("[Recording]: Going to default to first %s...\n", prefix);
      }

      recording_state.driver = (const record_driver_t*)record_drivers[0];

      if (!recording_state.driver)
         retroarch_fail(1, "find_record_driver()");
   }
}

/**
 * ffemu_find_backend:
 * @ident                   : Identifier of driver to find.
 *
 * Finds a recording driver with the name @ident.
 *
 * Returns: recording driver handle if successful, otherwise
 * NULL.
 **/
static const record_driver_t *ffemu_find_backend(const char *ident)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      if (string_is_equal(record_drivers[i]->ident, ident))
         return record_drivers[i];
   }

   return NULL;
}

static void recording_driver_free_state(void)
{
   /* TODO/FIXME - this is not being called anywhere */
   recording_state.gpu_width     = 0;
   recording_state.gpu_height    = 0;
   recording_state.width         = 0;
   recording_stte.height         = 0;
}
#endif

/**
 * gfx_ctx_init_first:
 * @param backend
 * Recording backend handle.
 * @param data
 * Recording data handle.
 * @param params
 * Recording info parameters.
 *
 * Finds first suitable recording context driver and initializes.
 *
 * @return true if successful, otherwise false.
 **/
static bool record_driver_init_first(
      const record_driver_t **backend, void **data,
      const struct record_params *params)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      void *handle = record_drivers[i]->init(params);

      if (!handle)
         continue;

      *backend = record_drivers[i];
      *data    = handle;
      return true;
   }

   return false;
}

bool recording_deinit(void)
{
   recording_state_t *recording_st = &recording_state;
   if (     !recording_st->data 
		   || !recording_st->driver)
      return false;

   if (recording_st->driver->finalize)
      recording_st->driver->finalize(recording_st->data);

   if (recording_st->driver->free)
      recording_st->driver->free(recording_st->data);

   recording_st->data              = NULL;
   recording_st->driver            = NULL;

   video_driver_gpu_record_deinit();

   return true;
}

void streaming_set_state(bool state)
{
   recording_state_t *recording_st = &recording_state;
   recording_st->streaming_enable  = state;
}

bool recording_init(void)
{
   char output[PATH_MAX_LENGTH];
   char buf[PATH_MAX_LENGTH];
   struct record_params params          = {0};
   settings_t *settings                 = config_get_ptr();
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   runloop_state_t *runloop_st          = runloop_state_get_ptr();
   bool video_gpu_record                = settings->bools.video_gpu_record;
   bool video_force_aspect              = settings->bools.video_force_aspect;
   const enum rarch_core_type
      current_core_type                 = runloop_st->current_core_type;
   const enum retro_pixel_format
      video_driver_pix_fmt              = video_st->pix_fmt;
   recording_state_t *recording_st      = &recording_state;
   bool recording_enable                = recording_st->enable;

   if (!recording_enable)
      return false;

   output[0] = '\0';

   if (current_core_type == CORE_TYPE_DUMMY)
   {
      RARCH_WARN("[Recording]: %s\n",
            msg_hash_to_str(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED));
      return false;
   }

   if (!video_gpu_record && video_driver_is_hw_context())
   {
      RARCH_WARN("[Recording]: %s.\n",
            msg_hash_to_str(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING));
      return false;
   }

   RARCH_LOG("[Recording]: %s: FPS: %.2f, Sample rate: %.2f\n",
         msg_hash_to_str(MSG_CUSTOM_TIMING_GIVEN),
         (float)av_info->timing.fps,
         (float)av_info->timing.sample_rate);

   if (!string_is_empty(recording_st->path))
      strlcpy(output, recording_st->path, sizeof(output));
   else
   {
      const char *stream_url        = settings->paths.path_stream_url;
      unsigned video_record_quality = settings->uints.video_record_quality;
      unsigned video_stream_port    = settings->uints.video_stream_port;
      if (recording_st->streaming_enable)
         if (!string_is_empty(stream_url))
            strlcpy(output, stream_url, sizeof(output));
         else
            /* Fallback, stream locally to 127.0.0.1 */
            snprintf(output, sizeof(output), "udp://127.0.0.1:%u",
                  video_stream_port);
      else
      {
         const char *game_name = path_basename(path_get(RARCH_PATH_BASENAME));
         /* Fallback to core name if started without content */
         if (string_is_empty(game_name))
            game_name          = runloop_st->system.info.library_name;

         if (video_record_quality < RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST)
         {
            fill_str_dated_filename(buf, game_name,
                     "mkv", sizeof(buf));
            fill_pathname_join_special(output, recording_st->output_dir, buf, sizeof(output));
         }
         else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST
               && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_GIF)
         {
            fill_str_dated_filename(buf, game_name,
                     "webm", sizeof(buf));
            fill_pathname_join_special(output, recording_st->output_dir, buf, sizeof(output));
         }
         else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_GIF
               && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_APNG)
         {
            fill_str_dated_filename(buf, game_name,
                     "gif", sizeof(buf));
            fill_pathname_join_special(output, recording_st->output_dir, buf, sizeof(output));
         }
         else
         {
            fill_str_dated_filename(buf, game_name,
                     "png", sizeof(buf));
            fill_pathname_join_special(output, recording_st->output_dir, buf, sizeof(output));
         }
      }
   }

   params.audio_resampler           = settings->arrays.audio_resampler;
   params.video_gpu_record          = settings->bools.video_gpu_record;
   params.video_record_scale_factor = settings->uints.video_record_scale_factor;
   params.video_stream_scale_factor = settings->uints.video_stream_scale_factor;
   params.video_record_threads      = settings->uints.video_record_threads;
   params.streaming_mode            = settings->uints.streaming_mode;

   params.out_width                 = av_info->geometry.base_width;
   params.out_height                = av_info->geometry.base_height;
   params.fb_width                  = av_info->geometry.max_width;
   params.fb_height                 = av_info->geometry.max_height;
   params.channels                  = 2;
   params.filename                  = output;
   params.fps                       = av_info->timing.fps;
   params.samplerate                = av_info->timing.sample_rate;
   params.pix_fmt                   =
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
      ? FFEMU_PIX_ARGB8888
      : FFEMU_PIX_RGB565;
   params.config                    = NULL;

   if (!string_is_empty(recording_st->config))
      params.config                 = recording_st->config;
   else
   {
      if (recording_st->streaming_enable)
      {
         params.config = settings->paths.path_stream_config;
         params.preset = (enum record_config_type)
            settings->uints.video_stream_quality;
      }
      else
      {
         params.config = settings->paths.path_record_config;
         params.preset = (enum record_config_type)
            settings->uints.video_record_quality;
      }
   }

   if (settings->bools.video_gpu_record
      && video_st->current_video->read_viewport)
   {
      unsigned gpu_size;
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
         RARCH_ERR("[Recording]: Failed to get viewport information from video driver. "
               "Cannot start recording ...\n");
         return false;
      }

      params.out_width                    = vp.width;
      params.out_height                   = vp.height;
      params.fb_width                     = next_pow2(vp.width);
      params.fb_height                    = next_pow2(vp.height);

      if (video_force_aspect &&
            (video_st->aspect_ratio > 0.0f))
         params.aspect_ratio              = video_st->aspect_ratio;
      else
         params.aspect_ratio              = (float)vp.width / vp.height;

      params.pix_fmt                      = FFEMU_PIX_BGR24;
      recording_st->gpu_width             = vp.width;
      recording_st->gpu_height            = vp.height;

      RARCH_LOG("[Recording]: %s %ux%u.\n", msg_hash_to_str(MSG_DETECTED_VIEWPORT_OF),
            vp.width, vp.height);

      gpu_size = vp.width * vp.height * 3;
      if (!(video_st->record_gpu_buffer = (uint8_t*)malloc(gpu_size)))
         return false;
   }
   else
   {
      if (recording_state.width || recording_state.height)
      {
         params.out_width  = recording_state.width;
         params.out_height = recording_state.height;
      }

      if (video_force_aspect &&
            (video_st->aspect_ratio > 0.0f))
         params.aspect_ratio = video_st->aspect_ratio;
      else
         params.aspect_ratio = (float)params.out_width / params.out_height;

#ifdef HAVE_VIDEO_FILTER
      if (settings->bools.video_post_filter_record
            && !!video_st->state_filter)
      {
         unsigned max_width  = 0;
         unsigned max_height = 0;

         params.pix_fmt      = FFEMU_PIX_RGB565;

         if (video_st->state_out_rgb32)
            params.pix_fmt = FFEMU_PIX_ARGB8888;

         rarch_softfilter_get_max_output_size(
               video_st->state_filter,
               &max_width, &max_height);
         params.fb_width  = next_pow2(max_width);
         params.fb_height = next_pow2(max_height);
      }
#endif
   }

   RARCH_LOG("[Recording]: %s %s @ %ux%u. (FB size: %ux%u pix_fmt: %u)\n",
         msg_hash_to_str(MSG_RECORDING_TO),
         output,
         params.out_width, params.out_height,
         params.fb_width, params.fb_height,
         (unsigned)params.pix_fmt);

   if (!record_driver_init_first(
            &recording_state.driver,
            &recording_state.data, &params))
   {
      RARCH_ERR("[Recording]: %s\n",
            msg_hash_to_str(MSG_FAILED_TO_START_RECORDING));
      video_driver_gpu_record_deinit();

      return false;
   }

   return true;
}

void recording_driver_update_streaming_url(void)
{
   settings_t     *settings      = config_get_ptr();
   const char     *youtube_url   = "rtmp://a.rtmp.youtube.com/live2/";
   const char     *twitch_url    = "rtmp://live.twitch.tv/app/";
   const char     *facebook_url  = "rtmps://live-api-s.facebook.com:443/rtmp/";

   if (!settings)
      return;

   switch (settings->uints.streaming_mode)
   {
      case STREAMING_MODE_TWITCH:
         if (!string_is_empty(settings->arrays.twitch_stream_key))
         {
            strlcpy(settings->paths.path_stream_url,
                  twitch_url,
                  sizeof(settings->paths.path_stream_url));
            strlcat(settings->paths.path_stream_url,
                  settings->arrays.twitch_stream_key,
                  sizeof(settings->paths.path_stream_url));
         }
         break;
      case STREAMING_MODE_YOUTUBE:
         if (!string_is_empty(settings->arrays.youtube_stream_key))
         {
            strlcpy(settings->paths.path_stream_url,
                  youtube_url,
                  sizeof(settings->paths.path_stream_url));
            strlcat(settings->paths.path_stream_url,
                  settings->arrays.youtube_stream_key,
                  sizeof(settings->paths.path_stream_url));
         }
         break;
      case STREAMING_MODE_LOCAL:
         /* TODO: figure out default interface and bind to that instead */
         snprintf(settings->paths.path_stream_url, sizeof(settings->paths.path_stream_url),
            "udp://%s:%u", "127.0.0.1", settings->uints.video_stream_port);
         break;
      case STREAMING_MODE_CUSTOM:
      default:
         /* Do nothing, let the user input the URL */
         break;
      case STREAMING_MODE_FACEBOOK:
         if (!string_is_empty(settings->arrays.facebook_stream_key))
         {
            strlcpy(settings->paths.path_stream_url,
                  facebook_url,
                  sizeof(settings->paths.path_stream_url));
            strlcat(settings->paths.path_stream_url,
                  settings->arrays.facebook_stream_key,
                  sizeof(settings->paths.path_stream_url));
         }
         break;
   }
}
