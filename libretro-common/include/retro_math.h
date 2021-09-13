/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_math.h).
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

#ifndef _LIBRETRO_COMMON_MATH_H
#define _LIBRETRO_COMMON_MATH_H

#include <stdint.h>

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(_WIN32) && defined(_XBOX)
#include <Xtl.h>
#endif

#include <limits.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif
#include <retro_inline.h>

#ifndef M_PI
#if !defined(USE_MATH_DEFINES)
#define M_PI 3.14159265358979323846264338327
#endif
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/**
 * next_pow2:
 * @v         : initial value
 *
 * Get next power of 2 value based on  initial value.
 *
 * Returns: next power of 2 value (derived from @v).
 **/
static INLINE uint32_t next_pow2(uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;
   return v;
}

/**
 * prev_pow2:
 * @v         : initial value
 *
 * Get previous power of 2 value based on initial value.
 *
 * Returns: previous power of 2 value (derived from @v).
 **/
static INLINE uint32_t prev_pow2(uint32_t v)
{
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   return v - (v >> 1);
}

/**
 * clamp:
 * @v         : initial value
 *
 * Get the clamped value based on initial value.
 *
 * Returns: clamped value (derived from @v).
 **/
static INLINE float clamp_value(float v, float min, float max)
{
   return v <= min ? min : v >= max ? max : v;
}

/**
 * saturate_value:
 * @v         : initial value
 *
 * Get the clamped 0.0-1.0 value based on initial value.
 *
 * Returns: clamped 0.0-1.0 value (derived from @v).
 **/
static INLINE float saturate_value(float v)
{
   return clamp_value(v, 0.0f, 1.0f);
}

/**
 * dot_product:
 * @a         : left hand vector value
 * @b         : right hand vector value
 *
 * Get the dot product of the two passed in vectors.
 *
 * Returns: dot product value (derived from @a and @b).
 **/
static INLINE float dot_product(const float* a, const float* b) 
{
   return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

/**
 * convert_rgb_to_yxy:
 * @rgb         : in RGB colour space value
 * @Yxy         : out Yxy colour space value
 *
 * Convert from RGB colour space to Yxy colour space.
 *
 * Returns: Yxy colour space value (derived from @rgb).
 **/
static INLINE void convert_rgb_to_yxy(const float* rgb, float* Yxy) 
{
   float inv;
   float xyz[3];
   float one[3]        = {1.0, 1.0, 1.0};
   float rgb_xyz[3][3] = {
      {0.4124564, 0.3575761, 0.1804375},
      {0.2126729, 0.7151522, 0.0721750},
      {0.0193339, 0.1191920, 0.9503041}
   };

   xyz[0]              = dot_product(rgb_xyz[0], rgb);
   xyz[1]              = dot_product(rgb_xyz[1], rgb);
   xyz[2]              = dot_product(rgb_xyz[2], rgb);

   inv                 = 1.0f / dot_product(xyz, one);
   Yxy[0]              = xyz[1]; 
   Yxy[1]              = xyz[0] * inv;
   Yxy[2]              = xyz[1] * inv;
}
 
/**
 * convert_yxy_to_rgb:
 * @rgb         : in Yxy colour space value
 * @Yxy         : out rgb colour space value
 *
 * Convert from Yxy colour space to rgb colour space.
 *
 * Returns: rgb colour space value (derived from @Yxy).
 **/
static INLINE void convert_yxy_to_rgb(const float* Yxy, float* rgb)
{
   float xyz[3];
   float xyz_rgb[3][3] = {
      {3.2404542, -1.5371385, -0.4985314},
      {-0.9692660, 1.8760108,  0.0415560},
      {0.0556434, -0.2040259, 1.0572252}
   };
   xyz[0]              = Yxy[0] * Yxy[1] / Yxy[2];
   xyz[1]              = Yxy[0];
   xyz[2]              = Yxy[0] * (1.0 - Yxy[1] - Yxy[2]) / Yxy[2];

   rgb[0]              = dot_product(xyz_rgb[0], xyz);
   rgb[1]              = dot_product(xyz_rgb[1], xyz);
   rgb[2]              = dot_product(xyz_rgb[2], xyz);
}

#endif
