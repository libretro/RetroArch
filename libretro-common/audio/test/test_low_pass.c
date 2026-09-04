/* Unit tests for the fast-forward low-pass filter.
 *
 * Build:  cc -O2 -std=c89 -Wall -Wextra test_low_pass.c \
 *            ../audio_low_pass.c -I ../../include -lm -o test_low_pass */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audio/audio_low_pass.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;

static void ok(const char *what, int cond)
{
   if (!cond)
   {
      printf("FAIL %s\n", what);
      failures++;
   }
   else
      printf("ok   %s\n", what);
}

/* Interleaved stereo sine at a fixed frequency, both channels identical. */
static void fill_sine(int16_t *buf, int frames, double hz, double rate,
      double *phase, double amplitude)
{
   int i;
   for (i = 0; i < frames; i++)
   {
      double v     = sin(*phase) * amplitude;
      buf[(i*2)+0] = (int16_t)v;
      buf[(i*2)+1] = (int16_t)v;
      *phase      += (2.0 * M_PI * hz) / rate;
   }
}

/* Goertzel magnitude of x[0..n-1] at digital frequency f (cycles/sample):
 * a single-bin DFT, cheaper than a full transform when only one known
 * frequency matters. Same technique test_time_stretch.c uses on a power
 * spectrum; here it runs directly on the signal. */
static double goertzel_mag(const double *x, int n, double f)
{
   double w     = 2.0 * M_PI * f;
   double coeff = 2.0 * cos(w);
   double q0;
   double q1    = 0.0;
   double q2    = 0.0;
   double real;
   double imag;
   int    i;
   for (i = 0; i < n; i++)
   {
      q0 = (coeff * q1) - q2 + x[i];
      q2 = q1;
      q1 = q0;
   }
   real = q1 - (q2 * cos(w));
   imag = q2 * sin(w);
   return sqrt((real * real) + (imag * imag));
}

/* Goertzel-estimated amplitude of a pure tone at tone_hz/amplitude, run
 * straight through (no filter), over the same window length the filtered
 * measurements use - this calibrates out the Goertzel scale factor and any
 * leakage from a non-integer number of cycles in the window, so the ratio
 * against the filtered measurement is a fair one. */
static double measure_raw_amplitude(double tone_hz, double amplitude,
      double rate, int frames)
{
   int16_t *buf = (int16_t*)malloc((size_t)frames * 2 * sizeof(int16_t));
   double  *sig = (double*)malloc((size_t)frames * sizeof(double));
   double   phase = 0.0;
   double   mag;
   double   amp_est;
   int      i;

   fill_sine(buf, frames, tone_hz, rate, &phase, amplitude);
   for (i = 0; i < frames; i++)
      sig[i] = (double)buf[(i * 2) + 0];

   mag     = goertzel_mag(sig, frames, tone_hz / rate);
   amp_est = (2.0 * mag) / (double)frames;

   free(sig);
   free(buf);
   return amp_est;
}

/* Runs the filter until its smoothed cutoff has converged on cutoff_hz
 * (many small blocks of silence, so only the smoother's own dynamics are
 * exercised - not the tone) and its biquad memory has decayed to ~0 on
 * that silence, then feeds a tone at tone_hz/amplitude, discards an
 * initial warmup stretch of it (so the biquad's impulse response to
 * *this* tone has settled), and returns the Goertzel-estimated output
 * amplitude at tone_hz over the remainder. */
