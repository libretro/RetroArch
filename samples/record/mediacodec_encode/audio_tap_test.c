/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_audio_tap.c"
uint64_t cpu_features_get(void) { return 0; }
static unsigned calls, fail_at;
static uint64_t accepted;
static int16_t expected[10000];
static int submit(
      void *context, const int16_t *samples, size_t frames, uint64_t pts)
{
   assert(context == &accepted && samples && frames <= 4096 && pts == accepted);
   calls++;
   if (calls == fail_at)
      return RECORD_AUDIO_BUSY;
   assert(!memcmp(
         samples, expected + accepted * 2, frames * 2 * sizeof(int16_t)));
   accepted += frames;
   return RECORD_AUDIO_OK;
}
static record_audio_tap_t *create(void)
{
   record_audio_tap_t *t;
   calls = fail_at = 0;
   accepted = 0;
   t = record_audio_tap_new(48000, submit, &accepted);
   assert(t && record_audio_tap_rate(t) == 48000);
   assert(record_audio_tap_begin(t) && !record_audio_tap_begin(t));
   assert(!record_audio_tap_end(t, NULL) && !record_audio_tap_free(t));
   return t;
}
int main(void)
{
   static int16_t input[10000];
   static float floating[10000];
   record_audio_tap_t *t;
   uint64_t end;
   unsigned i;
   for (i = 0; i < 10000; i++)
      input[i] = expected[i] = (int16_t)(i % 30000);
   t = create();
   assert(record_audio_tap_push(t, input, 5000, false, 48000));
   assert(calls == 2 && !record_audio_tap_failed(t));
   record_audio_tap_retire(t, false);
   assert(record_audio_tap_end(t, &end) && end == 5000);
   assert(!record_audio_tap_begin(t)
         && !record_audio_tap_push(t, input, 1, false, 48000));
   assert(record_audio_tap_free(t));
   for (i = 0; i < 10000; i++)
      floating[i] = (i & 1) ? 0.5f : -0.5f;
   for (i = 0; i < 10000; i++)
      expected[i] = (i & 1) ? 16384 : -16384;
   t = create();
   assert(record_audio_tap_push(t, floating, 5000, true, 48000));
   record_audio_tap_retire(t, false);
   assert(record_audio_tap_end(t, &end) && end == 5000
         && record_audio_tap_free(t));
   t = create();
   fail_at = 2;
   assert(!record_audio_tap_push(t, floating, 5000, true, 48000));
   assert(record_audio_tap_failed(t) && accepted == 4096);
   assert(!record_audio_tap_push(t, floating, 1, true, 48000) && calls == 2);
   record_audio_tap_retire(t, false);
   assert(record_audio_tap_end(t, &end) && end == 4096
         && record_audio_tap_free(t));
   t = create();
   assert(!record_audio_tap_push(t, input, 1, false, 44100) && !calls);
   record_audio_tap_retire(t, true);
   assert(record_audio_tap_failed(t) && record_audio_tap_end(t, &end) && !end);
   assert(record_audio_tap_free(t));
   t = create();
   t->frames = ~(uint64_t)0;
   assert(!record_audio_tap_push(t, input, 1, false, 48000) && !calls);
   record_audio_tap_retire(t, false);
   assert(record_audio_tap_free(t));
   assert(!record_audio_tap_new(0, submit, NULL));
   assert(!record_audio_tap_new(48000, NULL, NULL));
   assert(record_audio_tap_free(NULL));
   puts("PASS post-mixer S16/float capture, bounded packets, source clock, "
        "backpressure and independent producer retirement");
   return 0;
}
