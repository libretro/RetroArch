/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
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

#ifndef _SSNES_FUNC_HOOKS_H
#define _SSNES_FUNC_HOOKS_H

/*============================================================
	PLAYSTATION3
============================================================ */

#ifdef __CELLOS_LV2__

#define HAVE_GRIFFIN_OVERRIDE_AUDIO_FLUSH_FUNC 1

static bool audio_flush(const int16_t *data, size_t samples)
{
   const float *output_data = NULL;
   unsigned output_frames = 0;

   audio_convert_s16_to_float(g_extern.audio_data.data, data, samples);

   struct resampler_data src_data = {0};
   src_data.data_in = g_extern.audio_data.data;
   src_data.data_out = g_extern.audio_data.outsamples;
   src_data.input_frames = (samples / 2);

   src_data.ratio = g_extern.audio_data.src_ratio;
   if (g_extern.is_slowmotion)
      src_data.ratio *= g_settings.slowmotion_ratio;

   resampler_process(g_extern.audio_data.source, &src_data);

   output_data = g_extern.audio_data.outsamples;
   output_frames = src_data.output_frames;

   if (audio_write_func(output_data, output_frames * sizeof(float) * 2) < 0)
	   return false;

   return true;
}

#endif


#endif
