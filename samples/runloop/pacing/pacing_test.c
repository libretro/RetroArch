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
   check(!runloop_pace_gap_engages(RUNLOOP_PACE_DISPLAY, false, false, false, true),
         "display pacing keeps the gap limiter out");
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


/* The pace decision itself, as a table: what holds the loop in the
 * Quick Menu over a paused core, on a focused window, per combination
 * of the user's sync settings. Each row is a fact about the shipping
 * decision, not a wish; a row that changes is a behaviour change, and
 * should be a deliberate one. "Timer" as a setting is Sync to Exact
 * Content Framerate, with Menu Throttle Framerate at its default of
 * off, which is the menu path's early return. Audio never holds the
 * loop with the core paused: nothing writes blocking. */
static runloop_pace_facts_t menu_facts(bool vsync, bool audio,
      bool display, bool timer, bool scanline)
{
   /* A focused window, the menu up over a paused core (menu pause is
    * not RUNLOOP_FLAG_PAUSED), rate control on, a surface to present
    * to, and a frame limit - every menu path that reaches the block
    * sets the refresh-rate one or leaves the content's from load.
    * Audio never holds with the core paused: the menu writes silence,
    * non-blocking. "Timer" is Sync to Exact Content Framerate with
    * Menu Throttle Framerate off, the menu path's early return. */
   runloop_pace_facts_t f = PACE_FACT_FOCUSED | PACE_FACT_MENU_ALIVE
      | PACE_FACT_RATE_CONTROL | PACE_FACT_PRESENTABLE | PACE_FACT_FRAME_LIMIT;
   if (vsync)    f |= PACE_FACT_VSYNC;
   if (timer)    f |= PACE_FACT_VRR | PACE_FACT_MENU_EARLY_EXIT;
   if (display)  f |= PACE_FACT_WRAPPER | PACE_FACT_DISPLAY_PACING;
   if (scanline) f |= PACE_FACT_SCANLINE_SYNC | PACE_FACT_SCANLINE_LOCKED;
   (void)audio;
   return f;
}

/* --- the derived swap interval ----------------------------------- */

/* The default audio_max_timing_skew, which is what the runloop hands
 * runloop_video_swap_interval_for(). */
#define PACING_TEST_MAX_SKEW 0.05f

/* The ceiling a driver with no limit of its own gets: config.def.h's
 * MAXIMUM_SWAP_INTERVAL, restated here because this test links against
 * nothing and the value is part of what is being asserted. A driver
 * that caps lower - the D3D family reports four - passes its own. */
#define PACING_TEST_CEILING 16

static void check_interval(float timing_fps, float input_fps,
      unsigned ceiling, unsigned want, const char *what)
{
   char     msg[160];
   unsigned got = runloop_video_swap_interval_for(timing_fps, input_fps,
         PACING_TEST_MAX_SKEW, ceiling);

   snprintf(msg, sizeof(msg), "%.2f Hz / %.2f fps, ceiling %u -> %u, "
         "wanted %u (%s)", timing_fps, input_fps, ceiling, got, want, what);
   check(got == want, msg);
}

