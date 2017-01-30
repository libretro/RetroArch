/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (sinc_resampler_common.h).
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

#ifndef _LIBRETRO_SDK_SINC_RESAMPLER_COMMON_H
#define _LIBRETRO_SDK_SINC_RESAMPLER_COMMON_H

#include <retro_common_api.h>
#include <filters.h>

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#if defined(__AVX__) && ENABLE_AVX
#include <immintrin.h>
#endif

RETRO_BEGIN_DECLS

typedef struct rarch_sinc_resampler
{
   float *phase_table;
   float *buffer_l;
   float *buffer_r;

   unsigned taps;

   unsigned ptr;
   uint32_t time;

   /* A buffer for phase_table, buffer_l and buffer_r 
    * are created in a single calloc().
    * Ensure that we get as good cache locality as we can hope for. */
   float *main_buffer;
} rarch_sinc_resampler_t;

/* Rough SNR values for upsampling:
 * LOWEST: 40 dB
 * LOWER: 55 dB
 * NORMAL: 70 dB
 * HIGHER: 110 dB
 * HIGHEST: 140 dB
 */

/* TODO, make all this more configurable. */
#if defined(SINC_LOWEST_QUALITY)
#define SINC_WINDOW_LANCZOS
#define CUTOFF 0.98
#define PHASE_BITS 12
#define SINC_COEFF_LERP 0
#define SUBPHASE_BITS 10
#define SIDELOBES 2
#define ENABLE_AVX 0
#elif defined(SINC_LOWER_QUALITY)
#define SINC_WINDOW_LANCZOS
#define CUTOFF 0.98
#define PHASE_BITS 12
#define SUBPHASE_BITS 10
#define SINC_COEFF_LERP 0
#define SIDELOBES 4
#define ENABLE_AVX 0
#elif defined(SINC_HIGHER_QUALITY)
#define SINC_WINDOW_KAISER
#define SINC_WINDOW_KAISER_BETA 10.5
#define CUTOFF 0.90
#define PHASE_BITS 10
#define SUBPHASE_BITS 14
#define SINC_COEFF_LERP 1
#define SIDELOBES 32
#define ENABLE_AVX 1
#elif defined(SINC_HIGHEST_QUALITY)
#define SINC_WINDOW_KAISER
#define SINC_WINDOW_KAISER_BETA 14.5
#define CUTOFF 0.962
#define PHASE_BITS 10
#define SUBPHASE_BITS 14
#define SINC_COEFF_LERP 1
#define SIDELOBES 128
#define ENABLE_AVX 1
#else
#define SINC_WINDOW_KAISER
#define SINC_WINDOW_KAISER_BETA 5.5
#define CUTOFF 0.825
#define PHASE_BITS 8
#define SUBPHASE_BITS 16
#define SINC_COEFF_LERP 1
#define SIDELOBES 8
#define ENABLE_AVX 0
#endif

#if defined(SINC_WINDOW_LANCZOS)
#define window_function(idx)  (lanzcos_window_function(idx))
#elif defined(SINC_WINDOW_KAISER)
#define window_function(idx)  (kaiser_window_function(idx, SINC_WINDOW_KAISER_BETA))
#else
#error "No SINC window function defined."
#endif

/* For the little amount of taps we're using,
 * SSE1 is faster than AVX for some reason.
 * AVX code is kept here though as by increasing number
 * of sinc taps, the AVX code is clearly faster than SSE1.
 */

#define PHASES (1 << (PHASE_BITS + SUBPHASE_BITS))

#define TAPS (SIDELOBES * 2)
#define SUBPHASE_MASK ((1 << SUBPHASE_BITS) - 1)
#define SUBPHASE_MOD (1.0f / (1 << SUBPHASE_BITS))

