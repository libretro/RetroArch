/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_s16.h).
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

#ifndef __LIBRETRO_SDK_CONVERSION_FLOAT_TO_S16_H__
#define __LIBRETRO_SDK_CONVERSION_FLOAT_TO_S16_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#include <stdint.h>
#include <stddef.h>

/**
 * convert_float_to_s16_C:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts floating point 
 * to signed integer 16-bit.
 *
 * C implementation callback function.
 **/
void convert_float_to_s16_C(int16_t *out,
      const float *in, size_t samples);

#if defined(__SSE2__)
#define convert_float_to_s16 convert_float_to_s16_SSE2
/**
 * convert_float_to_s16_SSE2:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts floating point 
 * to signed integer 16-bit.
 *
 * SSE2 implementation callback function.
 **/
void convert_float_to_s16_SSE2(int16_t *out,
      const float *in, size_t samples);
#elif defined(__ALTIVEC__)
#define convert_float_to_s16 convert_float_to_s16_altivec
/**
 * convert_float_to_s16_altivec:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts floating point 
 * to signed integer 16-bit.
 *
 * AltiVec implementation callback function.
 **/
void convert_float_to_s16_altivec(int16_t *out,
      const float *in, size_t samples);
#elif defined(__ARM_NEON__) && !defined(VITA)
#define convert_float_to_s16 convert_float_to_s16_arm

void (*convert_float_to_s16_arm)(int16_t *out,
      const float *in, size_t samples);

void convert_float_s16_asm(int16_t *out, const float *in, size_t samples);
#elif defined(_MIPS_ARCH_ALLEGREX)
#define convert_float_to_s16 convert_float_to_s16_ALLEGREX
/**
 * convert_float_to_s16_ALLEGREX:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 *
 * Converts floating point 
 * to signed integer 16-bit.
 *
 * MIPS ALLEGREX implementation callback function.
 **/
void convert_float_to_s16_ALLEGREX(int16_t *out,
      const float *in, size_t samples);
#else
#define convert_float_to_s16 convert_float_to_s16_C
#endif

/**
 * convert_float_to_s16_init_simd:
 *
 * Sets up function pointers for conversion
 * functions based on CPU features.
 **/
void convert_float_to_s16_init_simd(void);

RETRO_END_DECLS

#endif
