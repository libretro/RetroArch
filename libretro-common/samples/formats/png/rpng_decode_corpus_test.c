/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_decode_corpus_test.c).
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

/* Decode-side corpus regression test.
 *
 * rpng_roundtrip_test.c already covers encode-then-decode, but that only
 * ever feeds the decoder images rpng itself wrote: colour type 2 or 6,
 * 8-bit, and whichever filter the encoder's heuristic happened to pick.
 * Whole decode paths - 1/2/4/16-bit depths, palette, grayscale, gray+
 * alpha, Adam7, and four of the five filter types on most rows - are
 * never reached that way.
 *
 * This test builds PNG streams itself, in memory, with the filter type
 * forced per scanline, so every (colour type, bit depth, filter, width)
 * combination is exercised and the expected pixels are known exactly
 * without needing a second decoder as an oracle.  Deflate comes from
 * libretro-common's own backend, so there is no external dependency.
 *
 * The cases here are the ones that have actually broken:
 *
 *  - Narrow widths.  A vectorised reverse filter runs its loop zero or
 *    one times at small widths and the scalar tail does the rest; a tail
 *    that assumed the vector loop had advanced past the first pixel read
 *    before the start of the scanline (found by ASan on a 1-pixel-wide
 *    16-bit grayscale image).
 *
 *  - Scanlines wider than the 32 KB inflate slice.  The decoder's driver
 *    stopped pulling once the compressed input was consumed, discarding
 *    output the codec still had buffered, and failed the whole image -
 *    which reached users as black screenshots for anything above about
 *    10900 pixels wide at RGB8.  A 16384-wide row is 49153 bytes.
 *
 *  - Every filter on every pixel stride.  The reverse filters are
 *    stride-generic SIMD; strides 1 through 8 all take different lane
 *    counts and different tail lengths.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <encodings/crc32.h>
#include <formats/rpng.h>
#include <formats/image.h>
#include <streams/trans_stream.h>

/* ------------------------------------------------------------------ */
/* PNG writer with a forced filter type                                */
/* ------------------------------------------------------------------ */

static const uint8_t png_sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };

struct buf
{
   uint8_t *p;
   size_t   len;
   size_t   cap;
};

static int buf_add(struct buf *b, const void *data, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t cap = (b->cap ? b->cap * 2 : 4096);
      uint8_t *np;
      while (cap < b->len + n)
         cap *= 2;
      if (!(np = (uint8_t*)realloc(b->p, cap)))
         return 0;
      b->p   = np;
      b->cap = cap;
   }
   memcpy(b->p + b->len, data, n);
   b->len += n;
   return 1;
}

static void be32(uint8_t *d, uint32_t v)
{
   d[0] = (uint8_t)(v >> 24); d[1] = (uint8_t)(v >> 16);
   d[2] = (uint8_t)(v >>  8); d[3] = (uint8_t)v;
}

static int chunk(struct buf *b, const char *tag, const uint8_t *data, size_t n)
{
   uint8_t hdr[8], crcb[4];
   uint32_t crc;
   be32(hdr, (uint32_t)n);
   memcpy(hdr + 4, tag, 4);
   crc = encoding_crc32(0, hdr + 4, 4);
   if (n)
      crc = encoding_crc32(crc, data, n);
   be32(crcb, crc);
   return buf_add(b, hdr, 8) && (n ? buf_add(b, data, n) : 1)
       && buf_add(b, crcb, 4);
}

static int paeth_ref(int a, int b, int c)
{
   int p  = a + b - c;
   int pa = p > a ? p - a : a - p;
   int pb = p > b ? p - b : b - p;
   int pc = p > c ? p - c : c - p;
   if (pa <= pb && pa <= pc)
      return a;
   if (pb <= pc)
      return b;
   return c;
}

/* Filter one scanline forward.  All inputs are raw bytes, so this is a
 * direct transcription of RFC 2083 section 6 with no state. */
