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
#include <math.h>
#include <stdlib.h>
#include <string.h>

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
   float *out;
   struct panning_data *pan = (struct panning_data*)data;

   output->samples          = input->samples;
   output->frames           = input->frames;
   out                      = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float left = out[0];
      float right = out[1];
      out[0] = left * pan->left[0]  + right * pan->left[1];
      out[1] = left * pan->right[0] + right * pan->right[1];
   }
}

static void *panning_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   static const float default_left[] = { 1.0f, 0.0f };
   static const float default_right[] = { 0.0f, 1.0f };
   float *left = NULL, *right = NULL;
   unsigned num_left = 0, num_right = 0;
   struct panning_data *pan = (struct panning_data*)calloc(1, sizeof(*pan));
   if (!pan)
      return NULL;

   config->get_float_array(userdata, "left_mix", &left, &num_left, default_left, 2);
   config->get_float_array(userdata, "right_mix", &right, &num_right, default_right, 2);

   memcpy(pan->left,  (num_left  == 2) ? left :  default_left,  sizeof(pan->left));
   memcpy(pan->right, (num_right == 2) ? right : default_right, sizeof(pan->right));

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

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &panning;
}

#undef dspfilter_get_implementation

