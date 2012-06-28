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

#include "retroarch_console.h"

static void rarch_manage_libretro_install(char *libretro_core_installed, size_t sizeof_libretro_core, const char *full_path, const char *path, const char *exe_ext)
{
   int ret;
   char tmp_path2[1024], tmp_pathnewfile[1024];

   RARCH_LOG("Assumed path of CORE executable: [%s]\n", full_path);

   if (path_file_exists(full_path))
   {
      // if CORE executable exists, this means we have just installed
      // a new libretro port and therefore we need to change it to a more
      // sane name.

      rarch_console_name_from_id(tmp_path2, sizeof(tmp_path2));
      strlcat(tmp_path2, exe_ext, sizeof(tmp_path2));
      snprintf(tmp_pathnewfile, sizeof(tmp_pathnewfile), "%s%s", path, tmp_path2);

      if (path_file_exists(tmp_pathnewfile))
      {
         // if libretro core already exists, this means we are
         // upgrading the libretro core - so delete pre-existing
         // file first.

         RARCH_LOG("Upgrading emulator core...\n");

         ret = remove(tmp_pathnewfile);

         if (ret == 0)
         {
            RARCH_LOG("Succeeded in removing pre-existing libretro core: [%s].\n", tmp_pathnewfile);
         }
         else
            RARCH_ERR("Failed to remove pre-existing libretro core: [%s].\n", tmp_pathnewfile);
      }

      //now attempt the renaming.
      ret = rename(full_path, tmp_pathnewfile);

      if (ret == 0)
      {
         RARCH_LOG("Libsnes core [%s] renamed to: [%s].\n", full_path, tmp_pathnewfile);
         strlcpy(libretro_core_installed, tmp_pathnewfile, sizeof_libretro_core);
      }
      else
      {
         RARCH_ERR("Failed to rename CORE executable.\n");
	 RARCH_WARN("CORE executable was not found, or some other errors occurred. Will attempt to load libretro core path from config file.\n");
      }
   }
}

bool rarch_configure_libretro_core(const char *full_path, const char *tmp_path,
 const char *libretro_path, const char *config_path, const char *extension)
{
   char libretro_core_installed[1024];
   g_extern.verbose = true;

   rarch_manage_libretro_install(libretro_core_installed, sizeof(libretro_core_installed), full_path, tmp_path, extension);

   g_extern.verbose = false;

   bool find_libretro_file = false;

   if(libretro_core_installed != NULL)
      strlcpy(g_settings.libretro, libretro_core_installed, sizeof(g_settings.libretro));
   else
      find_libretro_file = true;

   return find_libretro_file;
}

void rarch_manage_libretro_set_first_file(char *first_file, size_t size_of_first_file, const char *libretro_path, const char * exe_ext)
{
   //We need to set libretro to the first entry in the cores
   //directory so that it will be saved to the config file

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
