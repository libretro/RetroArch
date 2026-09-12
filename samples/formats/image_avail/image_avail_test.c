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
#include <formats/rbmp.h>

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

/* BMP: BITMAPINFOHEADER, rows bottom-up unless @top_down, each row
 * padded to a four-byte boundary. */
static fixture_t fx_bmp(unsigned w, unsigned h, int bpp, int top_down,
      unsigned pal_len)
{
   fixture_t f;
   unsigned  row   = ((w * bpp + 31) / 32) * 4;   /* padded row size */
   unsigned  polen = pal_len * 4;
   unsigned  off   = 14 + 40 + polen;
   size_t    len   = off + (size_t)row * h;
   uint8_t  *p     = (uint8_t*)calloc(1, len);
   unsigned  i, y, x;

   p[0] = 'B'; p[1] = 'M';
   p[2] = (uint8_t)len; p[3] = (uint8_t)(len >> 8);
   p[4] = (uint8_t)(len >> 16); p[5] = (uint8_t)(len >> 24);
   p[10] = (uint8_t)off; p[11] = (uint8_t)(off >> 8);
   p[14] = 40;                                    /* DIB header size */
   p[18] = (uint8_t)w; p[19] = (uint8_t)(w >> 8);
   if (top_down)
   {
      int nh = -(int)h;
      p[22] = (uint8_t)nh;         p[23] = (uint8_t)(nh >> 8);
      p[24] = (uint8_t)(nh >> 16); p[25] = (uint8_t)(nh >> 24);
   }
   else
   {
      p[22] = (uint8_t)h; p[23] = (uint8_t)(h >> 8);
   }
   p[26] = 1;                                     /* planes */
   p[28] = (uint8_t)bpp;
   p[46] = (uint8_t)pal_len;                      /* colours used */

   for (i = 0; i < pal_len; i++)
   {
      p[54 + i * 4 + 0] = (uint8_t)(i * 7);
      p[54 + i * 4 + 1] = (uint8_t)(i * 11 + 3);
      p[54 + i * 4 + 2] = (uint8_t)(i * 5 + 9);
   }
   for (y = 0; y < h; y++)
      for (x = 0; x < row; x++)
         p[off + (size_t)y * row + x] =
            (uint8_t)((y * 131u + x * 37u + (x >> 3) * 17u) & 0xff);

   f.buf = p;
   f.len = len;
   return f;
}

/* ---- the two decodes --------------------------------------------- */

typedef enum { CODEC_TGA, CODEC_BMP } codec_t;
static codec_t codec = CODEC_TGA;

