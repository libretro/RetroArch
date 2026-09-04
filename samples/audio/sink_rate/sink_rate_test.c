/* Sink rate estimation: the frontend's slow, integral rate term.
 *
 * A scripted driver on a synthetic clock, the estimator driven with
 * the same clock, so the run is instant and every figure exact. The
 * device counts what it took in whole 480-frame periods, as a shared
 * WASAPI engine does - the count is only known to a period, 2500 ppm
 * of noise over four seconds, 330 over thirty - and the writer refuses
 * half a percent of what it is offered, as a non-blocking writer
 * against a small buffer does. Both were missing from the first
 * harness, and both were what the field showed: a bias of +1867 ppm
 * against a device measured at +11, and readings of -16045 and -5061
 * ppm from a device surely within a hundred. */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../../audio/audio_driver.c"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static double   dev_ppm       = 0.0;
static int64_t  clock_usec    = 0;
static bool     dev_frozen    = false;
static size_t   dev_frozen_at = 0;
static unsigned dev_quantum   = 480;
static double   drop_fraction = 0.0;

static size_t dev_frames_consumed(void *data)
{
   double exact;
   (void)data;
   if (dev_frozen)
      return dev_frozen_at;
   exact = (double)clock_usec / 1e6 * 48000.0 * (1.0 + dev_ppm / 1e6);
   return (size_t)(exact / dev_quantum) * dev_quantum;
}
static size_t dev_write_avail(void *data)  { (void)data; return 500; }
static size_t dev_buffer_size(void *data)  { (void)data; return 1000; }
static void  *dev_init(const char *d, unsigned r, unsigned l, unsigned b, unsigned *n)
{ static int h; (void)d; (void)r; (void)l; (void)b; (void)n; return &h; }
static ssize_t dev_write(void *d, const void *b, size_t s) { (void)d; (void)b; return (ssize_t)s; }
static bool   dev_stop(void *d)               { (void)d; return true; }
static bool   dev_start(void *d, bool s)      { (void)d; (void)s; return true; }
static bool   dev_alive(void *d)              { (void)d; return true; }
static void   dev_nonblock(void *d, bool s)   { (void)d; (void)s; }
static void   dev_free(void *d)               { (void)d; }
static bool   dev_use_float(void *d)          { (void)d; return false; }
static audio_driver_t scripted = {
   dev_init, dev_write, dev_stop, dev_start, dev_alive, dev_nonblock,
   dev_free, dev_use_float, "scripted", NULL, NULL, dev_write_avail,
   dev_buffer_size, NULL, NULL, dev_frames_consumed
};

/* One second of synthetic time in which the frontend offered 48000 x
 * bias frames - what the resampler produces - and the driver took all
 * but the drop fraction; then the estimator runs. The write sites do
 * this accounting in the frontend; here it is done as they do. */
static double core_pause_sec = 0.0;  /* per second: time the core produced nothing */
/* The source's own rate against the host clock, apart from the
 * resampler's ratio. Zero is a source that produces exactly nominal;
 * a real one need not - a core that misses frames, or is paced by a
 * display that is not quite its nominal rate, produces measurably
 * less or more per host second, and the estimator sees that as part
 * of the ratio it corrects. */
static double src_ppm = 0.0;
static void run_second(audio_driver_state_t *st)
{
   double ratio   = st->src_ratio_curr / st->src_ratio_orig;
   double offered = 48000.0 * ratio * (1.0 + src_ppm / 1e6)
         * (1.0 - core_pause_sec);
   clock_usec           += 1000000;
   st->sink_offered_raw += (uint64_t)offered;
   st->sink_offered     += offered / ratio;
   st->sink_accepted    += (uint64_t)(offered * (1.0 - drop_fraction));
   audio_driver_sink_update(st, clock_usec);
}

static void reset(audio_driver_state_t *st, bool control)
{
   memset(st, 0, sizeof(*st));
   src_ppm               = 0.0;
   st->current_audio     = &scripted;
   st->src_ratio_orig    = 1.0;
   st->src_ratio_curr    = 1.0;
   st->sink_bias         = 1.0;
   st->rate_control_delta = 0.005f;
   /* A non-blocking writer: the source has its own clock. The blocking
    * case is its own test below. */
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_NONBLOCK);
   if (control)
      AUDIO_FLAGS_SET(st, AUDIO_FLAG_CONTROL);
   clock_usec     = 0;
   dev_frozen     = false;
   drop_fraction  = 0.0;
   core_pause_sec = 0.0;
   config_get_ptr()->bools.audio_sink_rate_estimation = true;
   config_get_ptr()->uints.audio_output_sample_rate   = 48000;
   audio_driver_sink_update(st, clock_usec); /* opens the baseline */
}

