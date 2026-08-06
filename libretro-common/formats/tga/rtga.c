/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtga.c).
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

/* rtga -- Targa (TGA) decoder (modified version of stb_image's TGA
 * sources).
 *
 * What it implements: image types 1 (palettised), 2 (truecolour) and
 * 3 (greyscale), both raw and RLE-compressed variants, at 8/16/24/32
 * bits per pixel, with palettes of 8-32 bits per entry, either row
 * origin, and 32-bit RGBA output.
 *
 * What it does not implement: the extension and developer areas of
 * TGA 2.0 (footer metadata is ignored), interleaved (legacy
 * two/four-way) scanline order, and encoding. */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h> /* ptrdiff_t on osx */
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* INT_MAX, SIZE_MAX via stdint */

#include <retro_inline.h>
#include <retro_endianness.h> /* MSB_FIRST: gates the LE memcpy fast path */

#include <formats/image.h>
#include <formats/rtga.h>

typedef struct
{
   uint8_t *img_buffer;
   uint8_t *img_buffer_end;
   uint8_t *img_buffer_original;
   int buflen;
   int img_n, img_out_n;
   uint32_t img_x, img_y;
   uint8_t buffer_start[128];
} rtga_context;

struct rtga
{
   uint8_t       *buff_data;
   uint32_t      *output_image;
   rtga_context   s;             /* stream cursor, latched at begin */
   /* Indexed palettes only: a TGA index is one byte, so entries past
    * 255 can never be addressed and a fixed cache covers every
    * reachable entry.  Holding it here rather than in a heap block
    * also keeps it out of the pixel loop's alias set. */
   unsigned char palette[256 * 4];
   int width, height, comp, inverted;
   int indexed, is_RLE, bits_per_pixel;
   int palette_len, palette_start, palette_bits;
   int swap_rb;                  /* supports_rgba latched at begin */
   int phase;
   int row;                      /* fast path resume point */
   int pixel_count, pixel_i;     /* generic path resume point */
   int cur_col, cur_row;
   int RLE_count, RLE_repeat, read_next;
   unsigned char raw_data[4];
   uint32_t pal32[256];          /* indexed, non-RLE: entry -> pixel */
};

static INLINE uint8_t rtga_get8(rtga_context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;
   return 0;
}

static void rtga_skip(rtga_context *s, int n)
{
   ptrdiff_t remaining;
   if (n < 0)
   {
      s->img_buffer = s->img_buffer_end;
      return;
   }
   /* Clamp the advance to the remaining input.  Pre-patch a large
    * attacker-supplied offset (TGA header byte 1, or palette
    * start) pushed img_buffer past img_buffer_end, which is
    * pointer arithmetic outside the allocated object (UB per C99).
    * All callers of rtga_get8 check "buffer < buffer_end" so the
    * clamped state parses as EOF and the subsequent header checks
    * or indexed-palette code fail cleanly. */
   remaining = s->img_buffer_end - s->img_buffer;
   if ((ptrdiff_t)n > remaining)
      s->img_buffer = s->img_buffer_end;
   else
      s->img_buffer += n;
}

static int rtga_get16le(rtga_context *s)
{
   /* Sequenced explicitly.  Combining the two reads in one expression
    * (rtga_get8(s) + (rtga_get8(s) << 8)) leaves no sequence point
    * between them, so their order is unspecified: a compiler free to
    * evaluate the high byte first swaps them, and since this reader
    * supplies the frame's width and height a 320x240 image is decoded
    * as 16385x61440 - reported as success, with the garbage dimensions
    * then driving a multi-gigabyte allocation.  It works today only by
    * the order GCC happens to pick; building with -fsanitize=undefined
    * already flips it. */
   int lo = rtga_get8(s);
   int hi = rtga_get8(s);
   return lo + (hi << 8);
}

/* --- sliced decode ------------------------------------------------ *
 * rtga_begin parses the header, loads any palette and allocates the
 * surface; each later call fills a bounded run and the handle carries
 * the resume point.  The two loop bodies below are the originals
 * verbatim - only the enclosing loops changed - so the locals they read
 * are aliased out of the handle on entry and the mutated ones written
 * back on exit. */

