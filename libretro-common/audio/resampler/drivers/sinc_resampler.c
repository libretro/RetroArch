/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (sinc_resampler.c).
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

/* Bog-standard windowed SINC implementation. */

#if defined(__GNUC__) && defined(__OPTIMIZE__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("fast-math")
#endif

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <retro_environment.h>
#include <retro_inline.h>
#include <retro_atomic.h>
#include <filters.h>
#include <memalign.h>

#include <audio/audio_resampler.h>
#include "sinc_resampler_internal.h"

#include <audio/sinc_resampler.h>

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#if defined(__AVX__)
#include <immintrin.h>
#endif

/* Rough SNR values for upsampling:
 * LOWEST: 40 dB
 * LOWER: 55 dB
 * NORMAL: 70 dB
 * HIGHER: 110 dB
 * HIGHEST: 140 dB
 */

/* TODO, make all this more configurable. */

enum sinc_window
{
   SINC_WINDOW_NONE   = 0,
   SINC_WINDOW_KAISER,
   SINC_WINDOW_LANCZOS
};

/* Decimation by two ahead of the kernel when fast-forward takes the
 * live ratio below what the table rejects images for. Each stage is the
 * preset's filter as its table would be built for a ratio of
 * SINC_DEC_ENGAGE / 2, at the one phase decimation needs, so behind a
 * stage the kernel may run down to SINC_DEC_ENGAGE of its design ratio.
 * The first stage joins below SINC_DEC_ENGAGE_FIRST, under every
 * preset's passband edge and over rate control's reach, and leaves
 * inside rate control's band; deeper ones leave well clear of where they
 * join, which keeps the powers of two mid-band against speed jitter. */
#define SINC_DEC_STAGES       4
#define SINC_DEC_BLOCK        256
#define SINC_DEC_ENGAGE_FIRST 0.97
#define SINC_DEC_ENGAGE       0.9
#define SINC_DEC_RELEASE_LAST 0.975
#define SINC_DEC_RELEASE      1.25
/* A change of stages renders the old path alongside until the new one
 * has filled its rings, then crossfades; this holds the old output. */
#define SINC_DEC_FADE_FRAMES  256
#define SINC_DEC_HALF         (2 * (SINC_DEC_BLOCK / 2 + 1))

/* For the little amount of taps we're using,
 * SSE1 is faster than AVX for some reason.
 * AVX code is kept here though as by increasing number
 * of sinc taps, the AVX code is clearly faster than SSE1.
 */

/* Leads the block that holds the immutable tables, which every sibling
 * of an instance reads; the last one out frees it. */
typedef struct sinc_tables
{
   retro_atomic_int_t refs;
} sinc_tables_t;

/* In floats: keeps the tables on the block's 128-byte alignment. */
#define SINC_TABLES_OFFSET 32

typedef struct rarch_sinc_resampler
{
   double design_ratio;
   /* The stream state - rings, the stages' rings, the shadow and the
    * blocks - in one block, and the shared tables in another. */
   float *main_buffer;
   sinc_tables_t *tables;
   float *phase_table;
   float *buffer_l;
   float *buffer_r;
   /* SINC_DEC_STAGES one-phase instances sharing one table, then the
    * kernel's shadow for a crossfade; two blocks stages hand through. */
   struct rarch_sinc_resampler *dec;
   float *dec_block;
   float *fade_block;
   unsigned dec_stages;
   unsigned fade_stages;
   unsigned fade_left;
   unsigned fade_len;
   unsigned phase_bits;
   unsigned subphase_bits;
   unsigned subphase_mask;
   unsigned taps;
   unsigned ptr;
   uint32_t time;
   float subphase_mod;
   float kaiser_beta;
   /* Per-instance kernel. The global sinc_resampler vtable is shared by
    * every instance (game audio, microphone, mixer voices), so the kernel
    * choice must live here: window types lay out phase_table differently
    * (Kaiser interleaves coeff+delta rows at stride taps*2, Lanczos rows
    * are stride taps), and letting a later init of one window type rewrite
    * a shared function pointer makes an earlier instance of the other type
    * read its table at the wrong stride - deltas get used as coefficients,
    * collapsing output level on a per-phase basis. */
   void (*process)(void *re, struct resampler_data *data);
} rarch_sinc_resampler_t;

typedef void (*sinc_process_t)(void *re, struct resampler_data *data);

/*
 * Macro to handle the common "push input samples into ring buffer"
 * logic shared by every process function. Reduces code duplication and
 * ensures the input-feeding logic stays consistent across all paths.
 */
#define SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames) \
   do {                                                              \
      while ((frames) && (resamp)->time >= (phases))                 \
      {                                                              \
         if (!(resamp)->ptr)                                         \
            (resamp)->ptr = (taps);                                  \
         (resamp)->ptr--;                                            \
         (resamp)->buffer_l[(resamp)->ptr + (taps)] =                \
            (resamp)->buffer_l[(resamp)->ptr]       = *(input)++;    \
         (resamp)->buffer_r[(resamp)->ptr + (taps)] =                \
            (resamp)->buffer_r[(resamp)->ptr]       = *(input)++;    \
         (resamp)->time -= (phases);                                 \
         (frames)--;                                                 \
      }                                                              \
   } while (0)


#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))

#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
void process_sinc_neon_asm(float *out, const float *left,
      const float *right, const float *coeff, unsigned taps);
void process_sinc_neon_kaiser_asm(float *out, const float *left,
      const float *right, const float *coeff, unsigned taps,
      const float *frac);
#endif
#include <arm_neon.h>

