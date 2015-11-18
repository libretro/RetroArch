/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <stddef.h>

#include <xtl.h>
#include <xbdm.h>
#include <xgraphics.h>

#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <file/file_list.h>
#endif
#include <retro_miscellaneous.h>

#include "platform_xdk.h"
#include "../../general.h"
#ifndef IS_SALAMANDER
#include "../../retroarch.h"
#ifdef HAVE_MENU
#include "../../menu/menu.h"
#endif
#endif

static bool exit_spawn;
static bool exitspawn_start_game;

#ifdef _XBOX360
typedef struct _STRING 
{
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING, *PSTRING;

#ifdef __cplusplus
extern "C" {
#endif
VOID RtlInitAnsiString(PSTRING DestinationString, PCHAR SourceString);	
HRESULT ObDeleteSymbolicLink(PSTRING SymbolicLinkName);
HRESULT ObCreateSymbolicLink(PSTRING SymbolicLinkName, PSTRING DeviceName);
#ifdef __cplusplus
}
#endif

HRESULT xbox_io_mount(const char* szDrive, char* szDevice)
{
	STRING DeviceName, LinkName;
	CHAR szDestinationDrive[PATH_MAX_LENGTH];
	sprintf_s(szDestinationDrive, PATH_MAX_LENGTH, "\\??\\%s", szDrive);
	RtlInitAnsiString(&DeviceName, szDevice);
	RtlInitAnsiString(&LinkName, szDestinationDrive);
	ObDeleteSymbolicLink(&LinkName);
	return (HRESULT)ObCreateSymbolicLink(&LinkName, &DeviceName);
}
#endif

#ifdef _XBOX1
static HRESULT xbox_io_mount(char *szDrive, char *szDevice)
{
#ifndef IS_SALAMANDER
   global_t            *global = global_get_ptr();
   bool original_verbose       = global->verbosity;
   global->verbosity           = true;
#endif
   char szSourceDevice[48]     = {0};
   char szDestinationDrive[16] = {0};

   snprintf(szSourceDevice, sizeof(szSourceDevice),
         "\\Device\\%s", szDevice);
   snprintf(szDestinationDrive, sizeof(szDestinationDrive),
         "\\??\\%s", szDrive);

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
   global->verbosity = original_verbose;
#endif
   return S_OK;
}

static HRESULT xbox_io_unmount(char *szDrive)
{
   char szDestinationDrive[16] = {0};

   snprintf(szDestinationDrive, sizeof(szDestinationDrive),
         "\\??\\%s", szDrive);

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
   global_t      *global = global_get_ptr();
   bool original_verbose = global->verbosity;

   global->verbosity = true;
#endif

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   global->log_file = fopen("/retroarch-log.txt", "w");
#endif
#endif

#ifdef _XBOX360
   // detect install environment
   unsigned long license_mask;
   DWORD volume_device_type;

   if (XContentGetLicenseMask(&license_mask, NULL) == ERROR_SUCCESS)
   {
      XContentQueryVolumeDeviceType("GAME",&volume_device_type, NULL);

      switch(volume_device_type)
      {
         case XCONTENTDEVICETYPE_HDD: /* Launched from content package on HDD */
         case XCONTENTDEVICETYPE_MU:  /* Launched from content package on USB/Memory Unit. */
         case XCONTENTDEVICETYPE_ODD: /* Launched from content package on Optial Disc Drive. */
         default:                     /* Launched from content package on unknown device. */
            break;
      }
   }
#endif

#if defined(_XBOX1)
   strlcpy(g_defaults.dir.core, "D:", sizeof(g_defaults.dir.core));
   strlcpy(g_defaults.dir.core_info, "D:", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.path.config, g_defaults.dir.core,
         "retroarch.cfg", sizeof(g_defaults.path.config));
   fill_pathname_join(g_defaults.dir.savestate, g_defaults.dir.core,
         "savestates", sizeof(g_defaults.dir.savestate));
   fill_pathname_join(g_defaults.dir.sram, g_defaults.dir.core,
         "savefiles", sizeof(g_defaults.dir.sram));
   fill_pathname_join(g_defaults.dir.system, g_defaults.dir.core,
         "system", sizeof(g_defaults.dir.system));
   fill_pathname_join(g_defaults.dir.screenshot, g_defaults.dir.core,
         "screenshots", sizeof(g_defaults.dir.screenshot));
#elif defined(_XBOX360)
   strlcpy(g_defaults.dir.core, "game:", sizeof(g_defaults.dir.core));
   strlcpy(g_defaults.dir.core_info,
         "game:", sizeof(g_defaults.dir.core_info));
   strlcpy(g_defaults.path.config,
         "game:\\retroarch.cfg", sizeof(g_defaults.path.config));
   strlcpy(g_defaults.dir.screenshot,
         "game:", sizeof(g_defaults.dir.screenshot));
   strlcpy(g_defaults.dir.savestate,
         "game:\\savestates", sizeof(g_defaults.dir.savestate));
   strlcpy(g_defaults.dir.playlist,
         "game:\\playlists", sizeof(g_defaults.dir.playlist));
   strlcpy(g_defaults.dir.sram,
         "game:\\savefiles", sizeof(g_defaults.dir.sram));
   strlcpy(g_defaults.dir.system,
         "game:\\system", sizeof(g_defaults.dir.system));
#endif

#ifndef IS_SALAMANDER
   static char path[PATH_MAX_LENGTH];
   *path = '\0';
#if defined(_XBOX1)
   LAUNCH_DATA ptr;
   DWORD launch_type;

   if (XGetLaunchInfo(&launch_type, &ptr) == ERROR_SUCCESS)
   {
      char *extracted_path = NULL;
      if (launch_type == LDT_FROM_DEBUGGER_CMDLINE)
         goto exit;

      extracted_path = (char*)&ptr.Data;

      if (extracted_path && extracted_path[0] != '\0'
            && (strstr(extracted_path, "Pool") == NULL)
            /* Hack. Unknown problem */)
      {
         /* Auto-start game */
         strlcpy(path, extracted_path, sizeof(path));
      }
   }
#elif defined(_XBOX360)
   DWORD dwLaunchDataSize;
   if (XGetLaunchDataSize(&dwLaunchDataSize) == ERROR_SUCCESS)
   {
      BYTE* pLaunchData = new BYTE[dwLaunchDataSize];
      XGetLaunchData(pLaunchData, dwLaunchDataSize);
	  AURORA_LAUNCHDATA_EXECUTABLE* aurora = (AURORA_LAUNCHDATA_EXECUTABLE*)pLaunchData;
	  char* extracted_path = new char[dwLaunchDataSize];
	  memset(extracted_path, 0, dwLaunchDataSize);
	  if (aurora->ApplicationId == AURORA_LAUNCHDATA_APPID && aurora->FunctionId == AURORA_LAUNCHDATA_EXECUTABLE_FUNCID)
	  {
		  if (xbox_io_mount("aurora:", aurora->SystemPath) >= 0)
			  sprintf_s(extracted_path, dwLaunchDataSize, "aurora:%s%s", aurora->RelativePath, aurora->Exectutable);
	  }
	  else
		  sprintf_s(extracted_path, dwLaunchDataSize, "%s", pLaunchData);
      if (extracted_path && extracted_path[0] != '\0')
      {
         /* Auto-start game */
         strlcpy(path, extracted_path, sizeof(path));
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
            /* Auto-start game. */
            args->touched        = true;
            args->no_content     = false;
            args->verbose        = false;
            args->config_path    = NULL;
            args->sram_path      = NULL;
            args->state_path     = NULL;
            args->content_path   = path;
            args->libretro_path  = NULL;
         }
   }
#endif

#ifndef IS_SALAMANDER
exit:
   global->verbosity = original_verbose;
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

static void frontend_xdk_set_fork(bool exit, bool start_game)
{
   exit_spawn = exit;
   exitspawn_start_game = start_game;
}

static void frontend_xdk_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   should_load_game = exitspawn_start_game;

   if (!exit_spawn)
      return;
#endif
   frontend_xdk_exec(s, should_load_game);
}

static void frontend_xdk_exec(const char *path, bool should_load_game)
{
#ifndef IS_SALAMANDER
   global_t *global = global_get_ptr();
   bool original_verbose = global->verbosity;
   global->verbosity = true;
#endif
   (void)should_load_game;

#ifdef IS_SALAMANDER
   if (path[0] != '\0')
      XLaunchNewImage(path, NULL);
#else
#ifdef _XBOX
   char *fullpath = NULL;
   menu-display_ctl(MENU_DISPLAY_CTL_GET_CONTENT_PATH, &fullpath);

#if defined(_XBOX1)
   LAUNCH_DATA ptr;
   memset(&ptr, 0, sizeof(ptr));


   if (should_load_game && fullpath[0] != '\0')
      snprintf((char*)ptr.Data, sizeof(ptr.Data), "%s", fullpath);

   if (path[0] != '\0')
      XLaunchNewImage(path, ptr.Data[0] != '\0' ? &ptr : NULL);
#elif defined(_XBOX360)
   char game_path[1024] = {0};

   if (should_load_game && fullpath[0] != '\0')
   {
      strlcpy(game_path, fullpath, sizeof(game_path));
      XSetLaunchData(game_path, MAX_LAUNCH_DATA_SIZE);
   }

   if (path[0] != '\0')
      XLaunchNewImage(path, NULL);
#endif
#endif
#endif
#ifndef IS_SALAMANDER
   global->verbosity = original_verbose;
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

enum frontend_architecture frontend_xdk_get_architecture(void)
{
#if defined(_XBOX360)
   return FRONTEND_ARCH_PPC;
#elif defined(_XBOX1)
   return FRONTEND_ARCH_X86;
#else
   return FRONTEND_ARCH_NONE;
#endif
}

static int frontend_xdk_parse_drive_list(void *data)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;

#if defined(_XBOX1)
   menu_entries_push(list,
         "C:", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "D:", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "E:", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "F:", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "G:", "", MENU_FILE_DIRECTORY, 0, 0);
#elif defined(_XBOX360)
   menu_entries_push(list,
         "game:", "", MENU_FILE_DIRECTORY, 0, 0);
#endif
#endif

   return 0;
}

frontend_ctx_driver_t frontend_ctx_xdk = {
   frontend_xdk_get_environment_settings,
   frontend_xdk_init,
   NULL,                         /* deinit */
   frontend_xdk_exitspawn,
   NULL,                         /* process_args */
   frontend_xdk_exec,
   frontend_xdk_set_fork,
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_xdk_get_rating,
   NULL,                         /* load_content */
   frontend_xdk_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_xdk_parse_drive_list,
   "xdk",
};
