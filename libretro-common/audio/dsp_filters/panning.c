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
