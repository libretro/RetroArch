/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

// Bog-standard windowed SINC implementation.
// Only suitable as an upsampler, as there is no low-pass filter stage.

#include "resampler.h"
#include "../performance.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef RESAMPLER_TEST
#include "../general.h"
#else
#define RARCH_LOG(...)
#endif

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#ifdef SINC_LOWER_QUALITY
#define PHASE_BITS 12
#define SIDELOBES 4
#define ENABLE_AVX 0
#elif defined(SINC_HIGHER_QUALITY)
#define PHASE_BITS 16
#define SIDELOBES 32
#define ENABLE_AVX 1
#else
#define PHASE_BITS 16
#define SIDELOBES 8
#define ENABLE_AVX 0
#endif

// For the little amount of taps we're using,
// SSE1 is faster than AVX for some reason.
// AVX code is kept here though as by increasing number
// of sinc taps, the AVX code is clearly faster than SSE1.

#if defined(__AVX__) && ENABLE_AVX
#include <immintrin.h>
#endif

#define SUBPHASE_BITS 10
#define PHASES (1 << (PHASE_BITS + SUBPHASE_BITS))

#define TAPS (SIDELOBES * 2)
#define CUTOFF 0.98

typedef struct rarch_sinc_resampler
{
   sample_t phase_table[1 << PHASE_BITS][TAPS];
   sample_t buffer_l[2 * TAPS];
   sample_t buffer_r[2 * TAPS];

   unsigned ptr;
   uint32_t time;
} rarch_sinc_resampler_t;

static inline double sinc(double val)
{
   if (fabs(val) < 0.00001)
      return 1.0;
   else
      return sin(val) / val;
}

static inline double lanzcos(double index)
{
   return sinc(index);
}

static void init_sinc_table(rarch_sinc_resampler_t *resamp)
{
   // Sinc phases: [..., p + 3, p + 2, p + 1, p + 0, p - 1, p - 2, p - 3, p - 4, ...]
   for (int i = 0; i < (1 << PHASE_BITS); i++)
   {
      for (int j = 0; j < TAPS; j++)
      {
         double p = (double)i / (1 << PHASE_BITS);
         double sinc_phase = M_PI * (p + (SIDELOBES - 1 - j));

         float val = CUTOFF * sinc(CUTOFF * sinc_phase) * lanzcos(sinc_phase / SIDELOBES);
         resamp->phase_table[i][j] = val;
      }
   }
}

// No memalign() for us on Win32 ...
static void *aligned_alloc__(size_t boundary, size_t size)
{
   void *ptr = malloc(boundary + size + sizeof(uintptr_t));
   if (!ptr)
      return NULL;

   uintptr_t addr = ((uintptr_t)ptr + sizeof(uintptr_t) + boundary) & ~(boundary - 1);
   void **place   = (void**)addr;
   place[-1]      = ptr;

   return (void*)addr;
}

static void aligned_free__(void *ptr)
{
   void **p = (void**)ptr;
   free(p[-1]);
}

static inline void process_sinc_C(rarch_sinc_resampler_t *resamp, float *out_buffer)
{
   float sum_l = 0.0f;
   float sum_r = 0.0f;
   const float *buffer_l = resamp->buffer_l + resamp->ptr;
   const float *buffer_r = resamp->buffer_r + resamp->ptr;

   unsigned phase = resamp->time >> SUBPHASE_BITS;
   const float *phase_table = resamp->phase_table[phase];

   for (unsigned i = 0; i < TAPS; i++)
   {
      float sinc_val = phase_table[i];
      sum_l         += buffer_l[i] * sinc_val;
      sum_r         += buffer_r[i] * sinc_val;
   }

   out_buffer[0] = sum_l;
   out_buffer[1] = sum_r;
}