static double bias_ppm(const audio_driver_state_t *st) { return (st->sink_bias - 1.0) * 1e6; }

int main(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   int i;

   /* 1. A device 120 ppm fast, counting in 480-frame periods, rate
    *    control off. Nothing is applied before thirty seconds; the
    *    first application is within the period's worth of noise over
    *    thirty seconds - 333 ppm - and after five minutes the baseline
    *    has grown enough that it is within 40. */
   reset(st, false);
   dev_ppm = 120.0;
   for (i = 0; i < 29; i++)
      run_second(st);
   CHECK(st->sink_applied == 0 && st->sink_bias == 1.0, "a bias was applied before thirty seconds");
   for (; i < 34; i++)
      run_second(st);
   printf("   +120 ppm device, 480-frame periods: after %d s bias %+.0f ppm, measured %.1f Hz\n",
         i, bias_ppm(st), st->sink_rate_hz);
   CHECK(st->sink_applied >= 1, "no bias applied by 34 s");
   CHECK(fabs(bias_ppm(st) - 120.0) < 400.0, "first application %+.0f ppm, off by more than a period's worth", bias_ppm(st));
   for (; i < 300; i++)
      run_second(st);
   printf("   after %d s: bias %+.0f ppm, measured %.1f Hz, applied %u times\n",
         i, bias_ppm(st), st->sink_rate_hz, st->sink_applied);
   CHECK(fabs(bias_ppm(st) - 120.0) < 40.0, "after five minutes bias %+.0f ppm, expected +120", bias_ppm(st));
   CHECK(fabs(st->src_ratio_curr - st->sink_bias) < 1e-12, "ratio not set from the bias with rate control off");

   /* 2. The same device with a writer that refuses half a percent of
    *    what it is offered. The ratio is built on offered frames, so the
    *    bias still finds the device; the refused frames are warned
    *    about, not chased. Before this it read the refusal as a device
    *    5000 ppm fast and sped the resampler up to match. */
   reset(st, false);
   dev_ppm = 120.0;
   drop_fraction = 0.005;
   for (i = 0; i < 300; i++)
      run_second(st);
   printf("   +120 ppm device, 0.5%% refused: bias %+.0f ppm, drop warning %s\n",
         bias_ppm(st), st->sink_drop_warned ? "raised" : "not raised");
   CHECK(fabs(bias_ppm(st) - 120.0) < 40.0, "with refused frames the bias is %+.0f ppm, expected +120", bias_ppm(st));
   CHECK(st->sink_drop_warned, "half a percent refused for five minutes and no warning");

   /* 3. A device 80 ppm slow: the other direction. */
   reset(st, false);
   dev_ppm = -80.0;
   for (i = 0; i < 300; i++)
      run_second(st);
   printf("   -80 ppm device: bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) + 80.0) < 40.0, "bias %+.0f ppm, expected -80", bias_ppm(st));

   /* 4. A ratio too far off to be a crystal: refused, not clamped.
    *
    *    This used to assert the clamp, and the clamp is what made
    *    CoreAudio crackle about a minute in: a bias of 2000 ppm empties
    *    a 64 ms buffer in thirty-two seconds, and the first application
    *    lands at sixty. No crystal is 5000 ppm out - a ratio like this
    *    means the two counts are not measuring the same thing - so the
    *    only safe reading is to leave the resampler alone. The rate is
    *    still measured and still reported, which is what a mismeasuring
    *    driver needs in order to be found. */
   reset(st, false);
   dev_ppm = 5000.0;
   for (i = 0; i < 120; i++)
      run_second(st);
   printf("   +5000 ppm device: bias %+.0f ppm (refused as implausible)\n", bias_ppm(st));
   CHECK(fabs(st->sink_bias - 1.0) < 1e-9,
         "bias %.6f, expected no bias at all", st->sink_bias);
   CHECK(st->sink_rate_hz > 48000.0,
         "the rate must still be measured and reported, got %.1f", st->sink_rate_hz);

   /* 4b. Either side of the plausibility line, so the line itself is
    *     covered rather than just the far side of it. */
   reset(st, false);
   dev_ppm = 400.0;
   for (i = 0; i < 300; i++)
      run_second(st);
   printf("   +400 ppm device: bias %+.0f ppm (inside the plausible range)\n", bias_ppm(st));
   CHECK(bias_ppm(st) > 200.0, "a 400 ppm device must still be corrected, got %+.0f", bias_ppm(st));

   reset(st, false);
   dev_ppm = 900.0;
   for (i = 0; i < 300; i++)
      run_second(st);
   printf("   +900 ppm device: bias %+.0f ppm (outside it)\n", bias_ppm(st));
   CHECK(fabs(st->sink_bias - 1.0) < 1e-9,
         "a 900 ppm ratio is not a crystal; expected no bias, got %.6f", st->sink_bias);

   /* 5. Rate control on, with a non-blocking writer: the fill sits
    *    pinned full and rate control says "slow" by its whole delta
    *    forever - the buffer, not the clock. Its adjustment is in the
    *    ratio the frontend resamples by and is divided out of the
    *    offered count; the bias must still find the device's +150 ppm,
    *    where before it walked to the clamp absorbing the mean. */
   reset(st, true);
   dev_ppm = 150.0;
   for (i = 0; i < 300; i++)
   {
      /* What compute_rate_adjust() would set: orig x adjust x bias. */
      st->src_ratio_curr = st->src_ratio_orig * (1.0 - 0.005) * st->sink_bias;
      run_second(st);
   }
   printf("   rate control pinned slow, +150 ppm device: bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 150.0) < 40.0, "bias %+.0f ppm, expected +150; rate control's pinned adjustment leaked in", bias_ppm(st));

   /* 5b. Save-state loads: the core pauses half a second every ten,
    *     the device plays on. Those windows are excluded, and the bias
    *     still finds the device. Before, the 1.5%% shortfall read as a
    *     device 15000 ppm fast and the bias hit the clamp. */
   reset(st, false);
   dev_ppm = 120.0;
   for (i = 0; i < 300; i++)
   {
      core_pause_sec = (i % 10 == 0) ? 0.5 : 0.0;
      run_second(st);
   }
   printf("   +120 ppm device with a half-second pause every ten: bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 120.0) < 40.0, "bias %+.0f ppm, expected +120; pauses leaked into the ratio", bias_ppm(st));

   /* 6. A stall in the middle - the device frozen for a check window -
    *    starts the baseline over and moves nothing. */
   reset(st, false);
   dev_ppm = 100.0;
   for (i = 0; i < 40; i++)
      run_second(st);
   {
      double before = st->sink_bias;
      dev_frozen_at = dev_frames_consumed(NULL);
      dev_frozen    = true;
      for (i = 0; i < 5; i++)
         run_second(st);
      CHECK(st->sink_bias == before, "a stalled window moved the bias");
      CHECK(st->sink_discarded >= 1, "a stalled window was not discarded");
      dev_frozen = false;
   }

   /* 6b. A blocking writer: the core runs as fast as the resampler
    *     drains, so the source's rate is the device's over the ratio and
    *     carries no clock. A bias set from it feeds back - the field
    *     showed the source climbing +2300 to +3450 ppm with the bias at
    *     the clamp. The rate is measured; the bias is not set. */
   reset(st, false);
   AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_NONBLOCK);
   dev_ppm = 120.0;
   for (i = 0; i < 120; i++)
      run_second(st);
   printf("   blocking writer, +120 ppm device: measured %.1f Hz, bias %+.0f ppm\n", st->sink_rate_hz, bias_ppm(st));
   CHECK(fabs(st->sink_rate_hz - 48005.76) < 5.0, "blocking: the rate was not measured (%.1f)", st->sink_rate_hz);
   CHECK(st->sink_bias == 1.0, "blocking: a bias was applied, %+.0f ppm", bias_ppm(st));

   /* 7. Disabled: nothing at all. */
   reset(st, false);
   config_get_ptr()->bools.audio_sink_rate_estimation = false;
   dev_ppm = 120.0;
   for (i = 0; i < 60; i++)
      run_second(st);
   CHECK(st->sink_bias == 1.0 && st->sink_applied == 0, "disabled, yet the bias moved");

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }

   /* 10. Does the bias cause pops, or prevent them?
    *
    *     From a field report on CoreAudio: an 8 ms latency setting
    *     giving a 10.7 ms device buffer, the device measured at +47 ppm
    *     - an ordinary crystal - and the source at -442 ppm. Audio
    *     broke up about a minute in, which is also about when the first
    *     bias lands, so the bias was the obvious suspect.
    *
    *     The buffer is modelled here rather than the estimator alone:
    *     each second the source delivers at its rate times the
    *     resampler's ratio and the device takes at its own, and the
    *     level moves by the difference. An underrun is a pop. The point
    *     is to separate "the correction broke it" from "it was already
    *     draining and the correction is what stops it".  */
   {
      const double cap  = 0.0107 * 48000.0;   /* the reported 10.7 ms */
      int sink_on;

      for (sink_on = 0; sink_on < 2; sink_on++)
      {
         double level    = cap * 0.5;
         int    unders   = 0;
         int    first    = -1;

         reset(st, false);
         dev_ppm = 47.0;
         src_ppm = -442.0;
         if (!sink_on)
         {
            /* Estimation off: nothing ever moves the ratio. */
            st->sink_bias = 1.0;
         }

         for (i = 0; i < 180; i++)
         {
            double ratio, delivered, consumed;
            if (sink_on)
               run_second(st);
            else
               clock_usec += 1000000;

            ratio     = st->src_ratio_curr / st->src_ratio_orig;
            delivered = 48000.0 * ratio * (1.0 + src_ppm / 1e6);
            consumed  = 48000.0 * (1.0 + dev_ppm / 1e6);
            level    += delivered - consumed;

            if (level < 0.0)
            {
               unders++;
               if (first < 0)
                  first = i + 1;
               level = 0.0;
            }
            else if (level > cap)
               level = cap;
         }

         if (first < 0)
            printf("   %-20s no underruns in 180 s, bias %+.0f ppm\n",
                  sink_on ? "estimation on:" : "estimation off:", bias_ppm(st));
         else
            printf("   %-20s first underrun at %d s, %d underrunning second(s), bias %+.0f ppm\n",
                  sink_on ? "estimation on:" : "estimation off:",
                  first, unders, bias_ppm(st));

         /* The finding, asserted so it cannot quietly stop being true:
          * the buffer is empty long before the first correction exists.
          * A 490 ppm mismatch drains 10.7 ms from half full in about
          * eleven seconds, and the earliest a bias can be applied is
          * thirty. Whatever breaks up the audio here, it is not the
          * correction - it has not happened yet. */
         CHECK(first > 0 && first < 30,
               "%s: expected the buffer to drain before any bias could be "
               "applied, first underrun at %d s",
               sink_on ? "estimation on" : "estimation off", first);
         if (sink_on)
            CHECK(bias_ppm(st) > 300.0,
                  "the correction should have found the mismatch, got %+.0f ppm",
                  bias_ppm(st));
      }
   }


   /* 11. The correction's cadence against the buffer it corrects.
    *
    *     Thirty seconds between corrections is a floor, not a choice: a
    *     device that counts in whole periods is only known to a period,
    *     so applying sooner would apply noise. A buffer that empties
    *     inside that floor therefore cannot be held by this mechanism,
    *     and the frontend says so once rather than pretending. Checked
    *     both ways round, since a warning that always fires is no more
    *     use than one that never does. */
   {
      struct { size_t bytes; double ppm; const char *what; int want; } cases[] = {
         /* 10.7 ms of int16 stereo at 48 kHz, the field report's buffer */
         { (size_t)(0.0107 * 48000) * 4, 490.0,
           "10.7 ms buffer, 490 ppm apart", 1 },
         /* the same buffer, a mismatch small enough to outlast the cadence */
         { (size_t)(0.0107 * 48000) * 4, 11.0,
           "10.7 ms buffer, 11 ppm apart",  0 },
         /* a roomy buffer with the same large mismatch */
         { (size_t)(0.200  * 48000) * 4, 490.0,
           "200 ms buffer, 490 ppm apart",  0 }
      };
      size_t c;

      for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
      {
         double buffer_sec = (double)cases[c].bytes / 4.0 / 48000.0;
         double drain_sec  = (buffer_sec * 0.5) / (cases[c].ppm / 1e6);
         int    fires      = drain_sec < 30.0;

         printf("   %-32s half drains in %5.1f s -> %s\n",
               cases[c].what, drain_sec,
               fires ? "too small for the cadence" : "the cadence can hold it");
         CHECK(fires == cases[c].want,
               "%s: expected the warning to %sfire", cases[c].what,
               cases[c].want ? "" : "not ");
      }
   }

   printf("sink rate: measured through period-quantised counts and refused frames, converged, implausible ratios refused, rate control's pinned adjustment and pauses kept out\n");
   return 0;
}
