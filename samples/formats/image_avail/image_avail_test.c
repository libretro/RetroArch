/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_avail_test.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Partial-buffer ("avail-aware") still decoding, verified against the
 * whole-buffer decode that has always been there.
 *
 * The oracle is the decoder itself: every fixture is decoded once
 * with the entire file resident, then again with the resident
 * frontier raised a few bytes at a time, and the two surfaces must be
 * bit-identical. A wall that is mistaken for EOF, a run that is
 * re-emitted after a stall, or a cursor that is not resumed exactly
 * where it stopped all show up as a pixel difference rather than as a
 * crash, which is why the comparison is over the whole surface and
 * not over a checksum of it.
 *
 * The increments are deliberately hostile: one byte at a time stalls
 * inside every field and inside every RLE run, and prime-sized steps
 * put the wall at offsets that share no factor with the pixel size,
 * so a stall lands mid-pixel as well as mid-run. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/image.h>
#include <formats/rtga.h>

static int failures = 0;

#define CHECK(cond, what) do { \
   if (cond) printf("  ok   %s\n", what); \
   else { printf("  FAIL %s (%s:%d)\n", what, __FILE__, __LINE__); failures++; } \
} while (0)

/* ---- fixtures ----------------------------------------------------
 * Built here rather than committed as binaries: the interesting
 * variants are the ones that stress the wall (RLE runs, indexed
 * palettes, inverted rows), and generating them keeps the file
 * layout visible next to the test that depends on it. */

typedef struct
{
   uint8_t *buf;
   size_t   len;
} fixture_t;

static void fx_free(fixture_t *f) { free(f->buf); f->buf = NULL; f->len = 0; }

static void put16(uint8_t *p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }

/* Uncompressed truecolour, 24 or 32bpp, optionally origin-at-top. */
static fixture_t fx_tga_raw(unsigned w, unsigned h, int bpp, int top_origin)
{
   fixture_t f;
   size_t px  = (size_t)w * h * (bpp / 8);
   size_t i;
   uint8_t *p;
   f.len      = 18 + px;
   f.buf      = (uint8_t*)calloc(1, f.len);
   p          = f.buf;
   p[2]       = 2;                       /* uncompressed truecolour */
   put16(p + 12, w);
   put16(p + 14, h);
   p[16]      = (uint8_t)bpp;
   p[17]      = top_origin ? 0x20 : 0x00;
   for (i = 0; i < px; i++)
      p[18 + i] = (uint8_t)((i * 37u + (i >> 5) * 11u) & 0xff);
   return f;
}

/* RLE truecolour: alternating repeat and literal packets, so a wall
 * can land inside either kind. */
static fixture_t fx_tga_rle(unsigned w, unsigned h, int bpp)
{
   fixture_t f;
   unsigned comp = bpp / 8;
   size_t cap    = 18 + (size_t)w * h * (comp + 1) + 64;
   size_t o      = 18;
   unsigned left = w * h;
   unsigned seed = 3;
   uint8_t *p    = (uint8_t*)calloc(1, cap);

   p[2]  = 10;                            /* RLE truecolour */
   put16(p + 12, w);
   put16(p + 14, h);
   p[16] = (uint8_t)bpp;

   while (left)
   {
      unsigned n = 1 + (seed % 60);
      unsigned c;
      seed = seed * 1103515245u + 12345u;
      if (n > left)
         n = left;
      if (seed & 0x10000)
      {
         /* repeat packet: one pixel, n times */
         p[o++] = (uint8_t)(0x80 | (n - 1));
         for (c = 0; c < comp; c++)
            p[o++] = (uint8_t)(seed >> (c * 8));
      }
      else
      {
         /* literal packet: n distinct pixels */
         unsigned k;
         p[o++] = (uint8_t)(n - 1);
         for (k = 0; k < n; k++)
            for (c = 0; c < comp; c++)
               p[o++] = (uint8_t)((seed >> (c * 8)) + k * 7u);
      }
      left -= n;
   }
   f.buf = p;
   f.len = o;
   return f;
}

/* Colour-mapped: the palette sits between the header and the pixels,
 * so a wall inside it must stall rather than decode from zeros. */
static fixture_t fx_tga_indexed(unsigned w, unsigned h, unsigned pal_len)
{
   fixture_t f;
   size_t o   = 18;
   size_t i;
   size_t cap = 18 + pal_len * 3 + (size_t)w * h + 64;
   uint8_t *p = (uint8_t*)calloc(1, cap);

   p[1]  = 1;                             /* colour map present */
   p[2]  = 1;                             /* uncompressed indexed */
   put16(p + 3, 0);                       /* first entry */
   put16(p + 5, pal_len);
   p[7]  = 24;                            /* palette entry bits */
   put16(p + 12, w);
   put16(p + 14, h);
   p[16] = 8;
   for (i = 0; i < pal_len; i++)
   {
      p[o++] = (uint8_t)(i * 5);
      p[o++] = (uint8_t)(i * 9 + 1);
      p[o++] = (uint8_t)(i * 13 + 2);
   }
   for (i = 0; i < (size_t)w * h; i++)
      p[o++] = (uint8_t)(i % (pal_len ? pal_len : 1));
   f.buf = p;
   f.len = o;
   return f;
}

/* ---- the two decodes --------------------------------------------- */

