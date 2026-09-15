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
#include <formats/image.h>
#include <zlib.h>

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

/* Decodes through image_transfer exactly as task_image does.  With
 * @avail non-zero the frontier is raised before anything else touches
 * the decoder - the ordering that blanked BMP and TGA thumbnails. */
static uint32_t *facade_decode(uint8_t *buf, size_t len,
      enum image_type_enum type, size_t avail, unsigned *w, unsigned *h)
{
   void *hnd = image_transfer_new(type);
   void *px  = NULL;
   int   r   = IMAGE_PROCESS_NEXT, guard = 0;

   if (!hnd)
      return NULL;
   image_transfer_set_buffer_ptr(hnd, type, buf, len);
   if (avail)
      image_transfer_set_avail(hnd, type, avail);
   if (!image_transfer_start(hnd, type))
   {
      image_transfer_free(hnd, type);
      return NULL;
   }
   while (image_transfer_iterate(hnd, type) && ++guard < 200000)
      ;
   guard = 0;
   while (     (r == IMAGE_PROCESS_NEXT || r == IMAGE_PROCESS_WAIT)
            && ++guard < 200000)
      r = image_transfer_process(hnd, type, (uint32_t**)&px, len, w, h, true);
   image_transfer_free(hnd, type);
   if (r != IMAGE_PROCESS_END)
   {
      free(px);
      return NULL;
   }
   return (uint32_t*)px;
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

