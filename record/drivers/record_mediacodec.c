/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "record_mediacodec.h"
#include "../../gfx/video_driver.h"
#if defined(HAVE_RECORD_MEDIACODEC) || defined(RECORD_MEDIACODEC_FRONTEND_TEST)
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <file/config_file.h>
#include <string/stdstring.h>
#include "../record_android_capture_config.h"
#include "../record_android_capture_request.h"
#include "../../audio/audio_driver.h"
#include "../../audio/audio_upmix.h"
#include "../../configuration.h"
#include "../../command.h"
#include "../../verbosity.h"
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_WEBMPLAYER)
#include "../../defaults.h"
#include "../../playlist.h"
#endif

typedef struct record_mc_context
{
   uint64_t next_us, step, carry, audio_frames, offered;
   struct record_mc_context *next;
   record_dispatch_mediacodec_t *io;
   record_android_capture_config_t *config;
   record_android_capture_request_t *request;
   void *device;
   char *path;
   unsigned divisor, dims, mode, rate, channels;
   enum record_android_capture_operation operation;
   enum record_dispatch_mediacodec_state final_state;
   int error, file_error, system_error;
   bool armed, stopped, failed, audio_closed, gpu_closed;
   bool resetting, reset_ready;
} record_mc_context_t;
static record_mc_context_t *record_mc_active, *record_mc_retired;
static bool record_mc_start_requested;

/* Rational microsecond cadence: rounding each frame separately must not
 * accumulate drift, and movie seeking must never reset this recording clock. */
static bool record_mc_rate(double fps, unsigned *num, unsigned *den)
{
   uint64_t h0 = 0, h1 = 1, k0 = 1, k1 = 0;
   double value = fps;
   unsigned i;
   if (fps != fps || fps < 1.0 || fps > 1000.0)
      return false;
   for (i = 0; i < 32; i++)
   {
      uint64_t a, h, k;
      double fraction;
      if (value > UINT_MAX)
         return false;
      a = (uint64_t)value;
      if ((h1 && a > (UINT_MAX - h0) / h1) || (k1 && a > (1000000u - k0) / k1))
         return false;
      h = a * h1 + h0;
      k = a * k1 + k0;
      if (!k || k > 1000000u)
         return false;
      if (fabs((double)h / k - fps) <= fps * 0.000000001)
      {
         *num = (unsigned)h;
         *den = (unsigned)k;
         return true;
      }
      h0 = h1;
      h1 = h;
      k0 = k1;
      k1 = k;
      fraction = value - (double)a;
      if (fraction <= 0.0)
         return false;
      value = 1.0 / fraction;
   }
   return false;
}

static bool record_mc_options(
      const char *path, mediacodec_encoder_request_t *video, bool *audio)
{
   config_file_t *config;
   struct config_file_entry entry;
   char codec[16], format[8];
   unsigned level;
   bool ok = false;
   *audio = true;
   if (!path || !*path)
      return true;
   config = config_file_new_from_path_to_string(path);
   if (!config)
      return false;
   if (config_get_entry(config, "audio_enable")
         && !config_get_bool(config, "audio_enable", audio))
      goto done;
   if (config_get_entry(config, "vcodec"))
   {
      if (!config_get_array(config, "vcodec", codec, sizeof(codec)))
         goto done;
      if (string_is_equal(codec, "h264"))
         video->mime = "video/avc";
      else if (string_is_equal(codec, "hevc"))
         video->mime = "video/hevc";
      else
         goto done;
   }
   if (config_get_entry(config, "format")
         && (!config_get_array(config, "format", format, sizeof(format))
               || !string_is_equal(format, "mp4")))
      goto done;
   if (config_get_entry(config, "video_bit_rate")
         && (!config_get_uint(config, "video_bit_rate", &video->bitrate)
               || !video->bitrate || video->bitrate > INT_MAX))
      goto done;
   if (config_get_entry(config, "video_level"))
   {
      if (!config_get_uint(config, "video_level", &level) || !level
            || level > INT_MAX || (level & (level - 1)))
         goto done;
      video->level = (int)level;
   }
   if (config_get_entry_list_head(config, &entry))
      do
      {
         if (!string_is_equal(entry.key, "audio_enable")
               && !string_is_equal(entry.key, "vcodec")
               && !string_is_equal(entry.key, "format")
               && !string_is_equal(entry.key, "video_bit_rate")
               && !string_is_equal(entry.key, "video_level"))
            goto done;
      } while (config_get_entry_list_next(&entry));
   ok = true;
done:
   config_file_free(config);
   return ok;
}

