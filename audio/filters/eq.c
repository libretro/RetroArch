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

#include "dspfilter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../fft/fft.c"

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

struct eq_data
{
   rarch_fft_t *fft;
   float buffer[8 * 1024];

   float *save;
   float *block;
   rarch_fft_complex_t *filter;
   rarch_fft_complex_t *fftblock;
   unsigned block_size;
   unsigned block_ptr;
};

struct eq_gain
{
   float freq;
   float gain; // Linear.
};

static void eq_free(void *data)
{
   struct eq_data *eq = (struct eq_data*)data;
   if (!eq)
      return;

   rarch_fft_free(eq->fft);
   free(eq->save);
   free(eq->block);
   free(eq->fftblock);
   free(eq->filter);
   free(eq);
}

static void eq_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   struct eq_data *eq = (struct eq_data*)data;

   output->samples = eq->buffer;
   output->frames  = 0;

   float *out = eq->buffer;
   const float *in = input->samples;
   unsigned input_frames = input->frames;

   while (input_frames)
   {
      unsigned write_avail = eq->block_size - eq->block_ptr;
      if (input_frames < write_avail)
         write_avail = input_frames;

      memcpy(eq->block + eq->block_ptr * 2, in, write_avail * 2 * sizeof(float));
      in += write_avail * 2;
      input_frames -= write_avail;
      eq->block_ptr += write_avail;

      // Convolve a new block.
      if (eq->block_ptr == eq->block_size)
      {
         unsigned i, c;

         for (c = 0; c < 2; c++)
         {
            rarch_fft_process_forward(eq->fft, eq->fftblock, eq->block + c, 2);
            for (i = 0; i < 2 * eq->block_size; i++)
               eq->fftblock[i] = rarch_fft_complex_mul(eq->fftblock[i], eq->filter[i]);
            rarch_fft_process_inverse(eq->fft, out + c, eq->fftblock, 2);
         }

         // Overlap add method, so add in saved block now.
         for (i = 0; i < 2 * eq->block_size; i++)
            out[i] += eq->save[i];

         // Save block for later.
         memcpy(eq->save, out + 2 * eq->block_size, 2 * eq->block_size * sizeof(float));

         out += eq->block_size * 2;
         output->frames += eq->block_size;
         eq->block_ptr = 0;
      }
   }
}

static int gains_cmp(const void *a_, const void *b_)
{
   const struct eq_gain *a = (const struct eq_gain*)a_;
   const struct eq_gain *b = (const struct eq_gain*)b_;
   if (a->freq < b->freq)
      return -1;
   else if (a->freq > b->freq)
      return 1;
   else
      return 0;
}

static void generate_response(rarch_fft_complex_t *response,
      const struct eq_gain *gains, unsigned num_gains, unsigned samples)
{
   unsigned i;

   // DC and Nyquist get 0 gain. (This will get smeared out good with windowing later though ...)
   response[0].real = 0.0f;
   response[0].imag = 0.0f;
   response[samples].real = 0.0f;
   response[samples].imag = 0.0f;

   float start_freq = 0.0f;
   float start_gain = 1.0f;

   float end_freq = 1.0f;
   float end_gain = 1.0f;

   if (num_gains)
   {
      end_freq = gains->freq;
      end_gain = gains->gain;
      num_gains--;
      gains++;
   }

   // Create a response by linear interpolation between known frequency sample points.
   for (i = 1; i < samples; i++)
   {
      float freq = (float)i / samples;

      while (freq > end_freq)
      {
         if (num_gains)
         {
            start_freq = end_freq;
            start_gain = end_gain;

            end_freq = gains->freq;
            end_gain = gains->gain;
            gains++;
            num_gains--;
         }
         else
         {
            end_freq = 1.0f;
            end_gain = 1.0f;
         }
      }

      float lerp = (freq - start_freq) / (end_freq - start_freq);
      float gain = (1.0f - lerp) * start_gain + lerp * end_gain;

      response[i].real = gain;
      response[i].imag = 0.0f;
      response[2 * samples - i].real = gain;
      response[2 * samples - i].imag = gain;
   }
}

static void create_filter(struct eq_data *eq, unsigned size_log2,
      struct eq_gain *gains, unsigned num_gains)
{
   int i;
   int half_block_size = eq->block_size >> 1;

   rarch_fft_t *fft = rarch_fft_new(size_log2);
   float *time_filter = (float*)calloc(eq->block_size * 2, sizeof(*time_filter));
   if (!fft || !time_filter)
      goto end;

   qsort(gains, num_gains, sizeof(*gains), gains_cmp);

   // Compute desired filter response.
   generate_response(eq->filter, gains, num_gains, half_block_size);

   // Get equivalent time-domain filter.
   rarch_fft_process_inverse(fft, time_filter, eq->filter, 1);

   // ifftshift() to create the correct linear phase filter.
   // The filter response was designed with zero phase, which won't work unless we compensate
   // for the repeating property of the FFT here by flipping left and right blocks.
   for (i = 0; i < half_block_size; i++)
   {
      float tmp = time_filter[i + half_block_size];
      time_filter[i + half_block_size] = time_filter[i];
      time_filter[i] = tmp;
   }

   // Apply a window to smooth out the frequency repsonse.
   for (i = 0; i < (int)eq->block_size; i++)
   {
      // Simple cosine window.
      double phase = (double)i / (eq->block_size - 1);
      phase = 2.0 * (phase - 0.5);
      time_filter[i] *= cos(phase * M_PI);
   }

   // Debugging.
#if 1
   FILE *file = fopen("/tmp/test.txt", "w");
   if (file)
   {
      for (i = 0; i < (int)eq->block_size; i++)
         fprintf(file, "%.6f", time_filter[i]);
      fclose(file);
   }
#endif

   // Padded FFT to create our FFT filter.
   rarch_fft_process_forward(eq->fft, eq->filter, time_filter, 1);

end:
   rarch_fft_free(fft);
   free(time_filter);
}

static void *eq_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   struct eq_data *eq = (struct eq_data*)calloc(1, sizeof(*eq));
   if (!eq)
      return NULL;

   unsigned size_log2 = 8;
   unsigned size = 1 << size_log2;

   eq->block_size = size;

   eq->save     = (float*)calloc(    size, 2 * sizeof(*eq->save));
   eq->block    = (float*)calloc(2 * size, 2 * sizeof(*eq->block));
   eq->fftblock = (rarch_fft_complex_t*)calloc(2 * size, sizeof(*eq->fftblock));
   eq->filter   = (rarch_fft_complex_t*)calloc(2 * size, sizeof(*eq->filter));

   // Use an FFT which is twice the block size with zero-padding
   // to make circular convolution => proper convolution.
   eq->fft = rarch_fft_new(size_log2 + 1);

   if (!eq->fft || !eq->fftblock || !eq->save || !eq->block || !eq->filter)
      goto error;

   struct eq_gain *gains = NULL;
   unsigned num_gains = 0;
   create_filter(eq, size_log2, gains, num_gains);

   return eq;

error:
   eq_free(eq);
   return NULL;
}

static const struct dspfilter_implementation eq_plug = {
   eq_init,
   eq_process,
   eq_free,

   DSPFILTER_API_VERSION,
   "Linear-Phase FFT Equalizer",
   "eq",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation eq_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *eq_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &eq_plug;
}

#undef dspfilter_get_implementation

