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
   int16_t input_i[1024];
   int16_t output_i[1024 * 8];

   float input_f[1024];
   float output_f[1024 * 8];

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
      fprintf(stderr, "Ratio is too high.\n");
      return 1;
   }

   rarch_resampler_t *resamp = resampler_new();
   if (!resamp)
   {
      fprintf(stderr, "Failed to allocate resampler ...\n");
      return 1;
   }

   for (;;)
   {
      if (fread(input_i, sizeof(int16_t), 1024, stdin) != 1024)
         break;

      audio_convert_s16_to_float(input_f, input_i, 1024, 1.0f);

      struct resampler_data data = {
         .data_in = input_f,
         .data_out = output_f,
         .input_frames = sizeof(input_f) / (2 * sizeof(float)),
         .ratio = ratio,
      };

      resampler_process(resamp, &data);

      size_t output_samples = data.output_frames * 2;

      audio_convert_float_to_s16(output_i, output_f, output_samples);

      if (fwrite(output_i, sizeof(int16_t), output_samples, stdout) != output_samples)
         break;
   }

   resampler_free(resamp);
}

