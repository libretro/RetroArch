/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <stdlib.h>

#if defined(_MSC_VER) && _MSC_VER > 1200
#define SWAP16 _byteswap_ushort
#define SWAP32 _byteswap_ulong
#else
static INLINE uint16_t SWAP16(uint16_t x)
{
  return ((x & 0x00ff) << 8) |
         ((x & 0xff00) >> 8);
}

static INLINE uint32_t SWAP32(uint32_t x)
{
  return ((x & 0x000000ff) << 24) |
         ((x & 0x0000ff00) <<  8) |
         ((x & 0x00ff0000) >>  8) |
         ((x & 0xff000000) >> 24);
}

#endif

#if defined(_MSC_VER) && _MSC_VER <= 1200
static INLINE uint64_t SWAP64(uint64_t val)
{
  return
      ((val & 0x00000000000000ff) << 56)
    | ((val & 0x000000000000ff00) << 40)
    | ((val & 0x0000000000ff0000) << 24)
    | ((val & 0x00000000ff000000) << 8)
    | ((val & 0x000000ff00000000) >> 8)
    | ((val & 0x0000ff0000000000) >> 24)
    | ((val & 0x00ff000000000000) >> 40)
    | ((val & 0xff00000000000000) >> 56);
}
#else
static INLINE uint64_t SWAP64(uint64_t val)
{
  return   ((val & 0x00000000000000ffULL) << 56)
	 | ((val & 0x000000000000ff00ULL) << 40)
	 | ((val & 0x0000000000ff0000ULL) << 24)
	 | ((val & 0x00000000ff000000ULL) << 8)
	 | ((val & 0x000000ff00000000ULL) >> 8)
	 | ((val & 0x0000ff0000000000ULL) >> 24)
	 | ((val & 0x00ff000000000000ULL) >> 40)
         | ((val & 0xff00000000000000ULL) >> 56);
}
#endif


#if defined (LSB_FIRST) || defined (MSB_FIRST)
#  warning Defining MSB_FIRST and LSB_FIRST in compile options is deprecated
#  undef LSB_FIRST
#  undef MSB_FIRST
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#include <winsock2.h>
#endif

#ifdef _MSC_VER
#if _M_IX86 || _M_AMD64 || _M_ARM || _M_ARM64
#define LSB_FIRST 1
#elif _M_PPC
#define MSB_FIRST 1
#else
/* MSVC can run on _M_ALPHA and _M_IA64 too, but they're both bi-endian; need to find what mode MSVC runs them at */
#error "unknown platform, can't determine endianness"
#endif
#else
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MSB_FIRST 1
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LSB_FIRST 1
#else
#error "Invalid endianness macros"
#endif
#endif

#if defined(MSB_FIRST) && defined(LSB_FIRST)
#  error "Bug in LSB_FIRST/MSB_FIRST definition"
#endif

#if !defined(MSB_FIRST) && !defined(LSB_FIRST)
#  error "Bug in LSB_FIRST/MSB_FIRST definition"
#endif

#ifdef MSB_FIRST
#  define RETRO_IS_BIG_ENDIAN 1
#  define RETRO_IS_LITTLE_ENDIAN 0
/* For compatibility */
#  define WORDS_BIGENDIAN 1
#else
#  define RETRO_IS_BIG_ENDIAN 0
#  define RETRO_IS_LITTLE_ENDIAN 1
/* For compatibility */
#  undef WORDS_BIGENDIAN
#endif


/**
 * is_little_endian:
 *
 * Checks if the system is little endian or big-endian.
 *
 * Returns: greater than 0 if little-endian,
 * otherwise big-endian.
 **/
#define is_little_endian() RETRO_IS_LITTLE_ENDIAN

/**
 * swap_if_big64:
 * @val        : unsigned 64-bit value
 *
 * Byteswap unsigned 64-bit value if system is big-endian.
 *
 * Returns: Byteswapped value in case system is big-endian,
 * otherwise returns same value.
 **/