static bool record_mc_color(
      mediacodec_encoder_request_t *video, const struct retro_hdr_metadata *hdr)
{
   unsigned i;
   float light[2];
   if (hdr->version != 1 || hdr->size < sizeof(*hdr) || hdr->output_mode > 2)
      return false;
   video->color = hdr->output_mode != 0;
   video->profile = video->color ? 4096 : 1;
   video->range = 1;
   if (!video->color)
      return true;
   if (!string_is_equal(video->mime, "video/hevc")
         || !((hdr->output_mode == 1 && hdr->color_primaries == 9
                    && hdr->transfer_function == 16)
               || (hdr->output_mode == 2 && hdr->color_primaries == 1
                     && hdr->transfer_function == 8)))
      return false;
   /* The core ABI does not supply mastering-display chromaticities.
    * Unknown fields stay zero; never invent them from panel capabilities.
    * The GLES worker converts scRGB to PQ once, before the encoder Surface. */
   video->hdr_static_size = 25;
   light[0] = hdr->max_cll;
   light[1] = hdr->max_fall;
   for (i = 0; i < 2; i++)
   {
      unsigned n;
      if (!(light[i] >= 0.0f && light[i] <= 65535.0f))
         return false;
      n = (unsigned)(light[i] + 0.5f);
      video->hdr_static[21 + 2 * i] = (unsigned char)n;
      video->hdr_static[22 + 2 * i] = (unsigned char)(n >> 8);
   }
   return true;
}

static void record_mc_fail(record_mc_context_t *ctx)
{
   ctx->failed = true;
   ctx->armed = false;
   record_dispatch_mediacodec_cancel(ctx->io);
}
static void record_mc_close(record_mc_context_t *ctx)
{
   ctx->armed = false;
   ctx->stopped = true;
   if (!ctx->audio_closed)
      ctx->audio_closed = record_dispatch_mediacodec_audio_finish(
            ctx->io, ctx->audio_frames);
}
static void record_mc_retire(record_mc_context_t *ctx)
{
   record_mc_close(ctx);
   ctx->next = record_mc_retired;
   record_mc_retired = ctx;
}

