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

#define NUMCOMBS 8
#define NUMALLPASSES 4
#define MUTED 0
#define FIXEDGAIN 0.015f
#define SCALEWET 3
#define SCALEDRY 2
#define SCALEDAMP	0.4f
#define SCALEROOM 0.28f
#define OFFSETROOM 0.7f
#define INITIALROOM 0.5f
#define INITIALDAMP 0.5f
#define INITIALWET (1 / SCALEWET)
#define INITIALDRY 0
#define INITIALWIDTH 1
#define INITIALMODE 0
#define FREEZEMODE 0.5f

#define COMBTUNINGL1		1116
#define COMBTUNINGL2		1188
#define COMBTUNINGL3		1277
#define COMBTUNINGL4		1356
#define COMBTUNINGL5		1422
#define COMBTUNINGL6		1491
#define COMBTUNINGL7		1557
#define COMBTUNINGL8		1617
#define ALLPASSTUNINGL1 556
#define ALLPASSTUNINGL2 441
#define ALLPASSTUNINGL3	341
#define ALLPASSTUNINGL4	225

struct comb
{
	float feedback;
	float filterstore;
	float damp1;
	float damp2;
	float *buffer;
	int bufsize;
	int bufidx;
};

struct allpass
{
   float feedback;
   float *buffer;
   int bufsize;
   int bufidx;
};

struct revmodel
{
   float gain;
   float roomsize, roomsize1;
   float damp, damp1;
   float wet, wet1, wet2;
   float dry;
   float width;
   float mode;

   struct comb combL[NUMCOMBS];

   struct allpass	allpassL[NUMALLPASSES];

   float bufcombL1[COMBTUNINGL1];
   float bufcombL2[COMBTUNINGL2];
   float bufcombL3[COMBTUNINGL3];
   float bufcombL4[COMBTUNINGL4];
   float bufcombL5[COMBTUNINGL5];
   float bufcombL6[COMBTUNINGL6];
   float bufcombL7[COMBTUNINGL7];
   float bufcombL8[COMBTUNINGL8];

   float bufallpassL1[ALLPASSTUNINGL1];
   float bufallpassL2[ALLPASSTUNINGL2];
   float bufallpassL3[ALLPASSTUNINGL3];
   float bufallpassL4[ALLPASSTUNINGL4];
};

// FIXME: Fix this really ugly hack
static inline float undenormalise(void *sample)
{
   if (((*(unsigned int*)sample) &  0x7f800000) == 0)
      return 0.0f;
   return *(float*)sample;
}

static inline float comb_process(void *data, float input)
{
   struct comb *comb = (struct comb*)data; 
   float output;

   output = comb->buffer[comb->bufidx];
   undenormalise(&output);

   comb->filterstore = (output * comb->damp2) + (comb->filterstore * comb->damp1);
   undenormalise(&comb->filterstore);

   comb->buffer[comb->bufidx] = input + (comb->filterstore * comb->feedback);

   if (++comb->bufidx >= comb->bufsize)
      comb->bufidx = 0;

   return output;
}


static inline float allpass_process(void *data, float input)
{
   struct allpass *allpass = (struct allpass*)data;
   float output, bufout;

   bufout = allpass->buffer[allpass->bufidx];
   undenormalise(&bufout);

   output = -input + bufout;
   allpass->buffer[allpass->bufidx] = input + (bufout * allpass->feedback);

   if (++allpass->bufidx >= allpass->bufsize)
      allpass->bufidx = 0;

   return output;
}

static float revmodel_getmode(float mode)
{
   if (mode >= FREEZEMODE)
      return 1;
   else
      return 0;
}

