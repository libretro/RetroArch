/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float16.h).
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
#ifndef __LIBRETRO_SDK_MATH_FLOAT16_H__
#define __LIBRETRO_SDK_MATH_FLOAT16_H__

/* IEEE 754 binary16 ("half") to and from binary32, exactly.
 *
 * Integer arithmetic only, so the result does not depend on the
 * FPU's flush-to-zero or denormals-are-zero modes: subnormal halves
 * widen to normal floats, and a float too small for a half rounds to
 * a subnormal half or a signed zero. Narrowing rounds to nearest,
 * ties to even, as F16C and GPUs do; past the largest half it gives
 * infinity. Infinities keep their sign, and a NaN stays a NaN with
 * its payload's high bits (a quiet bit is set where narrowing would
 * otherwise lose them all). */

#include <stdint.h>
#include <string.h>

#include <retro_inline.h>

static INLINE float retro_half_to_float(uint16_t h)
{
   uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
   uint32_t exp  = (h >> 10) & 0x1Fu;
   uint32_t mant = h & 0x3FFu;
   uint32_t bits;
   float    f;

   if (exp == 0x1Fu)                 /* infinity, NaN */
      bits = sign | 0x7F800000u | (mant << 13);
   else if (exp != 0)                /* normal */
      bits = sign | ((exp + (127 - 15)) << 23) | (mant << 13);
   else if (mant == 0)               /* signed zero */
      bits = sign;
   else
   {
      /* Subnormal: shift the leading 1 up to the implicit bit. */
      exp = 127 - 15 + 1;
      while (!(mant & 0x400u))
      {
         mant <<= 1;
         exp--;
      }
      bits = sign | (exp << 23) | ((mant & 0x3FFu) << 13);
   }
   memcpy(&f, &bits, sizeof(f));
   return f;
}

static INLINE uint16_t retro_float_to_half(float value)
{
   uint32_t bits;
   uint32_t sign, exp, mant;
   int32_t  e;

   memcpy(&bits, &value, sizeof(bits));
   sign = (bits >> 16) & 0x8000u;
   exp  = (bits >> 23) & 0xFFu;
   mant = bits & 0x7FFFFFu;

   if (exp == 0xFFu)                 /* infinity, NaN */
   {
      if (!mant)
         return (uint16_t)(sign | 0x7C00u);
      return (uint16_t)(sign | 0x7C00u | 0x200u | (mant >> 13));
   }

   e = (int32_t)exp - 127 + 15;
   if (e >= 0x1F)                    /* too large: infinity */
      return (uint16_t)(sign | 0x7C00u);

   if (e <= 0)
   {
      /* Subnormal half or zero. A float subnormal is far below the
       * smallest half and rounds to zero, as does anything below half
       * of the smallest half subnormal. */
      uint32_t shift, half_bit, rest;
      if (e < -10 || exp == 0)
         return (uint16_t)sign;
      mant    |= 0x800000u;          /* the implicit bit */
      shift    = (uint32_t)(14 - e); /* 14..24 */
      half_bit = 1u << (shift - 1);
      rest     = mant & ((1u << shift) - 1);
      mant   >>= shift;
      if (rest > half_bit || (rest == half_bit && (mant & 1u)))
         mant++;                     /* may carry into the smallest normal */
      return (uint16_t)(sign | mant);
   }

   {
      /* Normal: round the 23-bit mantissa to 10. A carry out of the
       * mantissa moves to the next exponent, and past the largest
       * exponent gives infinity, both by plain addition. */
      uint32_t half = ((uint32_t)e << 10) | (mant >> 13);
      uint32_t rest = mant & 0x1FFFu;
      if (rest > 0x1000u || (rest == 0x1000u && (half & 1u)))
         half++;
      return (uint16_t)(sign | half);
   }
}

#endif
