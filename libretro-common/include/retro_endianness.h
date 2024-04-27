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
/**
 * Swaps the byte order of a 16-bit unsigned integer.
 * @param x The integer to byteswap.
 * @return \c with its two bytes swapped.
 */
static INLINE uint16_t SWAP16(uint16_t x)
{
  return ((x & 0x00ff) << 8) |
         ((x & 0xff00) >> 8);
}

/**
 * Swaps the byte order of a 32-bit unsigned integer.
 * @param x The integer to byteswap.
 * @return \c with its bytes swapped.
 */
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
/**
 * Swaps the byte order of a 64-bit unsigned integer.
 * @param x The integer to byteswap.
 * @return \c with its bytes swapped.
 */
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

#ifdef _MSC_VER
/* MSVC pre-defines macros depending on target arch */
#if defined (_M_IX86) || defined (_M_AMD64) || defined (_M_ARM) || defined (_M_ARM64)
#ifndef LSB_FIRST
#define LSB_FIRST 1
#endif
#elif _M_PPC
#ifndef MSB_FIRST
#define MSB_FIRST 1
#endif
#else
/* MSVC can run on _M_ALPHA and _M_IA64 too, but they're both bi-endian; need to find what mode MSVC runs them at */
#error "unknown platform, can't determine endianness"
#endif
#else
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#ifndef MSB_FIRST
#define MSB_FIRST 1
#endif
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#ifndef LSB_FIRST
#define LSB_FIRST 1
#endif
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
 * Checks if the current CPU is little-endian.
 *
 * @return \c true on little-endian CPUs,
 * \c false on big-endian CPUs.
 */
#define is_little_endian() RETRO_IS_LITTLE_ENDIAN

/**
 * Byte-swaps an unsigned 64-bit integer on big-endian CPUs.
 *
 * @param val The value to byteswap if necessary.
 * @return \c val byteswapped on big-endian CPUs,
 * \c val unchanged on little-endian CPUs.
 */
#if RETRO_IS_BIG_ENDIAN
#define swap_if_big64(val) (SWAP64(val))
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_big64(val) (val)
#endif

/**
 * Byte-swaps an unsigned 32-bit integer on big-endian CPUs.
 *
 * @param val The value to byteswap if necessary.
 * @return \c val byteswapped on big-endian CPUs,
 * \c val unchanged on little-endian CPUs.
 */
#if RETRO_IS_BIG_ENDIAN
#define swap_if_big32(val) (SWAP32(val))
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_big32(val) (val)
#endif

/**
 * Byte-swaps an unsigned 64-bit integer on little-endian CPUs.
 *
 * @param val The value to byteswap if necessary.
 * @return \c val byteswapped on little-endian CPUs,
 * \c val unchanged on big-endian CPUs.
 */
#if RETRO_IS_BIG_ENDIAN
#define swap_if_little64(val) (val)
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_little64(val) (SWAP64(val))
#endif

/**
 * Byte-swaps an unsigned 32-bit integer on little-endian CPUs.
 *
 * @param val The value to byteswap if necessary.
 * @return \c val byteswapped on little-endian CPUs,
 * \c val unchanged on big-endian CPUs.
 */
#if RETRO_IS_BIG_ENDIAN
#define swap_if_little32(val) (val)
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_little32(val) (SWAP32(val))
#endif

/**
 * Byte-swaps an unsigned 16-bit integer on big-endian systems.
 *
 * @param val The value to byteswap if necessary.
 * @return \c val byteswapped on big-endian systems,
 * \c val unchanged on little-endian systems.
 */
#if RETRO_IS_BIG_ENDIAN
#define swap_if_big16(val) (SWAP16(val))
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_big16(val) (val)
#endif

/**
 * Byte-swaps an unsigned 16-bit integer on little-endian systems.
 *
 * @param val The value to byteswap if necessary.
 * @return \c val byteswapped on little-endian systems,
 * \c val unchanged on big-endian systems.
 */
#if RETRO_IS_BIG_ENDIAN
#define swap_if_little16(val) (val)
#elif RETRO_IS_LITTLE_ENDIAN
#define swap_if_little16(val) (SWAP16(val))
#endif

/**
 * Stores a 32-bit integer in at the given address, in big-endian order.
 *
 * @param addr The address to store the value at.
 * Behavior is undefined if \c NULL or not aligned to a 32-bit boundary.
 * @param data The value to store in \c addr.
 * Will be byteswapped if on a little-endian CPU.
 */
static INLINE void store32be(uint32_t *addr, uint32_t data)
{
   *addr = swap_if_little32(data);
}

