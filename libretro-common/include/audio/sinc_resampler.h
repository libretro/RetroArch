/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (sinc_resampler.h).
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

#ifndef LIBRETRO_AUDIO_SINC_RESAMPLER_H
#define LIBRETRO_AUDIO_SINC_RESAMPLER_H

#include <audio/audio_resampler.h>

RETRO_BEGIN_DECLS

#define SINC_HQ_CUTOFF        0.962
#define SINC_HQ_SIDELOBES     192
#define SINC_HQ_PHASE_BITS    10
#define SINC_HQ_SUBPHASE_BITS 14
#define SINC_HQ_KAISER_BETA   16.0

/* Experimental opt-in table preset. Use sinc_resampler's process/reset/free.
 * bandwidth_mod is the nominal output/input ratio, not a live DRC ratio.
 * At ratios below 2, or with HQ off, the selected quality is unchanged. */
void *sinc_resampler_init_hq(double bandwidth_mod,
      enum resampler_quality quality, resampler_simd_mask_t mask,
      int hq_oversampling);

RETRO_END_DECLS

#endif