#define RTGA_PHASE_IDLE    0
#define RTGA_PHASE_FAST    1
#define RTGA_PHASE_GENERIC 2
#define RTGA_PHASE_INDEXED 3

/* Output texels per call.  TGA costs a fraction of what the
 * block-compressed formats do per texel, so a budget four times rdds's
 * lands in the same fraction of a millisecond: the slowest variants
 * here (RLE and indexed, ~280 Mtexel/s) come to roughly 0.23 ms. */
#define RTGA_TEXELS_PER_CALL 65536

/* Precompute each reachable palette entry as the finished pixel word,
 * applying exactly the rules the generic loop would: clamp an
 * out-of-range index to 0, gather palette_bits/8 bytes, then assemble
 * by tga_comp.  Note tga_comp is palette_bits/8, so 15- and 16-bit
 * palettes take the <3 branch and replicate byte 0 - preserved here
 * rather than corrected, so the two paths agree. */
static void rtga_build_pal32(rtga_t *tga, bool supports_rgba)
{
   int tga_comp         = tga->comp;
   int tga_palette_bits = tga->palette_bits;
   int tga_palette_len  = tga->palette_len;
   int e;

   for (e = 0; e < 256; e++)
   {
      unsigned char raw_data[4] = {0};
      unsigned char b, g, r, a;
      int pal_idx = e;
      int j;

      if (pal_idx >= tga_palette_len)
         pal_idx = 0;
      pal_idx *= tga_palette_bits / 8;
      for (j = 0; j * 8 < tga_palette_bits; ++j)
         raw_data[j] = tga->palette[pal_idx + j];

      if (tga_comp >= 3)
      {
         b = raw_data[0];
         g = raw_data[1];
         r = raw_data[2];
         a = (tga_comp >= 4) ? raw_data[3] : 0xFF;
      }
      else
      {
         r = g = b = raw_data[0];
         a = (tga_comp >= 2) ? raw_data[1] : 0xFF;
      }

      if (supports_rgba)
         tga->pal32[e] = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                       | ((uint32_t)g << 8)  | (uint32_t)r;
      else
         tga->pal32[e] = ((uint32_t)a << 24) | ((uint32_t)r << 16)
                       | ((uint32_t)g << 8)  | (uint32_t)b;
   }
}

/* Indexed, non-RLE: one byte in, one table lookup out. */
static void rtga_indexed_pixels(rtga_t *tga, int npix)
{
   rtga_context ctx;
   rtga_context *s     = &ctx;
   uint32_t *output    = tga->output_image;
   const uint32_t *p32 = tga->pal32;
   int width           = tga->width;
   int height          = tga->height;
   int inverted        = tga->inverted;
   int cur_col         = tga->cur_col;
   int cur_row         = tga->cur_row;
   int i               = tga->pixel_i;
   int last            = i + npix;

   ctx.img_buffer          = tga->s.img_buffer;
   ctx.img_buffer_end      = tga->s.img_buffer_end;
   ctx.img_buffer_original = tga->s.img_buffer_original;

   if (last > tga->pixel_count)
      last = tga->pixel_count;

   for (; i < last; ++i)
   {
      int dst_row = inverted ? (height - 1 - cur_row) : cur_row;
      output[(size_t)dst_row * (size_t)width + (size_t)cur_col] =
         p32[rtga_get8(s)];
      if (++cur_col >= width)
      {
         cur_col = 0;
         ++cur_row;
      }
   }

   tga->pixel_i = i;
   tga->cur_col = cur_col;
   tga->cur_row = cur_row;
   tga->s.img_buffer = ctx.img_buffer;
}

