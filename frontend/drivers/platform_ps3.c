/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <compat/strl.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include "../frontend_driver.h"
#include "../../file_path_special.h"
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
static enum frontend_fork ps3_fork_mode = FRONTEND_FORK_NONE;

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

            rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
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
   bool original_verbose = verbosity_is_enabled();
   verbosity_enable();
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
   if (     path_is_valid(argv[2]) && *argc > 1
         && (string_is_equal(argv[2], EMULATOR_CONTENT_DIR)))
   {
      multiman_detected = true;
      RARCH_LOG("Started from multiMAN, auto-game start enabled.\n");
   }
   else
#endif
#ifndef IS_SALAMANDER
      if (*argc > 1 && !string_is_empty(argv[1]))
      {
         static char path[PATH_MAX_LENGTH] = {0};
         struct rarch_main_wrap      *args = (struct rarch_main_wrap*)params_data;

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

      ret = cellGameContentPermit(content_info_path, g_defaults.dirs[DEFAULT_DIR_PORT]);

#ifdef HAVE_MULTIMAN
      if (multiman_detected)
      {
         fill_pathname_join(content_info_path, "/dev_hdd0/game/",
               EMULATOR_CONTENT_DIR, sizeof(content_info_path));
         fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PORT], content_info_path,
               "USRDIR", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
      }
#endif

      if(ret < 0)
         RARCH_ERR("cellGameContentPermit() Error: 0x%x\n", ret);
      else
      {
         RARCH_LOG("cellGameContentPermit() OK.\n");
         RARCH_LOG("content_info_path : [%s].\n", content_info_path);
         RARCH_LOG("usrDirPath : [%s].\n", g_defaults.dirs[DEFAULT_DIR_PORT]);
      }

      strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
            g_defaults.dirs[DEFAULT_DIR_PORT],
            sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE],
            g_defaults.dirs[DEFAULT_DIR_PORT],
            "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "info",
            sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "shaders_cg", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
      fill_pathname_join(g_defaults.path.config, g_defaults.dirs[DEFAULT_DIR_PORT],
            file_path_str(FILE_PATH_MAIN_CONFIG),  sizeof(g_defaults.path.config));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS],
            g_defaults.dirs[DEFAULT_DIR_CORE], "cheats",
            sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS],
            g_defaults.dirs[DEFAULT_DIR_CORE],
            "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   }

#ifndef IS_SALAMANDER
   if (original_verbose)
      verbosity_enable();
   else
      verbosity_disable();
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
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         ps3_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         ps3_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         ps3_fork_mode  = FRONTEND_FORK_CORE;
         break;
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
   bool original_verbose = verbosity_is_enabled();
   verbosity_enable();
#endif

   (void)should_load_game;

   RARCH_LOG("Attempt to load executable: [%s].\n", path);

#ifndef IS_SALAMANDER
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      char game_path[256];
      strlcpy(game_path, path_get(RARCH_PATH_CONTENT), sizeof(game_path));

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
   if (original_verbose)
      verbosity_enable();
   else
      verbosity_disable();
#endif
}

static void frontend_ps3_exitspawn(char *core_path, size_t core_path_size)
{
#ifdef HAVE_RARCH_EXEC
   bool should_load_game = false;

#ifndef IS_SALAMANDER
   bool original_verbose = verbosity_is_enabled();

   verbosity_enable();

   if (ps3_fork_mode == FRONTEND_FORK_NONE)
   {
      frontend_ctx_driver_t *frontend = frontend_get_ptr();

      if (frontend)
         frontend->shutdown = frontend_ps3_shutdown;
      return;
   }

   switch (ps3_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }
#endif

   frontend_ps3_exec(core_path, should_load_game);

#ifdef IS_SALAMANDER
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
#endif

#ifndef IS_SALAMANDER
   if (original_verbose)
      verbosity_enable();
   else
      verbosity_disable();
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

static int frontend_ps3_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

   menu_entries_append_enum(list,
         "/app_home/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_hdd0/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_hdd1/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_bdvd/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/host_root/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_usb000/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_usb001/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_usb002/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_usb003/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_usb004/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_usb005/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/dev_usb006/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif

   return 0;
}

static void frontend_ps3_process_args(int *argc, char *argv[])
{
#ifndef IS_SALAMANDER
   /* A big hack: sometimes Salamander doesn't save the new core
    * it loads on first boot, so we make sure
    * active core path is set here. */

   if (*argc >= 1)
   {
      char path[PATH_MAX_LENGTH] = {0};
      strlcpy(path, argv[0], sizeof(path));
      if (path_is_valid(path))
         path_set(RARCH_PATH_CORE, path);
   }
#endif
}

frontend_ctx_driver_t frontend_ctx_ps3 = {
   frontend_ps3_get_environment_settings,
   frontend_ps3_init,
   frontend_ps3_deinit,
   frontend_ps3_exitspawn,
   frontend_ps3_process_args,
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
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_free */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   "ps3",
};
