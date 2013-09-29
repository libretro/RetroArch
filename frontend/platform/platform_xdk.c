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

#include <xtl.h>

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "platform_xdk.h"

#if defined(_XBOX360)
#include <xfilecache.h>
#endif

#include <xbdm.h>

#include "../../console/rarch_console.h"
#include "../../conf/config_file.h"
#include "../../conf/config_file_macros.h"
#include "../../file.h"
#include "../../general.h"

#ifdef IS_SALAMANDER

static void find_and_set_first_file(void)
{
   //Last fallback - we'll need to start the first executable file 
   // we can find in the RetroArch cores directory

   char first_file[PATH_MAX];
   find_first_libretro_core(first_file, sizeof(first_file),
#if defined(_XBOX360)
   "game:", "xex"
#elif defined(_XBOX1)
   "D:", "xbe"
#endif
);

   if(first_file)
   {
#ifdef _XBOX1
      snprintf(default_paths.libretro_path, sizeof(default_paths.libretro_path), "D:\\%s", first_file);
#else
      strlcpy(default_paths.libretro_path, first_file, sizeof(default_paths.libretro_path));
#endif
      RARCH_LOG("libretro_path now set to: %s.\n", default_paths.libretro_path);
   }
   else
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
}

static void salamander_init_settings(void)
{
   XINPUT_STATE state;
   (void)state;

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
	   char tmp_str[PATH_MAX];
	   bool config_file_exists = false;

	   if(path_file_exists(default_paths.config_path))
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
		   snprintf(default_paths.libretro_path, sizeof(default_paths.libretro_path), core_executable);
		   RARCH_LOG("Start [%s].\n", default_paths.libretro_path);
	   }
	   else
	   {
		   if(config_file_exists)
		   {
			   config_file_t * conf = config_file_new(default_paths.config_path);
			   config_get_array(conf, "libretro_path", tmp_str, sizeof(tmp_str));
			   snprintf(default_paths.libretro_path, sizeof(default_paths.libretro_path), tmp_str);
		   }

		   if(!config_file_exists || !strcmp(default_paths.libretro_path, ""))
		   {
			   find_and_set_first_file();
		   }
		   else
		   {
			   RARCH_LOG("Start [%s] found in retroarch.cfg.\n", default_paths.libretro_path);
		   }

		   if (!config_file_exists)
		   {
			   config_file_t *new_conf = config_file_new(NULL);
			   config_set_string(new_conf, "libretro_path", default_paths.libretro_path);
			   config_file_write(new_conf, default_paths.config_path);
			   config_file_free(new_conf);
		   }
	   }
   }
}

#endif

#ifdef _XBOX1
static HRESULT xbox_io_mount(char *szDrive, char *szDevice)
{
   char szSourceDevice[48];
   char szDestinationDrive[16];

   snprintf(szSourceDevice, sizeof(szSourceDevice), "\\Device\\%s", szDevice);
   snprintf(szDestinationDrive, sizeof(szDestinationDrive), "\\??\\%s", szDrive);
   RARCH_LOG("xbox_io_mount() - source device: %s.\n", szSourceDevice);
   RARCH_LOG("xbox_io_mount() - destination drive: %s.\n", szDestinationDrive);

   STRING DeviceName =
   {
      strlen(szSourceDevice),
      strlen(szSourceDevice) + 1,
      szSourceDevice
   };

   STRING LinkName =
   {
      strlen(szDestinationDrive),
      strlen(szDestinationDrive) + 1,
      szDestinationDrive
   };

   IoCreateSymbolicLink(&LinkName, &DeviceName);

   return S_OK;
}

static HRESULT xbox_io_unmount(char *szDrive)
{
   char szDestinationDrive[16];
   snprintf(szDestinationDrive, sizeof(szDestinationDrive), "\\??\\%s", szDrive);

   STRING LinkName =
   {
      strlen(szDestinationDrive),
      strlen(szDestinationDrive) + 1,
      szDestinationDrive
   };

   IoDeleteSymbolicLink(&LinkName);

   return S_OK;
}
#endif

