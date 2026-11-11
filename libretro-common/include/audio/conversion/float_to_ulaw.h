/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_ulaw.h).
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

#ifndef __LIBRETRO_SDK_CONVERSION_FLOAT_TO_ULAW_H__
#define __LIBRETRO_SDK_CONVERSION_FLOAT_TO_ULAW_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* G.711 mu-law, for a container's "ulaw" track or a telephone line.
 * Each 16-bit sample takes its top 14 bits and the code whose
 * reconstruction is nearest, the decision levels the midpoints between
 * neighbouring codes' levels - the codes FFmpeg's pcm_mulaw writes, and
 * rwav reads back. Full scale negative is 0x00, full scale positive
 * 0x80, silence 0xFF. @samples counts samples, any channels
 * interleaved. */
void convert_s16_to_ulaw(uint8_t *out, const int16_t *in, size_t samples);

/* The same from floats in -1..1, each rounded to 16 bits first (clipped
 * outside the range) */
void convert_float_to_ulaw(uint8_t *out, const float *in, size_t samples);

RETRO_END_DECLS

#endif
