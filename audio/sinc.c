/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// Bog-standard windowed SINC implementation.
// Only suitable as an upsampler, as there is no low-pass filter stage.

#include "resampler.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef RESAMPLER_TEST
#include "../general.h"
#else
#define RARCH_LOG(...)
#endif

#if __SSE__
#include <xmmintrin.h>
#endif

#define PHASE_BITS 8
#define SUBPHASE_BITS 16

#define PHASES (1 << PHASE_BITS)
#define PHASES_SHIFT (SUBPHASE_BITS)
#define SUBPHASES (1 << SUBPHASE_BITS)
#define SUBPHASES_SHIFT 0
#define SUBPHASES_MASK ((1 << SUBPHASE_BITS) - 1)
#define PHASES_WRAP (1 << (PHASE_BITS + SUBPHASE_BITS))
#define FRAMES_SHIFT (PHASE_BITS + SUBPHASE_BITS)

#define SIDELOBES 16
#define TAPS (SIDELOBES * 2)
#define CUTOFF 0.9

#define PHASE_INDEX 0
#define DELTA_INDEX 1

struct rarch_resampler
{
   float phase_table[PHASES][2][TAPS];
   float buffer_l[2 * TAPS];
   float buffer_r[2 * TAPS];

   unsigned ptr;

   uint32_t time;
};

void resampler_preinit(rarch_resampler_t *re, double omega, double *samples_offset)
{
   *samples_offset = SIDELOBES + 1;
   for (int i = 0; i < 2 * SIDELOBES; i++)
   {
      re->buffer_l[i] = re->buffer_l[i + TAPS] = cos((i - (SIDELOBES - 1)) * omega);
      re->buffer_r[i] = re->buffer_r[i + TAPS] = re->buffer_l[i];
   }

   re->time = 0;
   re->ptr = 0;
}

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

static void init_sinc_table(rarch_resampler_t *resamp)
{
   // Sinc phases: [..., p + 3, p + 2, p + 1, p + 0, p - 1, p - 2, p - 3, p - 4, ...]
   for (int i = 0; i < PHASES; i++)
   {
      for (int j = 0; j < TAPS; j++)
      {
         double p = (double)i / PHASES;
         double sinc_phase = M_PI * (p + (SIDELOBES - 1 - j));
         resamp->phase_table[i][PHASE_INDEX][j] = CUTOFF * sinc(CUTOFF * sinc_phase) *
            lanzcos(sinc_phase / SIDELOBES);
      }
   }

   // Optimize linear interpolation.
   for (int i = 0; i < PHASES - 1; i++)
   {
      for (int j = 0; j < TAPS; j++)
      {
         resamp->phase_table[i][DELTA_INDEX][j] =
            (resamp->phase_table[i + 1][PHASE_INDEX][j] - resamp->phase_table[i][PHASE_INDEX][j]) / SUBPHASES;
      }
   }

   // Interpolation between [PHASES - 1] => [PHASES] 
   for (int j = 0; j < TAPS; j++)
   {
      double p = 1.0;
      double sinc_phase = M_PI * (p + (SIDELOBES - 1 - j));
      double phase = CUTOFF * sinc(CUTOFF * sinc_phase) * lanzcos(sinc_phase / SIDELOBES);

      float result = (phase - resamp->phase_table[PHASES - 1][PHASE_INDEX][j]) / SUBPHASES;
      resamp->phase_table[PHASES - 1][DELTA_INDEX][j] = result;
   }
}

// No memalign() for us on Win32 ...
static void *aligned_alloc(size_t boundary, size_t size)
{
   void *ptr = malloc(boundary + size + sizeof(uintptr_t));
   if (!ptr)
      return NULL;

   uintptr_t addr = ((uintptr_t)ptr + sizeof(uintptr_t) + boundary) & ~(boundary - 1);
   void **place = (void**)addr;
   place[-1] = ptr;

   return (void*)addr;
}

