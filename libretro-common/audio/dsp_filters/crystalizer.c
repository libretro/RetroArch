/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (echo.c).
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
#include <string.h>

#include <retro_miscellaneous.h>
#include <libretro_dspfilter.h>

struct delta_data
{
   float intensity;
   float old[2];
   /* Q16 fixed-point mirror of intensity, and int16 history, for the
    * int16 path. */
   int32_t intensity_i;
   int32_t old_i[2];
};

static void delta_free(void *data)
{
   free(data);
}

static void delta_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i, c;
   struct delta_data *d   = (struct delta_data*)data;
   float *out             = output->samples;
   output->samples        = input->samples;
   output->frames         = input->frames;

   for (i = 0; i < input->frames; i++)
   {
      for (c = 0; c < 2; c++)
      {
           float current  = *out;
           *out++         = current + (current - d->old[c]) * d->intensity;
           d->old[c]      = current;
      }
   }
}

/* Deterministic int16 path: same first-difference sharpening as
 * delta_process() but in Q16 fixed point (int64 term, round-half-away-from-
 * zero, saturate).  The float path leaves clamping to the host; writing int16
 * directly, this saturates to the s16 range. */
static void delta_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i, c;
   struct delta_data *d   = (struct delta_data*)data;
   int16_t *out;
   output->samples        = input->samples;
   output->frames         = input->frames;
   out                    = output->samples;

   for (i = 0; i < input->frames; i++)
   {
      for (c = 0; c < 2; c++)
      {
         int32_t current = *out;
         int32_t diff    = current - d->old_i[c];
         int64_t term    = (int64_t)diff * d->intensity_i;
         int32_t term_r  = (term >= 0)
               ?  (int32_t)(( term + 32768) >> 16)
               : -(int32_t)((-term + 32768) >> 16);
         int32_t v       = current + term_r;
         if      (v >  32767) v =  32767;
         else if (v < -32768) v = -32768;
         *out++          = (int16_t)v;
         d->old_i[c]     = current;
      }
   }
}

static void *delta_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   struct delta_data *d = (struct delta_data*)calloc(1, sizeof(*d));
   if (!d)
      return NULL;
   config->get_float(userdata, "intensity", &d->intensity, 5.0f);
   d->intensity_i = (int32_t)floor((double)d->intensity * 65536.0 + 0.5);
   return d;
}

static const struct dspfilter_implementation delta_plug = {
   delta_init,
   delta_process,
   delta_free,
   DSPFILTER_API_VERSION,
   "Delta Sharpening",
   "crystalizer",
   delta_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation delta_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   return &delta_plug;
}

#undef dspfilter_get_implementation
