/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017 - Andre Leiradella
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

#ifndef __AUDIO_MIXER__H
#define __AUDIO_MIXER__H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct audio_mixer_sound_t audio_mixer_sound_t;
typedef struct audio_mixer_voice_t audio_mixer_voice_t;

typedef void (*audio_mixer_stop_cb_t)(audio_mixer_voice_t* voice, unsigned reason);

/* Reasons passed to the stop callback. */
#define AUDIO_MIXER_SOUND_FINISHED 0
#define AUDIO_MIXER_SOUND_STOPPED  1
#define AUDIO_MIXER_SOUND_REPEATED 2

void audio_mixer_init(unsigned rate);
void audio_mixer_done(void);

audio_mixer_sound_t* audio_mixer_load_wav(const char* path);
audio_mixer_sound_t* audio_mixer_load_ogg(const char* path);

void audio_mixer_destroy(audio_mixer_sound_t* sound);

audio_mixer_voice_t* audio_mixer_play(audio_mixer_sound_t* sound, bool repeat, float volume, audio_mixer_stop_cb_t stop_cb);
void                 audio_mixer_stop(audio_mixer_voice_t* voice);

void audio_mixer_mix(float* buffer, size_t num_frames);

RETRO_END_DECLS

#endif
