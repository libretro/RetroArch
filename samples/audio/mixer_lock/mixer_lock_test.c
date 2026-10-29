/* Harness for the mixer's stream table: how it is held, and what the
 * mix may do to it.
 *
 * The table is held by the main and task threads (audio_driver_mixer_
 * hold), never by the mix, and the mix's stop callbacks only report a
 * finish for the table's next holder to settle. A sequential play's
 * callback used to start the next stream itself, from inside the mix
 * and under the state lock the mix holds - against itself, on a
 * non-recursive mutex. The sequential stage below fires that finish
 * from a mix thread holding the state lock; the stress stage plays,
 * stops, adds and removes from two threads while a third finishes
 * voices.
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
#include <sched.h>

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

/* Ownership counter for the stage-0 refusal: the contract releases
 * buf_owner on every outcome, a refusal included. */
static unsigned owner_frees = 0;
static void counting_owner_free(void *owner)
{
   (void)owner;
   owner_frees++;
}

/* A sound is built - a WAV converted and resampled whole - with the
 * state lock free, so the audio thread mixes on meanwhile. The load
 * stubs call this; a try-lock that fails means this thread holds it. */
extern void (*stub_load_hook)(void);
extern const void *stub_wav_source;
static unsigned loads_seen       = 0;
static unsigned loads_under_lock = 0;
static void lock_free_during_load(void)
{
   loads_seen++;
   if (slock_try_lock(audio_driver_st.state_lock))
      slock_unlock(audio_driver_st.state_lock);
   else
      loads_under_lock++;
#ifdef AUDIO_MIXER_TABLE_OWNERSHIP
   if (retro_atomic_load_acquire_int(&audio_driver_st.mixer_table_owner)
         != AUDIO_MIXER_TABLE_FREE)
      loads_under_lock++;
#endif
}

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

extern retro_atomic_int_t stub_sound_destroys;
extern bool stub_play_fail;
extern bool stub_voice_finish(audio_mixer_voice_t *voice);
extern audio_mixer_voice_t *stub_voice_at(unsigned i);
extern unsigned stub_voice_count(void);

static int playing(void)
{
   return retro_atomic_load_acquire_int(&audio_driver_st.mixer_streams_playing);
}

/* The mix: it finishes a play holding the state lock, as a flush does. */
static void *finish_under_mix(void *voice)
{
   slock_lock(audio_driver_st.state_lock);
   stub_voice_finish((audio_mixer_voice_t*)voice);
   slock_unlock(audio_driver_st.state_lock);
   return NULL;
}

static void finish_on_mix_thread(audio_mixer_voice_t *voice)
{
   pthread_t mix;
   pthread_create(&mix, NULL, finish_under_mix, voice);
   pthread_join(mix, NULL);
}

static retro_atomic_int_t stress_stop;

/* A pass: holds the state lock, as a flush does, until let go. */
extern retro_atomic_int_t stub_dsp_frees;
static retro_atomic_int_t pass_entered, pass_release;
static void *held_pass(void *arg)
{
   (void)arg;
   audio_driver_state_lock();
   retro_atomic_store_release_int(&pass_entered, 1);
   while (!retro_atomic_load_acquire_int(&pass_release))
      sched_yield();
   audio_driver_state_unlock();
   return NULL;
}

static void *stress_mix(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_acquire_int(&stress_stop))
   {
      unsigned i;
      slock_lock(audio_driver_st.state_lock);
      for (i = 0; i < stub_voice_count(); i++)
         stub_voice_finish(stub_voice_at(i));
      slock_unlock(audio_driver_st.state_lock);
   }
   return NULL;
}

