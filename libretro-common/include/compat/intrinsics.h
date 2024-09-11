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

RETRO_END_DECLS

#endif
