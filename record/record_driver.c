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

#include "../command.h"
#include "../configuration.h"
#include "../content.h"
#include "../list_special.h"
#include "../gfx/video_driver.h"
#include "../audio/audio_driver.h"
#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif
#include "../paths.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"
#include "../defaults.h"

#include "record_driver.h"
#include "../audio/audio_upmix.h"
#include "drivers/record_ffmpeg.h"
#include "drivers/record_wav.h"
#include "drivers/record_nvenc.h"
#include "drivers/record_metal.h"
#include "drivers/record_mediacodec.h"
#include "drivers/record_avfoundation.h"

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
#ifdef HAVE_AVF
   &record_avfoundation,
#endif
#ifdef HAVE_RECORD_METAL
   &record_metal,
#endif
#ifdef HAVE_RECORD_MEDIACODEC
   &record_mediacodec,
#endif
#ifdef HAVE_FFMPEG
   &record_ffmpeg,
#endif
   &record_wav,
#ifdef HAVE_RECORD_NVENC
   &record_nvenc,
#endif
   &record_null,
   NULL,
};

void recording_gpu_frame_info(struct video_frame_info *info,bool capture)
{
#if defined(HAVE_RECORD_MEDIACODEC)
   record_mediacodec_frame_info(info, capture);
#elif defined(HAVE_RECORD_METAL)
   record_metal_frame_info(info,capture);
#else
   record_nvenc_frame_info(info,capture);
#endif
}
void recording_gpu_poll(void)
{
   if (recording_state.start_pending)
   {
      runloop_state_t *loop = runloop_state_get_ptr();
      if (loop->quit_pending || loop->content_closing)
         command_event(CMD_EVENT_RECORD_DEINIT, NULL);
   }
   record_nvenc_poll();
   record_metal_poll();
   record_mediacodec_poll();
   recording_gpu_reinit_poll();
}
void recording_start_frame(void)
{
   if (!recording_state.start_pending || runloop_is_content_switching()
         || content_load_state_in_progress(NULL)
         || runloop_state_get_ptr()->auto_state_load_pending)
      return;
   recording_state.start_pending = false;
   if (!recording_init())
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);
}
void recording_gpu_stop(void)
{
   record_nvenc_stop();
   record_metal_stop();
   record_mediacodec_stop();
}
bool recording_gpu_pending(void)
{
   return record_nvenc_pending() || record_metal_pending()
      || record_mediacodec_pending();
}
bool recording_gpu_reset_pending(void)
{
   return record_nvenc_pending() || record_metal_pending()
      || record_mediacodec_reset_pending();
}

bool recording_gpu_cancel_start(void)
{
   bool nvenc=record_nvenc_cancel_start();
   bool metal=record_metal_cancel_start();
   bool mediacodec = record_mediacodec_cancel_start();
   return nvenc || metal || mediacodec;
}
bool recording_gpu_defer_start(const record_driver_t *driver)
{
   if (driver==&record_nvenc) return record_nvenc_defer_start();
   if (driver==&record_metal) return record_metal_defer_start();
   if (driver == &record_mediacodec) return record_mediacodec_defer_start();
   return false;
}

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

static void find_record_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i                = (int)driver_find_index(
         "record_driver",
         settings->arrays.record_driver);

   if (i >= 0)
      recording_state.driver = (const record_driver_t*)record_drivers[i];
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;

         RARCH_ERR("Couldn't find any record driver named \"%s\".\n",
               settings->arrays.record_driver);
         RARCH_LOG_OUTPUT("Available record drivers are:\n");
         for (d = 0; record_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", record_drivers[d]->ident);
         RARCH_WARN("Going to default to first record driver...\n");
      }

      recording_state.driver = (const record_driver_t*)record_drivers[0];

      if (!recording_state.driver)
         retroarch_fail(1, "find_record_driver()");
   }
}

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
static bool record_driver_init(
      const struct record_params *params)
{
   find_record_driver();
   if (!recording_state.driver)
      return false;

   recording_state.data = recording_state.driver->init(params);
   return recording_state.data != NULL;
}

