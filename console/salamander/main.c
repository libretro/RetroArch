/* RetroArch - A frontend for libretro.
 * RetroArch Salamander - A frontend for managing some pre-launch tasks.
 * Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2012 - Daniel De Matteis
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

#if defined(__CELLOS_LV2__)
#include <cell/pad.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_gamecontent.h>
#include <sys/process.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#include <dirent.h>
#elif defined(_XBOX)
#include <xtl.h>
#endif

#include "../../compat/strl.h"
#include "../../conf/config_file.h"

#if defined(_XBOX)
#include "../../msvc/msvc_compat.h"
#elif defined(__CELLOS_LV2__)
#define NP_POOL_SIZE (128*1024)
#endif

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#include "../rarch_console_libretro_mgmt.h"
#include "../rarch_console_exec.h"

#include "../../retroarch_logger.h"
#include "../../file.h"

#if defined(__CELLOS_LV2__)
static uint8_t np_pool[NP_POOL_SIZE];
char usrDirPath[PATH_MAX];
SYS_PROCESS_PARAM(1001, 0x100000)
#elif defined(_XBOX)
DWORD volume_device_type;
#endif

char LIBRETRO_DIR_PATH[PATH_MAX];
char SYS_CONFIG_FILE[PATH_MAX];
char libretro_path[PATH_MAX];

static void find_and_set_first_file(void)
{
   //Last fallback - we'll need to start the first executable file 
   // we can find in the RetroArch cores directory

   char first_file[PATH_MAX];
   rarch_manage_libretro_set_first_file(first_file, sizeof(first_file),
#if defined(_XBOX360)
   "game:\\", "xex"
#elif defined(_XBOX1)
   "D:\\", "xbe"
#elif defined(__CELLOS_LV2__)
   LIBRETRO_DIR_PATH, "SELF"
#endif
);

   if(first_file)
      strlcpy(libretro_path, first_file, sizeof(libretro_path));
   else
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
}

static void init_settings(void)
{
   char tmp_str[PATH_MAX];
   bool config_file_exists;

   if(!path_file_exists(SYS_CONFIG_FILE))
   {
      FILE * f;
      config_file_exists = false;
      RARCH_ERR("Config file \"%s\" doesn't exist. Creating...\n", SYS_CONFIG_FILE);
      f = fopen(SYS_CONFIG_FILE, "w");
      fclose(f);
   }
   else
      config_file_exists = true;

   //try to find CORE executable
   char core_executable[1024];
#if defined(_XBOX360)
   snprintf(core_executable, sizeof(core_executable), "game:\\CORE.xex");
#elif defined(_XBOX1)
   snprintf(core_executable, sizeof(core_executable), "D:\\CORE.xbe");
#elif defined(__CELLOS_LV2__)
   snprintf(core_executable, sizeof(core_executable), "%s/CORE.SELF", LIBRETRO_DIR_PATH);
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
         config_file_t * conf = config_file_new(SYS_CONFIG_FILE);
	 config_get_array(conf, "libretro_path", tmp_str, sizeof(tmp_str));
	 snprintf(libretro_path, sizeof(libretro_path), tmp_str);
      }

      if(!config_file_exists || !strcmp(libretro_path, ""))
         find_and_set_first_file();
      else
      {
         RARCH_LOG("Start [%s] found in retroarch.cfg.\n", libretro_path);
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
#elif defined(_XBOX1)
   strlcpy(SYS_CONFIG_FILE, "D:\\retroarch.cfg", sizeof(SYS_CONFIG_FILE));
#elif defined(__CELLOS_LV2__)
   unsigned int get_type;
   unsigned int get_attributes;
   CellGameContentSize size;
   char dirName[CELL_GAME_DIRNAME_SIZE];
   char contentInfoPath[PATH_MAX];

   memset(&size, 0x00, sizeof(CellGameContentSize));

   int ret = cellGameBootCheck(&get_type, &get_attributes, &size, dirName);
   if(ret < 0)
   {
      RARCH_ERR("cellGameBootCheck() Error: 0x%x.\n", ret);
   }
   else
   {
      RARCH_LOG("cellGameBootCheck() OK.\n");
      RARCH_LOG("Directory name: [%s].\n", dirName);
      RARCH_LOG(" HDD Free Size (in KB) = [%d] Size (in KB) = [%d] System Size (in KB) = [%d].\n", size.hddFreeSizeKB, size.sizeKB, size.sysSizeKB);

      switch(get_type)
      {
         case CELL_GAME_GAMETYPE_DISC:
            RARCH_LOG("RetroArch was launched on Optical Disc Drive.\n");
	    break;
	 case CELL_GAME_GAMETYPE_HDD:
	    RARCH_LOG("RetroArch was launched on HDD.\n");
	    break;
      }

      if((get_attributes & CELL_GAME_ATTRIBUTE_APP_HOME) == CELL_GAME_ATTRIBUTE_APP_HOME)
         RARCH_LOG("RetroArch was launched from host machine (APP_HOME).\n");

      ret = cellGameContentPermit(contentInfoPath, usrDirPath);

      if(ret < 0)
      {
         RARCH_ERR("cellGameContentPermit() Error: 0x%x\n", ret);
      }
      else
      {
         RARCH_LOG("cellGameContentPermit() OK.\n");
	 RARCH_LOG("contentInfoPath : [%s].\n", contentInfoPath);
	 RARCH_LOG("usrDirPath : [%s].\n", usrDirPath);
      }

      /* now we fill in all the variables */
      snprintf(SYS_CONFIG_FILE, sizeof(SYS_CONFIG_FILE), "%s/retroarch.cfg", usrDirPath);
      snprintf(LIBRETRO_DIR_PATH, sizeof(LIBRETRO_DIR_PATH), "%s/cores", usrDirPath);
   }
#endif
}

#ifdef __CELLOS_LV2__
//dummy - just to avoid the emitted warnings
static void callback_sysutil_exit(uint64_t status, uint64_t param, void *userdata)
{
   (void) param;
   (void) userdata;
   (void) status;
}
#endif

int main(int argc, char *argv[])
{
#if defined(_XBOX)
   XINPUT_STATE state; //C4101

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
#elif defined(__CELLOS_LV2__)
   CellPadData pad_data;

   cellSysutilRegisterCallback(0, callback_sysutil_exit, NULL);

   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_NET);

   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP);

   sys_net_initialize_network();

#ifdef HAVE_LOGGER
   logger_init();
#endif

   sceNpInit(NP_POOL_SIZE, np_pool);

   get_environment_settings();

   cellPadInit(7);

   cellPadGetData(0, &pad_data);

   if(pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE)
   {
      //override path, boot first executable in cores directory
      RARCH_LOG("Fallback - Will boot first executable in RetroArch cores/ directory.\n");
      find_and_set_first_file();
   }
   else
   {
      //normal executable loading path
      init_settings();
   }

   cellPadEnd();

#ifdef HAVE_LOGGER
   logger_shutdown();
#endif
#endif

   rarch_console_exec(libretro_path);

#ifdef __CELLOS_LV2__
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
#endif

   return 1;
}
