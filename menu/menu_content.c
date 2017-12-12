/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <compat/strl.h>
#include <retro_assert.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "menu_content.h"
#include "menu_driver.h"
#include "menu_shader.h"

#include "../core_info.h"
#include "../configuration.h"
#include "../defaults.h"
#include "../playlist.h"
#include "../verbosity.h"

bool menu_content_playlist_find_associated_core(const char *path, char *s, size_t len)
{
   unsigned j;
   bool                                ret = false;
   settings_t *settings                    = config_get_ptr();
   struct string_list *existing_core_names =
      string_split(settings->arrays.playlist_names, ";");
   struct string_list *existing_core_paths =
      string_split(settings->arrays.playlist_cores, ";");

   for (j = 0; j < existing_core_names->size; j++)
   {
      if (!string_is_equal(path, existing_core_names->elems[j].data))
         continue;

      if (existing_core_paths)
      {
         const char *existing_core = existing_core_paths->elems[j].data;

         if (existing_core)
         {
            strlcpy(s, existing_core, len);
            ret = true;
         }
      }

      break;
   }

   string_list_free(existing_core_names);
   string_list_free(existing_core_paths);
   return ret;
}
