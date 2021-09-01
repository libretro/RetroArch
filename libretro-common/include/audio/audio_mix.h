/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mix.h).
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

#ifndef __LIBRETRO_SDK_AUDIO_MIX_H__
#define __LIBRETRO_SDK_AUDIO_MIX_H__

#include <retro_common_api.h>

#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <formats/rwav.h>
#include <audio/audio_resampler.h>

RETRO_BEGIN_DECLS

typedef struct
{
   double ratio;
   void *buf;
   int16_t *upsample_buf;
   float *float_buf;
   float *float_resample_buf;
   int16_t *resample_buf;
   const retro_resampler_t *resampler;
   void *resampler_data;
   rwav_t *rwav;
   ssize_t len;
   size_t resample_len;
   int sample_rate;
   bool resample;
} audio_chunk_t;

#if defined(__SSE2__)
#define audio_mix_volume           audio_mix_volume_SSE2

void audio_mix_volume_SSE2(float *out,
      const float *in, float vol, size_t samples);
#else
#define audio_mix_volume           audio_mix_volume_C
#endif

void audio_mix_volume_C(float *dst, const float *src, float vol, size_t samples);

void audio_mix_free_chunk(audio_chunk_t *chunk);

audio_chunk_t* audio_mix_load_wav_file(const char *path, int sample_rate,
      const char *resampler_ident, enum resampler_quality quality);

size_t audio_mix_get_chunk_num_samples(audio_chunk_t *chunk);

/**
 * audio_mix_get_chunk_sample:
 * @chunk              : audio chunk instance
 * @channel            : channel of the sample (0=left, 1=right)
 * @index              : index of the sample
 *
 * Get a sample from an audio chunk.
 *
 * Returns: A signed 16-bit audio sample, (if necessary) resampled into the desired output rate.
 **/
int16_t audio_mix_get_chunk_sample(audio_chunk_t *chunk, unsigned channel, size_t sample);

int16_t* audio_mix_get_chunk_samples(audio_chunk_t *chunk);

int audio_mix_get_chunk_num_channels(audio_chunk_t *chunk);

RETRO_END_DECLS

#endif
