/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_s32.h).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_CONVERSION_FLOAT_TO_S32_H__
#define __LIBRETRO_SDK_CONVERSION_FLOAT_TO_S32_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#include <stdint.h>
#include <stddef.h>

/**
 * Converts floating-point audio samples to signed 32-bit integer
 * samples, full scale, saturating; NaN becomes 0.
 *
 * @param out The buffer for the converted samples,
 * <tt>sizeof(int32_t) * samples</tt> bytes.
 * @param in The samples to convert. Any number of channels.
 * @param samples The length of \c in in samples, \em not bytes or frames.
 * @param valid_bits How many high bits of each sample carry signal:
 * 32 for plain s32, 24 for 24-in-32, whose low byte is cleared.
 **/
void convert_float_to_s32(int32_t *out,
      const float *in, size_t samples, unsigned valid_bits);

/**
 * Initializes any prerequisites for using SIMD implementations of
 * \c convert_float_to_s32. Does nothing where none is required.
 **/
void convert_float_to_s32_init_simd(void);

RETRO_END_DECLS

#endif
