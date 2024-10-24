/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_miscellaneous.h).
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

#ifndef __RARCH_MISCELLANEOUS_H
#define __RARCH_MISCELLANEOUS_H

#define RARCH_MAX_SUBSYSTEMS 10
#define RARCH_MAX_SUBSYSTEM_ROMS 10

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

#if defined(_WIN32)

#if defined(_XBOX)
#include <Xtl.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#endif

#include <limits.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#ifdef IOS
#include <sys/param.h>
#endif

/**
 * Computes the bitwise OR of two bit arrays.
 *
 * @param a[in,out] The first bit array, and the location of the result.
 * @param b[in] The second bit array.
 * @param count The length of each bit array, in 32-bit words.
 */
static INLINE void bits_or_bits(uint32_t *a, uint32_t *b, uint32_t count)
{
   uint32_t i;
   for (i = 0; i < count;i++)
      a[i] |= b[i];
}

/**
 * Clears every bit in \c a that is set in \c b.
 *
 * @param a[in,out] The bit array to modify.
 * @param b[in] The bit array to use for reference.
 * @param count The length of each bit array, in 32-bit words
 * (\em not bits or bytes).
 */
static INLINE void bits_clear_bits(uint32_t *a, uint32_t *b, uint32_t count)
{
   uint32_t i;
   for (i = 0; i < count;i++)
      a[i] &= ~b[i];
}

/**
 * Checks if any bits in \c ptr are set.
 *
 * @param ptr The bit array to check.
 * @param count The length of the buffer pointed to by \c ptr, in 32-bit words
 * (\em not bits or bytes).
 * @return \c true if any bit in \c ptr is set,
 * \c false if all bits are clear (zero).
 */
static INLINE bool bits_any_set(uint32_t* ptr, uint32_t count)
{
   uint32_t i;
   for (i = 0; i < count; i++)
   {
      if (ptr[i] != 0)
         return true;
   }
   return false;
}

/**
 * Checks if any bits in \c a are different from those in \c b.
 *
 * @param a The first bit array to compare.
 * @param b The second bit array to compare.
 * @param count The length of each bit array, in 32-bit words
 * (\em not bits or bytes).
 * @return \c true if \c and \c differ by at least one bit,
 * \c false if they're both identical.
 */
static INLINE bool bits_any_different(uint32_t *a, uint32_t *b, uint32_t count)
{
   uint32_t i;
   for (i = 0; i < count; i++)
   {
      if (a[i] != b[i])
         return true;
   }
   return false;
}

/**
 * An upper limit for the length of a path (including the filename).
 * If a path is longer than this, it may not work properly.
 * This value may vary by platform.
 */

#if defined(_XBOX1) || defined(_3DS) || defined(PSP) || defined(PS2) || defined(GEKKO)|| defined(WIIU) || defined(__PSL1GHT__) || defined(__PS3__) || defined(HAVE_EMSCRIPTEN)

#ifndef PATH_MAX_LENGTH
#define PATH_MAX_LENGTH 512
#endif

#ifndef DIR_MAX_LENGTH
#define DIR_MAX_LENGTH 256
#endif

/**
 * An upper limit for the length of a file or directory (excluding parent directories).
 * If a path has a component longer than this, it may not work properly.
 */
#ifndef NAME_MAX_LENGTH
#define NAME_MAX_LENGTH 128
#endif

#else

#ifndef PATH_MAX_LENGTH
#define PATH_MAX_LENGTH 2048
#endif

#ifndef DIR_MAX_LENGTH
#define DIR_MAX_LENGTH 1024
#endif

/**
 * An upper limit for the length of a file or directory (excluding parent directories).
 * If a path has a component longer than this, it may not work properly.
 */
#ifndef NAME_MAX_LENGTH
#define NAME_MAX_LENGTH 256
#endif

#endif


#ifndef MAX
/**
 * @return \c a or \c b, whichever is larger.
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
/**
 * @return \c a or \c b, whichever is smaller.
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/**
 * Gets the number of elements in an array whose size is known at compile time.
 * @param a An array of fixed length.
 * @return The number of elements in \c a.
 */
