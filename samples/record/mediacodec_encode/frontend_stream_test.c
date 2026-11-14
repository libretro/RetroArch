/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HAVE_NETWORKING
#define HAVE_CRYPTO
#define HAVE_RTMP
#define HAVE_RFLV
#define main file_frontend_acceptance
#include "frontend_test.c"
#undef main
static void stream_case(unsigned mode, const char *key, const char *value, bool menu)
{
   record_mc_context_t *ctx;
   struct record_params p;
   static video_frame_info_t info;
   static int16_t samples[32];
   struct record_audio_data audio;
   unsigned writes = history_writes, calls = stream_dispatches;
   reset(mode);
   recording.streaming_enable = true;
   p = parameters(); p.filename = "rtmp://localhost/live/test"; p.menu_source = menu;
   if (key)
   {
      option_key = key; option_value = value; p.config = "options";
   }
   ctx = (record_mc_context_t*)record_mediacodec.init(&p);
   assert(ctx && ctx->streaming && !ctx->path && stream_dispatches == calls + 1);
   recording.data = ctx; recording.driver = &record_mediacodec;
   arm(ctx);
   record_mediacodec_frame_info(&info, true);
   assert(info.recording_gpu_capture && ctx->offered);
   if (ctx->channels && !menu)
   {
      audio.data = samples; audio.frames = 16;
      assert(record_mediacodec.push_audio(ctx, &audio));
      assert(expected_audio == 16);
   }
   if (key && !strcmp(key, "rtmp_legacy_commands"))
      assert(stream_observed.legacy_commands);
   if (key && !strcmp(key, "audio_bit_rate"))
      assert(stream_audio_bitrate == 192000);
   if (key && !strcmp(key, "audio_enable"))
      assert(!ctx->channels && !stream_audio_bitrate);
   drain(false);
   assert(history_writes == writes);
}
int main(void)
{
   struct record_params p;
   record_mc_context_t *ctx;
   unsigned i, before;
   static const char *const keys[] = {"format","acodec","audio_bit_rate","rtmp_legacy_commands","sample_rate","unknown"};
   static const char *const values[] = {"mp4","opus","1","bad","48000","yes"};
   assert(file_frontend_acceptance() == 0);
   for (i = 0; i < 3; i++) stream_case(i, NULL, NULL, false);
   stream_case(0, "format", "flv", false);
   stream_case(0, "acodec", "aac", false);
   stream_case(1, "audio_bit_rate", "192000", false);
   stream_case(0, "rtmp_legacy_commands", "true", false);
   stream_case(0, "audio_enable", "false", false);
   stream_case(0, NULL, NULL, true);
   for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
   {
      reset(0); recording.streaming_enable = true;
      p = parameters(); p.filename = "rtmp://localhost/live/test"; p.config = "options";
      option_key = keys[i]; option_value = values[i]; before = config_creates;
      assert(!record_mediacodec.init(&p) && config_creates == before);
   }
   reset(0); recording.streaming_enable = true;
   p = parameters(); p.filename = "udp://localhost/test";
   before = config_creates;
   assert(!record_mediacodec.init(&p) && config_creates == before);
   p.filename = "rtmps://localhost/live/test";
#ifdef HAVE_SSL
   ctx = (record_mc_context_t*)record_mediacodec.init(&p); assert(ctx);
   recording.data = ctx; recording.driver = &record_mediacodec;
   arm(ctx); drain(false);
#else
   (void)ctx;
   assert(!record_mediacodec.init(&p) && config_creates == before);
#endif
   reset(0); recording.streaming_enable = true; io_fault = 1;
   p = parameters(); p.filename = "rtmp://localhost/live/test";
   before = config_frees;
   assert(!record_mediacodec.init(&p) && config_frees == before + 1);
   puts("PASS: streaming frontend routing, HDR source metadata, native configuration, video-only/menu audio, TLS admission, history exclusion and startup rollback; platform boundaries mocked");
   return 0;
}
