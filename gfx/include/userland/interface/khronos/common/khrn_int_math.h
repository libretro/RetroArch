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

#ifndef KHRN_INT_MATH_H
#define KHRN_INT_MATH_H

#include "interface/khronos/common/khrn_int_util.h"
#include <math.h>
#ifndef __VIDEOCORE__  // threadsx/nucleus define LONG which clashses
#include "interface/vcos/vcos.h"
#endif

#define PI 3.1415926535897932384626433832795f
#define SQRT_2 1.4142135623730950488016887242097f
#define EPS 1.0e-10f

static INLINE float floor_(float x)
{
   return floorf(x);
}

static INLINE float ceil_(float x)
{
   return ceilf(x);
}

/*
   Preconditions:

   -

   Postconditions:

   returns the magnitude of x. returns +infinity if x is infinite. returns a nan
   if x is nan.
*/

static INLINE float absf_(float x)
{
   return (x < 0.0f) ? -x : x;
}

static INLINE float modf_(float x, float y)
{
   return fmodf(x, y);
}

static INLINE void sin_cos_(float *s, float *c, float angle)
{
   *s = sinf(angle);
   *c = cosf(angle);
}

static INLINE float sin_(float angle)
{
   return sinf(angle);
}

static INLINE float cos_(float angle)
{
   return cosf(angle);
}

static INLINE float atan2_(float y, float x)
{
   return atan2f(y, x);
}

extern float acos_(float x);

static INLINE float exp_(float x)
{
   return expf(x);
}

extern float mod_one_(float x);

#ifdef _VIDEOCORE
#include <vc/intrinsics.h>

static INLINE float nan_recip_(float x)
{
   float est = _frecip(x);          /* initial estimate, accurate to ~12 bits */
   return est * (2.0f - (x * est)); /* refine with newton-raphson to get accuracy to ~24 bits */
}

static INLINE float rsqrt_(float x)
{
#ifndef NDEBUG
   vcos_verify(x > 0.0f);
#endif
   float est = _frsqrt(x);                         /* initial estimate, accurate to ~12 bits */
   return est * (1.5f - ((x * 0.5f) * est * est)); /* refine with newton-raphson to get accuracy to ~24 bits */
}
#else
static INLINE float nan_recip_(float x)
{
   return 1.0f / x;
}

static INLINE float rsqrt_(float x)
{
#ifndef NDEBUG
   vcos_verify(x > 0.0f);
#endif
   return 1.0f / sqrtf(x);
}
#endif

static INLINE float recip_(float x)
{
#ifndef NDEBUG
   vcos_verify(x != 0.0f);
#endif
   return nan_recip_(x);
}

static INLINE bool is_nan_(float x)
{
   uint32_t bits = float_to_bits(x);
   return ((bits & 0x7f800000) == 0x7f800000) && /* max exponent */
      (bits << 9); /* non-zero mantissa */
}

static INLINE bool nan_lt_(float x, float y)
{
   return
#ifndef KHRN_NAN_COMPARISONS_CORRECT
      !is_nan_(x) && !is_nan_(y) &&
#endif
      (x < y);
}

static INLINE bool nan_gt_(float x, float y)
{
   return
#ifndef KHRN_NAN_COMPARISONS_CORRECT
      !is_nan_(x) && !is_nan_(y) &&
#endif
      (x > y);
}

static INLINE bool nan_ne_(float x, float y)
{
   return
#ifndef KHRN_NAN_COMPARISONS_CORRECT
      !is_nan_(x) && !is_nan_(y) &&
#endif
      (x != y);
}

static INLINE float sqrt_(float x)
{
   vcos_assert(!nan_lt_(x, 0.0f));
   return sqrtf(x);
}

#endif
