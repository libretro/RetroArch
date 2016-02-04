/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <stdio.h>

#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#ifdef IS_SALAMANDER
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#include <cell/sysmodule.h>
#endif

#include <sys/process.h>

#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <file/file_list.h>
#endif

#include "../frontend_driver.h"
#include "../../defines/ps3_defines.h"
#include "../../defaults.h"
#include "../../verbosity.h"

#define EMULATOR_CONTENT_DIR "SSNE10000"

#ifndef __PSL1GHT__
#define NP_POOL_SIZE (128*1024)
static uint8_t np_pool[NP_POOL_SIZE];
#endif

#ifdef IS_SALAMANDER
SYS_PROCESS_PARAM(1001, 0x100000)
#else
SYS_PROCESS_PARAM(1001, 0x200000)
#endif
   
#ifdef HAVE_MULTIMAN
#define MULTIMAN_SELF_FILE "/dev_hdd0/game/BLES80608/USRDIR/RELOAD.SELF"
static bool multiman_detected  = false;
#endif

#ifndef IS_SALAMANDER
static bool exit_spawn = false;
static bool exitspawn_start_game = false;

static void frontend_ps3_shutdown(bool unused)
{
   sys_process_exit(0);
}
#endif

#ifdef HAVE_SYSUTILS
static void callback_sysutil_exit(uint64_t status,
      uint64_t param, void *userdata)
{

   (void)param;
   (void)userdata;
   (void)status;

#ifndef IS_SALAMANDER

   switch (status)
   {
      case CELL_SYSUTIL_REQUEST_EXITGAME:
         {
            frontend_ctx_driver_t *frontend = frontend_get_ptr();

            if (frontend)
               frontend->shutdown = frontend_ps3_shutdown;

            runloop_ctl(RUNLOOP_CTL_SET_SHUTDOWN, NULL);
         }
         break;
   }
#endif
}
#endif

static void frontend_ps3_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
#ifndef IS_SALAMANDER
   bool *verbose         = retro_main_verbosity();
   bool original_verbose = *verbose;
   *verbose              = true;
#endif

   (void)args;
#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   retro_main_log_file_init("/retroarch-log.txt");
#endif
#endif

   int ret;
   unsigned int get_type;
   unsigned int get_attributes;
   CellGameContentSize size;
   char dirName[CELL_GAME_DIRNAME_SIZE]  = {0};

#ifdef HAVE_MULTIMAN
   /* not launched from external launcher, set default path */
   // second param is multiMAN SELF file
   if(path_file_exists(argv[2]) && *argc > 1
         && (!strcmp(argv[2], EMULATOR_CONTENT_DIR)))
   {
      multiman_detected = true;
      RARCH_LOG("Started from multiMAN, auto-game start enabled.\n");
   }
   else
#endif
#ifndef IS_SALAMANDER
      if (*argc > 1 && !string_is_empty(argv[1]))
      {
         static char path[PATH_MAX_LENGTH];
         *path = '\0';
         struct rarch_main_wrap *args = (struct rarch_main_wrap*)params_data;

         if (args)
         {
            strlcpy(path, argv[1], sizeof(path));

            args->touched        = true;
            args->no_content     = false;
            args->verbose        = false;
            args->config_path    = NULL;
            args->sram_path      = NULL;
            args->state_path     = NULL;
            args->content_path   = path;
            args->libretro_path  = NULL;

            RARCH_LOG("argv[0]: %s\n", argv[0]);
            RARCH_LOG("argv[1]: %s\n", argv[1]);
            RARCH_LOG("argv[2]: %s\n", argv[2]);

            RARCH_LOG("Auto-start game %s.\n", argv[1]);
         }
      }
      else
         RARCH_WARN("Started from Salamander, auto-game start disabled.\n");
