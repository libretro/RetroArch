/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <stdint.h>
#include <string.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <filters.h>
#include <libretro_dspfilter.h>

#include "fft/fft.c"

/* int16 path: a fixed-point mirror of the float FFT convolution. Samples
 * enter the transform in Q13, twiddles are Q30, and each butterfly stage
 * halves the whole block (tracking a shared exponent) when a value could
 * otherwise overflow, so precision follows the signal for any block size.
 * The overlap-add tail is kept in Q8. */
#define EQ_I16_IN_SHIFT 13
#define EQ_I16_LIMIT    ((int32_t)1 << 29)
#define EQ_I16_SAVE_MAX ((int32_t)1 << 30)

struct eq_data
{
   fft_t *fft;
   /* Every array below except the two output buffers is a view into this
    * one block. */
   uint8_t *arena;
   float *save;
   float *block;
   fft_complex_t *filter;
   fft_complex_t *fftblock;
   int16_t *block_i;
   int32_t *save_i;
   int32_t *work_i;
   int32_t *filter_i;
   int32_t *twiddle_i;
   /* Output grows to the largest call seen: each completed block emits
    * block_size frames, and the float inverse transform writes a further
    * block_size frames of tail past them. */
   float *buffer;
   size_t buffer_cap;
   int16_t *buffer_i16;
   size_t buffer_i16_cap;
   unsigned block_size;
   unsigned block_ptr;
   unsigned block_ptr_i;
   unsigned fft_log2;
   int filter_exp;
};

struct eq_gain
{
   float freq;
   float gain; /* Linear. */
};

/* Grows *buf to at least need elements; cap counts elements. */
static int eq_reserve(void **buf, size_t *cap, size_t elem, size_t need)
{
   void *p;
   if (need <= *cap)
      return 1;
   if (!(p = realloc(*buf, need * elem)))
      return 0;
   *buf = p;
   *cap = need;
   return 1;
}

static void eq_free(void *data)
{
   struct eq_data *eq = (struct eq_data*)data;
   if (!eq)
      return;

   fft_free(eq->fft);
   free(eq->arena);
   free(eq->buffer);
   free(eq->buffer_i16);
   free(eq);
}

static void eq_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   float *out;
   const float *in;
   unsigned input_frames;
   struct eq_data *eq = (struct eq_data*)data;
   size_t blocks      = ((size_t)eq->block_ptr + input->frames) / eq->block_size;

   if (!eq_reserve((void**)&eq->buffer, &eq->buffer_cap, sizeof(float),
            (blocks + 1) * eq->block_size * 2))
   {
      output->samples = input->samples;
      output->frames  = input->frames;
      return;
   }

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

      in            += write_avail * 2;
      input_frames  -= write_avail;
      eq->block_ptr += write_avail;

      /* Convolve a new block. */
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

         /* Overlap add method, so add in saved block now. */
         for (i = 0; i < 2 * eq->block_size; i++)
            out[i]      += eq->save[i];

         /* Save block for later. */
         memcpy(eq->save, out + 2 * eq->block_size, 2 * eq->block_size * sizeof(float));

         out            += eq->block_size * 2;
         output->frames += eq->block_size;
         eq->block_ptr   = 0;
      }
   }
}

static INLINE int64_t eq_rsh(int64_t v, unsigned s)
{
   int64_t h;
   if (!s)
      return v;
   h = (int64_t)1 << (s - 1);
   return (v >= 0) ? ((v + h) >> s) : -(((-v) + h) >> s);
}

static INLINE uint32_t eq_abs32(int32_t v)
{
   return (uint32_t)(v < 0 ? -v : v);
}

/* In-place radix-2 transform on interleaved Q-format complex values in
 * bit-reversed order; dir is -1 forward and +1 inverse, matching the
 * float butterflies. *exp gains one for every halving. */
