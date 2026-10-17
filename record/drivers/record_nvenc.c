/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "record_nvenc.h"
#include "../../gfx/video_driver.h"
#ifdef HAVE_RECORD_NVENC
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <file/config_file.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include "../record_gpu_dispatch_d3d12.h"
#include "../../gfx/video_thread_wrapper.h"
#include "../../audio/audio_driver.h"
#include "../../audio/audio_upmix.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../defaults.h"
#include "../../playlist.h"
#include "../../verbosity.h"

typedef struct record_nvenc_context
{
   uint64_t next_timestamp, origin, captured, audio_frames;
   struct record_nvenc_context *next;
   record_gpu_dispatch_d3d12_t *job;
   void *device;
   char *path;
   unsigned dims, duration, audio_rate;
   retro_atomic_int_t failed, stop;
   bool allow_drop, have_origin, video_closed;
   bool audio_enabled, armed;
} record_nvenc_context_t;

static record_nvenc_context_t *record_nvenc_active;
static record_nvenc_context_t *record_nvenc_retired;
static bool record_nvenc_start_requested;

static bool record_nvenc_rate(double fps, unsigned *num, unsigned *den)
{
   uint64_t h0 = 0, h1 = 1, k0 = 1, k1 = 0;
   double value = fps;
   unsigned i;
   if (fps != fps || fps < 1.0 || fps > 1000.0) return false;
   for (i = 0; i < 32; i++)
   {
      uint64_t a, h, k;
      double fraction;
      if (value > UINT_MAX) return false;
      a = (uint64_t)value;
      if ((h1 && a > (UINT_MAX - h0) / h1)
            || (k1 && a > (1000000u - k0) / k1)) return false;
      h = a * h1 + h0; k = a * k1 + k0;
      if (!k || k > 1000000u) return false;
      if (fabs((double)h / k - fps) <= fps * 0.000000001)
      {
         *num = (unsigned)h; *den = (unsigned)k;
         return true;
      }
      h0 = h1; h1 = h; k0 = k1; k1 = k;
      fraction = value - (double)a;
      if (fraction <= 0.0) return false;
      value = 1.0 / fraction;
   }
   return false;
}

static bool record_nvenc_config(const char *path, nvenc_d3d12_config_t *encoder,
      bool *audio_enabled)
{
   config_file_t *config;
   struct config_file_entry entry;
   char codec[16], format[8];
   bool audio = true, ok = false;
   if (!path || !*path) return false;
   config = config_file_new_from_path_to_string(path);
   if (!config) return false;
   if (!config_get_bool(config, "audio_enable", &audio)) goto done;
   if (config_get_entry(config, "vcodec"))
   {
      if (!config_get_array(config, "vcodec", codec, sizeof(codec))) goto done;
      if (string_is_equal(codec, "h264")) encoder->codec = NVENC_D3D12_H264;
      else if (string_is_equal(codec, "hevc")) encoder->codec = NVENC_D3D12_HEVC;
      else if (string_is_equal(codec, "av1")) encoder->codec = NVENC_D3D12_AV1;
      else goto done;
   }
   if (config_get_entry(config, "format")
         && (!config_get_array(config, "format", format, sizeof(format))
            || !string_is_equal(format, "mp4"))) goto done;
   if (config_get_entry(config, "video_bit_rate")
         && (!config_get_uint(config, "video_bit_rate", &encoder->bitrate)
            || !encoder->bitrate)) goto done;
   if (config_get_entry_list_head(config, &entry))
      do
      {
         if (!string_is_equal(entry.key, "audio_enable")
               && !string_is_equal(entry.key, "vcodec")
               && !string_is_equal(entry.key, "video_bit_rate")
               && !string_is_equal(entry.key, "format")) goto done;
      } while (config_get_entry_list_next(&entry));
   *audio_enabled = audio;
   ok = true;
done:
   config_file_free(config);
   return ok;
}

static bool record_nvenc_layout(uint32_t layout, unsigned channels)
{
   if (!channels || channels > 6 || audio_layout_channels(layout) != channels)
      return false;
   /* AC-3 represents paired surrounds at the sides. */
   switch (layout & ~AUDIO_SPEAKER_LOW_FREQUENCY)
   {
      case 0x004u:
      case 0x003u:
      case 0x007u:
      case 0x103u:
      case 0x107u:
      case 0x603u:
      case 0x607u:
         return true;
   }
   return false;
}

