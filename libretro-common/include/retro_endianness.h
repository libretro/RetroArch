/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_endianness.h).
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

#include <retro_inline.h>
#include <stdint.h>

#define SWAP16(x) ((uint16_t)(				      \
         (((uint16_t)(x) & 0x00ff) << 8)      |	\
         (((uint16_t)(x) & 0xff00) >> 8)        \
          ))

#define SWAP32(x) ((uint32_t)(           \
         (((uint32_t)(x) & 0x000000ff) << 24) | \
         (((uint32_t)(x) & 0x0000ff00) <<  8) | \
         (((uint32_t)(x) & 0x00ff0000) >>  8) | \
         (((uint32_t)(x) & 0xff000000) >> 24)   \
         ))

/**
 * is_little_endian:
 *
 * Checks if the system is little endian or big-endian.
 *
 * Returns: greater than 0 if little-endian,
 * otherwise big-endian.
 **/
static INLINE uint8_t is_little_endian(void)
{
   union
   {
      uint16_t x;
      uint8_t y[2];
   } u;

   u.x = 1;
   return u.y[0];
}

/**
 * swap_if_big32:
 * @val        : unsigned 32-bit value
 *
 * Byteswap unsigned 32-bit value if system is big-endian.
 *
 * Returns: Byteswapped value in case system is big-endian,
 * otherwise returns same value.
 **/
static INLINE uint32_t swap_if_big32(uint32_t val)
{
   if (is_little_endian())
      return val;
   return (val >> 24) | ((val >> 8) & 0xFF00) |
      ((val << 8) & 0xFF0000) | (val << 24);
}

static INLINE uint32_t swap_little32(uint32_t val)
{
   return 
         (val >> 24) 
      | ((val >> 8) & 0xFF00)
      | ((val << 8) & 0xFF0000)
      |  (val << 24);
}

/**
 * swap_if_little32:
 * @val        : unsigned 32-bit value
 *
 * Byteswap unsigned 32-bit value if system is little-endian.
 *
 * Returns: Byteswapped value in case system is little-endian,
 * otherwise returns same value.
 **/
static INLINE uint32_t swap_if_little32(uint32_t val)
{
   if (is_little_endian())
      return swap_little32(val);
   return val;
}

static INLINE uint16_t swap_big16(uint16_t val)
{
   return (val >> 8) | (val << 8);
}

/**
 * swap_if_big16:
 * @val        : unsigned 16-bit value
 *
 * Byteswap unsigned 16-bit value if system is big-endian.
 *
 * Returns: Byteswapped value in case system is big-endian,
 * otherwise returns same value.
 **/
static INLINE uint16_t swap_if_big16(uint16_t val)
{
   if (is_little_endian())
      return val;
   return swap_big16(val);
}

static INLINE uint16_t swap_little16(uint16_t val)
{
   return (val >> 8) | (val << 8);
}

/**
 * swap_if_little16:
 * @val        : unsigned 16-bit value
 *
 * Byteswap unsigned 16-bit value if system is little-endian.
 *
 * Returns: Byteswapped value in case system is little-endian,
 * otherwise returns same value.
 **/
static INLINE uint16_t swap_if_little16(uint16_t val)
{
   if (is_little_endian())
      return swap_little16(val);
   return val;
}

/**
 * store32be:
 * @addr        : pointer to unsigned 32-bit buffer
 * @data        : unsigned 32-bit value to write
 *
 * Write data to address. Endian-safe. Byteswaps the data
 * first if necessary before storing it.
 **/
static INLINE void store32be(uint32_t *addr, uint32_t data)
{
   *addr = is_little_endian() ? SWAP32(data) : data;
}

/**
 * load32be:
 * @addr        : pointer to unsigned 32-bit buffer
 *
 * Load value from address. Endian-safe.
 *
 * Returns: value from address, byte-swapped if necessary.
 **/
static INLINE uint32_t load32be(const uint32_t *addr)
{
   return is_little_endian() ? SWAP32(*addr) : *addr;
}

#endif