/* Assumes that taps >= 8, and that taps is a multiple of 8. */
static void resampler_sinc_process_neon_kaiser(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases                = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio                 = phases / data->ratio;
   const float *input             = data->data_in;
   float *output                  = data->data_out;
   size_t frames                  = data->input_frames;
   size_t out_frames              = 0;
   unsigned taps                  = resamp->taps;
   unsigned taps2                 = taps * 2;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l    = resamp->buffer_l + resamp->ptr;
         const float *buffer_r    = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            unsigned phase           = resamp->time >> resamp->subphase_bits;
            const float *phase_table = resamp->phase_table + phase * taps2;
#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
            float frac               = (resamp->time & resamp->subphase_mask)
               * resamp->subphase_mod;
            process_sinc_neon_kaiser_asm(output, buffer_l, buffer_r,
                  phase_table, taps, &frac);
#else
            /* C89: all declarations at top of block */
            int i;
            float32x2_t p3, p4;
            const float *delta_table = phase_table + taps;
            float32x4_t delta        = vdupq_n_f32((resamp->time & resamp->subphase_mask) * resamp->subphase_mod);
            float32x4_t p1           = vdupq_n_f32(0.0f);
            float32x4_t p2           = vdupq_n_f32(0.0f);

            for (i = 0; i < (int)taps; i += 8)
            {
               float32x4x2_t coeff8  = vld2q_f32(&phase_table[i]);
               float32x4x2_t delta8  = vld2q_f32(&delta_table[i]);
               float32x4x2_t left8   = vld2q_f32(&buffer_l[i]);
               float32x4x2_t right8  = vld2q_f32(&buffer_r[i]);

               coeff8.val[0] = vmlaq_f32(coeff8.val[0], delta8.val[0], delta);
               coeff8.val[1] = vmlaq_f32(coeff8.val[1], delta8.val[1], delta);

               p1 = vmlaq_f32(p1,  left8.val[0], coeff8.val[0]);
               p2 = vmlaq_f32(p2, right8.val[0], coeff8.val[0]);
               p1 = vmlaq_f32(p1,  left8.val[1], coeff8.val[1]);
               p2 = vmlaq_f32(p2, right8.val[1], coeff8.val[1]);
            }

            p3 = vadd_f32(vget_low_f32(p1), vget_high_f32(p1));
            p4 = vadd_f32(vget_low_f32(p2), vget_high_f32(p2));
            vst1_f32(output, vpadd_f32(p3, p4));
#endif
            output                 += 2;
            out_frames++;
            resamp->time           += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}

/* Assumes that taps >= 8, and that taps is a multiple of 8. */
static void resampler_sinc_process_neon(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases                = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio                 = phases / data->ratio;
   const float *input             = data->data_in;
   float *output                  = data->data_out;
   size_t frames                  = data->input_frames;
   size_t out_frames              = 0;
   unsigned taps                  = resamp->taps;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l    = resamp->buffer_l + resamp->ptr;
         const float *buffer_r    = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            unsigned phase           = resamp->time >> resamp->subphase_bits;
            const float *phase_table = resamp->phase_table + phase * taps;
#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
            process_sinc_neon_asm(output, buffer_l, buffer_r, phase_table, taps);
#else
            {
               /* C89: declarations at top of block */
               int i;
               float32x2_t p3, p4;
               float32x4_t p1 = vdupq_n_f32(0.0f);
               float32x4_t p2 = vdupq_n_f32(0.0f);

               for (i = 0; i < (int)taps; i += 8)
               {
                  float32x4x2_t coeff8  = vld2q_f32(&phase_table[i]);
                  float32x4x2_t left8   = vld2q_f32(&buffer_l[i]);
                  float32x4x2_t right8  = vld2q_f32(&buffer_r[i]);

                  p1 = vmlaq_f32(p1,  left8.val[0], coeff8.val[0]);
                  p2 = vmlaq_f32(p2, right8.val[0], coeff8.val[0]);
                  p1 = vmlaq_f32(p1,  left8.val[1], coeff8.val[1]);
                  p2 = vmlaq_f32(p2, right8.val[1], coeff8.val[1]);
               }

               p3 = vadd_f32(vget_low_f32(p1), vget_high_f32(p1));
               p4 = vadd_f32(vget_low_f32(p2), vget_high_f32(p2));
               vst1_f32(output, vpadd_f32(p3, p4));
            }
#endif
            output                 += 2;
            out_frames++;
            resamp->time           += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}
#endif

#if defined(__AVX__)
static void resampler_sinc_process_avx_kaiser(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases                = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio                 = phases / data->ratio;
   const float *input             = data->data_in;
   float *output                  = data->data_out;
   size_t frames                  = data->input_frames;
   size_t out_frames              = 0;
   unsigned taps                  = resamp->taps;
   unsigned taps2                 = taps * 2;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l    = resamp->buffer_l + resamp->ptr;
         const float *buffer_r    = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            /* C89: all declarations at top of block */
            int i;
            __m256 res_l, res_r;
            unsigned phase     = resamp->time >> resamp->subphase_bits;
            float *phase_table = resamp->phase_table + phase * taps2;
            float *delta_table = phase_table + taps;
            __m256 delta       = _mm256_set1_ps((float)
                  (resamp->time & resamp->subphase_mask) * resamp->subphase_mod);
            __m256 sum_l       = _mm256_setzero_ps();
            __m256 sum_r       = _mm256_setzero_ps();

            for (i = 0; i < (int)taps; i += 8)
            {
               __m256 buf_l  = _mm256_loadu_ps(buffer_l + i);
               __m256 buf_r  = _mm256_loadu_ps(buffer_r + i);
               __m256 deltas = _mm256_load_ps(delta_table + i);
               __m256 sinc_v = _mm256_add_ps(_mm256_load_ps((const float*)phase_table + i),
                     _mm256_mul_ps(deltas, delta));

               sum_l         = _mm256_add_ps(sum_l, _mm256_mul_ps(buf_l, sinc_v));
               sum_r         = _mm256_add_ps(sum_r, _mm256_mul_ps(buf_r, sinc_v));
            }

            /* hadd on AVX is weird, and acts on low-lanes
             * and high-lanes separately. */
            res_l = _mm256_hadd_ps(sum_l, sum_l);
            res_r = _mm256_hadd_ps(sum_r, sum_r);
            res_l = _mm256_hadd_ps(res_l, res_l);
            res_r = _mm256_hadd_ps(res_r, res_r);
            res_l = _mm256_add_ps(_mm256_permute2f128_ps(res_l, res_l, 1), res_l);
            res_r = _mm256_add_ps(_mm256_permute2f128_ps(res_r, res_r, 1), res_r);

            /* This is optimized to mov %xmmN, [mem].
             * There doesn't seem to be any _mm256_store_ss intrinsic. */
            _mm_store_ss(output + 0, _mm256_extractf128_ps(res_l, 0));
            _mm_store_ss(output + 1, _mm256_extractf128_ps(res_r, 0));

            output += 2;
            out_frames++;
            resamp->time += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}

