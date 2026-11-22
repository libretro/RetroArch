/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_narrow.h).
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

#ifndef LIBRETRO_COMMON_IMAGE_NARROW_H
#define LIBRETRO_COMMON_IMAGE_NARROW_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_inline.h>

/* Narrow one packed XRGB2101010 row directly into its upload destination.
 * Source and destination may be identical; other overlap is unsupported.
 * RGBA names memory-order bytes, otherwise the result is ARGB8888 words. */
static INLINE void image_texture_narrow_10bit_row(uint32_t *dst,
      const uint32_t *src, size_t count, bool rgba)
{
   size_t i;
   if (rgba)
   {
      uint8_t *d = (uint8_t*)dst;
      for (i = 0; i < count; i++, d += 4)
      {
         uint32_t p = src[i];
         d[0] = (uint8_t)(p >> 22);
         d[1] = (uint8_t)(p >> 12);
         d[2] = (uint8_t)(p >>  2);
         d[3] = 0xff;
      }
   }
   else
      for (i = 0; i < count; i++)
      {
         uint32_t p = src[i];
         dst[i] = 0xff000000u
               | ((p >>  6) & 0xff0000u)
               | ((p >>  4) & 0x00ff00u)
               | ((p >>  2) & 0x0000ffu);
      }
}

#endif