#if !(defined(__AVX__) && ENABLE_AVX) && !defined(__SSE__)
static INLINE void process_sinc_C(rarch_sinc_resampler_t *resamp,
      float *out_buffer)
{
   unsigned i;
   float sum_l              = 0.0f;
   float sum_r              = 0.0f;
   const float *buffer_l    = resamp->buffer_l + resamp->ptr;
   const float *buffer_r    = resamp->buffer_r + resamp->ptr;
   unsigned taps            = resamp->taps;
   unsigned phase           = resamp->time >> SUBPHASE_BITS;
#if SINC_COEFF_LERP
   const float *phase_table = resamp->phase_table + phase * taps * 2;
   const float *delta_table = phase_table + taps;
   float delta              = (float)
      (resamp->time & SUBPHASE_MASK) * SUBPHASE_MOD;
#else
   const float *phase_table = resamp->phase_table + phase * taps;
#endif

   for (i = 0; i < taps; i++)
   {
#if SINC_COEFF_LERP
      float sinc_val = phase_table[i] + delta_table[i] * delta;
#else
      float sinc_val = phase_table[i];
#endif
      sum_l         += buffer_l[i] * sinc_val;
      sum_r         += buffer_r[i] * sinc_val;
   }

   out_buffer[0] = sum_l;
   out_buffer[1] = sum_r;
}
#endif

#if defined(__AVX__) && ENABLE_AVX
#define process_sinc_func process_sinc
static INLINE void process_sinc(rarch_sinc_resampler_t *resamp, float *out_buffer)
{
   unsigned i;
   __m256 sum_l             = _mm256_setzero_ps();
   __m256 sum_r             = _mm256_setzero_ps();

   const float *buffer_l    = resamp->buffer_l + resamp->ptr;
   const float *buffer_r    = resamp->buffer_r + resamp->ptr;

   unsigned taps            = resamp->taps;
   unsigned phase           = resamp->time >> SUBPHASE_BITS;
#if SINC_COEFF_LERP
   const float *phase_table = resamp->phase_table + phase * taps * 2;
   const float *delta_table = phase_table + taps;
   __m256 delta             = _mm256_set1_ps((float)
         (resamp->time & SUBPHASE_MASK) * SUBPHASE_MOD);
#else
   const float *phase_table = resamp->phase_table + phase * taps;
#endif

   for (i = 0; i < taps; i += 8)
   {
      __m256 buf_l  = _mm256_loadu_ps(buffer_l + i);
      __m256 buf_r  = _mm256_loadu_ps(buffer_r + i);

#if SINC_COEFF_LERP
      __m256 deltas = _mm256_load_ps(delta_table + i);
      __m256 sinc   = _mm256_add_ps(_mm256_load_ps(phase_table + i),
            _mm256_mul_ps(deltas, delta));
#else
      __m256 sinc   = _mm256_load_ps(phase_table + i);
#endif
      sum_l         = _mm256_add_ps(sum_l, _mm256_mul_ps(buf_l, sinc));
      sum_r         = _mm256_add_ps(sum_r, _mm256_mul_ps(buf_r, sinc));
   }

   /* hadd on AVX is weird, and acts on low-lanes 
    * and high-lanes separately. */
   __m256 res_l = _mm256_hadd_ps(sum_l, sum_l);
   __m256 res_r = _mm256_hadd_ps(sum_r, sum_r);
   res_l        = _mm256_hadd_ps(res_l, res_l);
   res_r        = _mm256_hadd_ps(res_r, res_r);
   res_l        = _mm256_add_ps(_mm256_permute2f128_ps(res_l, res_l, 1), res_l);
   res_r        = _mm256_add_ps(_mm256_permute2f128_ps(res_r, res_r, 1), res_r);

   /* This is optimized to mov %xmmN, [mem].
    * There doesn't seem to be any _mm256_store_ss intrinsic. */
   _mm_store_ss(out_buffer + 0, _mm256_extractf128_ps(res_l, 0));
   _mm_store_ss(out_buffer + 1, _mm256_extractf128_ps(res_r, 0));
}
#elif defined(__SSE__)
#define process_sinc_func process_sinc
static INLINE void process_sinc(rarch_sinc_resampler_t *resamp, float *out_buffer)
{
   unsigned i;
   __m128 sum;
   __m128 sum_l             = _mm_setzero_ps();
   __m128 sum_r             = _mm_setzero_ps();

   const float *buffer_l    = resamp->buffer_l + resamp->ptr;
   const float *buffer_r    = resamp->buffer_r + resamp->ptr;

   unsigned taps            = resamp->taps;
   unsigned phase           = resamp->time >> SUBPHASE_BITS;
#if SINC_COEFF_LERP
   const float *phase_table = resamp->phase_table + phase * taps * 2;
   const float *delta_table = phase_table + taps;
   __m128 delta             = _mm_set1_ps((float)
         (resamp->time & SUBPHASE_MASK) * SUBPHASE_MOD);
#else
   const float *phase_table = resamp->phase_table + phase * taps;
#endif

   for (i = 0; i < taps; i += 4)
   {
      __m128 buf_l = _mm_loadu_ps(buffer_l + i);
      __m128 buf_r = _mm_loadu_ps(buffer_r + i);

#if SINC_COEFF_LERP
      __m128 deltas = _mm_load_ps(delta_table + i);
      __m128 _sinc  = _mm_add_ps(_mm_load_ps(phase_table + i),
            _mm_mul_ps(deltas, delta));
#else
      __m128 _sinc = _mm_load_ps(phase_table + i);
#endif
      sum_l        = _mm_add_ps(sum_l, _mm_mul_ps(buf_l, _sinc));
      sum_r        = _mm_add_ps(sum_r, _mm_mul_ps(buf_r, _sinc));
   }

   /* Them annoying shuffles.
    * sum_l = { l3, l2, l1, l0 }
    * sum_r = { r3, r2, r1, r0 }
    */

   sum = _mm_add_ps(_mm_shuffle_ps(sum_l, sum_r,
            _MM_SHUFFLE(1, 0, 1, 0)),
         _mm_shuffle_ps(sum_l, sum_r, _MM_SHUFFLE(3, 2, 3, 2)));

   /* sum   = { r1, r0, l1, l0 } + { r3, r2, l3, l2 }
    * sum   = { R1, R0, L1, L0 }
    */

   sum = _mm_add_ps(_mm_shuffle_ps(sum, sum, _MM_SHUFFLE(3, 3, 1, 1)), sum);

   /* sum   = {R1, R1, L1, L1 } + { R1, R0, L1, L0 }
    * sum   = { X,  R,  X,  L } 
    */

   /* Store L */
   _mm_store_ss(out_buffer + 0, sum);

   /* movehl { X, R, X, L } == { X, R, X, R } */
   _mm_store_ss(out_buffer + 1, _mm_movehl_ps(sum, sum));
}
#elif defined(__ARM_NEON__)