static void *record_mc_init(const struct record_params *params)
{
   mediacodec_encoder_request_t encoder;
   video_device_info_t device;
   struct retro_hdr_metadata hdr;
   record_mc_context_t *ctx;
   settings_t *settings = config_get_ptr();
   unsigned num, den, count, rate = 0, channels = 0;
   size_t capacity = 0, path_length;
   bool audio;
   if (!params || !settings || record_mc_active || record_mc_retired
         || !params->filename || !*params->filename || !params->video_gpu_record
         || recording_state_get_ptr()->streaming_enable
         || settings->bools.video_record_game_only
         || audio_driver_has_callback() || !video_driver_get_device(&device)
         || device.api != VIDEO_DEVICE_VULKAN)
      return NULL;
   memset(&encoder, 0, sizeof(encoder));
   memset(&hdr, 0, sizeof(hdr));
   hdr.version = 1;
   hdr.size = sizeof(hdr);
   if (!video_driver_get_display_metadata(
             RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr))
      return NULL;
   encoder.width = VIDEO_SCALE_W(params->out_dims);
   encoder.height = VIDEO_SCALE_H(params->out_dims);
   encoder.frame_rate = params->fps;
   encoder.bitrate = 16000000;
   encoder.mime = hdr.output_mode ? "video/hevc" : "video/avc";
   if (!encoder.width || !encoder.height
         || ((encoder.width | encoder.height) & 1) || encoder.width > 16384
         || encoder.height > 16384 || !record_mc_rate(params->fps, &num, &den)
         || !record_mc_options(params->config, &encoder, &audio)
         || !record_mc_color(&encoder, &hdr)
         || (!hdr.output_mode
               && settings->uints.video_swapchain_bit_depth == 2))
      return NULL;
   /* Explicit Android levels. Inventory/session admission checks the complete
    * requested dimensions, rate, profile, level and color on the I/O worker. */
   if (!encoder.level)
      encoder.level = string_is_equal(encoder.mime, "video/hevc")
            ? (encoder.width <= 1920 && encoder.height <= 1080 ? 4096 : 65536)
            : (encoder.width <= 1920 && encoder.height <= 1080 ? 8192 : 65536);
   if (audio)
   {
      double sr = params->samplerate;
      uint32_t layout = recording_state_get_ptr()->layout;
      if (!(sr >= 8000.0 && sr <= 192000.0) || floor(sr) != sr
            || !params->channels || params->channels > 2
            || audio_layout_channels(layout) != params->channels
            || layout
                  != (params->channels == 1 ? AUDIO_SPEAKER_FRONT_CENTER
                                            : AUDIO_LAYOUT_STEREO))
         return NULL;
      rate = (unsigned)sr;
      channels = params->channels;
      capacity = (size_t)rate * channels * sizeof(int16_t) * 2 + 4096;
   }
   count = params->video_fifo_frames >= 2
               && params->video_fifo_frames <= RECORD_GPU_QUEUE_SLOTS
         ? params->video_fifo_frames
         : 4;
   for (path_length = 0;
         path_length < PATH_MAX_LENGTH && params->filename[path_length];
         path_length++)
   {
   }
   if (path_length == PATH_MAX_LENGTH)
      return NULL;
   ctx = (record_mc_context_t *)calloc(1, sizeof(*ctx) + path_length + 1);
   if (!ctx)
      return NULL;
   ctx->path = (char *)(ctx + 1);
   memcpy(ctx->path, params->filename, path_length + 1);
   ctx->config = record_android_capture_config_new(
         &device, encoder.width, encoder.height, count, hdr.output_mode, false);
   if (!ctx->config)
   {
      free(ctx);
      return NULL;
   }
   ctx->device = device.device;
   ctx->dims = params->out_dims;
   ctx->mode = hdr.output_mode;
   ctx->step = (uint64_t)1000000 * den;
   ctx->divisor = num;
   ctx->rate = rate;
   ctx->channels = channels;
   ctx->io = record_dispatch_mediacodec_new(&encoder, 2, rate, channels,
         audio ? 128000 : 0, audio ? 4096 : 0, capacity, ctx->path);
   if (!ctx->io)
   {
      record_android_capture_config_free(ctx->config);
      free(ctx);
      return NULL;
   }
   ctx->request = record_android_capture_request_new(ctx->io,
         record_android_capture_config_pool(ctx->config),
         (unsigned)(ctx->step / num), params->allow_frame_drop);
   if (!ctx->request)
   {
      record_mc_fail(ctx);
      record_mc_retire(ctx);
      return NULL;
   }
   record_mc_active = ctx;
   return ctx;
}

static bool record_mc_finalize(void *data)
{
   record_mc_context_t *ctx = (record_mc_context_t *)data;
   record_mc_close(ctx);
   return !ctx->failed && !record_dispatch_mediacodec_cancelled(ctx->io);
}
static void record_mc_release(void *data)
{
   record_mc_context_t *ctx = (record_mc_context_t *)data;
   record_mc_active = NULL;
   record_mc_retire(ctx);
}
static bool record_mc_audio(void *data, const struct record_audio_data *audio)
{
   record_mc_context_t *ctx = (record_mc_context_t *)data;
   const int16_t *samples;
   size_t frames;
   if (ctx->stopped || ctx->failed)
      return false;
   if (!ctx->channels || !ctx->armed || (audio && !audio->frames))
      return true;
   if (!audio || !audio->data
         || video_state_get_ptr()->av_info.timing.sample_rate != ctx->rate
         || audio->frames > (~(size_t)0) / ctx->channels / sizeof(int16_t)
         || ctx->audio_frames > ~(uint64_t)0 - audio->frames)
      goto failed;
   samples = (const int16_t *)audio->data;
   frames = audio->frames;
   while (frames)
   {
      size_t chunk = frames > 4096 ? 4096 : frames;
      if (record_dispatch_mediacodec_audio(
                ctx->io, samples, chunk, ctx->audio_frames)
            != RECORD_AUDIO_OK)
         goto failed;
      ctx->audio_frames += chunk;
      samples += chunk * ctx->channels;
      frames -= chunk;
   }
   return true;
failed:
   record_mc_fail(ctx);
   return false;
}
const record_driver_t record_mediacodec = {record_mc_init, record_mc_release,
      NULL, record_mc_audio, record_mc_finalize, "mediacodec"};

