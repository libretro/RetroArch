/* Pacing decisions the runloop makes every iteration.
 *
 * Three of them are pure functions of their arguments, and all three
 * were shipped on the strength of a throwaway model rather than
 * anything that would notice a later change:
 *
 *  - runloop_pace_gap_engages(): the frame limiter holds the loop at
 *    1.0x when nothing else is pacing it at all. The condition that
 *    matters is what it must NOT do - engage while another source is
 *    already holding the loop, or under fast-forward, where running
 *    unthrottled is the point. Both are silent failures: the first
 *    double-paces and the frontend runs slow, the second throttles
 *    fast-forward to 1x and looks like a performance bug.
 *
 *  - runloop_content_frame_time_us(): the period those waits use. A
 *    core reports its own rate and is free to report nonsense; zero
 *    must not divide, a huge rate must not become a busy loop, and a
 *    tiny one must not stall the frontend for a minute.
 *
 *  - runloop_pace_sample_usable(): which intervals feed the measured
 *    loop rate shown beside the pacing claim. A state load or a shader
 *    rebuild is a stall, not pacing; before this bound existed a single
 *    0.9 s sample pulled a 60 fps average to 8 and took several frames
 *    to climb back, so the overlay lied after every hitch.
 *
 * The three live in runloop.h precisely so this runs the shipping
 * versions. Nothing here is a copy: change the header and this test
 * changes with it, which is the point.
 *
 * The gap predicate is checked over every combination of the five pace
 * bits by the two boolean inputs - 128 cases, exhaustive, not a
 * sample. The period is checked over the rates a core can produce
 * including the degenerate ones, and for monotonicity, since a
 * clamp that inverts is a clamp nobody notices. The sample filter is
 * checked at its boundaries and against the regression that motivated
 * it, by running the same eight-sample average the runloop keeps.
 */

#include <stdio.h>
#include <string.h>

#include <retro_common_api.h>
#include <retro_inline.h>
#include <boolean.h>

#include "../../../runloop.h"

static unsigned failures;

static void check(bool ok, const char *what)
{
   if (!ok)
   {
      printf("   FAIL: %s\n", what);
      failures++;
   }
}

/* --- the gap predicate ------------------------------------------- */

static void test_gap_predicate(void)
{
   unsigned pace;
   int nb, fm;
   unsigned engaged = 0;

   for (pace = 0; pace < 64; pace++)
      for (nb = 0; nb < 2; nb++)
         for (fm = 0; fm < 2; fm++)
         {
            bool got  = runloop_pace_gap_engages(pace, nb != 0, fm != 0);
            bool want = (pace == RUNLOOP_PACE_NONE) && !nb && !fm;
            char msg[128];

            if (got)
               engaged++;
            snprintf(msg, sizeof(msg),
                  "pace=0x%02x nonblocking=%d fastmotion=%d -> %d, wanted %d",
                  pace, nb, fm, (int)got, (int)want);
            check(got == want, msg);
         }

   /* Exactly one of the 128 combinations may engage. */
   check(engaged == 1, "exactly one combination engages the gap limiter");

   /* Named cases, so a failure above reads as something rather than a
    * bit pattern. */
   check(runloop_pace_gap_engages(RUNLOOP_PACE_NONE, false, false),
         "nothing pacing, not fast-forwarding: engages");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_NONE, true, false),
         "fast-forward (nonblocking) must stay unthrottled");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_NONE, false, true),
         "FASTMOTION must stay unthrottled");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_VSYNC, false, false),
         "vsync already paces: must not double up");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_AUDIO, false, false),
         "audio already paces: must not double up");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_TIMER, false, false),
         "the frame limiter already paces: must not double up");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_NOWINDOW, false, false),
         "the no-window wait already paces: must not double up");

   printf("   gap predicate: 128 combinations, exactly one engages\n");
}

/* --- the frame period -------------------------------------------- */