#if defined(__AVX__) && ENABLE_AVX
#define process_sinc_func process_sinc
static void process_sinc(rarch_sinc_resampler_t *resamp, float *out_buffer)
{
   __m256 sum_l = _mm256_setzero_ps();
   __m256 sum_r = _mm256_setzero_ps();

   const float *buffer_l = resamp->buffer_l + resamp->ptr;
   const float *buffer_r = resamp->buffer_r + resamp->ptr;

   unsigned phase = resamp->time >> SUBPHASE_BITS;
   const float *phase_table = resamp->phase_table[phase];

   for (unsigned i = 0; i < TAPS; i += 8)
   {
      __m256 buf_l = _mm256_loadu_ps(buffer_l + i);
      __m256 buf_r = _mm256_loadu_ps(buffer_r + i);

      __m256 sinc = _mm256_load_ps(phase_table + i);
      sum_l       = _mm256_add_ps(sum_l, _mm256_mul_ps(buf_l, sinc));
      sum_r       = _mm256_add_ps(sum_r, _mm256_mul_ps(buf_r, sinc));
   }

   // hadd on AVX is weird, and acts on low-lanes and high-lanes separately.
   __m256 res_l = _mm256_hadd_ps(sum_l, sum_l);
   __m256 res_r = _mm256_hadd_ps(sum_r, sum_r);
   res_l = _mm256_hadd_ps(res_l, res_l);
   res_r = _mm256_hadd_ps(res_r, res_r);
   res_l = _mm256_add_ps(_mm256_permute2f128_ps(res_l, res_l, 1), res_l);
   res_r = _mm256_add_ps(_mm256_permute2f128_ps(res_r, res_r, 1), res_r);

   // This is optimized to mov %xmmN, [mem].
   // There doesn't seem to be any _mm256_store_ss intrinsic.
   _mm_store_ss(out_buffer + 0, _mm256_extractf128_ps(res_l, 0));
   _mm_store_ss(out_buffer + 1, _mm256_extractf128_ps(res_r, 0));
}
#elif defined(__SSE__)
#define process_sinc_func process_sinc
static void process_sinc(rarch_sinc_resampler_t *resamp, float *out_buffer)
{
   __m128 sum_l = _mm_setzero_ps();
   __m128 sum_r = _mm_setzero_ps();

   const float *buffer_l = resamp->buffer_l + resamp->ptr;
   const float *buffer_r = resamp->buffer_r + resamp->ptr;

   unsigned phase = resamp->time >> SUBPHASE_BITS;
   const float *phase_table = resamp->phase_table[phase];

   for (unsigned i = 0; i < TAPS; i += 4)
   {
      __m128 buf_l = _mm_loadu_ps(buffer_l + i);
      __m128 buf_r = _mm_loadu_ps(buffer_r + i);

      __m128 sinc = _mm_load_ps(phase_table + i);
      sum_l       = _mm_add_ps(sum_l, _mm_mul_ps(buf_l, sinc));
      sum_r       = _mm_add_ps(sum_r, _mm_mul_ps(buf_r, sinc));
   }

   // Them annoying shuffles :V
   // sum_l = { l3, l2, l1, l0 }
   // sum_r = { r3, r2, r1, r0 }

   __m128 sum = _mm_add_ps(_mm_shuffle_ps(sum_l, sum_r, _MM_SHUFFLE(1, 0, 1, 0)),
         _mm_shuffle_ps(sum_l, sum_r, _MM_SHUFFLE(3, 2, 3, 2)));

   // sum   = { r1, r0, l1, l0 } + { r3, r2, l3, l2 }
   // sum   = { R1, R0, L1, L0 }

   sum = _mm_add_ps(_mm_shuffle_ps(sum, sum, _MM_SHUFFLE(3, 3, 1, 1)), sum);

   // sum   = {R1, R1, L1, L1 } + { R1, R0, L1, L0 }
   // sum   = { X,  R,  X,  L } 

   // Store L
   _mm_store_ss(out_buffer + 0, sum);

   // movehl { X, R, X, L } == { X, R, X, R }
   _mm_store_ss(out_buffer + 1, _mm_movehl_ps(sum, sum));
}
#elif defined(HAVE_NEON)

// Need to make this function pointer as Android doesn't have built-in targets
// for NEON and plain ARMv7a.
static void (*process_sinc_func)(rarch_sinc_resampler_t *resamp, float *out_buffer);

// Assumes that taps >= 8, and that taps is a multiple of 8.
void process_sinc_neon_asm(float *out, const float *left, const float *right, const float *coeff, unsigned taps);

static void process_sinc_neon(rarch_sinc_resampler_t *resamp, float *out_buffer)
{
   const float *buffer_l = resamp->buffer_l + resamp->ptr;
   const float *buffer_r = resamp->buffer_r + resamp->ptr;

   unsigned phase = resamp->time >> SUBPHASE_BITS;
   const float *phase_table = resamp->phase_table[phase];

   process_sinc_neon_asm(out_buffer, buffer_l, buffer_r, phase_table, TAPS);
}
#else // Plain ol' C99
#define process_sinc_func process_sinc_C
#endif

static void resampler_sinc_process(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)re_;

   // If data->ratio is < 1, we are downsampling.
   // The sinc table is not set up for this, as it always assumes upsampling.
   // Downsampling will work, but with some added noise due to aliasing might be present.
   uint32_t ratio = PHASES / data->ratio;

   const sample_t *input = data->data_in;
   sample_t *output      = data->data_out;
   size_t frames         = data->input_frames;
   size_t out_frames     = 0;

   while (frames)
   {
      while (frames && re->time >= PHASES)
      {
         re->buffer_l[re->ptr + TAPS] = re->buffer_l[re->ptr] = *input++;
         re->buffer_r[re->ptr + TAPS] = re->buffer_r[re->ptr] = *input++;
         re->ptr = (re->ptr + 1) & (TAPS - 1);

         re->time -= PHASES;
         frames--;
      }

      while (re->time < PHASES)
      {
         process_sinc_func(re, output);
         output += 2;
         out_frames++;
         re->time += ratio;
      }
   }

   data->output_frames = out_frames;
}

static void resampler_sinc_free(void *re)
{
   aligned_free__(re);
}

static void *resampler_sinc_new(void)
{
   rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)aligned_alloc__(128, sizeof(*re));
   if (!re)
      return NULL;

   memset(re, 0, sizeof(*re));

   init_sinc_table(re);

#if defined(__AVX__) && ENABLE_AVX
   RARCH_LOG("Sinc resampler [AVX]\n");
#elif defined(__SSE__)
   RARCH_LOG("Sinc resampler [SSE]\n");
#elif defined(HAVE_NEON)
   struct rarch_cpu_features cpu;
   rarch_get_cpu_features(&cpu);
   process_sinc_func = cpu.simd & RARCH_SIMD_NEON ? process_sinc_neon : process_sinc_C;
   RARCH_LOG("Sinc resampler [%s]\n", cpu.simd & RARCH_SIMD_NEON ? "NEON" : "C");
#else
   RARCH_LOG("Sinc resampler [C]\n");
#endif

   RARCH_LOG("SINC params (%u phase bits, %u taps).\n", PHASE_BITS, TAPS);

   return re;
}

const rarch_resampler_t sinc_resampler = {
   resampler_sinc_new,
   resampler_sinc_process,
   resampler_sinc_free,
   "sinc",
};