#define ARRAY_SIZE(a)              (sizeof(a) / sizeof((a)[0]))

/** @defgroup BITS Bit Arrays
 *
 * @{
 */

#define BITS_GET_ELEM(a, i)        ((a).data[i])
#define BITS_GET_ELEM_PTR(a, i)    ((a)->data[i])

/** @defgroup BIT_ Arbitrary-length Bit Arrays
 *
 * @{
 */

/**
 * Sets a particular bit within a bit array to 1.
 *
 * @param a A \c uint8_t array,
 * treated here as a bit vector.
 * @param bit Index of the bit to set, where 0 is the least significant.
 */
#define BIT_SET(a, bit)   ((a)[(bit) >> 3] |=  (1 << ((bit) & 7)))

/**
 * Clears a particular bit within a bit array.
 *
 * @param a A \c uint8_t array,
 * treated here as a bit vector.
 * @param bit Index of the bit to clear, where 0 is the least significant.
 */
#define BIT_CLEAR(a, bit) ((a)[(bit) >> 3] &= ~(1 << ((bit) & 7)))

/**
 * Gets the value of a particular bit within a bit array.
 *
 * @param a A \c uint8_t array,
 * treated here as a bit vector.
 * @param bit Index of the bit to get, where 0 is the least significant.
 * @return The value of the bit at the specified index.
 */
#define BIT_GET(a, bit)   (((a)[(bit) >> 3] >> ((bit) & 7)) & 1)

/** @} */

/** @defgroup BIT16 16-bit Bit Arrays
 *
 * @{
 */

/**
 * Sets a particular bit within a 16-bit integer to 1.
 * @param a An unsigned 16-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to set, where 0 is the least significant and 15 is the most.
 */
#define BIT16_SET(a, bit)    ((a) |=  (1 << ((bit) & 15)))

/**
 * Clears a particular bit within a 16-bit integer.
 *
 * @param a An unsigned 16-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to clear, where 0 is the least significant and 15 is the most.
 */
#define BIT16_CLEAR(a, bit)  ((a) &= ~(1 << ((bit) & 15)))

/**
 * Gets the value of a particular bit within a 16-bit integer.
 *
 * @param a An unsigned 16-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to get, where 0 is the least significant and 15 is the most.
 * @return The value of the bit at the specified index.
 */
#define BIT16_GET(a, bit)    (((a) >> ((bit) & 15)) & 1)

/**
 * Clears all bits in a 16-bit bitmask.
 */
#define BIT16_CLEAR_ALL(a)   ((a) = 0)

/** @} */

/** @defgroup BIT32 32-bit Bit Arrays
 *
 * @{
 */

/**
 * Sets a particular bit within a 32-bit integer to 1.
 *
 * @param a An unsigned 32-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to set, where 0 is the least significant and 31 is the most.
 */
#define BIT32_SET(a, bit)    ((a) |=  (UINT32_C(1) << ((bit) & 31)))

/**
 * Clears a particular bit within a 32-bit integer.
 *
 * @param a An unsigned 32-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to clear, where 0 is the least significant and 31 is the most.
 */
#define BIT32_CLEAR(a, bit)  ((a) &= ~(UINT32_C(1) << ((bit) & 31)))

/**
 * Gets the value of a particular bit within a 32-bit integer.
 *
 * @param a An unsigned 32-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to get, where 0 is the least significant and 31 is the most.
 * @return The value of the bit at the specified index.
 */
#define BIT32_GET(a, bit)    (((a) >> ((bit) & 31)) & 1)

/**
 * Clears all bits in a 32-bit bitmask.
 *
 * @param a An unsigned 32-bit integer,
 * treated as a bit array.
 */
#define BIT32_CLEAR_ALL(a)   ((a) = 0)

/** @} */

/**
 * @defgroup BIT64 64-bit Bit Arrays
 * @{
 */