static void get_environment_settings(int argc, char *argv[], void *args)
{
   HRESULT ret;
   (void)ret;

#ifndef IS_SALAMANDER
   g_extern.verbose = true;

#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   g_extern.log_file = fopen("/retroarch-log.txt", "w");
#endif
#endif

#ifdef _XBOX360
   // detect install environment
   unsigned long license_mask;
   DWORD volume_device_type;

   if (XContentGetLicenseMask(&license_mask, NULL) != ERROR_SUCCESS)
      RARCH_LOG("RetroArch was launched as a standalone DVD, or using DVD emulation, or from the development area of the HDD.\n");
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
#ifdef IS_SALAMANDER
   strlcpy(default_paths.config_path, "D:\\retroarch.cfg", sizeof(default_paths.config_path));
#else
   strlcpy(g_extern.config_path, "D:\\retroarch.cfg", sizeof(g_extern.config_path));
#endif
   strlcpy(default_paths.savestate_dir, "D:\\savestates", sizeof(default_paths.savestate_dir));
   strlcpy(default_paths.sram_dir, "D:\\savefiles", sizeof(default_paths.sram_dir));
   strlcpy(default_paths.system_dir, "D:\\system", sizeof(default_paths.system_dir));
   strlcpy(default_paths.filesystem_root_dir, "D:", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.filebrowser_startup_dir, "D:", sizeof(default_paths.filebrowser_startup_dir));
#ifndef IS_SALAMANDER
   strlcpy(g_settings.screenshot_directory, "D:\\screenshots", sizeof(g_settings.screenshot_directory));
#endif
   strlcpy(default_paths.menu_border_file, "D:\\Media\\main-menu_480p.png", sizeof(default_paths.menu_border_file));
#elif defined(_XBOX360)
   strlcpy(default_paths.core_dir, "game:", sizeof(default_paths.core_dir));
   strlcpy(default_paths.filesystem_root_dir, "game:\\", sizeof(default_paths.filesystem_root_dir));
#ifdef IS_SALAMANDER
   strlcpy(default_paths.config_path, "game:\\retroarch.cfg", sizeof(default_paths.config_path));
#else
   strlcpy(g_settings.screenshot_directory, "game:", sizeof(g_settings.screenshot_directory));
   strlcpy(g_extern.config_path, "game:\\retroarch.cfg", sizeof(g_extern.config_path));
#endif
   strlcpy(default_paths.savestate_dir, "game:\\savestates", sizeof(default_paths.savestate_dir));
   strlcpy(default_paths.sram_dir, "game:\\savefiles", sizeof(default_paths.sram_dir));
   strlcpy(default_paths.system_dir, "game:\\system", sizeof(default_paths.system_dir));
   strlcpy(default_paths.filebrowser_startup_dir, "game:", sizeof(default_paths.filebrowser_startup_dir));
#endif

#ifndef IS_SALAMANDER
   rarch_make_dir(default_paths.port_dir, "port_dir");
   rarch_make_dir(default_paths.system_dir, "system_dir");
   rarch_make_dir(default_paths.savestate_dir, "savestate_dir");
   rarch_make_dir(default_paths.sram_dir, "sram_dir");

   config_load();

   rarch_get_environment_console();
#endif
}

static void system_init(void)
{
#if defined(_XBOX1) && !defined(IS_SALAMANDER)
   // Mount drives
   xbox_io_mount("A:", "cdrom0");
   xbox_io_mount("C:", "Harddisk0\\Partition0");
   xbox_io_mount("E:", "Harddisk0\\Partition1");
   xbox_io_mount("Z:", "Harddisk0\\Partition2");
   xbox_io_mount("F:", "Harddisk0\\Partition6");
   xbox_io_mount("G:", "Harddisk0\\Partition7");
#endif
}

static int system_process_args(int argc, char *argv[], void *args)
{
   (void)argc;
   (void)argv;

#if defined(_XBOX1)
   LAUNCH_DATA ptr;
   DWORD launch_type;

   if (XGetLaunchInfo(&launch_type, &ptr) == ERROR_SUCCESS)
   {
      if (launch_type == LDT_FROM_DEBUGGER_CMDLINE)
         RARCH_LOG("Launched from commandline debugger.\n");
      else
      {
         snprintf(g_extern.fullpath, sizeof(g_extern.fullpath), (char*)ptr.Data);
         RARCH_LOG("Auto-start game %s.\n", g_extern.fullpath);
         return 1;
      }
   }
#elif defined(_XBOX360)
   DWORD dwLaunchDataSize;
   if (XGetLaunchDataSize(&dwLaunchDataSize) == ERROR_SUCCESS)
   {
      BYTE* pLaunchData = new BYTE[dwLaunchDataSize];
      XGetLaunchData(pLaunchData, dwLaunchDataSize);

      snprintf(g_extern.fullpath, sizeof(g_extern.fullpath), "%s", (char*)pLaunchData);
      RARCH_LOG("Auto-start game %s.\n", g_extern.fullpath);

      delete []pLaunchData;
      return 1;
   }
#endif
   return 0;
}

static void system_exec(const char *path, bool should_load_game);

static void system_exitspawn(void)
{
#ifdef IS_SALAMANDER
   system_exec(default_paths.libretro_path, false);
#else
   bool should_load_game = false;
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_EXITSPAWN_START_GAME))
      should_load_game = true;

   system_exec(g_settings.libretro, should_load_game);
#endif
}

#include <stdio.h>

#include <xtl.h>

#include "../../retroarch_logger.h"

static void system_exec(const char *path, bool should_load_game)
{
   (void)should_load_game;

   RARCH_LOG("Attempt to load executable: [%s].\n", path);
#ifdef IS_SALAMANDER
   XLaunchNewImage(path, NULL);
#else
#if defined(_XBOX1)
   LAUNCH_DATA ptr;
   memset(&ptr, 0, sizeof(ptr));
   if (should_load_game)
   {
      snprintf((char*)ptr.Data, sizeof(ptr.Data), "%s", g_extern.fullpath);
      XLaunchNewImage(path, &ptr);
   }
   else
      XLaunchNewImage(path, NULL);
#elif defined(_XBOX360)
   char game_path[1024];
   if (should_load_game)
   {
      strlcpy(game_path, g_extern.fullpath, sizeof(game_path));
      XSetLaunchData(game_path, MAX_LAUNCH_DATA_SIZE);
   }
   XLaunchNewImage(path, NULL);
#endif
#endif
}

const frontend_ctx_driver_t frontend_ctx_xdk = {
   get_environment_settings,     /* get_environment_settings */
   system_init,                  /* init */
   NULL,                         /* deinit */
   system_exitspawn,             /* exitspawn */
   system_process_args,          /* process_args */
   NULL,                         /* process_events */
   system_exec,                  /* exec */
   NULL,                         /* shutdown */
   "xdk",
};
