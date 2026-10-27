/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_audio_tap.h"
#include <stdlib.h>
#include <retro_atomic.h>
#include <audio/conversion/float_to_s16.h>
#define RECORD_AUDIO_TAP_FRAMES 4096
struct record_audio_tap
{
   uint64_t frames;
   int (*submit)(void *, const int16_t *, size_t, uint64_t);
   void *context;
   unsigned rate;
   retro_atomic_int_t state, failed;
   bool producer_failed;
   int16_t scratch[RECORD_AUDIO_TAP_FRAMES * 2];
};
record_audio_tap_t *record_audio_tap_new(unsigned rate,
      int (*submit)(void *, const int16_t *, size_t, uint64_t), void *context)
{
   record_audio_tap_t *t;
   if (!submit || rate < 8000 || rate > 192000)
      return NULL;
   t = (record_audio_tap_t *)calloc(1, sizeof(*t));
   if (!t)
      return NULL;
   t->rate = rate;
   t->submit = submit;
   t->context = context;
   retro_atomic_int_init(&t->state, 0);
   retro_atomic_int_init(&t->failed, 0);
   return t;
}
unsigned record_audio_tap_rate(const record_audio_tap_t *t)
{
   return t ? t->rate : 0;
}
bool record_audio_tap_begin(record_audio_tap_t *t)
{
   return t && retro_atomic_cas_int(&t->state, 0, 1);
}
void record_audio_tap_retire(record_audio_tap_t *t, bool disconnected)
{
   if (!t)
      return;
   if (disconnected)
      retro_atomic_store_release_int(&t->failed, 1);
   retro_atomic_store_release_int(&t->state, 2);
}
bool record_audio_tap_failed(const record_audio_tap_t *t)
{
   return !t || retro_atomic_load_acquire_int(&t->failed) != 0;
}
bool record_audio_tap_push(record_audio_tap_t *t, const void *data,
      size_t frames, bool floating, unsigned rate)
{
   const uint8_t *input = (const uint8_t *)data;
   if (!t || retro_atomic_load_acquire_int(&t->state) != 1)
      return false;
   if (t->producer_failed)
      return false;
   if (rate != t->rate || (!data && frames)
         || frames
               > (~(size_t)0) / 2 / (floating ? sizeof(float) : sizeof(int16_t))
         || t->frames > ~(uint64_t)0 - frames)
      goto failed;
   while (frames)
   {
      size_t n = frames > RECORD_AUDIO_TAP_FRAMES ? RECORD_AUDIO_TAP_FRAMES
                                                  : frames;
      const int16_t *samples = (const int16_t *)input;
      if (floating)
      {
         convert_float_to_s16(t->scratch, (const float *)input, n * 2);
         samples = t->scratch;
      }
      if (t->submit(t->context, samples, n, t->frames) != RECORD_AUDIO_OK)
         goto failed;
      t->frames += n;
      input += n * 2 * (floating ? sizeof(float) : sizeof(int16_t));
      frames -= n;
   }
   return true;
failed:
   t->producer_failed = true;
   retro_atomic_store_release_int(&t->failed, 1);
   return false;
}
bool record_audio_tap_end(const record_audio_tap_t *t, uint64_t *frames)
{
   if (!t || retro_atomic_load_acquire_int(&t->state) == 1)
      return false;
   if (frames)
      *frames = t->frames;
   return true;
}
bool record_audio_tap_free(record_audio_tap_t *t)
{
   if (t && !record_audio_tap_end(t, NULL))
      return false;
   free(t);
   return true;
}
