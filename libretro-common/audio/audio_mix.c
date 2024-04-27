/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mix.c).
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

#include <stdlib.h>
#include <string.h>
#include <memalign.h>

#include <retro_environment.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

#include <retro_miscellaneous.h>
#include <audio/audio_mix.h>
#include <streams/file_stream.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>

void audio_mix_volume_C(float *out, const float *in, float vol, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; i++)
      out[i] += in[i] * vol;
}

#ifdef __SSE2__
void audio_mix_volume_SSE2(float *out, const float *in, float vol, size_t samples)
{
   size_t i, remaining_samples;
   __m128 volume = _mm_set1_ps(vol);

   for (i = 0; i + 16 <= samples; i += 16, out += 16, in += 16)
   {
      unsigned j;
      __m128 input[4];
      __m128 additive[4];

      input[0]    = _mm_loadu_ps(out +  0);
      input[1]    = _mm_loadu_ps(out +  4);
      input[2]    = _mm_loadu_ps(out +  8);
      input[3]    = _mm_loadu_ps(out + 12);

      additive[0] = _mm_mul_ps(volume, _mm_loadu_ps(in +  0));
      additive[1] = _mm_mul_ps(volume, _mm_loadu_ps(in +  4));
      additive[2] = _mm_mul_ps(volume, _mm_loadu_ps(in +  8));
      additive[3] = _mm_mul_ps(volume, _mm_loadu_ps(in + 12));

      for (j = 0; j < 4; j++)
         _mm_storeu_ps(out + 4 * j, _mm_add_ps(input[j], additive[j]));
   }

   remaining_samples = samples - i;

   for (i = 0; i < remaining_samples; i++)
      out[i] += in[i] * vol;
}
#endif

void audio_mix_free_chunk(audio_chunk_t *chunk)
{
   if (!chunk)
      return;

#ifdef HAVE_RWAV
   if (chunk->rwav && chunk->rwav->samples)
   {
      /* rwav_free only frees the samples */
      rwav_free(chunk->rwav);
      free(chunk->rwav);
   }
#endif

   if (chunk->buf)
      free(chunk->buf);

   if (chunk->upsample_buf)
      memalign_free(chunk->upsample_buf);

   if (chunk->float_buf)
      memalign_free(chunk->float_buf);

   if (chunk->float_resample_buf)
      memalign_free(chunk->float_resample_buf);

   if (chunk->resample_buf)
      memalign_free(chunk->resample_buf);

   if (chunk->resampler && chunk->resampler_data)
      chunk->resampler->free(chunk->resampler_data);

   free(chunk);
}

