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

const char *rarch_manage_libretro_install(const char *full_path, const char *path, const char *exe_ext)
{
   int ret;
   const char *retstr = NULL;
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
            RARCH_ERR("Failed to remove pre-existing libretro core: [%s].\n", tmp_pathnewfile);
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
	 retstr = tmp_pathnewfile;
         goto done;
      }
   }

   RARCH_WARN("CORE executable was not found, or some other errors occurred. Will attempt to load libretro core path from config file.\n");
done:
   return retstr;
}

const char *rarch_manage_libretro_set_first_file(const char *libretro_path, const char * exe_ext)
{
   //We need to set libretro to the first entry in the cores
   //directory so that it will be saved to the config file

   char ** dir_list = dir_list_new(libretro_path, exe_ext, false);

   const char * retstr = NULL;
   const char * first_exe;

   if (!dir_list)
   {
      RARCH_ERR("Couldn't read directory.\n");
      goto error;
   }

   first_exe = dir_list[0];

   if(first_exe)
   {
#ifdef _XBOX
      char fname_tmp[PATH_MAX];
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

      retstr = fname_tmp;
#else
      retstr = first_exe;
#endif
      RARCH_LOG("Set first entry in libretro core dir to libretro path: [%s].\n", retstr);
      goto end;
   }

error:
   RARCH_ERR("Failed to set first entry to libretro path.\n");
end:
   dir_list_free(dir_list);
   return retstr;
}
