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

#include <sys/process.h>

#include "../../ps3/sdk_defines.h"
#include "../menu/menu_common.h"

#include "../../conf/config_file.h"
#include "../../general.h"
#include "../../file.h"

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


#ifdef IS_SALAMANDER
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#include <cell/sysmodule.h>
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
         g_extern.system.shutdown = true;
         break;
      case CELL_SYSUTIL_OSKDIALOG_LOADED:
      case CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED:
      case CELL_SYSUTIL_OSKDIALOG_FINISHED:
      case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
         if (driver.osk && driver.osk_data)
            driver.osk->lifecycle(driver.osk_data, status);
         break;
   }
#endif
}
#endif

static void frontend_ps3_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
#ifndef IS_SALAMANDER
   bool original_verbose = g_extern.verbosity;
   g_extern.verbosity = true;
#endif

   (void)args;
#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   g_extern.log_file = fopen("/retroarch-log.txt", "w");
#endif
#endif

   int ret;
   unsigned int get_type;
   unsigned int get_attributes;
   CellGameContentSize size;
   char dirName[CELL_GAME_DIRNAME_SIZE];
   char contentInfoPath[PATH_MAX];

#ifdef HAVE_MULTIMAN
   /* not launched from external launcher, set default path */
   // second param is multiMAN SELF file
   if(path_file_exists(argv[2]) && *argc > 1
         && (strcmp(argv[2], EMULATOR_CONTENT_DIR) == 0))
   {
      multiman_detected = true;
      RARCH_LOG("Started from multiMAN, auto-game start enabled.\n");
   }
   else
#endif
#ifndef IS_SALAMANDER
      if (*argc > 1 && argv[1] != NULL && argv[1][0] != '\0')
      {
         static char path[PATH_MAX];
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

      ret = cellGameContentPermit(contentInfoPath, g_defaults.port_dir);

#ifdef HAVE_MULTIMAN
      if (multiman_detected)
      {
         fill_pathname_join(contentInfoPath, "/dev_hdd0/game/",
               EMULATOR_CONTENT_DIR, sizeof(contentInfoPath));
         fill_pathname_join(g_defaults.port_dir, contentInfoPath,
               "USRDIR", sizeof(g_defaults.port_dir));
      }
#endif

      if(ret < 0)
         RARCH_ERR("cellGameContentPermit() Error: 0x%x\n", ret);
      else
      {
         RARCH_LOG("cellGameContentPermit() OK.\n");
         RARCH_LOG("contentInfoPath : [%s].\n", contentInfoPath);
         RARCH_LOG("usrDirPath : [%s].\n", g_defaults.port_dir);
      }

      fill_pathname_join(g_defaults.core_dir, g_defaults.port_dir,
            "cores", sizeof(g_defaults.core_dir));
      fill_pathname_join(g_defaults.core_info_dir, g_defaults.port_dir,
            "cores", sizeof(g_defaults.core_info_dir));
      fill_pathname_join(g_defaults.savestate_dir, g_defaults.core_dir,
            "savestates", sizeof(g_defaults.savestate_dir));
      fill_pathname_join(g_defaults.sram_dir, g_defaults.core_dir,
            "savefiles", sizeof(g_defaults.sram_dir));
      fill_pathname_join(g_defaults.system_dir, g_defaults.core_dir,
            "system", sizeof(g_defaults.system_dir));
      fill_pathname_join(g_defaults.shader_dir,  g_defaults.core_dir,
            "shaders", sizeof(g_defaults.shader_dir));
      fill_pathname_join(g_defaults.config_path, g_defaults.port_dir,
            "retroarch.cfg",  sizeof(g_defaults.config_path));
      fill_pathname_join(g_defaults.overlay_dir, g_defaults.core_dir,
            "overlays", sizeof(g_defaults.overlay_dir));
      fill_pathname_join(g_defaults.assets_dir,   g_defaults.core_dir,
            "media", sizeof(g_defaults.assets_dir));
      fill_pathname_join(g_defaults.playlist_dir,   g_defaults.core_dir,
            "playlists", sizeof(g_defaults.playlist_dir));
   }

#ifndef IS_SALAMANDER
   g_extern.verbosity = original_verbose;
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

static void frontend_ps3_exec(const char *path, bool should_load_game);

static void frontend_ps3_exitspawn(char *core_path, size_t core_path_size)
{
#ifdef HAVE_RARCH_EXEC
   bool should_load_game = false;

#ifndef IS_SALAMANDER
   bool original_verbose = g_extern.verbosity;
   g_extern.verbosity = true;

   if (g_extern.lifecycle_state & (1ULL << MODE_EXITSPAWN_START_GAME))
      should_load_game = true;
#endif

   frontend_ps3_exec(core_path, should_load_game);

#ifdef IS_SALAMANDER
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
#else

#endif

#ifndef IS_SALAMANDER
   g_extern.verbosity = original_verbose;
#endif
#endif
}

#include <stdio.h>

#include <cell/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>

#include "../../retroarch_logger.h"

static void frontend_ps3_exec(const char *path, bool should_load_game)
{
   (void)should_load_game;
   char spawn_data[256];
#ifndef IS_SALAMANDER
   bool original_verbose = g_extern.verbosity;
   g_extern.verbosity = true;

   char game_path[256];
   game_path[0] = '\0';
#endif

   RARCH_LOG("Attempt to load executable: [%s].\n", path);

   for(unsigned int i = 0; i < sizeof(spawn_data); ++i)
      spawn_data[i] = i & 0xff;

   SceNpDrmKey * k_licensee = NULL;
   int ret;
#ifdef IS_SALAMANDER
   const char * const spawn_argv[] = { NULL};

   ret = sceNpDrmProcessExitSpawn2(k_licensee, path,
         (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data,
         256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

   if(ret <  0)
   {
      RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
      sys_game_process_exitspawn(path, (const char** const)spawn_argv,
            NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
   }
#else
   if (should_load_game && g_extern.fullpath[0] != '\0')
   {
      strlcpy(game_path, g_extern.fullpath, sizeof(game_path));

      const char * const spawn_argv[] = {
         game_path,
         NULL
      };

      ret = sceNpDrmProcessExitSpawn2(k_licensee, path,
            (const char** const)spawn_argv, NULL,
            (sys_addr_t)spawn_data, 256, 1000,
            SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

      if(ret <  0)
      {
         RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
         sys_game_process_exitspawn(path, (const char** const)spawn_argv,
               NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      }
   }
   else
   {
      const char * const spawn_argv[] = {NULL}; 
      ret = sceNpDrmProcessExitSpawn2(k_licensee, path,
            (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data,
            256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

      if(ret <  0)
      {
         RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
         sys_game_process_exitspawn(path, (const char** const)spawn_argv,
               NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      }
   }
#endif

   sceNpTerm();
   sys_net_finalize_network();
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);

#ifndef IS_SALAMANDER
   g_extern.verbosity = original_verbose;
#endif
}

static int frontend_ps3_get_rating(void)
{
   return 10;
}

const frontend_ctx_driver_t frontend_ctx_ps3 = {
   frontend_ps3_get_environment_settings,     /* get_environment_settings */
   frontend_ps3_init,            /* init */
   frontend_ps3_deinit,          /* deinit */
   frontend_ps3_exitspawn,       /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   frontend_ps3_exec,            /* exec */
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   frontend_ps3_get_rating,      /* get_rating */
   "ps3",
};
