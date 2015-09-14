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

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

#define CHORUS_MAX_DELAY 4096
#define CHORUS_DELAY_MASK (CHORUS_MAX_DELAY - 1)

struct chorus_data
{
   float old[2][CHORUS_MAX_DELAY];
   unsigned old_ptr;

   float delay;
   float depth;
   float input_rate;
   float mix_dry;
   float mix_wet;
   unsigned lfo_ptr;
   unsigned lfo_period;
};

static void chorus_free(void *data)
{
   free(data);
}

static void chorus_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct chorus_data *ch = (struct chorus_data*)data;

   output->samples = input->samples;
   output->frames  = input->frames;
   float *out = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      unsigned delay_int;
      float delay_frac, l_a, l_b, r_a, r_b;
      float chorus_l, chorus_r;
      float in[2] = { out[0], out[1] };
      float delay = ch->delay + ch->depth * sin((2.0 * M_PI * ch->lfo_ptr++) / ch->lfo_period);

      delay *= ch->input_rate;
      if (ch->lfo_ptr >= ch->lfo_period)
         ch->lfo_ptr = 0;

      delay_int = (unsigned)delay;
      if (delay_int >= CHORUS_MAX_DELAY - 1)
         delay_int = CHORUS_MAX_DELAY - 2;
      delay_frac = delay - delay_int;

      ch->old[0][ch->old_ptr] = in[0];
      ch->old[1][ch->old_ptr] = in[1];

      l_a = ch->old[0][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      l_b = ch->old[0][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];
      r_a = ch->old[1][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      r_b = ch->old[1][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];

      /* Lerp introduces aliasing of the chorus component, but doing full polyphase here is probably overkill. */
      chorus_l = l_a * (1.0f - delay_frac) + l_b * delay_frac;
      chorus_r = r_a * (1.0f - delay_frac) + r_b * delay_frac;

      out[0] = ch->mix_dry * in[0] + ch->mix_wet * chorus_l;
      out[1] = ch->mix_dry * in[1] + ch->mix_wet * chorus_r;

      ch->old_ptr = (ch->old_ptr + 1) & CHORUS_DELAY_MASK;
   }
}

static void *chorus_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float delay, depth, lfo_freq, drywet;
   struct chorus_data *ch = (struct chorus_data*)calloc(1, sizeof(*ch));
   if (!ch)
      return NULL;

   config->get_float(userdata, "delay_ms", &delay, 25.0f);
   config->get_float(userdata, "depth_ms", &depth, 1.0f);
   config->get_float(userdata, "lfo_freq", &lfo_freq, 0.5f);
   config->get_float(userdata, "drywet", &drywet, 0.8f);

   delay /= 1000.0f;
   depth /= 1000.0f;

   if (depth > delay)
      depth = delay;

   if (drywet < 0.0f)
      drywet = 0.0f;
   else if (drywet > 1.0f)
      drywet = 1.0f;

   ch->mix_dry = 1.0f - 0.5f * drywet;
   ch->mix_wet = 0.5f * drywet;

   ch->delay = delay;
   ch->depth = depth;
   ch->lfo_period = (1.0f / lfo_freq) * info->input_rate;
   ch->input_rate = info->input_rate;
   if (!ch->lfo_period)
      ch->lfo_period = 1;
   return ch;
}

static const struct dspfilter_implementation chorus_plug = {
   chorus_init,
   chorus_process,
   chorus_free,

   DSPFILTER_API_VERSION,
   "Chorus",
   "chorus",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation chorus_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &chorus_plug;
}

#undef dspfilter_get_implementation

