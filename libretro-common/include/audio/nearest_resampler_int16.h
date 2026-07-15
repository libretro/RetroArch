/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file
 * (nearest_resampler_int16.h).
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

#ifndef __LIBRETRO_SDK_NEAREST_RESAMPLER_INT16_H
#define __LIBRETRO_SDK_NEAREST_RESAMPLER_INT16_H

#include <stdint.h>
#include <stddef.h>

/* The interleaved-stereo int16 in/out struct is shared with the sinc int16
 * driver. */
#include <audio/sinc_resampler_int16.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Deterministic, integer-only nearest-neighbour (zero-order hold) resampler.
 * Output samples are exact copies of input samples, so only the phase
 * accumulator is fixed point; the result is bit-identical across compilers
 * and architectures.  Counterpart of the float nearest_resampler. */
void *nearest_resampler_int16_init(void);

void  nearest_resampler_int16_process(void *re,
      struct resampler_data_int16 *data);

void  nearest_resampler_int16_free(void *re);

#ifdef __cplusplus
}
#endif

#endif
