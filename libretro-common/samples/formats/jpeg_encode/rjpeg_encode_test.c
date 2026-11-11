/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rjpeg_encode_test.c).
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

/* rjpeg_encode: pictures made here (gradients, edges, noise) at sizes
 * that are and are not whole MCUs, 4:2:0 and 4:2:2, three qualities,
 * each encoded and decoded back with rjpeg:
 *   - the file is the bytes it was when checked against libjpeg at the
 *     same quality (sizes within a few percent, PSNR within 0.2 dB) and
 *     FFmpeg and libjpeg decoded it;
 *   - what rjpeg decodes is within the PSNR that quality gives;
 *   - noise at quality 100, the worst case, fits rjpeg_encode_bound()
 *     (its PSNR only says it decoded: rjpeg's chroma upsampling and the
 *     repeated chroma it is measured against differ most on noise);
 *   - a buffer a byte short gives 0 and writes nothing past its end.
 *
 * With a directory argument it also writes the files there, for the CI
 * workflow to decode with FFmpeg.
 *
 * Build and run:  make && ./rjpeg_encode_test [dir]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <formats/rjpeg.h>
#include <formats/rjpeg_encode.h>

static unsigned failures;

#define CHECK(c) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", #c, \
      (unsigned)__LINE__); failures++; } } while (0)

typedef struct
{
   unsigned       w, h, cw, ch;
   unsigned char *y, *cb, *cr;
} picture_t;

static unsigned long fnv(const unsigned char *p, size_t n)
{
   unsigned long h = 2166136261UL;
   size_t i;
   for (i = 0; i < n; i++)
      h = ((h ^ p[i]) * 16777619UL) & 0xFFFFFFFFUL;
   return h;
}

static unsigned char clip(int v)
{
   return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* kind 0: gradients and a sharp-edged box; kind 1: noise */
static void make(picture_t *p, unsigned w, unsigned h, bool c422, int kind)
{
   unsigned long seed = 99;
   unsigned x, y;
   p->w  = w;
   p->h  = h;
   p->cw = (w + 1) / 2;
   p->ch = c422 ? h : (h + 1) / 2;
   p->y  = (unsigned char*)malloc((size_t)w * h);
   p->cb = (unsigned char*)malloc((size_t)p->cw * p->ch);
   p->cr = (unsigned char*)malloc((size_t)p->cw * p->ch);
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         int v;
         seed = seed * 1103515245UL + 12345UL;
         if (kind)
            v = (int)((seed >> 16) & 0xFF);
         else
         {
            v = (int)(x * 255 / (w > 1 ? w - 1 : 1)) / 2 + (int)(y * 127 / (h > 1 ? h - 1 : 1));
            if (x > w / 4 && x < w / 2 && y > h / 4 && y < h / 2)
               v = 235;
         }
         p->y[y * w + x] = clip(v);
      }
   for (y = 0; y < p->ch; y++)
      for (x = 0; x < p->cw; x++)
      {
         seed = seed * 1103515245UL + 12345UL;
         p->cb[y * p->cw + x] = kind ? (unsigned char)(seed >> 20) : clip(64 + (int)(x * 128 / p->cw));
         p->cr[y * p->cw + x] = kind ? (unsigned char)(seed >> 12) : clip(200 - (int)(y * 128 / p->ch));
      }
}

static void release(picture_t *p)
{
   free(p->y);
   free(p->cb);
   free(p->cr);
}

/* rjpeg's rows, kept whole */
typedef struct
{
   unsigned char *rgba;
   unsigned       w, h;
} sink_t;

static bool sink_size(void *user, unsigned w, unsigned h)
{
   sink_t *s = (sink_t*)user;
   s->w = w;
   s->h = h;
   s->rgba = (unsigned char*)malloc((size_t)w * h * 4);
   return s->rgba != NULL;
}

static uint8_t *sink_row(void *user, unsigned y)
{
   sink_t *s = (sink_t*)user;
   return s->rgba + (size_t)y * s->w * 4;
}

static void sink_done(void *user, unsigned y, const uint8_t *px)
{
   sink_t *s = (sink_t*)user;
   if (px != s->rgba + (size_t)y * s->w * 4)
      memcpy(s->rgba + (size_t)y * s->w * 4, px, (size_t)s->w * 4);
}

/* PSNR of what rjpeg decodes against the picture taken to RGB with
 * JFIF's equations, chroma repeated */