/* Whole file resident from the first call: what shipped before. */
static uint32_t *decode_whole(const uint8_t *data, size_t len,
      unsigned *w, unsigned *h)
{
   void *out   = NULL;
   int ret;
   if (codec == CODEC_BMP)
   {
      rbmp_t *bmp = rbmp_alloc();
      if (!bmp)
         return NULL;
      if (!rbmp_set_buf_ptr(bmp, (void*)data))
      {
         rbmp_free(bmp);
         return NULL;
      }
      do
      {
         ret = rbmp_process_image(bmp, &out, len, w, h, true);
      } while (ret == IMAGE_PROCESS_NEXT);
      rbmp_free(bmp);
      if (ret != IMAGE_PROCESS_END)
      {
         free(out);
         return NULL;
      }
      return (uint32_t*)out;
   }
   {
   rtga_t *tga = rtga_alloc();
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
}

/* Frontier raised by @step bytes between calls.  Every WAIT must be
 * answered by more bytes, and the decode must never finish before the
 * frontier reaches the end of the file. */
static uint32_t *decode_sliced(const uint8_t *data, size_t len,
      size_t step, unsigned *w, unsigned *h, unsigned *waits)
{
   void *out     = NULL;
   rtga_t *tga   = (codec == CODEC_TGA) ? rtga_alloc() : NULL;
   rbmp_t *bmp   = (codec == CODEC_BMP) ? rbmp_alloc() : NULL;
   size_t avail  = 0;
   unsigned n    = 0;
   unsigned guard = 0;
   int ret       = IMAGE_PROCESS_NEXT;

   if (!tga && !bmp)
      return NULL;
   if (tga && !rtga_set_buf_ptr(tga, (void*)data))
   {
      rtga_free(tga);
      return NULL;
   }
   if (bmp && !rbmp_set_buf_ptr(bmp, (void*)data))
   {
      rbmp_free(bmp);
      return NULL;
   }

   if (tga) rtga_set_avail(tga, avail); else rbmp_set_avail(bmp, avail);
   for (;;)
   {
      if (++guard > 100000000u)
         break;
      ret = tga ? rtga_process_image(tga, &out, len, w, h, true)
                : rbmp_process_image(bmp, &out, len, w, h, true);
      if (ret == IMAGE_PROCESS_WAIT)
      {
         n++;
         if (!(tga ? rtga_need_more(tga) : rbmp_need_more(bmp)))
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
         if (tga) rtga_set_avail(tga, avail); else rbmp_set_avail(bmp, avail);
         continue;
      }
      if (ret != IMAGE_PROCESS_NEXT)
         break;
   }
   if (tga) rtga_free(tga); else rbmp_free(bmp);
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

   printf("image_avail_test: TGA/BMP partial-buffer decode vs whole-buffer\n");

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

      codec = CODEC_BMP;
      /* Rows are bottom-up by default, so a prefix of the file is the
       * bottom of the image: a stall that resumes on the wrong row
       * shows up as a vertically displaced surface. */
      f = fx_bmp(31, 17, 32, 0, 0);   compare("BMP 32bpp bottom-up", f, steps[s]); fx_free(&f);
      f = fx_bmp(31, 17, 24, 0, 0);   compare("BMP 24bpp bottom-up", f, steps[s]); fx_free(&f);
      f = fx_bmp(31, 17, 24, 1, 0);   compare("BMP 24bpp top-down",  f, steps[s]); fx_free(&f);
      f = fx_bmp(23, 11, 16, 0, 0);   compare("BMP 16bpp",           f, steps[s]); fx_free(&f);
      f = fx_bmp(23, 11,  8, 0, 200); compare("BMP 8bpp indexed",    f, steps[s]); fx_free(&f);
      f = fx_bmp(22, 11,  4, 0, 16);  compare("BMP 4bpp indexed",    f, steps[s]); fx_free(&f);
      codec = CODEC_TGA;
   }

   /* header_ready is the gate the task layer uses to decide a decode
    * can start early.  It has to agree with the decoder: any prefix
    * it accepts must be one the decoder can actually begin on, and it
    * must accept eventually rather than never (which would silently
    * restore whole-file loading).  Both directions are checked by
    * walking the prefix length up one byte at a time. */
   {
      struct { const char *name; fixture_t f; int bmp; } cases[6];
      unsigned c;
      cases[0].name = "TGA raw 32bpp";   cases[0].f = fx_tga_raw(31, 17, 32, 0);   cases[0].bmp = 0;
      cases[1].name = "TGA RLE 24bpp";   cases[1].f = fx_tga_rle(29, 13, 24);      cases[1].bmp = 0;
      cases[2].name = "TGA indexed";     cases[2].f = fx_tga_indexed(23, 11, 200); cases[2].bmp = 0;
      cases[3].name = "BMP 24bpp";       cases[3].f = fx_bmp(31, 17, 24, 0, 0);    cases[3].bmp = 1;
      cases[4].name = "BMP 8bpp";        cases[4].f = fx_bmp(23, 11, 8, 0, 200);   cases[4].bmp = 1;
      cases[5].name = "BMP 4bpp";        cases[5].f = fx_bmp(22, 11, 4, 0, 16);    cases[5].bmp = 1;

      printf("-- header_ready agrees with the decoder --\n");
      for (c = 0; c < 6; c++)
      {
         fixture_t f  = cases[c].f;
         size_t   n   = 0;
         size_t   first_ready = 0;
         bool     ok  = true;
         char     what[160];

         codec = cases[c].bmp ? CODEC_BMP : CODEC_TGA;

         for (n = 0; n <= f.len; n++)
         {
            bool r = cases[c].bmp ? rbmp_header_ready(f.buf, n)
                                  : rtga_header_ready(f.buf, n);
            if (r && !first_ready)
               first_ready = n;
            /* Monotonic: more bytes may not un-ready a file. */
            if (first_ready && !r)
               ok = false;
         }
         snprintf(what, sizeof(what), "%s: header_ready is monotonic in the prefix length", cases[c].name);
         CHECK(ok, what);
         snprintf(what, sizeof(what), "%s: header_ready eventually accepts (at %u of %u bytes)",
               cases[c].name, (unsigned)first_ready, (unsigned)f.len);
         CHECK(first_ready != 0, what);
         snprintf(what, sizeof(what), "%s: accepts before the whole file (%u < %u)",
               cases[c].name, (unsigned)first_ready, (unsigned)f.len);
         CHECK(first_ready > 0 && first_ready < f.len, what);

         /* The prefix it accepts must be one the decoder can start
          * on: opening at exactly that frontier must not error. */
         if (first_ready)
         {
            void *out = NULL;
            unsigned w = 0, h = 0;
            int ret;
            if (cases[c].bmp)
            {
               rbmp_t *b = rbmp_alloc();
               rbmp_set_buf_ptr(b, f.buf);
               rbmp_set_avail(b, first_ready);
               ret = rbmp_process_image(b, &out, f.len, &w, &h, true);
               rbmp_free(b);
            }
            else
            {
               rtga_t *t = rtga_alloc();
               rtga_set_buf_ptr(t, f.buf);
               rtga_set_avail(t, first_ready);
               ret = rtga_process_image(t, &out, f.len, &w, &h, true);
               rtga_free(t);
            }
            free(out);
            snprintf(what, sizeof(what),
                  "%s: decoder starts at the frontier header_ready accepted", cases[c].name);
            CHECK(ret != IMAGE_PROCESS_ERROR, what);
         }
         fx_free(&cases[c].f);
      }
      codec = CODEC_TGA;
   }

   /* The task layer raises the frontier BEFORE the first process()
    * call, and when a file finishes reading before any decode has run
    * it raises it to (size_t)-1 outright (task_image.c: "the frontier
    * is now the whole buffer").  At that moment the decoder does not
    * know the buffer length, so a set_avail that turns the request
    * into a pointer there computes buff_data + SIZE_MAX and wraps;
    * every later comparison against that end is nonsense, the header
    * check sees a negative length and waits for bytes that already
    * arrived.  That shipped: BMP and TGA thumbnails went blank in the
    * file browser, and small files - every file-browser icon - take
    * this path every time.
    *
    * Both orderings must decode exactly as the untouched path does. */
   {
      struct { const char *name; fixture_t f; int bmp; } cs[4];
      unsigned c;
      cs[0].name = "TGA raw 32bpp"; cs[0].f = fx_tga_raw(31, 17, 32, 0);   cs[0].bmp = 0;
      cs[1].name = "TGA indexed";   cs[1].f = fx_tga_indexed(23, 11, 200); cs[1].bmp = 0;
      cs[2].name = "BMP 24bpp";     cs[2].f = fx_bmp(31, 17, 24, 0, 0);    cs[2].bmp = 1;
      cs[3].name = "BMP 8bpp";      cs[3].f = fx_bmp(23, 11, 8, 0, 200);   cs[3].bmp = 1;

      printf("-- frontier set before the first process() call --\n");
      for (c = 0; c < 4; c++)
      {
         fixture_t f = cs[c].f;
         size_t    avails[2];
         unsigned  k, w0 = 0, h0 = 0;
         uint32_t *ref;

         codec = cs[c].bmp ? CODEC_BMP : CODEC_TGA;
         ref   = decode_whole(f.buf, f.len, &w0, &h0);

         avails[0] = (size_t)-1;   /* what task_image actually passes */
         avails[1] = f.len;        /* the honest equivalent           */

         for (k = 0; k < 2; k++)
         {
            void    *out = NULL;
            unsigned w = 0, h = 0;
            int      ret = IMAGE_PROCESS_NEXT, guard = 0;
            char     what[176];

            if (cs[c].bmp)
            {
               rbmp_t *b = rbmp_alloc();
               rbmp_set_buf_ptr(b, f.buf);
               rbmp_set_avail(b, avails[k]);      /* before any process */
               while (ret == IMAGE_PROCESS_NEXT && ++guard < 100000)
                  ret = rbmp_process_image(b, &out, f.len, &w, &h, true);
               rbmp_free(b);
            }
            else
            {
               rtga_t *t = rtga_alloc();
               rtga_set_buf_ptr(t, f.buf);
               rtga_set_avail(t, avails[k]);
               while (ret == IMAGE_PROCESS_NEXT && ++guard < 100000)
                  ret = rtga_process_image(t, &out, f.len, &w, &h, true);
               rtga_free(t);
            }

            snprintf(what, sizeof(what),
                  "%s: set_avail(%s) before process still completes",
                  cs[c].name, k ? "len" : "(size_t)-1");
            CHECK(ret == IMAGE_PROCESS_END, what);

            snprintf(what, sizeof(what),
                  "%s: set_avail(%s) before process is byte-exact",
                  cs[c].name, k ? "len" : "(size_t)-1");
            if (ret == IMAGE_PROCESS_END && ref && w == w0 && h == h0)
            {
               size_t n = (size_t)w0 * h0, i, bad = 0;
               for (i = 0; i < n; i++)
                  if (((uint32_t*)out)[i] != ref[i])
                     bad++;
               CHECK(bad == 0, what);
            }
            else
               CHECK(0, what);
            free(out);
         }
         free(ref);
         fx_free(&cs[c].f);
      }
      codec = CODEC_TGA;
   }

   /* Replay of task_image's actual call sequence, rather than an
    * order this test invented.
    *
    * That distinction is the whole reason the blank-thumbnail
    * regression shipped: every check here raised the frontier only
    * AFTER the first process() call, which is not how the caller
    * drives it.  task_image does, in order:
    *
    *   set_buffer_ptr(ptr, len)      once the read starts
    *   set_avail(done)               BEFORE any process(), each tick
    *   process() ...                 until END, WAIT means come back
    *   set_avail((size_t)-1)         the moment the read completes,
    *                                 which for a small file is before
    *                                 any process() has run at all
    *
    * The read granularity is the variable: a file that arrives in one
    * chunk hits the completion raise first, a file that dribbles in
    * gets many partial raises.  Both must land on the same pixels as
    * the untouched whole-buffer decode. */
   {
      struct { const char *name; fixture_t f; int bmp; } cs[4];
      static const size_t chunks[] = { 0, 1, 64, 4096 };
      unsigned c, q;

      cs[0].name = "TGA raw 32bpp"; cs[0].f = fx_tga_raw(31, 17, 32, 0);   cs[0].bmp = 0;
      cs[1].name = "TGA RLE 24bpp"; cs[1].f = fx_tga_rle(29, 13, 24);      cs[1].bmp = 0;
      cs[2].name = "BMP 24bpp";     cs[2].f = fx_bmp(31, 17, 24, 0, 0);    cs[2].bmp = 1;
      cs[3].name = "BMP 8bpp";      cs[3].f = fx_bmp(23, 11, 8, 0, 200);   cs[3].bmp = 1;

      printf("-- task_image call sequence replayed --\n");
      for (c = 0; c < 4; c++)
      {
         fixture_t f  = cs[c].f;
         unsigned  w0 = 0, h0 = 0;
         uint32_t *ref;

         codec = cs[c].bmp ? CODEC_BMP : CODEC_TGA;
         ref   = decode_whole(f.buf, f.len, &w0, &h0);

         for (q = 0; q < sizeof(chunks) / sizeof(chunks[0]); q++)
         {
            size_t   chunk = chunks[q];   /* 0 = whole file in one go */
            size_t   done  = chunk ? 0 : f.len;
            void    *out   = NULL;
            unsigned w = 0, h = 0;
            int      ret   = IMAGE_PROCESS_NEXT, guard = 0;
            bool     finished = (chunk == 0);
            char     what[192];
            rbmp_t  *b = NULL;
            rtga_t  *t = NULL;

            if (cs[c].bmp)
            {
               b = rbmp_alloc();
               rbmp_set_buf_ptr(b, f.buf);
            }
            else
            {
               t = rtga_alloc();
               rtga_set_buf_ptr(t, f.buf);
            }

            /* The completion raise happens the instant the read ends,
             * which for chunk == 0 is before the first process(). */
            if (finished)
            {
               if (b) rbmp_set_avail(b, (size_t)-1);
               else   rtga_set_avail(t, (size_t)-1);
            }

            while (ret == IMAGE_PROCESS_NEXT || ret == IMAGE_PROCESS_WAIT)
            {
               if (++guard > 200000)
                  break;
               /* Per-tick raise, before process, exactly as the
                * status machine does it. */
               if (!finished)
               {
                  done += chunk;
                  if (done >= f.len)
                  {
                     done     = f.len;
                     finished = true;
                     if (b) rbmp_set_avail(b, (size_t)-1);
                     else   rtga_set_avail(t, (size_t)-1);
                  }
                  else
                  {
                     if (b) rbmp_set_avail(b, done);
                     else   rtga_set_avail(t, done);
                  }
               }
               ret = b ? rbmp_process_image(b, &out, f.len, &w, &h, true)
                       : rtga_process_image(t, &out, f.len, &w, &h, true);
            }
            if (b) rbmp_free(b); else rtga_free(t);

            snprintf(what, sizeof(what), "%s: read in %s completes",
                  cs[c].name,
                  chunk ? (chunk == 1 ? "1-byte reads"
                        : (chunk == 64 ? "64-byte reads" : "4 KiB reads"))
                        : "one chunk");
            CHECK(ret == IMAGE_PROCESS_END, what);

            snprintf(what, sizeof(what), "%s: read in %s is byte-exact",
                  cs[c].name,
                  chunk ? (chunk == 1 ? "1-byte reads"
                        : (chunk == 64 ? "64-byte reads" : "4 KiB reads"))
                        : "one chunk");
            if (ret == IMAGE_PROCESS_END && ref && w == w0 && h == h0)
            {
               size_t n = (size_t)w0 * h0, i, bad = 0;
               for (i = 0; i < n; i++)
                  if (((uint32_t*)out)[i] != ref[i])
                     bad++;
               CHECK(bad == 0, what);
            }
            else
               CHECK(0, what);
            free(out);
         }
         free(ref);
         fx_free(&cs[c].f);
      }
      codec = CODEC_TGA;
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
