/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

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

#elif defined(__ARM_NEON__) && !defined(VITA)
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
 * convert_init_simd:
 *
 * Sets up function pointers for conversion
 * functions based on CPU features.
 **/
void convert_init_simd(void);

#ifdef __cplusplus
}
#endif

#endif
