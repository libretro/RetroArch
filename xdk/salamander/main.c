/* RetroArch - A frontend for libretro.
 * RetroArch Salamander - A frontend for managing some pre-launch tasks.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <xtl.h>

#include "../../compat/strl.h"
#include "../../conf/config_file.h"

#include "../../msvc/msvc_compat.h"

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#include "../../console/rarch_console.h"
#include "../../console/rarch_console_libretro_mgmt.h"
#include "../../console/rarch_console_exec.h"

#include "../../retroarch_logger.h"
#include "../../file.h"

DWORD volume_device_type;

char libretro_path[PATH_MAX];
char config_path[PATH_MAX];

default_paths_t default_paths;

static void find_and_set_first_file(void)
{
   //Last fallback - we'll need to start the first executable file 
   // we can find in the RetroArch cores directory

   char first_file[PATH_MAX];
   rarch_manage_libretro_set_first_file(first_file, sizeof(first_file),
#if defined(_XBOX360)
   "game:", "xex"
#elif defined(_XBOX1)
   "D:", "xbe"
#endif
);

   if(first_file)
   {
#ifdef _XBOX1
      snprintf(libretro_path, sizeof(libretro_path), "D:\\%s", first_file);
#else
      strlcpy(libretro_path, first_file, sizeof(libretro_path));
#endif
      RARCH_LOG("libretro_path now set to: %s.\n", libretro_path);
   }
   else
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
}

static void init_settings(void)
{
   char tmp_str[PATH_MAX];
   bool config_file_exists = false;

   if(path_file_exists(config_path))
      config_file_exists = true;

   //try to find CORE executable
   char core_executable[1024];
#if defined(_XBOX360)
   snprintf(core_executable, sizeof(core_executable), "game:\\CORE.xex");
#elif defined(_XBOX1)
   snprintf(core_executable, sizeof(core_executable), "D:\\CORE.xbe");
#endif

   if(path_file_exists(core_executable))
   {
      //Start CORE executable
      snprintf(libretro_path, sizeof(libretro_path), core_executable);
      RARCH_LOG("Start [%s].\n", libretro_path);
   }
   else
   {
      if(config_file_exists)
      {
		  config_file_t * conf = config_file_new(config_path);
         config_get_array(conf, "libretro_path", tmp_str, sizeof(tmp_str));
         snprintf(libretro_path, sizeof(libretro_path), tmp_str);
      }

      if(!config_file_exists || !strcmp(libretro_path, ""))
      {
         find_and_set_first_file();
      }
      else
      {
         RARCH_LOG("Start [%s] found in retroarch.cfg.\n", libretro_path);
      }

      if (!config_file_exists)
      {
         config_file_t *new_conf = config_file_new(NULL);
         config_set_string(new_conf, "libretro_path", libretro_path);
         config_file_write(new_conf, config_path);
         config_file_free(new_conf);
      }
   }
}

static void get_environment_settings (void)
{
#if defined(_XBOX360)
   //for devkits only, we will need to mount all partitions for retail
   //in a different way
   //DmMapDevkitDrive();

   int result_filecache = XSetFileCacheSize(0x100000);

   if(result_filecache != TRUE)
   {
      RARCH_ERR("Couldn't change number of bytes reserved for file system cache.\n");
   }
   unsigned long result = XMountUtilityDriveEx(XMOUNTUTILITYDRIVE_FORMAT0,8192, 0);

   if(result != ERROR_SUCCESS)
   {
      RARCH_ERR("Couldn't mount/format utility drive.\n");
   }

   // detect install environment
   unsigned long license_mask;

   if (XContentGetLicenseMask(&license_mask, NULL) != ERROR_SUCCESS)
   {
      RARCH_LOG("RetroArch was launched as a standalone DVD, or using DVD emulation, or from the development area of the HDD.\n");
   }
   else
   {
      XContentQueryVolumeDeviceType("GAME",&volume_device_type, NULL);

      switch(volume_device_type)
      {
         case XCONTENTDEVICETYPE_HDD:
            RARCH_LOG("RetroArch was launched from a content package on HDD.\n");
	    break;
	 case XCONTENTDEVICETYPE_MU:
	    RARCH_LOG("RetroArch was launched from a content package on USB or Memory Unit.\n");
	    break;
	 case XCONTENTDEVICETYPE_ODD:
	    RARCH_LOG("RetroArch was launched from a content package on Optical Disc Drive.\n");
	    break;
	 default:
	    RARCH_LOG("RetroArch was launched from a content package on an unknown device type.\n");
	    break;
      }
   }
#endif

#if defined(_XBOX1)
   strlcpy(default_paths.core_dir, "D:", sizeof(default_paths.core_dir));
   strlcpy(config_path, "D:\\retroarch.cfg", sizeof(config_path));
   strlcpy(default_paths.system_dir, "D:\\system", sizeof(default_paths.system_dir));
   strlcpy(default_paths.filesystem_root_dir, "D:", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.executable_extension, ".xbe", sizeof(default_paths.executable_extension));
   strlcpy(default_paths.filebrowser_startup_dir, "D:", sizeof(default_paths.filebrowser_startup_dir));
   strlcpy(default_paths.screenshots_dir, "D:\\screenshots", sizeof(default_paths.screenshots_dir));
   strlcpy(default_paths.salamander_file, "default.xbe", sizeof(default_paths.salamander_file));
#elif defined(_XBOX360)
   strlcpy(default_paths.filesystem_root_dir, "game:\\", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.screenshots_dir, "game:", sizeof(default_paths.screenshots_dir));
   strlcpy(config_path, "game:\\retroarch.cfg", sizeof(config_path));
   strlcpy(default_paths.system_dir, "game:\\system\\", sizeof(default_paths.system_dir));
   strlcpy(default_paths.executable_extension, ".xex", sizeof(default_paths.executable_extension));
   snprintf(default_paths.salamander_file, sizeof(default_paths.salamander_file), "default.xex");
#endif
}

int main(int argc, char *argv[])
{
   XINPUT_STATE state;
   (void)state;

   get_environment_settings();

   //WIP - no Xbox 1 controller input yet
#ifdef _XBOX360
   XInputGetState(0, &state);

   if(state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
   {
      //override path, boot first executable in cores directory
      RARCH_LOG("Fallback - Will boot first executable in RetroArch cores directory.\n");
      find_and_set_first_file();
   }
   else
#endif
   {
      //normal executable loading path
      init_settings();
   }

   rarch_console_exec(libretro_path);

   return 1;
}
