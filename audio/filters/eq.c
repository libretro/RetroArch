/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
 *
 */

#include "rarch_dsp.h"
#include <math.h>
#include <stdlib.h>
#include <complex.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "boolean.h"
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

#ifndef EQ_COEFF_SIZE
#define EQ_COEFF_SIZE 256
#endif

#ifndef EQ_FILT_SIZE
#define EQ_FILT_SIZE (EQ_COEFF_SIZE * 2)
#endif

#ifdef RARCH_INTERNAL
#define rarch_dsp_plugin_init eq_dsp_plugin_init
#endif

typedef struct dsp_eq_state dsp_eq_state_t;

static complex float phase_lut[2 * EQ_FILT_SIZE + 1];
static complex float * const phase_lut_ptr = phase_lut + EQ_FILT_SIZE;

static void generate_phase_lut(void)
{
   int i;
   for (i = -EQ_FILT_SIZE; i <= EQ_FILT_SIZE; i++)
   {
      float phase = (float)i / EQ_FILT_SIZE;
      phase_lut_ptr[i] = cexpf(M_PI * I * phase);
   }
}

static inline unsigned bitrange(unsigned len)
{
   unsigned ret;
   ret = 0;
   while ((len >>= 1))
      ret++;

   return ret;
}

static inline unsigned bitswap(unsigned i, unsigned range)
{
   unsigned ret, shifts;
   ret = 0;
   for (shifts = 0; shifts < range; shifts++)
      ret |= i & (1 << (range - shifts - 1)) ? (1 << shifts) : 0;

   return ret;
}

// When interleaving the butterfly buffer, addressing puts bits in reverse.
// [0, 1, 2, 3, 4, 5, 6, 7] => [0, 4, 2, 6, 1, 5, 3, 7] 
static void interleave(complex float *butterfly_buf, size_t samples)
{
   unsigned range, i, target;
   range = bitrange(samples);
   for (i = 0; i < samples; i++)
   {
      target = bitswap(i, range);
      if (target > i)
      {
         complex float tmp = butterfly_buf[target];
         butterfly_buf[target] = butterfly_buf[i];
         butterfly_buf[i] = tmp;
      }
   }
}

static void butterfly(complex float *a, complex float *b, complex float mod)
{
   complex float a_, b_;
   mod *= *b;
   a_ = *a + mod;
   b_ = *a - mod;
   *a = a_;
   *b = b_;
}

static void butterflies(complex float *butterfly_buf, int phase_dir, size_t step_size, size_t samples)
{
   unsigned i, j;
   int phase_step;
   for (i = 0; i < samples; i += 2 * step_size)
   {
      phase_step = EQ_FILT_SIZE * phase_dir / (int)step_size;
      for (j = i; j < i + step_size; j++)
         butterfly(&butterfly_buf[j], &butterfly_buf[j + step_size], phase_lut_ptr[phase_step * (int)(j - i)]);
   }
}

static void calculate_fft_butterfly(complex float *butterfly_buf, size_t samples)
{
   unsigned step_size;
   // Interleave buffer to work with FFT.
   interleave(butterfly_buf, samples);

   // Fly, lovely butterflies! :D
   for (step_size = 1; step_size < samples; step_size *= 2)
      butterflies(butterfly_buf, -1, step_size, samples);
}

static void calculate_fft(const float *data, complex float *butterfly_buf, size_t samples)
{
   unsigned i, step_size;
   for (i = 0; i < samples; i++)
      butterfly_buf[i] = data[i];

   // Interleave buffer to work with FFT.
   interleave(butterfly_buf, samples);

   // Fly, lovely butterflies! :D
   for (step_size = 1; step_size < samples; step_size *= 2)
      butterflies(butterfly_buf, -1, step_size, samples);
}

static void calculate_ifft(complex float *butterfly_buf, size_t samples)
{
   unsigned step_size, i;
   float factor;

   // Interleave buffer to work with FFT.
   interleave(butterfly_buf, samples);

   // Fly, lovely butterflies! In opposite direction! :D
   for (step_size = 1; step_size < samples; step_size *= 2)
      butterflies(butterfly_buf, 1, step_size, samples);

   factor = 1.0 / samples;
   for (i = 0; i < samples; i++)
      butterfly_buf[i] *= factor;
}

