/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../../record/drivers/record_mediacodec.c"

struct record_dispatch_mediacodec
{
   bool live, cancelled, audio_closed, gpu_closed, claimed;
   enum record_dispatch_mediacodec_state state;
};
struct record_android_capture_config
{
   bool live;
};
struct record_android_capture_request
{
   record_android_capture_renderer_t binding;
   enum record_android_capture_operation op;
   bool live, created, ready, busy, closed, collected;
   int result;
   int64_t end;
};
static struct record_dispatch_mediacodec io;
static struct record_android_capture_config configuration;
static struct record_android_capture_request request;
static record_android_capture_pool_config_t pool_config;
static settings_t settings;
static recording_state_t recording;
static video_driver_state_t video;
static runloop_state_t loop;
static int64_t wall_us;
static record_audio_tap_t *active_tap;
static bool tap_busy;
static video_device_info_t device;
static struct retro_hdr_metadata metadata;
static unsigned dims, io_creates, config_creates, config_frees, io_frees;
static unsigned audio_calls, stops, starts, logs, failures;
static unsigned config_fault, io_fault, request_fault;
static bool async_audio, held_frames, producer_ready, native_retired;
static uint64_t expected_audio, closed_audio;
static int audio_result;
static mediacodec_encoder_request_t encoded;
static const char *option_key, *option_value;
static int config_token;
static unsigned history_writes;
static int native_error, native_file_error, native_system_error;
static bool settings_available = true;
#ifdef HAVE_WEBMPLAYER
struct defaults g_defaults;
static int playlist_token;
void command_playlist_push_write(
      playlist_t *list, const struct playlist_entry *entry)
{
   assert(list == (playlist_t *)&playlist_token
         && list == g_defaults.video_history);
   assert(!io.live && !request.live && configuration.live);
   assert(entry && entry->path && !strcmp(entry->path, "test.mp4"));
   assert(!strcmp(entry->core_path, "builtin")
         && !strcmp(entry->core_name, "movieplayer"));
   history_writes++;
}
#endif

