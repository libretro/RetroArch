/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wahwah.c).
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

#define WAHWAH_LFO_SKIP_SAMPLES 30

/* int16 path: the normalised biquad coefficients are tabulated over one
 * LFO cycle (Q29) and read with linear interpolation from a 64-bit phase
 * accumulator. The filter output state is Q10 (1 LSB of s16 = 1024) with
 * first-order error feedback, which keeps rounding noise from being
 * amplified by the low, resonant poles. */
#define WAHWAH_LUT_BITS  12
#define WAHWAH_LUT_SIZE  (1 << WAHWAH_LUT_BITS)
#define WAHWAH_I16_LIMIT ((int32_t)1 << 30)

struct wahwah_coef_i
{
   int32_t b0; /* b1 = 2 * b0, b2 = b0 */
   int32_t a1;
   int32_t a2;
};

struct wahwah_data
{
   /* int16 path: biquad state with carried rounding residue, LFO phase
    * and step in 2^-64 turns, coefficient table and current set. */
   struct
   {
      int64_t err;
      int32_t xn1, xn2, yn1, yn2;
   } li, ri;
   uint64_t lfo_phase;
   uint64_t lfo_step;
   struct wahwah_coef_i *lut;

   unsigned long skipcount;

   struct wahwah_coef_i coef_i;
   unsigned skip_i;

   float phase;
   float lfoskip;
   float b0, b1, b2, a0, a1, a2;
   float freq, startphase;
   float depth, freqofs, res;

   struct
   {
      float xn1, xn2, yn1, yn2;
   } l, r;
};

static void wahwah_free(void *data)
{
   struct wahwah_data *wah = (struct wahwah_data*)data;
   if (!wah)
      return;
   free(wah->lut);
   free(wah);
}

/* One stereo frame of wah-wah on the float path. */
static INLINE void wahwah_frame(struct wahwah_data *wah,
      const float in[2], float *out)
{
   float out_l, out_r;

   if ((wah->skipcount++ % WAHWAH_LFO_SKIP_SAMPLES) == 0)
   {
      float omega, sn, cs, alpha;
      float frequency = (1.0f + cos(wah->skipcount * wah->lfoskip + wah->phase)) / 2.0f;

      frequency       = frequency * wah->depth * (1.0f - wah->freqofs) + wah->freqofs;
      frequency       = exp((frequency - 1.0f) * 6.0f);

      omega           = M_PI * frequency;
      sn              = sin(omega);
      cs              = cos(omega);
      alpha           = sn / (2.0f * wah->res);

      wah->b0         = (1.0f - cs) / 2.0f;
      wah->b1         = 1.0f  - cs;
      wah->b2         = (1.0f - cs) / 2.0f;
      wah->a0         = 1.0f + alpha;
      wah->a1         = -2.0f * cs;
      wah->a2         = 1.0f - alpha;
   }

   out_l              = (wah->b0 * in[0] + wah->b1 * wah->l.xn1 + wah->b2 * wah->l.xn2 - wah->a1 * wah->l.yn1 - wah->a2 * wah->l.yn2) / wah->a0;
   out_r              = (wah->b0 * in[1] + wah->b1 * wah->r.xn1 + wah->b2 * wah->r.xn2 - wah->a1 * wah->r.yn1 - wah->a2 * wah->r.yn2) / wah->a0;

   wah->l.xn2         = wah->l.xn1;
   wah->l.xn1         = in[0];
   wah->l.yn2         = wah->l.yn1;
   wah->l.yn1         = out_l;

   wah->r.xn2         = wah->r.xn1;
   wah->r.xn1         = in[1];
   wah->r.yn2         = wah->r.yn1;
   wah->r.yn1         = out_r;

   out[0]             = out_l;
   out[1]             = out_r;
}

static void wahwah_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct wahwah_data *wah = (struct wahwah_data*)data;
   float *out              = input->samples;

   output->samples         = input->samples;
   output->frames          = input->frames;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float in[2];
      in[0] = out[0];
      in[1] = out[1];
      wahwah_frame(wah, in, out);
   }
}

static INLINE int64_t wahwah_rsh(int64_t v, unsigned s)
{
   int64_t h = (int64_t)1 << (s - 1);
   return (v >= 0) ? ((v + h) >> s) : -(((-v) + h) >> s);
}

/* One channel of the Q10 biquad. The low 29 bits dropped from the
 * accumulator are carried into the next sample. */
static INLINE int32_t wahwah_biquad_i(const struct wahwah_coef_i *c,
      int32_t x, int32_t *xn1, int32_t *xn2, int32_t *yn1, int32_t *yn2,
      int64_t *err)
{
   int64_t acc, y;
   acc  = (int64_t)c->b0 * ((int64_t)x + 2 * (int64_t)*xn1 + *xn2) * 1024
        - (int64_t)c->a1 * *yn1 - (int64_t)c->a2 * *yn2 + *err;
   /* Floor division by 2^29, defined for negative values too. */
   y    = (acc >= 0) ? (acc >> 29) : -(((-acc) + (((int64_t)1 << 29) - 1)) >> 29);
   *err = acc - y * ((int64_t)1 << 29);
   if (y > WAHWAH_I16_LIMIT)
      y = WAHWAH_I16_LIMIT;
   else if (y < -WAHWAH_I16_LIMIT)
      y = -WAHWAH_I16_LIMIT;
   *xn2 = *xn1;
   *xn1 = x;
   *yn2 = *yn1;
   *yn1 = (int32_t)y;
   return (int32_t)y;
}