static void resampler_sinc_process_avx(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases    = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio     = phases / data->ratio;
   const float *input = data->data_in;
   float *output      = data->data_out;
   size_t frames      = data->input_frames;
   size_t out_frames  = 0;
   unsigned taps      = resamp->taps;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l    = resamp->buffer_l + resamp->ptr;
         const float *buffer_r    = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            /* C89: all declarations at top of block */
            int i;
            __m256 res_l, res_r;
            unsigned phase     = resamp->time >> resamp->subphase_bits;
            float *phase_table = resamp->phase_table + phase * taps;
            __m256 sum_l       = _mm256_setzero_ps();
            __m256 sum_r       = _mm256_setzero_ps();

            for (i = 0; i < (int)taps; i += 8)
            {
               __m256 buf_l  = _mm256_loadu_ps(buffer_l + i);
               __m256 buf_r  = _mm256_loadu_ps(buffer_r + i);
               __m256 sinc_v = _mm256_load_ps((const float*)phase_table + i);

               sum_l         = _mm256_add_ps(sum_l, _mm256_mul_ps(buf_l, sinc_v));
               sum_r         = _mm256_add_ps(sum_r, _mm256_mul_ps(buf_r, sinc_v));
            }

            /* hadd on AVX is weird, and acts on low-lanes
             * and high-lanes separately. */
            res_l = _mm256_hadd_ps(sum_l, sum_l);
            res_r = _mm256_hadd_ps(sum_r, sum_r);
            res_l = _mm256_hadd_ps(res_l, res_l);
            res_r = _mm256_hadd_ps(res_r, res_r);
            res_l = _mm256_add_ps(_mm256_permute2f128_ps(res_l, res_l, 1), res_l);
            res_r = _mm256_add_ps(_mm256_permute2f128_ps(res_r, res_r, 1), res_r);

            /* This is optimized to mov %xmmN, [mem].
             * There doesn't seem to be any _mm256_store_ss intrinsic. */
            _mm_store_ss(output + 0, _mm256_extractf128_ps(res_l, 0));
            _mm_store_ss(output + 1, _mm256_extractf128_ps(res_r, 0));

            output += 2;
            out_frames++;
            resamp->time += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}
#endif

