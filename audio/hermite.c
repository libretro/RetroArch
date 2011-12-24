/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// Hermite resampler based on bsnes' audio library.

#include "hermite.h"
#include <stdlib.h>

#define CHANNELS 2

struct hermite_resampler
{
   float chan_data[CHANNELS][4];
   double r_frac;
};

static inline float hermite_kernel(float mu1, float a, float b, float c, float d)
{
   float mu2, mu3, m0, m1, a0, a1, a2, a3;

   mu2 = mu1 * mu1;
   mu3 = mu2 * mu1;

   m0  = (c - a) * 0.5;
   m1  = (d - b) * 0.5;

   a0 = +2 * mu3 - 3 * mu2 + 1;
   a1 =      mu3 - 2 * mu2 + mu1;
   a2 =      mu3 -     mu2;
   a3 = -2 * mu3 + 3 * mu2;

   return (a0 * b) + (a1 * m0) + (a2 * m1) + (a3 * c);
}

hermite_resampler_t *hermite_new(void)
{
   hermite_resampler_t *re = (hermite_resampler_t*)calloc(1, sizeof(*re));
   if (!re)
      return NULL;
   return re;
}

// We make sure to allocate enough output data beforehand ... ;)
void hermite_process(hermite_resampler_t *re, struct hermite_data *data)
{
   double r_step = 1.0 / data->ratio;
   size_t processed_out = 0;

   size_t in_frames = data->input_frames;
   const float *in_data = data->data_in;
   float *out_data = data->data_out;

   for (size_t i = 0; i < in_frames; i++)
   {
      while (re->r_frac <= 1.0)
      {
         re->r_frac += r_step;
         for (unsigned i = 0; i < CHANNELS; i++)
         {
            float res = hermite_kernel(re->r_frac, 
                  re->chan_data[i][0], re->chan_data[i][1], re->chan_data[i][2], re->chan_data[i][3]);
            *out_data++ = res;
         }
         processed_out++;
      }

      re->r_frac -= 1.0;
      for (unsigned i = 0; i < CHANNELS; i++)
      {
         re->chan_data[i][0] = re->chan_data[i][1];
         re->chan_data[i][1] = re->chan_data[i][2];
         re->chan_data[i][2] = re->chan_data[i][3];
         re->chan_data[i][3] = *in_data++;
      }
   }

   data->output_frames = processed_out;
}

void hermite_free(hermite_resampler_t *re)
{
   free(re);
}