void record_mediacodec_frame_info(struct video_frame_info *info, bool capture)
{
   record_mc_context_t *ctx = record_mc_active;
   uint64_t delta, carry, maximum = (((uint64_t)1 << 63) - 1) / 1000;
   info->recording_gpu = NULL;
   info->recording_gpu_capture = false;
   info->recording_gpu_timestamp = 0;
   info->recording_gpu_pending = record_mc_retired != NULL;
   if (!ctx || ctx->stopped || ctx->failed || !ctx->armed)
      return;
   info->recording_gpu
         = (void *)record_android_capture_request_binding(ctx->request);
   if (!info->recording_gpu || !capture
         || recording_state_get_ptr()->data != ctx
         || video_state_get_ptr()->frame_repeat)
      return;
   carry = ctx->carry + ctx->step % ctx->divisor;
   delta = ctx->step / ctx->divisor + carry / ctx->divisor;
   if (ctx->next_us > maximum - delta)
   {
      record_mc_fail(ctx);
      info->recording_gpu = NULL;
      return;
   }
   info->recording_gpu_capture = true;
   info->recording_gpu_timestamp = ctx->next_us;
   ctx->next_us += delta;
   ctx->carry = carry % ctx->divisor;
   ctx->offered++;
}
void record_mediacodec_stop(void)
{
   if (record_mc_active || record_mc_start_requested)
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);
}
bool record_mediacodec_pending(void)
{
   return record_mc_active || record_mc_retired || record_mc_start_requested;
}
bool record_mediacodec_reset_pending(void)
{
   record_mc_context_t *ctx;
   bool pending = record_mc_active != NULL || record_mc_start_requested;
   for (ctx = record_mc_retired; ctx; ctx = ctx->next)
   {
      ctx->resetting = true;
      if (ctx->request && !ctx->reset_ready)
         pending = true;
   }
   return pending;
}
bool record_mediacodec_defer_start(void)
{
   record_mc_start_requested = true;
   return true;
}
bool record_mediacodec_cancel_start(void)
{
   bool requested = record_mc_start_requested;
   record_mc_start_requested = false;
   return requested;
}

