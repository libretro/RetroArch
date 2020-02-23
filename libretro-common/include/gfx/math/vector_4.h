/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vector_4.h).
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

#ifndef __LIBRETRO_SDK_GFX_MATH_VECTOR_4_H__
#define __LIBRETRO_SDK_GFX_MATH_VECTOR_4_H__

#include <stdint.h>
#include <math.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef float vec4_t[4];

#define vec4_add(dst, src) \
   dst[0] += src[0]; \
   dst[1] += src[1]; \
   dst[2] += src[2]; \
   dst[3] += src[3]

#define vec4_subtract(dst, src) \
   dst[0] -= src[0]; \
   dst[1] -= src[1]; \
   dst[2] -= src[2]; \
   dst[3] -= src[3]

#define vec4_scale(dst, scale) \
   dst[0] *= scale; \
   dst[1] *= scale; \
   dst[2] *= scale; \
   dst[3] *= scale

#define vec4_copy(dst, src) \
   dst[0] = src[0]; \
   dst[1] = src[1]; \
   dst[2] = src[2]; \
   dst[3] = src[3]

RETRO_END_DECLS

#endif
