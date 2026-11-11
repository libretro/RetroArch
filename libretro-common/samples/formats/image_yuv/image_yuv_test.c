/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (image_yuv_test.c).
 * ---------------------------------------------------------------------------
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

/* image_yuv_blit: the byte-order and UYVY conversions.
 *
 *   image_yuv_420_to_bytes gives, in RGBA order, the pixels
 *   image_yuv_420_to_rgb32 gives with FLAG_RGBA, byte for byte, in each
 *   colour space and range, for planar and interleaved chroma, odd sizes
 *   and padded strides; in ARGB order the same pixels moved; the alpha
 *   asked for; nothing written past a row's pixels; and every sample
 *   within 1 of the matrix in floating point.
 *
 *   image_yuv_420_to_uyvy gives U, Y0, V, Y1 with each chroma row for
 *   the two luma rows it covers, the last luma sample repeated on an odd
 *   width.
 *
 * Build and run:  make && ./image_yuv_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <formats/image_yuv_blit.h>

static unsigned failures;

#define CHECK(c, what) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", what, \
      (unsigned)__LINE__); failures++; } } while (0)

static uint32_t seed = 1;
static unsigned rnd(void)
{
   seed = seed * 1103515245u + 12345u;
   return (seed >> 16) & 0xff;
}

/* The matrix in floating point, as image_yuv_coefficients states it */
static int ref_sample(unsigned flags, int yv, int u, int v, int ch)
{
   double c[6], cy, cr, cb, r;
   float  f[6];
   int    i;
   image_yuv_coefficients(flags, f);
   for (i = 0; i < 6; i++)
      c[i] = f[i];
   cy = yv / 255.0;
   cb = (u - 128) / 255.0;
   cr = (v - 128) / 255.0;
   r  = ch == 0 ? c[0] * cy + c[1] + c[2] * cr
      : ch == 1 ? c[0] * cy + c[1] + c[3] * cb + c[4] * cr
      :           c[0] * cy + c[1] + c[5] * cb;
   r *= 255.0;
   if (r < 0) r = 0;
   if (r > 255) r = 255;
   return (int)floor(r + 0.5);
}

static void test_bytes(unsigned w, unsigned h, unsigned step, unsigned flags)
{
   unsigned cw = (w + 1) / 2, ch = (h + 1) / 2;
   unsigned ys = w + 3, cs = cw * step + 5;
   size_t   bs = (size_t)w * 4 + 12;
   uint8_t *yp = (uint8_t*)malloc((size_t)ys * h);
   uint8_t *cp = (uint8_t*)malloc((size_t)cs * ch + 1);
   uint8_t *cq = (uint8_t*)malloc((size_t)cs * ch);
   /* interleaved: Cr one byte after Cb; planar: a plane of its own */
   const uint8_t *crp = step == 2 ? cp + 1 : cq;
   uint32_t *word = (uint32_t*)malloc((size_t)w * h * 4);
   uint8_t *rgba = (uint8_t*)malloc(bs * h);
   uint8_t *argb = (uint8_t*)malloc(bs * h);
   unsigned x, y, i, worst = 0;
   int same = 1, moved = 1, alpha = 1, untouched = 1;
   char what[128];

   for (i = 0; i < ys * h; i++)
      yp[i] = (uint8_t)rnd();
   for (i = 0; i < cs * ch + 1; i++)
      cp[i] = (uint8_t)rnd();
   for (i = 0; i < cs * ch; i++)
      cq[i] = (uint8_t)rnd();
   memset(rgba, 0x5a, bs * h);
   memset(argb, 0x5a, bs * h);

   image_yuv_420_to_rgb32(word, w, yp, ys, cp, cs, crp, cs, step, w, h,
         flags | IMAGE_YUV_FLAG_RGBA);
   image_yuv_420_to_bytes(rgba, bs, yp, ys, cp, cs, crp, cs, step, w, h, flags,
         IMAGE_YUV_BYTES_RGBA, 0x80);
   image_yuv_420_to_bytes(argb, bs, yp, ys, cp, cs, crp, cs, step, w, h, flags,
         IMAGE_YUV_BYTES_ARGB, 0x40);

   for (y = 0; y < h; y++)
   {
      for (x = 0; x < w; x++)
      {
         uint32_t       p  = word[(size_t)y * w + x];
         const uint8_t *q  = rgba + (size_t)y * bs + (size_t)x * 4;
         const uint8_t *qa = argb + (size_t)y * bs + (size_t)x * 4;
         const uint8_t *cb = cp + (size_t)(y >> 1) * cs + (size_t)(x >> 1) * step;
         const uint8_t *cr = crp + (size_t)(y >> 1) * cs + (size_t)(x >> 1) * step;
         int k;
         if (q[0] != (p & 0xff) || q[1] != ((p >> 8) & 0xff) || q[2] != ((p >> 16) & 0xff))
            same = 0;
         if (qa[1] != q[0] || qa[2] != q[1] || qa[3] != q[2])
            moved = 0;
         if (q[3] != 0x80 || qa[0] != 0x40)
            alpha = 0;
         for (k = 0; k < 3; k++)
         {
            int e = abs((int)q[k] - ref_sample(flags, yp[(size_t)y * ys + x], *cb, *cr, k));
            if ((unsigned)e > worst)
               worst = (unsigned)e;
         }
      }
      for (i = w * 4; i < bs; i++)
         if (rgba[(size_t)y * bs + i] != 0x5a || argb[(size_t)y * bs + i] != 0x5a)
            untouched = 0;
   }

   sprintf(what, "%ux%u step %u flags %u: RGBA bytes as rgb32 with FLAG_RGBA", w, h, step, flags);
   CHECK(same, what);
   sprintf(what, "%ux%u step %u flags %u: ARGB the same pixels moved", w, h, step, flags);
   CHECK(moved, what);
   sprintf(what, "%ux%u step %u flags %u: the alpha asked for", w, h, step, flags);
   CHECK(alpha, what);
   sprintf(what, "%ux%u step %u flags %u: nothing past the row", w, h, step, flags);
   CHECK(untouched, what);
   sprintf(what, "%ux%u step %u flags %u: within 1 of the matrix (%u)", w, h, step, flags, worst);
   CHECK(worst <= 1, what);

   free(yp); free(cp); free(cq); free(word); free(rgba); free(argb);
}