static void wahwah_process_i16(void *data, struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i;
   struct wahwah_data *wah = (struct wahwah_data*)data;
   int16_t *out            = input->samples;

   output->samples         = input->samples;
   output->frames          = input->frames;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      int64_t yl, yr;

      /* Same cadence as the float path: the coefficients are refreshed
       * on every WAHWAH_LFO_SKIP_SAMPLES-th frame, from the advanced
       * phase. */
      wah->lfo_phase += wah->lfo_step;
      if (wah->skip_i++ == 0)
      {
         unsigned idx = (unsigned)(wah->lfo_phase >> (64 - WAHWAH_LUT_BITS));
         int64_t frac = (int64_t)((wah->lfo_phase >> (48 - WAHWAH_LUT_BITS))
               & 0xFFFF);
         const struct wahwah_coef_i *c0 = &wah->lut[idx];
         const struct wahwah_coef_i *c1 = &wah->lut[idx + 1];
         wah->coef_i.b0 = c0->b0 + (int32_t)wahwah_rsh((int64_t)(c1->b0 - c0->b0) * frac, 16);
         wah->coef_i.a1 = c0->a1 + (int32_t)wahwah_rsh((int64_t)(c1->a1 - c0->a1) * frac, 16);
         wah->coef_i.a2 = c0->a2 + (int32_t)wahwah_rsh((int64_t)(c1->a2 - c0->a2) * frac, 16);
      }
      if (wah->skip_i == WAHWAH_LFO_SKIP_SAMPLES)
         wah->skip_i = 0;

      yl = wahwah_rsh(wahwah_biquad_i(&wah->coef_i, out[0], &wah->li.xn1,
               &wah->li.xn2, &wah->li.yn1, &wah->li.yn2, &wah->li.err), 10);
      yr = wahwah_rsh(wahwah_biquad_i(&wah->coef_i, out[1], &wah->ri.xn1,
               &wah->ri.xn2, &wah->ri.yn1, &wah->ri.yn2, &wah->ri.err), 10);
      out[0] = (int16_t)(yl > 32767 ? 32767 : (yl < -32768 ? -32768 : yl));
      out[1] = (int16_t)(yr > 32767 ? 32767 : (yr < -32768 ? -32768 : yr));
   }
}

/* A fraction of a turn in [0, 1) as 2^-64 turns, built 32 bits at a
 * time so no precision is lost to the double's exponent range. */
static uint64_t wahwah_turns_q64(double t)
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

static void *wahwah_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   struct wahwah_data *wah = (struct wahwah_data*)calloc(1, sizeof(*wah));
   if (!wah)
      return NULL;

   config->get_float(userdata, "lfo_freq", &wah->freq, 1.5f);
   config->get_float(userdata, "lfo_start_phase", &wah->startphase, 0.0f);
   config->get_float(userdata, "freq_offset", &wah->freqofs, 0.3f);
   config->get_float(userdata, "depth", &wah->depth, 0.7f);
   config->get_float(userdata, "resonance", &wah->res, 2.5f);

   wah->lfoskip = wah->freq * 2.0f * M_PI / info->input_rate;
   wah->phase   = wah->startphase * M_PI / 180.0f;

   if (!(wah->lut = (struct wahwah_coef_i*)malloc(
               (WAHWAH_LUT_SIZE + 1) * sizeof(*wah->lut))))
   {
      free(wah);
      return NULL;
   }
   {
      unsigned k;
      double turns, t0;
      /* The float path's LFO argument, skipcount * lfoskip + phase in
       * radians, held as 2^-64 turns. */
      turns          = wah->lfoskip / (2.0 * M_PI);
      turns         -= floor(turns);
      wah->lfo_step  = wahwah_turns_q64(turns);
      t0             = wah->phase / (2.0 * M_PI);
      t0            -= floor(t0);
      wah->lfo_phase = wahwah_turns_q64(t0);
      for (k = 0; k <= WAHWAH_LUT_SIZE; k++)
      {
         double omega, sn, cs, alpha, a0;
         double frequency = (1.0 + cos(2.0 * M_PI * k / WAHWAH_LUT_SIZE)) / 2.0;
         frequency        = frequency * wah->depth * (1.0 - wah->freqofs)
                          + wah->freqofs;
         frequency        = exp((frequency - 1.0) * 6.0);
         omega            = M_PI * frequency;
         sn               = sin(omega);
         cs               = cos(omega);
         alpha            = sn / (2.0 * wah->res);
         a0               = 1.0 + alpha;
         wah->lut[k].b0   = (int32_t)floor((1.0 - cs) / 2.0 / a0 * 536870912.0 + 0.5);
         wah->lut[k].a1   = (int32_t)floor(-2.0 * cs / a0 * 536870912.0 + 0.5);
         wah->lut[k].a2   = (int32_t)floor((1.0 - alpha) / a0 * 536870912.0 + 0.5);
      }
   }

   return wah;
}

static const struct dspfilter_implementation wahwah_plug = {
   wahwah_init,
   wahwah_process,
   wahwah_free,

   DSPFILTER_API_VERSION,
   "Wah-Wah",
   "wahwah",

   wahwah_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation wahwah_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *
dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &wahwah_plug;
}

#undef dspfilter_get_implementation