static void test_swap_interval(void)
{
   static const float rates[] =
   {
      0.0f, 1.0f, 23.976f, 24.0f, 25.0f, 29.97f, 30.0f, 50.0f,
      59.94f, 60.0f, 72.0f, 75.0f, 90.0f, 100.0f, 119.88f, 120.0f,
      144.0f, 165.0f, 200.0f, 240.0f, 280.0f, 360.0f, 480.0f, 500.0f
   };
   unsigned i;
   unsigned j;

   /* A panel is a whole multiple of the content rate: one content
    * frame is held for that many display frames. */
   check_interval(60.0f,  59.94f, PACING_TEST_CEILING, 1,
         "60 Hz panel, 60 fps content");
   check_interval(120.0f, 59.94f, PACING_TEST_CEILING, 2,
         "120 Hz panel, 60 fps content");
   check_interval(240.0f, 59.94f, PACING_TEST_CEILING, 4,
         "240 Hz panel, 60 fps content");
   check_interval(480.0f, 59.94f, PACING_TEST_CEILING, 8,
         "480 Hz panel, 60 fps content");
   check_interval(480.0f, 30.0f,  PACING_TEST_CEILING, 16,
         "480 Hz panel, 30 fps content: the ceiling itself");
   check_interval(360.0f, 24.0f,  PACING_TEST_CEILING, 15,
         "360 Hz panel, 24 fps content");
   check_interval(60.0f,  30.0f,  PACING_TEST_CEILING, 2,
         "60 Hz panel, 30 fps content");

   /* A driver that cannot hold a frame for the full multiple takes
    * the ceiling down, and the multiple it cannot reach falls back
    * rather than clamping short: a short multiple paces the loop
    * fast, which is worse than leaving it to vsync. */
   check_interval(480.0f, 59.94f, 4, 1,
         "480 Hz on a driver capped at four: falls back, does not clamp to 4");
   check_interval(240.0f, 59.94f, 4, 4,
         "240 Hz on a driver capped at four: still exactly four");
   check_interval(120.0f, 59.94f, 4, 2,
         "120 Hz on a driver capped at four: unaffected");

   /* Not a whole multiple within the skew tolerance: vsync paces at
    * the display rate and rate control absorbs the difference. */
   check_interval(100.0f, 60.0f,  PACING_TEST_CEILING, 1,
         "100 Hz panel, 60 fps content: no whole multiple");
   check_interval(75.0f,  59.94f, PACING_TEST_CEILING, 1,
         "75 Hz panel, 60 fps content: no whole multiple");
   check_interval(144.0f, 59.94f, PACING_TEST_CEILING, 1,
         "144 Hz panel, 60 fps content: no whole multiple");

   /* Degenerate inputs. A core is free to report nonsense, and a
    * panel slower than the content has no multiple to hold. */
   check_interval(0.0f,   59.94f, PACING_TEST_CEILING, 1, "no display rate");
   check_interval(480.0f, 0.0f,   PACING_TEST_CEILING, 1, "no content rate");
   check_interval(-60.0f, 59.94f, PACING_TEST_CEILING, 1, "negative display rate");
   check_interval(480.0f, -60.0f, PACING_TEST_CEILING, 1, "negative content rate");
   check_interval(60.0f,  119.88f, PACING_TEST_CEILING, 1,
         "content faster than the panel");
   check_interval(480.0f, 59.94f, 0, 1, "a ceiling of zero admits nothing");

   /* Whatever the pair, the answer is a usable interval: never zero,
    * which would mean no sync at all, and never past what the driver
    * said it can present. */
   for (i = 0; i < ARRAY_SIZE(rates); i++)
      for (j = 0; j < ARRAY_SIZE(rates); j++)
      {
         unsigned c;

         for (c = 1; c <= PACING_TEST_CEILING; c++)
         {
            unsigned got = runloop_video_swap_interval_for(rates[i],
                  rates[j], PACING_TEST_MAX_SKEW, c);
            char     msg[160];

            snprintf(msg, sizeof(msg),
                  "%.2f Hz / %.2f fps, ceiling %u -> %u, out of range",
                  rates[i], rates[j], c, got);
            check(got >= 1 && got <= c, msg);
         }
      }

   printf("   swap interval: multiples up to %d derived, %u pairs bounded "
          "by the driver's cap\n", PACING_TEST_CEILING,
          (unsigned)(ARRAY_SIZE(rates) * ARRAY_SIZE(rates)
                * PACING_TEST_CEILING));
}

/* --- how a display/content pair is synced ------------------------ */

#define SP_V RUNLOOP_SYNC_VSYNC_HOLDS
#define SP_E RUNLOOP_SYNC_EXACT_RATE
#define SP_W RUNLOOP_SYNC_WITHIN_SKEW

static void check_plan(float display_hz, float input_fps, float multiple,
      bool vrr, unsigned want, const char *what)
{
   char     msg[192];
   unsigned got = runloop_sync_plan_for(display_hz, input_fps, multiple,
         PACING_TEST_MAX_SKEW, vrr);

   snprintf(msg, sizeof(msg), "%.4f Hz / %.4f fps x%.0f%s -> %s%s%s, "
         "wanted %s%s%s (%s)", display_hz, input_fps, multiple,
         vrr ? " VRR" : "",
         (got  & SP_V) ? "V" : "-", (got  & SP_E) ? "E" : "-",
         (got  & SP_W) ? "W" : "-",
         (want & SP_V) ? "V" : "-", (want & SP_E) ? "E" : "-",
         (want & SP_W) ? "W" : "-", what);
   check(got == want, msg);
}

