/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdint.h>

#include "rarch_console_rsound.h"

bool rarch_console_rsound_start(const char *ip)
{
   strlcpy(g_settings.audio.driver, "rsound", sizeof(g_settings.audio.driver));
   strlcpy(g_settings.audio.device, ip, sizeof(g_settings.audio.device));
   driver.audio_data = NULL;

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
   return g_extern.audio_active;
}

void rarch_console_rsound_stop(void)
{
   strlcpy(g_settings.audio.driver, config_get_default_audio(), sizeof(g_settings.audio.driver));

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
}
