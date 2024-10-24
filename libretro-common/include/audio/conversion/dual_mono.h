/* Copyright  (C) 2010-2023 The RetroArch team
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
#ifndef __LIBRETRO_SDK_CONVERSION_DUAL_MONO__
#define __LIBRETRO_SDK_CONVERSION_DUAL_MONO__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * Duplicates 1-channel (mono) frames into 2-channel (stereo) frames.
 * The resulting array is suitable for use in the resampler,
 * or for any use case that demands stereo input.
 * This version operates on 32-bit floating-point samples.
 *
 * May use SIMD intrinsics on supported platforms,
 * but will work without them.
 *
 * Will do nothing if \c out or \c in are \c NULL.
 *
 * @param[out] out The location in which the converted frames will be stored.
 * Must have enough room for twice the value of \c frames.
 * @param[in] in The location of the frames to convert.
 * @param[in] frames The number of frames to convert.
 */
void convert_to_dual_mono_float(float *out, const float *in, size_t frames);

/**
 * Downmixes 2-channel (stereo) frames into 1-channel (mono) frames.
 * This is intended for dual-mono audio (i.e. where both channels are identical),
 * but it will work if both channels are different.
 *
 * This version operates on 32-bit floating-point samples.
 * It preserves the left channel and ignores the right channel.
 *
 * Will do nothing if \c out or \c in are \c NULL.
 *
 * @param[out] out The location in which the converted frames will be stored.
 * Must have enough room for half the value of <code>frames * sizeof(float)</code>.
 * @param[in] in The location of the frames to convert.
 * @param[in] frames The number of frames to convert.
 */
void convert_to_mono_float_left(float *out, const float *in, size_t frames);

RETRO_END_DECLS

#endif
