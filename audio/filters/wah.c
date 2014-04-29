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

#ifndef LFOSKIPSAMPLES
#define LFOSKIPSAMPLES 30
#endif

#ifdef RARCH_INTERNAL
#define rarch_dsp_init    wah_dsp_init
#define rarch_dsp_process wah_dsp_process
#define rarch_dsp_free    wah_dsp_free
#define rarch_dsp_config  wah_dsp_config
#endif

struct wahwah_filter
{
   float phase;
   float lfoskip;
   unsigned long skipcount;
   float xn1, xn2, yn1, yn2;
   float b0, b1, b2, a0, a1, a2;
   float freq, startphase;
   float depth, freqofs, res;
};

struct wahwah_filter_data
{
   struct wahwah_filter wah_l;
   struct wahwah_filter wah_r;
   float buf[4096];
};

static void wahwah_init(void *data, int samplerate)
{
   struct wahwah_filter *wah = (struct wahwah_filter*)data;

   wah->lfoskip = wah->freq * 2 * M_PI / samplerate;
   wah->skipcount = 0;
   wah->xn1 = 0;
   wah->xn2 = 0;
   wah->yn1 = 0;
   wah->yn2 = 0;
   wah->b0 = 0;
   wah->b1 = 0;
   wah->b2 = 0;
   wah->a0 = 0;
   wah->a1 = 0;
   wah->a2 = 0;
   wah->phase = wah->startphase * M_PI / 180;
}

static float wahwah_process(void *data, float samp)
{
   float frequency, omega, sn, cs, alpha;
   float in, out;
   struct wahwah_filter *wah = (struct wahwah_filter*)data;

   in = samp;
   if ((wah->skipcount++) % LFOSKIPSAMPLES == 0)
   {
      frequency = (1 + cos(wah->skipcount * wah->lfoskip + wah->phase)) / 2;
      frequency = frequency * wah->depth * (1 - wah->freqofs) + wah->freqofs;
      frequency = exp((frequency - 1) * 6);
      omega = M_PI * frequency;
      sn = sin(omega);
      cs = cos(omega);
      alpha = sn / (2 * wah->res);
      wah->b0 = (1 - cs) / 2;
      wah->b1 = 1 - cs;
      wah->b2 = (1 - cs) / 2;
      wah->a0 = 1 + alpha;
      wah->a1 = -2 * cs;
      wah->a2 = 1 - alpha;
   }

   out = (wah->b0 * in + wah->b1 * wah->xn1 + wah->b2 * wah->xn2 - wah->a1 * wah->yn1 - wah->a2 * wah->yn2) / wah->a0;
   wah->xn2 = wah->xn1;
   wah->xn1 = in;
   wah->yn2 = wah->yn1;
   wah->yn1 = out;
   samp = out;
   return samp;
}

static void * rarch_dsp_init(const rarch_dsp_info_t *info)
{
   float freq = 1.5; 
   float startphase = 0.0;
   float res = 2.5;
   float depth = 0.70;
   float freqofs = 0.30;

   struct wahwah_filter_data *wah = (struct wahwah_filter_data*)calloc(1, sizeof(*wah));

   if (!wah)
      return NULL;

   wah->wah_l.depth = depth;
   wah->wah_l.freqofs = freqofs;
   wah->wah_l.freq = freq;
   wah->wah_l.startphase = startphase;
   wah->wah_l.res = res;
   wahwah_init(&wah->wah_l, info->input_rate);

   wah->wah_r.depth = depth;
   wah->wah_r.freqofs = freqofs;
   wah->wah_r.freq = freq;
   wah->wah_r.startphase = startphase;
   wah->wah_r.res = res;
   wahwah_init(&wah->wah_r, info->input_rate);

   return wah;
}

static void rarch_dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   int num_samples, i;
   struct wahwah_filter_data *wah = (struct wahwah_filter_data*)data;

	output->samples = wah->buf;
	num_samples = input->frames * 2;

	for (i = 0; i < num_samples;)
	{
		wah->buf[i] = wahwah_process(&wah->wah_l, input->samples[i]);
		i++;
		wah->buf[i] = wahwah_process(&wah->wah_r, input->samples[i]);
		i++;
	}
	output->frames = input->frames;
}

static void rarch_dsp_free(void *data)
{
   struct wahwah_filter_data *wah = (struct wahwah_filter_data*)data;

   if (wah)
      free(wah);
}

static void rarch_dsp_config(void *data)
{
}

const rarch_dsp_plugin_t dsp_plug = {
	rarch_dsp_init,
	rarch_dsp_process,
	rarch_dsp_free,
	RARCH_DSP_API_VERSION,
	rarch_dsp_config,
	"Wah",
   NULL
};

RARCH_API_EXPORT const rarch_dsp_plugin_t* RARCH_API_CALLTYPE rarch_dsp_plugin_init(void)
{
   return &dsp_plug;
}

#ifdef RARCH_INTERNAL
#undef rarch_dsp_init
#undef rarch_dsp_process
#undef rarch_dsp_free
#undef rarch_dsp_config
#endif
