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
   AUDIO_MIXER_TYPE_MP3,
   AUDIO_MIXER_TYPE_M4A,
   AUDIO_MIXER_TYPE_OPUS,
   AUDIO_MIXER_TYPE_WEBA  /* resolves to OPUS or OGG at load */
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

/* want_s16 selects which PCM format is built at load - the one the
 * mixer's current mode will play.  The other format derives on
 * demand at the first mode-mismatched play (see the derivation notes
 * in the implementation), so a WAV holds one PCM copy, not two. */
audio_mixer_sound_t* audio_mixer_load_wav(void *buffer, int32_t size,
      const char *resampler_ident, enum resampler_quality quality,
      bool want_s16);
audio_mixer_sound_t* audio_mixer_load_ogg(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_mod(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_flac(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_mp3(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_m4a(void *buffer, int32_t size);
audio_mixer_sound_t* audio_mixer_load_opus(void *buffer, int32_t size);
/* WebM audio (.weba): identifies the track's codec and returns a sound
 * of the matching existing type (OPUS or OGG), or NULL when the
 * container carries no supported track. */
audio_mixer_sound_t* audio_mixer_load_weba(void *buffer, int32_t size);

/* Compressed-byte read position of a stream voice's decoder (0 when
 * not a live buffer-mode stream voice).  Thread-safe. */
size_t audio_mixer_voice_buffer_tell(audio_mixer_voice_t *voice);

void audio_mixer_destroy(audio_mixer_sound_t* sound);

/* Mark the sound's compressed source data as borrowed: destroy will
 * hand it back through release(owner) instead of free()ing it.  For
 * callers whose bytes live inside a larger owned object (a file
 * mapping, a data_transfer) this removes the defensive copy.
 * Ownership of 'owner' transfers on this call in every outcome
 * (a NULL sound releases immediately). */
void audio_mixer_sound_set_data_owner(audio_mixer_sound_t *sound,
      void *owner, void (*release)(void *owner));

/* Windowed Ogg-Opus only: supply the stream's last-page granule so
 * the decoder skips its full-file end scan at play.  0 = not supplied.
 * No-op for a NULL sound or any non-Opus stream. */
void audio_mixer_sound_set_end_granule(audio_mixer_sound_t *sound,
      int64_t end_granule);

audio_mixer_voice_t* audio_mixer_play(audio_mixer_sound_t* sound,
      bool repeat, float volume,
      const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb);

void audio_mixer_stop(audio_mixer_voice_t* voice);

float audio_mixer_voice_get_volume(audio_mixer_voice_t *voice);

void audio_mixer_voice_set_volume(audio_mixer_voice_t *voice, float val);

void audio_mixer_mix(float* buffer, size_t num_frames, float volume_override, bool override);

/* s16 (fixed-point) mixer pipeline: parallel to the float API above,
 * no int16<->float round-trip. Voices played via audio_mixer_play_s16
 * are mixed only by audio_mixer_mix_s16, and vice versa. */
void audio_mixer_mix_s16(int16_t* buffer, size_t num_frames, float volume_override, bool override);

bool audio_mixer_has_float_voices(void);

bool audio_mixer_has_s16_voices(void);

audio_mixer_voice_t* audio_mixer_play_s16(audio_mixer_sound_t* sound,
      bool repeat, float volume, enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb);

RETRO_END_DECLS

#endif
