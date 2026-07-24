/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbmp.c).
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

/* rbmp -- BMP decoder (modified version of stb_image's BMP sources).
 *
 * What it implements: Windows BMP with core (12-byte) and
 * BITMAPINFOHEADER/V4/V5 (40/56/108/124-byte) headers, 1/4/8-bit
 * palettised, 16-bit with arbitrary channel bitfields, and 24/32-bit
 * images, top-down and bottom-up row order, with all-alpha-zero
 * detection for 32-bit files that leave the alpha channel empty.
 * A matching encoder (24/32-bit uncompressed) lives in
 * rbmp_encode.c.
 *
 * What it does not implement: RLE4/RLE8 compression (rejected up
 * front), embedded PNG/JPEG payloads (BI_PNG/BI_JPEG), and colour
 * management from the V4/V5 header extensions (the fields are
 * skipped). */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h> /* ptrdiff_t on osx */
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* INT_MAX */

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rbmp.h>

/* truncate int to byte without warnings */
#define RBMP_BYTECAST(x)  ((unsigned char) ((x) & 255))

typedef struct
{
   unsigned char *img_buffer;
   unsigned char *img_buffer_end;
   unsigned char *img_buffer_original;
   int img_n;
   int img_out_n;
   int buflen;
   uint32_t img_x;
   uint32_t img_y;
   unsigned char buffer_start[128];
} rbmp_context;

struct rbmp
{
   uint8_t *buff_data;
   uint32_t *output_image;
};

static INLINE unsigned char rbmp_get8(rbmp_context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;

   return 0;
}

static void rbmp_skip(rbmp_context *s, int n)
{
   ptrdiff_t remaining;
   if (n < 0)
   {
      s->img_buffer = s->img_buffer_end;
      return;
   }
   /* Clamp to remaining input to avoid pointer arithmetic beyond
    * img_buffer_end (UB per C99).  Subsequent rbmp_get8 calls
    * check "buffer < buffer_end" and parse as EOF. */
   remaining = s->img_buffer_end - s->img_buffer;
   if ((ptrdiff_t)n > remaining)
      s->img_buffer = s->img_buffer_end;
   else
      s->img_buffer += n;
}

static int rbmp_get16le(rbmp_context *s)
{
   /* Sequenced explicitly: two side-effecting reads in one expression
    * have no sequence point between them, so their order is
    * unspecified and the halves can swap.  See rbmp_get32le below. */
   int lo = rbmp_get8(s);
   int hi = rbmp_get8(s);
   return lo + (hi << 8);
}

/* Read a 32-bit little-endian value as two 16-bit halves.
 *
 * This was a macro expanding to
 *    (rbmp_get16le(s) + (rbmp_get16le(s) << 16))
 * which places two side-effecting calls in one expression with no
 * sequence point between them.  Their relative order is unspecified,
 * so a compiler is free to evaluate the high half first and the two
 * halves get swapped - a header field of 40 reads back as 2621440,
 * and every 32-bit BMP field is misparsed.  It happened to work with
 * the ordering GCC picked at the usual optimisation levels, but it is
 * not something the language guarantees: building with sanitizers
 * instrumented (which perturbs the order) already flips it, and so
 * could a different compiler, target or -O level.
 *
 * Sequence the reads explicitly. */
static INLINE uint32_t rbmp_get32le(rbmp_context *s)
{
   uint32_t lo = (uint32_t)rbmp_get16le(s);
   uint32_t hi = (uint32_t)rbmp_get16le(s);
   return lo + (hi << 16);
}
#define RBMP_GET32LE(s) rbmp_get32le(s)

/* Microsoft/Windows BMP image */

/* returns 0..31 for the highest set bit */
static int rbmp_high_bit(unsigned int z)
{
   int n=0;
   if (z == 0)
      return -1;

   if (z >= 0x10000)
   {
      n  += 16;
      z >>= 16;
   }
   if (z >= 0x00100)
   {
      n  +=  8;
      z >>=  8;
   }
   if (z >= 0x00010)
   {
      n  +=  4;
      z >>=  4;
   }
   if (z >= 0x00004)
   {
      n  +=  2;
      z >>=  2;
   }
   if (z >= 0x00002)
      n +=  1;
   return n;
}

