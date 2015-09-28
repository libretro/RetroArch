/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "../audio_resampler_driver.h"
#include "../audio_utils.h"

#ifndef RESAMPLER_IDENT
#define RESAMPLER_IDENT "sinc"
#endif

#undef min
#define min(a, b) (((a) < (b)) ? (a) : (b))

static void gen_signal(float *out, double omega, double bias_samples, size_t samples)
{
   size_t i;

   for (i = 0; i < samples; i += 2)
   {
      out[i + 0] = cos(((i >> 1) + bias_samples) * omega);
      out[i + 1] = out[i + 0];
   }
}

struct snr_result
{
   double snr;
   double gain;

   unsigned alias_freq[3];
   double alias_power[3];
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
   unsigned shifts;
   unsigned ret = 0;

   for (shifts = 0; shifts < range; shifts++)
      ret |= (i & (1 << (range - shifts - 1))) ? (1 << shifts) : 0;

   return ret;
}

/* When interleaving the butterfly buffer, addressing puts bits in reverse.
 * [0, 1, 2, 3, 4, 5, 6, 7] => [0, 4, 2, 6, 1, 5, 3, 7] */
static void interleave(complex double *butterfly_buf, size_t samples)
{
   unsigned i;
   unsigned range = bitrange(samples);

   for (i = 0; i < samples; i++)
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
   complex double a_;
   complex double b_;

   mod *= *b;
   a_   = *a + mod;
   b_   = *a - mod;
   *a   = a_;
   *b   = b_;
}

static void butterflies(complex double *butterfly_buf, double phase_dir, size_t step_size, size_t samples)
{
   unsigned i, j;

   for (i = 0; i < samples; i += 2 * step_size)
      for (j = i; j < i + step_size; j++)
         butterfly(&butterfly_buf[j], &butterfly_buf[j + step_size], gen_phase((phase_dir * (j - i)) / step_size));
}

static void calculate_fft(const float *data, complex double *butterfly_buf, size_t samples)
{
   unsigned i;

   /* Enforce POT. */
   assert((samples & (samples - 1)) == 0);

   for (i = 0; i < samples; i++)
      butterfly_buf[i] = data[2 * i];

   /* Interleave buffer to work with FFT. */
   interleave(butterfly_buf, samples);

   /* Fly, lovely butterflies! :D */
   for (unsigned step_size = 1; step_size < samples; step_size *= 2)
      butterflies(butterfly_buf, -1.0, step_size, samples);
}

static void calculate_fft_adjust(complex double *butterfly_buf, double gain, bool merge_high, size_t samples)
{
   unsigned i;
   if (merge_high)
   {
      for (i = 1; i < samples / 2; i++)
         butterfly_buf[i] *= 2.0;
   }

   /* Normalize amplitudes. */
   for (i = 0; i < samples; i++)
      butterfly_buf[i] *= gain;
}

static void calculate_ifft(complex double *butterfly_buf, size_t samples, bool normalize)
{
   unsigned step_size;

   /* Enforce POT. */
   assert((samples & (samples - 1)) == 0);

   interleave(butterfly_buf, samples);

   /* Fly, lovely butterflies! In opposite direction! :D */
   for (step_size = 1; step_size < samples; step_size *= 2)
      butterflies(butterfly_buf, 1.0, step_size, samples);

   if (normalize)
      calculate_fft_adjust(butterfly_buf, 1.0 / samples, false, samples);
}

static void test_fft(void)
{
   unsigned i, j;
   float signal[32];
   complex double butterfly_buf[16];
   complex double buf_tmp[16];
   const float cos_freqs[] = {
      1.0, 4.0, 6.0,
   };
   const float sin_freqs[] = {
      -2.0, 5.0, 7.0,
   };

   fprintf(stderr, "Sanity checking FFT ...\n");

   for (i = 0; i < 16; i++)
   {
      signal[2 * i] = 0.0;
      for (j = 0; j < sizeof(cos_freqs) / sizeof(cos_freqs[0]); j++)
         signal[2 * i] += cos(2.0 * M_PI * i * cos_freqs[j] / 16.0);
      for ( j = 0; j < sizeof(sin_freqs) / sizeof(sin_freqs[0]); j++)
         signal[2 * i] += sin(2.0 * M_PI * i * sin_freqs[j] / 16.0);
   }

   calculate_fft(signal, butterfly_buf, 16);
   memcpy(buf_tmp, butterfly_buf, sizeof(buf_tmp));
   calculate_fft_adjust(buf_tmp, 1.0 / 16, true, 16);

   fprintf(stderr, "FFT: { ");
   for (i = 0; i < 7; i++)
      fprintf(stderr, "(%4.2lf, %4.2lf), ", creal(buf_tmp[i]), cimag(buf_tmp[i]));
   fprintf(stderr, "(%4.2lf, %4.2lf) }\n", creal(buf_tmp[7]), cimag(buf_tmp[7]));

   calculate_ifft(butterfly_buf, 16, true);

   fprintf(stderr, "Original:    { ");
   for (i = 0; i < 15; i++)
      fprintf(stderr, "%5.2f, ", signal[2 * i]);
   fprintf(stderr, "%5.2f }\n", signal[2 * 15]);

   fprintf(stderr, "FFT => IFFT: { ");
   for ( i = 0; i < 15; i++)
      fprintf(stderr, "%5.2lf, ", creal(butterfly_buf[i]));
   fprintf(stderr, "%5.2lf }\n", creal(butterfly_buf[15]));
}

