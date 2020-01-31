/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fft.c).
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

#include <math.h>
#include <stdlib.h>

#include "fft.h"

#include <retro_miscellaneous.h>

struct fft
{
   fft_complex_t *interleave_buffer;
   fft_complex_t *phase_lut;
   unsigned *bitinverse_buffer;
   unsigned size;
};

static unsigned bitswap(unsigned x, unsigned size_log2)
{
   unsigned i;
   unsigned ret = 0;
   for (i = 0; i < size_log2; i++)
      ret |= ((x >> i) & 1) << (size_log2 - i - 1);
   return ret;
}

static void build_bitinverse(unsigned *bitinverse, unsigned size_log2)
{
   unsigned i;
   unsigned size = 1 << size_log2;
   for (i = 0; i < size; i++)
      bitinverse[i] = bitswap(i, size_log2);
}

static fft_complex_t exp_imag(double phase)
{
   fft_complex_t out = { cos(phase), sin(phase) };
   return out;
}

static void build_phase_lut(fft_complex_t *out, int size)
{
   int i;
   out += size;
   for (i = -size; i <= size; i++)
      out[i] = exp_imag((M_PI * i) / size);
}

static void interleave_complex(const unsigned *bitinverse,
      fft_complex_t *out, const fft_complex_t *in,
      unsigned samples, unsigned step)
{
   unsigned i;
   for (i = 0; i < samples; i++, in += step)
      out[bitinverse[i]] = *in;
}

static void interleave_float(const unsigned *bitinverse,
      fft_complex_t *out, const float *in,
      unsigned samples, unsigned step)
{
   unsigned i;
   for (i = 0; i < samples; i++, in += step)
   {
      unsigned inv_i = bitinverse[i];
      out[inv_i].real = *in;
      out[inv_i].imag = 0.0f;
   }
}

static void resolve_float(float *out, const fft_complex_t *in, unsigned samples,
      float gain, unsigned step)
{
   unsigned i;
   for (i = 0; i < samples; i++, in++, out += step)
      *out = gain * in->real;
}

fft_t *fft_new(unsigned block_size_log2)
{
   unsigned size;
   fft_t *fft = (fft_t*)calloc(1, sizeof(*fft));
   if (!fft)
      return NULL;

   size                   = 1 << block_size_log2;
   fft->interleave_buffer = (fft_complex_t*)calloc(size, sizeof(*fft->interleave_buffer));
   fft->bitinverse_buffer = (unsigned*)calloc(size, sizeof(*fft->bitinverse_buffer));
   fft->phase_lut         = (fft_complex_t*)calloc(2 * size + 1, sizeof(*fft->phase_lut));

   if (!fft->interleave_buffer || !fft->bitinverse_buffer || !fft->phase_lut)
      goto error;

   fft->size = size;

   build_bitinverse(fft->bitinverse_buffer, block_size_log2);
   build_phase_lut(fft->phase_lut, size);
   return fft;

error:
   fft_free(fft);
   return NULL;
}

void fft_free(fft_t *fft)
{
   if (!fft)
      return;

   free(fft->interleave_buffer);
   free(fft->bitinverse_buffer);
   free(fft->phase_lut);
   free(fft);
}

static void butterfly(fft_complex_t *a, fft_complex_t *b, fft_complex_t mod)
{
   mod = fft_complex_mul(mod, *b);
   *b  = fft_complex_sub(*a, mod);
   *a  = fft_complex_add(*a, mod);
}

static void butterflies(fft_complex_t *butterfly_buf,
      const fft_complex_t *phase_lut,
      int phase_dir, unsigned step_size, unsigned samples)
{
   unsigned i, j;
   for (i = 0; i < samples; i += step_size << 1)
   {
      int phase_step = (int)samples * phase_dir / (int)step_size;
      for (j = i; j < i + step_size; j++)
         butterfly(&butterfly_buf[j], &butterfly_buf[j + step_size],
               phase_lut[phase_step * (int)(j - i)]);
   }
}

void fft_process_forward_complex(fft_t *fft,
      fft_complex_t *out, const fft_complex_t *in, unsigned step)
{
   unsigned step_size;
   unsigned samples = fft->size;
   interleave_complex(fft->bitinverse_buffer, out, in, samples, step);

   for (step_size = 1; step_size < samples; step_size <<= 1)
   {
      butterflies(out,
            fft->phase_lut + samples,
            -1, step_size, samples);
   }
}

void fft_process_forward(fft_t *fft,
      fft_complex_t *out, const float *in, unsigned step)
{
   unsigned step_size;
   unsigned samples = fft->size;
   interleave_float(fft->bitinverse_buffer, out, in, samples, step);

   for (step_size = 1; step_size < fft->size; step_size <<= 1)
   {
      butterflies(out,
            fft->phase_lut + samples,
            -1, step_size, samples);
   }
}

void fft_process_inverse(fft_t *fft,
      float *out, const fft_complex_t *in, unsigned step)
{
   unsigned step_size;
   unsigned samples = fft->size;

   interleave_complex(fft->bitinverse_buffer, fft->interleave_buffer,
         in, samples, 1);

   for (step_size = 1; step_size < samples; step_size <<= 1)
   {
      butterflies(fft->interleave_buffer,
            fft->phase_lut + samples,
            1, step_size, samples);
   }

   resolve_float(out, fft->interleave_buffer, samples, 1.0f / samples, step);
}