static void filter_row(uint8_t *dst, const uint8_t *raw, const uint8_t *prev,
      size_t pitch, unsigned bpp, int ftype)
{
   size_t i;
   for (i = 0; i < pitch; i++)
   {
      int a = (i >= bpp) ? raw[i - bpp] : 0;
      int b = prev ? prev[i] : 0;
      int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
      switch (ftype)
      {
         case 0: dst[i] = raw[i]; break;
         case 1: dst[i] = (uint8_t)(raw[i] - a); break;
         case 2: dst[i] = (uint8_t)(raw[i] - b); break;
         case 3: dst[i] = (uint8_t)(raw[i] - ((a + b) >> 1)); break;
         default: dst[i] = (uint8_t)(raw[i] - paeth_ref(a, b, c)); break;
      }
   }
}

/* Deflate through libretro-common's own backend, so the test needs no
 * zlib of its own.  Level 1: the compression ratio is irrelevant here
 * and CI runs this at -O0 under sanitizers. */
static uint8_t *deflate_all(const uint8_t *in, size_t in_len, size_t *out_len)
{
   const struct trans_stream_backend *be =
      trans_stream_get_deflate_deflate_backend();
   void *st;
   uint8_t *out;
   uint32_t rd = 0, wn = 0;
   size_t cap = in_len + (in_len / 2) + 4096;
   enum trans_stream_error err = TRANS_STREAM_ERROR_NONE;

   if (!be || !(st = be->stream_new()))
      return NULL;
   if (!(out = (uint8_t*)malloc(cap)))
   {
      be->stream_free(st);
      return NULL;
   }
   be->define(st, "level", 1);
   be->set_in(st, in, (uint32_t)in_len);
   be->set_out(st, out, (uint32_t)cap);
   if (!be->trans(st, true, &rd, &wn, &err)
         || err != TRANS_STREAM_ERROR_NONE
         || rd != (uint32_t)in_len)
   {
      be->stream_free(st);
      free(out);
      return NULL;
   }
   be->stream_free(st);
   *out_len = wn;
   return out;
}

/* ------------------------------------------------------------------ */
/* Image model                                                         */
/* ------------------------------------------------------------------ */

/* Sample values are a deterministic function of position and channel so
 * the expected ARGB output can be recomputed without storing it. */
static unsigned sample_at(unsigned x, unsigned y, unsigned ch, unsigned maxv)
{
   unsigned v = (x * 37u + y * 101u + ch * 17u);
   v ^= (v >> 5);
   return v % (maxv + 1u);
}

struct fmt
{
   int      color_type;
   unsigned depth;
   unsigned channels;
   const char *name;
};

static const struct fmt formats[] = {
   { 0, 1,  1, "gray1"    },
   { 0, 2,  1, "gray2"    },
   { 0, 4,  1, "gray4"    },
   { 0, 8,  1, "gray8"    },
   { 0, 16, 1, "gray16"   },
   { 2, 8,  3, "rgb8"     },
   { 2, 16, 3, "rgb16"    },
   { 3, 1,  1, "plt1"     },
   { 3, 2,  1, "plt2"     },
   { 3, 4,  1, "plt4"     },
   { 3, 8,  1, "plt8"     },
   { 4, 8,  2, "graya8"   },
   { 4, 16, 2, "graya16"  },
   { 6, 8,  4, "rgba8"    },
   { 6, 16, 4, "rgba16"   }
};

static uint32_t palette_entry(unsigned i)
{
   return 0xff000000u
        | ((uint32_t)((i * 7u)  & 0xff) << 16)
        | ((uint32_t)((i * 13u) & 0xff) <<  8)
        |  (uint32_t)((i * 29u) & 0xff);
}

/* What rpng is expected to produce for one pixel, in the ARGB8888
 * layout its !supports_rgba path emits. */
