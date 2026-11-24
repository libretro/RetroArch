/* scanline_wait_test.c -- what ends Scanline Sync's wait for the beam.
 *
 * Runs video_driver_scanline_after_frame() from gfx/video_driver.c
 * (pulled out by extract_scanline.awk) against a scripted beam, a
 * clock and a perf counter of the test's own:
 *
 *   1. The beam reaching the target ends the wait, and the line it was
 *      seen on is what is stored.
 *   2. A display that cannot report the beam ends it at once.
 *   3. The beam wrapping above the target twice ends it.
 *   4. A beam that never arrives is waited for one frame's time and no
 *      longer, with the clock not read once a poll where the perf
 *      counter is a register read.
 *   5. While the perf counter's rate is not known, the clock holds the
 *      same bound. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>
#include <features/features_cpu.h>

/* The clock and the perf counter the wait reads: a microsecond is a
 * tick, and each look at the beam takes POLL_USEC of both */
#define POLL_USEC 50
static retro_time_t t_now;
static unsigned     clock_reads;
static bool         rate_known = true;

static retro_time_t t_time_usec(void)
{
   clock_reads++;
   return t_now;
}
static retro_perf_tick_t t_perf_counter(void) { return (retro_perf_tick_t)t_now; }
static retro_perf_tick_t t_perf_from_usec(retro_time_t usec)
{
   return (rate_known && usec > 0) ? (retro_perf_tick_t)usec : 0;
}
#define cpu_features_get_time_usec    t_time_usec
#define cpu_features_get_perf_counter t_perf_counter
#define cpu_features_perf_from_usec   t_perf_from_usec

/* The sleep ahead of the polls passes its time and nothing else */
static void t_sleep(unsigned msec) { t_now += (retro_time_t)msec * 1000; }
#define retro_sleep t_sleep

/* The beam: the lines it is seen on, one a poll, the last one repeated */
static const int16_t *beam;
static unsigned       beam_len;
static unsigned       beam_at;
static unsigned       polls;

static int16_t video_driver_scanline_get(void)
{
   int16_t line = beam[beam_at];
   if (beam_at + 1 < beam_len)
      beam_at++;
   polls++;
   t_now += POLL_USEC;
   return line;
}

#define VIDEO_NOINLINE

/* The slice of video_driver_state_t the wait reads; room for its
 * indices is checked below, once the driver's own are in. */
#define SCANLINE_ROOM 8
typedef struct
{
   uint16_t scanline[SCANLINE_ROOM];
} video_driver_state_t;

#include "scanline_wait_driver.h"

static unsigned failures;
#define CHECK(c, m) do { if (!(c)) { printf("   FAIL: %s\n", m); failures++; } } while (0)

#define FRAME_USEC 16667
#define ACTIVE     1080
#define TARGET     600

/* One wait: the beam's script, the core having run @core_usec */
static retro_time_t wait_for(video_driver_state_t *st, const int16_t *lines,
      unsigned n, uint16_t core_usec, retro_time_t *polled)
{
   retro_time_t began, polling;
   memset(st, 0, sizeof(*st));
   st->scanline[SCANLINE_TARGET] = TARGET;
   st->scanline[SCANLINE_ACTIVE] = ACTIVE;
   beam        = lines;
   beam_len    = n;
   beam_at     = 0;
   polls       = 0;
   clock_reads = 0;
   began       = t_now;
   video_driver_scanline_after_frame(st, FRAME_USEC, core_usec);
   polling     = (retro_time_t)polls * POLL_USEC;
   if (polled)
      *polled  = polling;
   return t_now - began;
}

int main(void)
{
   video_driver_state_t st;
   retro_time_t polled;
   static const int16_t arrives[]     = { 100, 300, 500, 640, 700 };
   static const int16_t unsupported[] = { -1 };
   static const int16_t wraps[]       = { 500, 100, 400, 50, 300, 20 };
   static const int16_t stuck[]       = { 200 };

   CHECK(SCANLINE_LAST <= SCANLINE_ROOM, "the test's state is too small");
   t_now = 1000000;

   wait_for(&st, arrives, 5, 4000, &polled);
   CHECK(polls == 4 && st.scanline[SCANLINE_TARGET] == 640,
         "the beam at the target ends the wait, on the line it was seen");

   wait_for(&st, unsupported, 1, 4000, &polled);
   CHECK(polls == 1 && st.scanline[SCANLINE_TARGET] == 0,
         "a display that cannot report the beam ends the wait at once");

   wait_for(&st, wraps, 6, 4000, &polled);
   CHECK(polls == 4, "the beam wrapping twice ends the wait");

   wait_for(&st, stuck, 1, 4000, &polled);
   CHECK(polled >= FRAME_USEC && polled <= FRAME_USEC + POLL_USEC,
         "a beam that never arrives is polled for a frame's time");
#ifdef CPU_FEATURES_PERF_COUNTER_CHEAP
   CHECK(clock_reads == 0, "and the clock is not read for it");
#endif

   rate_known = false;
   wait_for(&st, stuck, 1, 4000, &polled);
   CHECK(polled >= FRAME_USEC - POLL_USEC && polled <= FRAME_USEC + POLL_USEC,
         "with no rate for the counter the clock holds the same bound");
   CHECK(clock_reads > 1, "and it is the clock that holds it");
   rate_known = true;

   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   puts("scanline wait: the beam, the display or a frame's time ends it");
   return 0;
}
