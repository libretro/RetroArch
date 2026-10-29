/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <retro_math.h>
#include <compat/mingw_math.h>
#include "../../../audio/audio_driver.h"
#include "../../../record/record_audio_tap.h"
#define AUDIO_PAUSE_TAIL_FRAMES 512
static audio_driver_state_t audio_driver_st;
#include "audio_tap_fragment.c"
uint64_t cpu_features_get(void) { return 0; }
static retro_atomic_int_t entered, release_pass;
static unsigned pushes;
static int submit(
      void *context, const int16_t *samples, size_t frames, uint64_t pts)
{
   unsigned i;
   assert(context == &pushes && frames == 32 && pts == 0);
   for (i = 0; i < 64; i++)
      assert(samples[i] == (i & 1 ? 4096 : -4096));
   pushes++;
   return RECORD_AUDIO_OK;
}
#ifdef HAVE_THREADS
static void pass(void *unused)
{
   int16_t mixed[64];
   unsigned i;
   (void)unused;
   audio_driver_state_lock();
   retro_atomic_store_release_int(&entered, 1);
   while (!retro_atomic_load_acquire_int(&release_pass))
   {
   }
   for (i = 0; i < 64; i++)
      mixed[i] = (int16_t)(i & 1 ? 4096 : -4096);
   audio_driver_pause_track(&audio_driver_st, mixed, 32, false);
   audio_driver_state_unlock();
}
#endif
int main(void)
{
   record_audio_tap_t *t = record_audio_tap_new(48000, submit, &pushes);
   record_audio_tap_t *other = record_audio_tap_new(44100, submit, &pushes);
   uint64_t end;
#ifdef HAVE_THREADS
   sthread_t *thread;
   audio_driver_st.state_lock = slock_new();
   assert(audio_driver_st.state_lock);
#endif
   assert(t && other);
   audio_driver_st.out_rate = 48000;
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED);
   assert(audio_driver_record_tap_install(other) == -1);
   assert(audio_driver_record_tap_install(t) == 1);
   assert(!record_audio_tap_free(t));
#ifdef HAVE_THREADS
   retro_atomic_int_init(&entered, 0);
   retro_atomic_int_init(&release_pass, 0);
   thread = sthread_create(pass, NULL);
   assert(thread);
   while (!retro_atomic_load_acquire_int(&entered))
   {
   }
   assert(audio_driver_record_tap_install(t) == 0);
   assert(!audio_driver_record_tap_remove(t) && !pushes);
   assert(!record_audio_tap_end(t, &end));
   retro_atomic_store_release_int(&release_pass, 1);
   sthread_join(thread);
   assert(pushes == 1);
#else
   {
      int16_t mixed[64];
      unsigned i;
      for (i = 0; i < 64; i++)
         mixed[i] = (int16_t)(i & 1 ? 4096 : -4096);
      audio_driver_pause_track(&audio_driver_st, mixed, 32, false);
      assert(pushes == 1);
   }
#endif
   assert(audio_driver_record_tap_remove(t));
   assert(!AUDIO_DRIVER_PTR_GET(&audio_driver_st.record_tap));
   assert(record_audio_tap_end(t, &end) && end == 32);
   assert(record_audio_tap_free(t) && record_audio_tap_free(other));
#ifdef HAVE_THREADS
   slock_free(audio_driver_st.state_lock);
   audio_driver_st.state_lock = NULL;
#endif
   puts("PASS shipping audio owner try-install/remove, post-mix capture and "
        "callback quiescence without new pass locks");
   return 0;
}