#if defined(__SSE__)
static void resampler_sinc_process_sse_kaiser(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases                = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio                 = phases / data->ratio;
   const float *input             = data->data_in;
   float *output                  = data->data_out;
   size_t frames                  = data->input_frames;
   size_t out_frames              = 0;
   unsigned taps                  = resamp->taps;
   unsigned taps2                 = taps * 2;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l    = resamp->buffer_l + resamp->ptr;
         const float *buffer_r    = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            /* C89: all declarations at top of block */
            int i;
            __m128 sum;
            unsigned phase     = resamp->time >> resamp->subphase_bits;
            float *phase_table = resamp->phase_table + phase * taps2;
            float *delta_table = phase_table + taps;
            __m128 delta       = _mm_set1_ps((float)
                  (resamp->time & resamp->subphase_mask) * resamp->subphase_mod);
            __m128 sum_l       = _mm_setzero_ps();
            __m128 sum_r       = _mm_setzero_ps();

            for (i = 0; i < (int)taps; i += 4)
            {
               __m128 buf_l  = _mm_loadu_ps(buffer_l + i);
               __m128 buf_r  = _mm_loadu_ps(buffer_r + i);
               __m128 deltas = _mm_load_ps(delta_table + i);
               __m128 sinc_v = _mm_add_ps(_mm_load_ps((const float*)phase_table + i),
                     _mm_mul_ps(deltas, delta));
               sum_l        = _mm_add_ps(sum_l, _mm_mul_ps(buf_l, sinc_v));
               sum_r        = _mm_add_ps(sum_r, _mm_mul_ps(buf_r, sinc_v));
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
            _mm_store_ss(output + 0, sum);

            /* movehl { X, R, X, L } == { X, R, X, R } */
            _mm_store_ss(output + 1, _mm_movehl_ps(sum, sum));

            output += 2;
            out_frames++;
            resamp->time += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}

static void resampler_sinc_process_sse(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases                = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio                 = phases / data->ratio;
   const float *input             = data->data_in;
   float *output                  = data->data_out;
   size_t frames                  = data->input_frames;
   size_t out_frames              = 0;
   unsigned taps                  = resamp->taps;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l    = resamp->buffer_l + resamp->ptr;
         const float *buffer_r    = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            /* C89: all declarations at top of block */
            int i;
            __m128 sum;
            unsigned phase     = resamp->time >> resamp->subphase_bits;
            float *phase_table = resamp->phase_table + phase * taps;
            __m128 sum_l       = _mm_setzero_ps();
            __m128 sum_r       = _mm_setzero_ps();

            for (i = 0; i < (int)taps; i += 4)
            {
               __m128 buf_l  = _mm_loadu_ps(buffer_l + i);
               __m128 buf_r  = _mm_loadu_ps(buffer_r + i);
               __m128 sinc_v = _mm_load_ps((const float*)phase_table + i);
               sum_l        = _mm_add_ps(sum_l, _mm_mul_ps(buf_l, sinc_v));
               sum_r        = _mm_add_ps(sum_r, _mm_mul_ps(buf_r, sinc_v));
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
            _mm_store_ss(output + 0, sum);

            /* movehl { X, R, X, L } == { X, R, X, R } */
            _mm_store_ss(output + 1, _mm_movehl_ps(sum, sum));

            output += 2;
            out_frames++;
            resamp->time += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}
#endif

static void resampler_sinc_process_c_kaiser(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases                = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio                 = phases / data->ratio;
   const float *input             = data->data_in;
   float *output                  = data->data_out;
   size_t frames                  = data->input_frames;
   size_t out_frames              = 0;
   unsigned taps                  = resamp->taps;
   unsigned taps2                 = taps * 2;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l = resamp->buffer_l + resamp->ptr;
         const float *buffer_r = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            /* C89: all declarations at top of block */
            int i;
            float sum_l        = 0.0f;
            float sum_r        = 0.0f;
            unsigned phase     = resamp->time >> resamp->subphase_bits;
            float *phase_table = resamp->phase_table + phase * taps2;
            float *delta_table = phase_table + taps;
            float delta        = (float)
               (resamp->time & resamp->subphase_mask) * resamp->subphase_mod;

            /* Manual 4x unroll for scalar path.
             * Taps is guaranteed to be a multiple of 4 
               (see SIMD alignment in init). */
            int taps_aligned   = (int)taps & ~3;
            for (i = 0; i < taps_aligned; i += 4)
            {
               float s0 = phase_table[i]     + delta_table[i]     * delta;
               float s1 = phase_table[i + 1] + delta_table[i + 1] * delta;
               float s2 = phase_table[i + 2] + delta_table[i + 2] * delta;
               float s3 = phase_table[i + 3] + delta_table[i + 3] * delta;

               sum_l += buffer_l[i]     * s0 + buffer_l[i + 1] * s1
                      + buffer_l[i + 2] * s2 + buffer_l[i + 3] * s3;
               sum_r += buffer_r[i]     * s0 + buffer_r[i + 1] * s1
                      + buffer_r[i + 2] * s2 + buffer_r[i + 3] * s3;
            }
            /* Handle any remaining taps (safety net). */
            for (; i < (int)taps; i++)
            {
               float sinc_val  = phase_table[i] + delta_table[i] * delta;
               sum_l          += buffer_l[i] * sinc_val;
               sum_r          += buffer_r[i] * sinc_val;
            }

            output[0]          = sum_l;
            output[1]          = sum_r;

            output            += 2;
            out_frames++;
            resamp->time      += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}

static void resampler_sinc_process_c(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)re_;
   unsigned phases                = 1 << (resamp->phase_bits + resamp->subphase_bits);
   uint32_t ratio                 = phases / data->ratio;
   const float *input             = data->data_in;
   float *output                  = data->data_out;
   size_t frames                  = data->input_frames;
   size_t out_frames              = 0;
   unsigned taps                  = resamp->taps;

   while (frames)
   {
      SINC_PUSH_INPUT_SAMPLES(resamp, input, taps, phases, frames);

      {
         const float *buffer_l    = resamp->buffer_l + resamp->ptr;
         const float *buffer_r    = resamp->buffer_r + resamp->ptr;
         while (resamp->time < phases)
         {
            /* C89: all declarations at top of block */
            int i;
            float sum_l        = 0.0f;
            float sum_r        = 0.0f;
            unsigned phase     = resamp->time >> resamp->subphase_bits;
            float *phase_table = resamp->phase_table + phase * taps;
            /* Manual 4x unroll for scalar path. */
            int taps_aligned   = (int)taps & ~3;
            for (i = 0; i < taps_aligned; i += 4)
            {
               float s0 = phase_table[i];
               float s1 = phase_table[i + 1];
               float s2 = phase_table[i + 2];
               float s3 = phase_table[i + 3];

               sum_l += buffer_l[i]     * s0 + buffer_l[i + 1] * s1
                      + buffer_l[i + 2] * s2 + buffer_l[i + 3] * s3;
               sum_r += buffer_r[i]     * s0 + buffer_r[i + 1] * s1
                      + buffer_r[i + 2] * s2 + buffer_r[i + 3] * s3;
            }
            /* Handle any remaining taps. */
            for (; i < (int)taps; i++)
            {
               float sinc_val     = phase_table[i];
               sum_l             += buffer_l[i] * sinc_val;
               sum_r             += buffer_r[i] * sinc_val;
            }

            output[0]             = sum_l;
            output[1]             = sum_r;

            output               += 2;
            out_frames++;
            resamp->time         += ratio;
         }
      }
   }

   data->output_frames = out_frames;
}

static void resampler_sinc_free(void *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)data;
   if (resamp)
   {
      memalign_free(resamp->main_buffer);
      free(resamp->dec);
      if (     resamp->tables
            && retro_atomic_fetch_sub_int(&resamp->tables->refs, 1) == 1)
         memalign_free(resamp->tables);
   }
   free(resamp);
}

static size_t sinc_state_elems(unsigned taps, unsigned dec_taps)
{
   return 8 * taps + 4 * dec_taps * SINC_DEC_STAGES
      + 2 * SINC_DEC_FADE_FRAMES + 2 * SINC_DEC_HALF;
}

/* Lays this instance's state out over main_buffer: the kernel's rings,
 * each stage's, the shadow's, then the blocks. */
static void sinc_bind_state(rarch_sinc_resampler_t *re)
{
   unsigned s;
   unsigned dec_taps = re->dec[0].taps;
   float *at         = re->main_buffer + 4 * re->taps;
   re->buffer_l      = re->main_buffer;
   re->buffer_r      = re->buffer_l + 2 * re->taps;
   for (s = 0; s < SINC_DEC_STAGES; s++)
   {
      re->dec[s].buffer_l = at;
      re->dec[s].buffer_r = at + 2 * dec_taps;
      at                 += 4 * dec_taps;
   }
   re->dec[SINC_DEC_STAGES]          = *re;
   re->dec[SINC_DEC_STAGES].dec      = NULL;
   re->dec[SINC_DEC_STAGES].buffer_l = at;
   re->dec[SINC_DEC_STAGES].buffer_r = at + 2 * re->taps;
   re->fade_block                    = at + 4 * re->taps;
   re->dec_block                     = re->fade_block
      + 2 * SINC_DEC_FADE_FRAMES;
}

