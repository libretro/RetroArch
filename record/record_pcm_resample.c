/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pcm_resample.h"
#include <audio/sinc_resampler_int16.h>
#include <math.h>
#include <stdlib.h>

#define RECORD_PCM_INPUT 256
#define RECORD_PCM_OUTPUT 1552
struct record_pcm_resample
{
   double ratio, carry;
   uint64_t input_frames, output_frames, rate_numerator;
   record_audio_queue_t *source;
   void *filter;
   const int16_t *input;
   size_t frames, offset, count, tail;
   unsigned channels, rate, rate_shift;
   bool source_end, ended, cancelled, failed;
   int16_t mono[RECORD_PCM_INPUT * 2];
   int16_t output[RECORD_PCM_OUTPUT * 2];
};
record_pcm_resample_t *record_pcm_resample_new(record_audio_queue_t *source,
      double source_rate, unsigned output_rate)
{
   record_pcm_resample_t *s;
   unsigned channels = record_audio_queue_channels(source);
   int exponent;
   if (!(source_rate >= 8000.0 && source_rate <= 192000.0)
         || (output_rate != 44100 && output_rate != 48000)
         || source_rate == output_rate || !channels || channels > 2)
      return NULL;
   s = (record_pcm_resample_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->source = source;
   s->rate = output_rate;
   s->channels = channels;
   s->ratio = output_rate / source_rate;
   s->rate_numerator = (uint64_t)ldexp(frexp(source_rate, &exponent), 53);
   s->rate_shift = (unsigned)(53 - exponent);
   s->filter = sinc_resampler_int16_init(s->ratio, SINC_INT16_QUALITY_NORMAL);
   s->tail = sinc_resampler_int16_start_aligned(s->filter);
   if (!s->tail)
   {
      sinc_resampler_int16_free(s->filter);
      free(s);
      return NULL;
   }
   return s;
}
bool record_pcm_resample_duration(const record_pcm_resample_t *s,
      uint64_t frames, uint64_t *output)
{
   uint64_t product, count, remainder;
   unsigned i;
   if (!s || !output || frames > (uint64_t)-1 / s->rate)
      return false;
   /* Rate is numerator / 2^shift. Divide before shifting to keep ceil exact
    * without long-double precision or a platform-specific 128-bit type. */
   product = frames * s->rate;
   count = product / s->rate_numerator;
   remainder = product % s->rate_numerator;
   for (i = 0; i < s->rate_shift; i++)
   {
      if (count > (uint64_t)-1 / 2)
         return false;
      count *= 2;
      remainder *= 2;
      if (remainder >= s->rate_numerator)
      {
         remainder -= s->rate_numerator;
         count++;
      }
   }
   if (count == (uint64_t)-1 && remainder)
      return false;
   *output = count + (remainder != 0);
   return true;
}
int record_pcm_resample_peek(record_pcm_resample_t *s,
      const int16_t **samples, size_t *frames, uint64_t *timestamp)
{
   static const int16_t silence[RECORD_PCM_INPUT * 2] = {0};
   struct resampler_data_int16 data;
   const int16_t *input;
   uint64_t source_at, target;
   size_t count, i;
   int result;
   if (samples)
      *samples = NULL;
   if (frames)
      *frames = 0;
   if (!s || !samples || !frames || !timestamp || s->failed || s->cancelled)
      return RECORD_PCM_ERROR;
   if (s->count)
      goto packet;
   if (s->ended)
      return RECORD_PCM_END;
   if (!s->input && !s->source_end)
   {
      result = record_audio_queue_peek_lend(s->source, &s->input,
            &s->frames, &source_at);
      if (result == RECORD_AUDIO_BUSY)
         return RECORD_PCM_BUSY;
      if (result == RECORD_AUDIO_END)
         s->source_end = true;
      else if (result != RECORD_AUDIO_OK || source_at != s->input_frames)
         goto failed;
      s->offset = 0;
   }
   if (s->source_end)
   {
      if (!record_pcm_resample_duration(s, s->input_frames, &target)
            || s->output_frames > target)
         goto failed;
      if (s->output_frames == target)
      {
         s->ended = true;
         return RECORD_PCM_END;
      }
      if (!s->tail)
         goto failed;
      count = s->tail > RECORD_PCM_INPUT ? RECORD_PCM_INPUT : s->tail;
      input = silence;
      s->tail -= count;
   }
   else
   {
      count = s->frames - s->offset;
      if (count > RECORD_PCM_INPUT)
         count = RECORD_PCM_INPUT;
      if (!count || s->input_frames > (uint64_t)-1 / s->rate - count)
         goto failed;
      input = s->input + s->offset * s->channels;
      if (s->channels == 1)
      {
         for (i = 0; i < count; i++)
            s->mono[i * 2] = s->mono[i * 2 + 1] = input[i];
         input = s->mono;
      }
   }
   data.data_in = input;
   data.data_out = s->output;
   data.input_frames = count;
   data.output_frames = 0;
   data.ratio = s->ratio;
   if (!sinc_resampler_int16_process_aligned(s->filter, &data, &s->carry)
         || data.output_frames > RECORD_PCM_OUTPUT)
      goto failed;
   if (!s->source_end)
   {
      s->offset += count;
      s->input_frames += count;
      if (s->offset == s->frames)
      {
         if (!record_audio_queue_ack(s->source))
            goto failed;
         s->input = NULL;
      }
   }
   s->count = data.output_frames;
   if (s->source_end && s->count > target - s->output_frames)
      s->count = (size_t)(target - s->output_frames);
   if (!s->count)
      return RECORD_PCM_PROGRESS;
   if (s->channels == 1)
      for (i = 0; i < s->count; i++)
         s->output[i] = s->output[i * 2];
packet:
   *samples = s->output;
   *frames = s->count;
   *timestamp = s->output_frames;
   return RECORD_PCM_PACKET;
failed:
   s->failed = true;
   return RECORD_PCM_ERROR;
}
bool record_pcm_resample_ack(record_pcm_resample_t *s)
{
   if (!s || s->failed || s->cancelled || !s->count)
      return false;
   s->output_frames += s->count;
   s->count = 0;
   return true;
}
bool record_pcm_resample_end(const record_pcm_resample_t *s,
      uint64_t *source_frames, uint64_t *output_frames)
{
   if (!s || !s->ended || s->failed || s->cancelled
         || !source_frames || !output_frames)
      return false;
   *source_frames = s->input_frames;
   *output_frames = s->output_frames;
   return true;
}
bool record_pcm_resample_cancel(record_pcm_resample_t *s)
{
   if (!s)
      return false;
   if (s->input && !record_audio_queue_ack(s->source))
      return false;
   s->input = NULL;
   s->count = 0;
   s->cancelled = true;
   return true;
}
bool record_pcm_resample_free(record_pcm_resample_t *s)
{
   if (s && ((!s->ended && !s->cancelled) || s->input || s->count))
      return false;
   if (s)
      sinc_resampler_int16_free(s->filter);
   free(s);
   return true;
}