static int rbmp_bitcount(unsigned int a)
{
   a = (a & 0x55555555) + ((a >>  1) & 0x55555555); /* max 2 */
   a = (a & 0x33333333) + ((a >>  2) & 0x33333333); /* max 4 */
   a = (a + (a >> 4)) & 0x0f0f0f0f; /* max 8 per 4, now 8 bits */
   a = (a + (a >> 8));              /* max 16 per 8 bits */
   a = (a + (a >> 16));             /* max 32 per 8 bits */
   return a & 0xff;
}

static int rbmp_shiftsigned(int v, int shift, int bits)
{
   int ret;
   int z = bits;

   if (shift < 0)
      v <<= -shift;
   else
      v >>= shift;

   ret = v;

   while (z < 8)
   {
      ret += v >> z;
      z   += bits;
   }
   return ret;
}

static uint32_t *rbmp_bmp_load(rbmp_context *s, unsigned *x, unsigned *y,
      int *comp, bool supports_rgba)
{
   uint32_t *output;
   int bpp, flip_vertically, pad, offset, hsz;
   int psize=0,i,j,width;
   unsigned int mr=0,mg=0,mb=0,ma=0;

   /* Corrupt BMP? */
   if (rbmp_get8(s) != 'B' || rbmp_get8(s) != 'M')
      return 0;

   /* discard filesize */
   rbmp_get16le(s);
   rbmp_get16le(s);
   /* discard reserved */
   rbmp_get16le(s);
   rbmp_get16le(s);

   offset = (uint32_t)RBMP_GET32LE(s);
   hsz    = (uint32_t)RBMP_GET32LE(s);

   /* BMP type not supported? */
   if (hsz != 12 && hsz != 40 && hsz != 56 && hsz != 108 && hsz != 124)
      return 0;

   if (hsz == 12)
   {
      s->img_x = rbmp_get16le(s);
      s->img_y = rbmp_get16le(s);
   }
   else
   {
      s->img_x = (uint32_t)RBMP_GET32LE(s);
      s->img_y = (uint32_t)RBMP_GET32LE(s);
   }

   /* Bad BMP? */
   if (rbmp_get16le(s) != 1)
      return 0;

   bpp = rbmp_get16le(s);

   /* BMP 1-bit type not supported? */
   if (bpp == 1)
      return 0;

   /* img_y can legitimately be a negative int (top-down BMP).
    * Pre-patch the code did abs((int)s->img_y) -- if the uint32
    * happened to be 0x80000000, the cast to int is INT_MIN and
    * abs(INT_MIN) is undefined behaviour.  Detect that case and
    * treat as bottom-up zero-height (rejected by the overflow
    * guard below). */
   if (s->img_y == 0x80000000u)
      return 0;
   flip_vertically = ((int) s->img_y) > 0;
   s->img_y        = (uint32_t)abs((int) s->img_y);

   if (hsz == 12)
   {
      if (bpp < 24)
         psize = (offset - 14 - 24) / 3;
   }
   else
   {
      int compress = (uint32_t)RBMP_GET32LE(s);

      /* BMP RLE type not supported? */
      if (compress == 1 || compress == 2)
         return 0;

      /* discard sizeof */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard hres */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard vres */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard colors used */
      rbmp_get16le(s);
      rbmp_get16le(s);
      /* discard max important */
      rbmp_get16le(s);
      rbmp_get16le(s);

      if (hsz == 40 || hsz == 56)
      {
         if (hsz == 56)
         {
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
            rbmp_get16le(s);
         }
         if (bpp == 16 || bpp == 32)
         {
            switch (compress)
            {
               case 0:
                  /* BI_RGB: no masks in the file - use the standard layout
                   * (stbi__bmp_set_mask_defaults). 32bpp BI_RGB is BGRx;
                   * treat the 4th byte as alpha, with the all-zero-alpha
                   * fixup below handling files that leave it 0. Was an
                   * unconditional "Bad BMP" return that rejected every
                   * plain 16/32bpp BMP. */
                  if (bpp == 32)
                  {
                     mr = 0xffu << 16;
                     mg = 0xffu << 8;
                     mb = 0xffu;
                     ma = 0xffu << 24;
                  }
                  else
                  {
                     /* BI_RGB 16bpp defaults to X1R5G5B5 */
                     mr = 31u << 10;
                     mg = 31u << 5;
                     mb = 31u;
                  }
                  break;
               case 3:
                  mr = (uint32_t)RBMP_GET32LE(s);
                  mg = (uint32_t)RBMP_GET32LE(s);
                  mb = (uint32_t)RBMP_GET32LE(s);
                  /* not documented, but generated by
                   * Photoshop and handled by MS Paint */
                  /* Bad BMP ?*/
                  if (mr == mg && mg == mb)
                     return 0;
                  break;
               default:
                  /* Bad BMP? */
                  return 0;
            }
         }
      }
      else
      {
         mr = (uint32_t)RBMP_GET32LE(s);
         mg = (uint32_t)RBMP_GET32LE(s);
         mb = (uint32_t)RBMP_GET32LE(s);
         ma = (uint32_t)RBMP_GET32LE(s);
         /* Discard color space */
         rbmp_get16le(s);
         rbmp_get16le(s);
         for (i = 0; i < 12; ++i)
         {
            /* Discard color space parameters */
            rbmp_get16le(s);
            rbmp_get16le(s);
         }
         if (hsz == 124)
         {
            /* Discard rendering intent */
            rbmp_get16le(s);
            rbmp_get16le(s);
            /* Discard offset of profile data */
            rbmp_get16le(s);
            rbmp_get16le(s);
            /* Discard size of profile data */
            rbmp_get16le(s);
            rbmp_get16le(s);
            /* Discard reserved */
            rbmp_get16le(s);
            rbmp_get16le(s);
         }
      }
      if (bpp < 16)
         psize = (offset - 14 - hsz) >> 2;
   }
   s->img_n = ma ? 4 : 3;

   /* Always output as uint32 (4 bytes per pixel).
    *
    * Pre-patch this multiplied two uint32_t dimensions at uint32_t
    * width, so img_x * img_y silently wrapped on 32-bit -- e.g.
    * 0x10001 * 0x10000 = 0x100010000 wraps to 0x10000, and the
    * subsequent malloc returned a 256 KiB buffer that the pixel
    * decode loop wrote off the end of (4 GiB+ of writes).
    *
    * BMP dimensions come from 32-bit header fields, so unlike TGA's
    * 16-bit ones the product can overflow even a 64-bit size_t
    * (0xFFFFFFFF squared, times four, is ~64 EiB).  The
    * multiplication therefore has to be checked rather than merely
    * widened - but the check should bound what is *addressable*, not
    * what someone guessed a realistic asset looks like.  A fixed
    * 0x4000 ceiling refused large scans and renders outright, leaving
    * no thumbnail at all, and "far beyond any realistic libretro
    * asset" is not a safe assumption to make on a user's behalf.
    *
    * Divide instead of multiply so nothing can wrap: if the pixel
    * count exceeds SIZE_MAX/4 the RGBA buffer is unrepresentable on
    * this host and the file is refused; otherwise let the allocation
    * decide, and a request the host cannot satisfy fails at malloc,
    * which is handled below.  On a 32-bit host that division-based
    * ceiling is itself the old wrap guard, now exact rather than
    * approximate. */
   if (s->img_x == 0 || s->img_y == 0)
      return 0;
   {
      size_t max_px = (size_t)-1 / sizeof(uint32_t);
      if ((size_t)s->img_x > max_px / (size_t)s->img_y)
         return 0;
   }

   /* The dimensions are representable, but that alone does not make
    * them real: they are attacker-controlled header fields, and letting
    * the allocation decide only works where an over-large request
    * actually fails.  Under Linux overcommit it does not - a 128-byte
    * file declaring 0x10001 x 0x10000 gets a 16 GiB mapping and the
    * decoder then faults it all in, one zero-filled row at a time, for
    * an out-of-memory kill or a very long stall from a trivially small
    * input.
    *
    * A declared image cannot need more pixel data than the file has
    * left to give, so require the input to hold what it claims: one
    * padded row stride per row, from the pixel-data offset onwards.
    * That is a property of the file rather than a guess about what a
    * realistic asset looks like, so it refuses nothing a real decode
    * would have produced - a genuinely large BMP carries genuinely
    * large pixel data and still loads.  Rows are computed in size_t
    * from values already bounded above, so nothing here can wrap. */
   {
      size_t total     = (size_t)(s->img_buffer_end - s->img_buffer_original);
      size_t data_off  = (offset > 0) ? (size_t)offset : 0;
      size_t row_bits  = (size_t)s->img_x * (size_t)((bpp > 0) ? bpp : 1);
      size_t row_bytes = ((row_bits + 31) / 32) * 4; /* 4-byte aligned rows */

      if (data_off >= total)
         return 0;
      if (row_bytes > (total - data_off) / (size_t)s->img_y)
         return 0;
   }

   output = (uint32_t*)malloc(
         (size_t)s->img_x * (size_t)s->img_y * sizeof(uint32_t));
   if (!output)
      return 0;

   if (bpp < 16)
   {
      /* Palette mode: pre-convert palette to uint32 in target byte order */
      uint32_t pal32[256];

      if (psize == 0 || psize > 256)
      {
         free(output);
         return 0;
      }

      for (i = 0; i < psize; ++i)
      {
         unsigned char b = rbmp_get8(s);
         unsigned char g = rbmp_get8(s);
         unsigned char r = rbmp_get8(s);
         if (hsz != 12)
            rbmp_get8(s);
         if (supports_rgba)
            pal32[i] = 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
         else
            pal32[i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
      }

      rbmp_skip(s, offset - 14 - hsz - psize * (hsz == 12 ? 3 : 4));
      if (bpp == 4)
         width = (s->img_x + 1) >> 1;
      else if (bpp == 8)
         width = s->img_x;
      else
      {
         free(output);
         return 0;
      }

      pad = (-width)&3;
      for (j = 0; j < (int)s->img_y; ++j)
      {
         int dst_row = flip_vertically ? (int)(s->img_y - 1 - j) : j;
         /* Use size_t for the row-stride offset.  dst_row *
          * s->img_x in signed-int could overflow for a legitimate
          * 2 GiB BMP (e.g. 46341 x 46341).  Post-patch the
          * pointer math matches the size_t-based allocation. */
         uint32_t *dst = output + (size_t)dst_row * (size_t)s->img_x;
         int col = 0;

         for (i = 0; i < (int)s->img_x; i += 2)
         {
            int v  = rbmp_get8(s);
            int v2 = 0;
            if (bpp == 4)
            {
               v2  = v & 15;
               v >>= 4;
            }
            dst[col++] = pal32[v];

            if (i + 1 == (int)s->img_x)
               break;

            v = (bpp == 8) ? rbmp_get8(s) : v2;
            dst[col++] = pal32[v];
         }
         rbmp_skip(s, pad);
      }
   }
   else
   {
      int rshift = 0;
      int gshift = 0;
      int bshift = 0;
      int ashift = 0;
      int rcount = 0;
      int gcount = 0;
      int bcount = 0;
      int acount = 0;
      int easy   = 0;
      /* OR of every alpha value read; BI_RGB 32bpp files routinely leave
       * the 4th byte 0, which would decode fully transparent - if the
       * whole image had alpha 0, force it opaque afterwards (mirrors
       * stb_image's all_a fixup). */
      unsigned char all_a = 0;

      rbmp_skip(s, offset - 14 - hsz);

      if (bpp == 24)
         width = 3 * s->img_x;
      else if (bpp == 16)
         width = 2 * s->img_x;
      else /* bpp = 32 and pad = 0 */
         width = 0;

      pad = (-width) & 3;

      switch (bpp)
      {
         case 24:
            easy = 1;
            break;
         case 32:
            if (mb == 0xff && mg == 0xff00 && mr == 0x00ff0000 && ma == 0xff000000)
               easy = 2;
            break;
         default:
            break;
      }

      if (!easy)
      {
         /* Corrupt BMP? */
         if (!mr || !mg || !mb)
         {
            free(output);
            return 0;
         }

         rshift = rbmp_high_bit(mr)-7;
         rcount = rbmp_bitcount(mr);
         gshift = rbmp_high_bit(mg)-7;
         gcount = rbmp_bitcount(mg);
         bshift = rbmp_high_bit(mb)-7;
         bcount = rbmp_bitcount(mb);
         ashift = rbmp_high_bit(ma)-7;
         acount = rbmp_bitcount(ma);
      }

      for (j = 0; j < (int)s->img_y; ++j)
      {
         int dst_row = flip_vertically ? (int)(s->img_y - 1 - j) : j;
         uint32_t *dst = output + dst_row * s->img_x;

         if (easy)
         {
            if (easy == 2)
            {
               /* 32bpp BGRA — can read directly */
               if (!supports_rgba)
               {
                  /* BMP bytes [B,G,R,A] = uint32 ARGB on LE — memcpy */
                  int bytes_needed = s->img_x * 4;
                  if (s->img_buffer + bytes_needed <= s->img_buffer_end)
                  {
                     memcpy(dst, s->img_buffer, bytes_needed);
                     s->img_buffer += bytes_needed;
                     for (i = 0; i < (int)s->img_x; ++i)
                        all_a |= (unsigned char)(dst[i] >> 24);
                  }
               }
               else
               {
                  /* Need ABGR — swap R and B during read */
                  for (i = 0; i < (int)s->img_x; ++i)
                  {
                     unsigned char b = rbmp_get8(s);
                     unsigned char g = rbmp_get8(s);
                     unsigned char r = rbmp_get8(s);
                     unsigned char a = rbmp_get8(s);
                     all_a |= a;
                     dst[i] = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                            | ((uint32_t)g << 8)  | (uint32_t)r;
                  }
               }
            }
            else
            {
               /* 24bpp BGR */
               if (!supports_rgba)
               {
                  /* Need ARGB */
                  for (i = 0; i < (int)s->img_x; ++i)
                  {
                     unsigned char b = rbmp_get8(s);
                     unsigned char g = rbmp_get8(s);
                     unsigned char r = rbmp_get8(s);
                     dst[i] = 0xFF000000u | ((uint32_t)r << 16)
                            | ((uint32_t)g << 8)  | (uint32_t)b;
                  }
               }
               else
               {
                  /* Need ABGR */
                  for (i = 0; i < (int)s->img_x; ++i)
                  {
                     unsigned char b = rbmp_get8(s);
                     unsigned char g = rbmp_get8(s);
                     unsigned char r = rbmp_get8(s);
                     dst[i] = 0xFF000000u | ((uint32_t)b << 16)
                            | ((uint32_t)g << 8)  | (uint32_t)r;
                  }
               }
            }
         }
         else
         {
            /* Bitmask mode (16bpp or non-standard 32bpp) */
            for (i = 0; i < (int)s->img_x; ++i)
            {
               unsigned char r, g, b, a;
               uint32_t v = (bpp == 16)
                  ? (uint32_t)rbmp_get16le(s)
                  : (uint32_t)RBMP_GET32LE(s);
               r = RBMP_BYTECAST(rbmp_shiftsigned(v & mr, rshift, rcount));
               g = RBMP_BYTECAST(rbmp_shiftsigned(v & mg, gshift, gcount));
               b = RBMP_BYTECAST(rbmp_shiftsigned(v & mb, bshift, bcount));
               a = ma ? RBMP_BYTECAST(rbmp_shiftsigned(v & ma, ashift, acount)) : 255;
               all_a |= a;

               if (supports_rgba)
                  dst[i] = ((uint32_t)a << 24) | ((uint32_t)b << 16)
                         | ((uint32_t)g << 8)  | (uint32_t)r;
               else
                  dst[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16)
                         | ((uint32_t)g << 8)  | (uint32_t)b;
            }
         }
         rbmp_skip(s, pad);
      }

      /* Whole image decoded with alpha 0 (typical BI_RGB 32bpp with a
       * zeroed pad byte): force opaque, like stb_image. */
      if ((easy == 2 || (bpp == 32 && ma == 0xffu << 24)) && all_a == 0)
      {
         for (j = 0; j < (int)(s->img_x * s->img_y); ++j)
            output[j] |= 0xFF000000u;
      }
   }

   *x = s->img_x;
   *y = s->img_y;

   if (comp)
      *comp = s->img_n;

   return output;
}

static uint32_t *rbmp_load_from_memory(unsigned char const *buffer, int len,
      unsigned *x, unsigned *y, int *comp, bool supports_rgba)
{
   rbmp_context s;

   s.img_buffer          = (unsigned char*)buffer;
   s.img_buffer_original = (unsigned char*)buffer;
   s.img_buffer_end      = (unsigned char*)buffer+len;

   return rbmp_bmp_load(&s, x, y, comp, supports_rgba);
}

int rbmp_process_image(rbmp_t *rbmp, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   int comp;

   if (!rbmp)
      return IMAGE_PROCESS_ERROR;

   rbmp->output_image   = rbmp_load_from_memory(rbmp->buff_data,
                           (int)size, width, height, &comp, supports_rgba);
   *buf_data             = rbmp->output_image;

   if (!rbmp->output_image)
      return IMAGE_PROCESS_ERROR;

   return IMAGE_PROCESS_END;
}

bool rbmp_set_buf_ptr(rbmp_t *rbmp, void *data)
{
   if (!rbmp)
      return false;

   rbmp->buff_data = (uint8_t*)data;

   return true;
}

void rbmp_free(rbmp_t *rbmp)
{
   if (!rbmp)
      return;

   free(rbmp);
}

rbmp_t *rbmp_alloc(void)
{
   rbmp_t *rbmp = (rbmp_t*)calloc(1, sizeof(*rbmp));
   if (!rbmp)
      return NULL;
   return rbmp;
}