static void *record_nvenc_init(const struct record_params *params)
{
   nvenc_d3d12_config_t encoder;
   record_gpu_output_config_t output;
   video_device_info_t device;
   record_gpu_capture_d3d12_t *capture;
   record_nvenc_context_t *ctx;
   settings_t *settings = config_get_ptr();
   size_t length;
   bool audio_enabled;
   if (record_nvenc_active || record_nvenc_retired || !params
         || !params->filename || !*params->filename) return NULL;
   if (!params->video_gpu_record || recording_state_get_ptr()->streaming_enable
         || settings->uints.video_hdr_mode
         || settings->uints.video_swapchain_bit_depth == 2
         || settings->bools.video_record_game_only || audio_driver_has_callback())
   {
      RARCH_ERR("[NVENC] Requires SDR D3D12 viewport recording without Game Only, streaming or an asynchronous core audio callback.\n");
      return NULL;
   }
   memset(&encoder, 0, sizeof(encoder)); memset(&output, 0, sizeof(output));
   if (!video_driver_get_device(&device) || device.api != VIDEO_DEVICE_D3D12
         || !device.device || !device.queue) return NULL;
   encoder.device = device.device; encoder.slots = 4;
   encoder.width = VIDEO_SCALE_W(params->out_dims);
   encoder.height = VIDEO_SCALE_H(params->out_dims);
   encoder.format = NVENC_D3D12_BGRA8; encoder.color = NVENC_D3D12_COLOR_BT709;
   encoder.bitrate = 16000000;
   if (!record_nvenc_rate(params->fps, &encoder.fps_num, &encoder.fps_den)
         || !record_nvenc_config(params->config, &encoder, &audio_enabled))
   {
      RARCH_ERR("[NVENC] Requires a valid frame rate and a native MP4 config with an explicit audio_enable setting.\n");
      return NULL;
   }
   if (audio_enabled)
   {
      if ((params->samplerate != 32000.0 && params->samplerate != 44100.0
               && params->samplerate != 48000.0)
            || !record_nvenc_layout(recording_state_get_ptr()->layout, params->channels))
      {
         RARCH_ERR("[NVENC] Audio requires an AC-3 speaker layout (up to six channels) at 32, 44.1 or 48 kHz.\n");
         return NULL;
      }
      output.audio_rate = (unsigned)params->samplerate;
      output.audio_layout = recording_state_get_ptr()->layout;
      output.audio_kbps = params->channels > 2 ? 448 : 192;
      output.audio_fifo_frames = output.audio_rate;
   }
   length = strlen(params->filename);
   if (length >= PATH_MAX_LENGTH) return NULL;
   ctx = (record_nvenc_context_t*)calloc(1, sizeof(*ctx) + length + 1);
   if (!ctx) return NULL;
   ctx->path = (char*)(ctx + 1); memcpy(ctx->path, params->filename, length + 1);
   ctx->audio_rate = output.audio_rate;
   ctx->audio_enabled = audio_enabled;
   ctx->armed = !audio_enabled;
   ctx->device = device.device; ctx->dims = params->out_dims;
   ctx->duration = encoder.fps_den; ctx->allow_drop = params->allow_frame_drop;
   retro_atomic_int_init(&ctx->failed, 0); retro_atomic_int_init(&ctx->stop, 0);
   capture = record_gpu_capture_d3d12_new(&encoder, device.queue);
   if (!capture)
   {
      free(ctx);
      return NULL;
   }
   output.video.timescale = encoder.fps_num;
   output.video.max_packet_size = 16 * 1024 * 1024;
   ctx->job = record_gpu_dispatch_d3d12_new_owned(capture, &encoder, &output, ctx->path);
   if (!ctx->job)
   {
      record_gpu_capture_d3d12_free(capture);
      free(ctx); return NULL;
   }
   record_nvenc_active = ctx;
   RARCH_LOG("[NVENC] Starting native %s %u/%u fps recording.\n",
         audio_enabled ? "AC-3/video" : "video-only", encoder.fps_num, encoder.fps_den);
   return ctx;
}

static bool record_nvenc_finalize(void *data)
{
   record_nvenc_context_t *ctx = (record_nvenc_context_t*)data;
   enum record_gpu_session_error error;
   retro_atomic_cas_int(&ctx->stop, 0, 2);
   record_gpu_dispatch_d3d12_audio_finish(ctx->job);
   record_gpu_dispatch_d3d12_status(ctx->job, &error, NULL, NULL);
   return !error && !retro_atomic_load_acquire_int(&ctx->failed);
}