static double measure_filtered_amplitude(double cutoff_hz, double tone_hz,
      double amplitude, double rate, int warmup_frames, int measure_frames)
{
   audio_low_pass_t lp;
   const int settle_block  = 256;
   const int settle_blocks = 400; /* 400*256/48000 ~= 2.1s, >> 20*tau */
   int16_t  *silence;
   int16_t  *buf;
   double   *sig;
   double    phase = 0.0;
   double    mag;
   double    amp_est;
   int       total = warmup_frames + measure_frames;
   int       i, b;

   audio_low_pass_init(&lp, rate);

   silence = (int16_t*)calloc((size_t)settle_block * 2, sizeof(int16_t));
   for (b = 0; b < settle_blocks; b++)
      audio_low_pass_process(&lp, silence, settle_block, cutoff_hz,
            settle_block / rate);
   free(silence);

   buf = (int16_t*)malloc((size_t)total * 2 * sizeof(int16_t));
   fill_sine(buf, total, tone_hz, rate, &phase, amplitude);
   /* One call: the cutoff has already converged above, so this neither
    * moves it further nor changes the coefficients mid-buffer. */
   audio_low_pass_process(&lp, buf, total, cutoff_hz, (double)total / rate);

   sig = (double*)malloc((size_t)measure_frames * sizeof(double));
   for (i = 0; i < measure_frames; i++)
      sig[i] = (double)buf[((warmup_frames + i) * 2) + 0];

   mag     = goertzel_mag(sig, measure_frames, tone_hz / rate);
   amp_est = (2.0 * mag) / (double)measure_frames;

   free(sig);
   free(buf);
   return amp_est;
}

static double to_db(double out_amp, double ref_amp)
{
   if (ref_amp < 1.0e-9)
      return -300.0;
   return 20.0 * log10(out_amp / ref_amp);
}

static void test_lifecycle(void)
{
   audio_low_pass_t lp;
   audio_low_pass_init(&lp, 48000.0);
   ok("wide-open is 0.45 * sample_rate",
         fabs(audio_low_pass_wide_open(&lp) - (0.45 * 48000.0)) < 1.0e-6);
   ok("starts at wide-open cutoff",
         fabs(audio_low_pass_current(&lp) - audio_low_pass_wide_open(&lp)) < 1.0e-6);
   ok("starts bypassed", audio_low_pass_bypassed(&lp));
}

/* At wide-open, audio_low_pass_process() must leave the buffer untouched -
 * not merely close, bit-exact. */
static void test_wide_open_transparent(void)
{
   audio_low_pass_t lp;
   int16_t buf[1024 * 2];
   int16_t orig[1024 * 2];
   double  phase = 0.0;
   double  wide_open;
   int     i;
   int     mismatches = 0;

   audio_low_pass_init(&lp, 48000.0);
   wide_open = audio_low_pass_wide_open(&lp);
   fill_sine(buf, 1024, 1000.0, 48000.0, &phase, 12000.0);
   memcpy(orig, buf, sizeof(buf));

   audio_low_pass_process(&lp, buf, 1024, wide_open, 1024.0 / 48000.0);

   for (i = 0; i < 1024 * 2; i++)
      if (buf[i] != orig[i])
         mismatches++;

   ok("wide open passes through bit-exact", mismatches == 0);
}

/* Below cutoff: near-unity. Well above cutoff: attenuated hard. Both
 * measured in dB against an unfiltered reference tone. */
static void test_attenuation(void)
{
   const double rate    = 48000.0;
   const double cutoff  = 2000.0;
   const double amp     = 12000.0;
   const int    warmup  = 4096;
   const int    measure = 8192;
   double below_hz  = cutoff / 8.0;   /* 250 Hz  */
   double above_hz  = cutoff * 6.0;   /* 12 kHz  */
   double below_out, below_ref, below_db;
   double above_out, above_ref, above_db;

   below_out = measure_filtered_amplitude(cutoff, below_hz, amp, rate,
         warmup, measure);
   below_ref = measure_raw_amplitude(below_hz, amp, rate, measure);
   below_db  = to_db(below_out, below_ref);

   above_out = measure_filtered_amplitude(cutoff, above_hz, amp, rate,
         warmup, measure);
   above_ref = measure_raw_amplitude(above_hz, amp, rate, measure);
   above_db  = to_db(above_out, above_ref);

   printf("     attenuation @ %.0f Hz (cutoff/8):  %+.2f dB\n", below_hz, below_db);
   printf("     attenuation @ %.0f Hz (cutoff*6): %+.2f dB\n", above_hz, above_db);

   ok("well below cutoff passes near unity", fabs(below_db) < 1.0);
   ok("well above cutoff is attenuated hard", above_db < -30.0);
}

/* Fourth-order Butterworth rolls off at 24 dB/octave, so one octave past
 * the corner should already be near 24 dB down; a single biquad is not. */
