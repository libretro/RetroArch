/* The softfilter fan-out/join barrier's own contract, asserted rather
 * than inferred from output.
 *
 * samples/gfx/filter_lifecycle checks that a processed frame comes out
 * correct, which sounds like coverage for the barrier and is not:
 * deleting the join outright leaves that harness passing, because its
 * slices finish while the verification loop is still reading. A barrier
 * defect only shows as a wrong outcome under hostile timing, so this
 * harness supplies the timing and checks the contract directly:
 *
 *   - every slice has run by the time rarch_softfilter_process() returns
 *   - filt->outstanding is back to zero on return
 *   - no worker is left holding an unconsumed packet (go clear)
 *
 * video_filter.c is included rather than linked, the way
 * samples/audio/mixer_lock includes audio_driver.c: the pool state and
 * the count this is about have internal linkage, and checking them from
 * outside is the whole point.
 *
 * The pool is the real one - created through rarch_softfilter_new() with
 * a builtin filter so the shipping thread pool, fan-out and join all
 * run. Only the work each slice does is this harness's: filt->impl is
 * swapped for a stub whose slices sleep on a skewed schedule and then
 * mark themselves done, and swapped back before free() so the builtin's
 * destroy() still gets its own impl_data. Sleeping rather than spinning
 * is deliberate: a slice that has not been waited for is parked, not
 * merely late, so a missing join fails on every run instead of racing.
 *
 * The skew runs both ways - slowest slice first, then slowest last - so
 * a join that waits its workers out in index order and one that takes
 * them in completion order are both exercised.
 *
 * Four defects were injected into video_filter.c to check this harness
 * has teeth, and it names every one of them: the join deleted, the
 * outstanding count armed after the fan-out instead of before, a worker
 * that decrements the count without notifying, and a worker that tests
 * its go flag before opening its wait window rather than inside it. The
 * last of those is a narrow race and is what the long no-sleep loop
 * below is for - it reproduced on 5 of 5 runs. Keep all four working if
 * this file is amended.
 *
 *   samples/gfx/filter_barrier/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_atomic.h>
#include <retro_timers.h>

/* The subject. Its statics and struct internals are what this checks. */
#include "../../../gfx/video_filter.c"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

#define SRC_W        64
#define SRC_H        48
#define POOL_THREADS 4
/* Long enough that a slice which has not been waited for is still
 * parked when the checks run, short enough to keep the lane brisk. */
#define SLICE_SLEEP_US 15000

static uint32_t src[SRC_W * SRC_H];
static uint32_t dst[(SRC_W * 2) * (SRC_H * 2)];

/* One per worker slice. 'ran' is the generation the slice last
 * completed in, so a stale slice is distinguishable from an absent
 * one rather than both reading as zero. */
struct slice_state
{
   retro_atomic_int_t ran;
   unsigned index;
   unsigned sleep_us;
};

static struct slice_state slices[POOL_THREADS];
static retro_atomic_int_t generation;
static unsigned           slice_count;

/* Watchdog. A barrier defect that strands the join shows up as a hang,
 * which in CI means a job that burns its whole budget and reports
 * nothing useful. This turns that into a named failure: the harness
 * bumps 'progress' between frames, and if it stops moving the watchdog
 * says which phase it stopped in and aborts. Same reason
 * samples/audio/mixer_lock carries one. */
static retro_atomic_int_t progress;
static retro_atomic_int_t watchdog_stop;
static const char        *phase = "startup";

#define WATCHDOG_POLL_US 250000
#define WATCHDOG_STALL   40   /* polls without progress before aborting */

