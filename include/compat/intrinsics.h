/* Copyright  (C) 2010-2018 The RetroArch team
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

/* Count Leading Zero, unsigned 16bit input value */
static INLINE unsigned compat_clz_u16(uint16_t val)
{
#ifdef __GNUC__
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

/* Count Trailing Zero */
static INLINE int compat_ctz(unsigned x)
{
#if defined(__GNUC__) && !defined(RARCH_CONSOLE)
   return __builtin_ctz(x);
#elif _MSC_VER >= 1400 && !defined(_XBOX)
   unsigned long r = 0;
   _BitScanReverse((unsigned long*)&r, x);
   return (int)r;
#else
/* Only checks at nibble granularity,
 * because that's what we need. */
   if (x & 0x000f)
      return 0;
   if (x & 0x00f0)
      return 4;
   if (x & 0x0f00)
      return 8;
   if (x & 0xf000)
      return 12;
   return 16;
#endif
}

RETRO_END_DECLS

#endif
