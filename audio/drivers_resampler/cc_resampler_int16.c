/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2010-2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Deterministic integer (int16) Convoluted Cosine resampler.
 *
 * Fixed-point mirror of the CC_resampler C reference (cc_resampler.c).  The
 * cosine-convolution kernel and the phase accumulator are evaluated in Q24;
 * the kernel uses the same degree-5 polynomial approximation as the reference
 * at CC_RESAMPLER_PRECISION > 0.  int16 samples are multiplied by the Q24
 * kernel and accumulated in int64, then round-half-away-from-zero shifted back
 * to int16 with saturation.  No FPU is used per sample, and the output is
 * bit-identical across compilers and architectures. */

#include <stdint.h>
#include <stdlib.h>

#include <retro_inline.h>

#include <audio/cc_resampler_int16.h>

#ifndef CC_RESAMPLER_PRECISION
#define CC_RESAMPLER_PRECISION 1
#endif

#define CC_I16_FRAC_BITS 24
#define CC_I16_ONE       ((int64_t)1 << CC_I16_FRAC_BITS)
#define CC_I16_HALF      ((int64_t)1 << (CC_I16_FRAC_BITS - 1))

typedef struct cc_resampler_int16
{
   void (*process)(void *re, struct resampler_data_int16 *data);
   /* Downsample: Q24 weighted-sum accumulators.
    * Upsample:   raw int16 input samples (promoted). */
   int64_t buf_l[4];
   int64_t buf_r[4];
   int64_t distance; /* Q24 phase */
} cc_resampler_int16_t;

/* round-half-away-from-zero of (a * b) >> CC_I16_FRAC_BITS */
static INLINE int64_t cc_i16_mulq(int64_t a, int64_t b)
{
   int64_t p = a * b;
   if (p >= 0)
      return  ( p + CC_I16_HALF) >> CC_I16_FRAC_BITS;
   return    -((-p + CC_I16_HALF) >> CC_I16_FRAC_BITS);
}

/* Q24 counterpart of cc_int(): P(x*b) clamped to [-0.5, 0.5].
 * P(u) = u - 0.75*u^3 + 0.25*u^5 = u*(1 - 0.25*u^2*(3 - u^2)). */
static INLINE int64_t cc_i16_int(int64_t x, int64_t b)
{
   int64_t u = cc_i16_mulq(x, b);
#if (CC_RESAMPLER_PRECISION > 0)
   int64_t u2 = cc_i16_mulq(u, u);
   int64_t t  = cc_i16_mulq(u2, ((int64_t)3 << CC_I16_FRAC_BITS) - u2);
   t          = cc_i16_mulq(t, CC_I16_ONE >> 2);      /* * 0.25 */
   u          = cc_i16_mulq(u, CC_I16_ONE - t);       /* u * (1 - ...) */
#endif
   if      (u >  CC_I16_HALF) u =  CC_I16_HALF;
   else if (u < -CC_I16_HALF) u = -CC_I16_HALF;
   return u;
}

/* cc_kernel(x, b) = cc_int(x + 0.5, b) - cc_int(x - 0.5, b) */
static INLINE int64_t cc_i16_kernel(int64_t x, int64_t b)
{
   return cc_i16_int(x + CC_I16_HALF, b) - cc_i16_int(x - CC_I16_HALF, b);
}

static INLINE int16_t cc_i16_sat(int64_t acc)
{
   int64_t v = (acc >= 0) ?  (( acc + CC_I16_HALF) >> CC_I16_FRAC_BITS)
                          : -((-acc + CC_I16_HALF) >> CC_I16_FRAC_BITS);
   if      (v >  32767) v =  32767;
   else if (v < -32768) v = -32768;
   return (int16_t)v;
}

