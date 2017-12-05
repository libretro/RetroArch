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

#define BITS_ANY_SET(a)            ( ((a).data[0])||((a).data[1])||((a).data[2])||((a).data[3])||       \
                                                  ((a).data[4])||((a).data[5])||((a).data[6])||((a).data[7]) )
#define BITS_ANY_SET_PTR(a)        ( ((a)->data[0])||((a)->data[1])||((a)->data[2])||((a)->data[3])||   \
                                                  ((a)->data[4])||((a)->data[5])||((a)->data[6])||((a)->data[7]) )
#define BITS_CLEAR_BITS(a,b)    \
            ((a).data[0])&=(~((b).data[0])); \
            ((a).data[1])&=(~((b).data[1])); \
            ((a).data[2])&=(~((b).data[2])); \
            ((a).data[3])&=(~((b).data[3])); \
            ((a).data[4])&=(~((b).data[4])); \
            ((a).data[5])&=(~((b).data[5])); \
            ((a).data[6])&=(~((b).data[6])); \
            ((a).data[7])&=(~((b).data[7]));

#define BITS_COPY16_PTR(a,bits) \
{ \
   BIT128_CLEAR_ALL_PTR(a); \
   ((a)->data[0] = (bits) & 0xffff); \
}

#define BITS_COPY32_PTR(a,bits) \
{ \
   BIT128_CLEAR_ALL_PTR(a); \
   ((a)->data[0] = (bits)); \
}


#define BIT_SET(a, bit)   ((a)[(bit) >> 3] |=  (1 << ((bit) & 7)))
#define BIT_CLEAR(a, bit) ((a)[(bit) >> 3] &= ~(1 << ((bit) & 7)))
#define BIT_GET(a, bit)   (((a).data[(bit) >> 3] >> ((bit) & 7)) & 1)

#define BIT16_SET(a, bit)    ((a) |=  (1 << ((bit) & 15)))
#define BIT16_CLEAR(a, bit)  ((a) &= ~(1 << ((bit) & 15)))
#define BIT16_GET(a, bit)    (((a) >> ((bit) & 15)) & 1)
#define BIT16_CLEAR_ALL(a)   ((a) = 0)

#define BIT32_SET(a, bit)    ((a) |=  (1 << ((bit) & 31)))
#define BIT32_CLEAR(a, bit)  ((a) &= ~(1 << ((bit) & 31)))
#define BIT32_GET(a, bit)    (((a) >> ((bit) & 31)) & 1)
#define BIT32_CLEAR_ALL(a)   ((a) = 0)

#define BIT64_SET(a, bit)    ((a) |=  (UINT64_C(1) << ((bit) & 63)))
#define BIT64_CLEAR(a, bit)  ((a) &= ~(UINT64_C(1) << ((bit) & 63)))
#define BIT64_GET(a, bit)    (((a) >> ((bit) & 63)) & 1)
#define BIT64_CLEAR_ALL(a)   ((a) = 0)

#define BIT128_SET(a, bit)   ((a).data[(bit) >> 5] |=  (1 << ((bit) & 31)))
#define BIT128_CLEAR(a, bit) ((a).data[(bit) >> 5] &= ~(1 << ((bit) & 31)))
#define BIT128_GET(a, bit)   (((a).data[(bit) >> 5] >> ((bit) & 31)) & 1)
#define BIT128_CLEAR_ALL(a)  memset(&(a), 0, sizeof(a))

#define BIT128_SET_PTR(a, bit)   BIT128_SET(*a, bit)
#define BIT128_CLEAR_PTR(a, bit) BIT128_CLEAR(*a, bit)
#define BIT128_GET_PTR(a, bit)   BIT128_GET(*a, bit)
#define BIT128_CLEAR_ALL_PTR(a)  BIT128_CLEAR_ALL(*a)

#define BIT256_SET(a, bit)       BIT128_SET(a, bit)
#define BIT256_CLEAR(a, bit)     BIT128_CLEAR(a, bit)
#define BIT256_GET(a, bit)       BIT128_GET(a, bit)
#define BIT256_CLEAR_ALL(a)      BIT128_CLEAR_ALL(a)

#define BIT256_SET_PTR(a, bit)   BIT256_SET(*a, bit)
#define BIT256_CLEAR_PTR(a, bit) BIT256_CLEAR(*a, bit)
#define BIT256_GET_PTR(a, bit)   BIT256_GET(*a, bit)
#define BIT256_CLEAR_ALL_PTR(a)  BIT256_CLEAR_ALL(*a)

/* Helper macros and struct to keep track of many booleans. */
/* This struct has 256 bits. */
typedef struct
{
   uint32_t data[8];
} retro_bits_t;

#endif
