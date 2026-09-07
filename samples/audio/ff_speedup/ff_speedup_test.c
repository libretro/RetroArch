/* Fast-forward audio speedup: who measures the speed, and from what.
 *
 * audio_driver_fastforward_ratio_mult() tracks the wall-clock interval
 * between flushes against the interval a 1.0x core would produce, and
 * the resampler ratio is scaled by the result so fast-forward audio
 * pitches up with the speed instead of dropping out. The contract:
 *
 *  - The measurement is of the core's cadence. On the inline pipeline
 *    the flush runs on the core's thread, so the flush interval is
 *    that. On the threaded pipeline the flush runs on the audio
 *    thread, whose interval is the device draining the previous chunk
 *    - i.e. the previous multiplier played back, which measuring only
 *    confirms. There the producer measures at its publish cadence and
 *    the consumer takes the figure from pipe_ff_mult_q16; nothing the
 *    consumer does can move it.
 *  - The first sample of a fast-forward returns 1.0 and seeds the
 *    average at the 1.0x interval, and releasing fast-forward arms
 *    that seed again. The idle time between two fast-forwards is not a
 *    flush interval and must not enter the average: read as one, it
 *    pins the multiplier at AUDIO_MAX_RATIO and the audio plays back
 *    sixteen times too slow.
 *
 * Includes audio/audio_driver.c so the shipping functions run, with
 * the clock replaced by a counter the test advances. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>

static retro_time_t fake_now;
static retro_time_t fake_time_usec(void) { return fake_now; }
#define cpu_features_get_time_usec fake_time_usec

#include "../../../audio/audio_driver.c"

static int failures;

#define CHECK(cond, what) do { \
   if (!(cond)) { printf("FAIL: %s\n", what); failures++; } \
   else printf("   ok: %s\n", what); } while (0)

#define RATE   48000.0
#define FRAMES 800                 /* one 60 Hz frame of core audio */
#define ONE_X  ((retro_time_t)(FRAMES * 1000000.0 / RATE))

static bool near(double got, double want)
{
   return fabs(got - want) <= want * 0.05;
}

static void fresh(void)
{
   memset(&audio_driver_st, 0, sizeof(audio_driver_st));
   audio_driver_st.input = RATE;
   fake_now              = 1000000;
}

/* --- the measurement at the core's cadence ------------------------- */

static void test_inline_measures_speed(void)
{
   int i;
   double m;

   fresh();
   m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   CHECK(m == 1.0, "first sample of a fast-forward is unity");

   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   }
   CHECK(near(m, 0.25), "64 flushes at 4x settle to a 0.25 multiplier");

   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X * 2;
      m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   }
   CHECK(near(m, 2.0), "and follow a core slower than realtime to 2.0");
}

/* --- release and re-entry ------------------------------------------ */

static void test_reentry_ignores_idle_gap(void)
{
   int i;
   double m, peak = 0.0;

   fresh();
   audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   }

   /* Fast-forward released; a minute at 1.0x; pressed again. */
   audio_driver_ff_mult_reset(&audio_driver_st);
   fake_now += 60 * 1000000;

   m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   CHECK(m == 1.0, "re-entry after a minute idle starts at unity, not the gap");

   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
      if (m > peak)
         peak = m;
   }
   CHECK(peak <= 1.0, "the multiplier never rises above unity on the way to 4x");
   CHECK(near(m, 0.25), "and settles to 0.25 again");
}

/* --- the threaded handoff ------------------------------------------ */

static void test_threaded_consumer_takes_producer_figure(void)
{
   int i;
   double m;
   retro_time_t before;

   fresh();
   audio_driver_st.pipe_threaded = true;
   retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16,
         (int)(0.25 * 65536.0));

   /* The consumer flushes at the device's cadence: the previous chunk
    * at 0.25 played back is a quarter of the 1.0x interval, which is
    * what the old consumer-side measurement would have read as 4x -
    * and then, as the ratio it set drains, whatever it set last. */
   before = audio_driver_st.last_flush_time;
   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X * 4;
      m = audio_driver_ff_mult(&audio_driver_st, FRAMES);
   }
   CHECK(m == 0.25, "consumer reads 0.25 whatever its own cadence");
   CHECK(audio_driver_st.last_flush_time == before,
         "and its flushes do not enter the measurement");

   /* Inline: the same call measures. */
   audio_driver_st.pipe_threaded = false;
   m = audio_driver_ff_mult(&audio_driver_st, FRAMES);
   CHECK(m == 1.0 && audio_driver_st.last_flush_time == fake_now,
         "inline: the same call measures at the flush");
}

/* --- the producer end to end ---------------------------------------- */

static void test_producer_publishes_at_its_cadence(void)
{
   int i;
   int16_t block[FRAMES * 2];
   settings_t *settings = config_get_ptr();
   double got;

   fresh();
   memset(block, 0, sizeof(block));
   settings->bools.audio_fastforward_speedup = true;
   audio_driver_st.pipe_threaded = true;
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_PIPELINE_THREADED);
   if (!retro_spsc_init(&audio_driver_st.pipe_ring, 4096))
   {
      CHECK(false, "ring allocated");
      return;
   }
   audio_driver_st.pipe_lock = slock_new();
   audio_driver_st.pipe_cond = scond_new();
   retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16, 65536);

   /* A 1.0x publish first, so the re-entry seed is armed. */
   audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false);

   /* Then fast-forward at 4x, with nobody draining the ring: every
    * block past the first is dropped, and still counts. */
   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, true);
   }
   got = (double)retro_atomic_load_acquire_int(
         &audio_driver_st.pipe_ff_mult_q16) / 65536.0;
   CHECK(near(got, 0.25), "producer at 4x into a full ring publishes 0.25");

   /* Released: a 1.0x publish re-arms the seed. */
   audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false);
   CHECK(audio_driver_st.last_flush_time == 0,
         "a publish outside fast-forward re-arms the seed");

   retro_spsc_free(&audio_driver_st.pipe_ring);
   slock_free(audio_driver_st.pipe_lock);
   scond_free(audio_driver_st.pipe_cond);
}

int main(void)
{
   printf("fast-forward audio speedup:\n");
   test_inline_measures_speed();
   test_reentry_ignores_idle_gap();
   test_threaded_consumer_takes_producer_figure();
   test_producer_publishes_at_its_cadence();

   if (failures)
   {
      printf("FAILED: %d\n", failures);
      return 1;
   }
   printf("ok: the speedup follows the core's cadence on both pipelines, "
         "never the device's, and re-entry starts from unity\n");
   return 0;
}
