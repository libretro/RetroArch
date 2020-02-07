/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fxp.h).
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

#ifndef __LIBRETRO_SDK_MATH_FXP_H__
#define __LIBRETRO_SDK_MATH_FXP_H__

#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <retro_inline.h>

static INLINE int64_t fx32_mul(const int32_t a, const int32_t b)
{
#ifdef _MSC_VER
   return __emul(a, b);
#else
   return ((int64_t)a) * ((int64_t)b);
#endif
}

static INLINE int32_t fx32_shiftdown(const int64_t a)
{
#ifdef _MSC_VER
	return (int32_t)__ll_rshift(a, 12);
#else
	return (int32_t)(a >> 12);
#endif
}

static INLINE int64_t fx32_shiftup(const int32_t a)
{
#ifdef _MSC_VER
	return __ll_lshift(a, 12);
#else
	return ((int64_t)a) << 12;
#endif
}

#endif
