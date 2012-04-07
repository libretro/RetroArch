/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "resampler.h"
#include <stdlib.h>
#include <math.h>
#include "../boolean.h"

#define CHANNELS 2

struct ssnes_resampler
{
   float chan_data[CHANNELS][4];
   double r_frac;
};

void resampler_preinit(ssnes_resampler_t *re, double omega, double *samples_offset)
{
   *samples_offset = 2.0;
   for (int i = 0; i < 4; i++)
   {
      re->chan_data[0][i] = (float)cos((i - 2) * omega);
      re->chan_data[1][i] = re->chan_data[0][i];
   }

   re->r_frac = 0.0;
}

static inline float hermite_kernel(float mu1, float a, float b, float c, float d)
{
   float mu2, mu3, m0, m1, a0, a1, a2, a3;

   mu2 = mu1 * mu1;
   mu3 = mu2 * mu1;

   m0  = (c - a) * 0.5f;
   m1  = (d - b) * 0.5f;

   a0 = +2 * mu3 - 3 * mu2 + 1;
   a1 =      mu3 - 2 * mu2 + mu1;
   a2 =      mu3 -     mu2;
   a3 = -2 * mu3 + 3 * mu2;

   return (a0 * b) + (a1 * m0) + (a2 * m1) + (a3 * c);
}

ssnes_resampler_t *resampler_new(void)
{
   return (ssnes_resampler_t*)calloc(1, sizeof(ssnes_resampler_t));
}

void resampler_process(ssnes_resampler_t *re, struct resampler_data *data)
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
            float res = hermite_kernel((float)re->r_frac, 
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

void resampler_free(ssnes_resampler_t *re)
{
   free(re);
}

