/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "../boolean.h"
#include "utils.h"

#include "../general.h"
#include "../performance.h"

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

void audio_convert_s16_to_float_C(float *out,
      const int16_t *in, size_t samples, float gain)
{
   size_t i;
   gain = gain / 0x8000;
   for (i = 0; i < samples; i++)
      out[i] = (float)in[i] * gain; 
}

void audio_convert_float_to_s16_C(int16_t *out,
      const float *in, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; i++)
   {
      int32_t val = (int32_t)(in[i] * 0x8000);
      out[i] = (val > 0x7FFF) ? 0x7FFF : (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}

#if defined(__SSE2__)
void audio_convert_s16_to_float_SSE2(float *out,
      const int16_t *in, size_t samples, float gain)
{
   float fgain = gain / UINT32_C(0x80000000);
   __m128 factor = _mm_set1_ps(fgain);
   size_t i;
   for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
   {
      __m128i input = _mm_loadu_si128((const __m128i *)in);
      __m128i regs[2] = {
         _mm_unpacklo_epi16(_mm_setzero_si128(), input),
         _mm_unpackhi_epi16(_mm_setzero_si128(), input),
      };

      __m128 output[2] = {
         _mm_mul_ps(_mm_cvtepi32_ps(regs[0]), factor),
         _mm_mul_ps(_mm_cvtepi32_ps(regs[1]), factor),
      };

      _mm_storeu_ps(out + 0, output[0]);
      _mm_storeu_ps(out + 4, output[1]);
   }

   audio_convert_s16_to_float_C(out, in, samples - i, gain);
}

void audio_convert_float_to_s16_SSE2(int16_t *out,
      const float *in, size_t samples)
{
   __m128 factor = _mm_set1_ps((float)0x8000);
   size_t i;
   for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
   {
      __m128 input[2] = { _mm_loadu_ps(in + 0), _mm_loadu_ps(in + 4) };
      __m128 res[2] = { _mm_mul_ps(input[0], factor), _mm_mul_ps(input[1], factor) };

      __m128i ints[2] = { _mm_cvtps_epi32(res[0]), _mm_cvtps_epi32(res[1]) };
      __m128i packed = _mm_packs_epi32(ints[0], ints[1]);

      _mm_storeu_si128((__m128i *)out, packed);
   }

   audio_convert_float_to_s16_C(out, in, samples - i);
}
#elif defined(__ALTIVEC__)
void audio_convert_s16_to_float_altivec(float *out,
      const int16_t *in, size_t samples, float gain)
{
   const vector float gain_vec = vec_splats(gain);
   const vector float zero_vec = vec_splats(0.0f);
   // Unaligned loads/store is a bit expensive, so we optimize for the good path (very likely).
   if (((uintptr_t)out & 15) + ((uintptr_t)in & 15) == 0)
   {
      size_t i;
      for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
      {
         vector signed short input = vec_ld(0, in);
         vector signed int hi = vec_unpackh(input);
         vector signed int lo = vec_unpackl(input);
         vector float out_hi = vec_madd(vec_ctf(hi, 15), gain_vec, zero_vec);
         vector float out_lo = vec_madd(vec_ctf(lo, 15), gain_vec, zero_vec);

         vec_st(out_hi,  0, out);
         vec_st(out_lo, 16, out);
      }

      audio_convert_s16_to_float_C(out, in, samples - i, gain);
   }
   else
      audio_convert_s16_to_float_C(out, in, samples, gain);
}

void audio_convert_float_to_s16_altivec(int16_t *out,
      const float *in, size_t samples)
{
   // Unaligned loads/store is a bit expensive, so we optimize for the good path (very likely).
   if (((uintptr_t)out & 15) + ((uintptr_t)in & 15) == 0)
   {
      size_t i;
      for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
      {
         vector float input0 = vec_ld( 0, in);
         vector float input1 = vec_ld(16, in);
         vector signed int result0 = vec_cts(input0, 15);
         vector signed int result1 = vec_cts(input1, 15);
         vec_st(vec_packs(result0, result1), 0, out);
      }

      audio_convert_float_to_s16_C(out, in, samples - i);
   }
   else
      audio_convert_float_to_s16_C(out, in, samples);
}
#elif defined(HAVE_NEON)
void audio_convert_s16_float_asm(float *out, const int16_t *in, size_t samples, const float *gain); // Avoid potential hard-float/soft-float ABI issues.
static void audio_convert_s16_to_float_neon(float *out, const int16_t *in, size_t samples,
      float gain)
{
   size_t aligned_samples = samples & ~7;
   if (aligned_samples)
      audio_convert_s16_float_asm(out, in, aligned_samples, &gain);

   // Could do all conversion in ASM, but keep it simple for now.
   audio_convert_s16_to_float_C(out + aligned_samples, in + aligned_samples,
         samples - aligned_samples, gain);
}

void audio_convert_float_s16_asm(int16_t *out, const float *in, size_t samples);
static void audio_convert_float_to_s16_neon(int16_t *out, const float *in, size_t samples)
{
   size_t aligned_samples = samples & ~7;
   if (aligned_samples)
      audio_convert_float_s16_asm(out, in, aligned_samples);

   audio_convert_float_to_s16_C(out + aligned_samples, in + aligned_samples,
         samples - aligned_samples);
}
#endif

void audio_convert_init_simd(void)
{
#ifdef HAVE_NEON
   unsigned cpu = rarch_get_cpu_features();
   audio_convert_s16_to_float_arm = cpu & RETRO_SIMD_NEON ?
      audio_convert_s16_to_float_neon : audio_convert_s16_to_float_C;
   audio_convert_float_to_s16_arm = cpu & RETRO_SIMD_NEON ?
      audio_convert_float_to_s16_neon : audio_convert_float_to_s16_C;
#endif
}

#ifdef HAVE_RSOUND

bool rarch_rsound_start(const char *ip)
{
   strlcpy(g_settings.audio.driver, "rsound", sizeof(g_settings.audio.driver));
   strlcpy(g_settings.audio.device, ip, sizeof(g_settings.audio.device));
   driver.audio_data = NULL;

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
   return g_extern.audio_active;
}

void rarch_rsound_stop(void)
{
   strlcpy(g_settings.audio.driver, config_get_default_audio(), sizeof(g_settings.audio.driver));

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
}

#endif
