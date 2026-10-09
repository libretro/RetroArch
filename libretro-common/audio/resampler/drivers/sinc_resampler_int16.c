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
#include <memalign.h>
#include <math.h>

#include "sinc_resampler_internal.h"

#include <audio/sinc_resampler_int16.h>
#include <audio/sinc_resampler.h>

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

/* Coefficient-row prefetch.
 *
 * The phase advance is fully determined (time += ratio), so the row the next
 * output sample will read is known one iteration ahead. The table is swept
 * pseudo-randomly -- for any non-integer ratio consecutive outputs land in
 * unrelated rows -- so the row base is exactly the access the hardware
 * prefetcher cannot predict, while the run *within* a row is sequential and
 * needs no help.
 *
 * This is a hint only: it changes no arithmetic, the index is masked into
 * range, and the no-op fallback is equally correct. Output stays bit-exact. */
#if defined(__GNUC__) || defined(__clang__)
#define SINC_I16_PREFETCH(addr) __builtin_prefetch((const void*)(addr), 0, 3)
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#define SINC_I16_PREFETCH(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#else
#define SINC_I16_PREFETCH(addr) ((void)0)
#endif

/* Decimation by two ahead of the kernel when fast-forward takes the live
 * ratio below what the table rejects images for, as in the float driver:
 * each stage is the preset's table for a ratio of SINC_I16_DEC_ENGAGE / 2
 * at its single phase, with the same thresholds and crossfade. Decisions
 * run only when the ratio's bit pattern changes, so a stable ratio stays
 * integer-only. */
#define SINC_I16_DEC_STAGES       4
#define SINC_I16_DEC_BLOCK        256
#define SINC_I16_DEC_HALF         (2 * (SINC_I16_DEC_BLOCK / 2 + 1))
#define SINC_I16_DEC_ENGAGE_FIRST 0.97
#define SINC_I16_DEC_ENGAGE       0.9
#define SINC_I16_DEC_RELEASE_LAST 0.975
#define SINC_I16_DEC_RELEASE      1.25
#define SINC_I16_FADE_FRAMES      256

enum sinc_i16_window
{
   SINC_I16_WINDOW_LANCZOS = 0,
   SINC_I16_WINDOW_KAISER
};