/* A fresh stream over another instance's tables. */
static void *resampler_sinc_sibling(void *data)
{
   unsigned s;
   rarch_sinc_resampler_t *src = (rarch_sinc_resampler_t*)data;
   rarch_sinc_resampler_t *re;
   size_t elems;
   if (!src || !(re = (rarch_sinc_resampler_t*)calloc(1, sizeof(*re))))
      return NULL;
   *re             = *src;
   elems           = sinc_state_elems(src->taps, src->dec[0].taps);
   re->main_buffer = (float*)memalign_alloc(128, sizeof(float) * elems);
   re->dec         = (rarch_sinc_resampler_t*)
      calloc(SINC_DEC_STAGES + 1, sizeof(*re->dec));
   if (!re->main_buffer || !re->dec)
   {
      memalign_free(re->main_buffer);
      free(re->dec);
      free(re);
      return NULL;
   }
   memset(re->main_buffer, 0, sizeof(float) * elems);
   for (s = 0; s < SINC_DEC_STAGES; s++)
   {
      re->dec[s]      = src->dec[s];
      re->dec[s].ptr  = 0;
      re->dec[s].time = 0;
   }
   re->ptr        = 0;
   re->time       = 0;
   re->dec_stages = 0;
   re->fade_left  = 0;
   sinc_bind_state(re);
   retro_atomic_inc_int(&re->tables->refs);
   return re;
}

/* The rings, the ring pointer and the phase; the table stands. */
static void resampler_sinc_reset(void *data)
{
   rarch_sinc_resampler_t *resamp = (rarch_sinc_resampler_t*)data;
   if (!resamp)
      return;
   memset(resamp->buffer_l, 0, sizeof(float) * 2 * resamp->taps);
   memset(resamp->buffer_r, 0, sizeof(float) * 2 * resamp->taps);
   resamp->ptr        = 0;
   resamp->time       = 0;
   resamp->dec_stages = 0;
   resamp->fade_left  = 0;
}

/* Clean rings, and a clock that takes input before it emits: a joining
 * path then produces no output ahead of the frames it was given. */
static void sinc_dec_restart(rarch_sinc_resampler_t *stage)
{
   memset(stage->buffer_l, 0, sizeof(float) * 4 * stage->taps);
   stage->ptr  = 0;
   stage->time = 1u << (stage->phase_bits + stage->subphase_bits);
}

/* Runs stages [first, last) from in; returns where the output is. */
static const float *sinc_dec_chain(rarch_sinc_resampler_t *re,
      unsigned first, unsigned last, const float *in, size_t *frames)
{
   struct resampler_data chunk;
   float *out   = (in == re->dec_block) ? re->dec_block + SINC_DEC_HALF
                                        : re->dec_block;
   chunk.ratio  = 0.5;
   for (; first < last; first++)
   {
      chunk.data_in       = in;
      chunk.input_frames  = *frames;
      chunk.data_out      = out;
      chunk.output_frames = 0;
      re->dec[first].process(&re->dec[first], &chunk);
      *frames             = chunk.output_frames;
      in                  = out;
      out                 = (out == re->dec_block)
         ? re->dec_block + SINC_DEC_HALF : re->dec_block;
   }
   return in;
}

static size_t sinc_dec_kernel(rarch_sinc_resampler_t *kernel,
      const float *in, size_t frames, float *out, double ratio)
{
   struct resampler_data chunk;
   chunk.data_in       = in;
   chunk.input_frames  = frames;
   chunk.data_out      = out;
   chunk.output_frames = 0;
   chunk.ratio         = ratio;
   kernel->process(kernel, &chunk);
   return chunk.output_frames;
}

/* The kernel's rings move to the shadow, which carries on with the old
 * stages; the kernel and any stage joining start clean. */
static void sinc_dec_switch(rarch_sinc_resampler_t *re, unsigned stages,
      double ratio)
{
   unsigned s;
   double warm                    = re->taps * ratio * (double)(1u << stages);
   rarch_sinc_resampler_t *shadow = &re->dec[SINC_DEC_STAGES];
   memcpy(shadow->buffer_l, re->buffer_l, sizeof(float) * 4 * re->taps);
   shadow->ptr  = re->ptr;
   shadow->time = re->time;
   sinc_dec_restart(re);
   for (s = re->dec_stages; s < stages; s++)
   {
      sinc_dec_restart(&re->dec[s]);
      warm += re->dec[s].taps * ratio * (double)(1u << s);
   }
   if (warm > 8192.0)
      warm = 8192.0;
   /* The paths' delays differ, so a fade shorter than a few
    * milliseconds would splice rather than blend. */
   re->fade_len    = (warm < 256.0) ? 256 : (unsigned)warm + 1;
   re->fade_left   = (unsigned)warm + 1 + re->fade_len;
   re->fade_stages = re->dec_stages;
   re->dec_stages  = stages;
}

static void sinc_dec_blend(rarch_sinc_resampler_t *re, float *out,
      size_t frames, const float *old, size_t old_frames)
{
   size_t i;
   if (!old_frames)
      return;
   for (i = 0; i < frames && re->fade_left; i++, re->fade_left--)
   {
      size_t o = (i < old_frames) ? i : old_frames - 1;
      float w  = (re->fade_left > re->fade_len) ? 0.0f
         : 1.0f - (float)re->fade_left / (float)re->fade_len;
      out[2 * i]     = old[2 * o]     + (out[2 * i]     - old[2 * o])     * w;
      out[2 * i + 1] = old[2 * o + 1] + (out[2 * i + 1] - old[2 * o + 1]) * w;
   }
}