static bool rtga_begin(rtga_t *tga, bool supports_rgba)
{
   rtga_context *s         = &tga->s;
   /* Read in the TGA header stuff */
   int tga_offset          = rtga_get8(s);
   int tga_indexed         = rtga_get8(s);
   int tga_image_type      = rtga_get8(s);
   int tga_is_RLE          = 0;
   int tga_palette_start   = rtga_get16le(s);
   int tga_palette_len     = rtga_get16le(s);
   int tga_palette_bits    = rtga_get8(s);
   int tga_x_origin        = rtga_get16le(s);
   int tga_y_origin        = rtga_get16le(s);
   int tga_width           = rtga_get16le(s);
   int tga_height          = rtga_get16le(s);
   int tga_bits_per_pixel  = rtga_get8(s);
   int tga_comp            = tga_bits_per_pixel / 8;
   int tga_inverted        = rtga_get8(s);

   /* Output buffer — always 32bpp ARGB or ABGR */
   uint32_t *output        = NULL;

   (void)tga_palette_start;
   (void)tga_x_origin;
   (void)tga_y_origin;

   /*   do a tiny bit of precessing */
   if (tga_image_type >= 8)
   {
      tga_image_type -= 8;
      tga_is_RLE = 1;
   }

   /* int tga_alpha_bits = tga_inverted & 15; */
   tga_inverted = 1 - ((tga_inverted >> 5) & 1);

   /*   error check */
   if (
         (tga_width < 1) || (tga_height < 1) ||
         (tga_image_type < 1) || (tga_image_type > 3) ||
         (
          (tga_bits_per_pixel != 8)  && (tga_bits_per_pixel != 16) &&
          (tga_bits_per_pixel != 24) && (tga_bits_per_pixel != 32)
         )
      )
      return false;

   /*   If paletted, then we will use the number of bits from the palette.
    *
    *   tga_palette_bits is attacker-controlled (TGA byte 7, 0..255).
    *   Pre-patch the indexed-read loop below did
    *       for (j = 0; j * 8 < tga_palette_bits; ++j)
    *           raw_data[j] = tga_palette[pal_idx + j];
    *   raw_data is a 4-byte stack array, so tga_palette_bits > 32
    *   wrote past the end of raw_data -- a stack buffer overflow of
    *   up to 28 bytes, directly driven by the TGA header.  Reject
    *   bogus palette_bits / empty palettes here and everything
    *   downstream runs with bounded buffers. */
   if (tga_indexed)
   {
      if (    tga_palette_len < 1
           || (    tga_palette_bits != 15
                && tga_palette_bits != 16
                && tga_palette_bits != 24
                && tga_palette_bits != 32))
         return false;
      tga_comp = tga_palette_bits / 8;
   }

   /* Bound the output allocation.  TGA dimensions are attacker-
    * controlled 16-bit values (max 65535), so their product is
    * up to ~4.29 G pixels and the decoded RGBA up to 16 GiB.
    *
    * On a 32-bit host that product wraps: (size_t)w * h * 4 for
    * 65535x65535 truncates to a small positive size_t and the
    * per-pixel decode then runs off the undersized malloc.  A
    * ceiling is genuinely required there, and 0x4000 (1 GiB of
    * decoded RGBA) keeps the allocation far from wrap territory
    * while matching the cap rjpeg and rwebp apply on 32-bit.
    *
    * On a 64-bit host no wrap is possible - the full 16 GiB product
    * fits size_t with room to spare - so the same constant would be
    * pure policy, and "larger than any real-world asset" is not a
    * safe assumption to make on a user's behalf: large scans and
    * renders exist, and refusing them outright means no thumbnail at
    * all.  Let the allocation decide instead; a request the host
    * cannot satisfy fails at malloc and is handled below. */
#if SIZE_MAX <= 0xFFFFFFFFu
   if (tga_width > 0x4000 || tga_height > 0x4000)
      return false;
#endif
   output = (uint32_t*)malloc(
         (size_t)tga_width * (size_t)tga_height * sizeof(uint32_t));
   if (!output)
      return false;
   tga->output_image = output;

   /* skip to the data's starting position (offset usually = 0) */
   rtga_skip(s, tga_offset);

   tga->width          = tga_width;
   tga->height         = tga_height;
   tga->comp           = tga_comp;
   tga->inverted       = tga_inverted;
   tga->indexed        = tga_indexed;
   tga->is_RLE         = tga_is_RLE;
   tga->bits_per_pixel = tga_bits_per_pixel;
   tga->palette_len    = tga_palette_len;
   tga->palette_start  = tga_palette_start;
   tga->palette_bits   = tga_palette_bits;
   tga->swap_rb        = supports_rgba ? 1 : 0;
   tga->row            = 0;

   if (!tga_indexed && !tga_is_RLE && tga_comp >= 3)
   {
      tga->phase = RTGA_PHASE_FAST;
      return true;
   }

   {
      int i, j;
      int RLE_repeating          = 0;
      int RLE_count              = 0;
      int read_next_pixel        = 1;
      unsigned char raw_data[4]  = {0};
      unsigned char *tga_palette = NULL;
      int pixel_count            = tga_width * tga_height;
      int cur_col                = 0;
      int cur_row                = 0;

      /* Load palette if indexed.  Header-level checks above have
       * ensured tga_palette_len >= 1 and tga_palette_bits in
       * {15,16,24,32}, so n is positive and bounded at
       * 65535 * 32 / 8 = 262140 bytes -- fits comfortably in int
       * and in the input length we've already accepted. */
      if (tga_indexed)
      {
         int n, keep;
         rtga_skip(s, tga_palette_start);
         n = tga_palette_len * tga_palette_bits / 8;
         if (s->img_buffer_end - s->img_buffer < (ptrdiff_t)n)
            return false;
         /* Cache the addressable prefix; the cursor still advances over
          * the whole declared palette. */
         keep = (int)sizeof(tga->palette);
         if (n < keep)
            keep = n;
         memset(tga->palette, 0, sizeof(tga->palette));
         memcpy(tga->palette, s->img_buffer, (size_t)keep);
         s->img_buffer += n;
         tga_palette    = tga->palette;
      }

      (void)tga_palette;
      tga->pixel_count = pixel_count;
      tga->pixel_i     = 0;
      tga->cur_col     = cur_col;
      tga->cur_row     = cur_row;
      tga->RLE_count   = RLE_count;
      tga->RLE_repeat  = RLE_repeating;
      tga->read_next   = read_next_pixel;
      memcpy(tga->raw_data, raw_data, sizeof(raw_data));
      (void)i;
      (void)j;
   }
   if (tga->indexed && !tga->is_RLE)
   {
      rtga_build_pal32(tga, supports_rgba);
      tga->phase = RTGA_PHASE_INDEXED;
      return true;
   }
   tga->phase = RTGA_PHASE_GENERIC;
   return true;
}

