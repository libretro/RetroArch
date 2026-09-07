/* Pacing decisions the runloop makes every iteration.
 *
 * Three of them are pure functions of their arguments, and all three
 * were shipped on the strength of a throwaway model rather than
 * anything that would notice a later change:
 *
 *  - runloop_pace_gap_engages(): the frame limiter holds the loop to
 *    the display rate when nothing else is pacing it and either audio
 *    rate control can follow or Scanline Sync is between locks. The
 *    condition that matters is what it must NOT do - engage while
 *    another source is already holding the loop, with neither of
 *    those, or under fast-forward, where running unthrottled is the
 *    point. All are silent failures: double-pacing runs the frontend
 *    slow, and a throttled fast-forward looks like a performance bug.
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
 * The gap predicate is checked over every combination of the six pace
 * bits by the four boolean inputs - 512 cases, exhaustive, not a
 * sample. The period is checked over the rates a core can produce
 * including the degenerate ones, and for monotonicity, since a
 * clamp that inverts is a clamp nobody notices. The sample filter is
 * checked at its boundaries and against the regression that motivated
 * it, by running the same eight-sample average the runloop keeps.
 */

#include <stdio.h>
#include <stdint.h>
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
   int nb, fm, ss, rc;
   unsigned engaged = 0;

   for (pace = 0; pace < 64; pace++)
      for (nb = 0; nb < 2; nb++)
         for (fm = 0; fm < 2; fm++)
            for (ss = 0; ss < 2; ss++)
               for (rc = 0; rc < 2; rc++)
               {
                  bool got  = runloop_pace_gap_engages(pace,
                        nb != 0, fm != 0, ss != 0, rc != 0);
                  bool want = (pace == RUNLOOP_PACE_NONE)
                        && !nb && !fm && (ss || rc);
                  char msg[128];

                  if (got)
                     engaged++;
                  snprintf(msg, sizeof(msg),
                        "pace=0x%02x nonblocking=%d fastmotion=%d "
                        "scanline=%d ratecontrol=%d -> %d, wanted %d",
                        pace, nb, fm, ss, rc, (int)got, (int)want);
                  check(got == want, msg);
               }

   /* Three of the 512 combinations may engage: nothing pacing, not
    * fast-forwarding, with rate control, Scanline Sync, or both. */
   check(engaged == 3, "exactly three combinations engage the gap limiter");

   /* Named cases, so a failure above reads as something rather than a
    * bit pattern. */
   check(runloop_pace_gap_engages(RUNLOOP_PACE_NONE, false, false, false, true),
         "nothing pacing, rate control on, not fast-forwarding: engages");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_NONE, false, false, false, false),
         "rate control off: the loop runs unlimited as configured");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_NONE, true, false, false, true),
         "fast-forward (nonblocking) must stay unthrottled");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_NONE, false, true, false, true),
         "FASTMOTION must stay unthrottled");
   check(runloop_pace_gap_engages(RUNLOOP_PACE_NONE, false, false, true, false),
         "Scanline Sync enabled but unlocked, no rate control: bridges the "
         "recalibration at the display rate");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_VSYNC, false, false, false, true),
         "vsync already paces: must not double up");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_AUDIO, false, false, false, true),
         "audio already paces: must not double up");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_TIMER, false, false, false, true),
         "the frame limiter already paces: must not double up");
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_NOWINDOW, false, false, false, true),
         "the no-window wait already paces: must not double up");

   printf("   gap predicate: 512 combinations, exactly three engage\n");
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

/* --- the schedule --------------------------------------------------- */

static void test_schedule(void)
{
   const int64_t period = 16683350; /* 59.94 Hz, in nanoseconds */
   int64_t anchor;
   retro_time_t sleep;
   unsigned i;

   /* On time: the sleep is what is left, rounded up to a microsecond,
    * and the anchor is the slot. */
   anchor = 1000000000LL;
   sleep  = runloop_pace_schedule(&anchor, period, 1000000 + 12000);
   check(sleep == 4684 && anchor == 1000000000LL + period,
         "on time: sleep to the slot, rounded up, anchor on it");

   /* Late by less than a period: no sleep, and the anchor stays on the
    * slot, so the next frame is due a period after it, not after now -
    * the lateness is caught up, not kept. */
   anchor = 1000000000LL;
   sleep  = runloop_pace_schedule(&anchor, period, 1000000 + 16683 + 400);
   check(sleep == 0 && anchor == 1000000000LL + period,
         "late by less than a period: anchor stays on the slot");
   sleep  = runloop_pace_schedule(&anchor, period, 1000000 + 16683 + 400 + 15000);
   check(sleep == 1284,
         "the next frame gets a sleep shorter by the lateness");

   /* Late by a period or more: a stall; the schedule restarts from now
    * rather than trying to fit two frames into one. */
   anchor = 1000000000LL;
   sleep  = runloop_pace_schedule(&anchor, period, 1000000 + 3 * 16683);
   check(sleep == 0 && anchor == (1000000 + 3 * 16683) * 1000LL,
         "late by a period or more: restart from now");

   /* A sleep that overshoots every frame does not slow the loop, and
    * a period that is not a whole microsecond is kept exactly: over
    * 6000 frames the slots are exactly 6000 periods apart, to the
    * nanosecond - a whole-microsecond period would be 2.1 ms behind
    * by then, 21 ppm. */
   {
      const retro_time_t overshoot = 900;   /* a coalesced nanosleep */
      const retro_time_t work      = 15000; /* the frame's own time  */
      retro_time_t now             = 0;
      anchor                       = 0;
      for (i = 0; i < 6000; i++)
      {
         now  += work;
         sleep = runloop_pace_schedule(&anchor, period, now);
         if (sleep > 0)
            now += sleep + overshoot;
      }
      check(anchor == 6000 * period,
            "6000 overshooting frames land on the 6000th slot, to the nanosecond");
   }
}

/* --- the sleep margin ------------------------------------------------ */

static void test_margin(void)
{
   const retro_time_t period = 16683;
   retro_time_t margin = 0;
   unsigned i;

   /* Up at once. */
   margin = runloop_pace_margin_update(margin, 700, period);
   check(margin == 700, "one overshoot of 700 us sets the margin to 700");
   /* Down slowly: sixteen quiet sleeps take off well under all of it. */
   for (i = 0; i < 16; i++)
      margin = runloop_pace_margin_update(margin, 0, period);
   check(margin > 200 && margin < 400,
         "sixteen quiet sleeps leave the margin at about a third");
   /* Never negative, never past a quarter period. */
   check(runloop_pace_margin_update(100, -5000, period) < 100,
         "a negative overshoot counts as none");
   check(runloop_pace_margin_update(0, 100000, period) == period / 4,
         "a huge overshoot is capped at a quarter period");
   /* Settles on a steady overshoot. */
   margin = 0;
   for (i = 0; i < 200; i++)
      margin = runloop_pace_margin_update(margin, 250 + (i & 1) * 50, period);
   check(margin >= 250 && margin <= 300,
         "a 250-300 us overshoot settles the margin between them");
}

int main(void)
{
   printf("runloop pacing decisions:\n");

   test_gap_predicate();
   test_frame_period();
   test_sample_filter();
   test_schedule();
   test_margin();

   if (failures)
   {
      printf("FAILED: %u check(s)\n", failures);
      return 1;
   }

   printf("ok: the gap limiter engages only when nothing else paces and "
          "fast-forward is off, the period is always a sane frame, a "
          "stall never moves the measured rate, an overshooting sleep "
          "never slows the loop, and the margin follows the overshoot\n");
   return 0;
}
