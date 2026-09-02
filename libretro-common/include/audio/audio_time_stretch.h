/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_time_stretch.h).
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

/* Provenance: the WSOLA implementation here is the author's own work and
 * the author holds its copyright in full. The same code is also proposed on
 * unmerged branches of melonDS, mGBA and Azahar, where it carries each
 * project's per-file licence header (GPL-3.0, MPL-2.0 and GPL-2.0-or-later
 * respectively). None of those has been accepted upstream, so this copy may
 * be the first to land anywhere; the MIT grant above is the author's own
 * and does not derive from any of them. */

#ifndef __LIBRETRO_SDK_AUDIO_TIME_STRETCH_H
#define __LIBRETRO_SDK_AUDIO_TIME_STRETCH_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Analysis window, in frames. Smaller windows pump less at the synthesis
 * hop rate; below 256 the window holds barely one cycle of a bass note. */
#define AUDIO_STRETCH_FRAME_SIZE      256
/* Periodic Hann at 50% overlap sums to unity. */
#define AUDIO_STRETCH_SYNTHESIS_HOP   (AUDIO_STRETCH_FRAME_SIZE / 2)
#define AUDIO_STRETCH_SEARCH_RADIUS   1024
#define AUDIO_STRETCH_COARSE_STRIDE   4
#define AUDIO_STRETCH_FINE_RADIUS     3
/* Powers of two: the rings are masked, not modulo'd. */
#define AUDIO_STRETCH_INPUT_CAPACITY  32768
#define AUDIO_STRETCH_OUTPUT_CAPACITY 8192
/* Floor for audio_time_stretch_target_input_fill(), so the trim control loop
 * always has enough buffered input to react. */
#define AUDIO_STRETCH_MIN_TARGET_FILL 4096
#define AUDIO_STRETCH_MAX_WRITE       4096

/* How far the ring-fill trim may pull the ratio away from the measured
 * emulation speed: wide enough to steer occupancy, narrow enough that the
 * synthesis cannot over- or under-produce against the device. */
#define AUDIO_STRETCH_RATIO_TRIM_LO   0.95
#define AUDIO_STRETCH_RATIO_TRIM_HI   1.10
/* Floor the trim may widen to while the input ring is empty, so a cold start
 * fills it in a few flushes rather than a few hundred. Reached only at zero
 * fill and eased out in proportion as the reserve builds. */
#define AUDIO_STRETCH_RATIO_PRIME_LO  0.75

/* Most the ratio may move in one flush, as a factor. The measured speed
 * overshoots badly when the frame limiter lifts, and following it drains the
 * ring in a handful of flushes. */
#define AUDIO_STRETCH_RATIO_SLEW      1.08

/* Gain of the ring-fill correction on the stretch ratio. */
#define AUDIO_STRETCH_TRIM_GAIN       0.25
/* Below 1 the stretcher expands (slow-motion), above it compresses. */
#define AUDIO_STRETCH_MIN_RATIO       0.25
#define AUDIO_STRETCH_MAX_RATIO       32.0

/**
 * audio_time_stretch_ratio:
 * @arrival_per_flush   : frames of input arriving per flush, measured.
 * @output_per_flush    : frames of output the device wants per flush.
 * @input_fill          : frames currently buffered in the stretcher.
 * @target_fill         : occupancy the control loop aims for; 0 disables trim.
 *
 * Returns: the ratio at which consumption matches arrival, clamped to
 * [AUDIO_STRETCH_MIN_RATIO, AUDIO_STRETCH_MAX_RATIO], or 1.0 if
 * @output_per_flush is not positive.
 **/
double audio_time_stretch_ratio(double arrival_per_flush, int output_per_flush,
      int input_fill, int target_fill);

/**
 * audio_time_stretch_target_input_fill:
 * @arrival_per_flush   : frames of input arriving per flush, measured.
 *
 * Returns: target occupancy (a fixed figure would starve the stretcher at
 * high speed), at least AUDIO_STRETCH_MIN_TARGET_FILL and at most half
 * the input ring.
 **/
