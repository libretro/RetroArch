/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (phaser.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <libretro_dspfilter.h>

#define PHASER_LFO_SHAPE 4.0
#define PHASER_LFO_SKIP_SAMPLES 20

/* int16 path: the LFO gain curve is tabulated over one LFO cycle and
 * read with linear interpolation from a 64-bit phase accumulator; the
 * allpass chain runs in Q4 (1 LSB of s16 = 16) with Q30 gains. At the
 * LFO trough the stage gain reaches 1 and each stage briefly integrates,
 * so the state is given 2048 times full scale before it saturates. */
#define PHASER_LUT_BITS  12
#define PHASER_LUT_SIZE  (1 << PHASER_LUT_BITS)
#define PHASER_I16_LIMIT ((int32_t)1 << 30)

struct phaser_data
{
   /* int16 path: LFO phase and step in 2^-64 turns. */
   uint64_t lfo_phase;
   uint64_t lfo_step;

   unsigned long skipcount;

   /* int16 path: gain curve, allpass state and coefficients. */
   int32_t lut_q30[PHASER_LUT_SIZE + 1];
   int32_t old_i[2][24];
   int32_t fbout_i[2];
   int32_t gain_q30;
   int32_t fb_q30;
   int32_t drywet_q16;
   unsigned skip_i;

   float freq;
   float startphase;
   float fb;
   float depth;
   float drywet;
   float old[2][24];
   float gain;
   float fbout[2];
   float lfoskip;
   float phase;

   int stages;
};

static void phaser_free(void *data)
{
   free(data);
}

/* One stereo frame of phasing on the float path. */
static INLINE void phaser_frame(struct phaser_data *ph,
      const float in[2], float *out)
{
   unsigned c;
   int s;
   float m[2], tmp[2];

   for (c = 0; c < 2; c++)
      m[c] = in[c] + ph->fbout[c] * ph->fb * 0.01f;

   if ((ph->skipcount++ % PHASER_LFO_SKIP_SAMPLES) == 0)
   {
      ph->gain = 0.5 * (1.0 + cos(ph->skipcount * ph->lfoskip + ph->phase));
      ph->gain = (exp(ph->gain * PHASER_LFO_SHAPE) - 1.0) / (exp(PHASER_LFO_SHAPE) - 1);
      ph->gain = 1.0 - ph->gain * ph->depth;
   }

   for (s = 0; s < ph->stages; s++)
   {
      for (c = 0; c < 2; c++)
      {
         tmp[c]        = ph->old[c][s];
         ph->old[c][s] = ph->gain * tmp[c] + m[c];
         m[c]          = tmp[c] - ph->gain * ph->old[c][s];
      }
   }

   for (c = 0; c < 2; c++)
   {
      ph->fbout[c] = m[c];
      out[c]       = m[c] * ph->drywet + in[c] * (1.0f - ph->drywet);
   }
}

static void phaser_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct phaser_data *ph = (struct phaser_data*)data;
   float *out             = input->samples;

   output->samples        = input->samples;
   output->frames         = input->frames;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float in[2];
      in[0] = out[0];
      in[1] = out[1];
      phaser_frame(ph, in, out);
   }
}

static INLINE int64_t phaser_rsh(int64_t v, unsigned s)
{
   int64_t h = (int64_t)1 << (s - 1);
   return (v >= 0) ? ((v + h) >> s) : -(((-v) + h) >> s);
}

static INLINE int32_t phaser_sat(int64_t v)
{
   if (v > PHASER_I16_LIMIT)
      return PHASER_I16_LIMIT;
   if (v < -PHASER_I16_LIMIT)
      return -PHASER_I16_LIMIT;
   return (int32_t)v;
}

/* A fraction of a turn in [0, 1) as 2^-64 turns, built 32 bits at a
 * time so no precision is lost to the double's exponent range. */
static uint64_t phaser_turns_q64(double t)
{
   double hi = floor(t * 4294967296.0);
   double lo = floor((t * 4294967296.0 - hi) * 4294967296.0 + 0.5);
   if (lo >= 4294967296.0)
   {
      hi += 1.0;
      lo  = 0.0;
   }
   return ((uint64_t)(uint32_t)hi << 32) + (uint64_t)lo;
}