static void eq_fft_i(const struct eq_data *eq, int32_t *x, int dir, int *exp)
{
   unsigned step, i, j;
   unsigned n        = eq->fft->size;
   const int32_t *tw = eq->twiddle_i + 2 * (size_t)n;
   uint32_t m        = 0;

   for (i = 0; i < 2 * n; i++)
      m |= eq_abs32(x[i]);

   for (step = 1; step < n; step <<= 1)
   {
      int phase_step = (int)n * dir / (int)step;
      /* |a| + |w * b| stays below 2^31 while every value is below 2^29. */
      if (m >= (uint32_t)EQ_I16_LIMIT)
      {
         for (i = 0; i < 2 * n; i++)
            x[i] = (int32_t)eq_rsh(x[i], 1);
         (*exp)++;
      }
      m = 0;
      for (i = 0; i < n; i += step << 1)
      {
         for (j = i; j < i + step; j++)
         {
            const int32_t *w = tw + 2 * (phase_step * (int)(j - i));
            int32_t *a       = x + 2 * j;
            int32_t *b       = x + 2 * (j + step);
            int32_t tr       = (int32_t)eq_rsh((int64_t)w[0] * b[0]
                  - (int64_t)w[1] * b[1], 30);
            int32_t ti       = (int32_t)eq_rsh((int64_t)w[0] * b[1]
                  + (int64_t)w[1] * b[0], 30);
            b[0]             = a[0] - tr;
            b[1]             = a[1] - ti;
            a[0]            += tr;
            a[1]            += ti;
            m |= eq_abs32(a[0]) | eq_abs32(a[1])
               | eq_abs32(b[0]) | eq_abs32(b[1]);
         }
      }
   }
}

/* Multiplies the spectrum by the filter and renormalises the products
 * below 2^29, folding the scale into *exp. */
static void eq_spectrum_mul_i(const struct eq_data *eq, int32_t *x, int *exp)
{
   unsigned i, shift = 0;
   unsigned n        = eq->fft->size;
   const int32_t *h  = eq->filter_i;
   uint64_t m        = 0;

   for (i = 0; i < n; i++)
   {
      int64_t re = (int64_t)x[2 * i] * h[2 * i] - (int64_t)x[2 * i + 1] * h[2 * i + 1];
      int64_t im = (int64_t)x[2 * i] * h[2 * i + 1] + (int64_t)x[2 * i + 1] * h[2 * i];
      m |= (uint64_t)(re < 0 ? -re : re) | (uint64_t)(im < 0 ? -im : im);
   }
   while ((m >> shift) >= (uint64_t)EQ_I16_LIMIT)
      shift++;
   for (i = 0; i < n; i++)
   {
      int64_t re = (int64_t)x[2 * i] * h[2 * i] - (int64_t)x[2 * i + 1] * h[2 * i + 1];
      int64_t im = (int64_t)x[2 * i] * h[2 * i + 1] + (int64_t)x[2 * i + 1] * h[2 * i];
      x[2 * i]     = (int32_t)eq_rsh(re, shift);
      x[2 * i + 1] = (int32_t)eq_rsh(im, shift);
   }
   *exp += eq->filter_exp + (int)shift;
}

/* value * 2^shift as a saturated Q8 sample. */
static INLINE int32_t eq_to_q8(int32_t v, int shift)
{
   int64_t r;
   if (shift >= 0)
   {
      if (shift > 32)
         shift = 32;
      r = (int64_t)v * ((int64_t)1 << shift);
   }
   else
      r = eq_rsh(v, (unsigned)(-shift > 62 ? 62 : -shift));
   if (r > EQ_I16_SAVE_MAX)
      return EQ_I16_SAVE_MAX;
   if (r < -EQ_I16_SAVE_MAX)
      return -EQ_I16_SAVE_MAX;
   return (int32_t)r;
}

/* Convolves one full input block, emitting block_size frames to out. */
static void eq_block_i16(struct eq_data *eq, int16_t *out)
{
   unsigned i, c;
   unsigned n                = eq->fft->size;
   const unsigned *bitinv    = eq->fft->bitinverse_buffer;
   int32_t *x                = eq->work_i;

   for (c = 0; c < 2; c++)
   {
      int exp = -EQ_I16_IN_SHIFT;
      int shift;

      for (i = 0; i < n; i++)
      {
         unsigned k   = bitinv[i];
         x[2 * k]     = (i < eq->block_size)
            ? (int32_t)eq->block_i[2 * i + c] * (1 << EQ_I16_IN_SHIFT) : 0;
         x[2 * k + 1] = 0;
      }
      eq_fft_i(eq, x, -1, &exp);
      eq_spectrum_mul_i(eq, x, &exp);
      /* The inverse runs in natural order in place, so reorder first. */
      for (i = 0; i < n; i++)
      {
         unsigned k = bitinv[i];
         if (k > i)
         {
            int32_t tr = x[2 * i], ti = x[2 * i + 1];
            x[2 * i]     = x[2 * k];
            x[2 * i + 1] = x[2 * k + 1];
            x[2 * k]     = tr;
            x[2 * k + 1] = ti;
         }
      }
      eq_fft_i(eq, x, 1, &exp);

      /* Real part times 2^exp / n, in Q8. */
      shift = exp - (int)eq->fft_log2 + 8;
      for (i = 0; i < eq->block_size; i++)
      {
         int64_t v = (int64_t)eq_to_q8(x[2 * i], shift) + eq->save_i[2 * i + c];
         v         = eq_rsh(v, 8);
         out[2 * i + c] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
         eq->save_i[2 * i + c] = eq_to_q8(x[2 * (i + eq->block_size)], shift);
      }
   }
}