struct eq_band
{
   float gain;
   unsigned min_bin;
   unsigned max_bin;
};

struct dsp_eq_state
{
   struct eq_band *bands;
   unsigned num_bands;

   complex float fft_coeffs[EQ_FILT_SIZE];
   float cosine_window[EQ_COEFF_SIZE];

   float last_buf[EQ_COEFF_SIZE];
   float stage_buf[EQ_FILT_SIZE];
   unsigned stage_ptr;
};

static void calculate_band_range(struct eq_band *band, float norm_freq)
{
   unsigned max_bin = (unsigned)round(norm_freq * EQ_COEFF_SIZE);

   band->gain    = 1.0;
   band->max_bin = max_bin;
}

static void recalculate_fft_filt(dsp_eq_state_t *eq)
{
   unsigned i, j, start, end;
   complex float freq_response[EQ_FILT_SIZE] = {0.0f};

   for (i = 0; i < eq->num_bands; i++)
   {
      for (j = eq->bands[i].min_bin; j <= eq->bands[i].max_bin; j++)
         freq_response[j] = eq->bands[i].gain;
   }

   memset(eq->fft_coeffs, 0, sizeof(eq->fft_coeffs));

   for (start = 1, end = EQ_COEFF_SIZE - 1; start < EQ_COEFF_SIZE / 2; start++, end--)
      freq_response[end] = freq_response[start];

   calculate_ifft(freq_response, EQ_COEFF_SIZE);

   // ifftshift(). Needs to be done for some reason ... TODO: Figure out why :D
   memcpy(eq->fft_coeffs + EQ_COEFF_SIZE / 2, freq_response +              0, EQ_COEFF_SIZE / 2 * sizeof(complex float));
   memcpy(eq->fft_coeffs +              0, freq_response + EQ_COEFF_SIZE / 2, EQ_COEFF_SIZE / 2 * sizeof(complex float));

   for (i = 0; i < EQ_COEFF_SIZE; i++)
      eq->fft_coeffs[i] *= eq->cosine_window[i];

   calculate_fft_butterfly(eq->fft_coeffs, EQ_FILT_SIZE);
}

static void dsp_eq_free(dsp_eq_state_t *eq)
{
   if (eq)
   {
      if (eq->bands)
         free(eq->bands);
      free(eq);
   }
}

static dsp_eq_state_t *dsp_eq_new(float input_rate, const float *bands, unsigned num_bands)
{
   unsigned i;
   dsp_eq_state_t *state;

   for (i = 1; i < num_bands; i++)
   {
      if (bands[i] <= bands[i - 1])
         return NULL;
   }

   if (num_bands < 2)
      return NULL;

   state = (dsp_eq_state_t*)calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   state->num_bands = num_bands;

   state->bands = (struct eq_band*)calloc(num_bands, sizeof(struct eq_band));
   if (!state->bands)
      goto error;

   calculate_band_range(&state->bands[0], ((bands[0] + bands[1]) / 2.0) / input_rate);
   state->bands[0].min_bin = 0;

   for (i = 1; i < num_bands - 1; i++)
   {
      calculate_band_range(&state->bands[i], ((bands[i + 1] + bands[i + 0]) / 2.0) / input_rate);
      state->bands[i].min_bin = state->bands[i - 1].max_bin + 1;

      if (state->bands[i].max_bin < state->bands[i].min_bin)
         fprintf(stderr, "[Equalizer]: Band @ %.2f Hz does not have enough spectral resolution to fit.\n", bands[i]);
   }

   state->bands[num_bands - 1].max_bin = EQ_COEFF_SIZE / 2;
   state->bands[num_bands - 1].min_bin = state->bands[num_bands - 2].max_bin + 1;
   state->bands[num_bands - 1].gain    = 1.0f;

   for (i = 0; i < EQ_COEFF_SIZE; i++)
      state->cosine_window[i] = cosf(M_PI * (i + 0.5 - EQ_COEFF_SIZE / 2) / EQ_COEFF_SIZE);

   generate_phase_lut();
   recalculate_fft_filt(state);

   return state;

error:
   dsp_eq_free(state);
   return NULL;
}