static void record_nvenc_release(void *data)
{
   record_nvenc_context_t *ctx = (record_nvenc_context_t*)data;
   record_nvenc_active = NULL;
   ctx->next = record_nvenc_retired; record_nvenc_retired = ctx;
}

static bool record_nvenc_audio(void *data, const struct record_audio_data *audio)
{
   record_nvenc_context_t *ctx = (record_nvenc_context_t*)data;
   int result;
   if (!ctx->audio_enabled || !ctx->armed || (audio && !audio->frames)) return true;
   if (retro_atomic_load_acquire_int(&ctx->stop)
         || retro_atomic_load_acquire_int(&ctx->failed)) return false;
   if (!audio || video_state_get_ptr()->av_info.timing.sample_rate != ctx->audio_rate
         || ctx->audio_frames > UINT64_MAX - audio->frames)
      result = RECORD_GPU_OUTPUT_ERROR;
   else
      result = record_gpu_dispatch_d3d12_audio(ctx->job,
            (const int16_t*)audio->data, audio->frames);
   if (result != RECORD_GPU_OUTPUT_PROGRESS)
   {
      retro_atomic_store_release_int(&ctx->failed, 1);
      record_gpu_dispatch_d3d12_cancel(ctx->job);
      return false;
   }
   ctx->audio_frames += audio->frames;
   return true;
}

const record_driver_t record_nvenc = {
   record_nvenc_init, record_nvenc_release, NULL, record_nvenc_audio, record_nvenc_finalize, "nvenc"
};

void record_nvenc_frame_info(struct video_frame_info *info, bool capture)
{
   record_nvenc_context_t *ctx = record_nvenc_active;
   info->recording_gpu = ctx;
   info->recording_gpu_pending = record_nvenc_retired != NULL;
   info->recording_gpu_capture = false; info->recording_gpu_timestamp = 0;
   if (!ctx || !capture || !ctx->armed || recording_state_get_ptr()->data != ctx
         || video_state_get_ptr()->frame_repeat) return;
   if (ctx->next_timestamp > UINT64_MAX - ctx->duration)
   {
      retro_atomic_store_release_int(&ctx->failed, 1);
      record_gpu_dispatch_d3d12_cancel(ctx->job);
      return;
   }
   info->recording_gpu_timestamp = ctx->next_timestamp;
   ctx->next_timestamp += ctx->duration;
   info->recording_gpu_capture = true;
}

bool record_nvenc_gpu_reap(void *data)
{
   record_nvenc_context_t *ctx = (record_nvenc_context_t*)data;
   return !ctx || record_gpu_dispatch_d3d12_video_reap(ctx->job);
}

void record_nvenc_gpu_capture(void *data, void *source, unsigned state,
      unsigned pos, unsigned dims, uint64_t timestamp)
{
   record_nvenc_context_t *ctx = (record_nvenc_context_t*)data;
   int result, stop;
   uint64_t pts;
   if (!ctx) return;
   stop = retro_atomic_load_acquire_int(&ctx->stop);
   if ((stop && stop != 2)
         || retro_atomic_load_acquire_int(&ctx->failed)) return;
   if (dims != ctx->dims || VIDEO_POS_X(pos) < 0 || VIDEO_POS_Y(pos) < 0)
   {
      retro_atomic_cas_int(&ctx->stop, 0, 1);
      return;
   }
   pts = ctx->audio_enabled ? timestamp
      : ctx->have_origin ? timestamp - ctx->origin : 0;
   result = record_gpu_dispatch_d3d12_video(ctx->job, source, state,
         VIDEO_POS_X(pos), VIDEO_POS_Y(pos), NULL, 0, pts, ctx->duration,
         !ctx->have_origin, NULL);
   if (result == RECORD_GPU_QUEUE_OK)
   {
      if (!ctx->have_origin)
      {
         ctx->have_origin = true;
         ctx->origin = timestamp;
      }
      ctx->captured++;
   }
   else if (result == RECORD_GPU_QUEUE_ERROR || !ctx->allow_drop)
   {
      retro_atomic_store_release_int(&ctx->failed, 1);
      record_gpu_dispatch_d3d12_cancel(ctx->job);
   }
}

void record_nvenc_stop(void)
{
   if (record_nvenc_active || record_nvenc_start_requested)
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);
}

bool record_nvenc_pending(void)
{
   return record_nvenc_start_requested || record_nvenc_active || record_nvenc_retired;
}

bool record_nvenc_defer_start(void)
{
   record_nvenc_start_requested = true;
   return true;
}

