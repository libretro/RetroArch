/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
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
#include <complex.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

static void gen_signal(float *out, double omega, double bias_samples, size_t samples)
{
   for (size_t i = 0; i < samples; i += 2)
   {
      out[i + 0] = cos(((i >> 1) + bias_samples) * omega);
      out[i + 1] = out[i + 0];
   }
}

struct snr_result
{
   double snr;
   double gain;
};

static unsigned bitrange(unsigned len)
{
   unsigned ret = 0;
   while ((len >>= 1))
      ret++;

   return ret;
}

static unsigned bitswap(unsigned i, unsigned range)
{
   unsigned ret = 0;
   for (unsigned shifts = 0; shifts < range; shifts++)
      ret |= i & (1 << (range - shifts - 1)) ? (1 << shifts) : 0;

   return ret;
}

// When interleaving the butterfly buffer, addressing puts bits in reverse.
// [0, 1, 2, 3, 4, 5, 6, 7] => [0, 4, 2, 6, 1, 5, 3, 7] 
static void interleave(complex double *butterfly_buf, size_t samples)
{
   unsigned range = bitrange(samples);
   for (unsigned i = 0; i < samples; i++)
   {
      unsigned target = bitswap(i, range);
      if (target > i)
      {
         complex double tmp = butterfly_buf[target];
         butterfly_buf[target] = butterfly_buf[i];
         butterfly_buf[i] = tmp;
      }
   }
}

static complex double gen_phase(double index)
{
   return cexp(M_PI * I * index);
}

static void butterfly(complex double *a, complex double *b, complex double mod)
{
   mod *= *b;
   complex double a_ = *a + mod;
   complex double b_ = *a - mod;
   *a = a_;
   *b = b_;
}

static void butterflies(complex double *butterfly_buf, double phase_dir, size_t step_size, size_t samples)
{
   for (unsigned i = 0; i < samples; i += 2 * step_size)
      for (unsigned j = i; j < i + step_size; j++)
         butterfly(&butterfly_buf[j], &butterfly_buf[j + step_size], gen_phase((phase_dir * (j - i)) / step_size));
}

static void calculate_fft(const float *data, complex double *butterfly_buf, size_t samples)
{
   // Enforce POT.
   assert((samples & (samples - 1)) == 0);

   for (unsigned i = 0; i < samples; i++)
      butterfly_buf[i] = data[2 * i];

   // Interleave buffer to work with FFT.
   interleave(butterfly_buf, samples);

   // Fly, lovely butterflies! :D
   for (unsigned step_size = 1; step_size < samples; step_size *= 2)
      butterflies(butterfly_buf, -1.0, step_size, samples);
}

static void calculate_fft_adjust(complex double *butterfly_buf, double gain, bool merge_high, size_t samples)
{
   if (merge_high)
   {
      for (unsigned i = 1; i < samples / 2; i++)
         butterfly_buf[i] += conj(butterfly_buf[samples - i]);
   }

   // Normalize amplitudes.
   for (unsigned i = 0; i < samples; i++)
      butterfly_buf[i] *= gain;
}

static void calculate_ifft(complex double *butterfly_buf, size_t samples, bool normalize)
{
   // Enforce POT.
   assert((samples & (samples - 1)) == 0);

   interleave(butterfly_buf, samples);

   // Fly, lovely butterflies! In opposite direction! :D
   for (unsigned step_size = 1; step_size < samples; step_size *= 2)
      butterflies(butterfly_buf, 1.0, step_size, samples);

   if (normalize)
      calculate_fft_adjust(butterfly_buf, 1.0 / samples, false, samples);
}

