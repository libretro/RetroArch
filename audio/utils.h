#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <stdint.h>

#ifdef __SSE2__
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

#ifdef __SSE2__
static inline void audio_convert_s16_to_float_SSE2(float *out,
      const int16_t *in, unsigned samples)
{
   // Not aligned? FML :(
   if (((uintptr_t)in & 7) || ((uintptr_t)out & 15))
   {
      audio_convert_s16_to_float_C(out, in, samples);
      return;
   }

   __m128 factor = _mm_set1_ps(1.0f / 0x7fff);
   unsigned i;
   for (i = 0; i + 4 <= samples; i += 4, in += 4, out += 4)
   {
      __m64 input = *(const __m64*)in;
      __m128 reg = _mm_cvtpi16_ps(input);
      __m128 res = _mm_mul_ps(reg, factor);
      *(__m128*)out = res;
   }

   audio_convert_s16_to_float_C(out, in, samples - i);
}

static inline void audio_convert_float_to_s16_SSE2(int16_t *out,
      const float *in, unsigned samples)
{
   // Not aligned? FML :(
   if (((uintptr_t)in & 7) || ((uintptr_t)out & 15))
   {
      audio_convert_float_to_s16_C(out, in, samples);
      return;
   }

   __m128 factor = _mm_set1_ps(0x7fff);
   unsigned i;
   for (i = 0; i + 4 <= samples; i += 4, in += 4, out += 4)
   {
      __m128 input = *(const __m128*)in;
      __m128 res = _mm_mul_ps(input, factor);
      __m64 output = _mm_cvtps_pi16(res);
      *(__m64*)out = output;
   }

   audio_convert_float_to_s16_C(out, in, samples - i);
}


#endif

#endif

