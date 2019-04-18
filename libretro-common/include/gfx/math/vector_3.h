/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vector_3.h).
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

#ifndef __LIBRETRO_SDK_GFX_MATH_VECTOR_3_H__
#define __LIBRETRO_SDK_GFX_MATH_VECTOR_3_H__

#include <stdint.h>
#include <math.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef float vec3_t[3];

#define vec3_dot(a, b) (a[0] * b[0] + a[1] * b[1] + a[2] * b[2])

#define vec3_cross(dst, a, b)  \
   dst[0] = a[1]*b[2] - a[2]*b[1]; \
   dst[1] = a[2]*b[0] - a[0]*b[2]; \
   dst[2] = a[0]*b[1] - a[1]*b[0]

#define vec3_length(a) sqrtf(vec3_dot(a,a))

#define vec3_add(dst, src) \
   dst[0] += src[0]; \
   dst[1] += src[1]; \
   dst[2] += src[2]

#define vec3_subtract(dst, src) \
   dst[0] -= src[0]; \
   dst[1] -= src[1]; \
   dst[2] -= src[2]

#define vec3_scale(dst, scale) \
   dst[0] *= scale; \
   dst[1] *= scale; \
   dst[2] *= scale

#define vec3_copy(dst, src) \
   dst[0] = src[0]; \
   dst[1] = src[1]; \
   dst[2] = src[2]

#define vec3_normalize(dst) vec3_scale(dst,1.0f / vec3_length(dst))

RETRO_END_DECLS

#endif