/**
 * Sets a particular bit within a 64-bit integer to 1.
 *
 * @param a An unsigned 64-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to set, where 0 is the least significant and 63 is the most.
 */
#define BIT64_SET(a, bit)    ((a) |=  (UINT64_C(1) << ((bit) & 63)))

/**
 * Clears a particular bit within a 64-bit integer.
 *
 * @param a An unsigned 64-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to clear, where 0 is the least significant and 63 is the most.
 */
#define BIT64_CLEAR(a, bit)  ((a) &= ~(UINT64_C(1) << ((bit) & 63)))

/**
 * Gets the value of a particular bit within a 64-bit integer.
 *
 * @param a An unsigned 64-bit integer,
 * treated as a bit array.
 * @param bit Index of the bit to get, where 0 is the least significant and 63 is the most.
 * @return The value of the bit at the specified index.
 */
#define BIT64_GET(a, bit)    (((a) >> ((bit) & 63)) & 1)

/**
 * Clears all bits in a 64-bit bitmask.
 *
 * @param a An unsigned 64-bit integer,
 * treated as a bit array.
 */
#define BIT64_CLEAR_ALL(a)   ((a) = 0)

/** @} */

#define BIT128_SET(a, bit)   ((a).data[(bit) >> 5] |=  (UINT32_C(1) << ((bit) & 31)))
#define BIT128_CLEAR(a, bit) ((a).data[(bit) >> 5] &= ~(UINT32_C(1) << ((bit) & 31)))
#define BIT128_GET(a, bit)   (((a).data[(bit) >> 5] >> ((bit) & 31)) & 1)
#define BIT128_CLEAR_ALL(a)  memset(&(a), 0, sizeof(a))

#define BIT128_SET_PTR(a, bit)   BIT128_SET(*a, bit)
#define BIT128_CLEAR_PTR(a, bit) BIT128_CLEAR(*a, bit)
#define BIT128_GET_PTR(a, bit)   BIT128_GET(*a, bit)
#define BIT128_CLEAR_ALL_PTR(a)  BIT128_CLEAR_ALL(*a)

/**
 * Sets a single bit from a 256-bit \c retro_bits_t to 1.
 *
 * @param a A 256-bit \c retro_bits_t.
 * @param bit Index of the bit to set,
 * where 0 is the least significant and 255 is the most.
 */
#define BIT256_SET(a, bit)       BIT128_SET(a, bit)

/**
 * Clears a single bit from a 256-bit \c retro_bits_t.
 *
 * @param a A 256-bit \c retro_bits_t.
 * @param bit Index of the bit to clear,
 * where 0 is the least significant and 255 is the most.
 */
#define BIT256_CLEAR(a, bit)     BIT128_CLEAR(a, bit)

/**
 * Gets the value of a single bit from a 256-bit \c retro_bits_t.
 *
 * @param a A 256-bit \c retro_bits_t.
 * @param bit Index of the bit to get,
 * where 0 is the least significant and 255 is the most.
 * @return The value of the bit at the specified index.
 */
#define BIT256_GET(a, bit)       BIT128_GET(a, bit)

/**
 * Clears all bits in a 256-bit \c retro_bits_t.
 *
 * @param a A 256-bit \c retro_bits_t.
 */
#define BIT256_CLEAR_ALL(a)      BIT128_CLEAR_ALL(a)

/** Variant of BIT256_SET() that takes a pointer to a \c retro_bits_t. */
#define BIT256_SET_PTR(a, bit)   BIT256_SET(*a, bit)

/** Variant of BIT256_CLEAR() that takes a pointer to a \c retro_bits_t. */
#define BIT256_CLEAR_PTR(a, bit) BIT256_CLEAR(*a, bit)

/** Variant of BIT256_GET() that takes a pointer to a \c retro_bits_t. */
#define BIT256_GET_PTR(a, bit)   BIT256_GET(*a, bit)

/** Variant of BIT256_CLEAR_ALL() that takes a pointer to a \c retro_bits_t. */
#define BIT256_CLEAR_ALL_PTR(a)  BIT256_CLEAR_ALL(*a)

