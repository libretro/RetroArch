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

#include "console_ext.h"

bool rarch_manage_libretro_install(const char *full_path, const char *path, const char *exe_ext)
{
   g_extern.verbose = true;
   bool return_code;

   bool set_libretro_path = false;
   char tmp_path2[1024], tmp_pathnewfile[1024];
   RARCH_LOG("Assumed path of CORE executable: [%s]\n", full_path);

   if (path_file_exists(full_path))
   {
      // if CORE executable exists, this means we have just installed
      // a new libretro port and therefore we need to change it to a more
      // sane name.

      int ret;

      rarch_console_name_from_id(tmp_path2, sizeof(tmp_path2));
      strlcat(tmp_path2, exe_ext, sizeof(tmp_path2));
      snprintf(tmp_pathnewfile, sizeof(tmp_pathnewfile), "%s%s", path, tmp_path2);

      if (path_file_exists(tmp_pathnewfile))
      {
         // if libretro core already exists, this means we are
         // upgrading the libretro core - so delete pre-existing
         // file first.

         RARCH_LOG("Upgrading emulator core...\n");
#if defined(__CELLOS_LV2__)
         ret = cellFsUnlink(tmp_pathnewfile);
         if (ret == CELL_FS_SUCCEEDED)
#elif defined(_XBOX)
            ret = DeleteFile(tmp_pathnewfile);
         if (ret != 0)
#endif
         {
            RARCH_LOG("Succeeded in removing pre-existing libretro core: [%s].\n", tmp_pathnewfile);
         }
         else
            RARCH_LOG("Failed to remove pre-existing libretro core: [%s].\n", tmp_pathnewfile);
      }

      //now attempt the renaming.
#if defined(__CELLOS_LV2__)
      ret = cellFsRename(full_path, tmp_pathnewfile);

      if (ret != CELL_FS_SUCCEEDED)
#elif defined(_XBOX)
         ret = MoveFileExA(full_path, tmp_pathnewfile, NULL);
      if (ret == 0)
#endif
      {
         RARCH_ERR("Failed to rename CORE executable.\n");
      }
      else
      {
         RARCH_LOG("Libsnes core [%s] renamed to: [%s].\n", full_path, tmp_pathnewfile);
         set_libretro_path = true;
      }
   }
   else
   {
      RARCH_LOG("CORE executable was not found, libretro core path will be loaded from config file.\n");
   }

   if (set_libretro_path)
   {
      // CORE executable has been renamed, libretro path will now be set to the recently
      // renamed new libretro core.
      strlcpy(g_settings.libretro, tmp_pathnewfile, sizeof(g_settings.libretro));
      return_code = 0;
   }
   else
   {
      // There was no CORE executable present, or the CORE executable file was not renamed.
      // The libretro core path will still be loaded from the config file.
      return_code = 1;
   }

   g_extern.verbose = false;

   return return_code;
}

void rarch_manage_libretro_set_first_file(const char *libretro_path, const char * exe_ext)
{
#ifdef _XBOX
   char fname_tmp[PATH_MAX];
#endif

   //We need to set libretro to the first entry in the cores
   //directory so that it will be saved to the config file

   char ** dir_list = dir_list_new(libretro_path, exe_ext);

   if (!dir_list)
   {
      RARCH_ERR("Couldn't read directory.\n");
      return;
   }

   const char * first_exe = dir_list[0];

   if(first_exe)
   {
#ifdef _XBOX
      fill_pathname_base(fname_tmp, first_exe, sizeof(fname_tmp));

      if(strcmp(fname_tmp, "RetroArch-Salamander.xex") == 0)
      {
         RARCH_WARN("First entry is RetroArch Salamander itself, increment entry by one and check if it exists.\n");
	 first_exe = dir_list[1];
	 fill_pathname_base(fname_tmp, first_exe, sizeof(fname_tmp));

	 if(!first_exe)
	 {
            RARCH_ERR("Unlikely error happened - no second entry - no choice but to set it to RetroArch Salamander\n");
	    first_exe = dir_list[0];
	    fill_pathname_base(fname_tmp, first_exe, sizeof(fname_tmp));
	 }
      }

      RARCH_LOG("Set first entry in libretro core dir to libretro path: [%s].\n", fname_tmp);
      snprintf(g_settings.libretro, sizeof(g_settings.libretro), "game:\\%s", fname_tmp);
#else
      RARCH_LOG("Set first entry in libretro core dir to libretro path: [%s].\n", first_exe);
      strlcpy(g_settings.libretro, first_exe, sizeof(g_settings.libretro));
#endif
   }
   else
   {
      RARCH_ERR("Failed to set first .xex entry to libretro path.\n");
   }

   dir_list_free(dir_list);
}