typedef struct rarch_sinc_resampler_int16
{
   /* buffer_l owns one block that also holds the table and, on NEON,
    * the coefficient scratch; each region starts on a 64-byte boundary
    * from the rings' start and the others are views. */
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
   uint64_t  dec_bits;    /* ratio the stage decision was last taken for        */
   double    kaiser_beta;
   double    design_ratio;
   /* SINC_I16_DEC_STAGES one-phase stages, then the kernel's shadow. */
   struct rarch_sinc_resampler_int16 *dec;
   int16_t  *dec_block;
   int16_t  *fade_block;
   unsigned  dec_stages;
   unsigned  fade_stages;
   unsigned  fade_left;
   unsigned  fade_len;
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
   uint32_t row_mask = (1u << re->phase_bits) - 1u;

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
            const int32_t *np = re->phase_table
                              + (((re->time + ratio) >> sb) & row_mask) * taps2;

            SINC_I16_PREFETCH(np);
            /* Exact table phases need neither delta reads nor interpolation. */
            if (!dsub)
            {
               for (i = 0; i < taps; i++)
               {
                  sum_l += (int64_t)buffer_l[i] * pt[i];
                  sum_r += (int64_t)buffer_r[i] * pt[i];
               }
            }
            else
            {
               /* Both halves of the next row: the delta half is read first. */
               SINC_I16_PREFETCH(np + taps);

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
   uint32_t row_mask = (1u << re->phase_bits) - 1u;

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
            const int32_t *np = re->phase_table
                              + (((re->time + ratio) >> sb) & row_mask) * taps;

            SINC_I16_PREFETCH(np);

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

static void sinc_i16_kernel(rarch_sinc_resampler_int16_t *re,
      struct resampler_data_int16 *data)
{
   if (re->window == SINC_I16_WINDOW_KAISER)
      sinc_i16_process_kaiser(re, data);
   else
      sinc_i16_process_lanczos(re, data);
}

static size_t sinc_i16_dec_run(rarch_sinc_resampler_int16_t *kernel,
      const int16_t *in, size_t frames, int16_t *out, double ratio)
{
   struct resampler_data_int16 chunk;
   chunk.data_in       = in;
   chunk.input_frames  = frames;
   chunk.data_out      = out;
   chunk.output_frames = 0;
   chunk.ratio         = ratio;
   sinc_i16_kernel(kernel, &chunk);
   return chunk.output_frames;
}

/* Runs stages [first, last) from in; returns where the output is. */
static const int16_t *sinc_i16_dec_chain(rarch_sinc_resampler_int16_t *re,
      unsigned first, unsigned last, const int16_t *in, size_t *frames)
{
   int16_t *out = (in == re->dec_block) ? re->dec_block + SINC_I16_DEC_HALF
                                        : re->dec_block;
   for (; first < last; first++)
   {
      *frames = sinc_i16_dec_run(&re->dec[first], in, *frames, out, 0.5);
      in      = out;
      out     = (out == re->dec_block)
         ? re->dec_block + SINC_I16_DEC_HALF : re->dec_block;
   }
   return in;
}

/* Clean rings, and a clock that takes input before it emits: a joining
 * path then produces no output ahead of the frames it was given. */
static void sinc_i16_dec_restart(rarch_sinc_resampler_int16_t *stage)
{
   memset(stage->buffer_l, 0, 4 * stage->taps * sizeof(*stage->buffer_l));
   stage->ptr  = 0;
   stage->time = 1u << (stage->phase_bits + stage->subphase_bits);
}

/* The kernel's rings move to the shadow, which carries on with the old
 * stages; the kernel and any stage joining start clean. */
static void sinc_i16_dec_switch(rarch_sinc_resampler_int16_t *re,
      unsigned stages, double ratio)
{
   unsigned s;
   double warm = re->taps * ratio * (double)(1u << stages);
   rarch_sinc_resampler_int16_t *shadow = &re->dec[SINC_I16_DEC_STAGES];
   memcpy(shadow->buffer_l, re->buffer_l,
         4 * re->taps * sizeof(*re->buffer_l));
   shadow->ptr  = re->ptr;
   shadow->time = re->time;
   sinc_i16_dec_restart(re);
   for (s = re->dec_stages; s < stages; s++)
   {
      sinc_i16_dec_restart(&re->dec[s]);
      warm += re->dec[s].taps * ratio * (double)(1u << s);
   }
   if (warm > 8192.0)
      warm = 8192.0;
   re->fade_len    = (warm < 256.0) ? 256 : (unsigned)warm + 1;
   re->fade_left   = (unsigned)warm + 1 + re->fade_len;
   re->fade_stages = re->dec_stages;
   re->dec_stages  = stages;
}

static int16_t sinc_i16_mix(int16_t old, int16_t now, int32_t w)
{
   int32_t prod = ((int32_t)now - old) * w;
   if (prod >= 0)
      return (int16_t)(old + ((prod + 16384) >> 15));
   return (int16_t)(old - (((-prod) + 16384) >> 15));
}

static void sinc_i16_dec_blend(rarch_sinc_resampler_int16_t *re,
      int16_t *out, size_t frames, const int16_t *old, size_t old_frames)
{
   size_t i;
   if (!old_frames)
      return;
   for (i = 0; i < frames && re->fade_left; i++, re->fade_left--)
   {
      size_t o  = (i < old_frames) ? i : old_frames - 1;
      int32_t w = (re->fade_left > re->fade_len) ? 0
         : (int32_t)(((uint32_t)(re->fade_len - re->fade_left) << 15)
               / re->fade_len);
      out[2 * i]     = sinc_i16_mix(old[2 * o],     out[2 * i],     w);
      out[2 * i + 1] = sinc_i16_mix(old[2 * o + 1], out[2 * i + 1], w);
   }
}

static void sinc_i16_process_decimated(rarch_sinc_resampler_int16_t *re,
      struct resampler_data_int16 *data)
{
   const int16_t *input = data->data_in;
   int16_t *output      = data->data_out;
   size_t frames        = data->input_frames;
   size_t produced      = 0;
   size_t step          = SINC_I16_DEC_BLOCK;
   double ratio         = data->ratio;

   if (re->fade_left)
      while (step > 1 && step * ratio + 4.0 > SINC_I16_FADE_FRAMES)
         step >>= 1;

   while (frames)
   {
      size_t n         = (frames < step) ? frames : step;
      size_t got;
      unsigned now_k   = re->dec_stages;
      int16_t *out     = output + 2 * produced;
      const int16_t *p = input;
      input           += 2 * n;
      frames          -= n;
      if (!re->fade_left)
      {
         p   = sinc_i16_dec_chain(re, 0, now_k, p, &n);
         got = sinc_i16_dec_run(re, p, n, out, ratio * (double)(1u << now_k));
      }
      else
      {
         rarch_sinc_resampler_int16_t *shadow = &re->dec[SINC_I16_DEC_STAGES];
         unsigned old_k  = re->fade_stages;
         unsigned common = (now_k < old_k) ? now_k : old_k;
         size_t old;
         p = sinc_i16_dec_chain(re, 0, common, p, &n);
         /* The path without stages of its own reads p before the other
          * path's stages reuse its block. */
         if (now_k == common)
         {
            got = sinc_i16_dec_run(re, p, n, out,
                  ratio * (double)(1u << now_k));
            p   = sinc_i16_dec_chain(re, common, old_k, p, &n);
            old = sinc_i16_dec_run(shadow, p, n, re->fade_block,
                  ratio * (double)(1u << old_k));
         }
         else
         {
            old = sinc_i16_dec_run(shadow, p, n, re->fade_block,
                  ratio * (double)(1u << old_k));
            p   = sinc_i16_dec_chain(re, common, now_k, p, &n);
            got = sinc_i16_dec_run(re, p, n, out,
                  ratio * (double)(1u << now_k));
         }
         sinc_i16_dec_blend(re, out, got, re->fade_block, old);
      }
      produced += got;
   }
   data->output_frames = produced;
}

void sinc_resampler_int16_process(void *re_, struct resampler_data_int16 *data)
{
   uint64_t bits;
   rarch_sinc_resampler_int16_t *re = (rarch_sinc_resampler_int16_t*)re_;
   if (!sinc_resampler_ratio_valid(data->ratio,
            re->phase_bits, re->subphase_bits))
   {
      data->output_frames = 0;
      return;
   }
   memcpy(&bits, &data->ratio, sizeof(bits));
   if (bits != re->dec_bits && !re->fade_left)
   {
      unsigned stages = re->dec_stages;
      double ratio    = data->ratio;
      while (     stages < SINC_I16_DEC_STAGES
            && ratio * (double)(1u << stages)
               < re->design_ratio
               * (stages ? SINC_I16_DEC_ENGAGE : SINC_I16_DEC_ENGAGE_FIRST))
         stages++;
      while (stages && ratio * (double)(1u << (stages - 1))
            >= re->design_ratio * ((stages == 1)
               ? SINC_I16_DEC_RELEASE_LAST : SINC_I16_DEC_RELEASE))
         stages--;
      if (stages != re->dec_stages)
         sinc_i16_dec_switch(re, stages, ratio);
      re->dec_bits = bits;
   }
   if (!re->dec_stages && !re->fade_left)
      sinc_i16_kernel(re, data);
   else
      sinc_i16_process_decimated(re, data);
}

/* ------------------------------------------------------------------------- */
/* Lifecycle.                                                                */
/* ------------------------------------------------------------------------- */

void sinc_resampler_int16_reset(void *re_)
{
   rarch_sinc_resampler_int16_t *re = (rarch_sinc_resampler_int16_t*)re_;
   if (!re)
      return;
   memset(re->buffer_l, 0, 4 * re->taps * sizeof(*re->buffer_l));
   re->ptr         = 0;
   re->time        = 0;
   re->ratio_fixed = 0;
   re->ratio_bits  = 0;
   re->dec_bits    = 0;
   re->dec_stages  = 0;
   re->fade_left   = 0;
}

void sinc_resampler_int16_free(void *re_)
{
   rarch_sinc_resampler_int16_t *re = (rarch_sinc_resampler_int16_t*)re_;
   if (re)
   {
      memalign_free(re->buffer_l);
      free(re->dec);
   }
   free(re);
}

void *sinc_resampler_int16_init_hq(double bandwidth_mod,
      enum sinc_int16_quality quality, int hq_oversampling)
{
   double   cutoff  = 0.0;
   unsigned sidelobes = 0;
   int      window  = SINC_I16_WINDOW_LANCZOS;
   int      stride;
   size_t   phase_elems, o_table, o_coef, o_dec, o_dec_table, o_shadow,
            o_fade, len;
   int      phases;
   unsigned s, dec_taps;
   double   dec_cutoff, dec_beta;
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

   if (hq_oversampling && bandwidth_mod >= 2.0)
   {
      cutoff            = SINC_HQ_CUTOFF;
      sidelobes         = SINC_HQ_SIDELOBES;
      re->phase_bits    = SINC_HQ_PHASE_BITS;
      re->subphase_bits = SINC_HQ_SUBPHASE_BITS;
      window            = SINC_I16_WINDOW_KAISER;
      re->kaiser_beta   = SINC_HQ_KAISER_BETA;
   }

   if (!sinc_resampler_ratio_valid(bandwidth_mod,
            re->phase_bits, re->subphase_bits))
      goto error;

   re->window        = (unsigned)window;
   re->subphase_mask = (1u << re->subphase_bits) - 1u;
   re->taps          = sidelobes * 2;
   re->design_ratio  = (bandwidth_mod < 1.0) ? bandwidth_mod : 1.0;
   /* The Lanczos presets decimate through Normal's filter. */
   if (window == SINC_I16_WINDOW_KAISER)
   {
      dec_cutoff = cutoff;
      dec_beta   = re->kaiser_beta;
      dec_taps   = 4 * sidelobes;
   }
   else
   {
      dec_cutoff = 0.825;
      dec_beta   = 5.5;
      dec_taps   = 32;
   }
   dec_taps = ((unsigned)ceil(dec_taps / SINC_I16_DEC_ENGAGE) + 7u) & ~7u;

   /* Downsampling: lower cutoff and extend taps to hold stopband. */
   if (bandwidth_mod < 1.0)
   {
      cutoff  *= bandwidth_mod;
      re->taps = (unsigned)ceil((double)re->taps / bandwidth_mod);
   }

   /* Keep taps a multiple of 8, matching the float driver's rounding so
    * the two drivers select the same filter on every platform.  (Also
    * satisfies the scalar unroll's multiple-of-4 requirement.) */
   re->taps = (re->taps + 7u) & ~7u;

   stride      = (window == SINC_I16_WINDOW_KAISER) ? 2 : 1;
   phases      = 1 << re->phase_bits;
   phase_elems = (size_t)phases * re->taps * stride;

   /* The two rings first, then the table, then (NEON) the coefficient
    * scratch, out of one block; the rings start zeroed as before. The
    * block is owned through buffer_l. */
   o_table  = (sizeof(int16_t) * 4 * re->taps + 63) & ~(size_t)63;
   o_coef   = o_table + ((sizeof(int32_t) * phase_elems + 63) & ~(size_t)63);
   len      = o_coef;
#ifdef SINC_I16_KAISER_FISSION
   len     += (sizeof(int32_t) * (re->taps + 4u) + 63) & ~(size_t)63;
#endif
   /* Then the stage rings, their table, the shadow's rings and the
    * blocks the paths hand through. */
   o_dec       = len;
   o_dec_table = o_dec + ((sizeof(int16_t) * 4 * dec_taps
         * SINC_I16_DEC_STAGES + 63) & ~(size_t)63);
   o_shadow    = o_dec_table + ((sizeof(int32_t) * 2 * dec_taps + 63)
         & ~(size_t)63);
   o_fade      = o_shadow + ((sizeof(int16_t) * 4 * re->taps + 63)
         & ~(size_t)63);
   len         = o_fade + sizeof(int16_t)
      * (2 * SINC_I16_FADE_FRAMES + 2 * SINC_I16_DEC_HALF);
   re->buffer_l    = (int16_t*)memalign_alloc(128, len);
   re->dec         = (rarch_sinc_resampler_int16_t*)
      calloc(SINC_I16_DEC_STAGES + 1, sizeof(*re->dec));
   if (!re->buffer_l || !re->dec)
      goto error;
   memset(re->buffer_l, 0, sizeof(int16_t) * 4 * re->taps);
   re->buffer_r    = re->buffer_l + 2 * re->taps;
   re->phase_table = (int32_t*)((uint8_t*)re->buffer_l + o_table);
#ifdef SINC_I16_KAISER_FISSION
   re->coef_scratch = (int32_t*)((uint8_t*)re->buffer_l + o_coef);
#endif

   if (window == SINC_I16_WINDOW_KAISER)
      sinc_i16_init_table_kaiser(re, cutoff, re->phase_table,
            phases, (int)re->taps);
   else
      sinc_i16_init_table_lanczos(re, cutoff, re->phase_table,
            phases, (int)re->taps);

   re->fade_block = (int16_t*)((uint8_t*)re->buffer_l + o_fade);
   re->dec_block  = re->fade_block + 2 * SINC_I16_FADE_FRAMES;
   /* The Kaiser builder's delta row follows the one the stages read. */
   for (s = 0; s < SINC_I16_DEC_STAGES; s++)
   {
      rarch_sinc_resampler_int16_t *stage = &re->dec[s];
      stage->phase_table   = (int32_t*)((uint8_t*)re->buffer_l + o_dec_table);
      stage->buffer_l      = (int16_t*)((uint8_t*)re->buffer_l + o_dec)
         + 4 * dec_taps * s;
      stage->buffer_r      = stage->buffer_l + 2 * dec_taps;
      stage->taps          = dec_taps;
      stage->subphase_bits = 1;
      stage->subphase_mask = 1;
      stage->window        = SINC_I16_WINDOW_LANCZOS;
      stage->kaiser_beta   = dec_beta;
   }
   sinc_i16_init_table_kaiser(&re->dec[0],
         0.5 * SINC_I16_DEC_ENGAGE * dec_cutoff,
         re->dec[0].phase_table, 1, (int)dec_taps);

   re->dec[SINC_I16_DEC_STAGES]          = *re;
   re->dec[SINC_I16_DEC_STAGES].dec      = NULL;
   re->dec[SINC_I16_DEC_STAGES].buffer_l = (int16_t*)
      ((uint8_t*)re->buffer_l + o_shadow);
   re->dec[SINC_I16_DEC_STAGES].buffer_r =
      re->dec[SINC_I16_DEC_STAGES].buffer_l + 2 * re->taps;

   return re;

error:
   sinc_resampler_int16_free(re);
   return NULL;
}

void *sinc_resampler_int16_init(double bandwidth_mod,
      enum sinc_int16_quality quality)
{
   return sinc_resampler_int16_init_hq(bandwidth_mod, quality, 0);
}
