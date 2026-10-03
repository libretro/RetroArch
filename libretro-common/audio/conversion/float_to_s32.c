/* Copyright  (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_s32.c).
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
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#include <boolean.h>
#include <features/features_cpu.h>
#include <audio/conversion/float_to_s32.h>

/* The scale is 2^31, and a float has 24 bits of mantissa, so a sample
 * inside [-1, 1) scales to a whole number: truncation is exact and no
 * rounding bias is needed. What has to be handled is the edge: 1.0
 * scales to 2^31, which int32 cannot hold and which the SSE2 convert
 * turns into INT32_MIN, so the value is clamped to the largest float
 * below 2^31 first. NaN is squashed to 0, as the s16 conversion does. */
#define FLOAT_TO_S32_SCALE  2147483648.0f
#define FLOAT_TO_S32_MAX    2147483520.0f   /* largest float below 2^31 */

static int32_t float_to_s32_mask(unsigned valid_bits)
{
   return (valid_bits >= 32) ? (int32_t)~0 : (int32_t)~((1 << (32 - valid_bits)) - 1);
}

static void convert_float_to_s32_c(int32_t *s, const float *in,
      size_t len, int32_t mask)
{
   size_t i;
   for (i = 0; i < len; i++)
   {
      float    scaled = in[i] * FLOAT_TO_S32_SCALE;
      uint32_t bits;
      memcpy(&bits, &scaled, sizeof(bits));
      if ((bits & 0x7FFFFFFFu) > 0x7F800000u)
         s[i] = 0;
      else if (scaled >= FLOAT_TO_S32_SCALE)
         s[i] = 0x7FFFFFFF & mask;
      else if (scaled <= -FLOAT_TO_S32_SCALE)
         s[i] = (int32_t)(-0x7FFFFFFF - 1);
      else
         s[i] = (int32_t)scaled & mask;
   }
}

#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
#include <arm_neon.h>
static bool float_to_s32_neon_enabled = false;

void convert_float_to_s32(int32_t *s, const float *in, size_t len,
      unsigned valid_bits)
{
   int32_t mask = float_to_s32_mask(valid_bits);

   if (float_to_s32_neon_enabled)
   {
      /* vcvtq_s32_f32 saturates and gives NaN -> 0 by itself. */
      float32x4_t vscale = vdupq_n_f32(FLOAT_TO_S32_SCALE);
      int32x4_t   vmask  = vdupq_n_s32(mask);
      while (len >= 8)
      {
         float32x4_t a = vmulq_f32(vld1q_f32(in),     vscale);
         float32x4_t b = vmulq_f32(vld1q_f32(in + 4), vscale);
         vst1q_s32(s,     vandq_s32(vcvtq_s32_f32(a), vmask));
         vst1q_s32(s + 4, vandq_s32(vcvtq_s32_f32(b), vmask));
         in  += 8;
         s   += 8;
         len -= 8;
      }
   }
   convert_float_to_s32_c(s, in, len, mask);
}

void convert_float_to_s32_init_simd(void)
{
   if (cpu_features_get() & RETRO_SIMD_NEON)
      float_to_s32_neon_enabled = true;
}
#else
void convert_float_to_s32(int32_t *s, const float *in, size_t len,
      unsigned valid_bits)
{
   int32_t mask = float_to_s32_mask(valid_bits);
#if defined(__SSE2__)
   __m128  factor = _mm_set1_ps(FLOAT_TO_S32_SCALE);
   __m128  vmax   = _mm_set1_ps(FLOAT_TO_S32_MAX);
   __m128  vmin   = _mm_set1_ps(-FLOAT_TO_S32_SCALE);
   __m128i vmask  = _mm_set1_epi32(mask);
   __m128i vtop   = _mm_set1_epi32(0x7FFFFFFF);

   /* _mm_cvttps_epi32 returns INT32_MIN for NaN and for anything
    * outside int32; the ordered-compare mask zeroes NaN and the clamp
    * keeps the rest inside, so neither reaches it. A sample at or past
    * full scale is then set to INT32_MAX by hand, since the clamp can
    * only reach the largest float below it. */
   for (; len >= 8; len -= 8, in += 8, s += 8)
   {
      __m128  a  = _mm_mul_ps(_mm_loadu_ps(in),     factor);
      __m128  b  = _mm_mul_ps(_mm_loadu_ps(in + 4), factor);
      __m128i ta, tb, ra, rb;
      a          = _mm_and_ps(a, _mm_cmpord_ps(a, a));
      b          = _mm_and_ps(b, _mm_cmpord_ps(b, b));
      ta         = _mm_castps_si128(_mm_cmpge_ps(a, factor));
      tb         = _mm_castps_si128(_mm_cmpge_ps(b, factor));
      a          = _mm_max_ps(_mm_min_ps(a, vmax), vmin);
      b          = _mm_max_ps(_mm_min_ps(b, vmax), vmin);
      ra         = _mm_or_si128(_mm_andnot_si128(ta, _mm_cvttps_epi32(a)),
                                _mm_and_si128(ta, vtop));
      rb         = _mm_or_si128(_mm_andnot_si128(tb, _mm_cvttps_epi32(b)),
                                _mm_and_si128(tb, vtop));
      _mm_storeu_si128((__m128i*)s,       _mm_and_si128(ra, vmask));
      _mm_storeu_si128((__m128i*)(s + 4), _mm_and_si128(rb, vmask));
   }
#endif
   convert_float_to_s32_c(s, in, len, mask);
}

void convert_float_to_s32_init_simd(void)
{
}
#endif