static double decoded_psnr(const picture_t *p, const unsigned char *jpg, size_t n,
      bool c422)
{
   rjpeg_t *dec = rjpeg_alloc();
   sink_t   s;
   struct rjpeg_rows rows;
   unsigned w = 0, h = 0, x, y;
   double   se = 0;

   memset(&s, 0, sizeof(s));
   rows.size = sink_size;
   rows.row  = sink_row;
   rows.done = sink_done;
   rows.user = &s;
   if (!dec || !rjpeg_decode_rows(dec, jpg, n, true, &rows, &w, &h)
         || w != p->w || h != p->h)
   {
      rjpeg_free(dec);
      free(s.rgba);
      return 0;
   }
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         unsigned ci = (c422 ? y : y / 2) * p->cw + x / 2;
         double Y = p->y[y * w + x], Cb = p->cb[ci] - 128.0, Cr = p->cr[ci] - 128.0;
         double rgb[3];
         int k;
         rgb[0] = Y + 1.402 * Cr;
         rgb[1] = Y - 0.344136 * Cb - 0.714136 * Cr;
         rgb[2] = Y + 1.772 * Cb;
         for (k = 0; k < 3; k++)
         {
            double want = rgb[k] < 0 ? 0 : rgb[k] > 255 ? 255 : rgb[k];
            double d    = want - s.rgba[((size_t)y * w + x) * 4 + k];
            se += d * d;
         }
      }
   rjpeg_free(dec);
   free(s.rgba);
   se /= (double)w * h * 3;
   return se > 0 ? 10 * log10(255.0 * 255.0 / se) : 99;
}

static void test_case(const char *dir, unsigned w, unsigned h, bool c422,
      int kind, unsigned quality, double min_psnr, unsigned long want)
{
   static rjpeg_encoder_t e;
   picture_t      p;
   size_t         cap = rjpeg_encode_bound(w, h), n;
   unsigned char *out = (unsigned char*)malloc(cap);
   unsigned char *exact;
   double         psnr;
   unsigned long  hash;

   make(&p, w, h, c422, kind);
   rjpeg_encoder_init(&e, quality);
   n = rjpeg_encode(&e, out, cap, w, h,
         c422 ? RJPEG_ENCODE_422 : RJPEG_ENCODE_420,
         p.y, w, p.cb, p.cw, p.cr, p.cw);
   hash = n ? fnv(out, n) : 0;
   psnr = n ? decoded_psnr(&p, out, n, c422) : 0;
   printf("%ux%u %s %s q%u: %lu bytes, hash %08lx, PSNR %.2f\n", w, h,
         c422 ? "4:2:2" : "4:2:0", kind ? "noise" : "edges", quality,
         (unsigned long)n, hash, psnr);
   CHECK(n > 0);
   CHECK(psnr >= min_psnr);
   CHECK(!want || hash == want);

   /* a buffer exactly as big works, one byte short does not, and
    * nothing lands past either */
   if (n)
   {
      exact = (unsigned char*)malloc(n);
      CHECK(rjpeg_encode(&e, exact, n, w, h,
               c422 ? RJPEG_ENCODE_422 : RJPEG_ENCODE_420,
               p.y, w, p.cb, p.cw, p.cr, p.cw) == n);
      CHECK(!memcmp(exact, out, n));
      CHECK(rjpeg_encode(&e, exact, n - 1, w, h,
               c422 ? RJPEG_ENCODE_422 : RJPEG_ENCODE_420,
               p.y, w, p.cb, p.cw, p.cr, p.cw) == 0);
      free(exact);
   }

   if (dir && n)
   {
      char  path[1024];
      FILE *f;
      sprintf(path, "%.900s/%ux%u_%s_%d_q%u.jpg", dir, w, h, c422 ? "422" : "420", kind, quality);
      if ((f = fopen(path, "wb")))
      {
         fwrite(out, 1, n, f);
         fclose(f);
      }
   }
   release(&p);
   free(out);
}

int main(int argc, char **argv)
{
   const char *dir = argc > 1 ? argv[1] : NULL;

   test_case(dir, 352, 288, false, 0,  75, 38.0, 0x4ffe451cUL);
   test_case(dir, 352, 288, false, 0,  95, 42.0, 0x81051d6cUL);
   test_case(dir, 352, 288, false, 0,  20, 30.0, 0x12dc327bUL);
   test_case(dir, 333, 197, false, 0,  75, 36.0, 0xaa69fba9UL);
   test_case(dir, 333, 197, true,  0,  75, 36.0, 0xbbced5acUL);
   test_case(dir,  17,   9, false, 0,  90, 30.0, 0xe3550eb4UL);
   test_case(dir,   1,   1, false, 0,  90, 30.0, 0x1ed9e03eUL);
   test_case(dir, 128,  64, false, 1, 100, 12.0, 0xb50e30a3UL);
   test_case(dir, 130,  66, true,  1, 100, 12.0, 0x4e3b7a4dUL);

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("\nall checks passed\n");
   return failures ? 1 : 0;
}