static bool record_mc_collect(record_mc_context_t *ctx)
{
   enum record_dispatch_mediacodec_state state;
   if (ctx->request)
   {
      if (ctx->operation == RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE
            && record_android_capture_request_result(ctx->request) == 1)
         ctx->reset_ready = true;
      if (record_android_capture_request_free(ctx->request))
      {
         ctx->request = NULL;
         ctx->gpu_closed = true;
      }
      else
      {
         enum record_android_capture_operation op
               = ctx->resetting && !ctx->reset_ready
               ? RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE
               : RECORD_ANDROID_CAPTURE_COLLECT;
         if (record_android_capture_request_post(
                   ctx->request, op, (int64_t)ctx->next_us))
            ctx->operation = op;
         return false;
      }
   }
   if (!ctx->gpu_closed)
   {
      state = record_dispatch_mediacodec_status(ctx->io, NULL, NULL, NULL);
      if (state != RECORD_MC_DISPATCH_DONE
            && state != RECORD_MC_DISPATCH_FAILED)
      {
         if (!record_dispatch_mediacodec_gpu_claim(ctx->io))
            return false;
         ctx->gpu_closed = record_dispatch_mediacodec_gpu_retired(ctx->io, 0)
               && record_dispatch_mediacodec_gpu_released(ctx->io);
         record_dispatch_mediacodec_gpu_release_claim(ctx->io);
         if (!ctx->gpu_closed)
            return false;
      }
   }
   ctx->final_state = record_dispatch_mediacodec_status(
         ctx->io, &ctx->error, &ctx->file_error, &ctx->system_error);
   if (ctx->final_state != RECORD_MC_DISPATCH_DONE
         && ctx->final_state != RECORD_MC_DISPATCH_FAILED)
      return false;
   return record_dispatch_mediacodec_free(ctx->io);
}
void record_mediacodec_poll(void)
{
   record_mc_context_t **link;
   if (record_mc_start_requested)
   {
      struct video_viewport vp;
      memset(&vp, 0, sizeof(vp));
      if (!recording_state_get_ptr()->enable)
         record_mc_start_requested = false;
      else if (video_driver_get_viewport_info(&vp) && VIDEO_SCALE_W(vp.dims)
            && VIDEO_SCALE_H(vp.dims))
      {
         record_mc_start_requested = false;
         command_event(CMD_EVENT_RECORD_INIT, NULL);
      }
   }
   if (record_mc_active)
   {
      record_mc_context_t *ctx = record_mc_active;
      video_device_info_t device;
      struct retro_hdr_metadata hdr;
      struct video_viewport vp;
      enum record_dispatch_mediacodec_state state
            = record_dispatch_mediacodec_status(ctx->io, NULL, NULL, NULL);
      memset(&hdr, 0, sizeof(hdr));
      memset(&vp, 0, sizeof(vp));
      hdr.version = 1;
      hdr.size = sizeof(hdr);
      if (!video_driver_get_device(&device) || device.device != ctx->device
            || !video_driver_get_display_metadata(
                  RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr)
            || hdr.output_mode != ctx->mode
            || !video_driver_get_viewport_info(&vp) || vp.dims != ctx->dims
            || audio_driver_has_callback()
            || state >= RECORD_MC_DISPATCH_DRAINING
            || record_dispatch_mediacodec_cancelled(ctx->io))
         record_mc_fail(ctx);
      if (ctx->failed || ctx->stopped)
         record_mediacodec_stop();
      else if (!ctx->armed)
      {
         if (record_android_capture_request_binding(ctx->request))
            ctx->armed = true;
         else if (record_android_capture_request_result(ctx->request) < 0)
         {
            record_mc_fail(ctx);
            record_mediacodec_stop();
         }
         else if (record_android_capture_request_post(
                        ctx->request, RECORD_ANDROID_CAPTURE_CREATE, 0))
            ctx->operation = RECORD_ANDROID_CAPTURE_CREATE;
      }
   }
   link = &record_mc_retired;
   while (*link)
   {
      record_mc_context_t *ctx = *link;
      record_mc_close(ctx);
      if (!ctx->audio_closed || !record_mc_collect(ctx))
      {
         link = &ctx->next;
         continue;
      }
      if (ctx->final_state == RECORD_MC_DISPATCH_DONE && !ctx->failed
            && !ctx->error && !ctx->file_error && !ctx->system_error
            && ctx->offered)
      {
         RARCH_LOG("[MediaCodec recording] Finalized %llu offered frames, "
                   "%llu PCM frames.\n",
               (unsigned long long)ctx->offered,
               (unsigned long long)ctx->audio_frames);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_WEBMPLAYER)
         /* DONE follows mux/file finalization and independent native-user
          * retirement. Offered frames or frontend finalize alone prove none
          * of these; publish only after successful I/O collection. */
         {
            settings_t *settings = config_get_ptr();
            if (settings && settings->bools.history_list_enable
                  && g_defaults.video_history)
            {
               struct playlist_entry entry = {0};
               entry.path = ctx->path;
               entry.core_path = (char *)"builtin";
               entry.core_name = (char *)"movieplayer";
               command_playlist_push_write(g_defaults.video_history, &entry);
            }
         }
#endif
      }
      else
         RARCH_ERR("[MediaCodec recording] Failed (session %d, file %d, "
                   "system %d).\n",
               ctx->error, ctx->file_error, ctx->system_error);
      record_android_capture_config_free(ctx->config);
      *link = ctx->next;
      free(ctx);
   }
}
#else
const record_driver_t record_mediacodec
      = {NULL, NULL, NULL, NULL, NULL, "mediacodec"};
void record_mediacodec_frame_info(struct video_frame_info *info, bool capture)
{
   (void)capture;
   info->recording_gpu = NULL;
   info->recording_gpu_timestamp = 0;
   info->recording_gpu_capture = info->recording_gpu_pending = false;
}
void record_mediacodec_poll(void)
{
}
void record_mediacodec_stop(void)
{
}
bool record_mediacodec_pending(void)
{
   return false;
}
bool record_mediacodec_reset_pending(void)
{
   return false;
}
bool record_mediacodec_defer_start(void)
{
   return false;
}
bool record_mediacodec_cancel_start(void)
{
   return false;
}
#endif
