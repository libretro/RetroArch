/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "platform_xdk.h"

#include <xbdm.h>

#include "../../conf/config_file.h"
#include "../../conf/config_file_macros.h"
#include "../../file.h"

#ifndef IS_SALAMANDER
#include "../../general.h"
#endif

#ifdef _XBOX1
static HRESULT xbox_io_mount(char *szDrive, char *szDevice)
{
#ifndef IS_SALAMANDER
   bool original_verbose = g_extern.verbose;
   g_extern.verbose = true;
#endif
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

#ifndef IS_SALAMANDER
   g_extern.verbose = original_verbose;
#endif
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

static void frontend_xdk_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   HRESULT ret;
   (void)ret;

#ifndef IS_SALAMANDER
   bool original_verbose = g_extern.verbose;
   g_extern.verbose = true;
#endif

#ifndef IS_SALAMANDER
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
   strlcpy(g_defaults.core_dir, "D:", sizeof(g_defaults.core_dir));
   strlcpy(g_defaults.core_info_dir, "D:", sizeof(g_defaults.core_info_dir));
   fill_pathname_join(g_defaults.config_path, g_defaults.core_dir, "retroarch.cfg", sizeof(g_defaults.config_path));
   fill_pathname_join(g_defaults.savestate_dir, g_defaults.core_dir, "savestates", sizeof(g_defaults.savestate_dir));
   fill_pathname_join(g_defaults.sram_dir, g_defaults.core_dir, "savefiles", sizeof(g_defaults.sram_dir));
   fill_pathname_join(g_defaults.system_dir, g_defaults.core_dir, "system", sizeof(g_defaults.system_dir));
   fill_pathname_join(g_defaults.screenshot_dir, g_defaults.core_dir, "screenshots", sizeof(g_defaults.screenshot_dir));
#ifndef IS_SALAMANDER
   strlcpy(g_extern.menu_texture_path, "D:\\Media\\main-menu_480p.png", sizeof(g_extern.menu_texture_path));
#endif
#elif defined(_XBOX360)
   strlcpy(g_defaults.core_dir, "game:", sizeof(g_defaults.core_dir));
   strlcpy(g_defaults.core_info_dir, "game:", sizeof(g_defaults.core_info_dir));
   strlcpy(g_defaults.config_path, "game:\\retroarch.cfg", sizeof(g_defaults.config_path));
   strlcpy(g_defaults.screenshot_dir, "game:", sizeof(g_defaults.screenshot_dir));
   strlcpy(g_defaults.savestate_dir, "game:\\savestates", sizeof(g_defaults.savestate_dir));
   strlcpy(g_defaults.sram_dir, "game:\\savefiles", sizeof(g_defaults.sram_dir));
   strlcpy(g_defaults.system_dir, "game:\\system", sizeof(g_defaults.system_dir));
#endif

#ifndef IS_SALAMANDER
   char path[PATH_MAX];
#if defined(_XBOX1)
   RARCH_LOG("Gets here top.\n");
   LAUNCH_DATA ptr;
   DWORD launch_type;

   if (XGetLaunchInfo(&launch_type, &ptr) == ERROR_SUCCESS)
   {
      if (launch_type == LDT_FROM_DEBUGGER_CMDLINE)
      {
         RARCH_LOG("Launched from commandline debugger.\n");
         goto exit;
      }
      else
      {
         char *extracted_path = (char*)&ptr.Data;

         if (extracted_path && extracted_path[0] != '\0'
            && (strstr(extracted_path, "Pool") == NULL) /* Hack. Unknown problem */)
         {
            RARCH_LOG("Gets here 3.\n");
            strlcpy(path, extracted_path, sizeof(path));
            RARCH_LOG("Auto-start game %s.\n", path);
         }
      }
   }
#elif defined(_XBOX360)
   DWORD dwLaunchDataSize;
   if (XGetLaunchDataSize(&dwLaunchDataSize) == ERROR_SUCCESS)
   {
      BYTE* pLaunchData = new BYTE[dwLaunchDataSize];
      XGetLaunchData(pLaunchData, dwLaunchDataSize);
      char *extracted_path = (char*)&pLaunchData;

      if (extracted_path && extracted_path[0] != '\0')
      {
         strlcpy(path, extracted_path, sizeof(path));
         RARCH_LOG("Auto-start game %s.\n", path);
      }

      if (pLaunchData)
         delete []pLaunchData;
   }
#endif
   if (path && path[0] != '\0')
   {
         struct rarch_main_wrap *args = (struct rarch_main_wrap*)params_data;

         if (args)
         {
            args->touched        = true;
            args->no_rom         = false;
            args->verbose        = false;
            args->config_path    = NULL;
            args->sram_path      = NULL;
            args->state_path     = NULL;
            args->rom_path       = strdup(path);
            args->libretro_path  = NULL;

            RARCH_LOG("Auto-start game %s.\n", path);
         }
   }
#endif

#ifndef IS_SALAMANDER
exit:
   g_extern.verbose = original_verbose;
#endif
}

static void frontend_xdk_init(void *data)
{
   (void)data;
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

static void frontend_xdk_exec(const char *path, bool should_load_game);

static void frontend_xdk_exitspawn(char *core_path, size_t sizeof_core_path)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (g_extern.lifecycle_state & (1ULL << MODE_EXITSPAWN_START_GAME))
      should_load_game = true;
#endif
   frontend_xdk_exec(core_path, should_load_game);
}

#include <stdio.h>

#include <xtl.h>

#include "../../retroarch_logger.h"

static void frontend_xdk_exec(const char *path, bool should_load_game)
{
#ifndef IS_SALAMANDER
   bool original_verbose = g_extern.verbose;
   g_extern.verbose = true;
#endif
   (void)should_load_game;

   RARCH_LOG("Attempt to load executable: [%s].\n", path);
#ifdef IS_SALAMANDER
   if (path[0] != '\0')
      XLaunchNewImage(path, NULL);
#else
#if defined(_XBOX1)
   LAUNCH_DATA ptr;
   memset(&ptr, 0, sizeof(ptr));
   if (should_load_game && g_extern.fullpath[0] != '\0')
      snprintf((char*)ptr.Data, sizeof(ptr.Data), "%s", g_extern.fullpath);

   if (path[0] != '\0')
      XLaunchNewImage(path, ptr.Data[0] != '\0' ? &ptr : NULL);
#elif defined(_XBOX360)
   char game_path[1024];
   if (should_load_game && g_extern.fullpath[0] != '\0')
   {
      strlcpy(game_path, g_extern.fullpath, sizeof(game_path));
      XSetLaunchData(game_path, MAX_LAUNCH_DATA_SIZE);
   }

   if (path[0] != '\0')
      XLaunchNewImage(path, NULL);
#endif
#endif
#ifndef IS_SALAMANDER
   g_extern.verbose = original_verbose;
#endif
}

static int frontend_xdk_get_rating(void)
{
#if defined(_XBOX360)
   return 11;
#elif defined(_XBOX1)
   return 7;
#endif
}

const frontend_ctx_driver_t frontend_ctx_xdk = {
   frontend_xdk_get_environment_settings,     /* get_environment_settings */
   frontend_xdk_init,            /* init */
   NULL,                         /* deinit */
   frontend_xdk_exitspawn,       /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   frontend_xdk_exec,            /* exec */
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   frontend_xdk_get_rating,      /* get_rating */
   "xdk",
};