static void test_fft(void)
{
   fprintf(stderr, "Sanity checking FFT ...\n");
   float signal[32];
   complex double butterfly_buf[16];
   complex double buf_tmp[16];

   const float cos_freqs[] = {
      1.0, 4.0, 6.0,
   };

   const float sin_freqs[] = {
      -2.0, 5.0, 7.0,
   };

   for (unsigned i = 0; i < 16; i++)
   {
      signal[2 * i] = 0.0;
      for (unsigned j = 0; j < sizeof(cos_freqs) / sizeof(cos_freqs[0]); j++)
         signal[2 * i] += cos(2.0 * M_PI * i * cos_freqs[j] / 16.0);
      for (unsigned j = 0; j < sizeof(sin_freqs) / sizeof(sin_freqs[0]); j++)
         signal[2 * i] += sin(2.0 * M_PI * i * sin_freqs[j] / 16.0);
   }

   calculate_fft(signal, butterfly_buf, 16);
   memcpy(buf_tmp, butterfly_buf, sizeof(buf_tmp));
   calculate_fft_adjust(buf_tmp, 1.0 / 16, true, 16);

   printf("FFT: { ");
   for (unsigned i = 0; i < 7; i++)
      printf("(%4.2lf, %4.2lf), ", creal(buf_tmp[i]), cimag(buf_tmp[i]));
   printf("(%4.2lf, %4.2lf) }\n", creal(buf_tmp[7]), cimag(buf_tmp[7]));

   calculate_ifft(butterfly_buf, 16, true);

   printf("Original:    { ");
   for (unsigned i = 0; i < 15; i++)
      printf("%5.2f, ", signal[2 * i]);
   printf("%5.2f }\n", signal[2 * 15]);

   printf("FFT => IFFT: { ");
   for (unsigned i = 0; i < 15; i++)
      printf("%5.2lf, ", creal(butterfly_buf[i]));
   printf("%5.2lf }\n", creal(butterfly_buf[15]));
}

// This doesn't yet take account for slight phase distortions,
// so reported SNR is lower than reality.
static void calculate_snr(struct snr_result *res,
      unsigned in_rate,
      const float *resamp, complex double *butterfly_buf, size_t samples)
{
   samples >>= 1;
   calculate_fft(resamp, butterfly_buf, samples);
   calculate_fft_adjust(butterfly_buf, 1.0 / samples, true, samples);

   double signal = cabs(butterfly_buf[in_rate] * butterfly_buf[in_rate]);
   butterfly_buf[in_rate] = 0.0;

   double noise = 0.0;
   for (unsigned i = 0; i < samples / 2; i++)
      noise += cabs(butterfly_buf[i] * butterfly_buf[i]);

   res->snr = 10.0 * log10(signal / noise);
   res->gain = 10.0 * log10(signal);
}

int main(int argc, char *argv[])
{
   if (argc != 2)
   {
      fprintf(stderr, "Usage: %s <ratio> (out-rate is fixed for FFT).\n", argv[0]);
      return 1;
   }

   double ratio = strtod(argv[1], NULL);

   const unsigned fft_samples = 1024 * 128;
   unsigned out_rate = fft_samples;
   unsigned in_rate = out_rate / ratio;
   ratio = (double)out_rate / in_rate;

   if (ratio <= 1.0)
   {
      fprintf(stderr, "Ratio too low ...\n");
      return 1;
   }

   static const float freq_list[] = {
      0.001, 0.002, 0.003, 0.004, 0.005, 0.008, 
      0.010, 0.015, 0.020, 0.025, 0.030, 0.035, 0.040, 0.045, 0.050,
      0.10, 0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45,
      0.46, 0.47, 0.48, 0.49, 0.495,
   };

   unsigned samples = in_rate * 2;
   float *input = calloc(sizeof(float), samples);
   float *output = calloc(sizeof(float), (fft_samples + 1) * 2);
   complex double *butterfly_buf = calloc(sizeof(complex double), fft_samples);
   bool warned = false;
   assert(input);
   assert(output);

   rarch_resampler_t *re = resampler_new();
   assert(re);

   test_fft();

   for (unsigned i = 0; i < sizeof(freq_list) / sizeof(freq_list[0]); i++)
   {
      unsigned freq = freq_list[i] * in_rate;
      double omega = 2.0 * M_PI * freq / in_rate;
      double sample_offset;
      resampler_preinit(re, omega, &sample_offset);
      gen_signal(input, omega, sample_offset, samples);

      struct resampler_data data = {
         .data_in = input,
         .data_out = output,
         .input_frames = in_rate,
         .ratio = ratio,
      };

      resampler_process(re, &data);

      unsigned out_samples = data.output_frames * 2;

      if (out_samples != fft_samples * 2 && !warned)
      {
         fprintf(stderr, "Out samples != fft_samples ... %u / %u\n", out_samples, fft_samples * 2);
         warned = true;
      }

      struct snr_result res;
      calculate_snr(&res, freq, output, butterfly_buf, fft_samples * 2);

      printf("SNR @ w = %5.3f : %6.2lf dB, Gain: %6.1lf dB\n",
            freq_list[i], res.snr, res.gain);
   }

   resampler_free(re);
   free(input);
   free(output);
   free(butterfly_buf);
}

