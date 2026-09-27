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

#ifndef __AUDIO_BINAURAL_H
#define __AUDIO_BINAURAL_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include "audio_upmix.h"

RETRO_BEGIN_DECLS

/* A layout of speakers rendered to two ears. For headphones on a
 * stereo device: the stereo mix goes through the upmix to a 5.1 or
 * 7.1 of virtual speakers, and this renders each speaker to both ears
 * as a head would hear it from that direction, so the rear pair is
 * behind the listener rather than inside the head.
 *
 * The model is deliberately small and stated rather than measured: no
 * HRTF set, so no dependency and no per-listener data. For a speaker
 * at azimuth a (0 ahead, positive to the right), the near ear gets
 * the signal as it is and the far ear gets it later - the head's
 * width at the speed of sound, up to about 0.65 ms - quieter, and
 * through a one-pole low-pass, as the head shadows it; a speaker
 * behind the listener is low-passed to both ears a little, which is
 * what the pinna does to sound from behind and is most of the cue
 * that it is behind. The centre and the LFE go to both ears alike.
 * The far-ear delay is a whole number of samples at the rate, so
 * there is no fractional delay and no phase trouble; the rear
 * low-pass is a one-pole. Everything is a few multiplies per speaker
 * per frame.
 *
 * What it does and does not do: sources widen and the rears move
 * behind, the mix's timbre is barely touched, and it is the same for
 * every listener - a measured HRTF individualises and localises
 * better, at the cost of convolution and data. This is the mild,
 * cheap, always-safe version. */

#define AUDIO_BINAURAL_MAX_DELAY 64   /* samples; 0.65 ms at 96 kHz */

typedef struct audio_binaural_speaker
{
   int   slot;             /* the speaker's slot in the input frame */
   float gain_l, gain_r;   /* level to each ear */
   int   delay_l, delay_r; /* samples of delay to each ear */
   float lp_l, lp_r;       /* one-pole coefficient to each ear; 1 = none */
   float lp_state_l, lp_state_r;
} audio_binaural_speaker_t;

typedef struct audio_binaural
{
   uint32_t layout;
   unsigned channels;
   unsigned nspeakers;
   float    gain;          /* the render's overall level */
   audio_binaural_speaker_t speakers[8];
   /* One delay line per input channel, ring of MAX_DELAY. */
   float    delay[8][AUDIO_BINAURAL_MAX_DELAY];
   unsigned at;
} audio_binaural_t;

/* layout is one audio_layout_supported() accepts; stereo is rendered
 * too (fronts at +/-30 degrees, a mild crossfeed). */
bool audio_binaural_init(audio_binaural_t *b, uint32_t layout, unsigned rate);

/* frames of interleaved layout in, frames * 2 floats out. out may
 * not overlap in. */
void audio_binaural_process(audio_binaural_t *b, float *out, const float *in, size_t frames);

RETRO_END_DECLS

#endif