/* Fast path: uncompressed, non-indexed, 24/32-bit; whole rows. */
static void rtga_fast_rows(rtga_t *tga, int nrows)
{
   /* The cursor is copied to a local rather than used in place: with
    * 's' pointing into the handle, the compiler cannot prove a store
    * through 'output' misses the handle's own fields and reloads the
    * cursor on every pixel.  Aliasing it out and back measured a large
    * fraction of the decode.  Only the three pointers are copied -
    * rtga_context's 128-byte buffer_start tail is unused by this
    * decoder. */
   rtga_context ctx;
   rtga_context *s    = &ctx;
   uint32_t *output   = tga->output_image;
   int tga_width      = tga->width;
   int tga_height     = tga->height;
   int tga_comp       = tga->comp;
   int tga_inverted   = tga->inverted;
   bool supports_rgba = tga->swap_rb ? true : false;
   int row            = tga->row;
   int last           = row + nrows;

   ctx.img_buffer          = tga->s.img_buffer;
   ctx.img_buffer_end      = tga->s.img_buffer_end;
   ctx.img_buffer_original = tga->s.img_buffer_original;

   if (last > tga_height)
      last = tga_height;

   for (; row < last; ++row)
   {
         int dst_row     = tga_inverted ? (tga_height - 1 - row) : row;
         /* size_t index so dst_row * tga_width does not overflow
          * signed int for a legitimate 65535 x 65535 image - the
          * same widening the indexed path below already does. */
         uint32_t *dst   = output + (size_t)dst_row * (size_t)tga_width;
         int bytes_needed = tga_width * tga_comp;
         int col;

         if (s->img_buffer + bytes_needed > s->img_buffer_end)
            break;

         if (tga_comp == 4)
         {
            const uint8_t *src = s->img_buffer;
            if (supports_rgba)
            {
               /* TGA BGRA bytes → ABGR uint32: just memcpy on little-endian
                * since BGRA bytes = uint32 ARGB... no wait:
                * bytes [B,G,R,A] as little-endian uint32 = A<<24|R<<16|G<<8|B = ARGB.
                * We need ABGR = A<<24|B<<16|G<<8|R.
                * So we still need to swap R and B. */
               for (col = 0; col < tga_width; ++col)
               {
                  uint8_t b = src[0], g = src[1], r = src[2], a = src[3];
                  dst[col] = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                           | ((uint32_t)g << 8)  | (uint32_t)r;
                  src += 4;
               }
            }
            else
            {
               /* !supports_rgba wants each pixel as the uint32 value
                * ARGB = A<<24|R<<16|G<<8|B.
                * TGA stores bytes [B,G,R,A]; on a little-endian host those
                * bytes reinterpreted as a uint32 already equal ARGB, so a
                * straight memcpy is correct and fast. On a big-endian host
                * the same bytes would read as BGRA, so we must build the
                * ARGB value explicitly by shifting. */
#ifndef MSB_FIRST
               /* Direct memcpy! (little-endian) */
               memcpy(dst, src, tga_width * 4);
#else
               for (col = 0; col < tga_width; ++col)
               {
                  uint8_t b = src[0], g = src[1], r = src[2], a = src[3];
                  dst[col] = ((uint32_t)a << 24) | ((uint32_t)r << 16)
                           | ((uint32_t)g << 8)  | (uint32_t)b;
                  src += 4;
               }
#endif
            }
         }
         else /* tga_comp == 3 */
         {
            const uint8_t *src = s->img_buffer;
            if (supports_rgba)
            {
               for (col = 0; col < tga_width; ++col)
               {
                  uint8_t b = src[0], g = src[1], r = src[2];
                  dst[col] = 0xFF000000u | ((uint32_t)b << 16)
                           | ((uint32_t)g << 8)  | (uint32_t)r;
                  src += 3;
               }
            }
            else
            {
               for (col = 0; col < tga_width; ++col)
               {
                  uint8_t b = src[0], g = src[1], r = src[2];
                  dst[col] = 0xFF000000u | ((uint32_t)r << 16)
                           | ((uint32_t)g << 8)  | (uint32_t)b;
                  src += 3;
               }
            }
         }

         s->img_buffer += bytes_needed;
   }

   /* The body's only early exit is the truncation check, so stopping
    * short of 'last' means the file ran out.  The rows it could not
    * supply have never been written, and the surface came from malloc,
    * so leaving it here hands the caller uninitialised heap - decoding
    * the same short file twice returns different pixels.  Fill them
    * with what the loop itself would have produced from zero bytes,
    * which is also what the generic path yields on a short file, since
    * rtga_get8 reads zero past the end: transparent for 32-bit,
    * opaque black for 24-bit.  Under inversion the unwritten rows are
    * the low ones rather than the high ones, but either way they are
    * contiguous. */
   if (row < last)
   {
      size_t   n    = (size_t)(tga_height - row) * (size_t)tga_width;
      uint32_t fill = (tga_comp == 4) ? 0u : 0xFF000000u;
      uint32_t *p   = tga_inverted
                    ? output
                    : output + (size_t)row * (size_t)tga_width;
      size_t   k;
      for (k = 0; k < n; k++)
         p[k] = fill;
      row = tga_height;
   }

   tga->row              = row;
   tga->s.img_buffer     = ctx.img_buffer;
}

