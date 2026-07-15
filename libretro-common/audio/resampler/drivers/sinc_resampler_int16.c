/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file
 * (sinc_resampler_int16.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

/* Fixed-point (integer-only) windowed-SINC resampler.
 *
 * This is a bit-transparent counterpart to the float "sinc" driver in
 * drivers/sinc_resampler.c.  It exists for two reasons:
 *
 *   1) Determinism.  The float driver is compiled with -ffast-math and its
 *      output is therefore not bit-reproducible across compilers, archs or
 *      FMA-contraction settings.  This driver uses integer MACs only and is
 *      bit-identical everywhere, which matters for netplay / rewind.
 *
 *   2) It removes the s16->float->s16 round-trip on the (universal) libretro
 *      int16 audio path, and runs with no FPU dependency at all, which helps
 *      on FPU-poor targets (PS2, 3DS, older ARM).
 *
 * The control flow (ring push, phase = time >> subphase_bits, subphase delta,
 * time += ratio) is identical to the float driver; only the coefficients and
 * samples are integer.  Coefficients are Q1.30 in int32; samples are int16;
 * products accumulate in int64 and are round-half-away-from-zero shifted back
 * to int16 with saturation.
 *
 * NB: everything here is written to C89 / MSVC rules (declarations at the top
 * of each block, no // comments) and avoids implementation-defined signed
 * right shifts so the result is portable and deterministic.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audio/sinc_resampler_int16.h>

/* On targets whose compiler can auto-vectorize an int16*int32->int64 MAC
 * (e.g. AArch64/NEON via smlal), splitting the Kaiser inner loop into a
 * branchless interpolation pass and a pure MAC pass lets the vectorizer take
 * the MAC. On targets without that primitive (e.g. x86 SSE2) the fused loop is
 * faster (no scratch round-trip), so this is gated to NEON only. */
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SINC_I16_KAISER_FISSION 1
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum sinc_i16_window
{
   SINC_I16_WINDOW_LANCZOS = 0,
   SINC_I16_WINDOW_KAISER
};

typedef struct rarch_sinc_resampler_int16
{
   int32_t  *phase_table; /* Q1.30 coefficients (+ interleaved deltas for Kaiser) */
   int16_t  *buffer_l;    /* 2 * taps int16 ring (doubled to stay contiguous)     */
   int16_t  *buffer_r;
   unsigned  phase_bits;
   unsigned  subphase_bits;
   uint32_t  subphase_mask;
   unsigned  taps;
   unsigned  window;      /* enum sinc_i16_window */
#ifdef SINC_I16_KAISER_FISSION
   int32_t  *coef_scratch;/* per-output effective Kaiser coeffs (NEON fission) */
#endif
   unsigned  ptr;
   uint32_t  time;
   uint32_t  ratio_fixed; /* cached (uint32)(phases / ratio); recomputed only  */
   uint64_t  ratio_bits;  /* when the double ratio's bit pattern changes        */
   double    kaiser_beta;
} rarch_sinc_resampler_int16_t;

/* ------------------------------------------------------------------------- */
/* Local math helpers (kept local so the module is self-contained).          */
/* ------------------------------------------------------------------------- */

static double sinc_i16_sinc(double val)
{
   if (fabs(val) < 0.00000000001)
      return 1.0;
   return sin(val) / val;
}

static double sinc_i16_besseli0(double x)
{
   double sum     = 1.0;
   double term    = 1.0;
   double half_sq = (x * 0.5) * (x * 0.5);
   int    k       = 1;
   for (;;)
   {
      term *= half_sq / ((double)k * (double)k);
      sum  += term;
      if (term < 1.0e-21 * sum)
         break;
      k++;
   }
   return sum;
}

/* Round a real coefficient to Q1.30, round-half-away-from-zero. */
static int32_t sinc_i16_to_q30(double val)
{
   double scale = (double)((int64_t)1 << SINC_INT16_COEFF_BITS);
   double s     = val * scale;
   if (s >= 0.0)
      return (int32_t)floor(s + 0.5);
   return (int32_t)ceil(s - 0.5);
}

/* Round-half-away-from-zero shift of a signed int64 by COEFF_BITS.
 * Only shifts non-negative values, so it is fully defined in C89. */
static int64_t sinc_i16_round_shift(int64_t acc)
{
   int64_t half = (int64_t)1 << (SINC_INT16_COEFF_BITS - 1);
   if (acc >= 0)
      return  ( acc + half) >> SINC_INT16_COEFF_BITS;
   return    -((-acc + half) >> SINC_INT16_COEFF_BITS);
}

static int16_t sinc_i16_sat(int64_t v)
{
   if (v >  32767)
      return  32767;
   if (v < -32768)
      return -32768;
   return (int16_t)v;
}

/* ------------------------------------------------------------------------- */
/* Table generation.                                                         */
/* ------------------------------------------------------------------------- */

static void sinc_i16_init_table_kaiser(rarch_sinc_resampler_int16_t *re,
      double cutoff, int32_t *table, int phases, int taps)
{
   int    i, j, p;
   double beta      = re->kaiser_beta;
   double window_mod = sinc_i16_besseli0(beta);
   double sidelobes = taps / 2.0;
   int    stride    = 2; /* coeff + delta interleaved per phase */

   for (i = 0; i < phases; i++)
   {
      for (j = 0; j < taps; j++)
      {
         double sinc_phase;
         double arg;
         double val;
         int    n            = j * phases + i;
         double window_phase = (double)n / (phases * taps);
         window_phase        = 2.0 * window_phase - 1.0;
         sinc_phase          = sidelobes * window_phase;
         arg                 = 1.0 - window_phase * window_phase;
         if (arg < 0.0)
            arg = 0.0;
         val = cutoff * sinc_i16_sinc(M_PI * sinc_phase * cutoff) *
               sinc_i16_besseli0(beta * sqrt(arg)) / window_mod;
         table[i * stride * taps + j] = sinc_i16_to_q30(val);
      }
   }

   /* Deltas between adjacent phases (for subphase linear interpolation). */
   for (p = 0; p < phases - 1; p++)
   {
      for (j = 0; j < taps; j++)
         table[(p * stride + 1) * taps + j] =
              table[(p + 1) * stride * taps + j]
            - table[ p      * stride * taps + j];
   }

   /* Final phase: delta towards the coefficients at phase == phases. */
   for (j = 0; j < taps; j++)
   {
      double sinc_phase;
      double arg;
      double val;
      int32_t v;
      int    n            = j * phases + phases;
      double window_phase = (double)n / (phases * taps);
      window_phase        = 2.0 * window_phase - 1.0;
      sinc_phase          = sidelobes * window_phase;
      arg                 = 1.0 - window_phase * window_phase;
      if (arg < 0.0)
         arg = 0.0;
      val = cutoff * sinc_i16_sinc(M_PI * sinc_phase * cutoff) *
            sinc_i16_besseli0(beta * sqrt(arg)) / window_mod;
      v   = sinc_i16_to_q30(val);
      table[((phases - 1) * stride + 1) * taps + j] =
         v - table[(phases - 1) * stride * taps + j];
   }
}

static void sinc_i16_init_table_lanczos(rarch_sinc_resampler_int16_t *re,
      double cutoff, int32_t *table, int phases, int taps)
{
   int    i, j;
   double sidelobes = taps / 2.0;
   (void)re;

   for (i = 0; i < phases; i++)
   {
      for (j = 0; j < taps; j++)
      {
         double sinc_phase;
         double val;
         int    n            = j * phases + i;
         double window_phase = (double)n / (phases * taps);
         window_phase        = 2.0 * window_phase - 1.0;
         sinc_phase          = sidelobes * window_phase;
         val = cutoff * sinc_i16_sinc(M_PI * sinc_phase * cutoff) *
               sinc_i16_sinc(M_PI * window_phase);
         table[i * taps + j] = sinc_i16_to_q30(val);
      }
   }
}

/* ------------------------------------------------------------------------- */
/* Processing.                                                               */
/* ------------------------------------------------------------------------- */

#define SINC_I16_PUSH(re, input, taps, phases, frames) \
   do { \
      while ((frames) && (re)->time >= (phases)) \
      { \
         if (!(re)->ptr) \
            (re)->ptr = (taps); \
         (re)->ptr--; \
         (re)->buffer_l[(re)->ptr + (taps)] = \
            (re)->buffer_l[(re)->ptr]       = (input)[0]; \
         (re)->buffer_r[(re)->ptr + (taps)] = \
            (re)->buffer_r[(re)->ptr]       = (input)[1]; \
         (input) += 2; \
         (re)->time -= (phases); \
         (frames)--; \
      } \
   } while (0)

/* Convert the (double) ratio to the fixed-point time step. This is the only
 * floating-point operation in the whole runtime path, and it is skipped unless
 * the ratio actually changes: the double's bit pattern is compared as an
 * integer, so a stable ratio (the common case, and the only one used for
 * deterministic netplay) touches no FP after the first flush. Bit-exact with
 * the previous inline (uint32_t)(phases / data->ratio). */
static uint32_t sinc_i16_time_step(rarch_sinc_resampler_int16_t *re,
      unsigned phases, double ratio)
{
   uint64_t bits;
   memcpy(&bits, &ratio, sizeof(bits));
   if (bits != re->ratio_bits)
   {
      re->ratio_bits  = bits;
      re->ratio_fixed = (uint32_t)(phases / ratio);
   }
   return re->ratio_fixed;
}

static void sinc_i16_process_kaiser(rarch_sinc_resampler_int16_t *re,
      struct resampler_data_int16 *data)
{
   unsigned phases   = 1u << (re->phase_bits + re->subphase_bits);
   uint32_t ratio    = sinc_i16_time_step(re, phases, data->ratio);
   const int16_t *in = data->data_in;
   int16_t *output   = data->data_out;
   size_t frames     = data->input_frames;
   size_t out_frames = 0;
   unsigned taps     = re->taps;
   unsigned taps2    = taps * 2;
   unsigned sb       = re->subphase_bits;
   uint32_t sub_mask = re->subphase_mask;

   while (frames)
   {
      SINC_I16_PUSH(re, in, taps, phases, frames);

      {
         const int16_t *buffer_l = re->buffer_l + re->ptr;
         const int16_t *buffer_r = re->buffer_r + re->ptr;
         while (re->time < phases)
         {
            unsigned i;
            int64_t  sum_l    = 0;
            int64_t  sum_r    = 0;
            unsigned phase    = re->time >> sb;
            const int32_t *pt = re->phase_table + phase * taps2;
            const int32_t *dt = pt + taps;
            uint32_t dsub     = re->time & sub_mask;

#ifdef SINC_I16_KAISER_FISSION
            {
               int32_t *cs = re->coef_scratch;
               /* Interp pass: coeff = pt[i] + trunc(dsub*dt[i] / 2^sb).
                * Adding (2^sb-1) to negative products before the arithmetic
                * shift turns floor into truncate-toward-zero (branchless, so
                * the following MAC pass auto-vectorizes; bit-exact). */
               for (i = 0; i < taps; i++)
               {
                  int64_t prod = (int64_t)dsub * dt[i];
                  int64_t bias = (prod >> 63) & (((int64_t)1 << sb) - 1);
                  cs[i]        = pt[i] + (int32_t)((prod + bias) >> sb);
               }
               for (i = 0; i < taps; i++)
               {
                  sum_l += (int64_t)buffer_l[i] * cs[i];
                  sum_r += (int64_t)buffer_r[i] * cs[i];
               }
            }
#else
            for (i = 0; i < taps; i++)
            {
               /* coeff = pt[i] + trunc(dsub * dt[i] / 2^sb); dsub >= 0. */
               int64_t prod = (int64_t)dsub * dt[i];
               int32_t c;
               if (prod >= 0)
                  c = pt[i] + (int32_t)( prod >> sb);
               else
                  c = pt[i] - (int32_t)((-prod) >> sb);
               sum_l += (int64_t)buffer_l[i] * c;
               sum_r += (int64_t)buffer_r[i] * c;
            }
#endif

            output[0] = sinc_i16_sat(sinc_i16_round_shift(sum_l));
            output[1] = sinc_i16_sat(sinc_i16_round_shift(sum_r));
            output   += 2;
            out_frames++;
            re->time += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}

static void sinc_i16_process_lanczos(rarch_sinc_resampler_int16_t *re,
      struct resampler_data_int16 *data)
{
   unsigned phases   = 1u << (re->phase_bits + re->subphase_bits);
   uint32_t ratio    = sinc_i16_time_step(re, phases, data->ratio);
   const int16_t *in = data->data_in;
   int16_t *output   = data->data_out;
   size_t frames     = data->input_frames;
   size_t out_frames = 0;
   unsigned taps     = re->taps;
   unsigned sb       = re->subphase_bits;

   while (frames)
   {
      SINC_I16_PUSH(re, in, taps, phases, frames);

      {
         const int16_t *buffer_l = re->buffer_l + re->ptr;
         const int16_t *buffer_r = re->buffer_r + re->ptr;
         while (re->time < phases)
         {
            unsigned i;
            int64_t  sum_l    = 0;
            int64_t  sum_r    = 0;
            unsigned phase    = re->time >> sb;
            const int32_t *pt = re->phase_table + phase * taps;

            for (i = 0; i < taps; i++)
            {
               sum_l += (int64_t)buffer_l[i] * pt[i];
               sum_r += (int64_t)buffer_r[i] * pt[i];
            }

            output[0] = sinc_i16_sat(sinc_i16_round_shift(sum_l));
            output[1] = sinc_i16_sat(sinc_i16_round_shift(sum_r));
            output   += 2;
            out_frames++;
            re->time += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}

void sinc_resampler_int16_process(void *re_, struct resampler_data_int16 *data)
{
   rarch_sinc_resampler_int16_t *re = (rarch_sinc_resampler_int16_t*)re_;
   if (re->window == SINC_I16_WINDOW_KAISER)
      sinc_i16_process_kaiser(re, data);
   else
      sinc_i16_process_lanczos(re, data);
}

/* ------------------------------------------------------------------------- */
/* Lifecycle.                                                                */
/* ------------------------------------------------------------------------- */

void sinc_resampler_int16_free(void *re_)
{
   rarch_sinc_resampler_int16_t *re = (rarch_sinc_resampler_int16_t*)re_;
   if (re)
   {
      free(re->phase_table);
      free(re->buffer_l);
#ifdef SINC_I16_KAISER_FISSION
      free(re->coef_scratch);
#endif
   }
   free(re);
}

void *sinc_resampler_int16_init(double bandwidth_mod,
      enum sinc_int16_quality quality)
{
   double   cutoff  = 0.0;
   unsigned sidelobes = 0;
   int      window  = SINC_I16_WINDOW_LANCZOS;
   int      stride;
   size_t   phase_elems;
   int      phases;
   rarch_sinc_resampler_int16_t *re =
      (rarch_sinc_resampler_int16_t*)calloc(1, sizeof(*re));

   if (!re)
      return NULL;

   switch (quality)
   {
      case SINC_INT16_QUALITY_LOWEST:
         cutoff           = 0.98;
         sidelobes        = 2;
         re->phase_bits   = 12;
         re->subphase_bits = 10;
         window           = SINC_I16_WINDOW_LANCZOS;
         break;
      case SINC_INT16_QUALITY_LOWER:
         cutoff           = 0.98;
         sidelobes        = 4;
         re->phase_bits   = 12;
         re->subphase_bits = 10;
         window           = SINC_I16_WINDOW_LANCZOS;
         break;
      case SINC_INT16_QUALITY_HIGHER:
         cutoff           = 0.90;
         sidelobes        = 32;
         re->phase_bits   = 10;
         re->subphase_bits = 14;
         window           = SINC_I16_WINDOW_KAISER;
         re->kaiser_beta  = 10.5;
         break;
      case SINC_INT16_QUALITY_HIGHEST:
         cutoff           = 0.962;
         sidelobes        = 128;
         re->phase_bits   = 10;
         re->subphase_bits = 14;
         window           = SINC_I16_WINDOW_KAISER;
         re->kaiser_beta  = 14.5;
         break;
      case SINC_INT16_QUALITY_NORMAL:
      default:
         cutoff           = 0.825;
         sidelobes        = 8;
         re->phase_bits   = 8;
         re->subphase_bits = 16;
         window           = SINC_I16_WINDOW_KAISER;
         re->kaiser_beta  = 5.5;
         break;
   }

   re->window        = (unsigned)window;
   re->subphase_mask = (1u << re->subphase_bits) - 1u;
   re->taps          = sidelobes * 2;

   /* Downsampling: lower cutoff and extend taps to hold stopband. */
   if (bandwidth_mod < 1.0)
   {
      cutoff  *= bandwidth_mod;
      re->taps = (unsigned)ceil((double)re->taps / bandwidth_mod);
   }

   /* Keep taps a multiple of 4 (scalar unroll / future SIMD friendliness). */
   re->taps = (re->taps + 3u) & ~3u;

   stride      = (window == SINC_I16_WINDOW_KAISER) ? 2 : 1;
   phases      = 1 << re->phase_bits;
   phase_elems = (size_t)phases * re->taps * stride;

   re->phase_table = (int32_t*)malloc(sizeof(int32_t) * phase_elems);
   re->buffer_l    = (int16_t*)calloc(4 * re->taps, sizeof(int16_t));
   if (!re->phase_table || !re->buffer_l)
      goto error;
   re->buffer_r    = re->buffer_l + 2 * re->taps;

#ifdef SINC_I16_KAISER_FISSION
   re->coef_scratch = (int32_t*)malloc(sizeof(int32_t) * (re->taps + 4u));
   if (!re->coef_scratch)
      goto error;
#endif

   if (window == SINC_I16_WINDOW_KAISER)
      sinc_i16_init_table_kaiser(re, cutoff, re->phase_table,
            phases, (int)re->taps);
   else
      sinc_i16_init_table_lanczos(re, cutoff, re->phase_table,
            phases, (int)re->taps);

   return re;

error:
   sinc_resampler_int16_free(re);
   return NULL;
}