static void resampler_sinc_process_decimated(rarch_sinc_resampler_t *re,
      struct resampler_data *data)
{
   const float *input = data->data_in;
   float *output      = data->data_out;
   size_t frames      = data->input_frames;
   size_t produced    = 0;
   size_t step        = SINC_DEC_BLOCK;
   double ratio       = data->ratio;

   if (!re->fade_left)
   {
      unsigned stages = re->dec_stages;
      while (     stages < SINC_DEC_STAGES
            && ratio * (double)(1u << stages)
               < re->design_ratio
               * (stages ? SINC_DEC_ENGAGE : SINC_DEC_ENGAGE_FIRST))
         stages++;
      while (stages && ratio * (double)(1u << (stages - 1))
            >= re->design_ratio
            * ((stages == 1) ? SINC_DEC_RELEASE_LAST : SINC_DEC_RELEASE))
         stages--;
      if (stages != re->dec_stages)
         sinc_dec_switch(re, stages, ratio);
   }
   /* The old path's output must fit the fade block. */
   if (re->fade_left)
      while (step > 1 && step * ratio + 4.0 > SINC_DEC_FADE_FRAMES)
         step >>= 1;

   while (frames)
   {
      size_t n        = (frames < step) ? frames : step;
      size_t got;
      unsigned now_k  = re->dec_stages;
      float *out      = output + 2 * produced;
      input          += 2 * n;
      frames         -= n;
      if (!re->fade_left)
      {
         const float *p = sinc_dec_chain(re, 0, now_k, input - 2 * n, &n);
         got = sinc_dec_kernel(re, p, n, out, ratio * (double)(1u << now_k));
      }
      else
      {
         rarch_sinc_resampler_t *shadow = &re->dec[SINC_DEC_STAGES];
         unsigned old_k  = re->fade_stages;
         unsigned common = (now_k < old_k) ? now_k : old_k;
         const float *p  = sinc_dec_chain(re, 0, common, input - 2 * n, &n);
         size_t old;
         /* The path without stages of its own reads p before the other
          * path's stages reuse its block. */
         if (now_k == common)
         {
            got = sinc_dec_kernel(re, p, n, out,
                  ratio * (double)(1u << now_k));
            p   = sinc_dec_chain(re, common, old_k, p, &n);
            old = sinc_dec_kernel(shadow, p, n, re->fade_block,
                  ratio * (double)(1u << old_k));
         }
         else
         {
            old = sinc_dec_kernel(shadow, p, n, re->fade_block,
                  ratio * (double)(1u << old_k));
            p   = sinc_dec_chain(re, common, now_k, p, &n);
            got = sinc_dec_kernel(re, p, n, out,
                  ratio * (double)(1u << now_k));
         }
         sinc_dec_blend(re, out, got, re->fade_block, old);
      }
      produced += got;
   }
   data->output_frames = produced;
}

static void sinc_init_table_kaiser(rarch_sinc_resampler_t *resamp,
      double cutoff,
      float *phase_table, int phases, int taps, int calculate_delta)
{
   int i, j;
   /* Kaiser window function - need to normalize w(0) to 1.0f */
   double kaiser_beta = (double)resamp->kaiser_beta;
   double window_mod  = besseli0(kaiser_beta);
   int stride         = calculate_delta ? 2 : 1;
   double sidelobes   = taps / 2.0;

   for (i = 0; i < phases; i++)
   {
      for (j = 0; j < taps; j++)
      {
         double sinc_phase;
         double arg;
         float val;
         int               n = j * phases + i;
         double window_phase = (double)n / (phases * taps); /* [0, 1). */
         window_phase        = 2.0 * window_phase - 1.0;    /* [-1, 1) */
         sinc_phase          = sidelobes * window_phase;

         /* clamp argument to sqrt to avoid NaN from
          * floating-point precision loss at boundaries where
          * 1 - window_phase^2 can go slightly negative.
          * Also use double-precision sqrt() instead of sqrtf()
          * for better accuracy during table generation. */
         arg                 = 1.0 - window_phase * window_phase;
         if (arg < 0.0)
            arg = 0.0;

         val                 = (float)(cutoff * sinc(M_PI * sinc_phase * cutoff) *
              besseli0(kaiser_beta * sqrt(arg))
            / window_mod);
         phase_table[i * stride * taps + j] = val;
      }
   }

   if (calculate_delta)
   {
      int phase;
      int p;
      for (p = 0; p < phases - 1; p++)
      {
         for (j = 0; j < taps; j++)
         {
            float delta = phase_table[(p + 1) * stride * taps + j] -
               phase_table[p * stride * taps + j];
            phase_table[(p * stride + 1) * taps + j] = delta;
         }
      }

      phase = phases - 1;
      for (j = 0; j < taps; j++)
      {
         double sinc_phase;
         double arg;
         float val, delta;
         int n               = j * phases + (phase + 1);
         double window_phase = (double)n / (phases * taps); /* (0, 1]. */
         window_phase        = 2.0 * window_phase - 1.0;    /* (-1, 1] */
         sinc_phase          = sidelobes * window_phase;

         /* Same clamp + double-precision sqrt as above. */
         arg                 = 1.0 - window_phase * window_phase;
         if (arg < 0.0)
            arg = 0.0;

         val                 = (float)(cutoff * sinc(M_PI * sinc_phase * cutoff) *
              besseli0(kaiser_beta * sqrt(arg)) / window_mod);
         delta = (val - phase_table[phase * stride * taps + j]);
         phase_table[(phase * stride + 1) * taps + j] = delta;
      }
   }
}