static void watchdog_loop(void *data)
{
   int last   = -1;
   int stalls = 0;
   (void)data;

   while (!retro_atomic_load_acquire_int(&watchdog_stop))
   {
      int now;
      retro_sleep_us(WATCHDOG_POLL_US);
      now = retro_atomic_load_acquire_int(&progress);

      if (now != last)
      {
         last   = now;
         stalls = 0;
         continue;
      }

      if (++stalls >= WATCHDOG_STALL)
      {
         fprintf(stderr,
               "FAIL watchdog: no progress for %d s in phase \"%s\" - the "
               "join did not return\n",
               (int)((WATCHDOG_POLL_US / 1000000.0) * WATCHDOG_STALL),
               phase);
         fflush(stderr);
         abort();
      }
   }
}

static void slice_work(void *data, void *thread_data)
{
   struct slice_state *s = (struct slice_state*)thread_data;
   (void)data;

   if (s->sleep_us)
      retro_sleep_us(s->sleep_us);

   retro_atomic_store_release_int(&s->ran,
         retro_atomic_load_acquire_int(&generation));
}

static void stub_get_work_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height,
      size_t input_stride)
{
   unsigned i;
   (void)data; (void)output; (void)output_stride;
   (void)input; (void)width; (void)height; (void)input_stride;

   for (i = 0; i < slice_count; i++)
   {
      packets[i].work        = slice_work;
      packets[i].thread_data = &slices[i];
   }
}

static unsigned stub_query_num_threads(void *data)
{
   (void)data;
   return slice_count;
}

static const struct softfilter_implementation stub_impl = {
   NULL, NULL, NULL, NULL,
   stub_query_num_threads,
   NULL,
   stub_get_work_packets,
   SOFTFILTER_API_VERSION,
   "barrier stub",
   "barrierstub"
};

/* Runs one frame through the real barrier with the given per-slice
 * sleeps, then checks the contract the instant it returns. */
static void run_frame(rarch_softfilter_t *filt, unsigned ow,
      const unsigned *sleeps, const char *what)
{
   unsigned i;
   int      gen;

   phase = what;
   retro_atomic_fetch_add_int(&progress, 1);

   gen = retro_atomic_load_acquire_int(&generation) + 1;
   retro_atomic_store_release_int(&generation, gen);

   for (i = 0; i < slice_count; i++)
   {
      slices[i].index    = i;
      slices[i].sleep_us = sleeps[i];
      /* Deliberately not the current generation */
      retro_atomic_store_release_int(&slices[i].ran, 0);
   }

   rarch_softfilter_process(filt, dst, ow * sizeof(uint32_t),
         src, VIDEO_SCALE_PACK(SRC_W, SRC_H), SRC_W * sizeof(uint32_t));

   /* Nothing may sleep, print or branch before these: the window this
    * lane exists to catch is exactly the one a delay here would hide. */
   CHECK(retro_atomic_load_acquire_int(&filt->outstanding) == 0,
         "%s: outstanding %d on return, want 0", what,
         retro_atomic_load_acquire_int(&filt->outstanding));

   for (i = 0; i < slice_count; i++)
      CHECK(retro_atomic_load_acquire_int(&slices[i].ran) == gen,
            "%s: slice %u ran=%d, want %d - the join returned before it "
            "finished", what, i,
            retro_atomic_load_acquire_int(&slices[i].ran), gen);

   for (i = 0; i < slice_count; i++)
      CHECK(retro_atomic_load_acquire_int(&filt->thread_data[i].go) == 0,
            "%s: worker %u still holds an unconsumed packet", what, i);
}