static void test_fourth_order_rolloff(void)
{
   const double rate    = 48000.0;
   const double cutoff  = 2000.0;
   const double amp     = 12000.0;
   const int    warmup  = 4096;
   const int    measure = 8192;
   double tone_hz = cutoff * 2.0;
   double out_amp, ref_amp, db;

   out_amp = measure_filtered_amplitude(cutoff, tone_hz, amp, rate,
         warmup, measure);
   ref_amp = measure_raw_amplitude(tone_hz, amp, rate, measure);
   db      = to_db(out_amp, ref_amp);

   printf("     attenuation @ 2x cutoff (%.0f Hz): %+.2f dB (expect ~-24 dB)\n",
         tone_hz, db);

   ok("roll-off is fourth-order (~24 dB at 2x cutoff)",
         db < -18.0 && db > -30.0);
}

/* Engaging the filter mid-stream (target stepping from wide-open down to a
 * real cutoff, called in small blocks the way a driver flush would) must
 * not produce a click: the sample-to-sample delta right after the target
 * changes should stay in the same ballpark as before it changed, not spike
 * from a coefficient jump. */
static void test_engage_no_click(void)
{
   audio_low_pass_t lp;
   const double rate         = 48000.0;
   const int    block        = 64;
   const int    pre_blocks   = 200;
   const int    post_blocks  = 400;
   const double block_seconds = (double)block / rate;
   const double tone_hz      = 4000.0;
   const double post_cutoff  = 500.0;
   int16_t buf[64 * 2];
   double  phase             = 0.0;
   double  wide_open;
   double  max_delta_pre     = 0.0;
   double  max_delta_transition = 0.0;
   int16_t prev              = 0;
   int     have_prev         = 0;
   int     b, i;

   audio_low_pass_init(&lp, rate);
   wide_open = audio_low_pass_wide_open(&lp);

   for (b = 0; b < pre_blocks + post_blocks; b++)
   {
      double target = (b < pre_blocks) ? wide_open : post_cutoff;

      fill_sine(buf, block, tone_hz, rate, &phase, 12000.0);
      audio_low_pass_process(&lp, buf, block, target, block_seconds);

      for (i = 0; i < block; i++)
      {
         int16_t s = buf[(i * 2) + 0];
         if (have_prev)
         {
            double d = fabs((double)s - (double)prev);
            if (b < pre_blocks)
            {
               if (d > max_delta_pre)
                  max_delta_pre = d;
            }
            /* First 8 blocks after the target steps down: the transition
             * window where a coefficient-jump click would show up. */
            else if (b < pre_blocks + 8)
            {
               if (d > max_delta_transition)
                  max_delta_transition = d;
            }
         }
         prev      = s;
         have_prev = 1;
      }
   }

   printf("     max |delta|: pre-engage=%.1f  transition=%.1f\n",
         max_delta_pre, max_delta_transition);

   ok("no discontinuity engaging mid-stream",
         max_delta_transition <= (max_delta_pre * 1.5) + 50.0);
}

/* audio_low_pass_target_hz(): the fast-forward speed-to-cutoff mapping
 * used by the driver. Transparent at or below 1x; divides down above it;
 * clamped to the configured floor and to wide-open. */
static void test_target_hz_mapping(void)
{
   double wide_open = 21600.0; /* 0.45 * 48000 */

   ok("transparent at 1x",
         audio_low_pass_target_hz(12000.0, 1.0, wide_open) == wide_open);
   ok("transparent below 1x (slow-motion)",
         audio_low_pass_target_hz(12000.0, 0.5, wide_open) == wide_open);
   ok("divides by speed",
         fabs(audio_low_pass_target_hz(12000.0, 3.0, wide_open) - 4000.0) < 1.0e-9);
   ok("floors at the speed floor",
         fabs(audio_low_pass_target_hz(12000.0, 1000.0, wide_open)
               - AUDIO_LOW_PASS_SPEED_FLOOR) < 1.0e-9);
   ok("never exceeds wide-open",
         audio_low_pass_target_hz(1.0e9, 1.001, wide_open) <= wide_open);
}

int main(void)
{
   test_lifecycle();
   test_wide_open_transparent();
   test_attenuation();
   test_fourth_order_rolloff();
   test_engage_no_click();
   test_target_hz_mapping();

   printf("\n%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
   return failures ? 1 : 0;
}
