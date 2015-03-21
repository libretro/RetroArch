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
   global_t *global = global_get_ptr();
   const struct retro_system_timing *info = 
      (const struct retro_system_timing*)&global->system.av_info.timing;
   settings_t *settings = config_get_ptr();

   if (info->sample_rate <= 0.0)
      return;

   timing_skew                 = fabs(1.0f - info->fps / 
                                 settings->video.refresh_rate);
   global->audio_data.in_rate = info->sample_rate;

   if (timing_skew <= settings->audio.max_timing_skew)
      global->audio_data.in_rate *= (settings->video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n",
         global->audio_data.in_rate);
}

/**
 * audio_monitor_set_refresh_rate:
 *
 * Sets audio monitor refresh rate to new value.
 **/
void audio_monitor_set_refresh_rate(void)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   double new_src_ratio = (double)settings->audio.out_rate / 
                           global->audio_data.in_rate;

   global->audio_data.orig_src_ratio = new_src_ratio;
   global->audio_data.src_ratio      = new_src_ratio;
}