/**
 * Loads a 32-bit integer order from the given address, in big-endian order.
 *
 * @param addr The address to load the value from.
 * Behavior is undefined if \c NULL or not aligned to a 32-bit boundary.
 * @return The value at \c addr, byteswapped if on a little-endian CPU.
 */
static INLINE uint32_t load32be(const uint32_t *addr)
{
   return swap_if_little32(*addr);
}

/**
 * Converts the given unsigned 16-bit integer to little-endian order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a big-endian CPU,
 * unchanged otherwise.
 */
#define retro_cpu_to_le16(val) swap_if_big16(val)

/**
 * Converts the given unsigned 32-bit integer to little-endian order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a big-endian CPU,
 * unchanged otherwise.
 */
#define retro_cpu_to_le32(val) swap_if_big32(val)

/**
 * Converts the given unsigned 64-bit integer to little-endian order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a big-endian CPU,
 * unchanged otherwise.
 */
#define retro_cpu_to_le64(val) swap_if_big64(val)

/**
 * Converts the given unsigned 16-bit integer to host-native order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a big-endian CPU,
 * unchanged otherwise.
 */
#define retro_le_to_cpu16(val) swap_if_big16(val)

/**
 * Converts the given unsigned 32-bit integer to host-native order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a big-endian CPU,
 * unchanged otherwise.
 */
#define retro_le_to_cpu32(val) swap_if_big32(val)

/**
 * Converts the given unsigned 64-bit integer to host-native order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a big-endian CPU,
 * unchanged otherwise.
 */
#define retro_le_to_cpu64(val) swap_if_big64(val)

/**
 * Converts the given unsigned 16-bit integer to big-endian order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a little-endian CPU,
 * unchanged otherwise.
 */
#define retro_cpu_to_be16(val) swap_if_little16(val)

/**
 * Converts the given unsigned 32-bit integer to big-endian order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a little-endian CPU,
 * unchanged otherwise.
 */
#define retro_cpu_to_be32(val) swap_if_little32(val)

/**
 * Converts the given unsigned 64-bit integer to big-endian order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a little-endian CPU,
 * unchanged otherwise.
 */
#define retro_cpu_to_be64(val) swap_if_little64(val)

/**
 * Converts the given unsigned 16-bit integer from big-endian to host-native order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a little-endian CPU,
 * unchanged otherwise.
 */
#define retro_be_to_cpu16(val) swap_if_little16(val)

/**
 * Converts the given unsigned 32-bit integer from big-endian to host-native order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a little-endian CPU,
 * unchanged otherwise.
 */
#define retro_be_to_cpu32(val) swap_if_little32(val)

/**
 * Converts the given unsigned 64-bit integer from big-endian to host-native order if necessary.
 *
 * @param val The value to convert if necessary.
 * @return \c val byteswapped if on a little-endian CPU,
 * unchanged otherwise.
 */
#define retro_be_to_cpu64(val) swap_if_little64(val)

#ifdef  __GNUC__
/**
 * This attribute indicates that pointers to this type may alias
 * to pointers of any other type, similar to \c void* or \c char*.
 */
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

/**
 * A wrapper around a \c uint16_t that allows unaligned access
 * where supported by the compiler.
 */
typedef struct retro_unaligned_uint16_s retro_unaligned_uint16_t;

/**
 * A wrapper around a \c uint32_t that allows unaligned access
 * where supported by the compiler.
 */
typedef struct retro_unaligned_uint32_s retro_unaligned_uint32_t;

/**
 * A wrapper around a \c uint64_t that allows unaligned access
 * where supported by the compiler.
 */
typedef struct retro_unaligned_uint64_s retro_unaligned_uint64_t;

/* L-value references to unaligned pointers.  */
#define retro_unaligned16(p) (((retro_unaligned_uint16_t *)p)->val)
#define retro_unaligned32(p) (((retro_unaligned_uint32_t *)p)->val)
#define retro_unaligned64(p) (((retro_unaligned_uint64_t *)p)->val)

/**
 * Reads a 16-bit unsigned integer from the given address
 * and converts it from big-endian to host-native order (if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address of the integer to read.
 * Does not need to be divisible by 2
 * the way a \c uint16_t* usually would be.
 * @return The first two bytes of \c addr as a 16-bit unsigned integer,
 * byteswapped from big-endian to host-native order if necessary.
 */
static INLINE uint16_t retro_get_unaligned_16be(void *addr) {
  return retro_be_to_cpu16(retro_unaligned16(addr));
}

/**
 * Reads a 32-bit unsigned integer from the given address
 * and converts it from big-endian to host-native order (if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address of the integer to read.
 * Does not need to be divisible by 4
 * the way a \c uint32_t* usually would be.
 * @return The first four bytes of \c addr as a 32-bit unsigned integer,
 * byteswapped from big-endian to host-native order if necessary.
 */
