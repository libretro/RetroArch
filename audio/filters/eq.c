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
   unsigned block_size;
   unsigned block_ptr;
};

static void eq_free(void *data)
{
   struct eq_data *eq = (struct eq_data*)data;
   if (!eq)
      return;

   rarch_fft_free(eq->fft);
   free(eq->save);
   free(eq->block);
   free(eq->filter);
   free(eq);
}

static void eq_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   struct eq_data *eq = (struct eq_data*)data;
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

   eq->save   = (float*)calloc(2 * size, sizeof(*eq->save));
   eq->block  = (float*)calloc(2 * size, 2 * sizeof(*eq->block));
   eq->filter = (rarch_fft_complex_t*)calloc(2 * size, sizeof(*eq->filter));
   eq->fft    = rarch_fft_new(size_log2 + 1);

   if (!eq->fft || !eq->save || !eq->block || !eq->filter)
      goto error;

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

