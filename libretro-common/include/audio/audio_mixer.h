/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mixer.h).
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

#ifndef __LIBRETRO_SDK_AUDIO_MIXER__H
#define __LIBRETRO_SDK_AUDIO_MIXER__H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boolean.h>
#include <retro_common_api.h>

#include <audio/audio_resampler.h>

RETRO_BEGIN_DECLS

enum audio_mixer_type
{
   AUDIO_MIXER_TYPE_NONE = 0,
   AUDIO_MIXER_TYPE_WAV,
   AUDIO_MIXER_TYPE_OGG,
   AUDIO_MIXER_TYPE_MOD,
   AUDIO_MIXER_TYPE_FLAC,
   AUDIO_MIXER_TYPE_MP3
};

typedef struct audio_mixer_sound audio_mixer_sound_t;
typedef struct audio_mixer_voice audio_mixer_voice_t;

typedef void (*audio_mixer_stop_cb_t)(audio_mixer_sound_t* sound, unsigned reason);

/* Reasons passed to the stop callback. */
#define AUDIO_MIXER_SOUND_FINISHED 0
#define AUDIO_MIXER_SOUND_STOPPED  1
#define AUDIO_MIXER_SOUND_REPEATED 2

void audio_mixer_init(unsigned rate);

void audio_mixer_done(void);

audio_mixer_sound_t* audio_mixer_load_wav(void *buffer, int32_t size,
      const char *resampler_ident, enum resampler_quality quality);
audio_mixer_sound_t* audio_mixer_load_ogg(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_mod(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_flac(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_mp3(void *buffer, int32_t size);

void audio_mixer_destroy(audio_mixer_sound_t* sound);

audio_mixer_voice_t* audio_mixer_play(audio_mixer_sound_t* sound,
      bool repeat, float volume,
      const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb);

void audio_mixer_stop(audio_mixer_voice_t* voice);

float audio_mixer_voice_get_volume(audio_mixer_voice_t *voice);

void audio_mixer_voice_set_volume(audio_mixer_voice_t *voice, float val);

void audio_mixer_mix(float* buffer, size_t num_frames, float volume_override, bool override);

RETRO_END_DECLS

#endif
