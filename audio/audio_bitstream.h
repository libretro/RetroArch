/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_bitstream.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __AUDIO_BITSTREAM_H
#define __AUDIO_BITSTREAM_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A coded stream on its way to a decoder that is not this one.
 *
 * The audio path carries PCM: a core hands over samples, the filters,
 * the resampler, the mixer and rate control work on them, and what
 * reaches the device is samples. AC-3 to a television works within
 * that - the frontend has the PCM, encodes it in the driver and sends
 * the result. A stream that arrives already coded has no PCM to work
 * on and nothing to encode: a DTS track, a core delivering a
 * bitstream. It has to reach the device as it is or not at all.
 *
 * This is where such a stream plugs in. It is not a driver and not a
 * codec: it takes complete coded frames, wraps each in the IEC 61937
 * burst the format asks for, and hands out the bytes a device that
 * was opened for a bitstream expects - which are shaped exactly like
 * two channels of 16-bit PCM, because that is what a burst is on the
 * wire. So no audio driver needs a new entry point; what a driver
 * must do is open the device in the right subtype and stop treating
 * what it is given as samples.
 *
 * Three things it settles that a caller would otherwise get wrong:
 *
 * - Pacing. A burst stands for a fixed number of PCM frames (1536 for
 *   AC-3, 512, 1024 or 2048 for DTS), and the device consumes it in
 *   exactly that much device time. So a bitstream is paced by the
 *   device like any other stream, and the frames it accounts for are
 *   reported, which is what rate control and the clock need.
 *
 * - Gaps. A receiver locked to a burst stream must not be given
 *   silence when the source is late: the standard has a pause burst
 *   for that, and sending zeroed PCM instead makes a receiver drop
 *   its lock and mute for a second or more. Starvation produces pause
 *   bursts here.
 *
 * - Alignment. A burst is all-or-nothing. A device given half of one
 *   loses the rest of the stream, so reads come out on burst
 *   boundaries and a partial read is refused rather than truncated.
 *
 * What this does not do, and what a caller must arrange: the mixer
 * cannot be mixed into a bitstream, so menu sounds are silent while
 * one is playing; and only a device opened for the subtype in
 * question will take it. */

enum audio_bitstream_kind
{
   AUDIO_BITSTREAM_NONE = 0,
   AUDIO_BITSTREAM_AC3,      /* frames of an AC-3 stream, as they arrived */
   AUDIO_BITSTREAM_EAC3,
   AUDIO_BITSTREAM_DTS
};

typedef struct audio_bitstream audio_bitstream_t;

/* A source for a stream of the kind and sample rate given. The rate
 * is the coded stream's own; it is what the device must be opened at,
 * since a burst occupies the PCM time of the frames it stands for.
 * NULL if the kind is not one that can be carried. */
audio_bitstream_t *audio_bitstream_new(enum audio_bitstream_kind kind,
      unsigned rate);

void audio_bitstream_free(audio_bitstream_t *bs);

/* Hands over one complete coded frame. The frame is parsed, wrapped
 * and queued; false when it is not a frame of the kind this source
 * was made for, when its shape has no burst to go in, or when the
 * queue is full. A caller that gets false because the queue is full
 * should read from the source and try again - that is the source
 * being paced by the device, which is what should happen.
 *
 * Frames may be handed over in whatever packing they arrived in; a
 * DTS stream that is 14-bit or byte-swapped is repacked here into
 * what a receiver takes. */
bool audio_bitstream_submit(audio_bitstream_t *bs,
      const uint8_t *frame, size_t len);

/* Whether another frame would be taken now. */
bool audio_bitstream_writable(const audio_bitstream_t *bs);

/* Device-ready bytes, on burst boundaries. Writes whole bursts only:
 * a request smaller than one burst produces nothing, and a request
 * that is not a whole number of bursts produces as many whole ones as
 * fit. What is queued comes out, as far as the room goes; where
 * nothing at all is queued one pause burst comes out instead, so a
 * receiver keeps its lock through a gap - one, not as many as the
 * room would hold, since the device asks again on its next pass.
 *
 * pcm_frames, where not NULL, receives the PCM frames the bytes
 * account for - the device time they occupy - which is what the
 * caller reports to rate control. A pause burst occupies its own
 * length and no more, so a gap is accounted for honestly rather than
 * as the frames the missing audio would have been. */
size_t audio_bitstream_read(audio_bitstream_t *bs, uint8_t *out, size_t len,
      size_t *pcm_frames);

/* The bytes one burst of this stream takes, which is the unit every
 * read comes in. */
size_t audio_bitstream_burst_bytes(const audio_bitstream_t *bs);

/* The PCM frames one burst stands for. */
unsigned audio_bitstream_burst_frames(const audio_bitstream_t *bs);

/* Frames queued and not yet read, and how many the queue holds. */
unsigned audio_bitstream_queued(const audio_bitstream_t *bs);
unsigned audio_bitstream_capacity(const audio_bitstream_t *bs);

/* Pause bursts sent because nothing was queued, since the source was
 * made. A gap costs a receiver nothing but this counter says a source
 * is not keeping up. */
unsigned audio_bitstream_gaps(const audio_bitstream_t *bs);

RETRO_END_DECLS

#endif
