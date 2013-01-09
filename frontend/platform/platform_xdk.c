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
#include "platform_inl.h"

#if defined(_XBOX360)
#include <xfilecache.h>
#include "../../360/frontend-xdk/menu.h"
#include "../../xdk/menu_shared.h"
#elif defined(_XBOX1)
#include "../../xbox1/frontend/RetroLaunch/IoSupport.h"
#include "../../console/rmenu/rmenu.h"
#endif

#include <xbdm.h>

#ifdef _XBOX
#include "../../xdk/xdk_d3d.h"
#endif

#include "../../console/rarch_console.h"
#include "../../console/rarch_console_config.h"
#include "../../conf/config_file.h"
#include "../../conf/config_file_macros.h"
#include "../../file.h"
#include "../../general.h"

#ifdef IS_SALAMANDER

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

#define CTLCODE(DeviceType, Function, Method, Access) ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method)  )
#define FSCTL_DISMOUNT_VOLUME  CTLCODE( FILE_DEVICE_FILE_SYSTEM, 0x08, METHOD_BUFFERED, FILE_ANY_ACCESS )

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

static HRESULT xbox_io_remount(char *szDrive, char *szDevice)
{
   char szSourceDevice[48];
   snprintf(szSourceDevice, sizeof(szSourceDevice), "\\Device\\%s", szDevice);

   xbox_io_unmount(szDrive);

   ANSI_STRING filename;
   OBJECT_ATTRIBUTES attributes;
   IO_STATUS_BLOCK status;
   HANDLE hDevice;
   NTSTATUS error;
   DWORD dummy;

   RtlInitAnsiString(&filename, szSourceDevice);
   InitializeObjectAttributes(&attributes, &filename, OBJ_CASE_INSENSITIVE, NULL);

   if (!NT_SUCCESS(error = NtCreateFile(&hDevice, GENERIC_READ |
               SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attributes, &status, NULL, 0,
               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
               FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
   {
      return E_FAIL;
   }

   if (!DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dummy, NULL))
   {
      CloseHandle(hDevice);
      return E_FAIL;
   }

   CloseHandle(hDevice);
   xbox_io_mount(szDrive, szDevice);

   return S_OK;
}

static HRESULT xbox_io_remap(char *szMapping)
{
   char szMap[32];
   strlcpy(szMap, szMapping, sizeof(szMap));

   char *pComma = strstr(szMap, ",");

   if (pComma)
   {
      *pComma = 0;

      // map device to drive letter
      xbox_io_unmount(szMap);
      xbox_io_mount(szMap, &pComma[1]);
      return S_OK;
   }

   return E_FAIL;
}

static HRESULT xbox_io_shutdown(void)
{
   HalInitiateShutdown();
   return S_OK;
}

#endif

static void get_environment_settings(int argc, char *argv[])
{
   HRESULT ret;
   (void)argc;
   (void)argv;
   (void)ret;
#if defined(_XBOX360) || defined(HAVE_HDD_CACHE_PARTITION)
   //for devkits only, we will need to mount all partitions for retail
   //in a different way
   //DmMapDevkitDrive();
   ret = XSetFileCacheSize(0x100000);

   if(ret != TRUE)
   {
      RARCH_ERR("Couldn't change number of bytes reserved for file system cache.\n");
   }

   ret = XFileCacheInit(XFILECACHE_CLEAR_ALL, 0x100000, XFILECACHE_DEFAULT_THREAD, 0, 1);

   if(ret != ERROR_SUCCESS)
   {
      RARCH_ERR("File cache could not be initialized.\n");
   }

   XFlushUtilityDrive();
#endif

#ifdef _XBOX360
   // detect install environment
   unsigned long license_mask;
   DWORD volume_device_type;

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
#ifdef IS_SALAMANDER
   strlcpy(default_paths.config_path, "D:\\retroarch.cfg", sizeof(default_paths.config_path));
#else
   strlcpy(g_extern.config_path, "D:\\retroarch.cfg", sizeof(g_extern.config_path));
#endif
   strlcpy(default_paths.system_dir, "D:\\system", sizeof(default_paths.system_dir));
   strlcpy(default_paths.filesystem_root_dir, "D:", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.executable_extension, ".xbe", sizeof(default_paths.executable_extension));
   strlcpy(default_paths.filebrowser_startup_dir, "D:", sizeof(default_paths.filebrowser_startup_dir));
   strlcpy(default_paths.screenshots_dir, "D:\\screenshots", sizeof(default_paths.screenshots_dir));
   strlcpy(default_paths.salamander_file, "default.xbe", sizeof(default_paths.salamander_file));
#elif defined(_XBOX360)
#ifdef HAVE_HDD_CACHE_PARTITION
   strlcpy(default_paths.cache_dir, "cache:\\", sizeof(default_paths.cache_dir));
#endif
   strlcpy(default_paths.filesystem_root_dir, "game:\\", sizeof(default_paths.filesystem_root_dir));
   strlcpy(default_paths.screenshots_dir, "game:", sizeof(default_paths.screenshots_dir));
#ifdef IS_SALAMANDER
   strlcpy(default_paths.config_path, "game:\\retroarch.cfg", sizeof(default_paths.config_path));
#else
   strlcpy(default_paths.shader_file, "game:\\media\\shaders\\stock.cg", sizeof(default_paths.shader_file));
   strlcpy(g_extern.config_path, "game:\\retroarch.cfg", sizeof(g_extern.config_path));
#endif
   strlcpy(default_paths.system_dir, "game:\\system", sizeof(default_paths.system_dir));
   strlcpy(default_paths.executable_extension, ".xex", sizeof(default_paths.executable_extension));
   strlcpy(default_paths.filebrowser_startup_dir, "game:", sizeof(default_paths.filebrowser_startup_dir));
   snprintf(default_paths.salamander_file, sizeof(default_paths.salamander_file), "default.xex");
#endif
}

static void system_post_init(void)
{
}

static void system_init(void)
{
#if defined (HAVE_LOGGER) || defined(HAVE_FILE_LOGGER)
   inl_logger_init();
#endif

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

static void system_process_args(int argc, char *argv[])
{
   (void)argc;
   (void)argv;
}

static void system_deinit(void)
{
#if defined (HAVE_LOGGER) || defined(HAVE_FILE_LOGGER)
   logger_deinit();
#endif
}

static void system_deinit_save(void)
{
}

static void system_exitspawn(void)
{
#ifdef IS_SALAMANDER
   rarch_console_exec(default_paths.libretro_path);
#else
   if(g_extern.console.external_launch.enable)
      rarch_console_exec(g_extern.console.external_launch.launch_app);
#endif
}