int audio_time_stretch_target_input_fill(double arrival_per_flush);

/**
 * audio_time_stretch:
 *
 * WSOLA time-stretcher: changes playback rate while preserving pitch by
 * overlap-adding windowed frames picked for waveform similarity, so
 * consecutive frames splice on matching phase.
 *
 * Not internally synchronised: a write and a read must never run
 * concurrently. RetroArch serialises via audio_driver_state_lock(), held
 * at every audio_driver_flush() call site.
 **/
struct audio_time_stretch
{
   float    window[AUDIO_STRETCH_FRAME_SIZE];

   int16_t *in_l;
   int16_t *in_r;
   float   *in_mono;

   int64_t  write_pos;
   int64_t  analysis_pos;
   /* Where the previous frame would continue without a search - the
    * similarity search's reference point. */
   int64_t  natural_pos;
   bool     primed;

   float    acc_l[AUDIO_STRETCH_FRAME_SIZE];
   float    acc_r[AUDIO_STRETCH_FRAME_SIZE];

   int16_t *out_l;
   int16_t *out_r;
   int64_t  out_read_pos;
   int64_t  out_write_pos;
};

typedef struct audio_time_stretch audio_time_stretch_t;

/**
 * audio_time_stretch_init:
 *
 * Allocates the rings and builds the analysis window. Returns: true on
 * success; on failure the struct is safe to pass to
 * audio_time_stretch_free().
 **/
bool audio_time_stretch_init(audio_time_stretch_t *ts);

void audio_time_stretch_free(audio_time_stretch_t *ts);

/**
 * audio_time_stretch_reset:
 *
 * Drops all buffered audio and clears the overlap-add accumulator. Call
 * on any discontinuity so synthesis does not splice across the gap.
 **/
void audio_time_stretch_reset(audio_time_stretch_t *ts);

/**
 * audio_time_stretch_write:
 *
 * Appends interleaved stereo frames. Returns: frames accepted, at most
 * AUDIO_STRETCH_MAX_WRITE and short of @num_frames if the ring is full -
 * the excess is the caller's to drop.
 **/
int audio_time_stretch_write(audio_time_stretch_t *ts,
      const int16_t *frames, int num_frames);

/**
 * audio_time_stretch_read:
 * @ratio : how much input each output frame consumes; see
 *          audio_time_stretch_ratio().
 *
 * Emits up to @num_frames of interleaved stereo, synthesising as needed.
 *
 * Returns: frames written, capped at AUDIO_STRETCH_OUTPUT_CAPACITY
 * regardless of @num_frames. A short read means input hasn't caught up.
 **/
int audio_time_stretch_read(audio_time_stretch_t *ts,
      int16_t *frames, int num_frames, double ratio);

/**
 * audio_time_stretch_resync:
 *
 * Resumes from the newest buffered audio, keeping the ring as history for
 * the similarity search. Use where a reset would be wrong because the
 * buffered frames are still wanted as lookback - after a discontinuity in
 * the input, or when another path has been emitting meanwhile.
 **/
void audio_time_stretch_resync(audio_time_stretch_t *ts);

/**
 * audio_time_stretch_idle:
 * @ts      : handle.
 * @reserve : frames of recent input to keep behind the read point.
 *
 * Keeps the ring current while nothing is being synthesised: a caller that
 * stretches only some of the time still writes every block, then calls
 * this to move the read point to @reserve frames behind the write head, so
 * synthesis restarts on audio continuous with what arrives next. The read
 * point only moves forwards, so a shrinking @reserve cannot rewind it into
 * frames already emitted.
 **/
void audio_time_stretch_idle(audio_time_stretch_t *ts, int reserve);

int audio_time_stretch_input_fill(const audio_time_stretch_t *ts);
int audio_time_stretch_output_fill(const audio_time_stretch_t *ts);

RETRO_END_DECLS

#endif
