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

#include <retro_miscellaneous.h>

#define WAHWAH_LFO_SKIP_SAMPLES 30

struct wahwah_data
{
   float phase;
   float lfoskip;
   float b0, b1, b2, a0, a1, a2;
   float freq, startphase;
   float depth, freqofs, res;
   unsigned long skipcount;

   struct
   {
      float xn1, xn2, yn1, yn2;
   } l, r;
};

static void wahwah_free(void *data)
{
   if (data)
      free(data);
}

static void wahwah_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   float *out;
   struct wahwah_data *wah = (struct wahwah_data*)data;

   output->samples         = input->samples;
   output->frames          = input->frames;
   out                     = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float out_l, out_r;
      float in[2] = { out[0], out[1] };

      if ((wah->skipcount++ % WAHWAH_LFO_SKIP_SAMPLES) == 0)
      {
         float omega, sn, cs, alpha;
         float frequency = (1.0 + cos(wah->skipcount * wah->lfoskip + wah->phase)) / 2.0;

         frequency = frequency * wah->depth * (1.0 - wah->freqofs) + wah->freqofs;
         frequency = exp((frequency - 1.0) * 6.0);

         omega     = M_PI * frequency;
         sn        = sin(omega);
         cs        = cos(omega);
         alpha     = sn / (2.0 * wah->res);

         wah->b0   = (1.0 - cs) / 2.0;
         wah->b1   = 1.0 - cs;
         wah->b2   = (1.0 - cs) / 2.0;
         wah->a0   = 1.0 + alpha;
         wah->a1   = -2.0 * cs;
         wah->a2   = 1.0 - alpha;
      }

      out_l      = (wah->b0 * in[0] + wah->b1 * wah->l.xn1 + wah->b2 * wah->l.xn2 - wah->a1 * wah->l.yn1 - wah->a2 * wah->l.yn2) / wah->a0;
      out_r      = (wah->b0 * in[1] + wah->b1 * wah->r.xn1 + wah->b2 * wah->r.xn2 - wah->a1 * wah->r.yn1 - wah->a2 * wah->r.yn2) / wah->a0;

      wah->l.xn2 = wah->l.xn1;
      wah->l.xn1 = in[0];
      wah->l.yn2 = wah->l.yn1;
      wah->l.yn1 = out_l;

      wah->r.xn2 = wah->r.xn1;
      wah->r.xn1 = in[1];
      wah->r.yn2 = wah->r.yn1;
      wah->r.yn1 = out_r;

      out[0]     = out_l;
      out[1]     = out_r;
   }
}

static void *wahwah_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   struct wahwah_data *wah = (struct wahwah_data*)calloc(1, sizeof(*wah));
   if (!wah)
      return NULL;

   config->get_float(userdata, "lfo_freq", &wah->freq, 1.5f);
   config->get_float(userdata, "lfo_start_phase", &wah->startphase, 0.0f);
   config->get_float(userdata, "freq_offset", &wah->freqofs, 0.3f);
   config->get_float(userdata, "depth", &wah->depth, 0.7f);
   config->get_float(userdata, "resonance", &wah->res, 2.5f);

   wah->lfoskip = wah->freq * 2.0 * M_PI / info->input_rate;
   wah->phase = wah->startphase * M_PI / 180.0;

   return wah;
}

static const struct dspfilter_implementation wahwah_plug = {
   wahwah_init,
   wahwah_process,
   wahwah_free,

   DSPFILTER_API_VERSION,
   "Wah-Wah",
   "wahwah",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation wahwah_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &wahwah_plug;
}

#undef dspfilter_get_implementation

