/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// Resampler that reads raw S16NE/stereo from stdin and outputs to stdout in S16NE/stereo.
// Used for testing and performance benchmarking.

#include "../resampler.h"
#include "../utils.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
   srand(time(NULL));
   int16_t input_i[1024];
   int16_t output_i[1024 * 8];

   float input_f[1024];
   float output_f[1024 * 8];

   double ratio_max_deviation = 0.0;

   if (argc < 3 || argc > 4)
   {
      fprintf(stderr, "Usage: %s <in-rate> <out-rate> [ratio deviation] (max ratio: 8.0)\n", argv[0]);
      return 1;
   }
   else if (argc == 4)
   {
      ratio_max_deviation = fabs(strtod(argv[3], NULL));
      fprintf(stderr, "Ratio deviation: %.4f.\n", ratio_max_deviation);
   }

   double in_rate = strtod(argv[1], NULL);
   double out_rate = strtod(argv[2], NULL);

   double ratio = out_rate / in_rate;
   if (ratio >= 7.99)
   {
      fprintf(stderr, "Ratio is too high.\n");
      return 1;
   }

   const rarch_resampler_t *resampler = NULL;
   void *re = NULL;
   if (!rarch_resampler_realloc(&re, &resampler, NULL, out_rate / in_rate))
   {
      fprintf(stderr, "Failed to allocate resampler ...\n");
      return 1;
   }

   for (;;)
   {
      if (fread(input_i, sizeof(int16_t), 1024, stdin) != 1024)
         break;

      double uniform = (2.0 * rand()) / RAND_MAX - 1.0;
      double rate_mod = 1.0 + ratio_max_deviation * uniform;

      audio_convert_s16_to_float(input_f, input_i, 1024, 1.0f);

      struct resampler_data data = {
         .data_in = input_f,
         .data_out = output_f,
         .input_frames = sizeof(input_f) / (2 * sizeof(float)),
         .ratio = ratio * rate_mod,
      };

      rarch_resampler_process(resampler, re, &data);

      size_t output_samples = data.output_frames * 2;

      audio_convert_float_to_s16(output_i, output_f, output_samples);

      if (fwrite(output_i, sizeof(int16_t), output_samples, stdout) != output_samples)
         break;
   }

   rarch_resampler_freep(&resampler, &re);
}

