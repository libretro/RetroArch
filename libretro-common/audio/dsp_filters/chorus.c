/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chorus.c).
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
#include <string.h>

#include <retro_miscellaneous.h>
#include <libretro_dspfilter.h>

#define CHORUS_MAX_DELAY 4096
#define CHORUS_DELAY_MASK (CHORUS_MAX_DELAY - 1)

struct chorus_data
{
   float old[2][CHORUS_MAX_DELAY];
   /* int16 mirror of the delay line, plus a precomputed Q16 per-phase delay
    * LUT and Q16 mix gains, for the deterministic int16 path. */
   int16_t old_i[2][CHORUS_MAX_DELAY];
   int32_t *delay_lut;
   int32_t mix_dry_i;
   int32_t mix_wet_i;
   float delay;
   float depth;
   float input_rate;
   float mix_dry;
   float mix_wet;
   unsigned old_ptr;
   unsigned lfo_ptr;
   unsigned lfo_period;
};

static void chorus_free(void *data)
{
   if (data)
   {
      struct chorus_data *ch = (struct chorus_data*)data;
      free(ch->delay_lut);
      free(data);
   }
}

static void chorus_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   float *out             = NULL;
   struct chorus_data *ch = (struct chorus_data*)data;

   output->samples        = input->samples;
   output->frames         = input->frames;
   out                    = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      unsigned delay_int;
      float delay_frac, l_a, l_b, r_a, r_b;
      float chorus_l, chorus_r;
      float in[2]             = { out[0], out[1] };
      float delay             = ch->delay + ch->depth * sin((2.0 * M_PI * ch->lfo_ptr++) / ch->lfo_period);

      delay                  *= ch->input_rate;
      if (ch->lfo_ptr >= ch->lfo_period)
         ch->lfo_ptr          = 0;

      delay_int               = (unsigned)delay;

      if (delay_int >= CHORUS_MAX_DELAY - 1)
         delay_int            = CHORUS_MAX_DELAY - 2;

      delay_frac              = delay - delay_int;

      ch->old[0][ch->old_ptr] = in[0];
      ch->old[1][ch->old_ptr] = in[1];

      l_a                     = ch->old[0][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      l_b                     = ch->old[0][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];
      r_a                     = ch->old[1][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      r_b                     = ch->old[1][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];

      /* Lerp introduces aliasing of the chorus component,
       * but doing full polyphase here is probably overkill. */
      chorus_l                = l_a * (1.0f - delay_frac) + l_b * delay_frac;
      chorus_r                = r_a * (1.0f - delay_frac) + r_b * delay_frac;

      out[0]                  = ch->mix_dry * in[0] + ch->mix_wet * chorus_l;
      out[1]                  = ch->mix_dry * in[1] + ch->mix_wet * chorus_r;

      ch->old_ptr             = (ch->old_ptr + 1) & CHORUS_DELAY_MASK;
   }
}

/* Deterministic int16 path: same modulated fractional-delay chorus as
 * chorus_process(), but the LFO delay is read from a precomputed Q16
 * per-phase LUT, the delay line is int16, and the linear interpolation and
 * dry/wet mix are done in Q16 with int64 accumulation and s16 saturation. */