#if RETRO_IS_BIG_ENDIAN
#define swap_if_big64(val) (SWAP64(val))
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_big64(val) (val)
#endif

/**
 * swap_if_big32:
 * @val        : unsigned 32-bit value
 *
 * Byteswap unsigned 32-bit value if system is big-endian.
 *
 * Returns: Byteswapped value in case system is big-endian,
 * otherwise returns same value.
 **/

#if RETRO_IS_BIG_ENDIAN
#define swap_if_big32(val) (SWAP32(val))
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_big32(val) (val)
#endif

/**
 * swap_if_little64:
 * @val        : unsigned 64-bit value
 *
 * Byteswap unsigned 64-bit value if system is little-endian.
 *
 * Returns: Byteswapped value in case system is little-endian,
 * otherwise returns same value.
 **/

#if RETRO_IS_BIG_ENDIAN
#define swap_if_little64(val) (val)
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_little64(val) (SWAP64(val))
#endif

/**
 * swap_if_little32:
 * @val        : unsigned 32-bit value
 *
 * Byteswap unsigned 32-bit value if system is little-endian.
 *
 * Returns: Byteswapped value in case system is little-endian,
 * otherwise returns same value.
 **/

#if RETRO_IS_BIG_ENDIAN
#define swap_if_little32(val) (val)
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_little32(val) (SWAP32(val))
#endif

/**
 * swap_if_big16:
 * @val        : unsigned 16-bit value
 *
 * Byteswap unsigned 16-bit value if system is big-endian.
 *
 * Returns: Byteswapped value in case system is big-endian,
 * otherwise returns same value.
 **/

#if RETRO_IS_BIG_ENDIAN
#define swap_if_big16(val) (SWAP16(val))
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_big16(val) (val)
#endif

/**
 * swap_if_little16:
 * @val        : unsigned 16-bit value
 *
 * Byteswap unsigned 16-bit value if system is little-endian.
 *
 * Returns: Byteswapped value in case system is little-endian,
 * otherwise returns same value.
 **/

#if RETRO_IS_BIG_ENDIAN
#define swap_if_little16(val) (val)
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_little16(val) (SWAP16(val))
#endif

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
   *addr = swap_if_little32(data);
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
   return swap_if_little32(*addr);
}

/**
 * retro_cpu_to_le16:
 * @val        : unsigned 16-bit value
 *
 * Convert unsigned 16-bit value from system to little-endian.
 *
 * Returns: Little-endian representation of val.
 **/

#define retro_cpu_to_le16(val) swap_if_big16(val)

/**
 * retro_cpu_to_le32:
 * @val        : unsigned 32-bit value
 *
 * Convert unsigned 32-bit value from system to little-endian.
 *
 * Returns: Little-endian representation of val.
 **/

#define retro_cpu_to_le32(val) swap_if_big32(val)

/**
 * retro_cpu_to_le64:
 * @val        : unsigned 64-bit value
 *
 * Convert unsigned 64-bit value from system to little-endian.
 *
 * Returns: Little-endian representation of val.
 **/

#define retro_cpu_to_le64(val) swap_if_big64(val)

/**
 * retro_le_to_cpu16:
 * @val        : unsigned 16-bit value
 *
 * Convert unsigned 16-bit value from little-endian to native.
 *
 * Returns: Native representation of little-endian val.
 **/

#define retro_le_to_cpu16(val) swap_if_big16(val)

/**
 * retro_le_to_cpu32:
 * @val        : unsigned 32-bit value
 *
 * Convert unsigned 32-bit value from little-endian to native.
 *
 * Returns: Native representation of little-endian val.
 **/

#define retro_le_to_cpu32(val) swap_if_big32(val)

/**
 * retro_le_to_cpu16:
 * @val        : unsigned 64-bit value
 *
 * Convert unsigned 64-bit value from little-endian to native.
 *
 * Returns: Native representation of little-endian val.
 **/

