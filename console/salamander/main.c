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

#define MAX_PATH_LENGTH 1024

#ifdef HAVE_LOGGER
#include "logger.h"
#define RARCH_LOG(...) logger_send("RetroArch Salamander: " __VA_ARGS__);
#define RARCH_ERR(...) logger_send("RetroArch Salamander [ERROR] :: " __VA_ARGS__);
#define RARCH_WARN(...) logger_send("RetroArch Salamander [WARN] :: " __VA_ARGS__);
#else
#define RARCH_LOG(...) do { \
      fprintf(stderr, "RetroArch Salamander: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

#define RARCH_ERR(...) do { \
      fprintf(stderr, "RetroArch Salamander [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

#define RARCH_WARN(...) do { \
      fprintf(stderr, "RetroArch Salamander [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif

#include "../../file.h"

#if defined(__CELLOS_LV2__)
static uint8_t np_pool[NP_POOL_SIZE];
char contentInfoPath[MAX_PATH_LENGTH];
char usrDirPath[MAX_PATH_LENGTH];
SYS_PROCESS_PARAM(1001, 0x100000)
#elif defined(_XBOX)
DWORD volume_device_type;
#endif

char LIBRETRO_DIR_PATH[MAX_PATH_LENGTH];
char SYS_CONFIG_FILE[MAX_PATH_LENGTH];
char libretro_path[MAX_PATH_LENGTH];

static void find_and_set_first_file(void)
{
   //Last fallback - we'll need to start the first executable file 
   // we can find in the RetroArch cores directory

#if defined(_XBOX)
   char ** dir_list = dir_list_new("game:\\", ".xex");
#elif defined(__CELLOS_LV2__)
   char ** dir_list = dir_list_new(LIBRETRO_DIR_PATH, ".SELF");
#endif

   if (!dir_list)
   {
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
      return;
   }

   char * first_executable = dir_list[0];

   if(first_executable)
   {
#ifdef _XBOX
      //Check if it's RetroArch Salamander itself - if so, first_executable needs to
      //be overridden
      char fname_tmp[MAX_PATH_LENGTH];

      fill_pathname_base(fname_tmp, first_executable, sizeof(fname_tmp));

      if(strcmp(fname_tmp, "RetroArch-Salamander.xex") == 0)
      {
         RARCH_WARN("First entry is RetroArch Salamander itself, increment entry by one and check if it exists.\n");
	 first_executable = dir_list[1];
	 fill_pathname_base(fname_tmp, first_executable, sizeof(fname_tmp));

	 if(!first_executable)
	 {
            RARCH_WARN("There is no second entry - no choice but to boot RetroArch Salamander\n");
	    first_executable = dir_list[0];
	    fill_pathname_base(fname_tmp, first_executable, sizeof(fname_tmp));
	 }
      }

      snprintf(first_executable, sizeof(first_executable), "game:\\%s", fname_tmp);
#endif
      RARCH_LOG("Start first entry in libretro cores dir: [%s].\n", first_executable);
      strlcpy(libretro_path, first_executable, sizeof(libretro_path));
   }
   else
   {
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
   }

   dir_list_free(dir_list);
}

static void init_settings(void)
{
   char tmp_str[MAX_PATH_LENGTH];
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
#if defined(_XBOX)
   snprintf(core_executable, sizeof(core_executable), "game:\\CORE.xex");
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
#if defined(_XBOX)
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

   strlcpy(SYS_CONFIG_FILE, "game:\\retroarch.cfg", sizeof(SYS_CONFIG_FILE));
#elif defined(__CELLOS_LV2__)
   unsigned int get_type;
   unsigned int get_attributes;
   CellGameContentSize size;
   char dirName[CELL_GAME_DIRNAME_SIZE];

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
   int ret;
#if defined(_XBOX)
   XINPUT_STATE state;

   get_environment_settings();

   XInputGetState(0, &state);

   if(state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
   {
      //override path, boot first executable in cores directory
      RARCH_LOG("Fallback - Will boot first executable in RetroArch cores directory.\n");
      find_and_set_first_file();
   }
   else
   {
      //normal executable loading path
      init_settings();
   }

   XLaunchNewImage(libretro_path, NULL);
   RARCH_LOG("Launch libretro core: [%s] (return code: %x]).\n", libretro_path, ret);
#elif defined(__CELLOS_LV2__)
   CellPadData pad_data;
   char spawn_data[256], spawn_data_size[16];
   SceNpDrmKey * k_licensee = NULL;

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

   for(unsigned int i = 0; i < sizeof(spawn_data); ++i)
      spawn_data[i] = i & 0xff;

   sprintf(spawn_data_size, "%d", 256);

   const char * const spawn_argv[] = {
	   spawn_data_size,
	   "test argv for",
	   "sceNpDrmProcessExitSpawn2()",
	   NULL
   };

   ret = sceNpDrmProcessExitSpawn2(k_licensee, libretro_path, (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data, 256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
   RARCH_LOG("Launch libretro core: [%s] (return code: %x]).\n", libretro_path, ret);

   if(ret < 0)
   {
      RARCH_LOG("Executable file is not of NPDRM type, trying another approach to boot it...\n");
      sys_game_process_exitspawn2(libretro_path, NULL, NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
   }

   sceNpTerm();

   sys_net_finalize_network();

   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);

   cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
#endif

   return 1;
}
