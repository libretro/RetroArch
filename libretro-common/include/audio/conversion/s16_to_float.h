/* Copyright  (C) 2010-2021 The RetroArch team
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

/**
 * Converts an array of signed integer 16-bit audio samples
 * to floating-point format,
 * possibly using SIMD intrinsics.
 *
 * @param out The buffer that will be used to store the converted samples.
 * @param in The buffer containing the samples to convert.
 * Any number of channels is supported.
 * @param samples The length of \c in in samples, \em not bytes or frames.
 * \c out must be as large as <tt>sizeof(float) * samples</tt>.
 * @param gain The gain (audio volume) to apply to the samples.
 * Pass a value of 1.0 to not apply any gain.
 * @see convert_float_to_s16
 **/
void convert_s16_to_float(float *out,
      const int16_t *in, size_t samples, float gain);

/**
 * Initializes any prerequisites for
 * using SIMD implementations of \c convert_s16_to_float.
 *
 * If SIMD intrinsics are not available or no initialization is required,
 * this function does nothing.
 *
 * @see convert_s16_to_float
 **/
void convert_s16_to_float_init_simd(void);

RETRO_END_DECLS

#endif