static void eq_process_i16(void *data, struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   int16_t *out;
   struct eq_data *eq    = (struct eq_data*)data;
   const int16_t *in     = input->samples;
   unsigned input_frames = input->frames;
   size_t blocks         = ((size_t)eq->block_ptr_i + input_frames) / eq->block_size;

   output->samples       = input->samples;
   output->frames        = 0;
   if (blocks && !eq_reserve((void**)&eq->buffer_i16, &eq->buffer_i16_cap,
            sizeof(int16_t), blocks * eq->block_size * 2))
   {
      output->frames = input->frames;
      return;
   }
   if (blocks)
      output->samples = eq->buffer_i16;
   out = eq->buffer_i16;

   while (input_frames)
   {
      unsigned write_avail = eq->block_size - eq->block_ptr_i;
      if (input_frames < write_avail)
         write_avail = input_frames;

      memcpy(eq->block_i + eq->block_ptr_i * 2, in,
            write_avail * 2 * sizeof(int16_t));

      in              += write_avail * 2;
      input_frames    -= write_avail;
      eq->block_ptr_i += write_avail;

      if (eq->block_ptr_i == eq->block_size)
      {
         eq_block_i16(eq, out);
         out            += eq->block_size * 2;
         output->frames += eq->block_size;
         eq->block_ptr_i = 0;
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

      response[i].real               = gain;
      response[i].imag               = 0.0f;
      response[2 * samples - i].real = gain;
      response[2 * samples - i].imag = 0.0f;
   }
}

static void create_filter(struct eq_data *eq, unsigned size_log2,
      struct eq_gain *gains, unsigned num_gains, double beta, const char *filter_path)
{
   int i;
   int half_block_size = eq->block_size >> 1;
   double window_mod   = 1.0 / kaiser_window_function(0.0, beta);
   fft_t *fft          = fft_new(size_log2);
   float *time_filter  = (float*)calloc(eq->block_size * 2 + 1, sizeof(*time_filter));
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

   /* Apply a window to smooth out the frequency response. */
   for (i = 0; i < (int)eq->block_size; i++)
   {
      /* Kaiser window. */
      double phase    = (double)i / eq->block_size;
      phase           = 2.0 * (phase - 0.5);
      time_filter[i] *= window_mod * kaiser_window_function(phase, beta);
   }

#ifdef DEBUG
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
#endif

   /* Padded FFT to create our FFT filter.
    * Make our even-length filter odd by discarding the first coefficient.
    * For some interesting reason, this allows us to design an odd-length linear phase filter.
    */
   fft_process_forward(eq->fft, eq->filter, time_filter + 1, 1);

end:
   fft_free(fft);
   free(time_filter);
}

/* Region spacing inside the arena, in bytes. */
#define EQ_ARENA_ALIGN 64
#define EQ_ARENA_NEXT(cur, bytes) \
   ((((cur) + (bytes) + EQ_ARENA_ALIGN - 1) / EQ_ARENA_ALIGN) * EQ_ARENA_ALIGN)

static void *eq_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   int size_log2;
   size_t save_len, block_len, fftblock_len, filter_len;
   size_t block_i_len, save_i_len, work_i_len, filter_i_len, twiddle_i_len;
   float beta;
   float *frequencies, *gain;
   unsigned num_freq, num_gain, i, size;
   struct eq_gain *gains      = NULL;
   char *filter_path          = NULL;
   float default_freq[2];
   const float default_gain[] = { 0.0f, 0.0f };
   struct eq_data *eq         = (struct eq_data*)calloc(1, sizeof(*eq));
   if (!eq)
      return NULL;

   default_freq[0] = 0.0f;
   default_freq[1] = info->input_rate;

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

   if (!(gains = (struct eq_gain*)calloc(num_gain, sizeof(*gains))))
      goto error;

   for (i = 0; i < num_gain; i++)
   {
      gains[i].freq = frequencies[i] / (0.5f * info->input_rate);
      gains[i].gain = pow(10.0, gain[i] / 20.0);
   }
   config->free(frequencies);
   config->free(gain);

   eq->block_size = size;

   /* The overlap-save state, the input block and the two FFT-domain
    * blocks come out of one zeroed arena, each region starting on a
    * 64-byte boundary. */
   save_len       = EQ_ARENA_NEXT(0,        size * 2 * sizeof(*eq->save));
   block_len      = EQ_ARENA_NEXT(save_len, 2 * size * 2 * sizeof(*eq->block));
   fftblock_len   = EQ_ARENA_NEXT(block_len, 2 * size * sizeof(*eq->fftblock));
   filter_len     = EQ_ARENA_NEXT(fftblock_len, 2 * size * sizeof(*eq->filter));
   /* int16 path: input block, Q8 overlap tail, transform scratch, filter
    * spectrum and the twiddle table for phases -n..n of the 2*size FFT. */
   block_i_len    = EQ_ARENA_NEXT(filter_len,   size * 2 * sizeof(int16_t));
   save_i_len     = EQ_ARENA_NEXT(block_i_len,  size * 2 * sizeof(int32_t));
   work_i_len     = EQ_ARENA_NEXT(save_i_len,   2 * size * 2 * sizeof(int32_t));
   filter_i_len   = EQ_ARENA_NEXT(work_i_len,   2 * size * 2 * sizeof(int32_t));
   twiddle_i_len  = EQ_ARENA_NEXT(filter_i_len, (4 * (size_t)size + 1) * 2 * sizeof(int32_t));
   if ((eq->arena = (uint8_t*)calloc(1, twiddle_i_len)))
   {
      eq->save      = (float*)(eq->arena);
      eq->block     = (float*)(eq->arena + save_len);
      eq->fftblock  = (fft_complex_t*)(eq->arena + block_len);
      eq->filter    = (fft_complex_t*)(eq->arena + fftblock_len);
      eq->block_i   = (int16_t*)(eq->arena + filter_len);
      eq->save_i    = (int32_t*)(eq->arena + block_i_len);
      eq->work_i    = (int32_t*)(eq->arena + save_i_len);
      eq->filter_i  = (int32_t*)(eq->arena + work_i_len);
      eq->twiddle_i = (int32_t*)(eq->arena + filter_i_len);
   }

   /* Use an FFT which is twice the block size with zero-padding
    * to make circular convolution => proper convolution.
    */
   eq->fft        = fft_new(size_log2 + 1);

   if (!eq->fft || !eq->arena)
      goto error;

   create_filter(eq, size_log2, gains, num_gain, beta, filter_path);
   config->free(filter_path);
   filter_path = NULL;

   /* int16 path: Q30 twiddles and a Q-scaled copy of the filter spectrum,
    * scaled so its largest component sits just under 2^30. */
   eq->fft_log2 = (unsigned)size_log2 + 1;
   {
      int k, e;
      int n      = (int)(2 * size);
      double big = 0.0;
      for (k = -n; k <= n; k++)
      {
         eq->twiddle_i[2 * (k + n)]     = (int32_t)floor(
               cos(M_PI * k / n) * 1073741824.0 + 0.5);
         eq->twiddle_i[2 * (k + n) + 1] = (int32_t)floor(
               sin(M_PI * k / n) * 1073741824.0 + 0.5);
      }
      for (k = 0; k < n; k++)
      {
         if (fabs(eq->filter[k].real) > big)
            big = fabs(eq->filter[k].real);
         if (fabs(eq->filter[k].imag) > big)
            big = fabs(eq->filter[k].imag);
      }
      frexp(big > 0.0 ? big : 1.0, &e);
      eq->filter_exp = e - 30;
      for (k = 0; k < n; k++)
      {
         eq->filter_i[2 * k]     = (int32_t)floor(
               ldexp(eq->filter[k].real, -eq->filter_exp) + 0.5);
         eq->filter_i[2 * k + 1] = (int32_t)floor(
               ldexp(eq->filter[k].imag, -eq->filter_exp) + 0.5);
      }
   }

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

   eq_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation eq_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   return &eq_plug;
}

#undef dspfilter_get_implementation