runloop_state_t *runloop_state_get_ptr(void) { return &loop; }
retro_time_t cpu_features_get_time_usec(void) { return wall_us; }
uint64_t cpu_features_get(void) { return 0; }
int audio_driver_record_tap_install(struct record_audio_tap *tap)
{
   if (tap_busy) return 0;
   if (active_tap == tap) return 1;
   if (active_tap || !record_audio_tap_begin(tap)) return -1;
   active_tap = tap;
   return 1;
}
bool audio_driver_record_tap_remove(struct record_audio_tap *tap)
{
   if (tap_busy) return false;
   if (active_tap == tap)
   {
      active_tap = NULL;
      record_audio_tap_retire(tap, false);
   }
   return record_audio_tap_end(tap, NULL);
}
settings_t *config_get_ptr(void)
{
   return settings_available ? &settings : NULL;
}
recording_state_t *recording_state_get_ptr(void)
{
   return &recording;
}
video_driver_state_t *video_state_get_ptr(void)
{
   return &video;
}
bool audio_driver_has_callback(void)
{
   return async_audio;
}
unsigned audio_layout_channels(uint32_t layout)
{
   unsigned count = 0;
   while (layout)
   {
      count += layout & 1;
      layout >>= 1;
   }
   return count;
}
bool video_driver_get_device(video_device_info_t *out)
{
   *out = device;
   return device.device != NULL;
}
bool video_driver_get_display_metadata(unsigned command, void *out)
{
   assert(command == RETRO_ENVIRONMENT_GET_HDR_METADATA);
   *(struct retro_hdr_metadata *)out = metadata;
   return true;
}
static bool menu_view_only;
bool video_driver_get_viewport_info(struct video_viewport *out)
{
   out->dims = menu_view_only ? 0 : dims;
   out->full_dims = dims;
   return true;
}
void RARCH_LOG(const char *format, ...)
{
   assert(strstr(format, "Finalized"));
   logs++;
}
void RARCH_ERR(const char *format, ...)
{
   assert(strstr(format, "Failed"));
   failures++;
}
bool command_event(enum event_command command, void *unused)
{
   assert(!unused);
   if (command == CMD_EVENT_RECORD_DEINIT)
   {
      stops++;
      if (record_mediacodec_cancel_start())
         return true;
      assert(recording.data && record_mc_active);
      record_mediacodec.finalize(recording.data);
      record_mediacodec.free(recording.data);
      recording.data = NULL;
      recording.driver = NULL;
   }
   else
   {
      assert(command == CMD_EVENT_RECORD_INIT);
      starts++;
   }
   return true;
}
config_file_t *config_file_new_from_path_to_string(const char *path)
{
   assert(!strcmp(path, "options"));
   return (config_file_t *)&config_token;
}
void config_file_free(config_file_t *config)
{
   assert(config == (config_file_t *)&config_token);
}
struct config_entry_list *config_get_entry(
      const config_file_t *config, const char *key)
{
   assert(config == (const config_file_t *)&config_token);
   return option_key && !strcmp(key, option_key)
         ? (struct config_entry_list *)&config_token
         : NULL;
}
bool config_get_entry_list_head(
      config_file_t *config, struct config_file_entry *entry)
{
   assert(config == (config_file_t *)&config_token);
   entry->key = option_key;
   return option_key != NULL;
}
bool config_get_entry_list_next(struct config_file_entry *entry)
{
   (void)entry;
   return false;
}
bool config_get_array(
      config_file_t *config, const char *key, char *out, size_t size)
{
   assert(config_get_entry(config, key));
   if (strlen(option_value) >= size)
      return false;
   memcpy(out, option_value, strlen(option_value) + 1);
   return true;
}
bool config_get_bool(config_file_t *config, const char *key, bool *out)
{
   assert(config_get_entry(config, key));
   if (strcmp(option_value, "true") && strcmp(option_value, "false"))
      return false;
   *out = !strcmp(option_value, "true");
   return true;
}
bool config_get_uint(config_file_t *config, const char *key, unsigned *out)
{
   char *end;
   unsigned long value;
   assert(config_get_entry(config, key));
   value = strtoul(option_value, &end, 10);
   if (*end || value > UINT_MAX)
      return false;
   *out = (unsigned)value;
   return true;
}
record_android_capture_config_t *record_android_capture_config_new(
      const struct video_device_info *d, unsigned w, unsigned h, unsigned count,
      unsigned mode, bool flip)
{
   assert(d->device == device.device && w == VIDEO_SCALE_W(dims)
         && h == VIDEO_SCALE_H(dims) && count == 4
         && mode == metadata.output_mode && flip && !configuration.live);
   config_creates++;
   if (config_fault)
      return NULL;
   configuration.live = true;
   return &configuration;
}
const record_android_capture_pool_config_t *record_android_capture_config_pool(
      const record_android_capture_config_t *c)
{
   assert(c == &configuration && c->live);
   return &pool_config;
}
void record_android_capture_config_free(record_android_capture_config_t *c)
{
   assert(c == &configuration && c->live && !request.live && !io.live);
   config_frees++;
   c->live = false;
}
record_dispatch_mediacodec_t *record_dispatch_mediacodec_new(
      const mediacodec_encoder_request_t *v, unsigned interval, unsigned rate,
      unsigned channels, unsigned bitrate, size_t max_frames, size_t capacity,
      const char *path)
{
   assert(!io.live && configuration.live && interval == 2
         && !strcmp(path, "test.mp4"));
   assert((!rate && !channels && !bitrate && !max_frames && !capacity)
         || (rate == 48000 && (channels == 1 || channels == 2)
               && bitrate == 128000 && max_frames == 4096
               && capacity >= (size_t)48000 * channels * 4));
   io_creates++;
   encoded = *v;
   if (io_fault)
      return NULL;
   io.live = true;
   io.state = RECORD_MC_DISPATCH_STARTING;
   return &io;
}
int record_dispatch_mediacodec_audio(record_dispatch_mediacodec_t *j,
      const int16_t *samples, size_t frames, uint64_t pts)
{
   assert(j == &io && j->live && !j->audio_closed && samples && frames <= 4096);
   assert(pts == expected_audio);
   audio_calls++;
   if (audio_result == RECORD_AUDIO_OK)
      expected_audio += frames;
   return audio_result;
}
bool record_dispatch_mediacodec_audio_finish(
      record_dispatch_mediacodec_t *j, uint64_t end)
{
   assert(j == &io && j->live && end == expected_audio);
   if (j->audio_closed)
      assert(closed_audio == end);
   closed_audio = end;
   j->audio_closed = true;
   return true;
}
void record_dispatch_mediacodec_cancel(record_dispatch_mediacodec_t *j)
{
   assert(j == &io && j->live);
   j->cancelled = true;
}
bool record_dispatch_mediacodec_cancelled(const record_dispatch_mediacodec_t *j)
{
   assert(j == &io && j->live);
   return j->cancelled;
}
enum record_dispatch_mediacodec_state record_dispatch_mediacodec_status(
      const record_dispatch_mediacodec_t *j, int *error, int *file, int *system)
{
   assert(j == &io && j->live);
   if (error)
      *error = native_error;
   if (file)
      *file = native_file_error;
   if (system)
      *system = native_system_error;
   return j->state;
}
bool record_dispatch_mediacodec_gpu_claim(record_dispatch_mediacodec_t *j)
{
   assert(j == &io && j->live);
   if (j->claimed || j->gpu_closed)
      return false;
   j->claimed = true;
   return true;
}
bool record_dispatch_mediacodec_gpu_retired(
      record_dispatch_mediacodec_t *j, int64_t end)
{
   assert(j == &io && j->claimed && end == 0);
   return true;
}
bool record_dispatch_mediacodec_gpu_released(record_dispatch_mediacodec_t *j)
{
   assert(j == &io && j->claimed);
   j->gpu_closed = true;
   return true;
}
void record_dispatch_mediacodec_gpu_release_claim(
      record_dispatch_mediacodec_t *j)
{
   assert(j == &io && j->claimed);
   j->claimed = false;
}
bool record_dispatch_mediacodec_free(record_dispatch_mediacodec_t *j)
{
   assert(j == &io && j->live && !request.live && j->gpu_closed
         && j->audio_closed && !j->claimed);
   assert(j->state == RECORD_MC_DISPATCH_DONE
         || j->state == RECORD_MC_DISPATCH_FAILED);
   io_frees++;
   j->live = false;
   return true;
}
record_android_capture_request_t *record_android_capture_request_new(
      record_dispatch_mediacodec_t *j,
      const record_android_capture_pool_config_t *config, unsigned duration,
      bool allow_drop, bool menu_source)
{
   assert(j == &io && config == &pool_config && duration == 16683 && !allow_drop
         && !request.live);
   if (request_fault)
      return NULL;
   request.live = true;
   request.binding.flags = menu_source ? RECORD_ANDROID_CAPTURE_MENU : 0;
   return &request;
}
int record_android_capture_request_result(
      const record_android_capture_request_t *r)
{
   assert(r == &request && r->live);
   return r->busy ? 0 : r->result;
}
const record_android_capture_renderer_t *record_android_capture_request_binding(
      const record_android_capture_request_t *r)
{
   assert(r == &request && r->live);
   return r->ready && !r->closed ? &r->binding : NULL;
}
bool record_android_capture_request_post(record_android_capture_request_t *r,
      enum record_android_capture_operation op, int64_t end)
{
   assert(r == &request && r->live);
   if (r->busy || (op >= RECORD_ANDROID_CAPTURE_FINISH && held_frames))
      return false;
   if (op >= RECORD_ANDROID_CAPTURE_FINISH)
   {
      if (r->closed)
         assert(end == r->end);
      r->closed = true;
      r->ready = false;
      r->end = end;
   }
   r->op = op;
   r->busy = true;
   r->result = 0;
   return true;
}
bool record_android_capture_request_free(record_android_capture_request_t *r)
{
   assert(r == &request && r->live);
   if (r->busy || held_frames || (r->created && !r->collected))
      return false;
   r->live = false;
   io.gpu_closed = true;
   return true;
}
static void ack(int result)
{
   assert(request.busy);
   if (request.op == RECORD_ANDROID_CAPTURE_CREATE)
   {
      request.created = true;
      request.ready = result == 1;
   }
   if (request.op == RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE && result == 1)
      assert(producer_ready);
   if (request.op == RECORD_ANDROID_CAPTURE_COLLECT && result == 1)
   {
      assert(native_retired);
      request.collected = true;
   }
   request.result = result;
   request.busy = false;
}
static struct record_params parameters(void)
{
   struct record_params p;
   memset(&p, 0, sizeof(p));
   p.fps = 60000.0 / 1001;
   p.samplerate = 48000;
   p.filename = "test.mp4";
   p.out_dims = dims;
   p.video_gpu_record = true;
   p.channels = 2;
   return p;
}
static void reset(unsigned mode)
{
   assert(!record_mediacodec_pending() && !configuration.live && !io.live
         && !active_tap);
   memset(&settings, 0, sizeof(settings));
   memset(&recording, 0, sizeof(recording));
   memset(&video, 0, sizeof(video));
   memset(&device, 0, sizeof(device));
   memset(&loop, 0, sizeof(loop));
   loop.current_core_type = CORE_TYPE_DUMMY;
   wall_us = 100000;
   tap_busy = false;
   memset(&metadata, 0, sizeof(metadata));
   memset(&request, 0, sizeof(request));
   memset(&io, 0, sizeof(io));
   device.device = &video;
   device.api = VIDEO_DEVICE_VULKAN;
   dims = VIDEO_SCALE_PACK(1920, 1080);
   metadata.version = 1;
   metadata.size = sizeof(metadata);
   metadata.output_mode = mode;
   metadata.color_primaries = mode == 1 ? 9 : 1;
   metadata.transfer_function = mode == 1 ? 16 : 8;
   metadata.max_cll = 1000;
   metadata.max_fall = 300;
   video.av_info.timing.sample_rate = 48000;
   recording.layout = AUDIO_LAYOUT_STEREO;
   recording.enable = true;
   async_audio = held_frames = producer_ready = native_retired = false;
   config_fault = io_fault = request_fault = 0;
   expected_audio = closed_audio = 0;
   audio_calls = 0;
   native_error = native_file_error = native_system_error = 0;
   settings_available = true;
#ifdef HAVE_WEBMPLAYER
   settings.bools.history_list_enable = true;
   g_defaults.video_history = (playlist_t *)&playlist_token;
#endif
   audio_result = RECORD_AUDIO_OK;
   option_key = option_value = NULL;
}
static record_mc_context_t *start(void)
{
   struct record_params p = parameters();
   record_mc_context_t *ctx = (record_mc_context_t *)record_mediacodec.init(&p);
   assert(ctx && !ctx->armed && record_mediacodec_pending());
   recording.data = ctx;
   recording.driver = &record_mediacodec;
   return ctx;
}
static void arm(record_mc_context_t *ctx)
{
   record_mediacodec_poll();
   assert(request.busy && request.op == RECORD_ANDROID_CAPTURE_CREATE);
   ack(0);
   record_mediacodec_poll();
   assert(!ctx->armed && request.busy);
   io.state = RECORD_MC_DISPATCH_READY;
   ack(1);
   record_mediacodec_poll();
   assert(ctx->armed && !ctx->next_us && !ctx->audio_frames);
}
static void drain(bool failed)
{
   unsigned i, frees = config_frees, writes = history_writes;
   if (record_mc_active)
      record_mediacodec_stop();
   assert(record_mc_retired && !record_mc_active);
   held_frames = true;
   for (i = 0; i < 100; i++)
      record_mediacodec_poll();
   assert(record_mc_retired && config_frees == frees && !request.busy);
   assert(history_writes == writes);
   held_frames = false;
   record_mediacodec_poll();
   assert(request.busy && request.op == RECORD_ANDROID_CAPTURE_COLLECT);
   ack(0);
   record_mediacodec_poll();
   assert(request.busy && config_frees == frees);
   assert(history_writes == writes);
   native_retired = true;
   ack(1);
   io.state = failed ? RECORD_MC_DISPATCH_FAILED : RECORD_MC_DISPATCH_DONE;
   record_mediacodec_poll();
   assert(!record_mediacodec_pending() && config_frees == frees + 1);
}
static void clocks(unsigned mode)
{
   static int16_t pcm[10000];
   record_mc_context_t *ctx;
   struct record_audio_data audio;
   static video_frame_info_t info;
   unsigned i, writes = history_writes;
   reset(mode);
   ctx = start();
   assert(encoded.color == (mode != 0) && encoded.profile == (mode ? 4096 : 1));
   assert(encoded.level == (mode ? 4096 : 8192));
   if (mode)
   {
      assert(encoded.hdr_static_size == 25);
      for (i = 0; i < 21; i++)
         assert(!encoded.hdr_static[i]);
      assert(encoded.hdr_static[21] == 232 && encoded.hdr_static[22] == 3);
      assert(encoded.hdr_static[23] == 44 && encoded.hdr_static[24] == 1);
   }
   audio.data = pcm;
   audio.frames = 5000;
   assert(record_mediacodec.push_audio(ctx, &audio) && !audio_calls);
   arm(ctx);
   for (i = 0; i < 60000; i++)
   {
      record_mediacodec_frame_info(&info, true);
      assert(info.recording_gpu == &request.binding
            && info.recording_gpu_capture);
      assert(info.recording_gpu_timestamp
            == (uint64_t)i * 1000000 * 1001 / 60000);
   }
   assert(ctx->next_us == (uint64_t)1001000000);
   record_mediacodec_frame_info(&info, false);
   assert(!info.recording_gpu_capture && ctx->next_us == (uint64_t)1001000000);
   video.frame_repeat = true;
   record_mediacodec_frame_info(&info, true);
   assert(!info.recording_gpu_capture && ctx->next_us == (uint64_t)1001000000);
   video.frame_repeat = false;
   assert(record_mediacodec.push_audio(ctx, &audio));
   assert(audio_calls == 2 && expected_audio == 5000
         && ctx->audio_frames == 5000);
   drain(false);
   assert(closed_audio == 5000);
#ifdef HAVE_WEBMPLAYER
   assert(history_writes == writes + 1);
#else
   assert(history_writes == writes);
#endif
}
static void reset_drains(void)
{
   record_mc_context_t *ctx;
   static video_frame_info_t info;
   unsigned before = failures;
   reset(0);
   ctx = start();
   arm(ctx);
   record_mediacodec_frame_info(&info, true);
   record_mediacodec_stop();
   assert(record_mediacodec_reset_pending());
   record_mediacodec_poll();
   assert(request.busy && request.op == RECORD_ANDROID_CAPTURE_COLLECT);
   assert(!io.cancelled);
   ack(0);
   record_mediacodec_poll();
   assert(record_mediacodec_reset_pending() && !io.cancelled);
   native_retired = true;
   ack(1);
   record_mediacodec_poll();
   assert(!record_mediacodec_reset_pending() && record_mediacodec_pending());
   assert(!request.live && configuration.live && !io.cancelled);
   io.state = RECORD_MC_DISPATCH_DONE;
   record_mediacodec_poll();
   assert(!record_mediacodec_pending() && failures == before);
}
static void reset_lifecycle(void)
{
   record_mc_context_t *ctx;
   unsigned i, frees = config_frees;
   reset(0);
   ctx = start();
   arm(ctx);
   record_mc_fail(ctx);
   record_mediacodec_stop();
   assert(record_mediacodec_reset_pending());
   record_mediacodec_poll();
   assert(request.busy && request.op == RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE);
   ack(0);
   record_mediacodec_poll();
   assert(record_mediacodec_reset_pending());
   producer_ready = true;
   ack(1);
   record_mediacodec_poll();
   assert(!record_mediacodec_reset_pending() && record_mediacodec_pending());
   assert(request.busy && config_frees == frees);
   for (i = 0; i < 100; i++)
   {
      ack(0);
      record_mediacodec_poll();
      assert(record_mediacodec_pending() && !record_mediacodec_reset_pending());
   }
   device.device = &settings;
   native_retired = true;
   ack(1);
   record_mediacodec_poll();
   assert(!request.live && record_mediacodec_pending() && configuration.live);
   io.state = RECORD_MC_DISPATCH_FAILED;
   record_mediacodec_poll();
   assert(!record_mediacodec_pending() && config_frees == frees + 1);
}
static void audio_faults(void)
{
   static int16_t pcm[2];
   record_mc_context_t *ctx;
   struct record_audio_data audio;
   unsigned fault;
   audio.data = pcm;
   audio.frames = 1;
   for (fault = 0; fault < 3; fault++)
   {
      reset(0);
      ctx = start();
      arm(ctx);
      if (fault == 0)
         audio_result = RECORD_AUDIO_BUSY;
      if (fault == 1)
         video.av_info.timing.sample_rate = 44100;
      if (fault == 2)
         ctx->audio_frames = ~(uint64_t)0;
      assert(!record_mediacodec.push_audio(ctx, &audio) && ctx->failed
            && io.cancelled);
      if (fault == 2)
         ctx->audio_frames = 0;
      drain(true);
   }
}
static void startup_faults(void)
{
   unsigned fault;
   for (fault = 0; fault < 3; fault++)
   {
      struct record_params p;
      reset(0);
      p = parameters();
      config_fault = fault == 0;
      io_fault = fault == 1;
      request_fault = fault == 2;
      assert(!record_mediacodec.init(&p));
      if (fault == 2)
      {
         assert(record_mediacodec_pending() && io.cancelled);
         record_mediacodec_poll();
         assert(io.gpu_closed && io.audio_closed && configuration.live);
         io.state = RECORD_MC_DISPATCH_FAILED;
         record_mediacodec_poll();
      }
      assert(!record_mediacodec_pending() && !configuration.live && !io.live);
   }
}
static void output_faults(void)
{
   unsigned fault;
   for (fault = 0; fault < 4; fault++)
   {
      record_mc_context_t *ctx;
      reset(0);
      ctx = start();
      arm(ctx);
      if (fault == 0)
         dims = VIDEO_SCALE_PACK(1280, 720);
      if (fault == 1)
         metadata.output_mode = 1;
      if (fault == 2)
         device.device = &settings;
      if (fault == 3)
         io.cancelled = true;
      record_mediacodec_poll();
      assert(!record_mc_active && io.cancelled && record_mc_retired);
      if (request.busy)
         ack(0);
      drain(true);
   }
}
static void video_only(void)
{
   unsigned writes = history_writes;
   struct record_params p;
   record_mc_context_t *ctx;
   struct record_audio_data audio;
   reset(0);
   p = parameters();
   p.config = "options";
   option_key = "audio_enable";
   option_value = "false";
   ctx = (record_mc_context_t *)record_mediacodec.init(&p);
   assert(ctx && !ctx->channels && !ctx->rate);
   recording.data = ctx;
   recording.driver = &record_mediacodec;
   arm(ctx);
   audio.data = NULL;
   audio.frames = 300;
   assert(record_mediacodec.push_audio(ctx, &audio) && !audio_calls);
   drain(false);
   assert(history_writes == writes);
}
static void menu_capture(void)
{
   static int16_t mixed[10000];
   static video_frame_info_t info;
   struct record_params p;
   struct record_audio_data silence;
   record_mc_context_t *ctx;
   uint64_t end;
   unsigned writes = history_writes;
   reset(0);
   p = parameters();
   p.menu_source = true;
   assert(recording_is_native_gpu(&record_mediacodec));
   video.av_info.timing.sample_rate = 44100;
   ctx = (record_mc_context_t *)record_mediacodec.init(&p);
   assert(ctx && ctx->tap);
   recording.data = ctx;
   recording.driver = &record_mediacodec;
   record_mediacodec_poll();
   io.state = RECORD_MC_DISPATCH_READY;
   ack(1);
   tap_busy = true;
   record_mediacodec_poll();
   assert(!ctx->armed && !active_tap && !expected_audio);
   tap_busy = false;
   record_mediacodec_poll();
   assert(ctx->armed && active_tap == ctx->tap);
   assert(!record_audio_tap_end(ctx->tap, &end));
   silence.data = mixed; silence.frames = 5000;
   assert(record_mediacodec.push_audio(ctx, &silence) && !audio_calls);
   assert(record_audio_tap_push(active_tap, mixed, 5000, false, 48000));
   assert(audio_calls == 2 && expected_audio == 5000);
   video.frame_repeat = true;
   record_mediacodec_frame_info(&info, true);
   assert(info.recording_gpu_capture && info.recording_gpu_timestamp == 0);
   assert(((record_android_capture_renderer_t *)info.recording_gpu)->flags
         & RECORD_ANDROID_CAPTURE_MENU);
   record_mediacodec_frame_info(&info, true);
   assert(!info.recording_gpu_capture);
   wall_us += 50000;
   record_mediacodec_frame_info(&info, true);
   assert(info.recording_gpu_capture && info.recording_gpu_timestamp == 50000);
   wall_us += 100000;
   tap_busy = true;
   record_mediacodec_stop();
   assert(active_tap == ctx->tap && !ctx->audio_closed && ctx->next_us == 150000);
   record_mediacodec_poll();
   assert(request.busy && !io.audio_closed);
   native_retired = true;
   ack(1);
   record_mediacodec_poll();
   assert(!request.live && io.live && configuration.live);
   assert(history_writes == writes);
   tap_busy = false;
   record_mediacodec_poll();
   assert(!active_tap && closed_audio == 5000 && io.live);
   io.state = RECORD_MC_DISPATCH_DONE;
   record_mediacodec_poll();
   assert(!record_mediacodec_pending());
   assert(video.av_info.timing.sample_rate == 44100);
#ifdef HAVE_WEBMPLAYER
   assert(history_writes == writes + 1);
#else
   assert(history_writes == writes);
#endif
}
static void history(void)
{
   unsigned fault;
   for (fault = 0; fault < 9; fault++)
   {
      char filename[] = "test.mp4";
      struct record_params p;
      record_mc_context_t *ctx;
      static video_frame_info_t info;
      unsigned writes = history_writes;
      reset(0);
      p = parameters();
      p.filename = filename;
      ctx = (record_mc_context_t *)record_mediacodec.init(&p);
      assert(ctx && ctx->path != filename && !strcmp(ctx->path, filename));
      filename[0] = 'X';
      recording.data = ctx;
      recording.driver = &record_mediacodec;
      arm(ctx);
      record_mediacodec_frame_info(&info, true);
      assert(info.recording_gpu_capture && ctx->offered == 1);
#ifdef HAVE_WEBMPLAYER
      if (fault == 1)
         settings.bools.history_list_enable = false;
      if (fault == 2)
         g_defaults.video_history = NULL;
#endif
      if (fault == 3)
         native_error = 1;
      if (fault == 4)
         native_file_error = 1;
      if (fault == 5)
         native_system_error = 1;
      if (fault == 7)
         record_mc_fail(ctx);
      if (fault == 8)
         settings_available = false;
      drain(fault == 6);
#ifdef HAVE_WEBMPLAYER
      assert(history_writes == writes + (fault == 0 ? 1u : 0u));
#else
      assert(history_writes == writes);
#endif
      record_mediacodec_poll();
#ifdef HAVE_WEBMPLAYER
      assert(history_writes == writes + (fault == 0 ? 1u : 0u));
#endif
   }
}
static void level_selection(void)
{
   mediacodec_encoder_request_t v;
   bool audio;
   memset(&v, 0, sizeof(v));
   v.mime = "video/avc";
   v.width = 1920;
   v.height = 1080;
   v.bitrate = 16000000;
   v.frame_rate = 30.0;
   assert(record_mc_level(&v) == 2048);
   v.frame_rate = 60.0;
   assert(record_mc_level(&v) == 8192);
   v.width = 3840;
   v.height = 2160;
   assert(record_mc_level(&v) == 65536);
   v.frame_rate = 30.0;
   assert(record_mc_level(&v) == 32768);
   v.width = 7680;
   v.height = 4320;
   assert(record_mc_level(&v) == 131072);
   v.mime = "video/hevc";
   assert(record_mc_level(&v) == 1048576);
   v.width = 1920;
   v.height = 1080;
   assert(record_mc_level(&v) == 4096);
   v.bitrate = 12000000;
   assert(record_mc_level(&v) == 1024);
   v.bitrate++;
   assert(record_mc_level(&v) == 4096);
   v.frame_rate = 60.0;
   v.bitrate = 20000000;
   assert(record_mc_level(&v) == 4096);
   v.frame_rate = 60.0001;
   assert(record_mc_level(&v) == 4096);
   v.frame_rate = 65.0;
   assert(record_mc_level(&v) == 16384);
   v.width = 3840;
   v.height = 2160;
   v.frame_rate = 60.0;
   assert(record_mc_level(&v) == 65536);
   v.bitrate = 800000000;
   assert(record_mc_level(&v) == 33554432);
   v.bitrate++;
   assert(!record_mc_level(&v));
   v.bitrate = 1;
   v.width = 16384;
   v.height = 2;
   assert(record_mc_level(&v) == 1048576);
   v.width = v.height = 16384;
   assert(!record_mc_level(&v));
   v.width = 0;
   assert(!record_mc_level(&v));
   reset(0);
   option_key = "video_level";
   option_value = "65536";
   assert(record_mc_options("options", &v, &audio));
   assert(v.level == 65536);
}
static void admission(void)
{
   static char overlong_path[PATH_MAX_LENGTH + 1];
   struct record_params p;
   unsigned fault, before;
   const char *keys[] = {"format", "vcodec", "audio_enable", "video_level",
         "video_bit_rate", "unknown"};
   const char *values[] = {"avi", "vp9", "bad", "3", "0", "yes"};
   for (fault = 0; fault < 6; fault++)
   {
      reset(0);
      p = parameters();
      p.config = "options";
      option_key = keys[fault];
      option_value = values[fault];
      before = config_creates;
      assert(!record_mediacodec.init(&p) && config_creates == before);
   }
   reset(1);
   p = parameters();
   p.config = "options";
   option_key = "vcodec";
   option_value = "h264";
   assert(!record_mediacodec.init(&p));
   reset(0);
   p = parameters();
   recording.layout = AUDIO_SPEAKER_FRONT_LEFT | AUDIO_SPEAKER_BACK_LEFT;
   assert(!record_mediacodec.init(&p));
   reset(0);
   p = parameters();
   async_audio = true;
   assert(!record_mediacodec.init(&p));
   reset(0);
   p = parameters();
   p.channels = 6;
   assert(!record_mediacodec.init(&p));
   reset(0);
   p = parameters();
   p.fps = 0;
   assert(!record_mediacodec.init(&p));
   reset(0);
   p = parameters();
   p.out_dims = VIDEO_SCALE_PACK(1919, 1080);
   assert(!record_mediacodec.init(&p));
   reset(0);
   p = parameters();
   memset(overlong_path, 'x', sizeof(overlong_path) - 1);
   overlong_path[sizeof(overlong_path) - 1] = 0;
   p.filename = overlong_path;
   before = config_creates;
   assert(!record_mediacodec.init(&p) && config_creates == before);
   reset(0);
   assert(record_mediacodec_defer_start() && record_mediacodec_pending());
   dims = 0;
   record_mediacodec_poll();
   assert(record_mc_start_requested);
   dims = VIDEO_SCALE_PACK(1920, 1080);
   record_mediacodec_poll();
   assert(!record_mc_start_requested && starts == 1
         && !record_mediacodec_pending());
   menu_view_only = true;
   loop.current_core_type = CORE_TYPE_DUMMY;
   assert(record_mediacodec_defer_start());
   record_mediacodec_poll();
   assert(!record_mc_start_requested && starts == 2);
   menu_view_only = false;
}
int main(void)
{
   clocks(0);
   clocks(1);
   clocks(2);
   reset_drains();
   reset_lifecycle();
   output_faults();
   video_only();
   audio_faults();
   startup_faults();
   history();
   menu_capture();
   level_selection();
   admission();
   assert(io_frees && config_frees && logs >= 3 && failures >= 4 && stops);
   puts("PASS full MediaCodec frontend startup, rational clocks, PCM, closure, "
        "reset readiness and retained native lifetimes; platform boundaries "
        "mocked");
   return 0;
}
