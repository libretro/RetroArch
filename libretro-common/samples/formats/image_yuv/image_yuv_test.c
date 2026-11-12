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
 *   image_yuv_420_from_bytes, the other way, gives in each colour
 *   space and range, from each byte order, luma within 1 of the matrix
 *   in floating point and chroma within 1 of the 2x2 average's; black,
 *   white and every grey exactly; the same planes from any order and
 *   any alpha; nothing written past a row's samples; and the pixels
 *   back within 2 through image_yuv_420_to_bytes. With --ffmpeg it
 *   reads RGBA frames and FFmpeg's yuv420p of them (swscale with
 *   -sws_flags area+accurate_rnd, the same 2x2 average) and checks every
 *   sample within 1 of swscale's.
 *
 *   image_yuv_420_to_uyvy gives U, Y0, V, Y1 with each chroma row for
 *   the two luma rows it covers, the last luma sample repeated on an odd
 *   width.
 *
 * Build and run:  make && ./image_yuv_test
 *                 ./image_yuv_test --ffmpeg frames.rgba frames.yuv W H
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
   uint8_t *bgra = (uint8_t*)malloc(bs * h);
   uint8_t *abgr = (uint8_t*)malloc(bs * h);
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
   memset(bgra, 0x5a, bs * h);
   memset(abgr, 0x5a, bs * h);

   image_yuv_420_to_rgb32(word, w, yp, ys, cp, cs, crp, cs, step, w, h,
         flags | IMAGE_YUV_FLAG_RGBA);
   image_yuv_420_to_bytes(rgba, bs, yp, ys, cp, cs, crp, cs, step, w, h, flags,
         IMAGE_YUV_BYTES_RGBA, 0x80);
   image_yuv_420_to_bytes(argb, bs, yp, ys, cp, cs, crp, cs, step, w, h, flags,
         IMAGE_YUV_BYTES_ARGB, 0x40);
   image_yuv_420_to_bytes(bgra, bs, yp, ys, cp, cs, crp, cs, step, w, h, flags,
         IMAGE_YUV_BYTES_BGRA, 0x20);
   image_yuv_420_to_bytes(abgr, bs, yp, ys, cp, cs, crp, cs, step, w, h, flags,
         IMAGE_YUV_BYTES_ABGR, 0x10);

   for (y = 0; y < h; y++)
   {
      for (x = 0; x < w; x++)
      {
         uint32_t       p  = word[(size_t)y * w + x];
         const uint8_t *q  = rgba + (size_t)y * bs + (size_t)x * 4;
         const uint8_t *qa = argb + (size_t)y * bs + (size_t)x * 4;
         const uint8_t *qb = bgra + (size_t)y * bs + (size_t)x * 4;
         const uint8_t *qc = abgr + (size_t)y * bs + (size_t)x * 4;
         const uint8_t *cb = cp + (size_t)(y >> 1) * cs + (size_t)(x >> 1) * step;
         const uint8_t *cr = crp + (size_t)(y >> 1) * cs + (size_t)(x >> 1) * step;
         int k;
         if (q[0] != (p & 0xff) || q[1] != ((p >> 8) & 0xff) || q[2] != ((p >> 16) & 0xff))
            same = 0;
         if (qa[1] != q[0] || qa[2] != q[1] || qa[3] != q[2])
            moved = 0;
         if (qb[2] != q[0] || qb[1] != q[1] || qb[0] != q[2])
            moved = 0;
         if (qc[3] != q[0] || qc[2] != q[1] || qc[1] != q[2])
            moved = 0;
         if (q[3] != 0x80 || qa[0] != 0x40 || qb[3] != 0x20 || qc[0] != 0x10)
            alpha = 0;
         for (k = 0; k < 3; k++)
         {
            int e = abs((int)q[k] - ref_sample(flags, yp[(size_t)y * ys + x], *cb, *cr, k));
            if ((unsigned)e > worst)
               worst = (unsigned)e;
         }
      }
      for (i = w * 4; i < bs; i++)
         if (rgba[(size_t)y * bs + i] != 0x5a || argb[(size_t)y * bs + i] != 0x5a
               || bgra[(size_t)y * bs + i] != 0x5a || abgr[(size_t)y * bs + i] != 0x5a)
            untouched = 0;
   }

   sprintf(what, "%ux%u step %u flags %u: RGBA bytes as rgb32 with FLAG_RGBA", w, h, step, flags);
   CHECK(same, what);
   sprintf(what, "%ux%u step %u flags %u: ARGB, BGRA, ABGR the same pixels moved", w, h, step, flags);
   CHECK(moved, what);
   sprintf(what, "%ux%u step %u flags %u: the alpha asked for", w, h, step, flags);
   CHECK(alpha, what);
   sprintf(what, "%ux%u step %u flags %u: nothing past the row", w, h, step, flags);
   CHECK(untouched, what);
   sprintf(what, "%ux%u step %u flags %u: within 1 of the matrix (%u)", w, h, step, flags, worst);
   CHECK(worst <= 1, what);

   free(yp); free(cp); free(cq); free(word); free(rgba); free(argb);
   free(bgra); free(abgr);
}

