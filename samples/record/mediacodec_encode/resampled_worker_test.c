/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main base_worker_main
#define record_mediacodec_encoder_audio_submit base_audio_submit
#define record_mediacodec_encoder_audio_finish base_audio_finish
#define record_mux_mediacodec_end base_mux_end
#include "worker_test.c"
#undef main
#undef record_mediacodec_encoder_audio_submit
#undef record_mediacodec_encoder_audio_finish
#undef record_mux_mediacodec_end
#include <math.h>
#include <stdlib.h>
#include "../../../record/record_pcm_resample.h"
#define SOURCE_FRAMES 4096
static int16_t source_pcm[SOURCE_FRAMES * 2], received[25000 * 2];
static int16_t reference[25000 * 2];
static int16_t held_pcm[1552 * 2];
static const int16_t *held_pointer;
static size_t held_frames;
static uint64_t expected_end;
static unsigned channels_now;
static double frequency = 997;
static bool bad_end;

int record_mediacodec_encoder_audio_submit(record_mediacodec_encoder_t *e,
      const int16_t *pcm, size_t frames, uint64_t timestamp, size_t *accepted)
{
   size_t count = frames > 17 ? 17 : frames;
   assert(e->audio && !e->eos && timestamp == e->frames && frames <= 1552);
   assert(timestamp + count <= expected_end);
   if (held_pointer)
      assert(held_pointer == pcm && held_frames == frames
            && !memcmp(held_pcm, pcm, frames * channels_now * 2));
   *accepted = 0;
   if (++e->inputs % 3 == 0)
   {
      held_pointer = pcm;
      held_frames = frames;
      memcpy(held_pcm, pcm, frames * channels_now * 2);
      return 0;
   }
   held_pointer = NULL;
   memcpy(received + timestamp * channels_now, pcm, count * channels_now * 2);
   e->frames += (unsigned)count;
   *accepted = count;
   return 1;
}
int record_mediacodec_encoder_audio_finish(record_mediacodec_encoder_t *e,
      uint64_t end)
{
   assert(e->audio && e->frames == expected_end && end == expected_end);
   e->eos = true;
   return 1;
}
bool record_mux_mediacodec_end(record_mux_mediacodec_t *m, bool audio,
      int64_t end)
{
   assert(!m->ends[audio] && end == (audio
            ? (int64_t)(expected_end * 1000000 / 48000) : 33333));
   m->ends[audio]++;
   return true;
}
static void exercise_resampled(unsigned source_rate_twice, unsigned channels,
      bool cancel, bool packetized)
{
   record_mediacodec_encoder_t video = {0}, audio = {0};
   record_mux_mediacodec_t mux = {0};
   record_audio_queue_t *q;
   record_mediacodec_worker_t *w;
   const int16_t *borrow;
   uint64_t timestamp;
   size_t frames;
   unsigned i, at, packet;
   int result = 0;
   double source_rate = source_rate_twice / 2.0;
   expected_end = ((uint64_t)SOURCE_FRAMES * 48000 * 2 + source_rate_twice - 1)
      / source_rate_twice;
   channels_now = channels;
   held_pointer = NULL;
   audio.audio = mux.audio = true;
   q = record_audio_queue_new(channels, SOURCE_FRAMES, 131072);
   assert(q);
   for (i = 0; i < SOURCE_FRAMES; i++)
   {
      source_pcm[i * channels] = (int16_t)(12000
            * sin(6.283185307179586 * frequency * i / source_rate));
      if (channels == 2)
         source_pcm[i * 2 + 1] = -source_pcm[i * 2];
   }
   for (at = 0; at < SOURCE_FRAMES; at += packet)
   {
      packet = packetized ? at % 193 + 1 : SOURCE_FRAMES;
      if (packet > SOURCE_FRAMES - at)
         packet = SOURCE_FRAMES - at;
      assert(record_audio_queue_push(q, source_pcm + at * channels, packet, at) == RECORD_AUDIO_OK);
   }
   assert(record_audio_queue_finish(q));
   w = record_mediacodec_worker_new(&video, &audio, q, &mux, channels, 48000);
   assert(w && record_mediacodec_worker_resample(w, source_rate));
   assert(!record_mediacodec_worker_resample(w, source_rate));
   assert(record_mediacodec_worker_finish(w, 33333, SOURCE_FRAMES + bad_end));
   assert(record_mediacodec_worker_finish(w, 33333, SOURCE_FRAMES + bad_end));
   assert(!record_mediacodec_worker_finish(w, 33333, SOURCE_FRAMES + !bad_end));
   for (i = 0; i < 10000; i++)
   {
      result = record_mediacodec_worker_step(w);
      assert(result != RECORD_MC_WORKER_ERROR || bad_end);
      if (result == RECORD_MC_WORKER_ERROR || result == RECORD_MC_WORKER_END || (cancel && i == 8))
         break;
   }
   assert(i < 10000);
   if (cancel)
      record_mediacodec_worker_abort(w);
   else if (bad_end)
      assert(result == RECORD_MC_WORKER_ERROR);
   else
   {
      assert(result == RECORD_MC_WORKER_END && audio.frames == expected_end);
      for (i = 256; i + 256 < expected_end; i++)
      {
         double expected = frequency > 24000 ? 0
            : 12000 * sin(6.283185307179586 * frequency * i / 48000);
         assert(fabs(received[i * channels] - expected) < 32);
      }
      if (packetized)
         for (i = 0; i < expected_end * channels; i++)
            assert(abs(received[i] - reference[i]) <= 4);
      else
         memcpy(reference, received, (size_t)expected_end * channels * 2);
   }
   assert(record_mediacodec_worker_free(w));
   while (record_audio_queue_peek(q, &borrow, &frames, &timestamp) == RECORD_AUDIO_OK)
      assert(record_audio_queue_ack(q));
   assert(record_audio_queue_free(q));
}
static void converter_edges(void)
{
   record_audio_queue_t *q;
   record_pcm_resample_t *s;
   const int16_t *data;
   static const int16_t pcm[4] = {12000, -12000, 0, 0};
   uint64_t at, source, output;
   size_t frames, count = 0;
   unsigned i;
   int result = 0;
   q = record_audio_queue_new(2, 2, 512);
   assert(q && !record_pcm_resample_new(q, HUGE_VAL, 48000));
   assert(!record_pcm_resample_new(q, 32040.5, 32000));
   s = record_pcm_resample_new(q, 32040.5, 48000);
   assert(s && !record_pcm_resample_free(s));
   assert(record_pcm_resample_duration(s, (uint64_t)64081 * 1000000, &output)
         && output == (uint64_t)96000 * 1000000);
   assert(record_pcm_resample_peek(s, &data, &frames, &at) == RECORD_PCM_BUSY);
   assert(record_pcm_resample_cancel(s) && record_pcm_resample_free(s));
   s = record_pcm_resample_new(q, 47999.99999999999, 48000);
   assert(s && record_pcm_resample_duration(s, (uint64_t)-1 / 48000, &output)
         && output == (uint64_t)-1 / 48000 + 1);
   assert(record_pcm_resample_cancel(s) && record_pcm_resample_free(s));
   assert(record_audio_queue_finish(q));
   s = record_pcm_resample_new(q, 32040.5, 48000);
   assert(s && record_pcm_resample_peek(s, &data, &frames, &at) == RECORD_PCM_END);
   assert(record_pcm_resample_end(s, &source, &output) && !source && !output);
   assert(record_pcm_resample_free(s) && record_audio_queue_free(q));
   q = record_audio_queue_new(2, 2, 512);
   assert(q && record_audio_queue_push(q, pcm, 1, 0) == RECORD_AUDIO_OK);
   assert(record_audio_queue_finish(q));
   s = record_pcm_resample_new(q, 32040.5, 48000);
   assert(s);
   for (i = 0; i < 100; i++)
   {
      result = record_pcm_resample_peek(s, &data, &frames, &at);
      assert(result != RECORD_PCM_ERROR);
      if (result == RECORD_PCM_END)
         break;
      if (result == RECORD_PCM_PACKET)
      {
         assert(at == count);
         count += frames;
         assert(record_pcm_resample_ack(s));
      }
   }
   assert(result == RECORD_PCM_END && count == 2);
   assert(record_pcm_resample_end(s, &source, &output) && source == 1 && output == 2);
   assert(record_pcm_resample_free(s) && record_audio_queue_free(q));
   q = record_audio_queue_new(2, 2, 512);
   assert(q && record_audio_queue_push(q, pcm, 2, 0) == RECORD_AUDIO_OK);
   assert(record_audio_queue_push(q, pcm, 2, 3) == RECORD_AUDIO_OK);
   assert(record_audio_queue_finish(q));
   s = record_pcm_resample_new(q, 32040.5, 48000);
   assert(s);
   assert(record_pcm_resample_peek(s, &data, &frames, &at) == RECORD_PCM_PROGRESS);
   assert(record_pcm_resample_peek(s, &data, &frames, &at) == RECORD_PCM_ERROR);
   assert(record_pcm_resample_cancel(s) && record_pcm_resample_free(s));
   assert(record_audio_queue_free(q));
}
static int durations(void)
{
   double rate;
   unsigned encoded_rate, high, low;
   while (scanf("%lf %u %u %u", &rate, &encoded_rate, &high, &low) == 4)
   {
      record_audio_queue_t *q = record_audio_queue_new(2, 2, 512);
      record_pcm_resample_t *s = record_pcm_resample_new(q, rate, encoded_rate);
      uint64_t output, input = ((uint64_t)high << 32) | low;
      assert(q && s && record_pcm_resample_duration(s, input, &output));
      printf("%u %u\n", (unsigned)(output >> 32), (unsigned)output);
      assert(record_pcm_resample_cancel(s) && record_pcm_resample_free(s));
      assert(record_audio_queue_free(q));
   }
   return 0;
}
int main(int argc, char **argv)
{
   static const unsigned rates[] = {16000, 64081, 88201, 192000, 384000};
   unsigned i, ch;
   if (argc == 2 && !strcmp(argv[1], "--durations"))
      return durations();
   assert(argc == 1);
   converter_edges();
   for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++)
      for (ch = 1; ch <= 2; ch++)
      {
         exercise_resampled(rates[i], ch, false, false);
         exercise_resampled(rates[i], ch, false, true);
         exercise_resampled(rates[i], ch, true, false);
      }
   bad_end = true;
   exercise_resampled(64081, 2, false, false);
   bad_end = false;
   frequency = 60000;
   exercise_resampled(384000, 2, false, false);
   puts("PASS resampled worker: fractional clocks, source phase, partial acceptance, EOF and cancellation");
   return 0;
}
