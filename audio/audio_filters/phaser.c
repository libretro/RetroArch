/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2014 - Brad Miller
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

#define phaserlfoshape 4.0
#define phaserlfoskipsamples 20

#ifndef M_PI
#define M_PI		3.1415926535897932384626433832795
#endif

struct phaser_data
{
   float freq;
   float startphase;
   float fb;
   float depth;
   float drywet;
   float old[2][24];
   float gain;
   float fbout[2];
   float lfoskip;
   float phase;

   int stages;
   unsigned long skipcount;
};

static void phaser_free(void *data)
{
   free(data);
}

static void phaser_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i, c;
   int s;
   float m[2], tmp[2], *out;
   struct phaser_data *ph = (struct phaser_data*)data;

   output->samples = input->samples;
   output->frames  = input->frames;
   out             = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float in[2] = { out[0], out[1] };

      for (c = 0; c < 2; c++)
         m[c] = in[c] + ph->fbout[c] * ph->fb * 0.01f;

      if ((ph->skipcount++ % phaserlfoskipsamples) == 0)
      {
         ph->gain = 0.5 * (1.0 + cos(ph->skipcount * ph->lfoskip + ph->phase));
         ph->gain = (exp(ph->gain * phaserlfoshape) - 1.0) / (exp(phaserlfoshape) - 1);
         ph->gain = 1.0 - ph->gain * ph->depth;
      }

      for (s = 0; s < ph->stages; s++)
      {
         for (c = 0; c < 2; c++)
         {
            tmp[c] = ph->old[c][s];
            ph->old[c][s] = ph->gain * tmp[c] + m[c];
            m[c] = tmp[c] - ph->gain * ph->old[c][s];
         }
      }

      for (c = 0; c < 2; c++)
      {
         ph->fbout[c] = m[c];
         out[c] = m[c] * ph->drywet + in[c] * (1.0f - ph->drywet);
      }
   }
}

static void *phaser_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float lfo_freq, lfo_start_phase;
   struct phaser_data *ph = (struct phaser_data*)calloc(1, sizeof(*ph));
   if (!ph)
      return NULL;

   config->get_float(userdata, "lfo_freq", &lfo_freq, 0.4f);
   config->get_float(userdata, "lfo_start_phase", &lfo_start_phase, 0.0f);
   config->get_float(userdata, "feedback", &ph->fb, 0.0f);
   config->get_float(userdata, "depth", &ph->depth, 0.4f);
   config->get_float(userdata, "dry_wet", &ph->drywet, 0.5f);
   config->get_int(userdata, "stages", &ph->stages, 2);

   if (ph->stages < 1)
      ph->stages = 1;
   else if (ph->stages > 24)
      ph->stages = 24;

   ph->lfoskip = lfo_freq * 2.0 * M_PI / info->input_rate;
   ph->phase   = lfo_start_phase * M_PI / 180.0;

   return ph;
}

static const struct dspfilter_implementation phaser_plug = {
   phaser_init,
   phaser_process,
   phaser_free,

   DSPFILTER_API_VERSION,
   "Phaser",
   "phaser",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation phaser_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &phaser_plug;
}

#undef dspfilter_get_implementation

