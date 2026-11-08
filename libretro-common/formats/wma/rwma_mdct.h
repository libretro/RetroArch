/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwma_mdct.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* C89 radix-2 DCT-IV, reusable caller storage. */
#ifndef LIBRETRO_RWMA_MDCT_H
#define LIBRETRO_RWMA_MDCT_H
#if !defined(RWMA_NO_SIMD) &&                                                          \
    (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include <emmintrin.h>
#define RWMA_MDCT_SSE2
#endif
typedef struct rwma_mdct
{
   const float *windows[8];
   const float *pre_re[8], *pre_im[8];
   const float *post_re[8], *post_im[8];
   float *twiddle_re, *twiddle_im;
   float *fft_re, *fft_im;
   unsigned max_frame;
} rwma_mdct;
static void rwma_mdct_init(rwma_mdct *m, float *p, unsigned max_frame)
{
   unsigned i, c, n;
   m->max_frame = max_frame;
   m->fft_re = p;
   p += m->max_frame / 2;
   m->fft_im = p;
   p += m->max_frame / 2;
   m->twiddle_re = p;
   p += m->max_frame / 2;
   m->twiddle_im = p;
   p += m->max_frame / 2;
   for (i = 0; i < m->max_frame / 2; i++)
   {
      double angle = -2 * RWMA_PI * i / (m->max_frame / 2);
      m->twiddle_re[i] = (float)cos(angle);
      m->twiddle_im[i] = (float)sin(angle);
   }
   for (n = 64, i = 0; n <= m->max_frame; n *= 2, i++)
   {
      m->windows[i] = p;
      p += n;
      m->pre_re[i] = p;
      p += n / 2;
      m->pre_im[i] = p;
      p += n / 2;
      m->post_re[i] = p;
      p += n / 2;
      m->post_im[i] = p;
      p += n / 2;
      for (c = 0; c < n; c++)
         ((float *)m->windows[i])[c] = (float)sin((c + 0.5) * RWMA_PI / (2 * n));
      for (c = 0; c < n / 2; c++)
      {
         double a = -RWMA_PI * (4 * c + 1) / (4 * n);
         double b = -RWMA_PI * c / n;
         ((float *)m->pre_re[i])[c] = (float)cos(a);
         ((float *)m->pre_im[i])[c] = (float)sin(a);
         ((float *)m->post_re[i])[c] = (float)cos(b);
         ((float *)m->post_im[i])[c] = (float)sin(b);
      }
   }
}
static const unsigned char rwma_reverse8[256] = {
    0,  128, 64, 192, 32, 160, 96,  224, 16, 144, 80, 208, 48, 176, 112, 240,
    8,  136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
    4,  132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
    12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
    2,  130, 66, 194, 34, 162, 98,  226, 18, 146, 82, 210, 50, 178, 114, 242,
    10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
    6,  134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
    14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
    1,  129, 65, 193, 33, 161, 97,  225, 17, 145, 81, 209, 49, 177, 113, 241,
    9,  137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
    5,  133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
    13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
    3,  131, 67, 195, 35, 163, 99,  227, 19, 147, 83, 211, 51, 179, 115, 243,
    11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
    7,  135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
    15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255,
};
/* DCT-IV through an N/2-point FFT, scattered directly in IMDCT order. */
static void rwma_mdct_run(rwma_mdct *s, float *dst, const float *x, int n, float scale)
{
   int k, i, j, len, half, q, size = n / 2;
   int plan = rwma_log2(n) - 6;
   float *re = s->fft_re, *im = s->fft_im;
   const float *pr = s->pre_re[plan], *pi = s->pre_im[plan];
   const float *qr = s->post_re[plan], *qi = s->post_im[plan];
   if (size <= 256)
   {
      for (k = 0; k < size; k++)
      {
         float a = x[2 * k], b = x[n - 1 - 2 * k];
         j = rwma_reverse8[k] >> (3 - plan);
         re[j] = a * pr[k] - b * pi[k];
         im[j] = a * pi[k] + b * pr[k];
      }
   }
   else
   {
      k = 0;
#ifdef RWMA_MDCT_SSE2
      for (; k + 3 < size; k += 4)
      {
         __m128 a = _mm_shuffle_ps(_mm_loadu_ps(x + 2 * k), _mm_loadu_ps(x + 2 * k + 4),
                                   _MM_SHUFFLE(2, 0, 2, 0));
         __m128 b =
             _mm_shuffle_ps(_mm_loadu_ps(x + n - 2 * k - 8),
                            _mm_loadu_ps(x + n - 2 * k - 4), _MM_SHUFFLE(3, 1, 3, 1));
         __m128 real = _mm_loadu_ps(pr + k), imag = _mm_loadu_ps(pi + k);
         b = _mm_shuffle_ps(b, b, _MM_SHUFFLE(0, 1, 2, 3));
         _mm_storeu_ps(re + k, _mm_sub_ps(_mm_mul_ps(a, real), _mm_mul_ps(b, imag)));
         _mm_storeu_ps(im + k, _mm_add_ps(_mm_mul_ps(a, imag), _mm_mul_ps(b, real)));
      }
#endif
      for (; k < size; k++)
      {
         float a = x[2 * k], b = x[n - 1 - 2 * k];
         re[k] = a * pr[k] - b * pi[k];
         im[k] = a * pi[k] + b * pr[k];
      }
      for (i = 1, j = 0; i < size; i++)
      {
         int bit = size / 2;
         for (; j & bit; bit >>= 1)
            j ^= bit;
         j ^= bit;
         if (i < j)
         {
            float a = re[i], b = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = a;
            im[j] = b;
         }
      }
   }
   for (len = 2; len <= size; len *= 2)
   {
      half = len / 2;
      for (i = 0; i < size; i += len)
      {
         j = 0;
#ifdef RWMA_MDCT_SSE2
         for (; j + 3 < half; j += 4)
         {
            int a = i + j, b = a + half;
            int step = s->max_frame / 2 / len;
            __m128 wr, wi, vr, vi, ur, ui, br, bi;
            q = j * step;
            wr = _mm_set_ps(s->twiddle_re[q + 3 * step], s->twiddle_re[q + 2 * step],
                            s->twiddle_re[q + step], s->twiddle_re[q]);
            wi = _mm_set_ps(s->twiddle_im[q + 3 * step], s->twiddle_im[q + 2 * step],
                            s->twiddle_im[q + step], s->twiddle_im[q]);
            ur = _mm_loadu_ps(re + a);
            ui = _mm_loadu_ps(im + a);
            br = _mm_loadu_ps(re + b);
            bi = _mm_loadu_ps(im + b);
            vr = _mm_sub_ps(_mm_mul_ps(br, wr), _mm_mul_ps(bi, wi));
            vi = _mm_add_ps(_mm_mul_ps(br, wi), _mm_mul_ps(bi, wr));
            _mm_storeu_ps(re + a, _mm_add_ps(ur, vr));
            _mm_storeu_ps(im + a, _mm_add_ps(ui, vi));
            _mm_storeu_ps(re + b, _mm_sub_ps(ur, vr));
            _mm_storeu_ps(im + b, _mm_sub_ps(ui, vi));
         }
#endif
         for (; j < half; j++)
         {
            int a = i + j, b = a + half;
            float wr, wi, vr, vi, ur = re[a], ui = im[a];
            q = j * (s->max_frame / 2 / len);
            wr = s->twiddle_re[q];
            wi = s->twiddle_im[q];
            vr = re[b] * wr - im[b] * wi;
            vi = re[b] * wi + im[b] * wr;
            re[a] = ur + vr;
            im[a] = ui + vi;
            re[b] = ur - vr;
            im[b] = ui - vi;
         }
      }
   }
   k = 0;
#ifdef RWMA_MDCT_SSE2
   for (; k < size / 2; k += 4)
   {
      int opposite = size - 4 - k;
      __m128 gain = _mm_set1_ps(scale);
      __m128 sign = _mm_castsi128_ps(_mm_set1_epi32(INT_MIN));
      __m128 real = _mm_loadu_ps(re + k), imag = _mm_loadu_ps(im + k);
      __m128 cosine = _mm_loadu_ps(qr + k), sine = _mm_loadu_ps(qi + k);
      __m128 odd_low = _mm_mul_ps(
          _mm_sub_ps(_mm_mul_ps(real, cosine), _mm_mul_ps(imag, sine)), gain);
      __m128 even_low = _mm_mul_ps(
          _mm_xor_ps(_mm_add_ps(_mm_mul_ps(real, sine), _mm_mul_ps(imag, cosine)),
                     sign),
          gain);
      __m128 odd_high, even_high;
      real = _mm_loadu_ps(re + opposite);
      imag = _mm_loadu_ps(im + opposite);
      cosine = _mm_loadu_ps(qr + opposite);
      sine = _mm_loadu_ps(qi + opposite);
      odd_high = _mm_mul_ps(
          _mm_sub_ps(_mm_mul_ps(real, cosine), _mm_mul_ps(imag, sine)), gain);
      even_high = _mm_mul_ps(
          _mm_xor_ps(_mm_add_ps(_mm_mul_ps(real, sine), _mm_mul_ps(imag, cosine)),
                     sign),
          gain);
      odd_high = _mm_shuffle_ps(odd_high, odd_high, _MM_SHUFFLE(0, 1, 2, 3));
      odd_low = _mm_shuffle_ps(odd_low, odd_low, _MM_SHUFFLE(0, 1, 2, 3));
      _mm_storeu_ps(dst + 2 * k, _mm_unpacklo_ps(even_low, odd_high));
      _mm_storeu_ps(dst + 2 * k + 4, _mm_unpackhi_ps(even_low, odd_high));
      _mm_storeu_ps(dst + 2 * opposite, _mm_unpacklo_ps(even_high, odd_low));
      _mm_storeu_ps(dst + 2 * opposite + 4, _mm_unpackhi_ps(even_high, odd_low));
   }
   k = size;
#endif
   for (; k < size; k++)
   {
      float yr = re[k] * qr[k] - im[k] * qi[k];
      float yi = re[k] * qi[k] + im[k] * qr[k];
      dst[n - 1 - 2 * k] = yr * scale;
      dst[2 * k] = -yi * scale;
   }
}

#endif