#if SINC_COEFF_LERP
#error "NEON asm does not support SINC lerp."
#endif

/* Need to make this function pointer as Android doesn't 
 * have built-in targets for NEON and plain ARMv7a.
 */
static void (*process_sinc_func)(rarch_sinc_resampler_t *resamp,
      float *out_buffer);

/* Assumes that taps >= 8, and that taps is a multiple of 8. */
void process_sinc_neon_asm(float *out, const float *left, 
      const float *right, const float *coeff, unsigned taps);

static INLINE void process_sinc_neon(rarch_sinc_resampler_t *resamp,
      float *out_buffer)
{
   const float *buffer_l    = resamp->buffer_l + resamp->ptr;
   const float *buffer_r    = resamp->buffer_r + resamp->ptr;

   unsigned phase           = resamp->time >> SUBPHASE_BITS;
   unsigned taps            = resamp->taps;
   const float *phase_table = resamp->phase_table + phase * taps;

   process_sinc_neon_asm(out_buffer, buffer_l, buffer_r, phase_table, taps);
}
#else /* Plain ol' C99 */
#define process_sinc_func process_sinc_C
#endif


void sinc_init_table(rarch_sinc_resampler_t *resamp, double cutoff,
      float *phase_table, int phases, int taps, bool calculate_delta);

void sinc_free(void *data);

RETRO_END_DECLS

#endif
