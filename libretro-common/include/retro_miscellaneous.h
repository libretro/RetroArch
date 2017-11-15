/* Copyright  (C) 2010-2017 The RetroArch team
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

#include <stdint.h>

#if defined(_WIN32) && !defined(_XBOX)		
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN		
#endif
#include <windows.h>		
#elif defined(_WIN32) && defined(_XBOX)		
#include <Xtl.h>		
#endif

#include <limits.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#ifndef PATH_MAX_LENGTH
#if defined(_XBOX1) || defined(_3DS) || defined(PSP) || defined(GEKKO)|| defined(WIIU)
#define PATH_MAX_LENGTH 512
#else
#define PATH_MAX_LENGTH 4096
#endif
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define BIT_SET(a, bit)   ((a)[(bit) >> 3] |=  (1 << ((bit) & 7)))
#define BIT_CLEAR(a, bit) ((a)[(bit) >> 3] &= ~(1 << ((bit) & 7)))
#define BIT_GET(a, bit)   ((a)[(bit) >> 3] &   (1 << ((bit) & 7)))

#define BIT16_SET(a, bit)    ((a) |=  (1 << ((bit) & 15)))
#define BIT16_CLEAR(a, bit)  ((a) &= ~(1 << ((bit) & 15)))
#define BIT16_GET(a, bit) (!!((a) &   (1 << ((bit) & 15))))
#define BIT16_CLEAR_ALL(a)   ((a) = 0)

#define BIT32_SET(a, bit)    ((a) |=  (1 << ((bit) & 31)))
#define BIT32_CLEAR(a, bit)  ((a) &= ~(1 << ((bit) & 31)))
#define BIT32_GET(a, bit) (!!((a) &   (1 << ((bit) & 31))))
#define BIT32_CLEAR_ALL(a)   ((a) = 0)

#define BIT64_SET(a, bit)    ((a) |=  (UINT64_C(1) << ((bit) & 63)))
#define BIT64_CLEAR(a, bit)  ((a) &= ~(UINT64_C(1) << ((bit) & 63)))
#define BIT64_GET(a, bit) (!!((a) &   (UINT64_C(1) << ((bit) & 63))))
#define BIT64_CLEAR_ALL(a)   ((a) = 0)

#define BIT128_SET(a, bit)   ((a).data[(bit) >> 5] |=  (1 << ((bit) & 31)))
#define BIT128_CLEAR(a, bit) ((a).data[(bit) >> 5] &= ~(1 << ((bit) & 31)))
#define BIT128_GET(a, bit)   ((a).data[(bit) >> 5] &   (1 << ((bit) & 31)))
#define BIT128_CLEAR_ALL(a)  memset(&(a), 0, sizeof(a));

/* Helper macros and struct to keep track of many booleans.
 * To check for multiple bits, use &&, not &.
 * For OR, | can be used. */
typedef struct
{
   uint32_t data[8];
} retro_bits_t;

#endif