static void sinc_init_table_lanczos(
      rarch_sinc_resampler_t *resamp, double cutoff,
      float *phase_table, int phases, int taps, int calculate_delta)
{
   int i, j;
   /* Lanczos window function - need to normalize w(0) to 1.0f */
   double window_mod = 1.0;
   int stride        = calculate_delta ? 2 : 1;
   double sidelobes  = taps / 2.0;

   for (i = 0; i < phases; i++)
   {
      for (j = 0; j < taps; j++)
      {
         double sinc_phase;
         float val;
         int               n = j * phases + i;
         double window_phase = (double)n / (phases * taps); /* [0, 1). */
         window_phase        = 2.0 * window_phase - 1.0; /* [-1, 1) */
         sinc_phase          = sidelobes * window_phase;
         val                 = (float)(cutoff * sinc(M_PI * sinc_phase * cutoff) *
            sinc(M_PI * window_phase) / window_mod);
         phase_table[i * stride * taps + j] = val;
      }
   }

   if (calculate_delta)
   {
      int p;
      int phase;

      for (p = 0; p < phases - 1; p++)
      {
         for (j = 0; j < taps; j++)
         {
            float delta = phase_table[(p + 1) * stride * taps + j] -
               phase_table[p * stride * taps + j];
            phase_table[(p * stride + 1) * taps + j] = delta;
         }
      }

      phase = phases - 1;
      for (j = 0; j < taps; j++)
      {
         double sinc_phase;
         float val, delta;
         int n               = j * phases + (phase + 1);
         double window_phase = (double)n / (phases * taps); /* (0, 1]. */
         window_phase        = 2.0 * window_phase - 1.0; /* (-1, 1] */
         sinc_phase          = sidelobes * window_phase;

         val                 = (float)(cutoff * sinc(M_PI * sinc_phase * cutoff) *
            sinc(M_PI * window_phase) / window_mod);
         delta = (val - phase_table[phase * stride * taps + j]);
         phase_table[(phase * stride + 1) * taps + j] = delta;
      }
   }
}

/* Widest kernel both compiled in and allowed by the mask. */
static sinc_process_t sinc_select_process(int kaiser,
      resampler_simd_mask_t mask, unsigned enable_avx)
{
   sinc_process_t process = kaiser
      ? resampler_sinc_process_c_kaiser : resampler_sinc_process_c;
#if defined(__SSE__)
   if (mask & RESAMPLER_SIMD_SSE)
      process = kaiser
         ? resampler_sinc_process_sse_kaiser : resampler_sinc_process_sse;
#endif
#if defined(__AVX__)
   if ((mask & RESAMPLER_SIMD_AVX) && enable_avx)
      process = kaiser
         ? resampler_sinc_process_avx_kaiser : resampler_sinc_process_avx;
#else
   (void)enable_avx;
#endif
#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
   if (mask & RESAMPLER_SIMD_NEON)
      process = kaiser
         ? resampler_sinc_process_neon_kaiser : resampler_sinc_process_neon;
#endif
   return process;
}