static void aligned_free(void *ptr)
{
   void **p = (void**)ptr;
   free(p[-1]);
}

rarch_resampler_t *resampler_new(void)
{
   rarch_resampler_t *re = (rarch_resampler_t*)aligned_alloc(16, sizeof(*re));
   if (!re)
      return NULL;

   memset(re, 0, sizeof(*re));

   init_sinc_table(re);

#if __SSE__
   RARCH_LOG("Sinc resampler [SSE]\n");
#else
   RARCH_LOG("Sinc resampler [C]\n");
#endif

   return re;
}

#if __SSE__
static void process_sinc(rarch_resampler_t *resamp, float *out_buffer)
{
   __m128 sum_l = _mm_setzero_ps();
   __m128 sum_r = _mm_setzero_ps();

   const float *buffer_l = resamp->buffer_l + resamp->ptr;
   const float *buffer_r = resamp->buffer_r + resamp->ptr;

   unsigned phase = resamp->time >> PHASES_SHIFT;
   unsigned delta = (resamp->time >> SUBPHASES_SHIFT) & SUBPHASES_MASK;
   __m128 delta_f = _mm_set1_ps(delta);

   const float *phase_table = resamp->phase_table[phase][PHASE_INDEX];
   const float *delta_table = resamp->phase_table[phase][DELTA_INDEX];

   for (unsigned i = 0; i < TAPS; i += 4)
   {
      __m128 buf_l  = _mm_loadu_ps(buffer_l + i);
      __m128 buf_r  = _mm_loadu_ps(buffer_r + i);

      __m128 phases = _mm_load_ps(phase_table + i);
      __m128 deltas = _mm_load_ps(delta_table + i);

      __m128 sinc   = _mm_add_ps(phases, _mm_mul_ps(deltas, delta_f));

      sum_l         = _mm_add_ps(sum_l, _mm_mul_ps(buf_l, sinc));
      sum_r         = _mm_add_ps(sum_r, _mm_mul_ps(buf_r, sinc));
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
#else // Plain ol' C99
static void process_sinc(rarch_resampler_t *resamp, float *out_buffer)
{
   float sum_l = 0.0f;
   float sum_r = 0.0f;
   const float *buffer_l = resamp->buffer_l + resamp->ptr;
   const float *buffer_r = resamp->buffer_r + resamp->ptr;

   unsigned phase = resamp->time >> PHASES_SHIFT;
   unsigned delta = (resamp->time >> SUBPHASES_SHIFT) & SUBPHASES_MASK;
   float delta_f = (float)delta;

   const float *phase_table = resamp->phase_table[phase][PHASE_INDEX];
   const float *delta_table = resamp->phase_table[phase][DELTA_INDEX];

   for (unsigned i = 0; i < TAPS; i++)
   {
      float sinc_val = phase_table[i] + delta_f * delta_table[i];
      sum_l         += buffer_l[i] * sinc_val;
      sum_r         += buffer_r[i] * sinc_val;
   }

   out_buffer[0] = sum_l;
   out_buffer[1] = sum_r;
}
#endif

void resampler_process(rarch_resampler_t *re, struct resampler_data *data)
{
   uint32_t ratio = PHASES_WRAP / data->ratio;

   const float *input = data->data_in;
   float *output = data->data_out;
   size_t frames = data->input_frames;
   size_t out_frames = 0;

   while (frames)
   {
      process_sinc(re, output);
      output += 2;
      out_frames++;

      re->time += ratio;
      while (re->time >= PHASES_WRAP)
      {
         re->buffer_l[re->ptr + TAPS] = re->buffer_l[re->ptr] = *input++;
         re->buffer_r[re->ptr + TAPS] = re->buffer_r[re->ptr] = *input++;
         re->ptr = (re->ptr + 1) & (TAPS - 1);

         re->time -= PHASES_WRAP;
         frames--;
      }
   }

   data->output_frames = out_frames;
}

void resampler_free(rarch_resampler_t *re)
{
   aligned_free(re);
}

