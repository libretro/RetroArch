/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2014-2016 - Ali Bouhlel ( aliaspider@gmail.com )
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

#include <boolean.h>
#include "audio_utils.h"

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

#ifdef RARCH_INTERNAL
#include "../performance.h"
#include "../performance_counters.h"
#endif

/**
 * audio_convert_s16_to_float_C:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * C implementation callback function.
 **/
void audio_convert_s16_to_float_C(float *out,
      const int16_t *in, size_t samples, float gain)
{
   size_t i;
   gain = gain / 0x8000;
   for (i = 0; i < samples; i++)
      out[i] = (float)in[i] * gain; 
}

/**
 * audio_convert_float_to_s16_C:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts audio samples from floating point 
 * to signed integer 16-bit.
 *
 * C implementation callback function.
 **/
void audio_convert_float_to_s16_C(int16_t *out,
      const float *in, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; i++)
   {
      int32_t val = (int32_t)(in[i] * 0x8000);
      out[i]      = (val > 0x7FFF) ? 0x7FFF :
         (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}

#if defined(__SSE2__)
/**
 * audio_convert_s16_to_float_SSE2:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * SSE2 implementation callback function.
 **/
void audio_convert_s16_to_float_SSE2(float *out,
      const int16_t *in, size_t samples, float gain)
{
   size_t i;
   float fgain   = gain / UINT32_C(0x80000000);
   __m128 factor = _mm_set1_ps(fgain);

   for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
   {
      __m128i input    = _mm_loadu_si128((const __m128i *)in);
      __m128i regs_l   = _mm_unpacklo_epi16(_mm_setzero_si128(), input);
      __m128i regs_r   = _mm_unpackhi_epi16(_mm_setzero_si128(), input);
      __m128 output_l  = _mm_mul_ps(_mm_cvtepi32_ps(regs_l), factor);
      __m128 output_r  = _mm_mul_ps(_mm_cvtepi32_ps(regs_r), factor);

      _mm_storeu_ps(out + 0, output_l);
      _mm_storeu_ps(out + 4, output_r);
   }

   audio_convert_s16_to_float_C(out, in, samples - i, gain);
}

/**
 * audio_convert_float_to_s16_SSE2:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts audio samples from floating point 
 * to signed integer 16-bit.
 *
 * SSE2 implementation callback function.
 **/
void audio_convert_float_to_s16_SSE2(int16_t *out,
      const float *in, size_t samples)
{
   size_t i;
   __m128 factor = _mm_set1_ps((float)0x8000);

   for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
   {
      __m128 input_l = _mm_loadu_ps(in + 0);
      __m128 input_r = _mm_loadu_ps(in + 4);
      __m128 res_l   = _mm_mul_ps(input_l, factor);
      __m128 res_r   = _mm_mul_ps(input_r, factor);
      __m128i ints_l = _mm_cvtps_epi32(res_l);
      __m128i ints_r = _mm_cvtps_epi32(res_r);
      __m128i packed = _mm_packs_epi32(ints_l, ints_r);

      _mm_storeu_si128((__m128i *)out, packed);
   }

   audio_convert_float_to_s16_C(out, in, samples - i);
}
#elif defined(__ALTIVEC__)
/**
 * audio_convert_s16_to_float_altivec:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * AltiVec implementation callback function.
 **/
void audio_convert_s16_to_float_altivec(float *out,
      const int16_t *in, size_t samples, float gain)
{
   size_t samples_in = samples;

   /* Unaligned loads/store is a bit expensive, so we 
    * optimize for the good path (very likely). */
   if (((uintptr_t)out & 15) + ((uintptr_t)in & 15) == 0)
   {
      size_t i;
      const vector float gain_vec = { gain, gain , gain, gain };
      const vector float zero_vec = { 0.0f, 0.0f, 0.0f, 0.0f};

      for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
      {
         vector signed short input = vec_ld(0, in);
         vector signed int hi      = vec_unpackh(input);
         vector signed int lo      = vec_unpackl(input);
         vector float out_hi       = vec_madd(vec_ctf(hi, 15), gain_vec, zero_vec);
         vector float out_lo       = vec_madd(vec_ctf(lo, 15), gain_vec, zero_vec);

         vec_st(out_hi,  0, out);
         vec_st(out_lo, 16, out);
      }

      samples_in -= i;
   }
   audio_convert_s16_to_float_C(out, in, samples_in, gain);
}

/**
 * audio_convert_float_to_s16_altivec:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts audio samples from floating point 
 * to signed integer 16-bit.
 *
 * AltiVec implementation callback function.
 **/
void audio_convert_float_to_s16_altivec(int16_t *out,
      const float *in, size_t samples)
{
   int samples_in = samples;

   /* Unaligned loads/store is a bit expensive, 
    * so we optimize for the good path (very likely). */
   if (((uintptr_t)out & 15) + ((uintptr_t)in & 15) == 0)
   {
      size_t i;
      for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
      {
         vector float       input0 = vec_ld( 0, in);
         vector float       input1 = vec_ld(16, in);
         vector signed int result0 = vec_cts(input0, 15);
         vector signed int result1 = vec_cts(input1, 15);
         vec_st(vec_packs(result0, result1), 0, out);
      }

      samples_in -= i;
   }
   audio_convert_float_to_s16_C(out, in, samples_in);
}
#elif defined(__ARM_NEON__) && !defined(VITA)
/* Avoid potential hard-float/soft-float ABI issues. */
void audio_convert_s16_float_asm(float *out, const int16_t *in,
      size_t samples, const float *gain);

/**
 * audio_convert_s16_to_float_neon:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * ARM NEON implementation callback function.
 **/
static void audio_convert_s16_to_float_neon(float *out,
      const int16_t *in, size_t samples, float gain)
{
   size_t aligned_samples = samples & ~7;
   if (aligned_samples)
      audio_convert_s16_float_asm(out, in, aligned_samples, &gain);

   /* Could do all conversion in ASM, but keep it simple for now. */
   audio_convert_s16_to_float_C(out + aligned_samples, in + aligned_samples,
         samples - aligned_samples, gain);
}

void audio_convert_float_s16_asm(int16_t *out, const float *in, size_t samples);

/**
 * audio_convert_float_to_s16_neon:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts audio samples from floating point 
 * to signed integer 16-bit.
 *
 * ARM NEON implementation callback function.
 **/
static void audio_convert_float_to_s16_neon(int16_t *out,
      const float *in, size_t samples)
{
   size_t aligned_samples = samples & ~7;
   if (aligned_samples)
      audio_convert_float_s16_asm(out, in, aligned_samples);

   audio_convert_float_to_s16_C(out + aligned_samples, in + aligned_samples,
         samples - aligned_samples);
}
#elif defined(_MIPS_ARCH_ALLEGREX)

/**
 * audio_convert_s16_to_float_ALLEGREX:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * MIPS ALLEGREX implementation callback function.
 **/
void audio_convert_s16_to_float_ALLEGREX(float *out,
      const int16_t *in, size_t samples, float gain)
{
#ifdef DEBUG
   /* Make sure the buffer is 16 byte aligned, this should be the 
    * default behaviour of malloc in the PSPSDK.
    * Only the output buffer can be assumed to be 16-byte aligned. */
   retro_assert(((uintptr_t)out & 0xf) == 0);
#endif

   size_t i;
   gain = gain / 0x8000;
   __asm__ (
         ".set    push                    \n"
         ".set    noreorder               \n"
         "mtv     %0, s200                \n"
         ".set    pop                     \n"
         ::"r"(gain));

   for (i = 0; i + 16 <= samples; i += 16)
   {
      __asm__ (
            ".set    push                 \n"
            ".set    noreorder            \n"

            "lv.s    s100,  0(%0)         \n"
            "lv.s    s101,  4(%0)         \n"
            "lv.s    s110,  8(%0)         \n"
            "lv.s    s111, 12(%0)         \n"
            "lv.s    s120, 16(%0)         \n"
            "lv.s    s121, 20(%0)         \n"
            "lv.s    s130, 24(%0)         \n"
            "lv.s    s131, 28(%0)         \n"

            "vs2i.p  c100, c100           \n"
            "vs2i.p  c110, c110           \n"
            "vs2i.p  c120, c120           \n"
            "vs2i.p  c130, c130           \n"

            "vi2f.q  c100, c100, 16       \n"
            "vi2f.q  c110, c110, 16       \n"
            "vi2f.q  c120, c120, 16       \n"
            "vi2f.q  c130, c130, 16       \n"

            "vmscl.q e100, e100, s200     \n"

            "sv.q    c100,  0(%1)         \n"
            "sv.q    c110, 16(%1)         \n"
            "sv.q    c120, 32(%1)         \n"
            "sv.q    c130, 48(%1)         \n"

            ".set    pop                  \n"
            :: "r"(in + i), "r"(out + i));
   }

   for (; i < samples; i++)
      out[i] = (float)in[i] * gain;
}

/**
 * audio_convert_float_to_s16_ALLEGREX:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts audio samples from floating point 
 * to signed integer 16-bit.
 *
 * MIPS ALLEGREX implementation callback function.
 **/
void audio_convert_float_to_s16_ALLEGREX(int16_t *out,
      const float *in, size_t samples)
{
   size_t i;

#ifdef DEBUG
   /* Make sure the buffers are 16 byte aligned, this should be 
    * the default behaviour of malloc in the PSPSDK.
    * Both buffers are allocated by RetroArch, so can assume alignment. */
   retro_assert(((uintptr_t)in  & 0xf) == 0);
   retro_assert(((uintptr_t)out & 0xf) == 0);
#endif

   for (i = 0; i + 8 <= samples; i += 8)
   {
      __asm__ (
            ".set    push                 \n"
            ".set    noreorder            \n"

            "lv.q    c100,  0(%0)         \n"
            "lv.q    c110,  16(%0)        \n"

            "vf2in.q c100, c100, 31       \n"
            "vf2in.q c110, c110, 31       \n"
            "vi2s.q  c100, c100           \n"
            "vi2s.q  c102, c110           \n"

            "sv.q    c100,  0(%1)         \n"

            ".set    pop                  \n"
            :: "r"(in + i), "r"(out + i));
   }

   for (; i < samples; i++)
   {
      int32_t val = (int32_t)(in[i] * 0x8000);
      out[i]      = (val > 0x7FFF) ? 0x7FFF :
         (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}
#endif

#ifndef RARCH_INTERNAL

#ifdef __cplusplus
extern "C" {
#endif
retro_get_cpu_features_t perf_get_cpu_features_cb;

#ifdef __cplusplus
}
#endif

#endif

static unsigned audio_convert_get_cpu_features(void)
{
#ifdef RARCH_INTERNAL
   return cpu_features_get();
#else
   return perf_get_cpu_features_cb();
#endif
}

/**
 * audio_convert_init_simd:
 *
 * Sets up function pointers for audio conversion
 * functions based on CPU features.
 **/
void audio_convert_init_simd(void)
{
   unsigned cpu = audio_convert_get_cpu_features();

   (void)cpu;
#if defined(__ARM_NEON__) && !defined(VITA)
   audio_convert_s16_to_float_arm = (cpu & RETRO_SIMD_NEON) ?
      audio_convert_s16_to_float_neon : audio_convert_s16_to_float_C;
   audio_convert_float_to_s16_arm = (cpu & RETRO_SIMD_NEON) ?
      audio_convert_float_to_s16_neon : audio_convert_float_to_s16_C;
#endif
}
