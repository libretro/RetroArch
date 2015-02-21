/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng.c).
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

#ifndef _RPNG_COMMON_H
#define _RPNG_COMMON_H

#include <retro_inline.h>

#undef GOTO_END_ERROR
#define GOTO_END_ERROR() do { \
   fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while(0)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static const uint8_t png_magic[8] = {
   0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
};

enum png_chunk_type
{
   PNG_CHUNK_NOOP = 0,
   PNG_CHUNK_ERROR,
   PNG_CHUNK_IHDR,
   PNG_CHUNK_IDAT,
   PNG_CHUNK_PLTE,
   PNG_CHUNK_IEND
};

struct adam7_pass
{
   unsigned x;
   unsigned y;
   unsigned stride_x;
   unsigned stride_y;
};

/* Paeth prediction filter. */
static INLINE int paeth(int a, int b, int c)
{
   int p  = a + b - c;
   int pa = abs(p - a);
   int pb = abs(p - b);
   int pc = abs(p - c);

   if (pa <= pb && pa <= pc)
      return a;
   else if (pb <= pc)
      return b;
   return c;
}

static INLINE uint32_t dword_be(const uint8_t *buf)
{
   return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
}

static INLINE enum png_chunk_type png_chunk_type(const struct png_chunk *chunk)
{
   unsigned i;
   struct
   {
      const char *id;
      enum png_chunk_type type;
   } static const chunk_map[] = {
      { "IHDR", PNG_CHUNK_IHDR },
      { "IDAT", PNG_CHUNK_IDAT },
      { "IEND", PNG_CHUNK_IEND },
      { "PLTE", PNG_CHUNK_PLTE },
   };

   for (i = 0; i < ARRAY_SIZE(chunk_map); i++)
   {
      if (memcmp(chunk->type, chunk_map[i].id, 4) == 0)
         return chunk_map[i].type;
   }

   return PNG_CHUNK_NOOP;
}

static INLINE void copy_line_rgb(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   unsigned i;

   bpp /= 8;

   for (i = 0; i < width; i++)
   {
      uint32_t r, g, b;

      r        = *decoded;
      decoded += bpp;
      g        = *decoded;
      decoded += bpp;
      b        = *decoded;
      decoded += bpp;
      data[i]  = (0xffu << 24) | (r << 16) | (g << 8) | (b << 0);
   }
}

static INLINE void copy_line_rgba(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   unsigned i;

   bpp /= 8;

   for (i = 0; i < width; i++)
   {
      uint32_t r, g, b, a;
      r        = *decoded;
      decoded += bpp;
      g        = *decoded;
      decoded += bpp;
      b        = *decoded;
      decoded += bpp;
      a        = *decoded;
      decoded += bpp;
      data[i]  = (a << 24) | (r << 16) | (g << 8) | (b << 0);
   }
}

static INLINE void copy_line_bw(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned depth)
{
   unsigned i, bit;
   static const unsigned mul_table[] = { 0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
   unsigned mul, mask;
   
   if (depth == 16)
   {
      for (i = 0; i < width; i++)
      {
         uint32_t val = decoded[i << 1];
         data[i]      = (val * 0x010101) | (0xffu << 24);
      }
      return;
   }

   mul  = mul_table[depth];
   mask = (1 << depth) - 1;
   bit  = 0;

   for (i = 0; i < width; i++, bit += depth)
   {
      unsigned byte = bit >> 3;
      unsigned val  = decoded[byte] >> (8 - depth - (bit & 7));

      val          &= mask;
      val          *= mul;
      data[i]       = (val * 0x010101) | (0xffu << 24);
   }
}

static INLINE void copy_line_gray_alpha(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned bpp)
{
   unsigned i;

   bpp /= 8;

   for (i = 0; i < width; i++)
   {
      uint32_t gray, alpha;

      gray     = *decoded;
      decoded += bpp;
      alpha    = *decoded;
      decoded += bpp;

      data[i]  = (gray * 0x010101) | (alpha << 24);
   }
}

static INLINE void copy_line_plt(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned depth, const uint32_t *palette)
{
   unsigned i, bit;
   unsigned mask = (1 << depth) - 1;

   bit = 0;

   for (i = 0; i < width; i++, bit += depth)
   {
      unsigned byte = bit >> 3;
      unsigned val  = decoded[byte] >> (8 - depth - (bit & 7));

      val          &= mask;
      data[i]       = palette[val];
   }
}

#endif
