/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_low_pass.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Provenance: see <audio/audio_low_pass.h>. */

#include <math.h>
#include <string.h>

#include <retro_miscellaneous.h>

#include <audio/audio_low_pass.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Section Q's for a fourth-order Butterworth cascade. */
static const double audio_low_pass_section_q[2] =
      { 0.54119610014619698, 1.3065629648763766 };

/* floor(x + 0.5) rather than round()/lround(): C89 has neither. */
static int16_t audio_low_pass_saturate(double y)
{
   long v = (long)floor(y + 0.5);
   if (v > 32767)
      v = 32767;
   if (v < -32768)
      v = -32768;
   return (int16_t)v;
}

static void audio_low_pass_design(struct audio_biquad *bq,
      double cutoff_hz, double sample_rate, double q)
{
   double w0    = 2.0 * M_PI * (cutoff_hz / sample_rate);
   double cw    = cos(w0);
   double alpha = sin(w0) / (2.0 * q);
   double a0    = 1.0 + alpha;

   bq->b0 = ((1.0 - cw) * 0.5) / a0;
   bq->b1 = (1.0 - cw) / a0;
   bq->b2 = bq->b0;
   bq->a1 = (-2.0 * cw) / a0;
   bq->a2 = (1.0 - alpha) / a0;
}

/* transposed direct form II */
static double audio_low_pass_run(struct audio_biquad *bq, double x, int ch)
{
   double y   = (bq->b0 * x) + bq->z1[ch];
   bq->z1[ch] = (bq->b1 * x) - (bq->a1 * y) + bq->z2[ch];
   bq->z2[ch] = (bq->b2 * x) - (bq->a2 * y);
   return y;
}

static double audio_low_pass_process_sample(audio_low_pass_t *lp,
      double x, int ch)
{
   double y = x;
   int    s;
   for (s = 0; s < 2; s++)
      y = audio_low_pass_run(&lp->stages[s], y, ch);
   return y;
}

void audio_low_pass_set_cutoff_now(audio_low_pass_t *lp, double cutoff_hz)
{
   int s;
   lp->cur_cutoff = MAX(AUDIO_LOW_PASS_MIN_CUTOFF, MIN(lp->wide_open, cutoff_hz));
   for (s = 0; s < 2; s++)
      audio_low_pass_design(&lp->stages[s], lp->cur_cutoff, lp->sample_rate,
            audio_low_pass_section_q[s]);
}

void audio_low_pass_init(audio_low_pass_t *lp, double sample_rate)
{
   memset(lp, 0, sizeof(*lp));
   lp->sample_rate = sample_rate;
   lp->wide_open   = 0.45 * sample_rate;
   if (lp->wide_open < AUDIO_LOW_PASS_MIN_CUTOFF)
      lp->wide_open = AUDIO_LOW_PASS_MIN_CUTOFF;
   audio_low_pass_set_cutoff_now(lp, lp->wide_open);
}

void audio_low_pass_reset(audio_low_pass_t *lp)
{
   int s;
   for (s = 0; s < 2; s++)
   {
      lp->stages[s].z1[0] = 0.0;
      lp->stages[s].z1[1] = 0.0;
      lp->stages[s].z2[0] = 0.0;
      lp->stages[s].z2[1] = 0.0;
   }
   audio_low_pass_set_cutoff_now(lp, lp->wide_open);
}

double audio_low_pass_wide_open(const audio_low_pass_t *lp)
{
   return lp->wide_open;
}

double audio_low_pass_current(const audio_low_pass_t *lp)
{
   return lp->cur_cutoff;
}

bool audio_low_pass_bypassed(const audio_low_pass_t *lp)
{
   return lp->cur_cutoff >= (lp->wide_open * AUDIO_LOW_PASS_BYPASS);
}

void audio_low_pass_process(audio_low_pass_t *lp, int16_t *frames,
      int num_frames, double target_hz, double block_seconds)
{
   bool   bypass;
   int    i;
   int    ch;
   int    s;
   double a;
   double from[2][5];
   double to[2][5];

   if (num_frames < 1)
      return;

   /* Coefficients as they stand, so the move to the new cutoff can be spread
    * across the block rather than applied as a step. A transposed
    * direct-form biquad's state encodes its past under the coefficients that
    * produced it, so replacing them in one go leaves the state inconsistent
    * with the filter and it rings - once per block, and per call on a core
    * that delivers audio in many small batches. */
   for (s = 0; s < 2; s++)
   {
      from[s][0] = lp->stages[s].b0;
      from[s][1] = lp->stages[s].b1;
      from[s][2] = lp->stages[s].b2;
      from[s][3] = lp->stages[s].a1;
      from[s][4] = lp->stages[s].a2;
   }

   target_hz = MAX(AUDIO_LOW_PASS_MIN_CUTOFF, MIN(lp->wide_open, target_hz));
   a         = 1.0 - exp(-block_seconds / AUDIO_LOW_PASS_TAU);
   audio_low_pass_set_cutoff_now(lp,
         lp->cur_cutoff + ((target_hz - lp->cur_cutoff) * a));

   for (s = 0; s < 2; s++)
   {
      to[s][0] = lp->stages[s].b0;
      to[s][1] = lp->stages[s].b1;
      to[s][2] = lp->stages[s].b2;
      to[s][3] = lp->stages[s].a1;
      to[s][4] = lp->stages[s].a2;
   }

   bypass = audio_low_pass_bypassed(lp);

   for (i = 0; i < num_frames; i++)
   {
      double t = (double)(i + 1) / (double)num_frames;

      for (s = 0; s < 2; s++)
      {
         lp->stages[s].b0 = from[s][0] + ((to[s][0] - from[s][0]) * t);
         lp->stages[s].b1 = from[s][1] + ((to[s][1] - from[s][1]) * t);
         lp->stages[s].b2 = from[s][2] + ((to[s][2] - from[s][2]) * t);
         lp->stages[s].a1 = from[s][3] + ((to[s][3] - from[s][3]) * t);
         lp->stages[s].a2 = from[s][4] + ((to[s][4] - from[s][4]) * t);
      }

      for (ch = 0; ch < 2; ch++)
      {
         /* Runs even when bypassed: the state must stay in step with the
          * signal, or re-engaging would click. */
         double y = audio_low_pass_process_sample(lp, frames[(i * 2) + ch], ch);
         if (!bypass)
            frames[(i * 2) + ch] = audio_low_pass_saturate(y);
      }
   }

   /* Land exactly on the designed coefficients, so rounding in the
    * interpolation cannot accumulate across blocks. */
   for (s = 0; s < 2; s++)
   {
      lp->stages[s].b0 = to[s][0];
      lp->stages[s].b1 = to[s][1];
      lp->stages[s].b2 = to[s][2];
      lp->stages[s].a1 = to[s][3];
      lp->stages[s].a2 = to[s][4];
   }
}

double audio_low_pass_target_hz(double reference_hz, double speed,
      double wide_open)
{
   double target;
   if (speed <= 1.0)
      return wide_open;
   target = reference_hz / speed;
   return MAX(AUDIO_LOW_PASS_SPEED_FLOOR, MIN(wide_open, target));
}
