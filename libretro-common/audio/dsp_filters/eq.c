/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (eq.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <filters.h>
#include <libretro_dspfilter.h>

#include "fft/fft.c"

struct eq_data
{
   fft_t *fft;
   float buffer[8 * 1024];

   float *save;
   float *block;
   fft_complex_t *filter;
   fft_complex_t *fftblock;
   unsigned block_size;
   unsigned block_ptr;
};

struct eq_gain
{
   float freq;
   float gain; /* Linear. */
};

static void eq_free(void *data)
{
   struct eq_data *eq = (struct eq_data*)data;
   if (!eq)
      return;

   fft_free(eq->fft);
   free(eq->save);
   free(eq->block);
   free(eq->fftblock);
   free(eq->filter);
   free(eq);
}

static void eq_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   float *out;
   const float *in;
   unsigned input_frames;
   struct eq_data *eq = (struct eq_data*)data;

   output->samples    = eq->buffer;
   output->frames     = 0;

   out                = eq->buffer;
   in                 = input->samples;
   input_frames       = input->frames;

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
            fft_process_forward(eq->fft, eq->fftblock, eq->block + c, 2);
            for (i = 0; i < 2 * eq->block_size; i++)
               eq->fftblock[i] = fft_complex_mul(eq->fftblock[i], eq->filter[i]);
            fft_process_inverse(eq->fft, out + c, eq->fftblock, 2);
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
   if (a->freq > b->freq)
      return 1;
   return 0;
}

static void generate_response(fft_complex_t *response,
      const struct eq_gain *gains, unsigned num_gains, unsigned samples)
{
   unsigned i;

   float start_freq = 0.0f;
   float start_gain = 1.0f;

   float end_freq   = 1.0f;
   float end_gain   = 1.0f;

   if (num_gains)
   {
      end_freq = gains->freq;
      end_gain = gains->gain;
      num_gains--;
      gains++;
   }

   /* Create a response by linear interpolation between
    * known frequency sample points. */
   for (i = 0; i <= samples; i++)
   {
      float gain;
      float lerp = 0.5f;
      float freq = (float)i / samples;

      while (freq >= end_freq)
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
            start_freq = end_freq;
            start_gain = end_gain;
            end_freq = 1.0f;
            end_gain = 1.0f;
            break;
         }
      }

      /* Edge case where i == samples. */
      if (end_freq > start_freq)
         lerp = (freq - start_freq) / (end_freq - start_freq);
      gain = (1.0f - lerp) * start_gain + lerp * end_gain;

      response[i].real = gain;
      response[i].imag = 0.0f;
      response[2 * samples - i].real = gain;
      response[2 * samples - i].imag = 0.0f;
   }
}

static void create_filter(struct eq_data *eq, unsigned size_log2,
      struct eq_gain *gains, unsigned num_gains, double beta, const char *filter_path)
{
   int i;
   int half_block_size = eq->block_size >> 1;
   double window_mod = 1.0 / kaiser_window_function(0.0, beta);

   fft_t *fft = fft_new(size_log2);
   float *time_filter = (float*)calloc(eq->block_size * 2 + 1, sizeof(*time_filter));
   if (!fft || !time_filter)
      goto end;

   /* Make sure bands are in correct order. */
   qsort(gains, num_gains, sizeof(*gains), gains_cmp);

   /* Compute desired filter response. */
   generate_response(eq->filter, gains, num_gains, half_block_size);

   /* Get equivalent time-domain filter. */
   fft_process_inverse(fft, time_filter, eq->filter, 1);

   /* ifftshift() to create the correct linear phase filter.
    * The filter response was designed with zero phase, which
    * won't work unless we compensate
    * for the repeating property of the FFT here
    * by flipping left and right blocks. */
   for (i = 0; i < half_block_size; i++)
   {
      float tmp = time_filter[i + half_block_size];
      time_filter[i + half_block_size] = time_filter[i];
      time_filter[i] = tmp;
   }

   /* Apply a window to smooth out the frequency repsonse. */
   for (i = 0; i < (int)eq->block_size; i++)
   {
      /* Kaiser window. */
      double phase = (double)i / eq->block_size;
      phase = 2.0 * (phase - 0.5);
      time_filter[i] *= window_mod * kaiser_window_function(phase, beta);
   }

   /* Debugging. */
   if (filter_path)
   {
      FILE *file = fopen(filter_path, "w");
      if (file)
      {
         for (i = 0; i < (int)eq->block_size - 1; i++)
            fprintf(file, "%.8f\n", time_filter[i + 1]);
         fclose(file);
      }
   }

   /* Padded FFT to create our FFT filter.
    * Make our even-length filter odd by discarding the first coefficient.
    * For some interesting reason, this allows us to design an odd-length linear phase filter.
    */
   fft_process_forward(eq->fft, eq->filter, time_filter + 1, 1);

end:
   fft_free(fft);
   free(time_filter);
}

static void *eq_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float *frequencies, *gain;
   unsigned num_freq, num_gain, i, size;
   int size_log2;
   float beta;
   struct eq_gain *gains = NULL;
   char *filter_path = NULL;
   const float default_freq[] = { 0.0f, info->input_rate };
   const float default_gain[] = { 0.0f, 0.0f };
   struct eq_data *eq = (struct eq_data*)calloc(1, sizeof(*eq));
   if (!eq)
      return NULL;

   config->get_float(userdata, "window_beta", &beta, 4.0f);

   config->get_int(userdata, "block_size_log2", &size_log2, 8);
   size = 1 << size_log2;

   config->get_float_array(userdata, "frequencies", &frequencies, &num_freq, default_freq, 2);
   config->get_float_array(userdata, "gains", &gain, &num_gain, default_gain, 2);

   if (!config->get_string(userdata, "impulse_response_output", &filter_path, ""))
   {
      config->free(filter_path);
      filter_path = NULL;
   }

   num_gain = num_freq = MIN(num_gain, num_freq);

   gains = (struct eq_gain*)calloc(num_gain, sizeof(*gains));
   if (!gains)
      goto error;

   for (i = 0; i < num_gain; i++)
   {
      gains[i].freq = frequencies[i] / (0.5f * info->input_rate);
      gains[i].gain = pow(10.0, gain[i] / 20.0);
   }
   config->free(frequencies);
   config->free(gain);

   eq->block_size = size;

   eq->save     = (float*)calloc(    size, 2 * sizeof(*eq->save));
   eq->block    = (float*)calloc(2 * size, 2 * sizeof(*eq->block));
   eq->fftblock = (fft_complex_t*)calloc(2 * size, sizeof(*eq->fftblock));
   eq->filter   = (fft_complex_t*)calloc(2 * size, sizeof(*eq->filter));

   /* Use an FFT which is twice the block size with zero-padding
    * to make circular convolution => proper convolution.
    */
   eq->fft = fft_new(size_log2 + 1);

   if (!eq->fft || !eq->fftblock || !eq->save || !eq->block || !eq->filter)
      goto error;

   create_filter(eq, size_log2, gains, num_gain, beta, filter_path);
   config->free(filter_path);
   filter_path = NULL;

   free(gains);
   return eq;

error:
   free(gains);
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

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &eq_plug;
}

#undef dspfilter_get_implementation
