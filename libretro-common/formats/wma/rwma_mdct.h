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
/* DCT-IV through an N/2-point FFT, scattered directly in IMDCT order. */
static void rwma_mdct_run(rwma_mdct *s, float *dst, const float *x, int n, float scale)
{
   int k, i, j, len, half, q, size = n / 2;
   int plan = rwma_log2(n) - 6;
   float *re = s->fft_re, *im = s->fft_im;
   const float *pr = s->pre_re[plan], *pi = s->pre_im[plan];
   const float *qr = s->post_re[plan], *qi = s->post_im[plan];
   for (k = 0; k < size; k++)
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
   for (len = 2; len <= size; len *= 2)
   {
      half = len / 2;
      for (i = 0; i < size; i += len)
         for (j = 0; j < half; j++)
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
   for (k = 0; k < size; k++)
   {
      float yr = re[k] * qr[k] - im[k] * qi[k];
      float yi = re[k] * qi[k] + im[k] * qr[k];
      dst[n - 1 - 2 * k] = yr * scale;
      dst[2 * k] = -yi * scale;
   }
}

#endif
