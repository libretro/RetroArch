/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vector_2.h).
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

#ifndef __LIBRETRO_SDK_GFX_MATH_VECTOR_2_H__
#define __LIBRETRO_SDK_GFX_MATH_VECTOR_2_H__

#include <stdint.h>
#include <math.h>

#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

typedef float vec2_t[2];

#define vec2_dot(a, b)   ((a[0] * b[0]) + (a[1] * b[1]))

#define vec2_cross(a, b) ((a[0]*b[1]) - (a[1]*b[0]))

#define vec2_add(dst, src) \
   dst[0] += src[0]; \
   dst[1] += src[1]

#define vec2_subtract(dst, src) \
   dst[0] -= src[0]; \
   dst[1] -= src[1]

#define vec2_copy(dst, src) \
   dst[0] = src[0]; \
   dst[1] = src[1]

static INLINE float overflow(void)
{
   unsigned i;
   volatile float f = 1e10;

   for(i = 0; i < 10; ++i)
      f *= f;
   return f;
}

static INLINE int16_t tofloat16(float f)
{
	union uif32
   {
      float f;
      uint32_t i;
   };

   int i, s, e, m;
   union uif32 Entry;
   Entry.f = f;
   i       = (int)Entry.i;
   s       =  (i >> 16) & 0x00008000;
   e       = ((i >> 23) & 0x000000ff) - (127 - 15);
   m       =   i        & 0x007fffff;

   if(e <= 0)
   {
      if(e < -10)
         return (int16_t)(s);

      m = (m | 0x00800000) >> (1 - e);

      if(m & 0x00001000)
         m += 0x00002000;

      return (int16_t)(s | (m >> 13));
   }

   if(e == 0xff - (127 - 15))
   {
      if(m == 0)
         return (int16_t)(s | 0x7c00);

      m >>= 13;

      return (int16_t)(s | 0x7c00 | m | (m == 0));
   }

   if(m &  0x00001000)
   {
      m += 0x00002000;

      if(m & 0x00800000)
      {
         m =  0;
         e += 1;
      }
   }

   if (e > 30)
   {
      overflow();

      return (int16_t)(s | 0x7c00);
   }

   return (int16_t)(s | (e << 10) | (m >> 13));
}

static INLINE unsigned int vec2_packHalf2x16(float vec0, float vec1)
{
   union
   {
      int16_t in[2];
      unsigned int out;
   } u;

   u.in[0] = tofloat16(vec0);
   u.in[1] = tofloat16(vec1);

   return u.out;
}

RETRO_END_DECLS

#endif