/* Generic path: RLE, indexed, or grayscale; bounded pixel run. */
static void rtga_generic_pixels(rtga_t *tga, int npix)
{
   /* The cursor is copied to a local rather than used in place: with
    * 's' pointing into the handle, the compiler cannot prove a store
    * through 'output' misses the handle's own fields and reloads the
    * cursor on every pixel.  Aliasing it out and back measured a large
    * fraction of the decode.  Only the three pointers are copied -
    * rtga_context's 128-byte buffer_start tail is unused by this
    * decoder. */
   rtga_context ctx;
   rtga_context *s            = &ctx;
   uint32_t *output           = tga->output_image;
   int tga_width              = tga->width;
   int tga_height             = tga->height;
   int tga_comp               = tga->comp;
   int tga_inverted           = tga->inverted;
   int tga_indexed            = tga->indexed;
   int tga_is_RLE             = tga->is_RLE;
   int tga_bits_per_pixel     = tga->bits_per_pixel;
   int tga_palette_len        = tga->palette_len;
   int tga_palette_bits       = tga->palette_bits;
   unsigned char pal[256 * 4];
   unsigned char *tga_palette = pal;
   bool supports_rgba         = tga->swap_rb ? true : false;
   int RLE_repeating          = tga->RLE_repeat;
   int RLE_count              = tga->RLE_count;
   int read_next_pixel        = tga->read_next;
   unsigned char raw_data[4];
   int cur_col                = tga->cur_col;
   int cur_row                = tga->cur_row;
   int i                      = tga->pixel_i;
   int j;
   int last                   = i + npix;

   ctx.img_buffer          = tga->s.img_buffer;
   ctx.img_buffer_end      = tga->s.img_buffer_end;
   ctx.img_buffer_original = tga->s.img_buffer_original;
   memcpy(raw_data, tga->raw_data, sizeof(raw_data));
   if (tga_indexed)
      memcpy(pal, tga->palette, sizeof(pal));
   if (last > tga->pixel_count)
      last = tga->pixel_count;

   for (; i < last; ++i)
   {
         int dst_row;
         uint32_t pixel;
         unsigned char b, g, r, a;

         /* RLE handling */
         if (tga_is_RLE)
         {
            if (RLE_count == 0)
            {
               int RLE_cmd     = rtga_get8(s);
               RLE_count       = 1 + (RLE_cmd & 127);
               RLE_repeating   = RLE_cmd >> 7;
               read_next_pixel = 1;
            }
            else if (!RLE_repeating)
               read_next_pixel = 1;
         }
         else
            read_next_pixel = 1;

         /* Read raw pixel data */
         if (read_next_pixel)
         {
            if (tga_indexed)
            {
               int pal_idx = rtga_get8(s);
               if (pal_idx >= tga_palette_len)
                  pal_idx = 0;
               pal_idx *= tga_palette_bits / 8;
               for (j = 0; j * 8 < tga_palette_bits; ++j)
                  raw_data[j] = tga_palette[pal_idx + j];
            }
            else
            {
               j = 0;
               switch (tga_bits_per_pixel)
               {
                  case 32:
                     raw_data[j++] = rtga_get8(s); /* fallthrough */
                  case 24:
                     raw_data[j++] = rtga_get8(s); /* fallthrough */
                  case 16:
                     raw_data[j++] = rtga_get8(s); /* fallthrough */
                  case  8:
                     raw_data[j++] = rtga_get8(s);
               }
            }
            read_next_pixel = 0;
         }

         /* Assemble pixel in correct byte order */
         if (tga_comp >= 3)
         {
            b = raw_data[0];
            g = raw_data[1];
            r = raw_data[2];
            a = (tga_comp >= 4) ? raw_data[3] : 0xFF;
         }
         else
         {
            r = g = b = raw_data[0];
            a = (tga_comp >= 2) ? raw_data[1] : 0xFF;
         }

         if (supports_rgba)
            pixel = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                  | ((uint32_t)g << 8)  | (uint32_t)r;
         else
            pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16)
                  | ((uint32_t)g << 8)  | (uint32_t)b;

         /* Write to correct position using tracked row/col
          * (avoids per-pixel division and modulo).  Use size_t for
          * the index so dst_row * tga_width does not overflow
          * signed int for a legitimate 65535 x 65535 image. */
         dst_row = tga_inverted ? (tga_height - 1 - cur_row) : cur_row;
         output[(size_t)dst_row * (size_t)tga_width + (size_t)cur_col] = pixel;

         if (++cur_col >= tga_width)
         {
            cur_col = 0;
            ++cur_row;
         }

         --RLE_count;
   }

   tga->pixel_i    = i;
   tga->cur_col    = cur_col;
   tga->cur_row    = cur_row;
   tga->RLE_count  = RLE_count;
   tga->RLE_repeat = RLE_repeating;
   tga->read_next  = read_next_pixel;
   memcpy(tga->raw_data, raw_data, sizeof(raw_data));
   tga->s.img_buffer = ctx.img_buffer;
   (void)tga_height;
}