/**
 * Sets a single bit from a 512-bit \c retro_bits_512_t to 1.
 *
 * @param a A 512-bit \c retro_bits_512_t.
 * @param bit Index of the bit to set,
 * where 0 is the least significant and 511 is the most.
 */
#define BIT512_SET(a, bit)       BIT256_SET(a, bit)

/**
 * Clears a single bit from a 512-bit \c retro_bits_512_t.
 *
 * @param a A 512-bit \c retro_bits_512_t.
 * @param bit Index of the bit to clear,
 * where 0 is the least significant and 511 is the most.
 */
#define BIT512_CLEAR(a, bit)     BIT256_CLEAR(a, bit)

/**
 * Gets the value of a single bit from a 512-bit \c retro_bits_512_t.
 *
 * @param a A 512-bit \c retro_bits_512_t.
 * @param bit Index of the bit to get,
 * where 0 is the least significant and 511 is the most.
 * @return The value of the bit at the specified index.
 */
#define BIT512_GET(a, bit)       BIT256_GET(a, bit)

/**
 * Clears all bits in a 512-bit \c retro_bits_512_t.
 *
 * @param a A 512-bit \c retro_bits_512_t.
 */
#define BIT512_CLEAR_ALL(a)      BIT256_CLEAR_ALL(a)

/** Variant of BIT512_SET() that takes a pointer to a \c retro_bits_512_t. */
#define BIT512_SET_PTR(a, bit)   BIT512_SET(*a, bit)

/** Variant of BIT512_CLEAR() that takes a pointer to a \c retro_bits_512_t. */
#define BIT512_CLEAR_PTR(a, bit) BIT512_CLEAR(*a, bit)

/** Variant of BIT512_GET() that takes a pointer to a \c retro_bits_512_t. */
#define BIT512_GET_PTR(a, bit)   BIT512_GET(*a, bit)

/** Variant of BIT512_CLEAR_ALL() that takes a pointer to a \c retro_bits_512_t. */
#define BIT512_CLEAR_ALL_PTR(a)  BIT512_CLEAR_ALL(*a)

#define BITS_COPY16_PTR(a,bits) \
{ \
   BIT128_CLEAR_ALL_PTR(a); \
   BITS_GET_ELEM_PTR(a, 0) = (bits) & 0xffff; \
}

#define BITS_COPY32_PTR(a,bits) \
{ \
   BIT128_CLEAR_ALL_PTR(a); \
   BITS_GET_ELEM_PTR(a, 0) = (bits); \
}

#define BITS_COPY64_PTR(a,bits) \
{ \
   BIT128_CLEAR_ALL_PTR(a); \
   BITS_GET_ELEM_PTR(a, 0) = (bits); \
   BITS_GET_ELEM_PTR(a, 1) = (bits >> 32); \
}

/* Helper macros and struct to keep track of many booleans. */

/** A 256-bit boolean array. */
typedef struct
{
   /** @private 256 bits. Not intended for direct use. */
   uint32_t data[8];
} retro_bits_t;

/** A 512-bit boolean array. */
typedef struct
{
   /** @private 512 bits. Not intended for direct use. */
   uint32_t data[16];
} retro_bits_512_t;

/** @} */

#ifdef _WIN32
#  ifdef _WIN64
#    define PRI_SIZET PRIu64
#  else
#    if _MSC_VER == 1800
#      define PRI_SIZET PRIu32
#    else
#      define PRI_SIZET "u"
#    endif
#  endif
#elif defined(PS2)
#  define PRI_SIZET "u"
#else
#  if (SIZE_MAX == 0xFFFF)
#    define PRI_SIZET "hu"
#  elif (SIZE_MAX == 0xFFFFFFFF)
#    define PRI_SIZET "u"
#  elif (SIZE_MAX == 0xFFFFFFFFFFFFFFFF)
#    define PRI_SIZET "lu"
#  else
#    error PRI_SIZET: unknown SIZE_MAX
#  endif
#endif

#endif