static INLINE uint32_t retro_get_unaligned_32be(void *addr) {
  return retro_be_to_cpu32(retro_unaligned32(addr));
}

/**
 * Reads a 64-bit unsigned integer from the given address
 * and converts it from big-endian to host-native order (if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address of the integer to read.
 * Does not need to be divisible by 8
 * the way a \c uint64_t* usually would be.
 * @return The first eight bytes of \c addr as a 64-bit unsigned integer,
 * byteswapped from big-endian to host-native order if necessary.
 */
static INLINE uint64_t retro_get_unaligned_64be(void *addr) {
  return retro_be_to_cpu64(retro_unaligned64(addr));
}

/**
 * Reads a 16-bit unsigned integer from the given address
 * and converts it from little-endian to host-native order (if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address of the integer to read.
 * Does not need to be divisible by 2
 * the way a \c uint16_t* usually would be.
 * @return The first two bytes of \c addr as a 16-bit unsigned integer,
 * byteswapped from little-endian to host-native order if necessary.
 */
static INLINE uint16_t retro_get_unaligned_16le(void *addr) {
  return retro_le_to_cpu16(retro_unaligned16(addr));
}

/**
 * Reads a 32-bit unsigned integer from the given address
 * and converts it from little-endian to host-native order (if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address of the integer to read.
 * Does not need to be divisible by 4
 * the way a \c uint32_t* usually would be.
 * @return The first four bytes of \c addr as a 32-bit unsigned integer,
 * byteswapped from little-endian to host-native order if necessary.
 */
static INLINE uint32_t retro_get_unaligned_32le(void *addr) {
  return retro_le_to_cpu32(retro_unaligned32(addr));
}

/**
 * Reads a 64-bit unsigned integer from the given address
 * and converts it from little-endian to host-native order (if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address of the integer to read.
 * Does not need to be divisible by 8
 * the way a \c uint64_t* usually would be.
 * @return The first eight bytes of \c addr as a 64-bit unsigned integer,
 * byteswapped from little-endian to host-native order if necessary.
 */
static INLINE uint64_t retro_get_unaligned_64le(void *addr) {
  return retro_le_to_cpu64(retro_unaligned64(addr));
}

/**
 * Writes a 16-bit unsigned integer to the given address
 * (converted to little-endian order if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address to write the integer to.
 * Does not need to be divisible by 2
 * the way a \c uint16_t* usually would be.
 * @param v The value to write.
 */
static INLINE void retro_set_unaligned_16le(void *addr, uint16_t v) {
  retro_unaligned16(addr) = retro_cpu_to_le16(v);
}

/**
 * Writes a 32-bit unsigned integer to the given address
 * (converted to little-endian order if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address to write the integer to.
 * Does not need to be divisible by 4
 * the way a \c uint32_t* usually would be.
 * @param v The value to write.
 */
static INLINE void retro_set_unaligned_32le(void *addr, uint32_t v) {
  retro_unaligned32(addr) = retro_cpu_to_le32(v);
}

/**
 * Writes a 64-bit unsigned integer to the given address
 * (converted to little-endian order if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address to write the integer to.
 * Does not need to be divisible by 8
 * the way a \c uint64_t* usually would be.
 * @param v The value to write.
 */
static INLINE void retro_set_unaligned_64le(void *addr, uint64_t v) {
  retro_unaligned64(addr) = retro_cpu_to_le64(v);
}

/**
 * Writes a 16-bit unsigned integer to the given address
 * (converted to big-endian order if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address to write the integer to.
 * Does not need to be divisible by 2
 * the way a \c uint16_t* usually would be.
 * @param v The value to write.
 */
static INLINE void retro_set_unaligned_16be(void *addr, uint16_t v) {
  retro_unaligned16(addr) = retro_cpu_to_be16(v);
}

/**
 * Writes a 32-bit unsigned integer to the given address
 * (converted to big-endian order if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address to write the integer to.
 * Does not need to be divisible by 4
 * the way a \c uint32_t* usually would be.
 * @param v The value to write.
 */
static INLINE void retro_set_unaligned_32be(void *addr, uint32_t v) {
  retro_unaligned32(addr) = retro_cpu_to_be32(v);
}

/**
 * Writes a 64-bit unsigned integer to the given address
 * (converted to big-endian order if necessary),
 * regardless of the CPU's alignment requirements.
 *
 * @param addr The address to write the integer to.
 * Does not need to be divisible by 8
 * the way a \c uint64_t* usually would be.
 * @param v The value to write.
 */
static INLINE void retro_set_unaligned_64be(void *addr, uint64_t v) {
  retro_unaligned64(addr) = retro_cpu_to_be64(v);
}


#endif