static void revmodel_update(void *data)
{
   int i;
   struct revmodel *rev = (struct revmodel*)data;

   rev->wet1 = rev->wet * (rev->width / 2 + 0.5f);

   if (rev->mode >= FREEZEMODE)
   {
      rev->roomsize1 = 1;
      rev->damp1 = 0;
      rev->gain = MUTED;
   }
   else
   {
      rev->roomsize1 = rev->roomsize;
      rev->damp1 = rev->damp;
      rev->gain = FIXEDGAIN;
   }

   for (i = 0; i < NUMCOMBS; i++)
      rev->combL[i].feedback = rev->roomsize1;

   for (i = 0; i < NUMCOMBS; i++)
   {
      rev->combL[i].damp1 = rev->damp1;
      rev->combL[i].damp2 = 1 - rev->damp1;
   }
}

static void revmodel_set(void *data, float drytime,
      float wettime, float damping, float roomwidth, float roomsize)
{
   int i, j;
   struct revmodel *rev = (struct revmodel*)data;

	rev->wet = wettime;
	revmodel_update(rev);

   rev->roomsize = roomsize;
	revmodel_update(rev);

	rev->dry = drytime;

   rev->damp = damping;
	revmodel_update(rev);

	rev->width = roomwidth;
	revmodel_update(rev);

	rev->mode = INITIALMODE;
	revmodel_update(rev);

   if (revmodel_getmode(rev->mode) >= FREEZEMODE)
      return;

   for (i = 0; i < NUMCOMBS; i++)
   {
      for (j = 0; j < rev->combL[i].bufsize; j++)
         rev->combL[i].buffer[j] = 0;
   }

   for (i = 0; i < NUMALLPASSES; i++)
   {
      for (j = 0; j < rev->allpassL[i].bufsize; j++)
         rev->allpassL[i].buffer[j] = 0;
   }
}

static void revmodel_init(void *data)
{
   struct revmodel *rev = (struct revmodel*)data;

	rev->combL[0].filterstore = 0;
	rev->combL[0].bufidx      = 0;
   rev->combL[0].buffer      = (float*)rev->bufcombL1;
   rev->combL[0].bufsize     = COMBTUNINGL1;
	rev->combL[1].filterstore = 0;
	rev->combL[1].bufidx      = 0;
   rev->combL[1].buffer      = (float*)rev->bufcombL2;
   rev->combL[1].bufsize     = COMBTUNINGL2;
	rev->combL[2].filterstore = 0;
	rev->combL[2].bufidx      = 0;
   rev->combL[2].buffer      = (float*)rev->bufcombL3;
   rev->combL[2].bufsize     = COMBTUNINGL3;
	rev->combL[3].filterstore = 0;
	rev->combL[3].bufidx      = 0;
   rev->combL[3].buffer      = (float*)rev->bufcombL4;
   rev->combL[3].bufsize     = COMBTUNINGL4;
	rev->combL[4].filterstore = 0;
	rev->combL[4].bufidx      = 0;
   rev->combL[4].buffer      = (float*)rev->bufcombL5;
   rev->combL[4].bufsize     = COMBTUNINGL5;
	rev->combL[5].filterstore = 0;
	rev->combL[5].bufidx      = 0;
   rev->combL[5].buffer      = (float*)rev->bufcombL6;
   rev->combL[5].bufsize     = COMBTUNINGL6;
	rev->combL[6].filterstore = 0;
	rev->combL[6].bufidx      = 0;
   rev->combL[6].buffer      = (float*)rev->bufcombL7;
   rev->combL[6].bufsize     = COMBTUNINGL7;
	rev->combL[7].filterstore = 0;
	rev->combL[7].bufidx      = 0;
   rev->combL[7].buffer      = (float*)rev->bufcombL8;
   rev->combL[7].bufsize     = COMBTUNINGL8;

   rev->allpassL[0].bufidx   = 0;
   rev->allpassL[0].buffer   = (float*)rev->bufallpassL1;
   rev->allpassL[0].bufsize  = ALLPASSTUNINGL1;
	rev->allpassL[0].feedback = 0.5f;
   rev->allpassL[1].bufidx   = 0;
   rev->allpassL[1].buffer   = (float*)rev->bufallpassL2;
   rev->allpassL[1].bufsize  = ALLPASSTUNINGL2;
	rev->allpassL[1].feedback = 0.5f;
   rev->allpassL[2].bufidx   = 0;
   rev->allpassL[2].buffer   = (float*)rev->bufallpassL3;
   rev->allpassL[2].bufsize  = ALLPASSTUNINGL3;
	rev->allpassL[2].feedback = 0.5f;
   rev->allpassL[3].bufidx   = 0;
   rev->allpassL[3].buffer   = (float*)rev->bufallpassL4;
   rev->allpassL[3].bufsize  = ALLPASSTUNINGL4;
	rev->allpassL[3].feedback = 0.5f;

}