static void test_frame_period(void)
{
   static const struct
   {
      float hz;
      const char *what;
   } probes[] = {
      {  0.0f,      "unknown rate"          },
      { -1.0f,      "negative rate"         },
      {  0.001f,    "one frame per 1000 s"  },
      {  1.0f,      "1 Hz"                  },
      { 50.0f,      "PAL"                   },
      { 59.94f,     "NTSC"                  },
      { 60.0f,      "60 Hz"                 },
      { 120.0f,     "120 Hz"                },
      { 240.0f,     "240 Hz"                },
      { 1000000.0f, "a million Hz"          }
   };
   size_t i;
   float hz;
   retro_time_t prev;

   for (i = 0; i < sizeof(probes) / sizeof(probes[0]); i++)
   {
      retro_time_t us = runloop_content_frame_time_us(probes[i].hz);
      char msg[128];

      snprintf(msg, sizeof(msg), "%s (%.3f Hz) -> %ld us, outside 1000-100000",
            probes[i].what, probes[i].hz, (long)us);
      check(us >= 1000 && us <= 100000, msg);
   }

   /* The rates a core actually reports land on the rate asked for,
    * within the microsecond the truncation costs. */
   check(runloop_content_frame_time_us(60.0f) == 16666,
         "60 Hz is 16666 us");
   check(runloop_content_frame_time_us(59.94f) == 16683,
         "59.94 Hz is 16683 us");
   check(runloop_content_frame_time_us(50.0f) == 20000,
         "50 Hz is 20000 us");
   /* Unknown means 60 Hz, not zero and not a division. */
   check(runloop_content_frame_time_us(0.0f) == 16667,
         "an unknown rate is taken as 60 Hz");

   /* Monotonic across the whole usable range: a faster core never gets
    * a longer frame. A clamp written the wrong way round still passes
    * a bounds check. */
   prev = 100001;
   for (hz = 0.01f; hz < 2000.0f; hz *= 1.05f)
   {
      retro_time_t us = runloop_content_frame_time_us(hz);
      check(us <= prev, "period must not grow as the rate rises");
      prev = us;
   }

   printf("   frame period: bounded to 1-100 ms and monotonic over "
          "0.01 Hz to 2 kHz\n");
}

/* --- the measured-rate sample filter ------------------------------ */

/* The average the runloop keeps: an eight-sample exponential, fed only
 * by intervals the filter accepts. */
static retro_time_t feed(retro_time_t ema, retro_time_t delta)
{
   if (!runloop_pace_sample_usable(delta))
      return ema;
   if (ema)
      return ema + (delta - ema) / 8;
   return delta;
}

static void test_sample_filter(void)
{
   retro_time_t ema;
   int i;

   check(!runloop_pace_sample_usable(0),
         "a zero interval is not a sample");
   check(!runloop_pace_sample_usable(-1),
         "a clock that went backwards is not a sample");
   check(runloop_pace_sample_usable(1),
         "one microsecond is a sample");
   check(runloop_pace_sample_usable(249999),
         "just under a quarter second is a sample");
   check(!runloop_pace_sample_usable(250000),
         "a quarter second is a stall, not a sample");
   check(!runloop_pace_sample_usable(900000),
         "0.9 s is a stall, not a sample");

   /* Converges on a jittering 60 Hz loop. */
   ema = 0;
   for (i = 0; i < 200; i++)
      ema = feed(ema, (i % 2) ? 16000 : 17334);
   check(ema > 16000 && ema < 17334,
         "the average settles inside the jitter it was fed");

   /* The regression this bound exists for: a stall must not move it.
    * Before the filter, one 0.9 s sample took a 60 fps reading to
    * about 8 fps and needed several frames to recover. */
   {
      retro_time_t before = ema;
      ema = feed(ema, 900000);
      check(ema == before,
            "a 0.9 s stall leaves the average untouched");
   }

   /* A hitch inside the bound is still averaged - the filter rejects
    * stalls, it does not pretend the loop is always smooth. */
   {
      retro_time_t before = ema;
      ema = feed(ema, 200000);
      check(ema > before, "a 0.2 s hitch is averaged in");
   }

   printf("   sample filter: rejects stalls past 250 ms, averages "
          "everything under\n");
}

int main(void)
{
   printf("runloop pacing decisions:\n");

   test_gap_predicate();
   test_frame_period();
   test_sample_filter();

   if (failures)
   {
      printf("FAILED: %u check(s)\n", failures);
      return 1;
   }

   printf("ok: the gap limiter engages only when nothing else paces and "
          "fast-forward is off, the period is always a sane frame, and "
          "a stall never moves the measured rate\n");
   return 0;
}
