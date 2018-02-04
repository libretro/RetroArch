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

#include <stddef.h>
#include <string.h>

#include <compat/strl.h>

#include <file/file_path.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>

#include "../configuration.h"
#include "../verbosity.h"

/*We need to set libretro to the first entry in the cores
 * directory so that it will be saved to the config file
 */
bool find_libretro_core(char *fullpath,
   size_t sizeof_fullpath, char *needle, const char * ext)
{
   size_t i;
   settings_t *settings     = config_get_ptr();
   const char          *dir = settings->paths.directory_libretro;
   struct string_list *list = dir_list_new(dir, ext, false, true, false, false);

   if (!list)
   {
      RARCH_ERR("Couldn't read directory."
            " Cannot infer default libretro core.\n");
      return false;
   }

   RARCH_LOG("Searching for valid libretro implementation in: \"%s\".\n",
         dir);

   for (i = 0; i < list->size; i++)
   {
      char fname[PATH_MAX_LENGTH]           = {0};
      const char *libretro_elem             = (const char*)list->elems[i].data;

      RARCH_LOG("Checking library: \"%s\".\n", libretro_elem);

      if (!libretro_elem)
         continue;

      fill_pathname_base(fname, libretro_elem, sizeof(fname));

      if (!strstr(fname, needle))
         continue;

      strlcpy(fullpath, libretro_elem, sizeof_fullpath);
      break;
   }

   dir_list_free(list);

   return true;
}