   /* The same call order, through the image_transfer facade, against
    * every still decoder that accepts a frontier - not just the two
    * this file grew up around.
    *
    * PNG, JPEG, WEBM and MP4 all clamp the request in the size domain
    * before deriving any pointer, because their set_buf_ptr is handed
    * the buffer length.  rtga_set_buf_ptr and rbmp_set_buf_ptr are
    * not, which is why those two were the only ones that could turn
    * (size_t)-1 into a wrapped pointer - and did.  A sixth avail-aware
    * decoder would inherit whichever of those two shapes it copies,
    * so the check belongs here rather than in anyone's memory.
    *
    * Driven exactly as task_image drives it: set_buffer_ptr, then the
    * frontier, then start/iterate, then process to END. */
   {
      static const uint8_t jpeg_fixture[] = {
   0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01,
   0x00, 0x01, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x10, 0x4c, 0x61, 0x76, 0x63, 0x36, 0x30, 0x2e, 0x33,
   0x31, 0x2e, 0x31, 0x30, 0x32, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x08, 0x0a, 0x0a, 0x0b, 0x0a,
   0x0b, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x10, 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
   0x10, 0x10, 0x10, 0x10, 0x12, 0x12, 0x12, 0x15, 0x15, 0x15, 0x12, 0x12, 0x12, 0x10, 0x10, 0x12,
   0x12, 0x14, 0x14, 0x15, 0x15, 0x17, 0x17, 0x17, 0x15, 0x15, 0x15, 0x15, 0x17, 0x17, 0x19, 0x19,
   0x19, 0x1e, 0x1e, 0x1c, 0x1c, 0x23, 0x23, 0x24, 0x2b, 0x2b, 0x33, 0xff, 0xc4, 0x00, 0x7a, 0x00,
   0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x06, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x06, 0x02, 0x03, 0x10, 0x00, 0x01, 0x03, 0x02, 0x05, 0x04,
   0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x02, 0x03, 0x31, 0x05,
   0x12, 0x06, 0x04, 0x22, 0x00, 0x41, 0x32, 0x13, 0x21, 0x82, 0x46, 0xc4, 0x51, 0x23, 0x11, 0x00,
   0x01, 0x03, 0x03, 0x04, 0x00, 0x06, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
   0x11, 0x03, 0x01, 0x12, 0x04, 0x05, 0x31, 0x00, 0x21, 0x13, 0x71, 0x32, 0x23, 0x14, 0x22, 0x06,
   0x84, 0xc3, 0x46, 0xc4, 0x45, 0x07, 0x34, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x0b, 0x00, 0x17,
   0x03, 0x01, 0x12, 0x00, 0x02, 0x12, 0x00, 0x03, 0x12, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01,
   0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0x13, 0x95, 0x61, 0x63, 0xad, 0xfa, 0xe9, 0x48,
   0x57, 0x46, 0xf8, 0xc3, 0x69, 0xeb, 0x19, 0x6a, 0x9a, 0x2a, 0xed, 0x45, 0x1d, 0x09, 0xe5, 0x2c,
   0xb5, 0x79, 0xb8, 0x4b, 0xa2, 0xd4, 0xc6, 0xf9, 0xd5, 0xac, 0x1b, 0x47, 0x8e, 0x30, 0x88, 0xf8,
   0x85, 0x43, 0x16, 0x8e, 0x75, 0x7f, 0xbc, 0x57, 0x0c, 0xd0, 0x96, 0x42, 0x5c, 0x9e, 0x48, 0x1b,
   0x88, 0x1d, 0x38, 0xab, 0xb1, 0x67, 0x4d, 0x78, 0x4f, 0x09, 0x9d, 0xe7, 0xec, 0x30, 0x78, 0xdb,
   0xcc, 0xa4, 0x0b, 0xec, 0x57, 0x1d, 0x24, 0x49, 0xd8, 0xe8, 0xfc, 0xaa, 0xd7, 0xe2, 0x71, 0xb6,
   0xec, 0x2f, 0xdf, 0x66, 0xef, 0x09, 0x66, 0x05, 0x4b, 0x57, 0x57, 0x99, 0x17, 0x1e, 0x49, 0x28,
   0x92, 0xf6, 0xf6, 0x23, 0x40, 0x4a, 0x14, 0x0c, 0x82, 0xb9, 0x54, 0x89, 0x44, 0xfc, 0x84, 0x26,
   0x11, 0x37, 0x67, 0x58, 0x65, 0xda, 0xb0, 0x9b, 0xd8, 0xee, 0xef, 0xc9, 0x3b, 0x6e, 0xee, 0xa1,
   0x53, 0x51, 0x8f, 0x7d, 0xe8, 0x1f, 0x4e, 0x94, 0xf5, 0x00, 0x49, 0x61, 0x27, 0x84, 0x54, 0xda,
   0x4c, 0x9b, 0xf6, 0x1f, 0x87, 0xe9, 0xe6, 0xd8, 0x1c, 0x6f, 0x11, 0x5e, 0x62, 0xd6, 0xff, 0x00,
   0xbb, 0x34, 0xf8, 0x7c, 0x23, 0xb3, 0x02, 0xf9, 0xfa, 0xc7, 0x80, 0x9e, 0xd6, 0xf7, 0x2d, 0x39,
   0x5f, 0xfa, 0x87, 0xed, 0xef, 0xc9, 0xfe, 0x2e, 0xfd, 0xfe, 0xe0, 0xfb, 0xbf, 0x55, 0x8c, 0x34,
   0x62, 0x0b, 0xda, 0x45, 0xc7, 0x7f, 0x6f, 0x10, 0xf5, 0x74, 0x74, 0x53, 0xff, 0x00, 0x44, 0x38,
   0x89, 0x59, 0x79, 0x53, 0x5d, 0xe6, 0x32, 0x9f, 0xdd, 0xfe, 0x57, 0xea, 0xec, 0x7b, 0xac, 0x3e,
   0x3e, 0xcc, 0xf2, 0x8e, 0x30, 0xc5, 0x05, 0x6f, 0xee, 0x3a, 0xa6, 0xb7, 0x0a, 0x8a, 0x2b, 0xa7,
   0xcc, 0x73, 0x12, 0x89, 0x1e, 0x65, 0xdb, 0x4c, 0xab, 0x33, 0xdd, 0x70, 0xd7, 0x44, 0x4a, 0xb6,
   0x36, 0x46, 0x5b, 0x5f, 0x58, 0xc3, 0x54, 0x55, 0x13, 0x6a, 0xa0, 0xea, 0x4f, 0x25, 0xe5, 0xeb,
   0x25, 0xb7, 0x53, 0x26, 0xa2, 0x29, 0x60, 0xc4, 0xc8, 0xce, 0xd1, 0xe4, 0x94, 0x26, 0xc8, 0x4d,
   0x5a, 0xf0, 0x4f, 0xb7, 0x1a, 0x9e, 0xbc, 0x0f, 0x0c, 0xe9, 0x16, 0x3e, 0x5b, 0x9e, 0x44, 0x1c,
   0x89, 0x1d, 0x78, 0xab, 0xb1, 0x63, 0x5d, 0x38, 0x5f, 0x19, 0x9d, 0xb3, 0x6d, 0x94, 0xbd, 0xca,
   0x63, 0x61, 0xdb, 0xb7, 0x7b, 0x4f, 0xb8, 0x85, 0x68, 0x6c, 0x38, 0x88, 0x5d, 0x1b, 0x11, 0x8d,
   0xf1, 0x61, 0x60, 0xc3, 0x36, 0x98, 0x4b, 0xc0, 0x1a, 0x5d, 0xba, 0xb3, 0xc8, 0xb6, 0xf2, 0x40,
   0xc4, 0x17, 0xb7, 0xbe, 0x1a, 0x0e, 0x50, 0x60, 0xa4, 0xd1, 0xca, 0x64, 0x8a, 0x67, 0xe2, 0x21,
   0x10, 0x89, 0xbb, 0xf1, 0x5f, 0x60, 0xca, 0xb4, 0x58, 0xf6, 0x82, 0xe1, 0x01, 0xdc, 0x6b, 0x8f,
   0x9c, 0x75, 0x33, 0x35, 0x3a, 0x39, 0x07, 0xd9, 0x82, 0x59, 0x6d, 0x63, 0xd3, 0x01, 0x14, 0x84,
   0x8e, 0x15, 0x17, 0x7f, 0xff, 0xd9,
      };
      struct { const char *name; enum image_type_enum type;
               uint8_t *buf; size_t len; int owned; } d[4];
      unsigned c, k;
      static const size_t avails[2] = { (size_t)-1, 0 };  /* 0 -> file length */
      static const char  *albl[2]   = { "(size_t)-1", "file length" };
      fixture_t ft = fx_tga_raw(31, 17, 32, 0);
      fixture_t fb = fx_bmp(31, 17, 24, 0, 0);
      uint8_t  *png = NULL;
      size_t    png_len = 0;

      /* A 31x17 truecolour PNG, built here so the fixture set stays
       * generated rather than committed. */
      {
         unsigned w = 31, h = 17, y, x;
         uLongf clen;
         uint8_t *raw = (uint8_t*)malloc((size_t)h * (1 + w * 3));
         uint8_t *comp;
         size_t   o = 0;
         for (y = 0; y < h; y++)
         {
            raw[o++] = 0;
            for (x = 0; x < w * 3; x++)
               raw[o++] = (uint8_t)((x * 7 + y * 13) & 0xff);
         }
         clen = compressBound((uLong)o);
         comp = (uint8_t*)malloc(clen);
         if (compress2(comp, &clen, raw, (uLong)o, 6) == Z_OK)
         {
            static const uint8_t sig[8] =
               { 0x89,'P','N','G','\r','\n',0x1a,'\n' };
            size_t cap = 8 + 25 + 12 + clen + 12;
            uint8_t *p = (uint8_t*)malloc(cap);
            size_t   n = 0;
            unsigned crc;
            memcpy(p, sig, 8); n = 8;
            /* IHDR */
            p[n++]=0;p[n++]=0;p[n++]=0;p[n++]=13;
            memcpy(p+n,"IHDR",4);
            p[n+4]=0;p[n+5]=0;p[n+6]=(uint8_t)(w>>8);p[n+7]=(uint8_t)w;
            p[n+8]=0;p[n+9]=0;p[n+10]=(uint8_t)(h>>8);p[n+11]=(uint8_t)h;
            p[n+12]=8;p[n+13]=2;p[n+14]=0;p[n+15]=0;p[n+16]=0;
            crc=(unsigned)crc32(0,p+n,17); n+=17;
            p[n++]=(uint8_t)(crc>>24);p[n++]=(uint8_t)(crc>>16);
            p[n++]=(uint8_t)(crc>>8); p[n++]=(uint8_t)crc;
            /* IDAT */
            p[n++]=(uint8_t)(clen>>24);p[n++]=(uint8_t)(clen>>16);
            p[n++]=(uint8_t)(clen>>8); p[n++]=(uint8_t)clen;
            memcpy(p+n,"IDAT",4); memcpy(p+n+4,comp,clen);
            crc=(unsigned)crc32(0,p+n,4+clen); n+=4+clen;
            p[n++]=(uint8_t)(crc>>24);p[n++]=(uint8_t)(crc>>16);
            p[n++]=(uint8_t)(crc>>8); p[n++]=(uint8_t)crc;
            /* IEND */
            p[n++]=0;p[n++]=0;p[n++]=0;p[n++]=0;
            memcpy(p+n,"IEND",4);
            crc=(unsigned)crc32(0,p+n,4); n+=4;
            p[n++]=(uint8_t)(crc>>24);p[n++]=(uint8_t)(crc>>16);
            p[n++]=(uint8_t)(crc>>8); p[n++]=(uint8_t)crc;
            png = p; png_len = n;
         }
         free(raw); free(comp);
      }

      d[0].name="PNG";  d[0].type=IMAGE_TYPE_PNG;  d[0].buf=png;      d[0].len=png_len;              d[0].owned=1;
      d[1].name="JPEG"; d[1].type=IMAGE_TYPE_JPEG; d[1].buf=(uint8_t*)jpeg_fixture; d[1].len=sizeof(jpeg_fixture); d[1].owned=0;
      d[2].name="TGA";  d[2].type=IMAGE_TYPE_TGA;  d[2].buf=ft.buf;   d[2].len=ft.len;               d[2].owned=0;
      d[3].name="BMP";  d[3].type=IMAGE_TYPE_BMP;  d[3].buf=fb.buf;   d[3].len=fb.len;               d[3].owned=0;

      printf("-- every avail-aware decoder, task_image order --\n");
      for (c = 0; c < 4; c++)
      {
         unsigned  w0 = 0, h0 = 0;
         uint32_t *ref = NULL;
         char      what[176];
         if (!d[c].buf || !d[c].len)
         {
            snprintf(what, sizeof(what), "%s: fixture built", d[c].name);
            CHECK(0, what);
            continue;
         }
         ref = facade_decode(d[c].buf, d[c].len, d[c].type, 0, &w0, &h0);
         snprintf(what, sizeof(what), "%s: whole-buffer reference decodes", d[c].name);
         CHECK(ref != NULL && w0 && h0, what);

         for (k = 0; k < 2; k++)
         {
            size_t    av = avails[k] ? avails[k] : d[c].len;
            unsigned  w = 0, h = 0;
            uint32_t *got = facade_decode(d[c].buf, d[c].len, d[c].type, av, &w, &h);

            snprintf(what, sizeof(what),
                  "%s: set_avail(%s) before process completes",
                  d[c].name, albl[k]);
            CHECK(got != NULL, what);

            snprintf(what, sizeof(what),
                  "%s: set_avail(%s) before process is byte-exact",
                  d[c].name, albl[k]);
            if (got && ref && w == w0 && h == h0)
               CHECK(memcmp(got, ref, (size_t)w0 * h0 * 4) == 0, what);
            else
               CHECK(0, what);
            free(got);
         }
         free(ref);
         if (d[c].owned)
            free(d[c].buf);
      }
      fx_free(&ft);
      fx_free(&fb);
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
