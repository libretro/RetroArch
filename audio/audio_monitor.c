/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "audio_monitor.h"
#include "../general.h"

void audio_monitor_adjust_system_rates(void)
{
   float timing_skew;
   const struct retro_system_timing *info = 
      (const struct retro_system_timing*)&g_extern.system.av_info.timing;

   if (info->sample_rate <= 0.0)
      return;

   timing_skew                 = fabs(1.0f - info->fps / 
                                 g_settings.video.refresh_rate);
   g_extern.audio_data.in_rate = info->sample_rate;

   if (timing_skew <= g_settings.audio.max_timing_skew)
      g_extern.audio_data.in_rate *= (g_settings.video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n",
         g_extern.audio_data.in_rate);
}

/**
 * audio_monitor_set_refresh_rate:
 *
 * Sets audio monitor refresh rate to new value.
 **/
void audio_monitor_set_refresh_rate(void)
{
   double new_src_ratio = (double)g_settings.audio.out_rate / 
                           g_extern.audio_data.in_rate;

   g_extern.audio_data.orig_src_ratio = new_src_ratio;
   g_extern.audio_data.src_ratio      = new_src_ratio;
}
