/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AUDIO_UPMIX_H
#define __AUDIO_UPMIX_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Speaker positions.
 *
 * A layout is a mask of positions, one bit each, and a frame carries
 * the present positions interleaved in ascending bit order. The bits
 * are WAVEFORMATEXTENSIBLE's, whose order rule this is, and the
 * orders it yields for the common masks are SDL's fixed orders for
 * the same counts and PipeWire's and PulseAudio's default maps. A
 * count is derived from a mask, never the other way: six channels
 * with the rear pair at the back and six with it at the sides are
 * two layouts, and a core or a device that means one must not be
 * read as the other. */
#define AUDIO_SPEAKER_FRONT_LEFT            0x001u
#define AUDIO_SPEAKER_FRONT_RIGHT           0x002u
#define AUDIO_SPEAKER_FRONT_CENTER          0x004u
#define AUDIO_SPEAKER_LOW_FREQUENCY         0x008u
#define AUDIO_SPEAKER_BACK_LEFT             0x010u
#define AUDIO_SPEAKER_BACK_RIGHT            0x020u
#define AUDIO_SPEAKER_FRONT_LEFT_OF_CENTER  0x040u
#define AUDIO_SPEAKER_FRONT_RIGHT_OF_CENTER 0x080u
#define AUDIO_SPEAKER_BACK_CENTER           0x100u
#define AUDIO_SPEAKER_SIDE_LEFT             0x200u
#define AUDIO_SPEAKER_SIDE_RIGHT            0x400u

#define AUDIO_LAYOUT_STEREO \
   (AUDIO_SPEAKER_FRONT_LEFT | AUDIO_SPEAKER_FRONT_RIGHT)
/* FL FR BL BR */
#define AUDIO_LAYOUT_QUAD \
   (AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_BACK_LEFT | AUDIO_SPEAKER_BACK_RIGHT)
/* FL FR FC LFE BL BR: the rear pair at the back */
#define AUDIO_LAYOUT_5POINT1 \
   (AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_FRONT_CENTER | AUDIO_SPEAKER_LOW_FREQUENCY \
    | AUDIO_SPEAKER_BACK_LEFT | AUDIO_SPEAKER_BACK_RIGHT)
/* FL FR FC LFE SL SR: the rear pair at the sides */
#define AUDIO_LAYOUT_5POINT1_SURROUND \
   (AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_FRONT_CENTER | AUDIO_SPEAKER_LOW_FREQUENCY \
    | AUDIO_SPEAKER_SIDE_LEFT | AUDIO_SPEAKER_SIDE_RIGHT)
/* FL FR FC LFE BL BR SL SR */
#define AUDIO_LAYOUT_7POINT1 \
   (AUDIO_LAYOUT_5POINT1 | AUDIO_SPEAKER_SIDE_LEFT | AUDIO_SPEAKER_SIDE_RIGHT)

/* Channels in a layout: its set bits. */
unsigned audio_layout_channels(uint32_t layout);

/* The layouts this stage can fill: stereo, and the four above. A
 * mask the stage cannot fill is not a layout the frontend opens. */
bool audio_layout_supported(uint32_t layout);

/* A frame of any layout of the eleven positions above, folded to
 * stereo: the ITU-R BS.775 fold the mixer uses - centre and the
 * surround pairs into both sides at -3 dB, a back centre at -6 dB
 * into each, the fronts of centre at -3 dB to their side, LFE
 * dropped. Not normalised, as the mixer's is not: a 5.1 source folds
 * to the level a stereo one would have had, and correlated content
 * may clip, which the s16 form saturates. For a core delivering a
 * wider layout than the pipeline carries (the multi-channel batch
 * entry, RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_MULTI). 'channels'
 * is the layout's count, which the caller has checked. */
void audio_downmix_f32(float *out, const float *in, size_t frames, uint32_t layout, unsigned channels);
void audio_downmix_s16(int16_t *out, const int16_t *in, size_t frames, uint32_t layout, unsigned channels);

/* A frame of one layout to a frame of another: positions both have
 * copied to their slots, positions only the destination has left
 * zero (the fronts included: a mono source goes to both), and
 * positions only the source has folded into the destination's fronts
 * at the BS.775 gains, LFE dropped. The recorder's frame from any
 * batch. Equal layouts are a copy. */
void audio_layout_remap_s16(int16_t *out, uint32_t out_layout,
      const int16_t *in, uint32_t in_layout, size_t frames);

/* Every set bit names a position above. */
bool audio_layout_known(uint32_t layout);

/* Stereo to a wider layout, at the last step before the device. The
 * pipeline stays stereo - the core, the filters, the resampler, the
 * mixer - and a device opened with a wider layout gets this stage
 * between the stereo mix and its write.
 *
 * Positions are filled by role: the front pair carries the stereo as
 * it is; the centre is the sum at -6 dB, so a mono voice in the
 * middle of the mix arrives in the middle of the room without
 * doubling in level; the back pair and the side pair each carry the
 * stereo at -3 dB, whichever of them the layout has; the LFE is the
 * sum through a one-pole low-pass at about 120 Hz, so it moves with
 * the bass and nothing else. Whether a 5.1's rear pair is at the
 * back or the sides changes nothing here - the same signal goes to
 * whichever pair the device has - but the layout says which, so
 * what leaves this stage is never read as the other. This is the
 * plain matrix, not a decoder of a surround-encoded mix; it makes
 * every stereo core fill the room and is deliberately mild. */
typedef struct audio_upmix
{
   uint32_t layout;     /* the mask; AUDIO_LAYOUT_STEREO passes through */
   unsigned channels;   /* audio_layout_channels(layout) */
   /* Slot of each role in the output frame, or -1 when absent. */
   int      fl, fr, fc, lfe, bl, br, sl, sr;
   float    lfe_coeff;  /* the low-pass's coefficient for the rate */
   float    lfe_state;  /* its one sample of memory */
   int32_t  lfe_coeff_q30;  /* the same, for the int16 form: in Q30, because
                             * the coefficient is a hundredth or so and its
                             * rounding is what the filter's response is */
   int64_t  lfe_state_q30;  /* its sample of memory: an int16 in Q30, so the
                             * slow filter's small steps are not lost */
} audio_upmix_t;

/* Refuses a layout audio_layout_supported() does not, leaving stereo. */
bool audio_upmix_init(audio_upmix_t *up, uint32_t layout, unsigned rate);

/* frames of interleaved stereo in, frames * channels floats out.
 * out must not overlap in. With stereo this is a copy. */
void audio_upmix_process(audio_upmix_t *up, float *out, const float *in, size_t frames);

/* The same in int16, for an int16 pipeline into an int16 device: the
 * gains in Q15, the LFE low-pass in Q15 on a 32-bit accumulator, so
 * the whole path stays integer and never rounds through float. The
 * two forms keep their own filter memory; a pipeline uses one. */
void audio_upmix_process_s16(audio_upmix_t *up, int16_t *out, const int16_t *in, size_t frames);

RETRO_END_DECLS

#endif