static uint32_t expect_pixel(const struct fmt *f, unsigned x, unsigned y)
{
   unsigned maxv = (f->depth >= 8) ? 255u : ((1u << f->depth) - 1u);
   switch (f->color_type)
   {
      case 0:
      {
         /* Sub-8-bit grays are scaled to 8 bits by the replication
          * multiplier; 16-bit keeps the high byte. */
         static const unsigned mul[9] = { 0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
         unsigned v = sample_at(x, y, 0, maxv);
         if (f->depth < 8)
            v *= mul[f->depth];
         return 0xff000000u | (v * 0x010101u);
      }
      case 2:
      {
         unsigned r = sample_at(x, y, 0, maxv);
         unsigned g = sample_at(x, y, 1, maxv);
         unsigned b = sample_at(x, y, 2, maxv);
         return 0xff000000u | (r << 16) | (g << 8) | b;
      }
      case 3:
         return palette_entry(sample_at(x, y, 0, maxv));
      case 4:
      {
         unsigned g = sample_at(x, y, 0, maxv);
         unsigned a = sample_at(x, y, 1, maxv);
         return (a << 24) | (g * 0x010101u);
      }
      default:
      {
         unsigned r = sample_at(x, y, 0, maxv);
         unsigned g = sample_at(x, y, 1, maxv);
         unsigned b = sample_at(x, y, 2, maxv);
         unsigned a = sample_at(x, y, 3, maxv);
         return (a << 24) | (r << 16) | (g << 8) | b;
      }
   }
}

/* Pack one scanline of raw sample bytes for the given format. */
static void pack_row(uint8_t *dst, const struct fmt *f,
      unsigned y, unsigned width)
{
   unsigned x, c;
   unsigned maxv = (f->depth >= 8) ? 255u : ((1u << f->depth) - 1u);
   if (f->depth < 8)
   {
      unsigned per = 8u / f->depth;
      size_t   n   = ((size_t)width + per - 1) / per;
      memset(dst, 0, n);
      for (x = 0; x < width; x++)
      {
         unsigned v   = sample_at(x, y, 0, maxv);
         unsigned slot = x % per;
         dst[x / per] |= (uint8_t)(v << (8u - f->depth * (slot + 1u)));
      }
      return;
   }
   for (x = 0; x < width; x++)
      for (c = 0; c < f->channels; c++)
      {
         unsigned v = sample_at(x, y, c, maxv);
         if (f->depth == 16)
         {
            /* High byte carries the value the decoder keeps; the low
             * byte is deliberately different so a decoder that read the
             * wrong half would be caught. */
            dst[((size_t)x * f->channels + c) * 2 + 0] = (uint8_t)v;
            dst[((size_t)x * f->channels + c) * 2 + 1] = (uint8_t)(v ^ 0x5a);
         }
         else
            dst[(size_t)x * f->channels + c] = (uint8_t)v;
      }
}

static size_t row_bytes(const struct fmt *f, unsigned width)
{
   if (f->depth < 8)
   {
      unsigned per = 8u / f->depth;
      return ((size_t)width + per - 1) / per;
   }
   return (size_t)width * f->channels * (f->depth / 8);
}

static unsigned pixel_stride(const struct fmt *f)
{
   unsigned bpp = f->channels * f->depth / 8;
   return bpp ? bpp : 1;
}

/* Build a complete non-interlaced PNG with `ftype` forced on every row. */
static uint8_t *build_png(const struct fmt *f, unsigned width, unsigned height,
      int ftype, size_t *out_len)
{
   struct buf b;
   uint8_t ihdr[13];
   uint8_t *raw = NULL, *prev = NULL, *stream = NULL, *comp = NULL;
   uint8_t *plte = NULL;
   size_t pitch = row_bytes(f, width);
   size_t slen = 0, clen = 0;
   unsigned y, bpp = pixel_stride(f);
   int ok = 0;

   memset(&b, 0, sizeof(b));
   raw    = (uint8_t*)calloc(1, pitch ? pitch : 1);
   prev   = (uint8_t*)calloc(1, pitch ? pitch : 1);
   stream = (uint8_t*)malloc((pitch + 1) * (size_t)height);
   if (!raw || !prev || !stream)
      goto done;

   for (y = 0; y < height; y++)
   {
      uint8_t *row = stream + (size_t)y * (pitch + 1);
      pack_row(raw, f, y, width);
      row[0] = (uint8_t)ftype;
      filter_row(row + 1, raw, y ? prev : NULL, pitch, bpp, ftype);
      memcpy(prev, raw, pitch);
   }
   slen = (pitch + 1) * (size_t)height;

   be32(ihdr + 0, width);
   be32(ihdr + 4, height);
   ihdr[8]  = (uint8_t)f->depth;
   ihdr[9]  = (uint8_t)f->color_type;
   ihdr[10] = 0;
   ihdr[11] = 0;
   ihdr[12] = 0;

   if (!buf_add(&b, png_sig, sizeof(png_sig)))
      goto done;
   if (!chunk(&b, "IHDR", ihdr, sizeof(ihdr)))
      goto done;
   if (f->color_type == 3)
   {
      unsigned n = 1u << f->depth, i;
      if (!(plte = (uint8_t*)malloc((size_t)n * 3)))
         goto done;
      for (i = 0; i < n; i++)
      {
         uint32_t e = palette_entry(i);
         plte[i * 3 + 0] = (uint8_t)(e >> 16);
         plte[i * 3 + 1] = (uint8_t)(e >> 8);
         plte[i * 3 + 2] = (uint8_t)e;
      }
      if (!chunk(&b, "PLTE", plte, (size_t)n * 3))
         goto done;
   }
   if (!(comp = deflate_all(stream, slen, &clen)))
      goto done;
   if (!chunk(&b, "IDAT", comp, clen))
      goto done;
   if (!chunk(&b, "IEND", NULL, 0))
      goto done;
   ok = 1;

done:
   free(raw); free(prev); free(stream); free(comp); free(plte);
   if (!ok)
   {
      free(b.p);
      return NULL;
   }
   *out_len = b.len;
   return b.p;
}

/* Adam7: seven passes of (x offset, y offset, x stride, y stride).  Each
 * pass is an independent sub-image with its own filter state and its own
 * scanline width, which is why the decoder has a separate driver for it -
 * and why nothing above reaches that driver. */
static const unsigned adam7[7][4] = {
   { 0, 0, 8, 8 }, { 4, 0, 8, 8 }, { 0, 4, 4, 8 }, { 2, 0, 4, 4 },
   { 0, 2, 2, 4 }, { 1, 0, 2, 2 }, { 0, 1, 1, 2 }
};

static unsigned pass_count(unsigned total, unsigned offset, unsigned stride)
{
   if (total <= offset)
      return 0;
   return (total - offset + stride - 1) / stride;
}

/* Pack one scanline of a pass, sampling the full-image grid at the
 * pass's stride so the expected pixels stay the same function of
 * position that expect_pixel() uses. */
static void pack_pass_row(uint8_t *dst, const struct fmt *f,
      unsigned pass, unsigned sy, unsigned pw)
{
   unsigned i, c;
   unsigned maxv = (f->depth >= 8) ? 255u : ((1u << f->depth) - 1u);
   unsigned xo = adam7[pass][0], yo = adam7[pass][1];
   unsigned xs = adam7[pass][2], ys = adam7[pass][3];
   unsigned y  = yo + sy * ys;
   if (f->depth < 8)
   {
      unsigned per = 8u / f->depth;
      size_t   n   = ((size_t)pw + per - 1) / per;
      memset(dst, 0, n);
      for (i = 0; i < pw; i++)
      {
         unsigned v    = sample_at(xo + i * xs, y, 0, maxv);
         unsigned slot = i % per;
         dst[i / per] |= (uint8_t)(v << (8u - f->depth * (slot + 1u)));
      }
      return;
   }
   for (i = 0; i < pw; i++)
      for (c = 0; c < f->channels; c++)
      {
         unsigned v = sample_at(xo + i * xs, y, c, maxv);
         if (f->depth == 16)
         {
            dst[((size_t)i * f->channels + c) * 2 + 0] = (uint8_t)v;
            dst[((size_t)i * f->channels + c) * 2 + 1] = (uint8_t)(v ^ 0x5a);
         }
         else
            dst[(size_t)i * f->channels + c] = (uint8_t)v;
      }
}

/* Build an interlaced PNG with `ftype` forced on every row of every
 * pass.  Empty passes contribute nothing to the stream, which is itself
 * a case worth covering: at small widths several of the seven are
 * empty. */
static uint8_t *build_png_adam7(const struct fmt *f, unsigned width,
      unsigned height, int ftype, size_t *out_len)
{
   struct buf b;
   uint8_t ihdr[13];
   uint8_t *raw = NULL, *prev = NULL, *stream = NULL, *comp = NULL;
   uint8_t *plte = NULL;
   size_t maxpitch = row_bytes(f, width) + 8;
   size_t slen = 0, clen = 0, cap;
   unsigned pass, bpp = pixel_stride(f);
   int ok = 0;

   memset(&b, 0, sizeof(b));
   cap    = (maxpitch + 1) * (size_t)height * 2 + 64;
   raw    = (uint8_t*)calloc(1, maxpitch);
   prev   = (uint8_t*)calloc(1, maxpitch);
   stream = (uint8_t*)malloc(cap);
   if (!raw || !prev || !stream)
      goto done;

   for (pass = 0; pass < 7; pass++)
   {
      unsigned pw = pass_count(width,  adam7[pass][0], adam7[pass][2]);
      unsigned ph = pass_count(height, adam7[pass][1], adam7[pass][3]);
      size_t   pitch;
      unsigned sy;
      if (!pw || !ph)
         continue;
      pitch = row_bytes(f, pw);
      memset(prev, 0, pitch);
      for (sy = 0; sy < ph; sy++)
      {
         uint8_t *row = stream + slen;
         pack_pass_row(raw, f, pass, sy, pw);
         row[0] = (uint8_t)ftype;
         filter_row(row + 1, raw, sy ? prev : NULL, pitch, bpp, ftype);
         memcpy(prev, raw, pitch);
         slen += pitch + 1;
      }
   }

   be32(ihdr + 0, width);
   be32(ihdr + 4, height);
   ihdr[8]  = (uint8_t)f->depth;
   ihdr[9]  = (uint8_t)f->color_type;
   ihdr[10] = 0;
   ihdr[11] = 0;
   ihdr[12] = 1;   /* Adam7 */

   if (!buf_add(&b, png_sig, sizeof(png_sig)))
      goto done;
   if (!chunk(&b, "IHDR", ihdr, sizeof(ihdr)))
      goto done;
   if (f->color_type == 3)
   {
      unsigned n = 1u << f->depth, i;
      if (!(plte = (uint8_t*)malloc((size_t)n * 3)))
         goto done;
      for (i = 0; i < n; i++)
      {
         uint32_t e = palette_entry(i);
         plte[i * 3 + 0] = (uint8_t)(e >> 16);
         plte[i * 3 + 1] = (uint8_t)(e >> 8);
         plte[i * 3 + 2] = (uint8_t)e;
      }
      if (!chunk(&b, "PLTE", plte, (size_t)n * 3))
         goto done;
   }
   if (!(comp = deflate_all(stream, slen, &clen)))
      goto done;
   if (!chunk(&b, "IDAT", comp, clen))
      goto done;
   if (!chunk(&b, "IEND", NULL, 0))
      goto done;
   ok = 1;

done:
   free(raw); free(prev); free(stream); free(comp); free(plte);
   if (!ok)
   {
      free(b.p);
      return NULL;
   }
   *out_len = b.len;
   return b.p;
}

/* ------------------------------------------------------------------ */
/* Decode and compare                                                  */
/* ------------------------------------------------------------------ */

static uint32_t *decode(const uint8_t *png, size_t len,
      unsigned *w, unsigned *h)
{
   rpng_t   *rpng = rpng_alloc();
   uint32_t *data = NULL;
   int ret;
   if (!rpng)
      return NULL;
   if (!rpng_set_buf_ptr(rpng, (void*)png, len) || !rpng_start(rpng))
      goto fail;
   while (rpng_iterate_image(rpng));
   if (!rpng_is_valid(rpng))
      goto fail;
   do
   {
      ret = rpng_process_image(rpng, (void**)&data, len, w, h, false);
   } while (ret == IMAGE_PROCESS_NEXT);
   if (ret == IMAGE_PROCESS_ERROR || ret == IMAGE_PROCESS_ERROR_END)
      goto fail;
   rpng_free(rpng);
   return data;
fail:
   rpng_free(rpng);
   free(data);
   return NULL;
}

static int failures;

static void check_image_mode(const struct fmt *f, unsigned width,
      unsigned height, int ftype, int interlaced)
{
   size_t len = 0;
   uint8_t *png = interlaced
      ? build_png_adam7(f, width, height, ftype, &len)
      : build_png(f, width, height, ftype, &len);
   uint32_t *got;
   unsigned dw = 0, dh = 0, x, y;

   if (!png)
   {
      printf("FAIL %-8s w=%-5u f=%d i=%d: could not build test image\n",
            f->name, width, ftype, interlaced);
      failures++;
      return;
   }
   if (!(got = decode(png, len, &dw, &dh)))
   {
      printf("FAIL %-8s w=%-5u h=%u f=%d: decode failed\n",
            f->name, width, height, ftype);
      failures++;
      free(png);
      return;
   }
   if (dw != width || dh != height)
   {
      printf("FAIL %-8s w=%-5u f=%d: got %ux%u\n",
            f->name, width, ftype, dw, dh);
      failures++;
      free(png); free(got);
      return;
   }
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
      {
         uint32_t want = expect_pixel(f, x, y);
         uint32_t have = got[(size_t)y * width + x];
         if (want != have)
         {
            printf("FAIL %-8s w=%-5u f=%d: pixel (%u,%u) want %08x got %08x\n",
                  f->name, width, ftype, x, y,
                  (unsigned)want, (unsigned)have);
            failures++;
            free(png); free(got);
            return;
         }
      }
   free(png); free(got);
}

static void check_image(const struct fmt *f, unsigned width, unsigned height,
      int ftype)
{
   check_image_mode(f, width, height, ftype, 0);
}

int main(void)
{
   /* Widths 1..20 put the vector loops at zero or one iteration with a
    * tail of every possible length; the rest straddle the 4-, 8- and
    * 16-byte boundaries the kernels step by. */
   static const unsigned widths[] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
      31, 33, 63, 65, 127, 257
   };
   unsigned fi, wi;
   int ft;
   size_t nfmt   = sizeof(formats) / sizeof(formats[0]);
   size_t nwidth = sizeof(widths)  / sizeof(widths[0]);
   unsigned cases = 0;

   for (fi = 0; fi < nfmt; fi++)
      for (wi = 0; wi < nwidth; wi++)
         for (ft = 0; ft < 5; ft++)
         {
            check_image(&formats[fi], widths[wi], 3, ft);
            cases++;
         }
   printf("%u format/width/filter combinations checked\n", cases);

   /* Scanlines wider than the 32 KB inflate slice: 16384 RGB8 pixels is
    * 49153 bytes with the filter byte.  This is the shape that decoded
    * as black before 551ff75. */
   {
      static const unsigned wide_fmts[] = { 5, 13 }; /* rgb8, rgba8 */
      unsigned i;
      for (i = 0; i < 2; i++)
      {
         check_image(&formats[wide_fmts[i]], 16384, 4, 1);
         check_image(&formats[wide_fmts[i]], 16384, 4, 4);
         check_image(&formats[wide_fmts[i]], 11000, 4, 4);
      }
      printf("6 wide-scanline images checked\n");
   }

   /* Adam7.  Widths below 8 leave several of the seven passes empty,
    * and heights below 8 leave others with a single row, so the small
    * sizes here are as much of the point as the large ones. */
   {
      static const unsigned iw[] = { 1, 2, 3, 5, 7, 8, 9, 15, 16, 17, 33, 64, 129 };
      static const unsigned ih[] = { 1, 2, 3, 5, 8, 9, 16, 17 };
      unsigned n = 0;
      for (fi = 0; fi < nfmt; fi++)
         for (wi = 0; wi < sizeof(iw) / sizeof(iw[0]); wi++)
         {
            unsigned hi = wi % (sizeof(ih) / sizeof(ih[0]));
            for (ft = 0; ft < 5; ft++)
            {
               check_image_mode(&formats[fi], iw[wi], ih[hi], ft, 1);
               n++;
            }
         }
      printf("%u interlaced combinations checked\n", n);
   }

   if (failures)
   {
      printf("rpng_decode_corpus_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("rpng_decode_corpus_test: all passed\n");
   return 0;
}
