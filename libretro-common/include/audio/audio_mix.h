/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mix.h).
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

#ifndef __LIBRETRO_SDK_AUDIO_MIX_H__
#define __LIBRETRO_SDK_AUDIO_MIX_H__

#include <retro_common_api.h>

#include <stdint.h>
#include <stddef.h>

RETRO_BEGIN_DECLS

/* SSE2 wherever the whole build has it (all x86-64, MSVC included,
 * and 32-bit x86 built with it), NEON on ARMv8 and ARM builds with
 * it, the C loop elsewhere. AUDIO_MIX_NO_SIMD leaves them out. */
#if !defined(AUDIO_MIX_NO_SIMD) && (defined(__SSE2__) || defined(_M_X64) \
   || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define AUDIO_MIX_HAVE_SSE2 1
#define audio_mix_volume           audio_mix_volume_SSE2

void audio_mix_volume_SSE2(float *out,
      const float *in, float vol, size_t samples);
#elif !defined(AUDIO_MIX_NO_SIMD) && (defined(__ARM_NEON) \
   || defined(__ARM_NEON__) || defined(_M_ARM64))
#define AUDIO_MIX_HAVE_NEON 1
#define audio_mix_volume           audio_mix_volume_NEON

void audio_mix_volume_NEON(float *out,
      const float *in, float vol, size_t samples);
#else
#define audio_mix_volume           audio_mix_volume_C
#endif

void audio_mix_volume_C(float *dst, const float *src, float vol, size_t samples);

RETRO_END_DECLS

#endif
