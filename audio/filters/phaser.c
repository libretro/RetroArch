/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Brad Miller
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
 *
 */

#include "rarch_dsp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI		3.1415926535897932384626433832795
#endif

#define PHASERLFOSHAPE 4.0
#define PHASER_LFOSKIPSAMPLES 20

struct phaser_filter
{
   float freq;
   float startphase;
   float fb;
   int depth;
   int stages;
   int drywet;
   unsigned long skipcount;
   float old[24];
   float gain;
   float fbout;
   float lfoskip;
   float phase;
};

struct phaser_filter_data
{
   struct phaser_filter phase_l;
   struct phaser_filter phase_r;
   float buf[4096];
};

static void phaser_init(void *data, int samplerate)
{
   int j;
   struct phaser_filter *phaser = (struct phaser_filter*)data;

	phaser->skipcount = 0;
	phaser->gain = 0.0;
	phaser->fbout = 0.0;
	phaser->lfoskip = phaser->freq * 2 * M_PI / samplerate;
	phaser->phase = phaser->startphase * M_PI / 180;
	for (j = 0; j < phaser->stages; j++)
		phaser->old[j] = 0;
}

static float phaser_process(void *data, float in)
{
   float m, tmp, out;
   int j;
   struct phaser_filter *phaser = (struct phaser_filter*)data;

   m = in + phaser->fbout * phaser->fb / 100;

   if (((phaser->skipcount++) % PHASER_LFOSKIPSAMPLES) == 0)
   {
      phaser->gain = (1 + cos(phaser->skipcount * phaser->lfoskip + phaser->phase)) / 2;
      phaser->gain =(exp(phaser->gain * PHASERLFOSHAPE) - 1) / (exp(PHASERLFOSHAPE)-1);
      phaser->gain = 1 - phaser->gain / 255 * phaser->depth;  
   }
   for (j = 0; j < phaser->stages; j++)
   {
      tmp = phaser->old[j];
      phaser->old[j] = phaser->gain * tmp + m;
      m = tmp - phaser->gain * phaser->old[j];
   }
   phaser->fbout = m;
   out = (m * phaser->drywet + in * (255 - phaser->drywet)) / 255;
   if (out < -1.0) out = -1.0;
   if (out > 1.0) out = 1.0;
   return out;
}

static void * rarch_dsp_init(const rarch_dsp_info_t *info)
{
   float freq, startphase, fb;
   int depth, stages, drywet;
   struct phaser_filter_data *phaser;

   freq = 0.4; 
   startphase = 0;
   fb = 0;
   depth = 100;
   stages = 2;
   drywet = 128;

   phaser = (struct phaser_filter_data*)calloc(1, sizeof(*phaser));

   if (!phaser)
      return NULL;

   phaser->phase_l.freq = freq;
   phaser->phase_l.startphase = startphase;
   phaser->phase_l.fb = fb;
   phaser->phase_l.depth = depth;
   phaser->phase_l.stages = stages;
   phaser->phase_l.drywet = drywet;
   phaser_init(&phaser->phase_l, info->input_rate);

   phaser->phase_r.freq = freq;
   phaser->phase_r.startphase = startphase;
   phaser->phase_r.fb = fb;
   phaser->phase_r.depth = depth;
   phaser->phase_r.stages = stages;
   phaser->phase_r.drywet = drywet;
   phaser_init(&phaser->phase_r, info->input_rate);

   return phaser;
}

static void rarch_dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   int i, num_samples;
   struct phaser_filter_data *phaser = (struct phaser_filter_data*)data;

   output->samples = phaser->buf;
   num_samples = input->frames * 2;
   for (i = 0; i<num_samples;)
   {
		phaser->buf[i] = phaser_process(&phaser->phase_l, input->samples[i]);
		i++;
		phaser->buf[i] = phaser_process(&phaser->phase_r, input->samples[i]);
		i++;
	}
	output->frames = input->frames;
}

static void rarch_dsp_free(void *data)
{
   struct phaser_filter_data *phaser = (struct phaser_filter_data*)data;

   if (phaser)
   {
      int j;
      for (j = 0; j < phaser->phase_l.stages; j++)
         phaser->phase_l.old[j] = 0;
      for (j = 0; j < phaser->phase_r.stages; j++)
         phaser->phase_r.old[j] = 0;
      free(phaser);
   }
}

static void rarch_dsp_config(void *data)
{
   (void)data;
}

const rarch_dsp_plugin_t dsp_plug = {
	rarch_dsp_init,
	rarch_dsp_process,
	rarch_dsp_free,
	RARCH_DSP_API_VERSION,
	rarch_dsp_config,
	"Phaser plugin"
};

RARCH_API_EXPORT const rarch_dsp_plugin_t* RARCH_API_CALLTYPE rarch_dsp_plugin_init(void)
{
   return &dsp_plug;
}
