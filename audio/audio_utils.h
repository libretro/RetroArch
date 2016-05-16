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
#define audio_convert_s16_to_float audio_convert_s16_to_float_SSE2

/**
 * audio_convert_s16_to_float_SSE2:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * SSE2 implementation callback function.
 **/
void audio_convert_s16_to_float_SSE2(float *out,
      const int16_t *in, size_t samples, float gain);

#elif defined(__ALTIVEC__)
#define audio_convert_s16_to_float audio_convert_s16_to_float_altivec

/**
 * audio_convert_s16_to_float_altivec:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * AltiVec implementation callback function.
 **/
void audio_convert_s16_to_float_altivec(float *out,
      const int16_t *in, size_t samples, float gain);

#elif defined(__ARM_NEON__) && !defined(VITA)
#define audio_convert_s16_to_float audio_convert_s16_to_float_arm

void (*audio_convert_s16_to_float_arm)(float *out,
      const int16_t *in, size_t samples, float gain);

#elif defined(_MIPS_ARCH_ALLEGREX)
#define audio_convert_s16_to_float audio_convert_s16_to_float_ALLEGREX

/**
 * audio_convert_s16_to_float_ALLEGREX:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * MIPS ALLEGREX implementation callback function.
 **/
void audio_convert_s16_to_float_ALLEGREX(float *out,
      const int16_t *in, size_t samples, float gain);
#else
#define audio_convert_s16_to_float audio_convert_s16_to_float_C
#endif

/**
 * audio_convert_s16_to_float_C:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied to the audio volume
 *
 * Converts audio samples from signed integer 16-bit
 * to floating point.
 *
 * C implementation callback function.
 **/
void audio_convert_s16_to_float_C(float *out,
      const int16_t *in, size_t samples, float gain);

/**
 * audio_convert_init_simd:
 *
 * Sets up function pointers for audio conversion
 * functions based on CPU features.
 **/
void audio_convert_init_simd(void);

#ifdef __cplusplus
}
#endif

#endif
