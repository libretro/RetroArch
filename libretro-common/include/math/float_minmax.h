/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_minmax.h).
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
#ifndef __LIBRETRO_SDK_MATH_FLOAT_MINMAX_H__
#define __LIBRETRO_SDK_MATH_FLOAT_MINMAX_H__

#include <stdint.h>
#include <math.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_environment.h>

#ifdef __SSE2__
#include <emmintrin.h>
#include <mmintrin.h>
#endif

static INLINE float float_min(float a, float b)
{
#ifdef __SSE2__
   _mm_store_ss( &a, _mm_min_ss(_mm_set_ss(a),_mm_set_ss(b)) );
   return a;
#elif !defined(DJGPP) && (defined(__STDC_C99__) || defined(__STDC_C11__))
   return fminf(a, b);
#else
   return MIN(a, b);
#endif
}

static INLINE float float_max(float a, float b)
{
#ifdef __SSE2__
   _mm_store_ss( &a, _mm_max_ss(_mm_set_ss(a),_mm_set_ss(b)) );
   return a;
#elif !defined(DJGPP) && (defined(__STDC_C99__) || defined(__STDC_C11__))
   return fmaxf(a, b);
#else
   return MAX(a, b);
#endif
}

#endif
