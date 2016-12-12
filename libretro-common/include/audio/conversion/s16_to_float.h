/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (s16_to_float.h).
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
#ifndef __LIBRETRO_SDK_CONVERSION_S16_TO_FLOAT_H__
#define __LIBRETRO_SDK_CONVERSION_S16_TO_FLOAT_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#if defined(__SSE2__)
#define convert_s16_to_float convert_s16_to_float_SSE2

/**
 * convert_s16_to_float_SSE2:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied (.e.g. audio volume)
 *
 * Converts from signed integer 16-bit
 * to floating point.
 *
 * SSE2 implementation callback function.
 **/
void convert_s16_to_float_SSE2(float *out,
      const int16_t *in, size_t samples, float gain);

#elif defined(__ALTIVEC__)
#define convert_s16_to_float convert_s16_to_float_altivec

/**
 * convert_s16_to_float_altivec:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied (.e.g. audio volume)
 *
 * Converts from signed integer 16-bit
 * to floating point.
 *
 * AltiVec implementation callback function.
 **/
void convert_s16_to_float_altivec(float *out,
      const int16_t *in, size_t samples, float gain);

#elif defined(__ARM_NEON__)
#define convert_s16_to_float convert_s16_to_float_arm

void (*convert_s16_to_float_arm)(float *out,
      const int16_t *in, size_t samples, float gain);

#elif defined(_MIPS_ARCH_ALLEGREX)
#define convert_s16_to_float convert_s16_to_float_ALLEGREX

/**
 * convert_s16_to_float_ALLEGREX:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied (.e.g. audio volume)
 *
 * Converts from signed integer 16-bit
 * to floating point.
 *
 * MIPS ALLEGREX implementation callback function.
 **/
void convert_s16_to_float_ALLEGREX(float *out,
      const int16_t *in, size_t samples, float gain);
#else
#define convert_s16_to_float convert_s16_to_float_C
#endif

/**
 * convert_s16_to_float_C:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied (.e.g. audio volume)
 *
 * Converts from signed integer 16-bit
 * to floating point.
 *
 * C implementation callback function.
 **/
void convert_s16_to_float_C(float *out,
      const int16_t *in, size_t samples, float gain);

/**
 * convert_s16_to_float_init_simd:
 *
 * Sets up function pointers for conversion
 * functions based on CPU features.
 **/
void convert_s16_to_float_init_simd(void);

RETRO_END_DECLS

#endif
