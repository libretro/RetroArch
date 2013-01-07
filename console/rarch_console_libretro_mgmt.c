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

#include <stdio.h>
#include "../boolean.h"
#include "../file.h"
#include "rarch_console.h"

#include "rarch_console_libretro_mgmt.h"

#ifndef IS_SALAMANDER

void rarch_console_name_from_id(char *name, size_t size)
{
   if (size == 0)
      return;

   struct retro_system_info info;
   retro_get_system_info(&info);
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

// if a CORE executable exists (full_path), this means we have just installed
// a new libretro port and therefore we need to change it to a more
// sane name.

bool rarch_libretro_core_install(const char *core_exe_path, const char *tmp_path,
 const char *libretro_path, const char *config_path, const char *extension)
{
   int ret = 0;
   char tmp_path2[PATH_MAX], tmp_pathnewfile[PATH_MAX];

   rarch_console_name_from_id(tmp_path2, sizeof(tmp_path2));
   strlcat(tmp_path2, extension, sizeof(tmp_path2));
   snprintf(tmp_pathnewfile, sizeof(tmp_pathnewfile), "%s%s", tmp_path, tmp_path2);

   if (path_file_exists(tmp_pathnewfile))
   {
      // if libretro core already exists, this means we are
      // upgrading the libretro core - so delete pre-existing
      // file first.

      RARCH_LOG("Upgrading emulator core...\n");
      ret = remove(tmp_pathnewfile);

      if (ret == 0)
         RARCH_LOG("Succeeded in removing pre-existing libretro core: [%s].\n", tmp_pathnewfile);
      else
         RARCH_ERR("Failed to remove pre-existing libretro core: [%s].\n", tmp_pathnewfile);
   }

   //now attempt the renaming.
   ret = rename(core_exe_path, tmp_pathnewfile);

   if (ret == 0)
   {
      RARCH_LOG("Libretro core [%s] successfully renamed to: [%s].\n", core_exe_path, tmp_pathnewfile);
      snprintf(g_settings.libretro, sizeof(g_settings.libretro), tmp_pathnewfile);
   }
   else
   {
      RARCH_ERR("Failed to rename CORE executable.\n");
      RARCH_WARN("CORE executable was not found, or some other error occurred. Will attempt to load libretro core path from config file.\n");
      return false;
   }

   return true;
}

bool rarch_manage_libretro_extension_supported(const char *filename)
{
   bool ext_supported = false;
   struct string_list *ext_list = NULL;
   const char *file_ext = path_get_extension(filename);
   const char *ext = rarch_console_get_rom_ext();

   if (ext)
      ext_list = string_split(ext, "|");

   if (ext_list && string_list_find_elem(ext_list, file_ext))
      ext_supported = true; 

   return ext_supported;
}

#endif

void rarch_manage_libretro_set_first_file(char *first_file, size_t size_of_first_file, const char *libretro_path, const char * exe_ext)
{
   struct string_list *dir_list = dir_list_new(libretro_path, exe_ext, false);

   const char * first_exe;

   if (!dir_list)
   {
      RARCH_ERR("Couldn't read directory.\n");
      RARCH_ERR("Failed to set first entry to libretro path.\n");
      goto end;
   }

   first_exe = dir_list->elems[0].data;

   if(first_exe)
   {
      char fname_tmp[PATH_MAX];
      fill_pathname_base(fname_tmp, first_exe, sizeof(fname_tmp));

      if(strncmp(fname_tmp, default_paths.salamander_file, sizeof(fname_tmp)) == 0)
      {
         RARCH_WARN("First entry is RetroArch Salamander itself, increment entry by one and check if it exists.\n");
         first_exe = dir_list->elems[1].data;
         fill_pathname_base(fname_tmp, first_exe, sizeof(fname_tmp));

         if(!first_exe)
         {
            RARCH_ERR("Unlikely error happened - no second entry - no choice but to set it to RetroArch Salamander\n");
            first_exe = dir_list->elems[0].data;
            fill_pathname_base(fname_tmp, first_exe, sizeof(fname_tmp));
         }
      }

      strlcpy(first_file, fname_tmp, size_of_first_file);
      RARCH_LOG("Set first entry in libretro core dir to libretro path: [%s].\n", first_file);
   }

end:
   dir_list_free(dir_list);
}