#if 0
static void dsp_eq_set_gain(dsp_eq_state_t *eq, unsigned band, float gain)
{
   assert(band < eq->num_bands);

   eq->bands[band].gain = gain;
   recalculate_fft_filt(eq);
}
#endif

static size_t dsp_eq_process(dsp_eq_state_t *eq, float *output, size_t out_samples,
      const float *input, size_t in_samples, unsigned stride)
{
   size_t written = 0;
   while (in_samples)
   {
      unsigned i;
      size_t to_read = EQ_COEFF_SIZE - eq->stage_ptr;

      if (to_read > in_samples)
         to_read = in_samples;

      for (i = 0; i < to_read; i++, input += stride)
         eq->stage_buf[eq->stage_ptr + i] = *input;

      in_samples    -= to_read;
      eq->stage_ptr += to_read;

      if (eq->stage_ptr >= EQ_COEFF_SIZE)
      {
         complex float butterfly_buf[EQ_FILT_SIZE];
         if (out_samples < EQ_COEFF_SIZE)
            return written;

         calculate_fft(eq->stage_buf, butterfly_buf, EQ_FILT_SIZE);
         for (i = 0; i < EQ_FILT_SIZE; i++)
            butterfly_buf[i] *= eq->fft_coeffs[i];

         calculate_ifft(butterfly_buf, EQ_FILT_SIZE);

         for (i = 0; i < EQ_COEFF_SIZE; i++, output += stride, out_samples--, written++)
            *output = crealf(butterfly_buf[i]) + eq->last_buf[i];

         for (i = 0; i < EQ_COEFF_SIZE; i++)
            eq->last_buf[i] = crealf(butterfly_buf[i + EQ_COEFF_SIZE]);

         eq->stage_ptr = 0;
      }
   }

   return written;
}


#if 0
static float db2gain(float val)
{
   return powf(10.0, val / 20.0);
}

static float noise(void)
{
   return 2.0 * ((float)(rand()) / RAND_MAX - 0.5);
}
#endif

struct equalizer_filter_data
{
   dsp_eq_state_t *eq_l;
   dsp_eq_state_t *eq_r;
   float out_buffer[8092];
};

static size_t equalizer_process(void *data, const float *in, unsigned frames)
{
   struct equalizer_filter_data *eq = (struct equalizer_filter_data*)data;

   size_t written = dsp_eq_process(eq->eq_l, eq->out_buffer + 0, 4096, in + 0, frames, 2);
   dsp_eq_process(eq->eq_r, eq->out_buffer + 1, 4096, in + 1, frames, 2);

   return written;
}

static void * eq_dsp_init(const rarch_dsp_info_t *info)
{
   const float bands[] = { 30, 80, 150, 250, 500, 800, 1000, 2000, 3000, 5000, 8000, 10000, 12000, 15000 };
   struct equalizer_filter_data *eq = (struct equalizer_filter_data*)calloc(1, sizeof(*eq));

   if (!eq)
      return NULL;

   eq->eq_l = dsp_eq_new(info->input_rate, bands, sizeof(bands) / sizeof(bands[0]));
   eq->eq_r = dsp_eq_new(info->input_rate, bands, sizeof(bands) / sizeof(bands[0]));

   return eq;
}

static void eq_dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   struct equalizer_filter_data *eq = (struct equalizer_filter_data*)data;

   output->samples = eq->out_buffer;
   size_t out_frames = equalizer_process(eq, input->samples, input->frames);
   output->frames = out_frames;
}

static void eq_dsp_free(void *data)
{
   struct equalizer_filter_data *eq = (struct equalizer_filter_data*)data;

   if (eq)
   {
      dsp_eq_free(eq->eq_l);
      dsp_eq_free(eq->eq_r);
      free(eq);
   }
}

static void eq_dsp_config(void *data)
{
   (void)data;
}

const struct dspfilter_implementation generic_eq_dsp = {
   eq_dsp_init,
   eq_dsp_process,
   eq_dsp_free,
   RARCH_DSP_API_VERSION,
   eq_dsp_config,
   "Equalizer",
   NULL
};

const struct dspfilter_implementation *rarch_dsp_plugin_init(dspfilter_simd_mask_t simd)
{
   (void)simd;
   return &generic_eq_dsp;
}

#ifdef RARCH_INTERNAL
#undef rarch_dsp_plugin_init
#endif