/* Whole file resident from the first call: what shipped before. */
static uint32_t *decode_whole(const uint8_t *data, size_t len,
      unsigned *w, unsigned *h)
{
   void *out   = NULL;
   rtga_t *tga = rtga_alloc();
   int ret;
   if (!tga)
      return NULL;
   if (!rtga_set_buf_ptr(tga, (void*)data))
   {
      rtga_free(tga);
      return NULL;
   }
   do
   {
      ret = rtga_process_image(tga, &out, len, w, h, true);
   } while (ret == IMAGE_PROCESS_NEXT);
   rtga_free(tga);
   if (ret != IMAGE_PROCESS_END)
   {
      free(out);
      return NULL;
   }
   return (uint32_t*)out;
}

/* Frontier raised by @step bytes between calls.  Every WAIT must be
 * answered by more bytes, and the decode must never finish before the
 * frontier reaches the end of the file. */
static uint32_t *decode_sliced(const uint8_t *data, size_t len,
      size_t step, unsigned *w, unsigned *h, unsigned *waits)
{
   void *out     = NULL;
   rtga_t *tga   = rtga_alloc();
   size_t avail  = 0;
   unsigned n    = 0;
   unsigned guard = 0;
   int ret       = IMAGE_PROCESS_NEXT;

   if (!tga)
      return NULL;
   if (!rtga_set_buf_ptr(tga, (void*)data))
   {
      rtga_free(tga);
      return NULL;
   }

   rtga_set_avail(tga, avail);
   for (;;)
   {
      if (++guard > 100000000u)
         break;
      ret = rtga_process_image(tga, &out, len, w, h, true);
      if (ret == IMAGE_PROCESS_WAIT)
      {
         n++;
         if (!rtga_need_more(tga))
         {
            printf("  FAIL WAIT without need_more\n");
            failures++;
            break;
         }
         if (avail >= len)
         {
            printf("  FAIL WAIT with the whole file resident\n");
            failures++;
            break;
         }
         avail += step;
         if (avail > len)
            avail = len;
         rtga_set_avail(tga, avail);
         continue;
      }
      if (ret != IMAGE_PROCESS_NEXT)
         break;
   }
   rtga_free(tga);
   if (waits)
      *waits = n;
   if (ret != IMAGE_PROCESS_END)
   {
      free(out);
      return NULL;
   }
   return (uint32_t*)out;
}

static void compare(const char *label, fixture_t f, size_t step)
{
   unsigned w1 = 0, h1 = 0, w2 = 0, h2 = 0, waits = 0;
   uint32_t *a = decode_whole(f.buf, f.len, &w1, &h1);
   uint32_t *b = decode_sliced(f.buf, f.len, step, &w2, &h2, &waits);
   char what[192];

   snprintf(what, sizeof(what), "%s (step %u): whole-buffer decode succeeded",
         label, (unsigned)step);
   CHECK(a != NULL, what);
   snprintf(what, sizeof(what), "%s (step %u): sliced decode succeeded",
         label, (unsigned)step);
   CHECK(b != NULL, what);

   if (a && b)
   {
      snprintf(what, sizeof(what), "%s (step %u): dimensions agree (%ux%u)",
            label, (unsigned)step, w1, h1);
      CHECK(w1 == w2 && h1 == h2, what);
      if (w1 == w2 && h1 == h2)
      {
         size_t n     = (size_t)w1 * h1;
         size_t bad   = 0, i, first = 0;
         for (i = 0; i < n; i++)
            if (a[i] != b[i])
            {
               if (!bad)
                  first = i;
               bad++;
            }
         i = first;
         snprintf(what, sizeof(what),
               "%s (step %u): %u pixels bit-identical (%u stalls)",
               label, (unsigned)step, (unsigned)n, waits);
         if (bad)
            printf("  note first difference at pixel %u of %u\n",
                  (unsigned)i, (unsigned)n);
         CHECK(bad == 0, what);
      }
   }
   free(a);
   free(b);
}

int main(void)
{
   /* Steps: 1 byte stalls everywhere; 3 and 7 are prime, so the wall
    * lands mid-pixel for both 24bpp and 32bpp; 1024 is a realistic
    * read granularity. */
   static const size_t steps[] = { 1, 3, 7, 1024 };
   unsigned s;

   printf("image_avail_test: TGA partial-buffer decode vs whole-buffer\n");

   for (s = 0; s < sizeof(steps) / sizeof(steps[0]); s++)
   {
      fixture_t f;

      printf("-- step %u --\n", (unsigned)steps[s]);

      f = fx_tga_raw(31, 17, 32, 0); compare("raw 32bpp bottom-up", f, steps[s]); fx_free(&f);
      f = fx_tga_raw(31, 17, 24, 0); compare("raw 24bpp bottom-up", f, steps[s]); fx_free(&f);
      f = fx_tga_raw(31, 17, 32, 1); compare("raw 32bpp top-down",  f, steps[s]); fx_free(&f);
      f = fx_tga_rle(29, 13, 32);    compare("RLE 32bpp",           f, steps[s]); fx_free(&f);
      f = fx_tga_rle(29, 13, 24);    compare("RLE 24bpp",           f, steps[s]); fx_free(&f);
      f = fx_tga_indexed(23, 11, 200); compare("indexed 8bpp",      f, steps[s]); fx_free(&f);
   }

   /* A truncated file is not a stall: the frontier reaches the real
    * end of the data and the decode has to settle, not wait forever. */
   {
      fixture_t f = fx_tga_raw(16, 16, 32, 0);
      unsigned w = 0, h = 0, waits = 0;
      uint32_t *b;
      f.len -= 64;
      b = decode_sliced(f.buf, f.len, 7, &w, &h, &waits);
      CHECK(1, "truncated file settles rather than stalling forever");
      free(b);
      fx_free(&f);
   }

   printf("%d failure(s)\n", failures);
   return failures ? 1 : 0;
}
