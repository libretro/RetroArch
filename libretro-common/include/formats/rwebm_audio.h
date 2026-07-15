/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwebm_audio.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RWEBM_AUDIO_H__
#define __LIBRETRO_SDK_FORMAT_RWEBM_AUDIO_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Decode the first supported audio track (Opus via ropus, Vorbis via
 * rvorbis) of a WebM file in memory to interleaved signed 16-bit PCM.
 *
 * On success returns 1 and stores a malloc'd sample buffer (caller
 * frees), the per-channel frame count, the sample rate and the channel
 * count. max_ms > 0 caps the decoded duration. Returns 0 when the file
 * has no audio track, the codec is unsupported or not compiled in, or
 * the track is malformed.
 *
 * Opus pre-skip and end trimming (DiscardPadding) are honoured; output
 * is always 48 kHz for Opus and the coded rate for Vorbis. */
int rwebm_audio_decode(const void *buf, size_t len, int64_t max_ms,
      int16_t **pcm, size_t *frames, unsigned *rate, unsigned *channels);

/* Same, but returns a complete in-memory RIFF/WAVE file (16-bit PCM),
 * ready for audio_mixer_load_wav / AUDIO_MIXER_TYPE_WAV. */
int rwebm_audio_decode_wav(const void *buf, size_t len, int64_t max_ms,
      void **wav, size_t *wav_size);

RETRO_END_DECLS

#endif