static void chorus_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i;
   int16_t *out           = NULL;
   struct chorus_data *ch = (struct chorus_data*)data;

   output->samples        = input->samples;
   output->frames         = input->frames;
   out                    = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      unsigned delay_int;
      int32_t frac_q16;
      int16_t l_a, l_b, r_a, r_b;
      int64_t chorus_l, chorus_r, acc;
      int32_t v;
      int16_t in0             = out[0];
      int16_t in1             = out[1];
      int32_t delay_q16       = ch->delay_lut[ch->lfo_ptr++];

      if (ch->lfo_ptr >= ch->lfo_period)
         ch->lfo_ptr          = 0;

      delay_int               = (unsigned)(delay_q16 >> 16);
      if (delay_int >= CHORUS_MAX_DELAY - 1)
         delay_int            = CHORUS_MAX_DELAY - 2;
      frac_q16                = delay_q16 - (int32_t)(delay_int << 16);

      ch->old_i[0][ch->old_ptr] = in0;
      ch->old_i[1][ch->old_ptr] = in1;

      l_a = ch->old_i[0][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      l_b = ch->old_i[0][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];
      r_a = ch->old_i[1][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      r_b = ch->old_i[1][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];

      /* chorus = lerp(a, b, frac), kept in Q16 (value << 16). */
      chorus_l = (int64_t)l_a * (65536 - frac_q16) + (int64_t)l_b * frac_q16;
      chorus_r = (int64_t)r_a * (65536 - frac_q16) + (int64_t)r_b * frac_q16;

      /* out = mix_dry*in + mix_wet*chorus, all in Q16, then round + saturate.
       * The wet term folds the Q16 chorus and the Q16 gain: (gain*chorus)>>16. */
      acc = (int64_t)ch->mix_dry_i * in0
          + ((int64_t)ch->mix_wet_i * chorus_l >> 16);
      v   = (acc >= 0) ?  (int32_t)(( acc + 32768) >> 16)
                       : -(int32_t)((-acc + 32768) >> 16);
      if      (v >  32767) v =  32767;
      else if (v < -32768) v = -32768;
      out[0] = (int16_t)v;

      acc = (int64_t)ch->mix_dry_i * in1
          + ((int64_t)ch->mix_wet_i * chorus_r >> 16);
      v   = (acc >= 0) ?  (int32_t)(( acc + 32768) >> 16)
                       : -(int32_t)((-acc + 32768) >> 16);
      if      (v >  32767) v =  32767;
      else if (v < -32768) v = -32768;
      out[1] = (int16_t)v;

      ch->old_ptr             = (ch->old_ptr + 1) & CHORUS_DELAY_MASK;
   }
}

static void *chorus_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float delay, depth, lfo_freq, drywet;
   struct chorus_data *ch = (struct chorus_data*)calloc(1, sizeof(*ch));
   if (!ch)
      return NULL;

   config->get_float(userdata, "delay_ms", &delay, 25.0f);
   config->get_float(userdata, "depth_ms", &depth, 1.0f);
   config->get_float(userdata, "lfo_freq", &lfo_freq, 0.5f);
   config->get_float(userdata, "drywet", &drywet, 0.8f);

   delay            /= 1000.0f;
   depth            /= 1000.0f;

   if (depth > delay)
      depth          = delay;

   if (drywet < 0.0f)
      drywet         = 0.0f;
   else if (drywet > 1.0f)
      drywet         = 1.0f;

   ch->mix_dry       = 1.0f - 0.5f * drywet;
   ch->mix_wet       = 0.5f * drywet;

   ch->delay         = delay;
   ch->depth         = depth;
   ch->lfo_period    = (1.0f / lfo_freq) * info->input_rate;
   ch->input_rate    = info->input_rate;
   if (!ch->lfo_period)
      ch->lfo_period = 1;

   /* Precompute the Q16 per-phase delay LUT (in samples) and Q16 mix gains
    * for the int16 path: one full LFO period of the same double-precision
    * sine the float path uses, so no FPU is needed at run time. */
   ch->mix_dry_i     = (int32_t)floor((double)ch->mix_dry * 65536.0 + 0.5);
   ch->mix_wet_i     = (int32_t)floor((double)ch->mix_wet * 65536.0 + 0.5);
   ch->delay_lut     = (int32_t*)malloc(ch->lfo_period * sizeof(int32_t));
   if (!ch->delay_lut)
   {
      chorus_free(ch);
      return NULL;
   }
   {
      unsigned p;
      for (p = 0; p < ch->lfo_period; p++)
      {
         double   d  = (ch->delay + ch->depth *
               sin((2.0 * M_PI * p) / ch->lfo_period)) * ch->input_rate;
         double   di = floor(d);            /* matches float (unsigned)delay */
         int32_t  ip = (int32_t)di;
         int32_t  fp = (int32_t)floor((d - di) * 65536.0 + 0.5);
         if (fp > 65535)                    /* rounding carried into next int */
         {
            fp -= 65536;
            ip += 1;
         }
         ch->delay_lut[p] = (ip << 16) | (fp & 0xFFFF);
      }
   }
   return ch;
}

static const struct dspfilter_implementation chorus_plug = {
   chorus_init,
   chorus_process,
   chorus_free,

   DSPFILTER_API_VERSION,
   "Chorus",
   "chorus",

   chorus_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation chorus_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *
dspfilter_get_implementation(dspfilter_simd_mask_t mask) { return &chorus_plug; }

#undef dspfilter_get_implementation