/* A mixer task: user streams added on a free slot and removed again. */
static void *stress_task(void *arg)
{
   unsigned n;
   (void)arg;
   for (n = 0; n < 3000; n++)
   {
      audio_mixer_stream_params_t p;
      int at = -1;
      memset(&p, 0, sizeof(p));
      p.buf                 = dummy_wav;
      p.bufsize             = sizeof(dummy_wav);
      p.volume              = 1.0f;
      p.slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;
      p.stream_type         = AUDIO_STREAM_TYPE_USER;
      p.type                = AUDIO_MIXER_TYPE_WAV;
      p.state               = (n & 1) ? AUDIO_STREAM_STATE_PLAYING
                                      : AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL;
      p.out_slot            = &at;
      if (audio_driver_mixer_add_stream(&p) && at >= 0)
      {
         audio_driver_mixer_get_stream_state((unsigned)at);
         if (n & 2)
            audio_driver_mixer_remove_stream((unsigned)at);
      }
   }
   return NULL;
}

static void prepared_adoption(unsigned slot)
{
   audio_mixer_stream_params_t p;
   audio_mixer_sound_t *sound;
   audio_mixer_voice_t *voice;
   size_t serial;
   unsigned before, loads;
   int installed;
   unsigned pass;

   /* The caller's buffer owner is unrelated to the prepared sound.
    * It must never be released or attached by the adoption path. */
   for (pass = 0; pass < 7; pass++)
   {
      fill_params(&p, slot);
      p.state = AUDIO_STREAM_STATE_PLAYING_LOOPED;
      p.buf_owner = dummy_wav;
      p.buf_owner_free = counting_owner_free;
      p.out_slot = &installed;
      installed = 123;
      voice = NULL;
      serial = 123;
      before = (unsigned)retro_atomic_load_acquire_int(&stub_sound_destroys);
      sound = audio_mixer_load_wav_stream(malloc(64), 64);
      loads = loads_seen;
      stub_load_hook = lock_free_during_load;
      if (pass == 0) p.state = AUDIO_STREAM_STATE_STOPPED;
      if (pass == 1) p.slot_selection_idx = AUDIO_MIXER_MAX_SYSTEM_STREAMS;
      if (pass == 2) AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_MIXER_INITED);
      if (pass == 3) stub_play_fail = true;
      if (pass == 6)
      {
         unsigned i;
         p.slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;
         for (i = AUDIO_MIXER_MAX_STREAMS; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
            audio_driver_st.mixer_streams[i].state = AUDIO_STREAM_STATE_STOPPED;
      }
      if (pass == 4)
      {
         CHECK(!audio_driver_mixer_add_prepared_stream(NULL, sound,
                  &voice, &serial), "prepared NULL params accepted");
      }
      else if (pass != 5)
         CHECK(!audio_driver_mixer_add_prepared_stream(&p, sound,
                  &voice, &serial), "prepared failure case %u accepted", pass);
      else
      {
         CHECK(audio_driver_mixer_add_prepared_stream(&p, sound,
                  &voice, &serial), "prepared success refused");
         CHECK(installed == (int)slot, "prepared slot not returned");
         CHECK(audio_driver_st.mixer_streams[slot].voice != NULL,
               "prepared play installed without voice");
         audio_driver_mixer_stop_stream(slot);
         audio_driver_mixer_remove_stream(slot);
      }
      stub_load_hook = NULL;
      CHECK(loads_seen == loads, "adoption reloaded the prepared sound");
      CHECK((unsigned)retro_atomic_load_acquire_int(&stub_sound_destroys) == before + 1,
            "prepared case %u did not destroy exactly once", pass);
      if (pass != 5)
      {
         CHECK(!voice && !serial, "prepared refusal left a binding");
         if (pass != 4) CHECK(installed == -1, "prepared refusal left a slot");
      }
      if (pass == 6)
      {
         unsigned i;
         for (i = AUDIO_MIXER_MAX_STREAMS; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
            audio_driver_st.mixer_streams[i].state = AUDIO_STREAM_STATE_NONE;
      }
      stub_play_fail = false;
      AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_MIXER_INITED);
   }
}

