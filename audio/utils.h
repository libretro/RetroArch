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

#include <stdint.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(__SSE2__)
#define audio_convert_s16_to_float audio_convert_s16_to_float_SSE2
#define audio_convert_float_to_s16 audio_convert_float_to_s16_SSE2

void audio_convert_s16_to_float_SSE2(float *out,
      const int16_t *in, size_t samples, float gain);

void audio_convert_float_to_s16_SSE2(int16_t *out,
      const float *in, size_t samples);

#elif defined(__ALTIVEC__)
#define audio_convert_s16_to_float audio_convert_s16_to_float_altivec
#define audio_convert_float_to_s16 audio_convert_float_to_s16_altivec

void audio_convert_s16_to_float_altivec(float *out,
      const int16_t *in, size_t samples, float gain);

void audio_convert_float_to_s16_altivec(int16_t *out,
      const float *in, size_t samples);

#elif defined(HAVE_NEON)
#define audio_convert_s16_to_float audio_convert_s16_to_float_arm
#define audio_convert_float_to_s16 audio_convert_float_to_s16_arm

void (*audio_convert_s16_to_float_arm)(float *out,
      const int16_t *in, size_t samples, float gain);
void (*audio_convert_float_to_s16_arm)(int16_t *out,
      const float *in, size_t samples);

#else
#define audio_convert_s16_to_float audio_convert_s16_to_float_C
#define audio_convert_float_to_s16 audio_convert_float_to_s16_C
#endif

void audio_convert_s16_to_float_C(float *out,
      const int16_t *in, size_t samples, float gain);
void audio_convert_float_to_s16_C(int16_t *out,
      const float *in, size_t samples);

void audio_convert_init_simd(void);

#ifdef HAVE_RSOUND
bool rarch_rsound_start(const char *ip);
void rarch_rsound_stop(void);
#endif

#endif

