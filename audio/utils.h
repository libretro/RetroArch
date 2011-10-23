#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <stdint.h>

#if __SSE2__
#include <mmintrin.h>
#include <emmintrin.h>
#define audio_convert_s16_to_float audio_convert_s16_to_float_SSE2
#define audio_convert_float_to_s16 audio_convert_float_to_s16_SSE2
#else
#define audio_convert_s16_to_float audio_convert_s16_to_float_C
#define audio_convert_float_to_s16 audio_convert_float_to_s16_C
#endif

static inline void audio_convert_s16_to_float_C(float *out,
      const int16_t *in, unsigned samples)
{
   for (unsigned i = 0; i < samples; i++)
      out[i] = (float)in[i] / 0x8000; 
}

static inline void audio_convert_float_to_s16_C(int16_t *out,
      const float *in, unsigned samples)
{
   for (unsigned i = 0; i < samples; i++)
   {
      int32_t val = in[i] * 0x8000;
      out[i] = (val > 0x7FFF) ? 0x7FFF : (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}

#if __SSE2__
static inline void audio_convert_s16_to_float_SSE2(float *out,
      const int16_t *in, unsigned samples)
{
   __m128 factor = _mm_set1_ps(1.0f / (0x7fff * 0x10000));
   unsigned i;
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

   audio_convert_s16_to_float_C(out, in, samples - i);
}

static inline void audio_convert_float_to_s16_SSE2(int16_t *out,
      const float *in, unsigned samples)
{
   __m128 factor = _mm_set1_ps((float)0x7fff);
   unsigned i;
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


#endif

#endif

