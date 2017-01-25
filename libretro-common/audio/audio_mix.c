/* Copyright  (C) 2010-2017 The RetroArch team
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

#include <audio/audio_mix.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

#include <stdio.h>

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
   size_t i;
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

   audio_mix_volume_C(out, in, vol, samples - i);
}
#endif

void audio_mix_free_chunk(audio_chunk_t *chunk)
{
   if (!chunk)
      return;

   if (chunk->rwav && chunk->rwav->samples)
   {
      /* rwav_free only frees the samples */
      rwav_free(chunk->rwav);
      free(chunk->rwav);
   }

   if (chunk->buf)
      free(chunk->buf);

   if (chunk->upsample_buf)
      free(chunk->upsample_buf);

   if (chunk->float_buf)
      free(chunk->float_buf);

   if (chunk->float_resample_buf)
      free(chunk->float_resample_buf);

   if (chunk->resample_buf)
      free(chunk->resample_buf);

   if (chunk->resampler && chunk->resampler_data)
      chunk->resampler->free(chunk->resampler_data);

   free(chunk);
}

audio_chunk_t* audio_mix_load_wav_file(const char *path, int sample_rate)
{
   audio_chunk_t *chunk = (audio_chunk_t*)calloc(1, sizeof(*chunk));

   chunk->sample_rate = sample_rate;

   if (!filestream_read_file(path, &chunk->buf, &chunk->len))
   {
      printf("Could not open WAV file for reading.\n");
      return NULL;
   }

   chunk->rwav = (rwav_t*)malloc(sizeof(rwav_t));

   if (rwav_load(chunk->rwav, chunk->buf, chunk->len) == RWAV_ITERATE_ERROR)
   {
      printf("error: could not load WAV file\n");
      audio_mix_free_chunk(chunk);
      return NULL;
   }

   if (sample_rate != chunk->rwav->samplerate)
   {
      int sample_size = chunk->rwav->bitspersample / 8;

      if (sample_size == 1)
      {
         unsigned i;
         chunk->upsample_buf = (int16_t*)malloc(chunk->rwav->numsamples * chunk->rwav->numchannels * sizeof(int16_t));

        for (i = 0; i < chunk->rwav->numsamples; i++)
        {
           unsigned channel;
           uint8_t *sample = ((uint8_t*)chunk->rwav->samples) +
                 (sample_size * i * chunk->rwav->numchannels);

           for (channel = 0; channel < chunk->rwav->numchannels; channel++)
           {
              chunk->upsample_buf[i] = (int16_t)((sample[channel] - 128) << 8);
           }
        }
      }
      else if (sample_size != 2)
      {
         /* we don't support any other sample size besides 8 and 16-bit yet */
         printf("error: we don't support a sample size of %d\n", sample_size);
         audio_mix_free_chunk(chunk);
         return NULL;
      }

      chunk->resample = true;
      chunk->ratio = (double)sample_rate / chunk->rwav->samplerate;

      retro_resampler_realloc(&chunk->resampler_data,
            &chunk->resampler,
            NULL,
            chunk->ratio);

      if (chunk->resampler && chunk->resampler_data)
      {
         struct resampler_data info = {0};

         chunk->float_buf = (float*)malloc(chunk->rwav->numsamples * chunk->rwav->numchannels * sizeof(float));
         chunk->float_resample_buf = (float*)malloc((chunk->rwav->numsamples * chunk->ratio + 16) * chunk->rwav->numsamples * chunk->rwav->numchannels * sizeof(float));

         if (sample_size == 1)
            convert_s16_to_float(chunk->float_buf, chunk->upsample_buf, chunk->rwav->numsamples * chunk->rwav->numchannels, 1.0);
         else
            convert_s16_to_float(chunk->float_buf, chunk->buf, chunk->rwav->numsamples * chunk->rwav->numchannels, 1.0);

         info.data_in = (const float*)chunk->float_buf;
         info.data_out = chunk->float_resample_buf;
         info.input_frames = chunk->rwav->numsamples;
         info.ratio = chunk->ratio;

         chunk->resampler->process(chunk->resampler_data, &info);

         chunk->resample_buf = (int16_t*)malloc(info.output_frames * chunk->rwav->numchannels * sizeof(int16_t));
         chunk->resample_len = info.output_frames;

         convert_float_to_s16(chunk->resample_buf, chunk->float_resample_buf, info.output_frames * chunk->rwav->numchannels);
      }
   }

   return chunk;
}

size_t audio_mix_get_chunk_num_samples(audio_chunk_t *chunk)
{
   if (!chunk)
      return 0;

   if (chunk->rwav)
   {
      if (chunk->resample)
         return chunk->resample_len;
      else
         return chunk->rwav->numsamples;
   }
   else
   {
      /* no other filetypes supported yet */
      return 0;
   }
}

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
int16_t audio_mix_get_chunk_sample(audio_chunk_t *chunk, unsigned channel, size_t index)
{
   if (!chunk)
      return 0;

   if (chunk->rwav)
   {
      int sample_size = chunk->rwav->bitspersample / 8;
      int16_t sample_out = 0;

      /* 0 is the first/left channel */
      uint8_t *sample = ((uint8_t*)chunk->rwav->samples) +
            (sample_size * index * chunk->rwav->numchannels) + (channel * sample_size);

      if (sample_size == 1)
      {
         if (chunk->resample)
            sample_out = chunk->resample_buf[index];
         else
         {
            /* convert unsigned 8-bit to signed 16-bit */
            sample_out = (*sample - 128) << 8;
         }
      }
      else if (sample_size == 2)
      {
         if (chunk->resample)
            return chunk->resample_buf[index];
         else
         {
            /* signed 16-bit is native, pass it through */
            sample_out = (uint16_t)*sample;
         }
      }
      else
      {
         return 0;
      }

      return sample_out;
   }
   else
   {
      /* no other filetypes supported yet */
      return 0;
   }
}

int audio_mix_get_chunk_num_channels(audio_chunk_t *chunk)
{
   if (!chunk)
      return 0;

   if (chunk->rwav)
      return chunk->rwav->numchannels;
   else
   {
      /* don't support other formats yet */
      return 0;
   }
}
