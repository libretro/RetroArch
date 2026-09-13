/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wsola_search.h).
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

#ifndef LIBRETRO_AUDIO_WSOLA_SEARCH_H
#define LIBRETRO_AUDIO_WSOLA_SEARCH_H

#include <math.h>
#include <stdint.h>
#include <retro_inline.h>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define WSOLA_HAVE_SSE2 1
#else
#define WSOLA_HAVE_SSE2 0
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define WSOLA_HAVE_NEON 1
#else
#define WSOLA_HAVE_NEON 0
#endif

enum wsola_simd
{
   WSOLA_SIMD_SCALAR = 0,
   WSOLA_SIMD_SSE2,
   WSOLA_SIMD_NEON
};

typedef double (*wsola_corr_func_t)(const float *, const float *,
      unsigned, double);

/* floor(sqrt(v)) for a 64-bit value, bit by bit. */
static INLINE uint64_t wsola_isqrt64(uint64_t v)
{
   uint64_t r   = 0;
   uint64_t bit = (uint64_t)1 << 62;
   while (bit > v)
      bit >>= 2;
   while (bit)
   {
      if (v >= r + bit)
      {
         v -= r + bit;
         r  = (r >> 1) + bit;
      }
      else
         r >>= 1;
      bit >>= 2;
   }
   return r;
}

static double wsola_corr_scalar(const float *a, const float *b,
      unsigned n, double ae)
{
   unsigned i;
   double dot = 0.0, be = 0.0;
   for (i = 0; i < n; ++i)
   {
      double x = a[i], y = b[i];
      dot     += x * y;
      be      += y * y;
   }
   return dot / sqrt(ae * be + 1.0e-30);
}

#if WSOLA_HAVE_SSE2
static double wsola_corr_sse2(const float *a, const float *b,
      unsigned n, double ae)
{
   float td[4], te[4];
   double dot, be;
   unsigned i = 0;
   __m128 vd  = _mm_setzero_ps();
   __m128 ve  = _mm_setzero_ps();
   for (; i + 4u <= n; i += 4u)
   {
      __m128 x = _mm_loadu_ps(a + i);
      __m128 y = _mm_loadu_ps(b + i);
      vd       = _mm_add_ps(vd, _mm_mul_ps(x, y));
      ve       = _mm_add_ps(ve, _mm_mul_ps(y, y));
   }
   _mm_storeu_ps(td, vd);
   _mm_storeu_ps(te, ve);
   dot = (double)td[0] + td[1] + td[2] + td[3];
   be  = (double)te[0] + te[1] + te[2] + te[3];
   for (; i < n; ++i)
   {
      double x = a[i], y = b[i];
      dot     += x * y;
      be      += y * y;
   }
   return dot / sqrt(ae * be + 1.0e-30);
}
#endif

#if WSOLA_HAVE_NEON
static double wsola_corr_neon(const float *a, const float *b,
      unsigned n, double ae)
{
   float td[4], te[4];
   double dot, be;
   unsigned i     = 0;
   float32x4_t vd = vdupq_n_f32(0.0f);
   float32x4_t ve = vdupq_n_f32(0.0f);
   for (; i + 4u <= n; i += 4u)
   {
      float32x4_t x = vld1q_f32(a + i);
      float32x4_t y = vld1q_f32(b + i);
      vd            = vmlaq_f32(vd, x, y);
      ve            = vmlaq_f32(ve, y, y);
   }
   vst1q_f32(td, vd);
   vst1q_f32(te, ve);
   dot = (double)td[0] + td[1] + td[2] + td[3];
   be  = (double)te[0] + te[1] + te[2] + te[3];
   for (; i < n; ++i)
   {
      double x = a[i], y = b[i];
      dot     += x * y;
      be      += y * y;
   }
   return dot / sqrt(ae * be + 1.0e-30);
}
#endif

/* Inputs are stereo int16 sums (absolute value <= 65536), n <= 4096.
 * The reference energy is common to every candidate, so candidates
 * rank by dot / sqrt(candidate energy), in Q16. */
static INLINE int64_t wsola_corr_i(const int32_t *a, const int32_t *b, unsigned n)
{
   unsigned i;
   int64_t dot  = 0;
   uint64_t be  = 0;
   uint64_t root;
   for (i = 0; i < n; ++i)
   {
      dot += (int64_t)a[i] * b[i];
      be  += (uint64_t)((int64_t)b[i] * b[i]);
   }
   root = wsola_isqrt64(be);
   return (dot * 65536) / (int64_t)(root ? root : 1);
}

/* Select once per instance; candidate loops call the selected kernel. */
static INLINE wsola_corr_func_t wsola_corr_get(enum wsola_simd simd)
{
#if WSOLA_HAVE_SSE2
   if (simd == WSOLA_SIMD_SSE2)
      return wsola_corr_sse2;
#endif
#if WSOLA_HAVE_NEON
   if (simd == WSOLA_SIMD_NEON)
      return wsola_corr_neon;
#endif
   (void)simd;
   return wsola_corr_scalar;
}

#endif