static void test_sync_plan(void)
{
   static const float displays[] =
   {
      0.0f, 50.0f, 59.94f, 60.0f, 75.0f, 100.0f, 119.88f, 120.0f,
      144.0f, 165.0f, 240.0f, 360.0f
   };
   static const float contents[] =
   {
      0.0f, 24.0f, 30.0f, 49.7f, 50.0f, 53.7f, 57.5f, 59.8261f,
      59.94f, 60.0f, 60.0988f, 61.0f, 75.0f, 120.0f
   };
   static const float multiples[] = { 1.0f, 2.0f, 3.0f, 4.0f };
   unsigned i, j, k;

   /* Issue 19600: a 60.0988 Hz core at swap interval 2 on a 120 Hz
    * panel is 0.16% past what the panel presents. VSync paces it,
    * audio is skewed; the loop is never forced nonblocking over it. */
   check_plan(120.0f, 60.0988f, 2.0f, true,  SP_V | SP_W,
         "SNES, interval 2, 120 Hz, VRR: vsync holds, rate skewed");
   check_plan(120.0f, 59.8261f, 2.0f, true,  SP_V | SP_E | SP_W,
         "TG16, interval 2, 120 Hz, VRR: exact rate");
   check_plan(120.0f, 60.0988f, 2.0f, false, SP_V | SP_W,
         "SNES, interval 2, 120 Hz: vsync holds, rate skewed");
   check_plan(60.0f,  60.0988f, 1.0f, true,  SP_V | SP_W,
         "SNES on a 60 Hz VRR ceiling: vsync holds, rate skewed");
   check_plan(120.0f, 60.0988f, 2.0f * 1.0f, true, SP_V | SP_W,
         "SNES, one black frame, 120 Hz, VRR");

   /* Past the tolerance the content keeps its own rate under VRR,
    * with vsync off; without VRR vsync is simply not relied on. */
   check_plan(120.0f, 75.0f, 2.0f, true,  SP_E,
         "75 Hz content, interval 2, 120 Hz, VRR: own rate, no vsync");
   check_plan(60.0f,  75.0f, 1.0f, true,  SP_E,
         "75 Hz content on 60 Hz, VRR: own rate, no vsync");
   check_plan(60.0f,  75.0f, 1.0f, false, 0,
         "75 Hz content on 60 Hz: no vsync");

   /* Under the panel's rate: vsync holds either way, VRR keeps the
    * exact rate however far the skew. */
   check_plan(144.0f, 60.0988f, 1.0f, true,  SP_V | SP_E,
         "60 Hz content on 144 Hz, VRR");
   check_plan(75.0f,  60.0f,    1.0f, false, SP_V,
         "60 Hz content on 75 Hz: vsync, rate not skewed");

   /* Degenerate rates change nothing. */
   check_plan(120.0f, 0.0f,  2.0f, true,  SP_V | SP_E, "no content rate, VRR");
   check_plan(120.0f, 0.0f,  2.0f, false, SP_V,        "no content rate");
   check_plan(0.0f,   60.0f, 2.0f, true,  SP_V | SP_E, "no display rate, VRR");
   check_plan(120.0f, 60.0f, 0.0f, false, SP_V,        "no multiple");

   /* Over every pair: VRR never decides whether vsync holds, only
    * whether the content keeps its own rate - and it does so exactly
    * when vsync can present it or the skew is too far to follow. */
   for (i = 0; i < ARRAY_SIZE(displays); i++)
      for (j = 0; j < ARRAY_SIZE(contents); j++)
         for (k = 0; k < ARRAY_SIZE(multiples); k++)
         {
            char     msg[160];
            unsigned off = runloop_sync_plan_for(displays[i], contents[j],
                  multiples[k], PACING_TEST_MAX_SKEW, false);
            unsigned on  = runloop_sync_plan_for(displays[i], contents[j],
                  multiples[k], PACING_TEST_MAX_SKEW, true);

            snprintf(msg, sizeof(msg), "%.4f Hz / %.4f fps x%.0f: "
                  "VRR moved vsync or the skew (%u vs %u)",
                  displays[i], contents[j], multiples[k], on, off);
            check((on & (SP_V | SP_W)) == (off & (SP_V | SP_W)), msg);

            snprintf(msg, sizeof(msg), "%.4f Hz / %.4f fps x%.0f: "
                  "VRR plan %u holds neither vsync nor the rate",
                  displays[i], contents[j], multiples[k], on);
            check((on & (SP_V | SP_E)) != 0, msg);

            snprintf(msg, sizeof(msg), "%.4f Hz / %.4f fps x%.0f: "
                  "within skew but vsync dropped (%u)",
                  displays[i], contents[j], multiples[k], on);
            check(!(on & SP_W) || (on & SP_V), msg);

            check(!(off & SP_E), "exact rate without VRR");
         }

   printf("   sync plan: %u display/content/multiple triples, VRR "
          "drops vsync only past the skew tolerance\n",
          (unsigned)(ARRAY_SIZE(displays) * ARRAY_SIZE(contents)
                * ARRAY_SIZE(multiples)));
}