void *sinc_resampler_init_hq(double bandwidth_mod,
      enum resampler_quality quality, resampler_simd_mask_t mask,
      int hq_oversampling)
{
   double cutoff                  = 0.0;
   double dec_cutoff              = 0.0;
   float dec_beta                 = 0.0f;
   unsigned dec_taps              = 0;
   unsigned s;
   size_t phase_elems             = 0;
   size_t elems                   = 0;
   unsigned enable_avx            = 0;
   unsigned sidelobes             = 0;
   enum sinc_window window_type   = SINC_WINDOW_NONE;
   rarch_sinc_resampler_t *re     = (rarch_sinc_resampler_t*)
      calloc(1, sizeof(*re));

   if (!re)
      return NULL;

   switch (quality)
   {
      case RESAMPLER_QUALITY_LOWEST:
         cutoff            = 0.98;
         sidelobes         = 2;
         re->phase_bits    = 12;
         re->subphase_bits = 10;
         window_type       = SINC_WINDOW_LANCZOS;
         break;
      case RESAMPLER_QUALITY_LOWER:
         cutoff            = 0.98;
         sidelobes         = 4;
         re->phase_bits    = 12;
         re->subphase_bits = 10;
         window_type       = SINC_WINDOW_LANCZOS;
         break;
      case RESAMPLER_QUALITY_HIGHER:
         cutoff            = 0.90;
         sidelobes         = 32;
         re->phase_bits    = 10;
         re->subphase_bits = 14;
         window_type       = SINC_WINDOW_KAISER;
         re->kaiser_beta   = 10.5;
         enable_avx        = 1;
         break;
      case RESAMPLER_QUALITY_HIGHEST:
         cutoff            = 0.962;
         sidelobes         = 128;
         re->phase_bits    = 10;
         re->subphase_bits = 14;
         window_type       = SINC_WINDOW_KAISER;
         re->kaiser_beta   = 14.5;
         enable_avx        = 1;
         break;
      case RESAMPLER_QUALITY_NORMAL:
      case RESAMPLER_QUALITY_DONTCARE:
         cutoff            = 0.825;
         sidelobes         = 8;
         re->phase_bits    = 8;
         re->subphase_bits = 16;
         window_type       = SINC_WINDOW_KAISER;
         re->kaiser_beta   = 5.5;
         break;
   }

   if (hq_oversampling && bandwidth_mod >= 2.0)
   {
      cutoff            = SINC_HQ_CUTOFF;
      sidelobes         = SINC_HQ_SIDELOBES;
      re->phase_bits    = SINC_HQ_PHASE_BITS;
      re->subphase_bits = SINC_HQ_SUBPHASE_BITS;
      window_type       = SINC_WINDOW_KAISER;
      re->kaiser_beta   = SINC_HQ_KAISER_BETA;
      enable_avx        = 1;
   }

   if (!sinc_resampler_ratio_valid(bandwidth_mod,
            re->phase_bits, re->subphase_bits))
      goto error;

   re->subphase_mask = (1 << re->subphase_bits) - 1;
   re->subphase_mod  = 1.0f / (1 << re->subphase_bits);
   re->taps          = sidelobes * 2;
   re->design_ratio  = (bandwidth_mod < 1.0) ? bandwidth_mod : 1.0;
   /* The Lanczos presets decimate through Normal's filter. */
   if (window_type == SINC_WINDOW_KAISER)
   {
      dec_cutoff = cutoff;
      dec_beta   = re->kaiser_beta;
      dec_taps   = 4 * sidelobes;
   }
   else
   {
      dec_cutoff = 0.825;
      dec_beta   = 5.5f;
      dec_taps   = 32;
   }
   dec_taps = ((unsigned)ceil(dec_taps / SINC_DEC_ENGAGE) + 7) & ~7;

   /* Downsampling, must lower cutoff, and extend number of
    * taps accordingly to keep same stopband attenuation. */
   if (bandwidth_mod < 1.0)
   {
      cutoff  *= bandwidth_mod;
      re->taps = (unsigned)ceil(re->taps / bandwidth_mod);
   }

   /* Be SIMD-friendly: round taps up to a multiple of 8 on every platform.
    * NEON and AVX builds always required this; SSE/scalar builds only
    * required a multiple of 4, which made LOWEST a different filter per
    * platform: 4 taps on x86, 8 on ARM (identical to LOWER there).  The
    * 4-tap variant carries 0.117 dB of per-phase DC gain ripple - audible
    * as slow amplitude wobble on sustained tones as rate control sweeps
    * the phase - while the 8-tap variant measures 0.003 dB.  A uniform
    * multiple of 8 gives every platform the same filter and retires the
    * one quality tier with audible gain ripple.  LOWEST thereby becomes
    * an alias of LOWER for upsampling, matching what ARM and AVX builds
    * have always shipped. */
   re->taps = (re->taps + 7) & ~7;

   phase_elems = ((1 << re->phase_bits) * re->taps);
   if (window_type == SINC_WINDOW_KAISER)
      phase_elems  = phase_elems * 2;
   elems       = sinc_state_elems(re->taps, dec_taps);

   re->main_buffer = (float*)memalign_alloc(128, sizeof(float) * elems);
   re->tables      = (sinc_tables_t*)memalign_alloc(128, sizeof(float)
         * (SINC_TABLES_OFFSET + phase_elems + dec_taps));
   re->dec         = (rarch_sinc_resampler_t*)
      calloc(SINC_DEC_STAGES + 1, sizeof(*re->dec));
   if (!re->main_buffer || !re->tables || !re->dec)
      goto error;

   memset(re->main_buffer, 0, sizeof(float) * elems);
   retro_atomic_int_init(&re->tables->refs, 1);
   re->phase_table = (float*)re->tables + SINC_TABLES_OFFSET;

   /* Each stage is the preset's table for a ratio of SINC_DEC_ENGAGE / 2,
    * at the single phase an exact halving visits. */
   for (s = 0; s < SINC_DEC_STAGES; s++)
   {
      rarch_sinc_resampler_t *stage = &re->dec[s];
      stage->phase_table   = re->phase_table + phase_elems;
      stage->taps          = dec_taps;
      stage->subphase_bits = 1;
      stage->subphase_mask = 1;
      stage->subphase_mod  = 0.5f;
      stage->kaiser_beta   = dec_beta;
      stage->process       = sinc_select_process(0, mask, enable_avx);
   }
   sinc_init_table_kaiser(&re->dec[0], 0.5 * SINC_DEC_ENGAGE * dec_cutoff,
         re->dec[0].phase_table, 1, dec_taps, 0);

   switch (window_type)
   {
      case SINC_WINDOW_LANCZOS:
         sinc_init_table_lanczos(re, cutoff, re->phase_table,
               1 << re->phase_bits, re->taps, 0);
         break;
      case SINC_WINDOW_KAISER:
         sinc_init_table_kaiser(re, cutoff, re->phase_table,
               1 << re->phase_bits, re->taps, 1);
         break;
      case SINC_WINDOW_NONE:
         goto error;
   }

   re->process = sinc_select_process(window_type == SINC_WINDOW_KAISER,
         mask, enable_avx);
   sinc_bind_state(re);

   return re;

error:
   memalign_free(re->main_buffer);
   memalign_free(re->tables);
   free(re->dec);
   free(re);
   return NULL;
}

static void *resampler_sinc_new(const struct resampler_config *config,
      double bandwidth_mod, enum resampler_quality quality,
      resampler_simd_mask_t mask)
{
   (void)config;
   return sinc_resampler_init_hq(bandwidth_mod, quality, mask, 0);
}

/* Thin dispatcher: the vtable is shared across all live instances, so the
 * actual kernel is read from the instance itself.  One indirect call per
 * process() invocation (per chunk, not per sample) - no measurable cost. */
static void resampler_sinc_process(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)re_;
   /* init refuses a nominal ratio the phase clock cannot advance on.
    * The ratio each call carries is a different number - rate control
    * and slow motion move it - and phases / data->ratio is taken from
    * it, so an unusable one leaves a zero step and a loop that emits
    * past data_out. */
   if (!sinc_resampler_ratio_valid(data->ratio,
            re->phase_bits, re->subphase_bits))
   {
      data->output_frames = 0;
      return;
   }
   if (     !re->dec_stages && !re->fade_left
         && data->ratio >= re->design_ratio * SINC_DEC_ENGAGE_FIRST)
      re->process(re_, data);
   else
      resampler_sinc_process_decimated(re, data);
}

retro_resampler_t sinc_resampler = {
   resampler_sinc_new,
   resampler_sinc_process,
   resampler_sinc_free,
   RESAMPLER_API_VERSION,
   "sinc",
   "sinc",
   resampler_sinc_reset,
   RESAMPLER_CAP_QUALITY | RESAMPLER_CAP_HQ_OVERSAMPLE,
   resampler_sinc_sibling
};

#if defined(__GNUC__) && defined(__OPTIMIZE__) && !defined(__clang__)
#if __GNUC__ == 4 && __GNUC_MINOR__ >= 6
/* GCC 4.x pops restore the target options too, which backends without
 * target pragmas (MIPS) answer with a warning; the optimize options are
 * restored all the same. Diagnostic push/pop exist from 4.6. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC pop_options
#pragma GCC diagnostic pop
#else
#pragma GCC pop_options
#endif
#endif
