/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file
 * (sinc_resampler_int16.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

#ifndef __LIBRETRO_SDK_SINC_RESAMPLER_INT16_H
#define __LIBRETRO_SDK_SINC_RESAMPLER_INT16_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-point coefficient format.  Q1.30 in an int32_t:  a coefficient value
 * of 1.0 is stored as (1 << 30).  With int16 samples this needs an int64
 * accumulator (16-bit sample * 30-bit coeff * up to 256 taps ~= 54 bits). */
#define SINC_INT16_COEFF_BITS 30

enum sinc_int16_quality
{
   SINC_INT16_QUALITY_LOWEST = 0,
   SINC_INT16_QUALITY_LOWER,
   SINC_INT16_QUALITY_NORMAL,
   SINC_INT16_QUALITY_HIGHER,
   SINC_INT16_QUALITY_HIGHEST
};

/* Interleaved-stereo int16 in/out counterpart of struct resampler_data.
 * Kept deliberately separate from the float resampler_data so the shared
 * float ABI in audio_resampler.h is left untouched. */
struct resampler_data_int16
{
   const int16_t *data_in;   /* interleaved L/R int16 input  */
   int16_t       *data_out;  /* interleaved L/R int16 output */
   size_t         input_frames;
   size_t         output_frames;
   double         ratio;     /* desired output_frames / input_frames */
};

/* Allocates a resampler.  bandwidth_mod < 1.0 selects downsampling (lowers
 * cutoff and extends the tap count exactly like the float driver). */
void *sinc_resampler_int16_init(double bandwidth_mod,
      enum sinc_int16_quality quality);

/* Deterministic, integer-only.  Bit-identical across compilers/architectures. */
void  sinc_resampler_int16_process(void *re,
      struct resampler_data_int16 *data);

void  sinc_resampler_int16_free(void *re);

#ifdef __cplusplus
}
#endif

#endif
