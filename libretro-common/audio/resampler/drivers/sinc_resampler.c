/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (sinc_resampler.c).
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

/* Bog-standard windowed SINC implementation. */

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <retro_inline.h>
#include <filters.h>
#include <memalign.h>

#include <audio/audio_resampler.h>

#include "sinc_resampler_common.h"

static void resampler_sinc_process(void *re_, struct resampler_data *data)
{
   rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)re_;

   uint32_t ratio        = PHASES / data->ratio;
   const float *input    = data->data_in;
   float *output         = data->data_out;
   size_t frames         = data->input_frames;
   size_t out_frames     = 0;

   while (frames)
   {
      while (frames && re->time >= PHASES)
      {
         /* Push in reverse to make filter more obvious. */
         if (!re->ptr)
            re->ptr = re->taps;
         re->ptr--;

         re->buffer_l[re->ptr + re->taps] = re->buffer_l[re->ptr] = *input++;
         re->buffer_r[re->ptr + re->taps] = re->buffer_r[re->ptr] = *input++;

         re->time -= PHASES;
         frames--;
      }

      while (re->time < PHASES)
      {
         process_sinc_func(re, output);
         output += 2;
         out_frames++;
         re->time += ratio;
      }
   }

   data->output_frames = out_frames;
}

static void *resampler_sinc_new(const struct resampler_config *config,
      double bandwidth_mod, resampler_simd_mask_t mask)
{
   double cutoff;
   size_t phase_elems, elems;
   rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)
      calloc(1, sizeof(*re));

   if (!re)
      return NULL;

   (void)config;

   re->taps = TAPS;
   cutoff   = CUTOFF;

   /* Downsampling, must lower cutoff, and extend number of 
    * taps accordingly to keep same stopband attenuation. */
   if (bandwidth_mod < 1.0)
   {
      cutoff *= bandwidth_mod;
      re->taps = (unsigned)ceil(re->taps / bandwidth_mod);
   }

   /* Be SIMD-friendly. */
#if (defined(__AVX__) && ENABLE_AVX) || (defined(__ARM_NEON__))
   re->taps     = (re->taps + 7) & ~7;
#else
   re->taps     = (re->taps + 3) & ~3;
#endif

   phase_elems  = (1 << PHASE_BITS) * re->taps;
#if SINC_COEFF_LERP
   phase_elems *= 2;
#endif
   elems        = phase_elems + 4 * re->taps;

   re->main_buffer = (float*)memalign_alloc(128, sizeof(float) * elems);
   if (!re->main_buffer)
      goto error;

   re->phase_table = re->main_buffer;
   re->buffer_l    = re->main_buffer + phase_elems;
   re->buffer_r    = re->buffer_l + 2 * re->taps;

   sinc_init_table(re, cutoff, re->phase_table,
         1 << PHASE_BITS, re->taps, SINC_COEFF_LERP);

#if defined(__ARM_NEON__) 
   process_sinc_func = mask & RESAMPLER_SIMD_NEON 
      ? process_sinc_neon : process_sinc_C;
#endif

   return re;

error:
   sinc_free(re);
   return NULL;
}

retro_resampler_t sinc_resampler = {
   resampler_sinc_new,
   resampler_sinc_process,
   sinc_free,
   RESAMPLER_API_VERSION,
   "sinc",
   "sinc"
};
