/* pace_hold_start_test.c -- where display pacing's hold ends.
 *
 * Runs video_thread_hold_until() from gfx/video_thread_wrapper.c
 * (pulled out by extract_hold.awk) against a clock, a perf counter and
 * sleeps of the test's own. The hold is handed a "now" from before the
 * frame was handed over; the handover - a copy of the frame, a drain of
 * the ring - took time since:
 *
 *   1. Where a mark is a perf counter reading, the hold ends at its
 *      target, not the handover's time past it, and reads no clock to
 *      do so.
 *   2. A sleep woken early is followed by one for what is left.
 *   3. A target the handover ran past is not slept for.
 *   4. While the counter's rate is not known the hold is as it was: it
 *      ends no earlier than its target, and reads no clock ahead of its
 *      first sleep.
 *   5. Where a mark is the clock the hold is as it was. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>
#include <features/features_cpu.h>

/* The clock and the perf counter: a microsecond is a tick */
static retro_time_t t_now;
static unsigned     clock_reads;
static bool         rate_known = true;

static retro_time_t t_time_usec(void)
{
   clock_reads++;
   return t_now;
}
static retro_perf_tick_t t_perf_counter(void) { return (retro_perf_tick_t)t_now; }
static retro_time_t t_perf_to_usec(retro_perf_tick_t ticks)
{
   return (rate_known && !(ticks >> 63)) ? (retro_time_t)ticks : 0;
}
#define cpu_features_get_time_usec    t_time_usec
#define cpu_features_get_perf_counter t_perf_counter
#define cpu_features_perf_to_usec     t_perf_to_usec

/* What the wait reads of its surroundings */
typedef struct { int unused; } thread_video_t;
typedef struct
{
   retro_time_t      frame_time;
   retro_perf_tick_t frame_mark;
} video_frame_info_t;

/* The sleeps: as long as asked, or half of it when woken early; the
 * clock's reads ahead of the first are kept */
static unsigned sleeps;
static unsigned wake_early;
static unsigned reads_before_sleep;

static void t_sleep(retro_time_t usec)
{
   if (!sleeps++)
      reads_before_sleep = clock_reads;
   if (wake_early)
   {
      wake_early--;
      usec /= 2;
   }
   t_now += usec;
}
static int  video_thread_ring_load(thread_video_t *thr) { (void)thr; return 0; }
static void video_thread_ring_sleep(thread_video_t *thr, int state,
      int64_t timeout_us)
{
   (void)thr; (void)state;
   t_sleep((retro_time_t)timeout_us);
}
static void t_sleep_us(unsigned usec) { t_sleep((retro_time_t)usec); }
#define retro_sleep_us t_sleep_us

#include "pace_hold_start_driver.h"

static unsigned failures;
#define CHECK(c, m) do { if (!(c)) { printf("   FAIL: %s\n", m); failures++; } } while (0)

/* One hold: the frame read the clock, the handover began @entry_usec
 * after and took @handover_usec; the hold is to end @left_usec after
 * that. How far past its target it ended. */
static retro_time_t hold(retro_time_t entry_usec, retro_time_t handover_usec,
      retro_time_t left_usec)
{
   thread_video_t thr;
   video_frame_info_t info;
   retro_time_t now, target;

   memset(&thr, 0, sizeof(thr));
   t_now          += 1000000;
   info.frame_time = t_now;
   info.frame_mark = (retro_perf_tick_t)t_now;
   t_now          += entry_usec;
   now             = t_now;
   t_now          += handover_usec;
   target          = t_now + left_usec;
   sleeps          = 0;
   clock_reads     = 0;
   reads_before_sleep = 0;
   video_thread_hold_until(&thr, &info, now, target);
   (void)video_thread_ring_load;
   (void)video_thread_ring_sleep;
   (void)t_sleep_us;
   return t_now - target;
}

int main(void)
{
   retro_time_t past;

   t_now = 5000000;

#ifdef CPU_FEATURES_PERF_COUNTER_CHEAP
   past = hold(20, 470, 6000);
   CHECK(past == 0, "a handover of 470 us leaves the hold ending at its target");
   CHECK(sleeps == 1 && clock_reads == 0,
         "in one sleep, with no clock read");

   wake_early = 1;
   past = hold(20, 470, 6000);
   CHECK(past == 0 && sleeps == 2,
         "a sleep woken early is followed by one for what is left");

   past = hold(20, 9000, -500);
   CHECK(sleeps == 0 && past == 500,
         "a target the handover ran past is not slept for");

   rate_known = false;
   past = hold(20, 470, 6000);
   CHECK(past >= 0 && past <= 470 + 20,
         "with no rate for the counter the hold ends no earlier than its target");
   CHECK(reads_before_sleep == 0,
         "and reads no clock ahead of its first sleep");
   rate_known = true;
#else
   past = hold(20, 470, 6000);
   CHECK(past >= 0 && past <= 470,
         "where a mark is the clock the hold ends no earlier than its target");
   past = hold(0, 0, -500);
   CHECK(sleeps == 0, "a target already passed is not slept for");
#endif

   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   puts("pace hold: it ends at its target, whatever the handover took");
   return 0;
}
