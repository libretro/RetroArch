/* Sink rate estimation: the frontend's slow, integral rate term, put
 * through its paces.
 *
 * A scripted driver on a synthetic clock, the estimator driven with
 * the same clock, so every run is instant and every figure exact. The
 * device counts what it took in whole 480-frame periods, as a shared
 * WASAPI engine does; the writer can refuse a fraction of what it is
 * offered, as a non-blocking writer against a small buffer does; the
 * core can pause, run off rate, or warm up; the threaded pipeline's
 * ring can fill and drain; the main thread can stall. Each scenario
 * is one of those, from a field report or a harness that found it,
 * and asserts what the estimator must do about it.
 *
 * Includes audio/audio_driver.c so the shipping estimator runs. */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../../audio/audio_driver.c"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the device and the source ---------------------------------------- */

static double   dev_ppm       = 0.0;     /* the device's clock against the host's */
static int64_t  clock_usec    = 0;
static bool     dev_frozen    = false;   /* the device stopped consuming */
static size_t   dev_frozen_at = 0;
static unsigned dev_quantum   = 480;     /* it counts in whole periods */
static double   drop_fraction = 0.0;     /* the writer refuses this much */
static double   core_pause_sec = 0.0;    /* per second: time the core produced nothing */
static double   src_ppm       = 0.0;     /* the source's own clock, apart from the ratio */

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

/* A span of synthetic time, as the write sites account for it: the
 * frontend offered 48000 x ratio frames a second, the driver took all
 * but the refused fraction, and the estimator ran. */
static void run_usec(audio_driver_state_t *st, int64_t usec)
{
   double ratio   = st->src_ratio_curr / st->src_ratio_orig;
   double span    = (double)usec / 1e6;
   double offered = 48000.0 * span * ratio * (1.0 + src_ppm / 1e6)
         * (1.0 - core_pause_sec);
   clock_usec           += usec;
   st->sink_offered_raw += (uint64_t)offered;
   st->sink_offered     += offered / ratio;
   st->sink_accepted    += (uint64_t)(offered * (1.0 - drop_fraction));
   audio_driver_sink_refused(st);
   audio_driver_sink_update(st, clock_usec);
}
static void run_second(audio_driver_state_t *st) { run_usec(st, 1000000); }
static void run_seconds(audio_driver_state_t *st, int n)
{
   int i;
   for (i = 0; i < n; i++)
      run_second(st);
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
   /* A non-blocking writer: the source has its own clock. */
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_NONBLOCK);
   if (control)
      AUDIO_FLAGS_SET(st, AUDIO_FLAG_CONTROL);
   clock_usec     = 0;
   dev_ppm        = 0.0;
   dev_frozen     = false;
   dev_quantum    = 480;
   drop_fraction  = 0.0;
   core_pause_sec = 0.0;
   config_get_ptr()->bools.audio_sink_rate_estimation = true;
   config_get_ptr()->uints.audio_output_sample_rate   = 48000;
   audio_driver_sink_update(st, clock_usec); /* opens the baseline */
}

static double bias_ppm(const audio_driver_state_t *st) { return (st->sink_bias - 1.0) * 1e6; }
static double dev_meas_ppm(const audio_driver_state_t *st) { return (st->sink_rate_hz / 48000.0 - 1.0) * 1e6; }
static double src_meas_ppm(const audio_driver_state_t *st) { return (st->sink_source_hz / 48000.0 - 1.0) * 1e6; }

/* --- the clocks ------------------------------------------------------- */

/* A device 120 ppm fast, counting in periods, rate control off. Nothing
 * before thirty seconds; the first application within a period's worth
 * of noise over thirty seconds; after five minutes within 40. */
static void s_fast_device(audio_driver_state_t *st)
{
   reset(st, false);
   dev_ppm = 120.0;
   run_seconds(st, 29);
   CHECK(st->sink_applied == 0 && st->sink_bias == 1.0, "a bias was applied before thirty seconds");
   run_seconds(st, 9);
   printf("      at 38 s: bias %+.0f ppm, device measured %+.0f ppm\n", bias_ppm(st), dev_meas_ppm(st));
   CHECK(st->sink_applied >= 1, "no bias applied by 38 s");
   CHECK(fabs(bias_ppm(st) - 120.0) < 400.0, "first application %+.0f ppm, off by more than a period's worth", bias_ppm(st));
   run_seconds(st, 262);
   printf("      at 300 s: bias %+.0f ppm, applied %u times\n", bias_ppm(st), st->sink_applied);
   CHECK(fabs(bias_ppm(st) - 120.0) < 40.0, "after five minutes bias %+.0f ppm, expected +120", bias_ppm(st));
   CHECK(fabs(audio_driver_sink_bias(st) - st->sink_bias) < 1e-7, "the bias the resampler reads is not the bias");
}

