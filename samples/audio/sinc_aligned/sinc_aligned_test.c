/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Acceptance requires assertions
#endif
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <audio/sinc_resampler_int16.h>

#define FRAMES 4096
#define CAPACITY 26000
static int16_t input[(FRAMES + 512) * 2];
static int16_t whole[CAPACITY * 2], split[CAPACITY * 2];

static size_t convert(double ratio, int16_t *output, bool chunked)
{
   struct resampler_data_int16 data;
   void *s = sinc_resampler_int16_init(ratio, SINC_INT16_QUALITY_NORMAL);
   size_t lookahead, at = 0, count = 0;
   assert(s);
   lookahead = sinc_resampler_int16_start_aligned(s);
   assert(lookahead && lookahead < 510);
   while (at < FRAMES + lookahead)
   {
      size_t frames = FRAMES + lookahead - at;
      size_t limit = at % 257 + 1;
      if (chunked && frames > limit)
         frames = limit;
      assert(count + (size_t)ceil(frames * ratio) + 8 < CAPACITY);
      data.data_in = input + at * 2;
      data.data_out = output + count * 2;
      data.input_frames = frames;
      data.output_frames = 0;
      data.ratio = ratio;
      sinc_resampler_int16_process(s, &data);
      at += frames;
      count += data.output_frames;
   }
   sinc_resampler_int16_free(s);
   return count;
}

int main(void)
{
   static const double rates[] = {8000, 32040.5, 44100.5, 48000, 96000, 192000};
   unsigned r;
   assert(!sinc_resampler_int16_start_aligned(NULL));
   for (r = 0; r < sizeof(rates) / sizeof(rates[0]); r++)
   {
      double ratio = 48000 / rates[r];
      size_t i, n, m, duration = (size_t)ceil(FRAMES * ratio);
      memset(input, 0, sizeof(input));
      input[0] = 12000;
      input[1] = -12000;
      n = convert(ratio, whole, false);
      m = convert(ratio, split, true);
      assert(n == m && n >= duration && !memcmp(whole, split, n * 4));
      for (i = 1; i < duration; i++)
         assert(abs(whole[i * 2]) <= abs(whole[0]));
      assert(whole[0] > 1000 && whole[1] < -1000);
      memset(input, 0, sizeof(input));
      for (i = 0; i < FRAMES; i++)
      {
         input[i * 2] = (int16_t)(12000 * sin(6.283185307179586 * 997 * i / rates[r]));
         input[i * 2 + 1] = -input[i * 2];
      }
      n = convert(ratio, whole, false);
      m = convert(ratio, split, true);
      assert(n == m && !memcmp(whole, split, n * 4));
      for (i = 256; i + 256 < duration; i++)
      {
         double expected = 12000 * sin(6.283185307179586 * 997 * i / 48000);
         if (fabs(whole[i * 2] - expected) >= 32)
            fprintf(stderr, "rate %.8f frame %lu actual %d expected %.3f\n",
                  rates[r], (unsigned long)i, whole[i * 2], expected);
         assert(fabs(whole[i * 2] - expected) < 32);
         assert(abs(whole[i * 2] + whole[i * 2 + 1]) <= 1);
      }
   }
   puts("PASS aligned sinc: source-zero impulse, fractional tone phase, chunk invariance and tail drain");
   return 0;
}