bool recording_deinit(void)
{
   recording_state_t *recording_st = &recording_state;
   bool success                   = true;
#ifdef HAVE_FFMPEG
   bool native                    = recording_is_native_gpu(recording_st->driver);
   bool history_list_enable        = config_get_ptr()->bools.history_list_enable;
#endif

   if (recording_st->start_pending)
   {
      recording_st->start_pending = false;
      recording_st->path[0] = '\0';
      return true;
   }

   if (recording_gpu_cancel_start())
   {
      recording_st->driver = NULL;
      recording_st->data = NULL;
      recording_st->path[0] = '\0';
      return true;
   }

   if (     !recording_st->data
		   || !recording_st->driver)
      return false;

   if (recording_st->driver->finalize)
      success = recording_st->driver->finalize(recording_st->data);

   if (recording_st->driver->free)
      recording_st->driver->free(recording_st->data);

   recording_st->data              = NULL;
   recording_st->driver            = NULL;

   /* Under the wrapper the readback buffers are retired, freed once no
    * frame in flight names them: nothing to wait for */
   video_driver_gpu_record_deinit();

   if (!success)
      RARCH_ERR("[Recording] Failed to finalize recording.\n");

   /* Push recording to video history playlist */
#ifdef HAVE_FFMPEG
   if (     success && history_list_enable
         && !native
         && *recording_st->path)
   {
      struct playlist_entry entry = {0};

      /* the push function reads our entry as const, so these casts are safe */
      entry.path                  = recording_st->path;
      entry.core_path             = (char*)"builtin";
      entry.core_name             = (char*)"movieplayer";

      command_playlist_push_write(g_defaults.video_history, &entry);
   }
#endif

   /* Forget cached path to create a new one next */
   recording_st->path[0] = '\0';

   return success;
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
   bool nvenc_native = string_is_equal(settings->arrays.record_driver,"nvenc");
   bool metal_native = string_is_equal(settings->arrays.record_driver,"metal");
   bool mc_native = string_is_equal(settings->arrays.record_driver,"mediacodec");
   bool gpu_native = nvenc_native || metal_native || mc_native;
   bool menu_source = false;
#ifdef HAVE_RECORD_MEDIACODEC
   menu_source = mc_native && current_core_type == CORE_TYPE_DUMMY;
#endif

   if (!recording_enable)
      return false;

   if (content_load_state_in_progress(NULL)
         || runloop_st->auto_state_load_pending)
   {
      recording_st->start_pending = true;
      return true;
   }
   recording_st->start_pending = false;

#ifndef HAVE_RECORD_NVENC
   if (nvenc_native)
   {
      RARCH_ERR("[Recording] Native NVENC recording is unavailable in this build.\n");
      return false;
   }
#endif
#ifndef HAVE_RECORD_METAL
   if (metal_native)
   {
      RARCH_ERR("[Recording] Native Metal recording is unavailable in this build.\n");
      return false;
   }
#endif
#ifndef HAVE_RECORD_MEDIACODEC
   if (mc_native)
   {
      RARCH_ERR("[Recording] Native MediaCodec recording is unavailable in this build.\n");
      return false;
   }
#endif
   if (gpu_native) video_gpu_record = true;
   if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_SCRGB_RGBA16F)
   {
      RARCH_ERR("[Recording] Native FP16 core recording is not supported.\n");
      return false;
   }
   output[0] = '\0';

   if (current_core_type == CORE_TYPE_DUMMY && !menu_source)
   {
      RARCH_WARN("[Recording] %s\n",
            msg_hash_to_str(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED));
      return false;
   }

   if (!video_gpu_record && (video_driver_is_hw_context()
            || video_driver_cached_frame_is_hw_render()))
   {
      RARCH_WARN("[Recording] %s\n",
            msg_hash_to_str(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING));
      video_gpu_record = true;
   }

   RARCH_LOG("[Recording] %s: FPS: %.2f, Sample rate: %.2f Hz.\n",
         msg_hash_to_str(MSG_CUSTOM_TIMING_GIVEN),
         (float)av_info->timing.fps,
         (float)av_info->timing.sample_rate);

   if (*recording_st->path)
      strlcpy(output, recording_st->path, sizeof(output));
   else
   {
      const char *stream_url        = settings->paths.path_stream_url;
      unsigned video_record_quality = settings->uints.video_record_quality;
      unsigned video_stream_port    = settings->uints.video_stream_port;
      if (recording_st->streaming_enable)
      {
         if (stream_url && *stream_url)
            strlcpy(output, stream_url, sizeof(output));
         else
         {
            /* Fallback, stream locally to 127.0.0.1 */
            size_t _len = strlcpy_lit(output, "udp://127.0.0.1:", sizeof(output));
            snprintf(output + _len, sizeof(output) - _len, "%u",
                  video_stream_port);
         }
      }
      else
      {
         const char *game_name = path_basename(path_get(RARCH_PATH_BASENAME));
         if (!path_is_directory(recording_st->output_dir))
            path_mkdir(recording_st->output_dir);
         /* Fallback to core name if started without content */
         if (!game_name || !*game_name)
            game_name = runloop_st->system.info.library_name;

         {
            const char *ext = "mkv";
            if (gpu_native) ext = "mp4";
            else
#ifdef HAVE_AVF
            if (string_is_equal(settings->arrays.record_driver,
                     "avfoundation"))
               ext = "mov";
            else
#endif
            if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST
                  && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_GIF)
               ext = "webm";
            else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_GIF
                  && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_APNG)
               ext = "gif";
            else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_APNG)
               ext = "png";

            fill_str_dated_filename(buf, game_name, ext, sizeof(buf));
            fill_pathname_join_special(output,
                  recording_st->output_dir, buf, sizeof(output));
         }

         /* Cache path for playlist saving */
         if (*output)
            strlcpy(recording_st->path, output, sizeof(recording_st->path));
      }
   }

   params.audio_resampler           = settings->arrays.audio_resampler;
   params.video_gpu_record          = video_gpu_record;
   params.video_record_scale_factor = settings->uints.video_record_scale_factor;
   params.video_stream_scale_factor = settings->uints.video_stream_scale_factor;
   params.video_record_threads      = settings->uints.video_record_threads;
   params.streaming_mode            = settings->uints.streaming_mode;
   params.allow_frame_drop          = settings->bools.video_record_allow_frame_drop;
   params.video_fifo_frames         = settings->uints.video_record_fifo_frames;
   params.hw_encoder                = settings->bools.video_record_hw_encoder;

   params.out_dims                  = VIDEO_SCALE_PACK(
         av_info->geometry.base_width, av_info->geometry.base_height);
   params.fb_dims                   = VIDEO_SCALE_PACK(
         av_info->geometry.max_width, av_info->geometry.max_height);
   /* A core delivering a wider layout than stereo through the
    * multi-channel batch entry is recorded in it, where the container
    * has a default layout for the count - quad, 5.1 with the pair at
    * the back, 7.1 - which are the layouts in the order the recorder
    * takes them. Anything else, stereo. Native GPU recording keeps the
    * exact declared mask for its encoder to admit or refuse. */
   params.channels                  = 2;
   recording_st->layout              = AUDIO_LAYOUT_STEREO;
   {
      uint32_t core_layout = audio_state_get_ptr()->core_layout;
      if (     (gpu_native && core_layout)
            || core_layout == AUDIO_LAYOUT_QUAD
            || core_layout == AUDIO_LAYOUT_5POINT1
            || core_layout == AUDIO_LAYOUT_7POINT1)
      {
         params.channels   = audio_layout_channels(core_layout);
         recording_st->layout = core_layout;
      }
   }
   recording_st->channels            = params.channels;
   params.filename                  = output;
   params.fps                       = av_info->timing.fps;
   params.samplerate                = av_info->timing.sample_rate;
   params.menu_source               = menu_source;
   if (menu_source)
   {
      /* Menu capture follows output timing, not dummy-core AV info. */
      params.fps = settings->floats.video_refresh_rate;
      if (!(params.fps > 0.0))
         params.fps = video_driver_get_window_refresh_rate();
      params.samplerate = audio_state_get_ptr()->out_rate;
      params.channels = recording_st->channels = 2;
      recording_st->layout = AUDIO_LAYOUT_STEREO;
   }
   /* XRGB2101010 source frames are down-converted to XRGB8888 before the
    * recording path sees them (see video_driver_frame), so they record as
    * ARGB8888 just like a native XRGB8888 core. */
   params.pix_fmt                   =
      (   video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888
       || video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB2101010)
      ? FFEMU_PIX_ARGB8888
      : FFEMU_PIX_RGB565;
   params.config                    = NULL;

   if (*recording_st->config)
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

   if (gpu_native)
   {
      struct video_viewport vp;
      memset(&vp, 0, sizeof(vp));
      if (!video_driver_get_viewport_info(&vp)) return false;
      if (!VIDEO_SCALE_W(menu_source ? vp.full_dims : vp.dims)
            || !VIDEO_SCALE_H(menu_source ? vp.full_dims : vp.dims))
      {
         recording_st->driver = mc_native ? &record_mediacodec
               : metal_native ? &record_metal : &record_nvenc;
         return recording_gpu_defer_start(recording_st->driver);
      }
      params.out_dims = params.fb_dims = menu_source ? vp.full_dims : vp.dims;
      params.pix_fmt = FFEMU_PIX_ARGB8888;
      params.aspect_ratio = (float)VIDEO_SCALE_W(params.out_dims)
            / VIDEO_SCALE_H(params.out_dims);
      recording_st->gpu_dims = params.out_dims;
      recording_st->gpu_bgrx = false;
   }
   else if (video_gpu_record && video_st->current_video->read_viewport)
   {
      size_t gpu_size;
      struct video_viewport vp;

      vp.pos                      = VIDEO_POS_PACK(0, 0);
      vp.dims                     = 0;
      vp.full_dims                = 0;

      video_driver_get_viewport_info(&vp);

      if (!VIDEO_SCALE_W(vp.dims) || !VIDEO_SCALE_H(vp.dims))
      {
         RARCH_ERR("[Recording] Failed to get viewport information from video driver. "
               "Cannot start recording.\n");
         return false;
      }

      params.out_dims                     = vp.dims;
      /* Every frame is exactly the viewport (a resize ends the
       * recording), so size the encoder's queue to it. */
      params.fb_dims                      = vp.dims;

      if (video_force_aspect &&
            (VIDEO_DRIVER_ASPECT_RATIO(video_st) > 0.0f))
         params.aspect_ratio              = VIDEO_DRIVER_ASPECT_RATIO(video_st);
      else
         params.aspect_ratio              = (float)VIDEO_SCALE_W(vp.dims) / VIDEO_SCALE_H(vp.dims);

      /* The driver's 32-bit readback skips a per-frame BGR24
       * conversion; FFEMU_PIX_ARGB8888 is a native-endian word, so B,G,R,X
       * bytes only on little-endian hosts. The threaded wrapper reads back
       * on its own thread, in either format when the driver gives it a
       * reader. */
#ifdef MSB_FIRST
      recording_st->gpu_bgrx              = false;
#else
      if (video_driver_thread_wrapper_active())
      {
#ifdef HAVE_THREADS
         recording_st->gpu_bgrx           =
               video_thread_record_has_reader(video_st->data);
#else
         recording_st->gpu_bgrx           = false;
#endif
      }
      else
         recording_st->gpu_bgrx           =
               video_st->current_video->read_viewport_bgrx != NULL;
#endif
      params.pix_fmt                      = recording_st->gpu_bgrx
         ? FFEMU_PIX_ARGB8888 : FFEMU_PIX_BGR24;

      /* Said once, as the recording starts: this driver reads back the
       * screen as shown, with the menu and messages in it. */
      if (     settings->bools.video_record_game_only
            && !video_driver_test_all_flags(GFX_CTX_FLAGS_RECORD_GAME_ONLY))
      {
         const char *_msg = msg_hash_to_str(
               MSG_RECORDING_GAME_ONLY_UNSUPPORTED);
         RARCH_WARN("[Recording] %s\n", _msg);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 240, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      }
      recording_st->gpu_dims              = vp.dims;

      RARCH_LOG("[Recording] %s %ux%u (%s readback).\n",
            msg_hash_to_str(MSG_DETECTED_VIEWPORT_OF),
            VIDEO_SCALE_W(vp.dims), VIDEO_SCALE_H(vp.dims),
            recording_st->gpu_bgrx ? "BGRX" : "BGR24");

      gpu_size = VIDEO_SCALE_AREA(vp.dims) * (recording_st->gpu_bgrx ? 4 : 3);
      if (!(video_st->record_gpu_buffer = (uint8_t*)malloc(gpu_size)))
         return false;
   }
   else
   {
      recording_st->gpu_bgrx = false;
      if (recording_state.out_dims)
         params.out_dims   = recording_state.out_dims;

      /* Raw core frames are unrotated; the GPU path reads back the
       * already-rotated screen. Rotate them like the display does. */
      params.rotation = retroarch_get_rotation() % 4;
      record_raw_geometry(&params.out_dims, &params.aspect_ratio,
            params.rotation,
            video_force_aspect ? VIDEO_DRIVER_ASPECT_RATIO(video_st) : 0.0f,
            recording_state.out_dims != 0);

#ifdef HAVE_VIDEO_FILTER
      if (settings->bools.video_post_filter_record
            && !!video_st->state_filter)
      {
         unsigned max_dims   = 0;

         params.pix_fmt      = FFEMU_PIX_RGB565;

         if ((uint32_t)retro_atomic_load_relaxed_int(&video_st->flags) & VIDEO_FLAG_STATE_OUT_RGB32)
            params.pix_fmt = FFEMU_PIX_ARGB8888;

         rarch_softfilter_get_max_output_size(
               video_st->state_filter, &max_dims);
         params.fb_dims   = VIDEO_SCALE_PACK(
               next_pow2(VIDEO_SCALE_W(max_dims)),
               next_pow2(VIDEO_SCALE_H(max_dims)));
      }
#endif
   }

   RARCH_LOG("[Recording] %s %s @ %ux%u (FB size: %ux%u pix_fmt: %u).\n",
         msg_hash_to_str(MSG_RECORDING_TO),
         output,
         VIDEO_SCALE_W(params.out_dims), VIDEO_SCALE_H(params.out_dims),
         VIDEO_SCALE_W(params.fb_dims), VIDEO_SCALE_H(params.fb_dims),
         (unsigned)params.pix_fmt);

   if (!record_driver_init(&params))
   {
      RARCH_ERR("[Recording] %s\n",
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
   const char     *kick_url      = "rtmps://fa723fc1b171.global-contribute.live-video.net:443/app/";

   if (!settings)
      return;

   switch (settings->uints.streaming_mode)
   {
      case STREAMING_MODE_KICK:
         if (*settings->arrays.kick_stream_key)
         {
            size_t _len = strlcpy(settings->paths.path_stream_url,
                  kick_url,
                  sizeof(settings->paths.path_stream_url));
            strlcpy(settings->paths.path_stream_url       + _len,
                  settings->arrays.kick_stream_key,
                  sizeof(settings->paths.path_stream_url) - _len);
         }
         break;
      case STREAMING_MODE_TWITCH:
         if (*settings->arrays.twitch_stream_key)
         {
            size_t _len = strlcpy(settings->paths.path_stream_url,
                  twitch_url,
                  sizeof(settings->paths.path_stream_url));
            strlcpy(settings->paths.path_stream_url       + _len,
                  settings->arrays.twitch_stream_key,
                  sizeof(settings->paths.path_stream_url) - _len);
         }
         break;
      case STREAMING_MODE_YOUTUBE:
         if (*settings->arrays.youtube_stream_key)
         {
            size_t _len = strlcpy(settings->paths.path_stream_url,
                  youtube_url,
                  sizeof(settings->paths.path_stream_url));
            strlcpy(settings->paths.path_stream_url       + _len,
                  settings->arrays.youtube_stream_key,
                  sizeof(settings->paths.path_stream_url) - _len);
         }
         break;
      case STREAMING_MODE_LOCAL:
         {
            /* TODO: figure out default interface and bind to that instead */
            size_t _len = strlcpy_lit(settings->paths.path_stream_url, "udp://127.0.0.1:",
                  sizeof(settings->paths.path_stream_url));
            snprintf(settings->paths.path_stream_url      + _len,
                  sizeof(settings->paths.path_stream_url) - _len,
                  "%u", settings->uints.video_stream_port);
         }
         break;
      case STREAMING_MODE_FACEBOOK:
         if (*settings->arrays.facebook_stream_key)
         {
            size_t _len = strlcpy(settings->paths.path_stream_url,
                  facebook_url,
                  sizeof(settings->paths.path_stream_url));
            strlcpy(settings->paths.path_stream_url       + _len,
                  settings->arrays.facebook_stream_key,
                  sizeof(settings->paths.path_stream_url) - _len);
         }
         break;
      case STREAMING_MODE_CUSTOM:
      default:
         /* Do nothing, let the user input the URL */
         break;
   }
}