audio_chunk_t* audio_mix_load_wav_file(const char *path, int sample_rate,
      const char *resampler_ident, enum resampler_quality quality)
{
#ifdef HAVE_RWAV
   int sample_size;
   int64_t len                = 0;
   void *buf                  = NULL;
   audio_chunk_t *chunk       = (audio_chunk_t*)malloc(sizeof(*chunk));

   if (!chunk)
      return NULL;

   chunk->buf                 = NULL;
   chunk->upsample_buf        = NULL;
   chunk->float_buf           = NULL;
   chunk->float_resample_buf  = NULL;
   chunk->resample_buf        = NULL;
   chunk->len                 = 0;
   chunk->resample_len        = 0;
   chunk->sample_rate         = sample_rate;
   chunk->resample            = false;
   chunk->resampler           = NULL;
   chunk->resampler_data      = NULL;
   chunk->ratio               = 0.00f;
   chunk->rwav                = (rwav_t*)malloc(sizeof(rwav_t));

   if (!chunk->rwav)
      goto error;

   chunk->rwav->bitspersample = 0;
   chunk->rwav->numchannels   = 0;
   chunk->rwav->samplerate    = 0;
   chunk->rwav->numsamples    = 0;
   chunk->rwav->subchunk2size = 0;
   chunk->rwav->samples       = NULL;

   if (!filestream_read_file(path, &buf, &len))
      goto error;

   chunk->buf                 = buf;
   chunk->len                 = len;

   if (rwav_load(chunk->rwav, chunk->buf, chunk->len) == RWAV_ITERATE_ERROR)
      goto error;

   /* numsamples does not know or care about
    * multiple channels, but we need space for 2 */
   chunk->upsample_buf        = (int16_t*)memalign_alloc(128,
         chunk->rwav->numsamples * 2 * sizeof(int16_t));

   sample_size                = chunk->rwav->bitspersample / 8;

   if (sample_size == 1)
   {
      unsigned i;

      if (chunk->rwav->numchannels == 1)
      {
         for (i = 0; i < chunk->rwav->numsamples; i++)
         {
            uint8_t *sample                  = (
                  (uint8_t*)chunk->rwav->samples) + i;

            chunk->upsample_buf[i * 2]       = 
               (int16_t)((sample[0] - 128) << 8);
            chunk->upsample_buf[(i * 2) + 1] = 
               (int16_t)((sample[0] - 128) << 8);
         }
      }
      else if (chunk->rwav->numchannels == 2)
      {
         for (i = 0; i < chunk->rwav->numsamples; i++)
         {
            uint8_t *sample                  = (
                  (uint8_t*)chunk->rwav->samples) +
               (i * 2);

            chunk->upsample_buf[i * 2]       = 
               (int16_t)((sample[0] - 128) << 8);
            chunk->upsample_buf[(i * 2) + 1] = 
               (int16_t)((sample[1] - 128) << 8);
         }
      }
   }
   else if (sample_size == 2)
   {
      if (chunk->rwav->numchannels == 1)
      {
         unsigned i;

         for (i = 0; i < chunk->rwav->numsamples; i++)
         {
            int16_t sample                   = ((int16_t*)
                  chunk->rwav->samples)[i];

            chunk->upsample_buf[i * 2]       = sample;
            chunk->upsample_buf[(i * 2) + 1] = sample;
         }
      }
      else if (chunk->rwav->numchannels == 2)
         memcpy(chunk->upsample_buf, chunk->rwav->samples,
               chunk->rwav->subchunk2size);
   }
   else if (sample_size != 2)
   {
      /* we don't support any other sample size besides 8 and 16-bit yet */
      goto error;
   }

   if (sample_rate != (int)chunk->rwav->samplerate)
   {
      chunk->resample = true;
      chunk->ratio    = (double)sample_rate / chunk->rwav->samplerate;

      retro_resampler_realloc(&chunk->resampler_data,
            &chunk->resampler,
            resampler_ident,
            quality,
            chunk->ratio);

      if (chunk->resampler && chunk->resampler_data)
      {
         struct resampler_data info;

         chunk->float_buf          = (float*)memalign_alloc(128,
               chunk->rwav->numsamples * 2 * 
               chunk->ratio * sizeof(float));

         /* why is *3 needed instead of just *2? Does the 
          * sinc driver require more space than we know about? */
         chunk->float_resample_buf = (float*)memalign_alloc(128,
               chunk->rwav->numsamples * 3 * 
               chunk->ratio * sizeof(float));

         convert_s16_to_float(chunk->float_buf,
               chunk->upsample_buf, chunk->rwav->numsamples * 2, 1.0);

         info.data_in       = (const float*)chunk->float_buf;
         info.data_out      = chunk->float_resample_buf;
         /* a 'frame' consists of two channels, so we set this
          * to the number of samples irrespective of channel count */
         info.input_frames  = chunk->rwav->numsamples;
         info.output_frames = 0;
         info.ratio         = chunk->ratio;

         chunk->resampler->process(chunk->resampler_data, &info);

         /* number of output_frames does not increase with 
          * multiple channels, but assume we need space for 2 */
         chunk->resample_buf = (int16_t*)memalign_alloc(128,
               info.output_frames * 2 * sizeof(int16_t));
         chunk->resample_len = info.output_frames;
         convert_float_to_s16(chunk->resample_buf,
               chunk->float_resample_buf, info.output_frames * 2);
      }
   }

   return chunk;

error:
   audio_mix_free_chunk(chunk);
#endif
   return NULL;
}

size_t audio_mix_get_chunk_num_samples(audio_chunk_t *chunk)
{
   if (!chunk)
      return 0;

#ifdef HAVE_RWAV
   if (chunk->rwav)
   {
      if (chunk->resample)
         return chunk->resample_len;
      return chunk->rwav->numsamples;
   }
#endif

   /* no other filetypes supported yet */
   return 0;
}

/**
 * audio_mix_get_chunk_sample:
 * @chunk              : audio chunk instance
 * @channel            : channel of the sample (0=left, 1=right)
 * @index              : index of the sample
 *
 * Get a sample from an audio chunk.
 *
 * Returns: A signed 16-bit audio sample.
 **/
int16_t audio_mix_get_chunk_sample(audio_chunk_t *chunk,
      unsigned channel, size_t index)
{
   if (!chunk)
      return 0;

#ifdef HAVE_RWAV
   if (chunk->rwav)
   {
      int sample_size    = chunk->rwav->bitspersample / 8;
      int16_t sample_out = 0;

      /* 0 is the first/left channel */
      uint8_t *sample    = NULL;

      if (chunk->resample)
         sample = (uint8_t*)chunk->resample_buf +
            (sample_size * index * chunk->rwav->numchannels) 
            + (channel * sample_size);
      else
         sample = (uint8_t*)chunk->upsample_buf +
            (sample_size * index * chunk->rwav->numchannels) 
            + (channel * sample_size);

      sample_out = (int16_t)*sample;

      return sample_out;
   }
#endif

   /* no other filetypes supported yet */
   return 0;
}

int16_t* audio_mix_get_chunk_samples(audio_chunk_t *chunk)
{
   if (!chunk)
      return 0;

#ifdef HAVE_RWAV
   if (chunk->rwav)
   {
      int16_t *sample;

      if (chunk->resample)
         sample = chunk->resample_buf;
      else
         sample = chunk->upsample_buf;

      return sample;
   }
#endif

   return NULL;
}

int audio_mix_get_chunk_num_channels(audio_chunk_t *chunk)
{
   if (!chunk)
      return 0;

#ifdef HAVE_RWAV
   if (chunk->rwav)
      return chunk->rwav->numchannels;
#endif

   /* don't support other formats yet */
   return 0;
}
