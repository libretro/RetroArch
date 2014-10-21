/* Copyright  (C) 2010-2014 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (boolean.h).
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

#ifndef __LIBRETRO_SDK_ENDIANNESS_H
#define __LIBRETRO_SDK_ENDIANNESS_H

#include <stdint.h>

#define SWAP32(x) ((uint32_t)(           \
         (((uint32_t)(x) & 0x000000ff) << 24) | \
         (((uint32_t)(x) & 0x0000ff00) <<  8) | \
         (((uint32_t)(x) & 0x00ff0000) >>  8) | \
         (((uint32_t)(x) & 0xff000000) >> 24)   \
         ))

static inline uint8_t is_little_endian(void)
{
   union
   {
      uint16_t x;
      uint8_t y[2];
   } u;

   u.x = 1;
   return u.y[0];
}

static inline uint32_t swap_if_big32(uint32_t val)
{
   if (is_little_endian())
      return val;
   return (val >> 24) | ((val >> 8) & 0xFF00) |
      ((val << 8) & 0xFF0000) | (val << 24);
}

static inline uint32_t swap_if_little32(uint32_t val)
{
   if (is_little_endian())
      return (val >> 24) | ((val >> 8) & 0xFF00) |
         ((val << 8) & 0xFF0000) | (val << 24);
   return val;
}

static inline uint16_t swap_if_big16(uint16_t val)
{
   if (is_little_endian())
      return val;
   return (val >> 8) | (val << 8);
}

static inline uint16_t swap_if_little16(uint16_t val)
{
   if (is_little_endian())
      return (val >> 8) | (val << 8);
   return val;
}

static inline void store32be(uint32_t *addr, uint32_t data)
{
   *addr = is_little_endian() ? SWAP32(data) : data;
}

static inline uint32_t load32be(const uint32_t *addr)
{
   return is_little_endian() ? SWAP32(*addr) : *addr;
}

#endif