#endif

   memset(&size, 0x00, sizeof(CellGameContentSize));

   ret = cellGameBootCheck(&get_type, &get_attributes, &size, dirName);
   if(ret < 0)
   {
      RARCH_ERR("cellGameBootCheck() Error: 0x%x.\n", ret);
   }
   else
   {
      char content_info_path[PATH_MAX_LENGTH] = {0};

      RARCH_LOG("cellGameBootCheck() OK.\n");
      RARCH_LOG("Directory name: [%s].\n", dirName);
      RARCH_LOG(" HDD Free Size (in KB) = [%d] Size (in KB) = [%d] System Size (in KB) = [%d].\n",
            size.hddFreeSizeKB, size.sizeKB, size.sysSizeKB);

      switch(get_type)
      {
         case CELL_GAME_GAMETYPE_DISC:
            RARCH_LOG("RetroArch was launched on Optical Disc Drive.\n");
            break;
         case CELL_GAME_GAMETYPE_HDD:
            RARCH_LOG("RetroArch was launched on HDD.\n");
            break;
      }

      if((get_attributes & CELL_GAME_ATTRIBUTE_APP_HOME) 
            == CELL_GAME_ATTRIBUTE_APP_HOME)
         RARCH_LOG("RetroArch was launched from host machine (APP_HOME).\n");

      ret = cellGameContentPermit(content_info_path, g_defaults.dir.port);

#ifdef HAVE_MULTIMAN
      if (multiman_detected)
      {
         fill_pathname_join(content_info_path, "/dev_hdd0/game/",
               EMULATOR_CONTENT_DIR, sizeof(content_info_path));
         fill_pathname_join(g_defaults.dir.port, content_info_path,
               "USRDIR", sizeof(g_defaults.dir.port));
      }
#endif

      if(ret < 0)
         RARCH_ERR("cellGameContentPermit() Error: 0x%x\n", ret);
      else
      {
         RARCH_LOG("cellGameContentPermit() OK.\n");
         RARCH_LOG("content_info_path : [%s].\n", content_info_path);
         RARCH_LOG("usrDirPath : [%s].\n", g_defaults.dir.port);
      }

      fill_pathname_join(g_defaults.dir.core, g_defaults.dir.port,
            "cores", sizeof(g_defaults.dir.core));
      fill_pathname_join(g_defaults.dir.core_info, g_defaults.dir.port,
            "cores", sizeof(g_defaults.dir.core_info));
      fill_pathname_join(g_defaults.dir.savestate, g_defaults.dir.core,
            "savestates", sizeof(g_defaults.dir.savestate));
      fill_pathname_join(g_defaults.dir.sram, g_defaults.dir.core,
            "savefiles", sizeof(g_defaults.dir.sram));
      fill_pathname_join(g_defaults.dir.system, g_defaults.dir.core,
            "system", sizeof(g_defaults.dir.system));
      fill_pathname_join(g_defaults.dir.shader,  g_defaults.dir.core,
            "shaders_cg", sizeof(g_defaults.dir.shader));
      fill_pathname_join(g_defaults.path.config, g_defaults.dir.port,
            "retroarch.cfg",  sizeof(g_defaults.path.config));
      fill_pathname_join(g_defaults.dir.overlay, g_defaults.dir.core,
            "overlays", sizeof(g_defaults.dir.overlay));
      fill_pathname_join(g_defaults.dir.assets,   g_defaults.dir.core,
            "assets", sizeof(g_defaults.dir.assets));
      fill_pathname_join(g_defaults.dir.cursor,   g_defaults.dir.core,
            "database/cursors", sizeof(g_defaults.dir.cursor));
      fill_pathname_join(g_defaults.dir.database,   g_defaults.dir.core,
            "database/rdb", sizeof(g_defaults.dir.database));
      fill_pathname_join(g_defaults.dir.playlist,   g_defaults.dir.core,
            "playlists", sizeof(g_defaults.dir.playlist));
   }

#ifndef IS_SALAMANDER
   *verbose = original_verbose;
#endif
}

static void frontend_ps3_init(void *data)
{
   (void)data;
#ifdef HAVE_SYSUTILS
   RARCH_LOG("Registering system utility callback...\n");
   cellSysutilRegisterCallback(0, callback_sysutil_exit, NULL);
#endif

#ifdef HAVE_SYSMODULES

#ifdef HAVE_FREETYPE
   cellSysmoduleLoadModule(CELL_SYSMODULE_FONT);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FREETYPE);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FONTFT);
#endif

   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
#ifndef __PSL1GHT__
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);
#endif
#ifndef IS_SALAMANDER
#ifndef __PSL1GHT__
   cellSysmoduleLoadModule(CELL_SYSMODULE_AVCONF_EXT);
#endif
   cellSysmoduleLoadModule(CELL_SYSMODULE_PNGDEC);
   cellSysmoduleLoadModule(CELL_SYSMODULE_JPGDEC);
#endif
   cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP);
#endif

#ifndef __PSL1GHT__
   sys_net_initialize_network();
   sceNpInit(NP_POOL_SIZE, np_pool);
#endif

#ifndef IS_SALAMANDER
#if (CELL_SDK_VERSION > 0x340000) && !defined(__PSL1GHT__)
#ifdef HAVE_SYSMODULES
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
#endif
#ifdef HAVE_SYSUTILS
   CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

   screenshot_param.photo_title = "RetroArch PS3";
   screenshot_param.game_title = "RetroArch PS3";
   cellScreenShotSetParameter (&screenshot_param);
   cellScreenShotEnable();
