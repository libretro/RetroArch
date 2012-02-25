/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../resampler.h"
#include "../utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

static void gen_signal(float *out, double freq, double sample_rate, double bias_phase, size_t samples)
{
   for (size_t i = 0; i < samples; i += 2)
   {
      out[i + 0] = cos((2.0 * M_PI * freq * ((i >> 1) + bias_phase)) / sample_rate);
      out[i + 1] = out[i + 0];
   }
}

static double calculate_snr(const float *orig, const float *resamp, size_t samples)
{
   double noise = 0.0;
   double signal = 0.0;

   for (size_t i = 0; i < samples; i += 2)
      signal += orig[i] * orig[i];

   for (size_t i = 0; i < samples; i += 2)
   {
      double diff = resamp[i] - orig[i];
      noise += diff * diff;
   }

   double snr = 10 * log10(signal / noise);

   return snr;
}

#define SAMPLES 0x100000

// This approach is kinda stupid.
// There should be a good way to directly (and accurately) determine phase after correlating
// the two signals.
double find_best_snr(const float *output,
      size_t samples,
      double freq,
      double out_rate,
      uint64_t *first_offset, uint64_t *last_offset,
      uint64_t *first_subphase, uint64_t *last_subphase, uint64_t *subphases)
{
   static float output_expected[SAMPLES];

   double max_snr = -100.0;
   uint64_t best_offset = *first_offset;
   uint64_t best_subphase = *first_subphase;

   for (uint64_t offset = *first_offset; offset <= *last_offset; offset += 2)
   {
      for (uint64_t subphase = *first_subphase; subphase <= *last_subphase; subphase++)
      {
         gen_signal(output_expected, freq, out_rate, (double)subphase / *subphases, samples);
         double snr = calculate_snr(output_expected, output + offset, samples);
         if (snr > max_snr)
         {
            max_snr = snr;
            best_offset = offset;
            best_subphase = subphase;
         }
      }
   }

   // Narrow down the search area.
   uint64_t left_offset  = *first_offset;
   uint64_t right_offset = *last_offset;
   if (best_offset > left_offset)
      left_offset = best_offset - 1;
   if (best_offset < right_offset)
      right_offset = best_offset + 1;

   *first_offset = left_offset;
   *last_offset = right_offset;

   *subphases *= 2;
   best_subphase *= 2;

   uint64_t left_subphase  = best_subphase - 2;
   uint64_t right_subphase = best_subphase + 2;
   if (best_subphase < 2)
      left_subphase = 0;

   *first_subphase = left_subphase;
   *last_subphase = right_subphase;

   return max_snr;
}

int main(int argc, char *argv[])
{
   static float input[SAMPLES];
   static float output[SAMPLES * 8];

   if (argc != 3)
   {
      fprintf(stderr, "Usage: %s <in-rate> <out-rate> (max ratio: 8.0)\n", argv[0]);
      return 1;
   }

   double in_rate = strtod(argv[1], NULL);
   double out_rate = strtod(argv[2], NULL);

   double ratio = out_rate / in_rate;
   if (ratio >= 7.99)
   {
      fprintf(stderr, "Ratio is too high ...\n");
      return 1;
   }

   if (ratio < 1.0)
   {
      fprintf(stderr, "Ratio too low ...\n");
      return 1;
   }

   static const float freq_list[] = {
      100, 200, 400, 600, 800, 1000,
      2000, 3000, 5000, 8000, 10000, 12000, 15000, 18000, 20000,
   };

   for (unsigned i = 0; i < sizeof(freq_list) / sizeof(freq_list[0]); i++)
   {
      gen_signal(input, freq_list[i], in_rate, 0.0, SAMPLES);

      struct resampler_data data = {
         .data_in = input,
         .data_out = output,
         .input_frames = SAMPLES / 2,
         .ratio = ratio,
      };

      ssnes_resampler_t *re = resampler_new();
      assert(re);
      resampler_process(re, &data);
      resampler_free(re);

#define MAX_OFFSET 128
      uint64_t first_offset = 0;
      uint64_t last_offset = MAX_OFFSET - 2;
      uint64_t first_subphase = 0;
      uint64_t last_subphase = 1;
      uint64_t subphases = 2;

      double max_snr = -100.0;

      // Iteratively find the correct SNR value.
      for (unsigned j = 0; j < 48; j++)
      {
         double snr = find_best_snr(output, SAMPLES - MAX_OFFSET, freq_list[i], out_rate,
               &first_offset, &last_offset,
               &first_subphase, &last_subphase, &subphases);

         if (snr > max_snr)
            max_snr = snr;
      }

      printf("SNR @ %.0f Hz: %lf dB\n", freq_list[i], max_snr);
   }
}

