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

#include <stdio.h>
#include "../boolean.h"
#include "../file.h"

#include "rarch_console_libretro_mgmt.h"

#ifndef IS_SALAMANDER

static void rarch_console_name_from_id(char *name, size_t size)
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

bool rarch_configure_libretro_core(const char *full_path, const char *tmp_path,
 const char *libretro_path, const char *config_path, const char *extension)
{
   bool ret = false;
   bool find_libretro_file = false;
   char libretro_core_installed[1024];

   g_extern.verbose = true;

   //install and rename libretro core first if 'CORE' executable exists
   if (path_file_exists(full_path))
   {
      size_t sizeof_libretro_core = sizeof(libretro_core_installed);
      char tmp_path2[1024], tmp_pathnewfile[1024];

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
      ret = rename(full_path, tmp_pathnewfile);

      if (ret == 0)
      {
         RARCH_LOG("libretro core [%s] renamed to: [%s].\n", full_path, tmp_pathnewfile);
	 strlcpy(libretro_core_installed, tmp_pathnewfile, sizeof_libretro_core);
	 ret = 1;
      }
      else
      {
         RARCH_ERR("Failed to rename CORE executable.\n");
	 RARCH_WARN("CORE executable was not found, or some other errors occurred. Will attempt to load libretro core path from config file.\n");
	 ret = 0;
      }
   }

   g_extern.verbose = false;

   //if we have just installed a libretro core, set libretro path in settings to newly installed libretro core

   if(ret)
      strlcpy(g_settings.libretro, libretro_core_installed, sizeof(g_settings.libretro));
   else
      find_libretro_file = true;

   return find_libretro_file;
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
#ifdef _XBOX
      char fname_tmp[PATH_MAX];
      fill_pathname_base(fname_tmp, first_exe, sizeof(fname_tmp));

      if(strcmp(fname_tmp, "RetroArch-Salamander.xex") == 0)
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
#else
      strlcpy(first_file, first_exe, size_of_first_file);
#endif
      RARCH_LOG("Set first entry in libretro core dir to libretro path: [%s].\n", first_file);
   }

end:
   dir_list_free(dir_list);
}
