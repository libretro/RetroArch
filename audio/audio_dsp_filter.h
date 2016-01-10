/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __AUDIO_DSP_FILTER_H__
#define __AUDIO_DSP_FILTER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rarch_dsp_filter rarch_dsp_filter_t;

rarch_dsp_filter_t *rarch_dsp_filter_new(const char *filter_config,
      float sample_rate);

void rarch_dsp_filter_free(rarch_dsp_filter_t *dsp);

struct rarch_dsp_data
{
   float *input;
   unsigned input_frames;

   /* Set by rarch_dsp_filter_process(). */
   float *output;
   unsigned output_frames;
};

void rarch_dsp_filter_process(rarch_dsp_filter_t *dsp,
      struct rarch_dsp_data *data);

#ifdef __cplusplus
}
#endif

#endif