/* RGB to YCbCr in floating point, from the standard's Kr and Kb */
static void ref_forward(unsigned flags, double r, double g, double b, double out[3])
{
   double kr = (flags & IMAGE_YUV_FLAG_BT709) ? 0.2126 : 0.299;
   double kb = (flags & IMAGE_YUV_FLAG_BT709) ? 0.0722 : 0.114;
   int    full = (flags & IMAGE_YUV_FLAG_FULL_RANGE) != 0;
   double yy = kr * r + (1 - kr - kb) * g + kb * b;
   out[0] = full ? yy : 16 + yy * 219 / 255;
   out[1] = 128 + (b - yy) / (2 * (1 - kb)) * (full ? 1 : 224.0 / 255);
   out[2] = 128 + (r - yy) / (2 * (1 - kr)) * (full ? 1 : 224.0 / 255);
}

static const unsigned orders[4][3] = { { 0, 1, 2 }, { 1, 2, 3 }, { 2, 1, 0 }, { 3, 2, 1 } };

static void test_from_bytes(unsigned w, unsigned h, unsigned step, unsigned flags)
{
   unsigned cw = (w + 1) / 2, ch = (h + 1) / 2;
   unsigned ys = w + 3, cs = cw * step + 5;
   size_t   ss = (size_t)w * 4 + 12;
   uint8_t *rgb = (uint8_t*)malloc((size_t)w * h * 3);
   uint8_t *src = (uint8_t*)malloc(ss * h);
   uint8_t *yp[4], *cp[4], *cq[4];
   uint8_t *back = (uint8_t*)malloc((size_t)w * h * 4);
   unsigned x, y, i, o, worst_y = 0, worst_c = 0, worst_back = 0;
   int same = 1, untouched = 1;
   char what[128];

   for (i = 0; i < w * h * 3; i++)
      rgb[i] = (uint8_t)rnd();
   for (o = 0; o < 4; o++)
   {
      yp[o] = (uint8_t*)malloc((size_t)ys * h);
      cp[o] = (uint8_t*)malloc((size_t)cs * ch + 1);
      cq[o] = (uint8_t*)malloc((size_t)cs * ch);
      memset(yp[o], 0x5a, (size_t)ys * h);
      memset(cp[o], 0x5a, (size_t)cs * ch + 1);
      memset(cq[o], 0x5a, (size_t)cs * ch);
      /* the pixels in this order, any alpha, padding past the row */
      for (y = 0; y < h; y++)
      {
         uint8_t *row = src + (size_t)y * ss;
         memset(row, 0xa5, ss);
         for (x = 0; x < w; x++)
         {
            unsigned k;
            for (k = 0; k < 4; k++)
               row[x * 4 + k] = (uint8_t)rnd();   /* the alpha stays random */
            for (k = 0; k < 3; k++)
               row[x * 4 + orders[o][k]] = rgb[((size_t)y * w + x) * 3 + k];
         }
      }
      image_yuv_420_from_bytes(yp[o], ys, cp[o], cs, step == 2 ? cp[o] + 1 : cq[o], cs, step,
            src, ss, w, h, flags, o);
   }

   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         const uint8_t *p = rgb + ((size_t)y * w + x) * 3;
         double ref[3];
         int    e;
         ref_forward(flags, p[0], p[1], p[2], ref);
         e = abs((int)yp[0][(size_t)y * ys + x] - (int)floor(ref[0] + 0.5));
         if ((unsigned)e > worst_y)
            worst_y = (unsigned)e;
      }
   for (y = 0; y < h; y += 2)
      for (x = 0; x < w; x += 2)
      {
         /* the 2x2 average, an odd edge's pixel counted twice */
         unsigned x1 = x + 1 < w ? x + 1 : x, y1 = y + 1 < h ? y + 1 : y, k;
         double   sum[3] = { 0, 0, 0 }, ref[3];
         const uint8_t *cb = cp[0] + (size_t)(y >> 1) * cs + (size_t)(x >> 1) * step;
         const uint8_t *cr = (step == 2 ? cp[0] + 1 : cq[0]) + (size_t)(y >> 1) * cs + (size_t)(x >> 1) * step;
         int e;
         for (k = 0; k < 3; k++)
            sum[k] = (rgb[((size_t)y * w + x) * 3 + k] + rgb[((size_t)y * w + x1) * 3 + k]
                  + rgb[((size_t)y1 * w + x) * 3 + k] + rgb[((size_t)y1 * w + x1) * 3 + k]) / 4.0;
         ref_forward(flags, sum[0], sum[1], sum[2], ref);
         if (ref[1] > 255) ref[1] = 255;
         if (ref[2] > 255) ref[2] = 255;
         e = abs((int)*cb - (int)floor(ref[1] + 0.5));
         if ((unsigned)e > worst_c) worst_c = (unsigned)e;
         e = abs((int)*cr - (int)floor(ref[2] + 0.5));
         if ((unsigned)e > worst_c) worst_c = (unsigned)e;
      }
   for (o = 1; o < 4; o++)
      if (memcmp(yp[o], yp[0], (size_t)ys * h) || memcmp(cp[o], cp[0], (size_t)cs * ch + 1)
            || memcmp(cq[o], cq[0], (size_t)cs * ch))
         same = 0;
   for (y = 0; y < h; y++)
      for (i = w; i < ys; i++)
         if (yp[0][(size_t)y * ys + i] != 0x5a)
            untouched = 0;
   for (y = 0; y < ch; y++)
   {
      for (i = 0; i < cs; i++)
      {
         /* planar: the samples; interleaved: Cb and Cr, then nothing */
         int written = step == 1 ? i < cw : i < cw * 2;
         if (!written && cp[0][(size_t)y * cs + i] != 0x5a)
            untouched = 0;
         if ((step == 2 || i >= cw) && cq[0][(size_t)y * cs + i] != 0x5a)
            untouched = 0;
      }
   }
   if (cp[0][(size_t)cs * ch] != 0x5a)
      untouched = 0;

   /* and back, on pixels whose 2x2 blocks are flat */
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         unsigned k;
         for (k = 0; k < 3; k++)
            rgb[((size_t)y * w + x) * 3 + k] = rgb[((size_t)(y & ~1u) * w + (x & ~1u)) * 3 + k];
         for (k = 0; k < 3; k++)
            src[(size_t)y * ss + x * 4 + k] = rgb[((size_t)y * w + x) * 3 + k];
      }
   image_yuv_420_from_bytes(yp[1], ys, cp[1], cs, step == 2 ? cp[1] + 1 : cq[1], cs, step,
         src, ss, w, h, flags, IMAGE_YUV_BYTES_RGBA);
   image_yuv_420_to_bytes(back, (size_t)w * 4, yp[1], ys, cp[1], cs, step == 2 ? cp[1] + 1 : cq[1], cs,
         step, w, h, flags, IMAGE_YUV_BYTES_RGBA, 0);
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         unsigned k;
         for (k = 0; k < 3; k++)
         {
            int e = abs((int)back[((size_t)y * w + x) * 4 + k] - (int)rgb[((size_t)y * w + x) * 3 + k]);
            if ((unsigned)e > worst_back)
               worst_back = (unsigned)e;
         }
      }

   sprintf(what, "from %ux%u step %u flags %u: luma within 1 of the matrix (%u)", w, h, step, flags, worst_y);
   CHECK(worst_y <= 1, what);
   sprintf(what, "from %ux%u step %u flags %u: chroma within 1 of the average's (%u)", w, h, step, flags, worst_c);
   CHECK(worst_c <= 1, what);
   sprintf(what, "from %ux%u step %u flags %u: the same planes from every order", w, h, step, flags);
   CHECK(same, what);
   sprintf(what, "from %ux%u step %u flags %u: nothing past the row", w, h, step, flags);
   CHECK(untouched, what);
   sprintf(what, "from %ux%u step %u flags %u: back within 2 (%u)", w, h, step, flags, worst_back);
   CHECK(worst_back <= 2, what);

   for (o = 0; o < 4; o++)
   {
      free(yp[o]); free(cp[o]); free(cq[o]);
   }
   free(rgb); free(src); free(back);
}

