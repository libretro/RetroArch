/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_low_pass.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Provenance: the low-pass filter here is the author's own work, written to
 * pair with the WSOLA time-stretcher (see audio_time_stretch.h), and the
 * author holds its copyright in full. The same code is also proposed on
 * unmerged branches of melonDS and mGBA, where it carries each project's
 * per-file licence header (GPL-3.0 and MPL-2.0 respectively). Neither has
 * been accepted upstream, so this copy may be the first to land anywhere;
 * the MIT grant above is the author's own and does not derive from either
 * of them. */

#ifndef __LIBRETRO_SDK_AUDIO_LOW_PASS_H
#define __LIBRETRO_SDK_AUDIO_LOW_PASS_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Time constant of the cutoff smoother, in seconds. The cutoff slides
 * toward its target over roughly this long; stepping the biquad coefficients
 * directly is audible as a click. */
#define AUDIO_LOW_PASS_TAU          0.05
/* Fraction of wide-open at which the filter stops touching the output. */
#define AUDIO_LOW_PASS_BYPASS       0.995
/* Lowest cutoff the coefficient design will accept. */
#define AUDIO_LOW_PASS_MIN_CUTOFF   20.0
/* Floor of the speed-to-cutoff mapping in audio_low_pass_target_hz(): below
 * this the audio is muffled rather than merely smoothed. */
#define AUDIO_LOW_PASS_SPEED_FLOOR  200.0

struct audio_biquad
{
   double b0;
   double b1;
   double b2;
   double a1;
   double a2;
   double z1[2];
   double z2[2];
};

/**
 * audio_low_pass:
 *
 * Fourth-order Butterworth low-pass: two cascaded RBJ biquads, stereo with
 * independent state per channel. Pairs with the WSOLA time-stretcher (see
 * audio_time_stretch.h) to take the edge off fast-forward audio - the cutoff
 * scales down with emulation speed, smoothed rather than stepped.
 *
 * Not internally synchronised: a caller feeding it from more than one thread
 * must serialise its own calls, same contract as audio_time_stretch.
 **/
struct audio_low_pass
{
   double sample_rate;
   double wide_open;
   double cur_cutoff;
   struct audio_biquad stages[2];
};

typedef struct audio_low_pass audio_low_pass_t;

/**
 * audio_low_pass_init:
 * @sample_rate : rate the filter will run at, in Hz.
 *
 * Sets wide-open (and the initial cutoff) to 0.45 * @sample_rate, floored
 * at AUDIO_LOW_PASS_MIN_CUTOFF, and clears the biquad state.
 **/
void audio_low_pass_init(audio_low_pass_t *lp, double sample_rate);

/**
 * audio_low_pass_set_cutoff_now:
 *
 * Designs both biquad stages for @cutoff_hz (clamped to
 * [AUDIO_LOW_PASS_MIN_CUTOFF, wide-open]) immediately - no smoothing. Used
 * internally by audio_low_pass_process(); exposed so a caller can force an
 * immediate jump.
 **/
void audio_low_pass_set_cutoff_now(audio_low_pass_t *lp, double cutoff_hz);

/**
 * audio_low_pass_reset:
 *
 * Clears the biquad state and snaps the cutoff straight back to wide-open.
 * Call on any discontinuity in the underlying signal, so stale biquad memory
 * does not splice against new samples and re-engaging always smooths down
 * from transparent.
 **/
void audio_low_pass_reset(audio_low_pass_t *lp);

/**
 * audio_low_pass_process:
 * @frames        : interleaved stereo s16, filtered in place.
 * @num_frames    : frame count in @frames.
 * @target_hz     : where the cutoff should end up.
 * @block_seconds : duration of @num_frames at the filter's sample rate; sets
 *                   how far this call slides the smoothed cutoff toward
 *                   @target_hz.
 **/
void audio_low_pass_process(audio_low_pass_t *lp, int16_t *frames,
      int num_frames, double target_hz, double block_seconds);

double audio_low_pass_wide_open(const audio_low_pass_t *lp);
double audio_low_pass_current(const audio_low_pass_t *lp);
bool   audio_low_pass_bypassed(const audio_low_pass_t *lp);

/**
 * audio_low_pass_target_hz:
 * @reference_hz : configured reference cutoff, in Hz; 0 is not special-cased.
 * @speed        : emulation speed multiplier (1.0 is real-time).
 * @wide_open    : audio_low_pass_wide_open() for the filter in use.
 *
 * Divides the reference by speed, so a faster fast-forward gets a lower
 * cutoff; transparent at or below 1x.
 *
 * Returns: target cutoff in Hz, clamped to
 * [AUDIO_LOW_PASS_SPEED_FLOOR, @wide_open].
 **/
double audio_low_pass_target_hz(double reference_hz, double speed,
      double wide_open);

RETRO_END_DECLS

#endif
