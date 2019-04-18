/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <limits.h>
#include "interface/mmal/util/mmal_util_rational.h"

#define Q16_ONE   (1 << 16)

#define ABS(v)    (((v) < 0) ? -(v) : (v))

/** Calculate the greatest common denominator between 2 integers.
 * Avoids division. */
static int32_t gcd(int32_t a, int32_t b)
{
   int shift;

   if (a == 0 || b == 0)
      return 1;

   a = ABS(a);
   b = ABS(b);
   for (shift = 0; !((a | b) & 0x01); shift++)
      a >>= 1, b >>= 1;

   while (a > 0)
   {
      while (!(a & 0x01))
         a >>= 1;
      while (!(b & 0x01))
         b >>= 1;
      if (a >= b)
         a = (a - b) >> 1;
      else
         b = (b - a) >> 1;
   }
   return b << shift;
}

/** Calculate a + b. */
MMAL_RATIONAL_T mmal_rational_add(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b)
{
   MMAL_RATIONAL_T result;
   int32_t g = gcd(a.den, b.den);
   a.den /= g;
   a.num = a.num * (b.den / g) + b.num * a.den;
   g = gcd(a.num, g);
   a.num /= g;
   a.den *= b.den / g;

   result.num = a.num;
   result.den = a.den;
   return result;
}

/** Calculate a - b. */
MMAL_RATIONAL_T mmal_rational_subtract(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b)
{
   b.num = -b.num;
   return mmal_rational_add(a, b);
}

/** Calculate a * b */
MMAL_RATIONAL_T mmal_rational_multiply(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b)
{
   MMAL_RATIONAL_T result;
   int32_t gcd1 = gcd(a.num, b.den);
   int32_t gcd2 = gcd(b.num, a.den);
   result.num = (a.num / gcd1) * (b.num / gcd2);
   result.den = (a.den / gcd2) * (b.den / gcd1);

   return result;
}

/** Calculate a / b */
MMAL_RATIONAL_T mmal_rational_divide(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b)
{
   MMAL_RATIONAL_T result;
   int32_t gcd1, gcd2;

   if (b.num == 0)
   {
      vcos_assert(0);
      return a;
   }

   if (a.num == 0)
      return a;

   gcd1 = gcd(a.num, b.num);
   gcd2 = gcd(b.den, a.den);
   result.num = (a.num / gcd1) * (b.den / gcd2);
   result.den = (a.den / gcd2) * (b.num / gcd1);

   return result;
}

/** Convert a rational number to a signed 32-bit Q16 number. */
int32_t mmal_rational_to_fixed_16_16(MMAL_RATIONAL_T rational)
{
   int64_t result = (int64_t)rational.num << 16;
   if (rational.den)
      result /= rational.den;

   if (result > INT_MAX)
      result = INT_MAX;
   else if (result < INT_MIN)
      result = INT_MIN;

   return (int32_t)result;
}

/** Convert a rational number to a signed 32-bit Q16 number. */
MMAL_RATIONAL_T mmal_rational_from_fixed_16_16(int32_t fixed)
{
   MMAL_RATIONAL_T result = { fixed, Q16_ONE };
   mmal_rational_simplify(&result);
   return result;
}

/** Reduce a rational number to it's simplest form. */
void mmal_rational_simplify(MMAL_RATIONAL_T *rational)
{
   int g = gcd(rational->num, rational->den);
   rational->num /= g;
   rational->den /= g;
}

/** Tests for equality */
MMAL_BOOL_T mmal_rational_equal(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b)
{
   if (a.num != b.num && a.num * (int64_t)b.num == 0)
      return MMAL_FALSE;
   return a.num * (int64_t)b.den == b.num * (int64_t)a.den;
}