static void set_alias_power(struct snr_result *res, unsigned freq, double power)
{
   unsigned i;
   for (i = 0; i < 3; i++)
   {
      if (power >= res->alias_power[i])
      {
         memmove(res->alias_freq + i + 1, res->alias_freq + i, (2 - i) * sizeof(res->alias_freq[0]));
         memmove(res->alias_power + i + 1, res->alias_power + i, (2 - i) * sizeof(res->alias_power[0]));
         res->alias_power[i] = power;
         res->alias_freq[i] = freq;
         break;
      }
   }
}

static void calculate_snr(struct snr_result *res,
      unsigned in_rate, unsigned max_rate,
      const float *resamp, complex double *butterfly_buf,
      size_t samples)
{
   unsigned i;
   double signal;
   double noise = 0.0;

   samples >>= 1;
   calculate_fft(resamp, butterfly_buf, samples);
   calculate_fft_adjust(butterfly_buf, 1.0 / samples, true, samples);

   memset(res, 0, sizeof(*res));

   signal = cabs(butterfly_buf[in_rate] * butterfly_buf[in_rate]);
   butterfly_buf[in_rate] = 0.0;

   /* Aliased frequencies above half the original sampling rate are not considered. */
   for (i = 0; i <= max_rate; i++)
   {
      double power = cabs(butterfly_buf[i] * butterfly_buf[i]);
      set_alias_power(res, i, power);
      noise += power;
   }

   res->snr = 10.0 * log10(signal / noise);
   res->gain = 10.0 * log10(signal);

   for (i = 0; i < 3; i++)
      res->alias_power[i] = 10.0 * log10(res->alias_power[i]);
}

int main(int argc, char *argv[])
{
   static const float freq_list[] = {
      0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007, 0.008, 0.009,
      0.010, 0.015, 0.020, 0.025, 0.030, 0.035, 0.040, 0.045, 0.050,
      0.060, 0.070, 0.080, 0.090,
      0.10, 0.15, 0.20, 0.25, 0.30, 0.35,
      0.40, 0.41, 0.42, 0.43, 0.44, 0.45,
      0.46, 0.47, 0.48, 0.49,
      0.495, 0.496, 0.497, 0.498, 0.499,
   };

   unsigned out_rate, in_rate, samples, i;
   double ratio;
   float *input, *output;
   complex double *butterfly_buf;
   const rarch_resampler_t *resampler = NULL;
   const unsigned fft_samples = 1024 * 128;
   void *re = NULL;

   if (argc != 2)
   {
      fprintf(stderr, "Usage: %s <ratio> (out-rate is fixed for FFT).\n", argv[0]);
      return 1;
   }

   ratio         = strtod(argv[1], NULL);
   out_rate      = fft_samples / 2;
   in_rate       = round(out_rate / ratio);
   ratio         = (double)out_rate / in_rate;

   samples       = in_rate * 4;
   input         = calloc(sizeof(float), samples);
   output        = calloc(sizeof(float), (fft_samples + 16) * 2);
   butterfly_buf = calloc(sizeof(complex double), fft_samples / 2);

   assert(input);
   assert(output);

   if (!rarch_resampler_realloc(&re, &resampler, RESAMPLER_IDENT, ratio))
   {
      free(input);
      free(output);
      free(butterfly_buf);
      return 1;
   }

   test_fft();

   for (i = 0; i < sizeof(freq_list) / sizeof(freq_list[0]); i++)
   {
      struct resampler_data data;
      unsigned max_freq;
      struct snr_result res = {0};
      unsigned freq = freq_list[i] * in_rate;
      double omega  = 2.0 * M_PI * freq / in_rate;

      gen_signal(input, omega, 0, samples);

      data.data_in      = input;
      data.data_out     = output;
      data.input_frames = in_rate * 2;
      data.ratio        = ratio;

      rarch_resampler_process(resampler, re, &data);

      /* We generate 2 seconds worth of audio, however, 
       * only the last second is considered so phase has stabilized. */
      max_freq = min(in_rate, out_rate) / 2;
      if (freq > max_freq)
         continue;

      calculate_snr(&res, freq, max_freq, output + fft_samples - 2048, butterfly_buf, fft_samples);

      printf("SNR @ w = %5.3f : %6.2lf dB, Gain: %6.1lf dB\n",
            freq_list[i], res.snr, res.gain);

      printf("\tAliases: #1 (w = %5.3f, %6.2lf dB), #2 (w = %5.3f, %6.2lf dB), #3 (w = %5.3f, %6.2lf dB)\n",
            res.alias_freq[0] / (float)in_rate, res.alias_power[0],
            res.alias_freq[1] / (float)in_rate, res.alias_power[1],
            res.alias_freq[2] / (float)in_rate, res.alias_power[2]);
   }

   rarch_resampler_freep(&resampler, &re);

   free(input);
   free(output);
   free(butterfly_buf);
}