int main(void)
{
   pthread_t dog;
   audio_mixer_stream_params_t params;
   unsigned slot = AUDIO_MIXER_SYSTEM_SLOT_OK;
   unsigned prepared_rate = 123;
   bool prepared_s16 = true;

   pthread_create(&dog, NULL, watchdog, NULL);

   audio_driver_st.state_lock = slock_new();
#ifdef AUDIO_MIXER_TABLE_OWNERSHIP
   /* What audio_driver_init_internal() makes before the mixer starts. */
   audio_driver_st.mixer_table_ec_ok =
      retro_eventcount_init(&audio_driver_st.mixer_table_ec);
#endif
   if (!audio_driver_st.state_lock)
   {
      printf("FAIL: could not create the state lock\n");
      return 1;
   }

   /* 0. Outside the mixer's init..done window every add is refused,
    *    with ownership released and the lock balanced - the contract
    *    AUDIO_FLAG_MIXER_INITED enforces for streams retiring after
    *    teardown. */
   STAGE(0);
   CHECK(!audio_driver_mixer_prepare_format(&prepared_rate, &prepared_s16)
         && !prepared_rate && !prepared_s16, "inactive mixer exposed prepare format");
   fill_params(&params, slot);
   owner_frees      = 0;
   params.buf_owner      = dummy_wav;
   params.buf_owner_free = counting_owner_free;
   CHECK(!audio_driver_mixer_add_stream(&params),
         "add_stream accepted a stream outside the init window");
   CHECK(owner_frees == 1,
         "refused add released the owner %u times, want exactly 1",
         owner_frees);
   /* Lock balance after the refusal is proven the way stage 6 proves
    * it: the next add - stage 1 - has to be able to take the lock,
    * and the watchdog turns a held lock into an abort. */

   /* Everything below runs inside the window, as the shipping
    * lifecycle has it: audio_driver_init_internal() raises the latch
    * right after audio_mixer_init(). */
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_MIXER_INITED);
   audio_driver_mixer_init_rate = 48000;
   CHECK(audio_driver_mixer_prepare_format(&prepared_rate, &prepared_s16)
         && prepared_rate == 48000 && !prepared_s16,
         "initialized mixer prepare format snapshot is wrong");

   /* 1. The menu-sound load itself: an empty manual slot. This is the
    *    call that deadlocked. The sound is built with the lock free. */
   STAGE(1);
   fill_params(&params, slot);
   stub_load_hook = lock_free_during_load;
   CHECK(audio_driver_mixer_add_stream(&params),
         "add_stream into an empty manual slot failed");
   fill_params(&params, slot);
   params.type = AUDIO_MIXER_TYPE_WAV;
   CHECK(audio_driver_mixer_add_stream(&params),
         "add_stream of a WAV into a manual slot failed");
   /* A WAV's load copies the samples it keeps: it reads the caller's
    * bytes, not a copy of them. */
   CHECK(stub_wav_source == dummy_wav, "the WAV was copied before its load");
   stub_load_hook = NULL;
   CHECK(loads_seen == 2, "%u sounds built, want 2", loads_seen);
   CHECK(loads_under_lock == 0,
         "%u of %u sounds built with the state lock held",
         loads_under_lock, loads_seen);

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
   {
      unsigned releases = owner_frees;
      int before = playing();
      prepared_adoption(slot);
      CHECK(owner_frees == releases, "prepared adoption released params owner");
      CHECK(playing() == before, "prepared adoption leaked a playing counter");
   }

   /* 10. A sequential play the mix finishes: the next stopped stream
    *     starts, settled by the table's holder - not by the mix, which
    *     holds the state lock as it mixes. */
   STAGE(10);
   {
      unsigned a = AUDIO_MIXER_MAX_STREAMS, b = AUDIO_MIXER_MAX_STREAMS + 1;
      audio_mixer_voice_t *voice;
      fill_params(&params, a);
      CHECK(audio_driver_mixer_add_stream(&params), "sequential: load a");
      fill_params(&params, b);
      CHECK(audio_driver_mixer_add_stream(&params), "sequential: load b");
      audio_driver_mixer_play_stream_sequential(a);
      voice = audio_driver_st.mixer_streams[a].voice;
      CHECK(voice && playing() == 1, "sequential: a plays");
      finish_on_mix_thread(voice);
      CHECK(audio_driver_st.mixer_streams[a].state
            == AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
            "sequential: the mix changed the table");
      audio_driver_mixer_reap();
      CHECK(audio_driver_st.mixer_streams[a].state == AUDIO_STREAM_STATE_NONE,
            "sequential: a was not retired");
      CHECK(audio_driver_st.mixer_streams[b].state
            == AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL && playing() == 1,
            "sequential: b did not follow a");
      finish_on_mix_thread(audio_driver_st.mixer_streams[b].voice);
      audio_driver_mixer_reap();
      CHECK(audio_driver_st.mixer_streams[b].state == AUDIO_STREAM_STATE_NONE
            && playing() == 0, "sequential: b was not retired");
      /* A finish reaped twice is settled once. */
      audio_driver_mixer_reap();
      CHECK(playing() == 0, "sequential: a finish settled twice");
   }

   /* 11. Menu sounds replayed on the main thread and streams added and
    *     removed by a task while a mix finishes whatever plays. */
   STAGE(11);
   {
      pthread_t mix, task;
      unsigned n, i;
      fill_params(&params, slot);
      CHECK(audio_driver_mixer_add_stream(&params), "stress: menu sound");
      retro_atomic_store_release_int(&stress_stop, 0);
      pthread_create(&mix, NULL, stress_mix, NULL);
      pthread_create(&task, NULL, stress_task, NULL);
      for (n = 0; n < 3000; n++)
      {
         STAGE(1100 + (int)(n / 100));
         audio_driver_mixer_play_menu_sound(slot);
         audio_driver_mixer_set_stream_volume(slot, 0.0f);
         audio_driver_mixer_reap();
         if (n & 1)
            audio_driver_mixer_stop_stream(slot);
      }
      pthread_join(task, NULL);
      retro_atomic_store_release_int(&stress_stop, 1);
      pthread_join(mix, NULL);
      STAGE(12);
      for (i = 0; i < stub_voice_count(); i++)
         stub_voice_finish(stub_voice_at(i));
      audio_driver_mixer_reap();
      for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
         audio_driver_mixer_remove_stream(i);
      CHECK(playing() == 0, "stress: %d plays left counted", playing());
   }

   /* 13. The DSP filter is swapped and freed from the main thread while
    *     a pass runs, without waiting for it: what the pass may still be
    *     running is freed once it is over. */
   STAGE(13);
   {
      pthread_t pass;
      int frees = retro_atomic_load_acquire_int(&stub_dsp_frees);
      CHECK(audio_driver_dsp_filter_init("a.dsp"), "dsp: init a");
      CHECK(retro_atomic_load_acquire_int(&audio_driver_st.dsp_caps) == 1,
            "dsp: a float-only filter is not published as one");
      retro_atomic_store_release_int(&pass_entered, 0);
      retro_atomic_store_release_int(&pass_release, 0);
      pthread_create(&pass, NULL, held_pass, NULL);
      while (!retro_atomic_load_acquire_int(&pass_entered))
         sched_yield();
      CHECK(audio_driver_dsp_filter_init("b.dsp"), "dsp: init b");
      audio_driver_dsp_filter_free();
      CHECK(retro_atomic_load_acquire_int(&stub_dsp_frees) == frees,
            "dsp: a filter was freed under a pass");
      CHECK(!AUDIO_DRIVER_PTR_GET(&audio_driver_st.dsp)
            && !retro_atomic_load_acquire_int(&audio_driver_st.dsp_caps),
            "dsp: the filter is still published");
      retro_atomic_store_release_int(&pass_release, 1);
      pthread_join(pass, NULL);
      audio_driver_dsp_reap(&audio_driver_st);
      CHECK(retro_atomic_load_acquire_int(&stub_dsp_frees) == frees + 2,
            "dsp: %d of 2 retired filters freed after the pass",
            retro_atomic_load_acquire_int(&stub_dsp_frees) - frees);
      CHECK(audio_driver_dsp_filter_init("c.dsp"), "dsp: init c");
      audio_driver_dsp_filter_free();
      CHECK(retro_atomic_load_acquire_int(&stub_dsp_frees) == frees + 3,
            "dsp: a filter no pass runs was not freed at once");
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