#endif
#endif
#endif
}

static void frontend_ps3_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER

#if defined(HAVE_SYSMODULES)
#ifdef HAVE_FREETYPE
   /* Freetype font PRX */
   cellSysmoduleLoadModule(CELL_SYSMODULE_FONTFT);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_FREETYPE);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_FONT);
#endif

#ifndef __PSL1GHT__
   /* screenshot PRX */
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
#endif

   cellSysmoduleUnloadModule(CELL_SYSMODULE_JPGDEC);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_PNGDEC);

#ifndef __PSL1GHT__
   /* system game utility PRX */
   cellSysmoduleUnloadModule(CELL_SYSMODULE_AVCONF_EXT);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
#endif

#endif

#endif
}

#ifndef IS_SALAMANDER
static bool frontend_ps3_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         exit_spawn = true;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         exit_spawn = true;
         exitspawn_start_game = true;
         break;
      case FRONTEND_FORK_SALAMANDER_RESTART:
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static int frontend_ps3_exec_exitspawn(const char *path,
      char const *argv[], char const *envp[])
{
   int ret;
   unsigned i;
   char spawn_data[256];
   SceNpDrmKey *license_data = NULL;

   for(i = 0; i < sizeof(spawn_data); ++i)
      spawn_data[i] = i & 0xff;

   ret = sceNpDrmProcessExitSpawn(license_data, path,
         (const char** const)argv, envp, (sys_addr_t)spawn_data,
         256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

   if(ret <  0)
   {
      RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
      sys_game_process_exitspawn(path, (const char** const)argv,
            envp, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
   }

   return ret;
}

static void frontend_ps3_exec(const char *path, bool should_load_game)
{
#ifndef IS_SALAMANDER
   char *fullpath        = NULL;
   bool *verbose         = retro_main_verbosity();
   bool original_verbose = *verbose;

   *verbose              = true;
#endif

   (void)should_load_game;

   RARCH_LOG("Attempt to load executable: [%s].\n", path);

#ifndef IS_SALAMANDER
   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (should_load_game && !string_is_empty(fullpath))
   {
      char game_path[256];
      strlcpy(game_path, fullpath, sizeof(game_path));

      const char * const spawn_argv[] = {
         game_path,
         NULL
      };

      frontend_ps3_exec_exitspawn(path,
            (const char** const)spawn_argv, NULL);
   }
   else
#endif
   {
      frontend_ps3_exec_exitspawn(path,
            NULL, NULL);
   }

   sceNpTerm();
   sys_net_finalize_network();
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);

#ifndef IS_SALAMANDER
   *verbose = original_verbose;
#endif
}

static void frontend_ps3_exitspawn(char *core_path, size_t core_path_size)
{
#ifdef HAVE_RARCH_EXEC
   bool should_load_game = false;

#ifndef IS_SALAMANDER
   bool *verbose         = retro_main_verbosity();
   bool original_verbose = *verbose;

   *verbose              = true;

   should_load_game = exitspawn_start_game;

   if (!exit_spawn)
   {
      frontend_ctx_driver_t *frontend = frontend_get_ptr();

      if (frontend)
         frontend->shutdown = frontend_ps3_shutdown;
      return;
   }
#endif

   frontend_ps3_exec(core_path, should_load_game);

#ifdef IS_SALAMANDER
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
#endif

#ifndef IS_SALAMANDER
   *verbose = original_verbose;
#endif
#endif
}

static int frontend_ps3_get_rating(void)
{
   return 10;
}

enum frontend_architecture frontend_ps3_get_architecture(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_ps3_parse_drive_list(void *data)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;

   menu_entries_push(list,
         "/app_home/",   "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_hdd0/",   "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_hdd1/",   "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/host_root/",  "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_usb000/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_usb001/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_usb002/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_usb003/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_usb004/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_usb005/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "/dev_usb006/", "", MENU_FILE_DIRECTORY, 0, 0);
#endif

   return 0;
}

frontend_ctx_driver_t frontend_ctx_ps3 = {
   frontend_ps3_get_environment_settings,
   frontend_ps3_init,
   frontend_ps3_deinit,
   frontend_ps3_exitspawn,
   NULL,                         /* process_args */
   frontend_ps3_exec,
#ifdef IS_SALAMANDER
   NULL,
#else
   frontend_ps3_set_fork,
#endif
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_ps3_get_rating,
   NULL,                         /* load_content */
   frontend_ps3_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_ps3_parse_drive_list,
   "ps3",
};