/* Abandon an in-flight decode.  The END path clears output_image
 * first, transferring ownership, so this only frees what nobody
 * received. */
static void rtga_proc_reset(rtga_t *rtga)
{
   if (rtga->phase != RTGA_PHASE_IDLE)
   {
      free(rtga->output_image);
      rtga->output_image = NULL;
   }
   rtga->phase   = RTGA_PHASE_IDLE;
}

int rtga_process_image(rtga_t *rtga, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   if (!rtga || !buf_data)
      return IMAGE_PROCESS_ERROR;

   if (rtga->phase == RTGA_PHASE_IDLE)
   {
      *buf_data = NULL;
      /* Reject sizes that don't fit in int before casting.  A TGA
       * file larger than INT_MAX handed to an int-taking API would
       * truncate, the truncated value (potentially negative)
       * propagated to img_buffer_end = buffer + len, producing
       * pointer arithmetic outside the source object (UB).  A 2 GiB
       * TGA is unreasonable; reject early. */
      if (size > (size_t)INT_MAX)
         return IMAGE_PROCESS_ERROR;
      if (!rtga->buff_data)
         return IMAGE_PROCESS_ERROR;

      rtga->s.img_buffer          = rtga->buff_data;
      rtga->s.img_buffer_original = rtga->buff_data;
      rtga->s.img_buffer_end      = rtga->buff_data + (int)size;

      if (!rtga_begin(rtga, supports_rgba))
      {
         rtga_proc_reset(rtga);
         return IMAGE_PROCESS_ERROR;
      }
      *width  = (unsigned)rtga->width;
      *height = (unsigned)rtga->height;
      return IMAGE_PROCESS_NEXT;
   }

   *width  = (unsigned)rtga->width;
   *height = (unsigned)rtga->height;

   if (rtga->phase == RTGA_PHASE_FAST)
   {
      int rows = (rtga->width > 0)
               ? (RTGA_TEXELS_PER_CALL / rtga->width) : rtga->height;
      if (rows < 1)
         rows = 1;
      rtga_fast_rows(rtga, rows);
      if (rtga->row < rtga->height)
         return IMAGE_PROCESS_NEXT;
   }
   else
   {
      if (rtga->phase == RTGA_PHASE_INDEXED)
         rtga_indexed_pixels(rtga, RTGA_TEXELS_PER_CALL);
      else
         rtga_generic_pixels(rtga, RTGA_TEXELS_PER_CALL);
      if (rtga->pixel_i < rtga->pixel_count)
         return IMAGE_PROCESS_NEXT;
   }

   *buf_data          = rtga->output_image;
   rtga->output_image = NULL;   /* ownership -> caller */
   rtga->phase        = RTGA_PHASE_IDLE;
   return IMAGE_PROCESS_END;
}

bool rtga_set_buf_ptr(rtga_t *rtga, void *data)
{
   if (!rtga)
      return false;

   /* Repointing invalidates any decode still in flight. */
   rtga_proc_reset(rtga);
   rtga->buff_data = (uint8_t*)data;

   return true;
}

void rtga_free(rtga_t *rtga)
{
   if (!rtga)
      return;

   /* A finished decode handed its surface to the caller; an abandoned
    * one still holds a partial surface. */
   rtga_proc_reset(rtga);
   free(rtga);
}

rtga_t *rtga_alloc(void)
{
   rtga_t *rtga = (rtga_t*)calloc(1, sizeof(*rtga));
   if (!rtga)
      return NULL;
   return rtga;
}
