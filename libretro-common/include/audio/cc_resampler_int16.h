/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file
 * (cc_resampler_int16.h).
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

#ifndef __LIBRETRO_SDK_CC_RESAMPLER_INT16_H
#define __LIBRETRO_SDK_CC_RESAMPLER_INT16_H

#include <stdint.h>
#include <stddef.h>

/* The interleaved-stereo int16 in/out struct is shared with the sinc int16
 * driver. */
#include <audio/sinc_resampler_int16.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Deterministic, integer-only Convoluted Cosine resampler.  Fixed-point
 * counterpart of the CC_resampler C reference: the cosine-convolution kernel
 * and the phase accumulator are evaluated in Q24, samples are multiplied and
 * accumulated in int64, and the result is round-half-away-from-zero shifted
 * back to int16 with saturation.  Bit-identical across compilers and
 * architectures.
 *
 * As with the float driver, bandwidth_mod selects the down- or up-sampling
 * variant at init (< 0.75 -> downsample). */
void *cc_resampler_int16_init(double bandwidth_mod);

void  cc_resampler_int16_process(void *re,
      struct resampler_data_int16 *data);

void  cc_resampler_int16_free(void *re);

#ifdef __cplusplus
}
#endif

#endif
