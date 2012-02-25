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

static void gen_signal(float *out, double freq, double sample_rate, double bias_samples, size_t samples)
{
   double omega = 2.0 * M_PI * freq / sample_rate;

   for (size_t i = 0; i < samples; i += 2)
   {
      out[i + 0] = cos(((i >> 1) + bias_samples) * omega);
      out[i + 1] = out[i + 0];
   }
}

static double calculate_gain(const float *orig, const float *resamp, size_t samples)
{
   double orig_power = 0.0;
   double resamp_power = 0.0;

   for (size_t i = 0; i < samples; i += 2)
      orig_power += orig[i] * orig[i];

   for (size_t i = 0; i < samples; i += 2)
      resamp_power += resamp[i] * resamp[i];

   return sqrt(resamp_power / orig_power);
}

struct snr_result
{
   double snr;
   double gain;
};

static void calculate_snr(struct snr_result *res, const float *orig, const float *resamp, size_t samples)
{
   double noise = 0.0;
   double signal = 0.0;

   // Account for gain losses at higher frequencies as it's not really noise.
   double filter_gain = calculate_gain(orig, resamp, samples);
   double makeup_gain = 1.0 / filter_gain;

   for (size_t i = 0; i < samples; i += 2)
      signal += orig[i] * orig[i];

   for (size_t i = 0; i < samples; i += 2)
   {
      double diff = makeup_gain * resamp[i] - orig[i];
      noise += diff * diff;
   }

   res->snr = 10 * log10(signal / noise);
   res->gain = 20.0 * log10(filter_gain);
}

int main(int argc, char *argv[])
{
   float *input;
   float *output;
   float *output_expected;

   if (argc != 3)
   {
      fprintf(stderr, "Usage: %s <in-rate> <out-rate> (max ratio: 8.0)\n", argv[0]);
      return 1;
   }

   unsigned in_rate = strtoul(argv[1], NULL, 0);
   unsigned out_rate = strtoul(argv[2], NULL, 0);

   double ratio = (double)out_rate / in_rate;
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
       30,  50,
      100, 150,
      200, 250,
      300, 350,
      400, 450,
      500, 550,
      600, 650,
      700, 800,
      900, 1000,
      1100, 1200,
      1300, 1500,
      1600, 1700,
      1800, 1900,
      2000, 2100,
      2200, 2300,
      2500, 3000,
      3500, 4000,
      4500, 5000,
      5500, 6000,
      6500, 7000,
      7500, 8000,
      9000, 9500,
      10000, 11000,
      12000, 13000,
      14000, 15000,
      16000, 17000,
      18000, 19000,
      20000, 21000,
      22000,
   };

   unsigned samples = in_rate * 2;
   input = calloc(sizeof(float), samples);
   output = calloc(sizeof(float), samples * 8);
   output_expected = calloc(sizeof(float), samples * 8);
   assert(input);
   assert(output);
   assert(output_expected);

   ssnes_resampler_t *re = resampler_new();
   assert(re);

   for (unsigned i = 0; i < sizeof(freq_list) / sizeof(freq_list[0]) && freq_list[i] < 0.5f * in_rate; i++)
   {
      double omega = 2.0 * M_PI * freq_list[i] / in_rate;
      double sample_offset;
      resampler_preinit(re, omega, &sample_offset);

      gen_signal(input, freq_list[i], in_rate, sample_offset, samples);

      struct resampler_data data = {
         .data_in = input,
         .data_out = output,
         .input_frames = in_rate,
         .ratio = ratio,
      };

      resampler_process(re, &data);

      unsigned out_samples = data.output_frames * 2;
      gen_signal(output_expected, freq_list[i], out_rate, 0, out_samples);

      struct snr_result res;
      calculate_snr(&res, output_expected, output, out_samples);

      printf("SNR @ %7.1f Hz: %6.2lf dB, Gain: %6.1f dB\n",
            freq_list[i], res.snr, res.gain);

      //printf("Generated:\n\t");
      //for (unsigned i = 0; i < 10; i++)
      //   printf("%.4f, ", output[i]);
      //printf("\n");
   }

   resampler_free(re);
   free(input);
   free(output);
   free(output_expected);
}