bool record_nvenc_cancel_start(void)
{
   bool requested = record_nvenc_start_requested;
   record_nvenc_start_requested = false;
   return requested;
}

void record_nvenc_poll(void)
{
   record_nvenc_context_t **link = &record_nvenc_retired;
   video_driver_state_t *video = video_state_get_ptr();
   if (record_nvenc_start_requested)
   {
      struct video_viewport vp;
      memset(&vp, 0, sizeof(vp));
      if (!recording_state_get_ptr()->enable)
         record_nvenc_start_requested = false;
      else if (video_driver_get_viewport_info(&vp) && VIDEO_SCALE_W(vp.dims)
            && VIDEO_SCALE_H(vp.dims))
      {
         record_nvenc_start_requested = false;
         command_event(CMD_EVENT_RECORD_INIT, NULL);
      }
   }
   if (record_nvenc_active)
   {
      video_device_info_t device;
      record_nvenc_context_t *ctx = record_nvenc_active;
      enum record_gpu_dispatch_state state = record_gpu_dispatch_d3d12_status(ctx->job, NULL, NULL, NULL);
      if (state == RECORD_GPU_DISPATCH_READY) ctx->armed = true;
      if (!video_driver_get_device(&device) || device.device != ctx->device)
         retro_atomic_cas_int(&ctx->stop, 0, 3);
      if (retro_atomic_load_acquire_int(&ctx->stop)
            || retro_atomic_load_acquire_int(&ctx->failed)
            || state >= RECORD_GPU_DISPATCH_DRAINING)
      {
         if (retro_atomic_load_acquire_int(&ctx->stop) == 1)
            RARCH_WARN("[NVENC] %s\n", msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE));
         record_nvenc_stop();
      }
   }
   while (*link)
   {
      record_nvenc_context_t *ctx = *link;
      enum record_gpu_dispatch_state state;
      enum record_gpu_session_error error;
      uint32_t file_error, system_error;
      if (!ctx->video_closed)
      {
         if (video_driver_thread_wrapper_active()
               && video_thread_record_gpu_in_flight(video->data, ctx))
         {
            link = &ctx->next;
            continue;
         }
         if (!record_gpu_dispatch_d3d12_video_finish(ctx->job))
         {
            link = &ctx->next;
            continue;
         }
         ctx->video_closed = true;
      }
      state = record_gpu_dispatch_d3d12_status(ctx->job, &error, &file_error, &system_error);
      if ((state != RECORD_GPU_DISPATCH_DONE && state != RECORD_GPU_DISPATCH_FAILED)
            || !record_gpu_dispatch_d3d12_free(ctx->job))
      {
         link = &ctx->next;
         continue;
      }
      if (state == RECORD_GPU_DISPATCH_DONE && ctx->captured)
      {
         RARCH_LOG("[NVENC] Recording finalized (%llu GPU frames).\n", (unsigned long long)ctx->captured);
         if (ctx->audio_enabled)
            RARCH_LOG("[NVENC] Audio finalized (%llu PCM frames).\n",
                  (unsigned long long)ctx->audio_frames);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
         if (config_get_ptr()->bools.history_list_enable)
         {
            struct playlist_entry entry = {0};
            entry.path = ctx->path; entry.core_path = (char*)"builtin";
            entry.core_name = (char*)"movieplayer";
            command_playlist_push_write(g_defaults.video_history, &entry);
         }
#endif
      }
      else
         RARCH_ERR("[NVENC] Recording failed (session %u, file %u, system %u).\n", (unsigned)error, file_error, system_error);
      *link = ctx->next; free(ctx);
   }
}
#else
const record_driver_t record_nvenc = { NULL, NULL, NULL, NULL, NULL, "nvenc" };
void record_nvenc_frame_info(struct video_frame_info *info, bool capture)
{
   (void)capture;
   info->recording_gpu = NULL; info->recording_gpu_timestamp = 0;
   info->recording_gpu_pending = info->recording_gpu_capture = false;
}
void record_nvenc_poll(void) { }
void record_nvenc_stop(void) { }
bool record_nvenc_pending(void) { return false; }
bool record_nvenc_defer_start(void) { return false; }
bool record_nvenc_cancel_start(void) { return false; }
bool record_nvenc_gpu_reap(void *data) { (void)data; return true; }
void record_nvenc_gpu_capture(void *data, void *source, unsigned state,
      unsigned pos, unsigned dims, uint64_t timestamp)
{ (void)data; (void)source; (void)state; (void)pos; (void)dims; (void)timestamp; }
#endif