static float revmodel_process(void *data, float in)
{
   float samp, mono_out, mono_in, input;
   int i;
   struct revmodel *rev = (struct revmodel*)data;

	samp = in;
	mono_out = 0.0f;
	mono_in = samp;
	input = (mono_in) * rev->gain;

	for(i=0; i < NUMCOMBS; i++)
		mono_out += comb_process(&rev->combL[i], input);
	for(i = 0; i < NUMALLPASSES; i++)
		mono_out = allpass_process(&rev->allpassL[i], mono_out);
	samp = mono_in * rev->dry + mono_out * rev->wet1;
	return samp;
}


#define REVMODEL_GETWET(revmodel) (revmodel->wet / SCALEWET)
#define REVMODEL_GETROOMSIZE(revmodel) ((revmodel->roomsize - OFFSETROOM) / SCALEROOM)
#define REVMODEL_GETDRY(revmodel) (revmodel->dry / SCALEDRY)
#define REVMODEL_GETWIDTH(revmodel) (revmodel->width)



struct reverb_filter_data
{
   struct revmodel rev_l;
   struct revmodel rev_r;
   float buf[4096];
};


static void * rarch_dsp_init(const rarch_dsp_info_t *info)
{
   float drytime, wettime, damping, roomwidth, roomsize;
   (void)info;

   drytime = 0.43; 
   wettime = 0.57;
   damping = 0.45;
   roomwidth = 0.56;
   roomsize = 0.56;

   struct reverb_filter_data *reverb = (struct reverb_filter_data*)calloc(1, sizeof(*reverb));

   if (!reverb)
      return NULL;

   revmodel_init(&reverb->rev_l);
   revmodel_set(&reverb->rev_l, INITIALDRY,
         INITIALWET * SCALEWET, INITIALDAMP * SCALEDAMP, INITIALWIDTH, (INITIALROOM * SCALEROOM) + OFFSETROOM);
   revmodel_set(&reverb->rev_l, drytime, wettime, damping, roomwidth, roomsize);

   revmodel_init(&reverb->rev_r);
   revmodel_set(&reverb->rev_r, INITIALDRY,
         INITIALWET * SCALEWET, INITIALDAMP * SCALEDAMP, INITIALWIDTH, (INITIALROOM * SCALEROOM) + OFFSETROOM);
   revmodel_set(&reverb->rev_r, drytime, wettime, damping, roomwidth, roomsize);

   return reverb;
}

static void rarch_dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   int i, num_samples;
   struct reverb_filter_data *reverb = (struct reverb_filter_data*)data;

	output->samples = reverb->buf;
	num_samples = input->frames * 2;
	for (i = 0; i < num_samples;)
	{
		reverb->buf[i] = revmodel_process(&reverb->rev_l, input->samples[i]);
		i++;
		reverb->buf[i] = revmodel_process(&reverb->rev_r, input->samples[i]);
		i++;
	}
	output->frames = input->frames;
}

static void rarch_dsp_free(void *data)
{
   struct reverb_filter_data *rev = (struct reverb_filter_data*)data;

   if (rev)
      free(rev);
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
	"Reverberatation"
};

RARCH_API_EXPORT const rarch_dsp_plugin_t* RARCH_API_CALLTYPE rarch_dsp_plugin_init(void)
{
   return &dsp_plug;
}
