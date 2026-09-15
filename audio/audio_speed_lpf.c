/* Copyright (C) 2026 - The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <math.h>
#include <string.h>
#include "audio_speed_lpf.h"

#define SPEED_LPF_ONE UINT32_C(1073741824)
#define SPEED_LPF_WET UINT32_C(65536)

uint32_t audio_speed_lpf_cutoff(unsigned rate, uint32_t speed_q16)
{
   uint32_t cutoff;
   if (rate < 8000 || rate > 192000 || speed_q16 <= 65536)
      return 0;
   cutoff = (uint32_t)(((uint64_t)rate * 9 * 65536)
         / ((uint64_t)speed_q16 * 20));
   return cutoff < 20 ? 20 : cutoff;
}

static int64_t speed_lpf_round(int64_t value, int64_t divisor)
{
   return value < 0 ? -((-value + divisor / 2) / divisor)
                    : (value + divisor / 2) / divisor;
}

bool audio_speed_lpf_init(audio_speed_lpf_t *s, unsigned rate,
      unsigned channels, bool is_float)
{
   if (!s || rate < 8000 || rate > 192000 || !channels
         || channels > AUDIO_SPEED_LPF_CHANNELS) return false;
   memset(s, 0, sizeof(*s));
   s->rate = rate; s->channels = channels; s->is_float = is_float;
   s->ramp_frames = (rate + 10) / 20;
   return true;
}

bool audio_speed_lpf_quiescent(const audio_speed_lpf_t *s)
{
   return !s || (!s->wet && !s->wet_target);
}

void audio_speed_lpf_reset(audio_speed_lpf_t *s)
{
   if (!s) return;
   memset(&s->history, 0, sizeof(s->history));
   s->wet = s->wet_start = s->wet_progress = 0;
   s->alpha = s->alpha_start = s->alpha_target;
   s->alpha_f = s->alpha_start_f = s->alpha_target_f;
   s->alpha_progress = s->ramp_frames;
   s->block_left = 0;
   s->primed = false;
}

bool audio_speed_lpf_set(audio_speed_lpf_t *s, bool enabled, double cutoff)
{
   uint64_t bits;
   uint32_t alpha, wet = enabled ? SPEED_LPF_WET : 0;
   double k, value;
   memcpy(&bits, &cutoff, sizeof(bits));
   if (!s || !s->rate || bits >= UINT64_C(0x7ff0000000000000)
         || cutoff <= 0.0) return false;
   if (cutoff < 20.0) cutoff = 20.0;
   if (cutoff > s->rate * 0.45) cutoff = s->rate * 0.45;
   k = (1.0 - cos(6.2831853071795864769 * cutoff / s->rate))
         * 2.4142135623730950488;
   value = 2.0 * k / (sqrt(k * k + 2.0 * k) + k);
   alpha = (uint32_t)(value * SPEED_LPF_ONE + 0.5);
   if (audio_speed_lpf_quiescent(s))
   {
      s->alpha = s->alpha_start = alpha;
      s->alpha_f = s->alpha_start_f = (float)value;
      s->alpha_progress = s->ramp_frames;
   }
   else if (alpha != s->alpha_target)
   {
      s->alpha_start = s->alpha; s->alpha_start_f = s->alpha_f;
      s->alpha_progress = 0;
      s->block_left = 64;
   }
   s->alpha_target = alpha; s->alpha_target_f = (float)value;
   if (wet != s->wet_target)
   {
      s->wet_start = s->wet;
      s->wet_progress = 0;
      s->wet_target = wet;
   }
   return true;
}

static float speed_lpf_normal(float value)
{
   return value < 1e-20f && value > -1e-20f ? 0.0f : value;
}

bool audio_speed_lpf_process(audio_speed_lpf_t *s, void *samples, size_t frames)
{
   return audio_speed_lpf_process_into(s, samples, samples, frames);
}

bool audio_speed_lpf_process_into(audio_speed_lpf_t *s,
      const void *source, void *samples, size_t frames)
{
   size_t f, sample, bytes;
   if (!s || !s->channels) return false;
   sample = s->is_float ? sizeof(float) : sizeof(int16_t);
   if ((!samples && frames) || (!source && frames)
         || (uintptr_t)source % sample || (uintptr_t)samples % sample
         || frames > SIZE_MAX / (s->channels * sample)) return false;
   bytes = frames * s->channels * sample;
   if (source != samples)
   {
      uintptr_t a = (uintptr_t)source, b = (uintptr_t)samples;
      if ((a > b ? a - b : b - a) < bytes) return false;
   }
   for (f = 0; f < frames; f++)
   {
      unsigned c;
      if (audio_speed_lpf_quiescent(s)) break;
      if (s->alpha_progress < s->ramp_frames)
      {
         s->alpha_progress++;
         if (!--s->block_left || s->alpha_progress == s->ramp_frames)
         {
            s->alpha = (uint32_t)((int64_t)s->alpha_start
                  + ((int64_t)s->alpha_target - s->alpha_start)
                  * s->alpha_progress / s->ramp_frames);
            s->alpha_f = s->alpha_start_f + (s->alpha_target_f - s->alpha_start_f)
                  * ((float)s->alpha_progress / s->ramp_frames);
            if (s->alpha_progress == s->ramp_frames)
               s->alpha_f = s->alpha_target_f;
            s->block_left = 64;
         }
      }
      if (s->wet_progress < s->ramp_frames)
      {
         s->wet_progress++;
         s->wet = (uint32_t)((int64_t)s->wet_start
               + ((int64_t)s->wet_target - s->wet_start)
               * s->wet_progress / s->ramp_frames);
      }
      if (audio_speed_lpf_quiescent(s))
      {
         s->primed = false;
         break;
      }
      if (s->is_float)
      {
         float *p = (float*)samples + f * s->channels;
         const float *input = (const float*)source + f * s->channels;
         float mix = (float)s->wet / SPEED_LPF_WET;
         for (c = 0; c < s->channels; c++)
         {
            float x = speed_lpf_normal(input[c]);
            float a = s->primed ? s->history.f[0][c] : x;
            float b = s->primed ? s->history.f[1][c] : x;
            a = speed_lpf_normal(a + s->alpha_f * (x - a));
            b = speed_lpf_normal(b + s->alpha_f * (a - b));
            s->history.f[0][c] = a; s->history.f[1][c] = b;
            p[c] = s->wet == SPEED_LPF_WET ? b : input[c] + mix * (b - input[c]);
         }
      }
      else
      {
         int16_t *p = (int16_t*)samples + f * s->channels;
         const int16_t *input = (const int16_t*)source + f * s->channels;
         for (c = 0; c < s->channels; c++)
         {
            /* Q16 states stay within the native input extrema. The largest
             * difference is below 2^32; with a Q30 pole below one, every
             * product fits signed 64 bits. Convex updates also bound output,
             * so the native lane needs neither clipping nor float storage. */
            int64_t x = (int64_t)input[c] * SPEED_LPF_WET;
            int64_t a = s->primed ? s->history.i[0][c] : x;
            int64_t b = s->primed ? s->history.i[1][c] : x;
            a += speed_lpf_round((x - a) * s->alpha, SPEED_LPF_ONE);
            b += speed_lpf_round((a - b) * s->alpha, SPEED_LPF_ONE);
            s->history.i[0][c] = a; s->history.i[1][c] = b;
            if (s->wet)
            {
               if (s->wet != SPEED_LPF_WET)
                  b = x + speed_lpf_round((b - x) * s->wet, SPEED_LPF_WET);
               p[c] = (int16_t)speed_lpf_round(b, SPEED_LPF_WET);
            }
         }
      }
      s->primed = true;
   }
   if (source != samples && f < frames)
   {
      size_t offset = f * s->channels * sample;
      memcpy((uint8_t*)samples + offset, (const uint8_t*)source + offset,
            bytes - offset);
   }
   return true;
}
