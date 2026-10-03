/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (intrinsics.h).
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

#ifndef __LIBRETRO_SDK_COMPAT_INTRINSICS_H
#define __LIBRETRO_SDK_COMPAT_INTRINSICS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <retro_common_api.h>
#include <retro_inline.h>

#if defined(_MSC_VER) && !defined(_XBOX)
#if (_MSC_VER > 1310)
#include <intrin.h>
#endif
#endif

RETRO_BEGIN_DECLS

/**
 * Counts the leading zero bits in a \c uint16_t.
 * Uses compiler intrinsics if available, or a standard C implementation if not.
 *
 * @param val Value to count leading zeroes in.
 * @return Number of leading zeroes in \c val.
 */
static INLINE unsigned compat_clz_u16(uint16_t val)
{
#if defined(__GNUC__)
   return __builtin_clz(val << 16 | 0x8000);
#elif _MSC_VER >= 1400 && !defined(_XBOX) && !defined(__WINRT__)
   /* Highest set bit via the hardware BSR, mirroring compat_ctz()'s use of
    * _BitScanForward, instead of the software loop below. val is zero-extended
    * to 32 bits, so its top set bit stays within [0, 15]. */
   unsigned long idx;
   if (_BitScanReverse(&idx, (unsigned long)val))
      return 15u - (unsigned)idx;
   return 16u;
#else
   unsigned ret = 0;

   while(!(val & 0x8000) && ret < 16)
   {
      val <<= 1;
      ret++;
   }

   return ret;
#endif
}

/**
 * Counts the trailing zero bits in a \c uint16_t.
 * Uses compiler intrinsics if available, or a standard C implementation if not.
 *
 * @param val Value to count trailing zeroes in.
 * @return Number of trailing zeroes in \c val.
 */
static INLINE int compat_ctz(unsigned x)
{
#if defined(__GNUC__) && !defined(RARCH_CONSOLE)
   return __builtin_ctz(x);
#elif _MSC_VER >= 1400 && !defined(_XBOX) && !defined(__WINRT__)
   unsigned long r = 0;
   _BitScanForward((unsigned long*)&r, x);
   return (int)r;
#else
   int count = 0;
   if (!(x & 0xffff))
   {
      x >>= 16;
      count |= 16;
   }
   if (!(x & 0xff))
   {
      x >>= 8;
      count |= 8;
   }
   if (!(x & 0xf))
   {
      x >>= 4;
      count |= 4;
   }
   if (!(x & 0x3))
   {
      x >>= 2;
      count |= 2;
   }
   if (!(x & 0x1))
      count |= 1;

   return count;
#endif
}

/**
 * Counts the leading zero bits in a \c uint32_t.
 *
 * @param x Value to count leading zeroes in.
 * @return Number of leading zeroes in \c x, and 32 for zero.
 */
static INLINE uint32_t compat_clz_u32(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
   /* Both builtins leave a zero argument undefined, so it never
    * reaches them. */
   return x ? (uint32_t)__builtin_clz(x) : 32u;
#elif defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(_XBOX) && !defined(__WINRT__)
   {
      unsigned long idx;
      if (_BitScanReverse(&idx, (unsigned long)x))
         return 31u - (uint32_t)idx;
      return 32u;
   }
#else
   {
      uint32_t n = 0;
      if (!x)
         return 32u;
      if (!(x & 0xffff0000u)) { n += 16; x <<= 16; }
      if (!(x & 0xff000000u)) { n +=  8; x <<=  8; }
      if (!(x & 0xf0000000u)) { n +=  4; x <<=  4; }
      if (!(x & 0xc0000000u)) { n +=  2; x <<=  2; }
      if (!(x & 0x80000000u)) { n +=  1; }
      return n;
   }
#endif
}

/**
 * Counts the leading zero bits in a \c uint64_t.
 *
 * @param x Value to count leading zeroes in.
 * @return Number of leading zeroes in \c x, and 64 for zero.
 */
static INLINE uint32_t compat_clz_u64(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
   return x ? (uint32_t)__builtin_clzll(x) : 64u;
#elif defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(_XBOX) && !defined(__WINRT__) \
   && (defined(_M_X64) || defined(_M_ARM64))
   {
      unsigned long idx;
      if (_BitScanReverse64(&idx, x))
         return 63u - (uint32_t)idx;
      return 64u;
   }
#else
   /* _BitScanReverse64 is 64-bit only, and the halving form below costs
    * one extra compare either way, so both remaining cases take the
    * high half first. */
   {
      uint32_t hi = (uint32_t)(x >> 32);
      if (hi)
         return compat_clz_u32(hi);
      return 32u + compat_clz_u32((uint32_t)x);
   }
#endif
}

/**
 * Counts the trailing zero bits in a \c uint64_t.
 *
 * @param x Value to count trailing zeroes in.
 * @return Number of trailing zeroes in \c x, and 64 for zero.
 */
static INLINE uint32_t compat_ctz_u64(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
   return x ? (uint32_t)__builtin_ctzll(x) : 64u;
#elif defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(_XBOX) && !defined(__WINRT__) \
   && (defined(_M_X64) || defined(_M_ARM64))
   {
      unsigned long idx;
      if (_BitScanForward64(&idx, x))
         return (uint32_t)idx;
      return 64u;
   }
#else
   {
      uint32_t lo = (uint32_t)x;
      uint32_t n;
      if (lo)
      {
         n = 0;
         if (!(lo & 0x0000ffffu)) { n += 16; lo >>= 16; }
         if (!(lo & 0x000000ffu)) { n +=  8; lo >>=  8; }
         if (!(lo & 0x0000000fu)) { n +=  4; lo >>=  4; }
         if (!(lo & 0x00000003u)) { n +=  2; lo >>=  2; }
         if (!(lo & 0x00000001u)) { n +=  1; }
         return n;
      }
      lo = (uint32_t)(x >> 32);
      if (!lo)
         return 64u;
      n = 32;
      if (!(lo & 0x0000ffffu)) { n += 16; lo >>= 16; }
      if (!(lo & 0x000000ffu)) { n +=  8; lo >>=  8; }
      if (!(lo & 0x0000000fu)) { n +=  4; lo >>=  4; }
      if (!(lo & 0x00000003u)) { n +=  2; lo >>=  2; }
      if (!(lo & 0x00000001u)) { n +=  1; }
      return n;
   }
#endif
}

/**
 * Index of the highest set bit in a \c uint32_t, the floor of its
 * base-two logarithm.
 *
 * @param x Value to inspect, which must not be zero.
 * @return Index of the highest set bit, counting from zero.
 */
static INLINE uint32_t compat_highbit_u32(uint32_t x)
{
   return 31u - compat_clz_u32(x);
}

RETRO_END_DECLS

#endif
