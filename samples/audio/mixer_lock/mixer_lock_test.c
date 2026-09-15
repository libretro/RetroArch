/* Harness for the mixer's state_lock discipline.
 *
 * The state lock is a plain non-recursive mutex, so any path that
 * reaches a locking entry point while already holding it parks that
 * thread forever against itself. libretro/RetroArch#19485 was exactly
 * that: loading a menu sound goes through
 * audio_driver_mixer_add_stream() with a manually chosen slot, which
 * frees whatever occupies the slot first - and it did so through the
 * public stop/remove entry points while holding the lock. The main
 * thread deadlocked on the first menu sound, which looked like a
 * rendered but frozen menu.
 *
 * Every case here calls the real functions in audio/audio_driver.c
 * through the same entry points the frontend uses, on a watchdog: a
 * reintroduced recursive acquisition aborts with a stack rather than
 * hanging the suite. The cases cover the manual-slot reload that
 * triggered the bug, an occupied slot (the reload that reaches the
 * free path), the public entry points on their own, and the
 * out-of-range slot each entry point has to reject without leaving
 * the lock held.
 *
 * Links the driver's translation unit; the stubs alongside supply the
 * frontend and mixer symbols it references. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_atomic.h>

/* Included rather than linked: the driver state, and with it the lock
 * this harness is about, has internal linkage. Creating the lock here
 * is what audio_driver_init_internal() does at line ~2201; without it
 * audio_driver_state_lock() sees a NULL lock, quietly does nothing,
 * and every case below would pass no matter how the mixer nests its
 * acquisitions. */
#include "../../../audio/audio_driver.c"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

static retro_atomic_int_t stage = RETRO_ATOMIC_INT_INITIALIZER(0);

#define STAGE(v) retro_atomic_store_release_int(&stage, (v))

static void *watchdog(void *arg)
{
   int last = retro_atomic_load_acquire_int(&stage);
   int cur;
   (void)arg;
   for (;;)
   {
      sleep(5);
      cur = retro_atomic_load_acquire_int(&stage);
      if (cur == last && cur >= 0)
      {
         fprintf(stderr,
               "WATCHDOG: stage %d has not returned in 5s; the mixer "
               "state lock is being acquired recursively\n", cur);
         abort();
      }
      last = cur;
      if (cur < 0)
         return NULL;
   }
}

/* A minimal WAV the stub mixer accepts; contents do not matter,
 * only that add_stream walks its manual-slot path. */
static unsigned char dummy_wav[64];

static void fill_params(audio_mixer_stream_params_t *p, unsigned slot)
{
   memset(p, 0, sizeof(*p));
   p->buf                 = dummy_wav;
   p->bufsize             = sizeof(dummy_wav);
   p->basename            = NULL;   /* menu sounds carry no name */
   p->volume              = 1.0f;
   p->slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_MANUAL;
   p->slot_selection_idx  = slot;
   p->stream_type         = AUDIO_STREAM_TYPE_SYSTEM;
   p->type                = AUDIO_MIXER_TYPE_WAV_STREAM;
   p->state               = AUDIO_STREAM_STATE_STOPPED;
}

int main(void)
{
   pthread_t dog;
   audio_mixer_stream_params_t params;
   unsigned slot = AUDIO_MIXER_SYSTEM_SLOT_OK;

   pthread_create(&dog, NULL, watchdog, NULL);

   audio_driver_st.state_lock = slock_new();
   if (!audio_driver_st.state_lock)
   {
      printf("FAIL: could not create the state lock\n");
      return 1;
   }

   /* 1. The menu-sound load itself: an empty manual slot. This is the
    *    call that deadlocked. */
   STAGE(1);
   fill_params(&params, slot);
   CHECK(audio_driver_mixer_add_stream(&params),
         "add_stream into an empty manual slot failed");

   /* 2. The same slot again, now occupied: add_stream must free the
    *    resident stream before taking the slot, which is the path
    *    that reached the locking entry points. */
   STAGE(2);
   fill_params(&params, slot);
   CHECK(audio_driver_mixer_add_stream(&params),
         "add_stream over an occupied manual slot failed");

   /* 3. A playing stream in the slot: add_stream's free path then runs
    *    the stop arm as well as the remove arm. */
   STAGE(3);
   audio_driver_mixer_play_stream(slot);
   fill_params(&params, slot);
   CHECK(audio_driver_mixer_add_stream(&params),
         "add_stream over a playing manual slot failed");

   /* 4. The public entry points on their own must still take and
    *    release the lock. */
   STAGE(4);
   audio_driver_mixer_stop_stream(slot);
   audio_driver_mixer_remove_stream(slot);
   audio_driver_mixer_set_stream_volume(slot, 1.0f);

   /* 5. Having released it, the lock is free for the next caller: a
    *    leak shows up here as a hang rather than a wrong answer. */
   STAGE(5);
   fill_params(&params, slot);
   CHECK(audio_driver_mixer_add_stream(&params),
         "add_stream after the public entry points failed");

   /* 6. Out-of-range slots: rejected, and the lock left free. Each is
    *    followed by a call that has to be able to take it. */
   STAGE(6);
   audio_driver_mixer_stop_stream(AUDIO_MIXER_MAX_SYSTEM_STREAMS);
   audio_driver_mixer_remove_stream(AUDIO_MIXER_MAX_SYSTEM_STREAMS + 7);
   audio_driver_mixer_set_stream_volume(AUDIO_MIXER_MAX_SYSTEM_STREAMS, 1.0f);
   fill_params(&params, AUDIO_MIXER_MAX_SYSTEM_STREAMS);
   CHECK(!audio_driver_mixer_add_stream(&params),
         "add_stream accepted an out-of-range manual slot");
   STAGE(7);
   fill_params(&params, slot);
   CHECK(audio_driver_mixer_add_stream(&params),
         "the lock was left held by an out-of-range call");

   /* 7. Every system slot in turn, twice: what loading the full set of
    *    menu sounds does at startup, and again on a settings change. */
   STAGE(8);
   {
      unsigned i, pass;
      for (pass = 0; pass < 2; pass++)
         for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
         {
            fill_params(&params, i);
            CHECK(audio_driver_mixer_add_stream(&params),
                  "add_stream failed for slot %u on pass %u", i, pass);
         }
   }

   /* Release what is still resident, as audio_driver_mixer_deinit()
    * does at shutdown, so leak checking covers these paths. */
   STAGE(9);
   {
      unsigned i;
      for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
         audio_driver_mixer_remove_stream(i);
   }
   slock_free(audio_driver_st.state_lock);
   audio_driver_st.state_lock = NULL;

   STAGE(-1);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("mixer state lock: no recursive acquisition, no leaked lock\n");
   return 0;
}
