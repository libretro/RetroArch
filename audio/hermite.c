/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

// Hermite resampler based on bsnes' audio library.

#include "resampler.h"
#include <stdlib.h>
#include <math.h>
#include "../boolean.h"
#include "../general.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#define CHANNELS 2

struct rarch_resampler
{
   float chan_data[CHANNELS][4];
   double r_frac;
};

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

rarch_resampler_t *resampler_new(void)
{
#ifndef RESAMPLER_TEST
   RARCH_LOG("Hermite resampler [C]\n");
#endif
   return (rarch_resampler_t*)calloc(1, sizeof(rarch_resampler_t));
}

void resampler_process(rarch_resampler_t *re, struct resampler_data *data)
{
   double r_step = 1.0 / data->ratio;
   size_t processed_out = 0;

   size_t in_frames = data->input_frames;
   const float *in_data = data->data_in;
   float *out_data = data->data_out;

   size_t i = 0;
   while (i < in_frames)
   {
      while (re->r_frac >= 1.0 && i < in_frames)
      {
         re->r_frac -= 1.0;
         for (unsigned c = 0; c < CHANNELS; c++)
         {
            re->chan_data[c][0] = re->chan_data[c][1];
            re->chan_data[c][1] = re->chan_data[c][2];
            re->chan_data[c][2] = re->chan_data[c][3];
            re->chan_data[c][3] = *in_data++;
         }
         i++;
      }

      while (re->r_frac <= 1.0)
      {
         re->r_frac += r_step;
         for (unsigned c = 0; c < CHANNELS; c++)
         {
            float res = hermite_kernel((float)re->r_frac, 
                  re->chan_data[c][0], re->chan_data[c][1],
                  re->chan_data[c][2], re->chan_data[c][3]);
            *out_data++ = res;
         }
         processed_out++;
      }
   }

   data->output_frames = processed_out;
}

void resampler_free(rarch_resampler_t *re)
{
   free(re);
}

