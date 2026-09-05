/* Copyright  (C) 2010-2021 The RetroArch team
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
 * Converts an array of floating-point audio samples
 * to signed integer 16-bit audio samples,
 * possibly using SIMD intrinsics.
 *
 * @param out The buffer that will be used to store the converted samples.
 * @param in The buffer containing the samples to convert.
 * Any number of channels is supported.
 * @param samples The length of \c in in samples, \em not bytes or frames.
 * \c out must be as large as <tt>sizeof(int16_t) * samples</tt>.
 * @see convert_s16_to_float
 **/
void convert_float_to_s16(int16_t *out,
      const float *in, size_t samples);

/**
 * Initializes any prerequisites for
 * using SIMD implementations of \c convert_float_to_s16.
 *
 * If SIMD intrinsics are not available or no initialization is required,
 * this function does nothing.
 *
 * @see convert_float_to_s16
 **/
void convert_float_to_s16_init_simd(void);

RETRO_END_DECLS

#endif