static void phaser_process_i16(void *data, struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i, c;
   int s;
   struct phaser_data *ph = (struct phaser_data*)data;
   int16_t *out           = input->samples;

   output->samples        = input->samples;
   output->frames         = input->frames;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      int32_t in[2], m[2];

      in[0] = (int32_t)out[0] * 16;
      in[1] = (int32_t)out[1] * 16;
      for (c = 0; c < 2; c++)
         m[c] = phaser_sat((int64_t)in[c]
               + phaser_rsh((int64_t)ph->fbout_i[c] * ph->fb_q30, 30));

      /* Same cadence as the float path: the gain is refreshed on every
       * PHASER_LFO_SKIP_SAMPLES-th frame, from the advanced phase. */
      ph->lfo_phase += ph->lfo_step;
      if (ph->skip_i++ == 0)
      {
         unsigned idx  = (unsigned)(ph->lfo_phase >> (64 - PHASER_LUT_BITS));
         int64_t  frac = (int64_t)((ph->lfo_phase >> (48 - PHASER_LUT_BITS))
               & 0xFFFF);
         ph->gain_q30  = ph->lut_q30[idx] + (int32_t)phaser_rsh(
               (int64_t)(ph->lut_q30[idx + 1] - ph->lut_q30[idx]) * frac, 16);
      }
      if (ph->skip_i == PHASER_LFO_SKIP_SAMPLES)
         ph->skip_i = 0;

      for (s = 0; s < ph->stages; s++)
      {
         for (c = 0; c < 2; c++)
         {
            int32_t tmp      = ph->old_i[c][s];
            ph->old_i[c][s]  = phaser_sat(phaser_rsh(
                     (int64_t)ph->gain_q30 * tmp, 30) + m[c]);
            m[c]             = phaser_sat((int64_t)tmp - phaser_rsh(
                     (int64_t)ph->gain_q30 * ph->old_i[c][s], 30));
         }
      }

      for (c = 0; c < 2; c++)
      {
         int64_t v;
         ph->fbout_i[c] = m[c];
         v = phaser_rsh((int64_t)m[c] * ph->drywet_q16
               + (int64_t)in[c] * (65536 - ph->drywet_q16), 20);
         out[c] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
      }
   }
}

static void *phaser_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float lfo_freq, lfo_start_phase;
   struct phaser_data *ph = (struct phaser_data*)calloc(1, sizeof(*ph));
   if (!ph)
      return NULL;

   config->get_float(userdata, "lfo_freq", &lfo_freq, 0.4f);
   config->get_float(userdata, "lfo_start_phase", &lfo_start_phase, 0.0f);
   config->get_float(userdata, "feedback", &ph->fb, 0.0f);
   config->get_float(userdata, "depth", &ph->depth, 0.4f);
   config->get_float(userdata, "dry_wet", &ph->drywet, 0.5f);
   config->get_int(userdata, "stages", &ph->stages, 2);

   if (ph->stages < 1)
      ph->stages = 1;
   else if (ph->stages > 24)
      ph->stages = 24;

   ph->lfoskip = lfo_freq * 2.0 * M_PI / info->input_rate;
   ph->phase   = lfo_start_phase * M_PI / 180.0;

   {
      unsigned k;
      double turns, t0;
      /* The float path's LFO argument is skipcount * lfoskip + phase in
       * radians; the accumulator holds the same angle in 2^-64 turns, fine
       * enough that it never drifts measurably from the float angle. */
      turns         = ph->lfoskip / (2.0 * M_PI);
      turns        -= floor(turns);
      ph->lfo_step  = phaser_turns_q64(turns);
      t0            = ph->phase / (2.0 * M_PI);
      t0           -= floor(t0);
      ph->lfo_phase = phaser_turns_q64(t0);
      for (k = 0; k <= PHASER_LUT_SIZE; k++)
      {
         double g = 0.5 * (1.0 + cos(2.0 * M_PI * k / PHASER_LUT_SIZE));
         g = (exp(g * PHASER_LFO_SHAPE) - 1.0) / (exp(PHASER_LFO_SHAPE) - 1);
         g = 1.0 - g * ph->depth;
         ph->lut_q30[k] = (int32_t)floor(g * 1073741824.0 + 0.5);
      }
      ph->fb_q30     = (int32_t)floor(ph->fb * 0.01 * 1073741824.0 + 0.5);
      ph->drywet_q16 = (int32_t)floor(ph->drywet * 65536.0 + 0.5);
   }

   return ph;
}

static const struct dspfilter_implementation phaser_plug = {
   phaser_init,
   phaser_process,
   phaser_free,

   DSPFILTER_API_VERSION,
   "Phaser",
   "phaser",

   phaser_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation phaser_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   return &phaser_plug;
}

#undef dspfilter_get_implementation