/* The other direction. */
static void s_slow_device(audio_driver_state_t *st)
{
   reset(st, false);
   dev_ppm = -80.0;
   run_seconds(st, 300);
   printf("      bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) + 80.0) < 40.0, "bias %+.0f ppm, expected -80", bias_ppm(st));
}

/* Two hours: the sums are doubles over a session, and must neither
 * drift nor lose the crystal. */
static void s_long_session(audio_driver_state_t *st)
{
   reset(st, false);
   dev_ppm = 37.0;
   run_seconds(st, 7200);
   printf("      after two hours: bias %+.0f ppm, applied %u times\n", bias_ppm(st), st->sink_applied);
   CHECK(fabs(bias_ppm(st) - 37.0) < 5.0, "after two hours bias %+.0f ppm, expected +37", bias_ppm(st));
}

/* The estimator is driven from every write; a core writing per frame
 * and one writing per second must measure the same clock. */
static void s_write_cadence(audio_driver_state_t *st)
{
   double per_frame, per_second;
   int i;
   reset(st, false);
   dev_ppm = 120.0;
   for (i = 0; i < 300 * 60; i++)
      run_usec(st, 16667);
   per_frame = bias_ppm(st);
   reset(st, false);
   dev_ppm = 120.0;
   run_seconds(st, 300);
   per_second = bias_ppm(st);
   printf("      per frame %+.0f ppm, per second %+.0f ppm\n", per_frame, per_second);
   CHECK(fabs(per_frame - per_second) < 30.0, "the write cadence changed the estimate: %+.0f vs %+.0f", per_frame, per_second);
}

/* --- what is not a clock ---------------------------------------------- */

/* A writer refusing half a percent: the ratio is on offered frames, so
 * the bias still finds the device; the refusal is warned about once. */
static void s_refused_frames(audio_driver_state_t *st)
{
   reset(st, false);
   dev_ppm = 120.0;
   drop_fraction = 0.005;
   run_seconds(st, 300);
   printf("      bias %+.0f ppm, drop warning %s\n", bias_ppm(st),
         (st->sink_warned & AUDIO_SINK_WARNED_DROPPED) ? "raised" : "not raised");
   CHECK(fabs(bias_ppm(st) - 120.0) < 40.0, "with refused frames the bias is %+.0f ppm, expected +120", bias_ppm(st));
   CHECK(st->sink_warned & AUDIO_SINK_WARNED_DROPPED, "half a percent refused for five minutes and no warning");
}

/* Too far off to be a crystal: refused, never clamped, and the rate is
 * still measured and shown so a mismeasuring driver can be found. */
static void s_implausible_device(audio_driver_state_t *st)
{
   reset(st, false);
   dev_ppm = 5000.0;
   run_seconds(st, 120);
   printf("      +5000 ppm: bias %+.0f ppm, device shown at %+.0f ppm\n", bias_ppm(st), dev_meas_ppm(st));
   CHECK(fabs(st->sink_bias - 1.0) < 1e-9, "bias %.6f, expected none at all", st->sink_bias);
   CHECK(fabs(dev_meas_ppm(st) - 5000.0) < 500.0, "the rate must still be measured and shown, got %+.0f", dev_meas_ppm(st));
   CHECK(st->sink_warned & AUDIO_SINK_WARNED_IMPLAUSIBLE, "no refusal was logged");

   reset(st, false);
   dev_ppm = 400.0;
   run_seconds(st, 300);
   printf("      +400 ppm: bias %+.0f ppm (inside the band)\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 400.0) < 60.0, "+400 ppm: bias %+.0f, expected +400", bias_ppm(st));

   reset(st, false);
   dev_ppm = 900.0;
   run_seconds(st, 300);
   printf("      +900 ppm: bias %+.0f ppm (outside it)\n", bias_ppm(st));
   CHECK(st->sink_bias == 1.0, "+900 ppm: bias %+.0f, expected refused", bias_ppm(st));
}

/* A source paced at the display's rate rather than its own - video
 * sync off, a 60.00 Hz panel against a 59.94 core - is +1000 ppm for
 * the whole session. It never comes into the band, no bias is ever
 * applied, the rates are shown, and it is said once. */
static void s_source_at_display_rate(audio_driver_state_t *st)
{
   reset(st, false);
   dev_ppm = 20.0;
   src_ppm = 1000.0;
   run_seconds(st, 180);
   printf("      bias %+.0f ppm, source shown at %+.0f ppm, device at %+.0f\n",
         bias_ppm(st), src_meas_ppm(st), dev_meas_ppm(st));
   CHECK(st->sink_bias == 1.0 && st->sink_applied == 0, "a display-paced source was biased: %+.0f ppm", bias_ppm(st));
   CHECK(fabs(src_meas_ppm(st) - 1000.0) < 100.0, "the source's rate is not shown: %+.0f", src_meas_ppm(st));
   CHECK(st->sink_warned & AUDIO_SINK_WARNED_UNSETTLED, "a source off the band for three minutes was never said");
}

/* Rate control on with a non-blocking writer: its adjustment sits at
 * its bound and is in the ratio; divided out of the offered count, the
 * bias must still find the device's +150. */
static void s_rate_control_pinned(audio_driver_state_t *st)
{
   int i;
   reset(st, true);
   dev_ppm = 150.0;
   for (i = 0; i < 300; i++)
   {
      st->src_ratio_curr = st->src_ratio_orig * (1.0 - 0.005) * st->sink_bias;
      run_second(st);
   }
   printf("      bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 150.0) < 40.0, "bias %+.0f ppm, expected +150; rate control's adjustment leaked in", bias_ppm(st));
}

/* The core pauses half a second every ten - save states loading - and
 * the device plays on. Those windows are out; the bias finds +120. */
static void s_core_pauses(audio_driver_state_t *st)
{
   int i;
   reset(st, false);
   dev_ppm = 120.0;
   for (i = 0; i < 300; i++)
   {
      core_pause_sec = (i % 10 == 0) ? 0.5 : 0.0;
      run_second(st);
   }
   core_pause_sec = 0.0;
   printf("      bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 120.0) < 40.0, "bias %+.0f ppm, expected +120; pauses leaked into the ratio", bias_ppm(st));
}

/* The main thread held for 50 ms once every two minutes - a state save,
 * a shader built on first use - with the device at rate. In a window
 * that is a source 1.25% slow; diluted it is -416 ppm, plausible. The
 * source has to be within the band in every kept window. */
static void s_main_thread_stalls(audio_driver_state_t *st)
{
   int i;
   reset(st, false);
   run_seconds(st, 40);
   for (i = 0; i < 240; i++)
   {
      core_pause_sec = (i % 120 == 0) ? 0.05 : 0.0;
      run_second(st);
   }
   core_pause_sec = 0.0;
   printf("      bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st)) < 60.0, "the stalls were summed as a slow source: bias %+.0f ppm", bias_ppm(st));
}

/* The device frozen for a window - a stall - moves nothing and is left
 * out; the estimate goes on from the next window. */
static void s_device_stall(audio_driver_state_t *st)
{
   double before;
   reset(st, false);
   dev_ppm = 100.0;
   run_seconds(st, 40);
   before        = st->sink_bias;
   dev_frozen_at = dev_frames_consumed(NULL);
   dev_frozen    = true;
   run_seconds(st, 5);
   CHECK(st->sink_bias == before, "a stalled window moved the bias");
   CHECK(st->sink_discarded >= 1, "a stalled window was not left out");
   dev_frozen = false;
   run_seconds(st, 200);
   printf("      bias %+.0f ppm after the stall\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 100.0) < 40.0, "after a stall the bias is %+.0f, expected +100", bias_ppm(st));
}

/* A source slow for its first half minute - a core warming up after
 * load - then at nominal. The sums open only once the source has been
 * in the band for two windows, so the warm-up is never in them. */
static void s_slow_start(audio_driver_state_t *st)
{
   reset(st, false);
   src_ppm = -1500.0;
   run_seconds(st, 12);
   CHECK(st->sink_rate_hz > 0.0 && src_meas_ppm(st) < -1000.0,
         "unsettled, the rates are not shown or wrong: device %.1f source %+.0f ppm", st->sink_rate_hz, src_meas_ppm(st));
   run_seconds(st, 18);
   src_ppm = 0.0;
   run_seconds(st, 90);
   printf("      bias %+.0f ppm at 120 s, source shown at %+.0f ppm\n", bias_ppm(st), src_meas_ppm(st));
   CHECK(fabs(bias_ppm(st)) < 60.0, "the slow start is in the estimate two minutes on: bias %+.0f ppm", bias_ppm(st));
   CHECK(st->sink_applied > 0, "the bias was never applied after the source settled");
   CHECK(fabs(src_meas_ppm(st)) < 100.0, "the shown source still carries the warm-up: %+.0f ppm", src_meas_ppm(st));
}

/* --- the threaded pipeline ---------------------------------------------- */

/* On the threaded pipeline the source is counted where it is exact:
 * what the core published into the ring, at the producer, where the
 * windows close. Neither the ring's occupancy nor what it refused is
 * in the count, so a window holds whole publishes. Modelled here as
 * publishes of 800 frames every 1/60 s with a device at +50 ppm: the
 * bias must find the device, not the burst. The end-to-end run, with
 * the shipping producer and consumer against a real-time device, is
 * samples/audio/pipeline_rate_control. */
static void s_publishes(audio_driver_state_t *st)
{
   int i;
   reset(st, true);
   st->pipe_threaded = true;
   dev_ppm = 50.0;
   for (i = 0; i < 300 * 60; i++)
   {
      clock_usec += 16667;
      st->sink_offered += 800.0 * st->src_ratio_orig;
      audio_driver_sink_update(st, clock_usec);
   }
   printf("      bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 50.0) < 40.0, "publishes measured as a source: bias %+.0f ppm, expected +50", bias_ppm(st));
   st->pipe_threaded = false;
}

/* --- modes and lifetimes ---------------------------------------------- */

/* A blocking writer: the core runs as fast as the resampler drains, so
 * the source carries no clock of its own and a bias would feed back.
 * The rate is measured and shown; nothing is applied. */
static void s_blocking_writer(audio_driver_state_t *st)
{
   reset(st, false);
   AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_NONBLOCK);
   dev_ppm = 120.0;
   run_seconds(st, 120);
   printf("      measured %+.0f ppm, bias %+.0f ppm\n", dev_meas_ppm(st), bias_ppm(st));
   CHECK(fabs(dev_meas_ppm(st) - 120.0) < 60.0, "blocking: the rate was not measured (%+.0f)", dev_meas_ppm(st));
   CHECK(st->sink_bias == 1.0, "blocking: a bias was applied, %+.0f ppm", bias_ppm(st));
}

/* Off: nothing at all, and no rates shown. Turned off mid-session the
 * bias is dropped at once; turned back on, it is measured afresh. */
static void s_disabled_and_toggled(audio_driver_state_t *st)
{
   reset(st, false);
   config_get_ptr()->bools.audio_sink_rate_estimation = false;
   dev_ppm = 120.0;
   run_seconds(st, 60);
   CHECK(st->sink_bias == 1.0 && st->sink_applied == 0, "disabled, yet the bias moved");
   CHECK(st->sink_rate_hz == 0.0, "disabled, yet a rate is shown");

   reset(st, false);
   dev_ppm = 120.0;
   run_seconds(st, 120);
   CHECK(st->sink_bias != 1.0, "no bias to drop");
   config_get_ptr()->bools.audio_sink_rate_estimation = false;
   run_second(st);
   CHECK(st->sink_bias == 1.0 && audio_driver_sink_bias(st) == 1.0, "turned off, the bias stayed");
   config_get_ptr()->bools.audio_sink_rate_estimation = true;
   run_seconds(st, 120);
   printf("      off then on: bias %+.0f ppm\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 120.0) < 60.0, "turned back on, the bias is %+.0f, expected +120 afresh", bias_ppm(st));
}

/* A driver reinit starts the estimate over - the counts are the new
 * driver's - and the bias stands until re-derived. */
static void s_driver_restart(audio_driver_state_t *st)
{
   double before;
   reset(st, false);
   dev_ppm = 90.0;
   run_seconds(st, 120);
   before = st->sink_bias;
   st->sink_started = 0;          /* what init does, with the counts */
   st->sink_offered = 0.0; st->sink_offered_raw = 0; st->sink_accepted = 0;
   clock_usec += 500000;
   run_second(st);
   CHECK(st->sink_bias == before, "a restart dropped the bias before it was re-derived");
   run_seconds(st, 200);
   printf("      bias %+.0f ppm after the restart\n", bias_ppm(st));
   CHECK(fabs(bias_ppm(st) - 90.0) < 40.0, "after a restart the bias is %+.0f, expected +90", bias_ppm(st));
}

/* --- the buffer it corrects ------------------------------------------- */

/* From a CoreAudio field report: a 10.7 ms buffer, the device at +47,
 * the source at -442, audio breaking up about a minute in, when the
 * first bias lands. The buffer is modelled: it is empty long before
 * any bias exists, so whatever broke it was not the correction - and
 * the correction, when it comes, is what would hold it. */
static void s_small_buffer_report(audio_driver_state_t *st)
{
   const double cap = 0.0107 * 48000.0;
   int sink_on, i;
   for (sink_on = 0; sink_on < 2; sink_on++)
   {
      double level = cap * 0.5;
      int    unders = 0, first = -1;
      reset(st, false);
      dev_ppm = 47.0;
      src_ppm = -442.0;
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
      printf("      estimation %s: first underrun at %d s, %d underrunning second(s), bias %+.0f ppm\n",
            sink_on ? "on " : "off", first, unders, bias_ppm(st));
      CHECK(first > 0 && first < 30, "expected the buffer to drain before any bias could be applied, first underrun at %d s", first);
      if (sink_on)
         CHECK(bias_ppm(st) > 300.0, "the correction should have found the mismatch, got %+.0f ppm", bias_ppm(st));
   }
}

/* A correction every thirty seconds holds only a buffer that lasts
 * thirty seconds of the mismatch; a smaller one is said so, once. */
static void s_cadence_warning(audio_driver_state_t *st)
{
   struct { size_t bytes; double ppm; const char *what; int want; } cases[] = {
      { (size_t)(0.0107 * 48000) * 4, 490.0, "10.7 ms, 490 ppm apart", 1 },
      { (size_t)(0.0107 * 48000) * 4,  11.0, "10.7 ms, 11 ppm apart",  0 },
      { (size_t)(0.200  * 48000) * 4, 490.0, "200 ms, 490 ppm apart",  0 }
   };
   size_t c;
   for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
   {
      int fired;
      reset(st, false);
      st->buffer_size = cases[c].bytes;
      dev_ppm = cases[c].ppm;
      run_seconds(st, 120);
      fired = (st->sink_warned & AUDIO_SINK_WARNED_TOO_SLOW) != 0;
      printf("      %-24s -> %s\n", cases[c].what, fired ? "too small for the cadence" : "the cadence can hold it");
      CHECK(fired == cases[c].want, "%s: expected the warning to %sfire", cases[c].what, cases[c].want ? "" : "not ");
   }
}

/* --- the runner ------------------------------------------------------- */

typedef struct
{
   const char *name;
   void (*run)(audio_driver_state_t *st);
} scenario_t;

static const scenario_t scenarios[] = {
   { "a device 120 ppm fast, counting in periods",        s_fast_device },
   { "a device 80 ppm slow",                               s_slow_device },
   { "two hours at +37 ppm",                               s_long_session },
   { "written per frame and per second",                   s_write_cadence },
   { "a writer refusing half a percent",                   s_refused_frames },
   { "ratios too far off to be a crystal",                 s_implausible_device },
   { "a source paced at the display's rate",               s_source_at_display_rate },
   { "rate control pinned at its bound",                   s_rate_control_pinned },
   { "the core pausing half a second every ten",           s_core_pauses },
   { "the main thread stalling 50 ms every two minutes",   s_main_thread_stalls },
   { "the device frozen for a window",                     s_device_stall },
   { "a slow start, then nominal",                         s_slow_start },
   { "publishes counted at the producer",                  s_publishes },
   { "a blocking writer",                                  s_blocking_writer },
   { "disabled, and toggled off and on",                   s_disabled_and_toggled },
   { "a driver restart",                                   s_driver_restart },
   { "the CoreAudio small-buffer report",                  s_small_buffer_report },
   { "the correction's cadence against the buffer",        s_cadence_warning },
};

int main(void)
{
   size_t i;
   printf("sink rate estimation, %u scenarios:\n", (unsigned)(sizeof(scenarios) / sizeof(scenarios[0])));
   for (i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++)
   {
      unsigned before = failures;
      printf("   %s\n", scenarios[i].name);
      scenarios[i].run(&audio_driver_st);
      if (failures != before)
         printf("      -> FAILED\n");
   }
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("sink rate: every scenario measures the clocks and nothing else\n");
   return 0;
}