#define retro_le_to_cpu64(val) swap_if_big64(val)

/**
 * retro_cpu_to_be16:
 * @val        : unsigned 16-bit value
 *
 * Convert unsigned 16-bit value from system to big-endian.
 *
 * Returns: Big-endian representation of val.
 **/

#define retro_cpu_to_be16(val) swap_if_little16(val)

/**
 * retro_cpu_to_be32:
 * @val        : unsigned 32-bit value
 *
 * Convert unsigned 32-bit value from system to big-endian.
 *
 * Returns: Big-endian representation of val.
 **/

#define retro_cpu_to_be32(val) swap_if_little32(val)

/**
 * retro_cpu_to_be64:
 * @val        : unsigned 64-bit value
 *
 * Convert unsigned 64-bit value from system to big-endian.
 *
 * Returns: Big-endian representation of val.
 **/

#define retro_cpu_to_be64(val) swap_if_little64(val)

/**
 * retro_be_to_cpu16:
 * @val        : unsigned 16-bit value
 *
 * Convert unsigned 16-bit value from big-endian to native.
 *
 * Returns: Native representation of big-endian val.
 **/

#define retro_be_to_cpu16(val) swap_if_little16(val)

/**
 * retro_be_to_cpu32:
 * @val        : unsigned 32-bit value
 *
 * Convert unsigned 32-bit value from big-endian to native.
 *
 * Returns: Native representation of big-endian val.
 **/

#define retro_be_to_cpu32(val) swap_if_little32(val)

/**
 * retro_be_to_cpu64:
 * @val        : unsigned 64-bit value
 *
 * Convert unsigned 64-bit value from big-endian to native.
 *
 * Returns: Native representation of big-endian val.
 **/

#define retro_be_to_cpu64(val) swap_if_little64(val)

#ifdef  __GNUC__
/* This attribute means that the same memory may be referred through
   pointers to different size of the object (aliasing). E.g. that u8 *
   and u32 * may actually be pointing to the same object.  */
#define MAY_ALIAS  __attribute__((__may_alias__))
#else
#define MAY_ALIAS
#endif

#pragma pack(push, 1)
struct retro_unaligned_uint16_s
{
  uint16_t val;
} MAY_ALIAS;
struct retro_unaligned_uint32_s
{
  uint32_t val;
} MAY_ALIAS;
struct retro_unaligned_uint64_s
{
  uint64_t val;
} MAY_ALIAS;
#pragma pack(pop)

typedef struct retro_unaligned_uint16_s retro_unaligned_uint16_t;
typedef struct retro_unaligned_uint32_s retro_unaligned_uint32_t;
typedef struct retro_unaligned_uint64_s retro_unaligned_uint64_t;

/* L-value references to unaligned pointers.  */
#define retro_unaligned16(p) (((retro_unaligned_uint16_t *)p)->val)
#define retro_unaligned32(p) (((retro_unaligned_uint32_t *)p)->val)
#define retro_unaligned64(p) (((retro_unaligned_uint64_t *)p)->val)

/**
 * retro_get_unaligned_16be:
 * @addr        : pointer to unsigned 16-bit value
 *
 * Convert unsigned unaligned 16-bit value from big-endian to native.
 *
 * Returns: Native representation of big-endian val.
 **/

static INLINE uint16_t retro_get_unaligned_16be(void *addr) {
  return retro_be_to_cpu16(retro_unaligned16(addr));
}

/**
 * retro_get_unaligned_32be:
 * @addr        : pointer to unsigned 32-bit value
 *
 * Convert unsigned unaligned 32-bit value from big-endian to native.
 *
 * Returns: Native representation of big-endian val.
 **/

static INLINE uint32_t retro_get_unaligned_32be(void *addr) {
  return retro_be_to_cpu32(retro_unaligned32(addr));
}