int main(void)
{
   rarch_softfilter_t *filt = NULL;
   sthread_t *watchdog      = NULL;
   unsigned ow = 0, oh = 0, x, y, i;
   unsigned sleeps[POOL_THREADS];
   char filt_path[1024];

   retro_atomic_store_release_int(&progress, 0);
   retro_atomic_store_release_int(&watchdog_stop, 0);
   watchdog = sthread_create(watchdog_loop, NULL);

   filt_path[0] = '\0';
   /* Darken is one of only two builtin filters whose query_num_threads
    * returns the count it was given - every other one hardcodes
    * filt->threads = 1 and never reaches the barrier at all. Changing
    * this path to a single-threaded filter turns the whole lane into a
    * no-op, which the threaded-pool check below is here to catch. */
   strlcpy(filt_path, "../../../gfx/video_filters/Darken.filt",
         sizeof(filt_path));

   for (y = 0; y < SRC_H; y++)
      for (x = 0; x < SRC_W; x++)
         src[y * SRC_W + x] = (x << 16) | y;

   filt = rarch_softfilter_new(filt_path, POOL_THREADS,
         RETRO_PIXEL_FORMAT_XRGB8888, VIDEO_SCALE_PACK(SRC_W, SRC_H));
   if (!filt)
   {
      fprintf(stderr, "SKIP filter_barrier: could not create the filter "
            "(need %s)\n", filt_path);
      retro_atomic_store_release_int(&watchdog_stop, 1);
      sthread_join(watchdog);
      return 0;
   }

   {
      unsigned od = 0;
      rarch_softfilter_get_output_size(filt, &od,
            VIDEO_SCALE_PACK(SRC_W, SRC_H));
      ow = VIDEO_SCALE_W(od);
      oh = VIDEO_SCALE_H(od);
   }

   /* The pool is threaded or there is no barrier to test. */
   CHECK(filt->threads > 1 && filt->thread_data != NULL,
         "pool is not threaded (threads=%u), nothing to check",
         filt->threads);

   if (filt->threads > 1 && filt->thread_data)
   {
      const struct softfilter_implementation *real_impl = filt->impl;

      slice_count = filt->threads;
      filt->impl  = &stub_impl;

      /* Even: every slice parked the same length of time. */
      for (i = 0; i < slice_count; i++)
         sleeps[i] = SLICE_SLEEP_US;
      run_frame(filt, ow, sleeps, "even slices");

      /* Slowest first: a join that waits index 0 out before it looks at
       * the rest still has to see the rest finish. */
      for (i = 0; i < slice_count; i++)
         sleeps[i] = (i == 0) ? SLICE_SLEEP_US : SLICE_SLEEP_US / 5;
      run_frame(filt, ow, sleeps, "slowest slice first");

      /* Slowest last: the ordered join reaches its slot last, so a
       * count that is not armed for every worker shows here. */
      for (i = 0; i < slice_count; i++)
         sleeps[i] = (i == slice_count - 1)
            ? SLICE_SLEEP_US : SLICE_SLEEP_US / 5;
      run_frame(filt, ow, sleeps, "slowest slice last");

      /* Back-to-back frames: the pool is reused, so a barrier that
       * leaves the count or a go flag dirty fails on the second. */
      for (i = 0; i < slice_count; i++)
         sleeps[i] = SLICE_SLEEP_US / 3;
      run_frame(filt, ow, sleeps, "reuse 1");
      run_frame(filt, ow, sleeps, "reuse 2");
      run_frame(filt, ow, sleeps, "reuse 3");

      /* No sleeps at all: the fast path, where a worker can finish
       * before the fan-out has handed out the remaining packets, and
       * where a wake that is published between a worker's predicate
       * test and its park would be lost. That window is narrow, so it
       * is hunted with iterations rather than with timing - a worker
       * that misses one wake stalls the join and the watchdog names
       * it. Cheap: no slice sleeps here. */
      for (i = 0; i < slice_count; i++)
         sleeps[i] = 0;
      for (i = 0; i < 40000; i++)
         run_frame(filt, ow, sleeps, "no sleep");

      filt->impl = real_impl;
   }

   phase = "free";
   retro_atomic_fetch_add_int(&progress, 1);
   rarch_softfilter_free(filt);

   retro_atomic_store_release_int(&watchdog_stop, 1);
   sthread_join(watchdog);

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }

   printf("filter_barrier: all lanes passed (%u workers)\n", slice_count);
   return 0;
}