/* black, white and every grey: exact */
static void test_greys(void)
{
   unsigned flags, v;
   for (flags = 0; flags < 4; flags++)
   {
      unsigned f = ((flags & 1) ? IMAGE_YUV_FLAG_FULL_RANGE : 0) | ((flags & 2) ? IMAGE_YUV_FLAG_BT709 : 0);
      int      ok = 1;
      char     what[64];
      for (v = 0; v < 256; v++)
      {
         uint8_t px[16], yy[4], u, c;
         double  want = (flags & 1) ? v : 16 + v * 219.0 / 255;
         unsigned i;
         for (i = 0; i < 16; i++)
            px[i] = (uint8_t)v;
         image_yuv_420_from_bytes(yy, 2, &u, 1, &c, 1, 1, px, 8, 2, 2, f, IMAGE_YUV_BYTES_ARGB);
         if (u != 128 || c != 128 || yy[0] != (uint8_t)floor(want + 0.5) || yy[3] != yy[0])
            ok = 0;
      }
      sprintf(what, "greys exact, flags %u", f);
      CHECK(ok, what);
   }
}

/* FFmpeg's yuv420p of RGBA frames */
static int against_ffmpeg(const char *rgba_path, const char *yuv_path, unsigned w, unsigned h)
{
   size_t   fs = (size_t)w * h * 4, ysz = (size_t)w * h;
   unsigned cw = (w + 1) / 2, ch = (h + 1) / 2;
   size_t   ysize = ysz + 2 * (size_t)cw * ch;
   uint8_t *src = (uint8_t*)malloc(fs), *ref = (uint8_t*)malloc(ysize), *ours = (uint8_t*)malloc(ysize);
   FILE    *a = fopen(rgba_path, "rb"), *b = fopen(yuv_path, "rb");
   unsigned frames = 0, worst_y = 0, worst_c = 0;
   size_t   i;
   if (!a || !b || !src || !ref || !ours)
   {
      printf("  FAIL  cannot read %s or %s\n", rgba_path, yuv_path);
      return 1;
   }
   while (fread(src, 1, fs, a) == fs && fread(ref, 1, ysize, b) == ysize)
   {
      image_yuv_420_from_bytes(ours, w, ours + ysz, cw, ours + ysz + (size_t)cw * ch, cw, 1,
            src, (size_t)w * 4, w, h, 0, IMAGE_YUV_BYTES_RGBA);
      for (i = 0; i < ysize; i++)
      {
         unsigned e = (unsigned)abs((int)ours[i] - (int)ref[i]);
         if (i < ysz && e > worst_y) worst_y = e;
         if (i >= ysz && e > worst_c) worst_c = e;
      }
      frames++;
   }
   fclose(a); fclose(b);
   printf("%u frames %ux%u: luma within %u of FFmpeg's, chroma within %u\n", frames, w, h, worst_y, worst_c);
   CHECK(frames > 0, "frames read");
   CHECK(worst_y <= 1, "luma within 1 of swscale's");
   CHECK(worst_c <= 1, "chroma within 1 of swscale's");
   free(src); free(ref); free(ours);
   return failures ? 1 : 0;
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

int main(int argc, char **argv)
{
   static const unsigned sizes[][2] = { { 1, 1 }, { 2, 2 }, { 7, 5 }, { 64, 48 }, { 321, 241 } };
   unsigned s, step, flags;

   if (argc == 6 && !strcmp(argv[1], "--ffmpeg"))
      return against_ffmpeg(argv[2], argv[3], (unsigned)atoi(argv[4]), (unsigned)atoi(argv[5]));

   for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
   {
      for (step = 1; step <= 2; step++)
         for (flags = 0; flags < 8; flags += 2) /* BT709 x FULL_RANGE, not VU */
         {
            unsigned f = ((flags & 2) ? IMAGE_YUV_FLAG_FULL_RANGE : 0) | ((flags & 4) ? IMAGE_YUV_FLAG_BT709 : 0);
            test_bytes(sizes[s][0], sizes[s][1], step, f);
            test_from_bytes(sizes[s][0], sizes[s][1], step, f);
         }
      test_uyvy(sizes[s][0], sizes[s][1]);
   }
   test_greys();

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("all checks passed\n");
   return failures ? 1 : 0;
}
