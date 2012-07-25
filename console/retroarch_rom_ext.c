/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include <ctype.h>

#ifdef _WIN32
#include "../compat/posix_string.h"
#endif

#include "../general.h"

#include "console_settings.h"

#include "retroarch_rom_ext.h"

void rarch_console_load_game(const char *path)
{
   snprintf(g_console.rom_path, sizeof(g_console.rom_path), path);
   rarch_settings_change(S_START_RARCH);
}

const char *rarch_console_get_rom_ext(void)
{
   const char *retval = NULL;

   struct retro_system_info info;
#ifdef ANDROID
   pretro_get_system_info(&info);
#else
   retro_get_system_info(&info);
#endif

   if (info.valid_extensions)
      retval = info.valid_extensions;
   else
      retval = "ZIP|zip";

   return retval;
}

void rarch_console_name_from_id(char *name, size_t size)
{
   if (size == 0)
      return;

   struct retro_system_info info;
#ifdef ANDROID
   pretro_get_system_info(&info);
#else
   retro_get_system_info(&info);
#endif
   const char *id = info.library_name ? info.library_name : "Unknown";

   if (!id || strlen(id) >= size)
   {
      name[0] = '\0';
      return;
   }

   name[strlen(id)] = '\0';

   for (size_t i = 0; id[i] != '\0'; i++)
   {
      char c = id[i];
      if (isspace(c) || isblank(c))
         name[i] = '_';
      else
         name[i] = tolower(c);
   }
}