/**
 * retro_get_unaligned_64be:
 * @addr        : pointer to unsigned 64-bit value
 *
 * Convert unsigned unaligned 64-bit value from big-endian to native.
 *
 * Returns: Native representation of big-endian val.
 **/

static INLINE uint64_t retro_get_unaligned_64be(void *addr) {
  return retro_be_to_cpu64(retro_unaligned64(addr));
}

/**
 * retro_get_unaligned_16le:
 * @addr        : pointer to unsigned 16-bit value
 *
 * Convert unsigned unaligned 16-bit value from little-endian to native.
 *
 * Returns: Native representation of little-endian val.
 **/

static INLINE uint16_t retro_get_unaligned_16le(void *addr) {
  return retro_le_to_cpu16(retro_unaligned16(addr));
}

/**
 * retro_get_unaligned_32le:
 * @addr        : pointer to unsigned 32-bit value
 *
 * Convert unsigned unaligned 32-bit value from little-endian to native.
 *
 * Returns: Native representation of little-endian val.
 **/

static INLINE uint32_t retro_get_unaligned_32le(void *addr) {
  return retro_le_to_cpu32(retro_unaligned32(addr));
}

/**
 * retro_get_unaligned_64le:
 * @addr        : pointer to unsigned 64-bit value
 *
 * Convert unsigned unaligned 64-bit value from little-endian to native.
 *
 * Returns: Native representation of little-endian val.
 **/

static INLINE uint64_t retro_get_unaligned_64le(void *addr) {
  return retro_le_to_cpu64(retro_unaligned64(addr));
}

/**
 * retro_set_unaligned_16le:
 * @addr        : pointer to unsigned 16-bit value
 * @val         : value to store
 *
 * Convert native value to unsigned unaligned 16-bit little-endian value
 *
 **/

static INLINE void retro_set_unaligned_16le(void *addr, uint16_t v) {
  retro_unaligned16(addr) = retro_cpu_to_le16(v);
}

/**
 * retro_set_unaligned_32le:
 * @addr        : pointer to unsigned 32-bit value
 * @val         : value to store
 *
 * Convert native value to unsigned unaligned 32-bit little-endian value
 *
 **/

static INLINE void retro_set_unaligned_32le(void *addr, uint32_t v) {
  retro_unaligned32(addr) = retro_cpu_to_le32(v);
}

/**
 * retro_set_unaligned_32le:
 * @addr        : pointer to unsigned 32-bit value
 * @val         : value to store
 *
 * Convert native value to unsigned unaligned 32-bit little-endian value
 *
 **/

static INLINE void retro_set_unaligned_64le(void *addr, uint64_t v) {
  retro_unaligned64(addr) = retro_cpu_to_le64(v);
}

/**
 * retro_set_unaligned_16be:
 * @addr        : pointer to unsigned 16-bit value
 * @val         : value to store
 *
 * Convert native value to unsigned unaligned 16-bit big-endian value
 *
 **/

static INLINE void retro_set_unaligned_16be(void *addr, uint16_t v) {
  retro_unaligned16(addr) = retro_cpu_to_be16(v);
}

/**
 * retro_set_unaligned_32be:
 * @addr        : pointer to unsigned 32-bit value
 * @val         : value to store
 *
 * Convert native value to unsigned unaligned 32-bit big-endian value
 *
 **/

static INLINE void retro_set_unaligned_32be(void *addr, uint32_t v) {
  retro_unaligned32(addr) = retro_cpu_to_be32(v);
}

/**
 * retro_set_unaligned_32be:
 * @addr        : pointer to unsigned 32-bit value
 * @val         : value to store
 *
 * Convert native value to unsigned unaligned 32-bit big-endian value
 *
 **/

static INLINE void retro_set_unaligned_64be(void *addr, uint64_t v) {
  retro_unaligned64(addr) = retro_cpu_to_be64(v);
}


#endif