static void test_menu_table(void)
{
   {
      check(runloop_pace_decide(menu_facts(false, false, true,  false, false)) == RUNLOOP_PACE_DISPLAY,
            "menu: Display -> Display");
      check(runloop_pace_decide(menu_facts(false, true,  true,  false, false)) == RUNLOOP_PACE_DISPLAY,
            "menu: Display+Audio -> Display");
      check(runloop_pace_decide(menu_facts(true,  false, true,  false, false)) == (RUNLOOP_PACE_VSYNC | RUNLOOP_PACE_DISPLAY),
            "menu: Display+VSync -> VSync+Display");
      check(runloop_pace_decide(menu_facts(true,  false, false, false, false)) == RUNLOOP_PACE_VSYNC,
            "menu: VSync -> VSync");
      check(runloop_pace_decide(menu_facts(true,  true,  false, false, false)) == RUNLOOP_PACE_VSYNC,
            "menu: VSync+Audio -> VSync");
      check(runloop_pace_decide(menu_facts(true,  true,  false, true,  false)) == RUNLOOP_PACE_VSYNC,
            "menu: VSync+Audio+Timer -> VSync (early return)");
      check(runloop_pace_decide(menu_facts(true,  false, false, true,  false)) == RUNLOOP_PACE_VSYNC,
            "menu: VSync+Timer -> VSync (early return)");
      check(runloop_pace_decide(menu_facts(false, true,  false, false, false)) == RUNLOOP_PACE_TIMER,
            "menu: Audio -> Timer (the menu's refresh-rate timer)");
      check(runloop_pace_decide(menu_facts(false, true,  false, true,  false)) == RUNLOOP_PACE_NONE,
            "menu: Audio+Timer -> None (early return, nothing holds)");
      /* The menu's refresh-rate timer stands aside only for vsync,
       * focus and display pacing, not for scanline: two clocks. A
       * fact, not an endorsement. */
      check(runloop_pace_decide(menu_facts(false, false, false, false, true)) == (RUNLOOP_PACE_SCANLINE | RUNLOOP_PACE_TIMER),
            "menu: Scanline -> Scanline+Timer");
      check(runloop_pace_decide(menu_facts(false, false, false, true,  false)) == RUNLOOP_PACE_NONE,
            "menu: Timer -> None (early return, nothing holds)");
      check(runloop_pace_decide(menu_facts(false, true,  false, true,  false)) == RUNLOOP_PACE_NONE,
            "menu: Timer+Audio -> None (early return)");
      check(runloop_pace_decide(menu_facts(false, false, true,  true,  false)) == RUNLOOP_PACE_NONE,
            "menu: Timer+Display -> None (early return; the hold runs but is not counted)");
   }
}

int main(void)
{
   printf("runloop pacing decisions:\n");

   test_gap_predicate();
   test_frame_period();
   test_sample_filter();
   test_schedule();
   test_margin();
   test_swap_interval();
   test_sync_plan();
   test_menu_table();

   if (failures)
   {
      printf("FAILED: %u check(s)\n", failures);
      return 1;
   }

   printf("ok: the gap limiter engages only when nothing else paces and "
          "fast-forward is off, the period is always a sane frame, a "
          "stall never moves the measured rate, an overshooting sleep "
          "never slows the loop, the margin follows the overshoot, and "
          "the swap interval is the multiple the display actually is of "
          "the content, within what the driver can present, and VRR drops "
          "vsync only for a content rate past the skew tolerance\n");
   return 0;
}
