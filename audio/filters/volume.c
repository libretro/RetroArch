/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#ifdef RARCH_INTERNAL
#define rarch_dsp_plugin_init    volume_dsp_plugin_init
#endif

struct volume_filter_data
{
#ifdef __GNUC__
   float buf[4096] __attribute__((aligned(16)));
#else
   float buf[4096];
#endif
   float m_vol;
   float m_pan_vol_l;
   float m_pan_vol_r;
};

#if 0
static void pan2gain(float &left, float &right, int val)
{
   left = (100 - val) / 100.0f;
   right = (val + 100) / 100.0f;
   if (left > 1.0)
      left = 1.0;
   if (right > 1.0)
      right = 1.0;
}

static float db2gain(float val)
{
   return powf(10.0, val / 20.0);
}
#endif

static void volume_process(void *data, const float *in, unsigned frames)
{
   float vol_left, vol_right;
   unsigned i;
   struct volume_filter_data *vol = (struct volume_filter_data*)data;

   if (!vol)
      return;

   vol_left  = vol->m_vol * vol->m_pan_vol_l;
   vol_right = vol->m_vol * vol->m_pan_vol_r;

   for (i = 0; i < frames; i++)
   {
      vol->buf[(i << 1) + 0] = in[(i << 1) + 0] * vol_left;
      vol->buf[(i << 1) + 1] = in[(i << 1) + 1] * vol_right;
   }
}

static void *volume_dsp_init(const rarch_dsp_info_t *info)
{
   struct volume_filter_data *vol = (struct volume_filter_data*)calloc(1, sizeof(*vol));
   (void)info;

   if (!vol)
      return NULL;

   vol->m_vol = 1.0;
   vol->m_pan_vol_l = 1.0;
   vol->m_pan_vol_r = 1.0;

   return vol;
}

static void volume_dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   struct volume_filter_data *vol = (struct volume_filter_data*)data;

   output->samples = vol->buf;
   volume_process(vol, input->samples, input->frames);
   output->frames = input->frames;
}

static void volume_dsp_free(void *data)
{
   struct volume_filter_data *vol = (struct volume_filter_data*)data;

   if (vol)
      free(vol);
}

static void volume_dsp_config(void *data)
{
   (void)data;
}

const struct dspfilter_implementation generic_volume_dsp = {
   volume_dsp_init,
   volume_dsp_process,
   volume_dsp_free,
   RARCH_DSP_API_VERSION,
   volume_dsp_config,
   "Volume",
   NULL
};

const struct dspfilter_implementation *rarch_dsp_plugin_init(void)
{
   return &generic_volume_dsp;
}

#ifdef RARCH_INTERNAL
#undef rarch_dsp_plugin_init
#endif
