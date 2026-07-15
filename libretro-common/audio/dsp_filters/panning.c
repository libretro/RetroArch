/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (panning.c).
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

#include <libretro_dspfilter.h>

struct panning_data
{
   float left[2];
   float right[2];
   /* Q16 fixed-point mirror of left/right, for the int16 path. */
   int32_t left_i[2];
   int32_t right_i[2];
};

static void panning_free(void *data)
{
   free(data);
}

static void panning_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct panning_data *pan = (struct panning_data*)data;
   float *out               = output->samples;

   output->samples          = input->samples;
   output->frames           = input->frames;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float left  = out[0];
      float right = out[1];
      out[0]      = left * pan->left[0]  + right * pan->left[1];
      out[1]      = left * pan->right[0] + right * pan->right[1];
   }
}

/* Deterministic int16 path: same 2x2 mix as panning_process() but in Q16
 * fixed point (int64 accumulate, round-half-away-from-zero, saturate). */
static void panning_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i;
   struct panning_data *pan = (struct panning_data*)data;
   int16_t *out;

   output->samples          = input->samples;
   output->frames           = input->frames;
   out                      = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      int32_t left  = out[0];
      int32_t right = out[1];
      int64_t l     = (int64_t)left * pan->left_i[0]
                    + (int64_t)right * pan->left_i[1];
      int64_t r     = (int64_t)left * pan->right_i[0]
                    + (int64_t)right * pan->right_i[1];
      l = (l >= 0) ? ((l + 32768) >> 16) : -(((-l) + 32768) >> 16);
      r = (r >= 0) ? ((r + 32768) >> 16) : -(((-r) + 32768) >> 16);
      if      (l >  32767) l =  32767;
      else if (l < -32768) l = -32768;
      if      (r >  32767) r =  32767;
      else if (r < -32768) r = -32768;
      out[0] = (int16_t)l;
      out[1] = (int16_t)r;
   }
}

static void *panning_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   static const float default_left[]  = { 1.0f, 0.0f };
   static const float default_right[] = { 0.0f, 1.0f };
   float *left                        = NULL;
   float *right                       = NULL;
   unsigned num_left                  = 0;
   unsigned num_right                 = 0;
   struct panning_data *pan           = (struct panning_data*)
      calloc(1, sizeof(*pan));

   if (!pan)
      return NULL;

   config->get_float_array(userdata, "left_mix",
         &left, &num_left, default_left, 2);
   config->get_float_array(userdata, "right_mix",
         &right, &num_right, default_right, 2);

   memcpy(pan->left,  (num_left  == 2) ?
         left :  default_left,  sizeof(pan->left));
   memcpy(pan->right, (num_right == 2) ?
         right : default_right, sizeof(pan->right));

   pan->left_i[0]  = (int32_t)floor((double)pan->left[0]  * 65536.0 + 0.5);
   pan->left_i[1]  = (int32_t)floor((double)pan->left[1]  * 65536.0 + 0.5);
   pan->right_i[0] = (int32_t)floor((double)pan->right[0] * 65536.0 + 0.5);
   pan->right_i[1] = (int32_t)floor((double)pan->right[1] * 65536.0 + 0.5);

   config->free(left);
   config->free(right);

   return pan;
}

static const struct dspfilter_implementation panning = {
   panning_init,
   panning_process,
   panning_free,

   DSPFILTER_API_VERSION,
   "Panning",
   "panning",

   panning_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation panning_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *
dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   return &panning;
}

#undef dspfilter_get_implementation