static void test_uyvy(unsigned w, unsigned h)
{
   unsigned cw = (w + 1) / 2, ch = (h + 1) / 2;
   unsigned ys = w + 1, cs = cw + 2;
   size_t   ds = (size_t)cw * 4 + 8;
   uint8_t *yp = (uint8_t*)malloc((size_t)ys * h);
   uint8_t *u  = (uint8_t*)malloc((size_t)cs * ch);
   uint8_t *v  = (uint8_t*)malloc((size_t)cs * ch);
   uint8_t *d  = (uint8_t*)malloc(ds * h);
   unsigned x, y, i;
   int ok = 1;
   char what[96];

   for (i = 0; i < ys * h; i++) yp[i] = (uint8_t)rnd();
   for (i = 0; i < cs * ch; i++) { u[i] = (uint8_t)rnd(); v[i] = (uint8_t)rnd(); }
   memset(d, 0x5a, ds * h);
   image_yuv_420_to_uyvy(d, ds, yp, ys, u, cs, v, cs, w, h);

   for (y = 0; y < h; y++)
   {
      const uint8_t *r = d + (size_t)y * ds;
      for (x = 0; x < w; x += 2, r += 4)
      {
         unsigned x1 = x + 1 < w ? x + 1 : x;
         if (r[0] != u[(y >> 1) * cs + (x >> 1)] || r[1] != yp[(size_t)y * ys + x]
               || r[2] != v[(y >> 1) * cs + (x >> 1)] || r[3] != yp[(size_t)y * ys + x1])
            ok = 0;
      }
      for (i = cw * 4; i < ds; i++)
         if (d[(size_t)y * ds + i] != 0x5a)
            ok = 0;
   }
   sprintf(what, "UYVY %ux%u", w, h);
   CHECK(ok, what);
   free(yp); free(u); free(v); free(d);
}

int main(void)
{
   static const unsigned sizes[][2] = { { 1, 1 }, { 2, 2 }, { 7, 5 }, { 64, 48 }, { 321, 241 } };
   unsigned s, step, flags;

   for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
   {
      for (step = 1; step <= 2; step++)
         for (flags = 0; flags < 8; flags += 2) /* BT709 x FULL_RANGE, not VU */
            test_bytes(sizes[s][0], sizes[s][1], step,
                  ((flags & 2) ? IMAGE_YUV_FLAG_FULL_RANGE : 0) | ((flags & 4) ? IMAGE_YUV_FLAG_BT709 : 0));
      test_uyvy(sizes[s][0], sizes[s][1]);
   }

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("all checks passed\n");
   return failures ? 1 : 0;
}