static void cc_i16_downsample(void *re_, struct resampler_data_int16 *data)
{
   cc_resampler_int16_t *re = (cc_resampler_int16_t*)re_;
   const int16_t *inp       = data->data_in;
   const int16_t *inp_max   = inp + data->input_frames * 2;
   int16_t       *outp      = data->data_out;
   int16_t       *outp0     = outp;
   int64_t        ratio     = (int64_t)((1.0 / data->ratio) * (double)CC_I16_ONE + 0.5);
   int64_t        b         = (int64_t)(data->ratio * (double)CC_I16_ONE + 0.5);

   while (inp != inp_max)
   {
      int64_t sl = inp[0];
      int64_t sr = inp[1];
      int64_t k0 = cc_i16_kernel(re->distance,               b);
      int64_t k1 = cc_i16_kernel(re->distance -     ratio,   b);
      int64_t k2 = cc_i16_kernel(re->distance - 2 * ratio,   b);

      re->buf_l[0] += sl * k0; re->buf_r[0] += sr * k0;
      re->buf_l[1] += sl * k1; re->buf_r[1] += sr * k1;
      re->buf_l[2] += sl * k2; re->buf_r[2] += sr * k2;

      re->distance += CC_I16_ONE;
      inp          += 2;

      if (re->distance > ratio + CC_I16_HALF)
      {
         outp[0]      = cc_i16_sat(re->buf_l[0]);
         outp[1]      = cc_i16_sat(re->buf_r[0]);

         re->buf_l[0] = re->buf_l[1]; re->buf_l[1] = re->buf_l[2]; re->buf_l[2] = 0;
         re->buf_r[0] = re->buf_r[1]; re->buf_r[1] = re->buf_r[2]; re->buf_r[2] = 0;

         re->distance -= ratio;
         outp         += 2;
      }
   }

   data->output_frames = (size_t)((outp - outp0) >> 1);
}

static void cc_i16_upsample(void *re_, struct resampler_data_int16 *data)
{
   cc_resampler_int16_t *re = (cc_resampler_int16_t*)re_;
   const int16_t *inp       = data->data_in;
   const int16_t *inp_max   = inp + data->input_frames * 2;
   int16_t       *outp      = data->data_out;
   int16_t       *outp0     = outp;
   int64_t        ratio     = (int64_t)((1.0 / data->ratio) * (double)CC_I16_ONE + 0.5);
   /* b = min(data->ratio, 1.0) */
   double         bf        = (data->ratio < 1.0) ? data->ratio : 1.0;
   int64_t        b         = (int64_t)(bf * (double)CC_I16_ONE + 0.5);

   while (inp != inp_max)
   {
      re->buf_l[0] = re->buf_l[1]; re->buf_l[1] = re->buf_l[2];
      re->buf_l[2] = re->buf_l[3]; re->buf_l[3] = inp[0];
      re->buf_r[0] = re->buf_r[1]; re->buf_r[1] = re->buf_r[2];
      re->buf_r[2] = re->buf_r[3]; re->buf_r[3] = inp[1];

      while (re->distance < CC_I16_ONE)
      {
         int     i;
         int64_t ol = 0;
         int64_t orr = 0;

         for (i = 0; i < 4; i++)
         {
            int64_t x = re->distance + CC_I16_ONE - (int64_t)i * CC_I16_ONE;
            int64_t k = cc_i16_kernel(x, b);
            ol  += re->buf_l[i] * k;
            orr += re->buf_r[i] * k;
         }

         outp[0]       = cc_i16_sat(ol);
         outp[1]       = cc_i16_sat(orr);
         re->distance += ratio;
         outp         += 2;
      }

      re->distance -= CC_I16_ONE;
      inp          += 2;
   }

   data->output_frames = (size_t)((outp - outp0) >> 1);
}

void *cc_resampler_int16_init(double bandwidth_mod)
{
   cc_resampler_int16_t *re = (cc_resampler_int16_t*)calloc(1, sizeof(*re));
   if (!re)
      return NULL;

   /* Variations of data->ratio around 0.75 are safer than around 1.0 for
    * both up/downsampler (matches the float driver's selection). */
   if (bandwidth_mod < 0.75)
   {
      re->process  = cc_i16_downsample;
      re->distance = 0;
   }
   else
   {
      re->process  = cc_i16_upsample;
      re->distance = 2 * CC_I16_ONE;
   }

   return re;
}

void cc_resampler_int16_process(void *re_, struct resampler_data_int16 *data)
{
   cc_resampler_int16_t *re = (cc_resampler_int16_t*)re_;
   if (re)
      re->process(re_, data);
}

void cc_resampler_int16_free(void *re_)
{
   cc_resampler_int16_t *re = (cc_resampler_int16_t*)re_;
   if (re)
      free(re);
}
