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
#if !defined(__PSL1GHT__)
#include <sysutil/sysutil_common.h>
#endif
#if defined (IS_SALAMANDER) && !defined(__PSL1GHT__)
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#include <cell/sysmodule.h>
#endif

#if defined(__PSL1GHT__)
#include <lv2/process.h>
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
#include <defines/ps3_defines.h>

#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../verbosity.h"

#if !defined(IS_SALAMANDER) && defined(HAVE_NETWORKING)
#include "../../network/netplay/netplay.h"
#endif

#ifdef __PSL1GHT__
#define EMULATOR_CONTENT_DIR "SSNE10001"
#else
#define EMULATOR_CONTENT_DIR "SSNE10000"
#endif

#ifndef __PSL1GHT__
#define NP_POOL_SIZE (128*1024)
static uint8_t np_pool[NP_POOL_SIZE];
#endif

#ifdef IS_SALAMANDER
SYS_PROCESS_PARAM(1001, 0x100000)
#else
SYS_PROCESS_PARAM(1001, 0x200000)
#endif

#ifndef __PSL1GHT__
#ifdef HAVE_MULTIMAN
#define MULTIMAN_SELF_FILE "/dev_hdd0/game/BLES80608/USRDIR/RELOAD.SELF"
static bool multiman_detected  = false;
#endif
#endif

#ifdef HAVE_MEMINFO
typedef struct {
   uint32_t total;
   uint32_t avail;
} sys_memory_info_t;
#ifdef __PSL1GHT__
#define sys_memory_get_user_memory_size(x) lv2syscall1(352, x)
#else
#define sys_memory_get_user_memory_size(x) system_call_1(352, x)
#endif
#endif

#ifndef IS_SALAMANDER
static enum frontend_fork ps3_fork_mode = FRONTEND_FORK_NONE;

static void frontend_ps3_shutdown(bool unused)
{
   sysProcessExit(0);
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

            retroarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
         }
         break;
   }
#endif
}
#endif

static void fill_derived_paths(void)
{
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
    fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
		       g_defaults.dirs[DEFAULT_DIR_CORE],
		       "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
    fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
		       g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
		       "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
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
    fill_pathname_join(g_defaults.path_config, g_defaults.dirs[DEFAULT_DIR_PORT],
		       FILE_PATH_MAIN_CONFIG,  sizeof(g_defaults.path_config));
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

static void use_app_path(char *content_info_path)
{
    fill_pathname_join(content_info_path, "/dev_hdd0/game/",
		       EMULATOR_CONTENT_DIR, PATH_MAX_LENGTH);
    fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PORT], content_info_path,
		       "USRDIR", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
}

static void frontend_ps3_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   char content_info_path[PATH_MAX_LENGTH];
#ifndef __PSL1GHT__
   CellGameContentSize size;
   unsigned int type, attributes;
   char dirname[CELL_GAME_DIRNAME_SIZE];
#ifndef IS_SALAMANDER
   struct rarch_main_wrap *params = (struct rarch_main_wrap*)params_data;
#endif
#endif
#ifndef IS_SALAMANDER
   bool verbosity = verbosity_is_enabled();

   verbosity_enable();

#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
#ifdef __PSL1GHT__
   retro_main_log_file_init("/dev_hdd0/game/"
      EMULATOR_CONTENT_DIR "/USRDIR/retroarch-log.txt", false);
#else
   retro_main_log_file_init("/dev_hdd0/retroarch-log.txt", false);
#endif
#endif
#elif defined(__PSL1GHT__)
#ifdef HAVE_FILE_LOGGER
   retro_main_log_file_init("/dev_hdd0/game/"
      EMULATOR_CONTENT_DIR "/USRDIR/retroarch-log-salamander.txt", false);
#endif
#endif

#ifdef __PSL1GHT__
   use_app_path(content_info_path);
#else
   memset(&size, 0, sizeof(size));
   cellGameBootCheck(&type, &attributes, &size, dirname);
   cellGameContentPermit(content_info_path,
      g_defaults.dirs[DEFAULT_DIR_PORT]);

#ifdef HAVE_MULTIMAN
   /* Not launched from external launcher, set default path.
      Second parameter is multiMAN SELF file. */
   if (*argc > 2 && string_is_equal(argv[2], EMULATOR_CONTENT_DIR) &&
         path_is_valid(argv[2]))
   {
      multiman_detected = true;
      use_app_path(content_info_path);
   }
#ifndef IS_SALAMANDER
   else
#endif
#endif
#ifndef IS_SALAMANDER
   if (params && *argc > 1 && !string_is_empty(argv[1]))
#ifdef HAVE_NETWORKING
   /* If the process was forked for netplay purposes,
      DO NOT touch the arguments. */
   if (!string_is_equal(argv[1], "-H") && !string_is_equal(argv[1], "-C"))
#endif
   {
      params->content_path  = argv[1];
      params->sram_path     = NULL;
      params->state_path    = NULL;
      params->config_path   = NULL;
      params->libretro_path = NULL;
      params->flags        &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
                              | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
      params->flags        |=   RARCH_MAIN_WRAP_FLAG_TOUCHED;
   }
#endif
#endif

   fill_derived_paths();

#ifndef IS_SALAMANDER
   if (verbosity)
      verbosity_enable();
   else
      verbosity_disable();

   dir_check_defaults("custom.ini");
#endif
}

static void frontend_ps3_init(void *data)
{
   (void)data;
#ifdef HAVE_SYSUTILS
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

#ifdef HAVE_LIGHTGUN
   cellSysmoduleLoadModule(SYSMODULE_GEM);
   cellSysmoduleLoadModule(SYSMODULE_CAMERA);
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
         ps3_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         ps3_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
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
#ifndef __PSL1GHT__
   SceNpDrmKey *license_data = NULL;
#endif

   for (i = 0; i < sizeof(spawn_data); ++i)
      spawn_data[i] = i & 0xff;

#ifndef __PSL1GHT__
   ret = sceNpDrmProcessExitSpawn(license_data, path,
         (const char** const)argv, envp, (sys_addr_t)spawn_data,
         256, 1000, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
#else
   ret = -1;
#endif

   if (ret <  0)
   {
      RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
      sysProcessExitSpawn2(path, (const char** const)argv,
            envp, NULL, 0, 1000, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
   }

   return ret;
}

static void frontend_ps3_exec(const char *path, bool should_load_game)
{
#ifndef IS_SALAMANDER
#ifdef HAVE_NETWORKING
   char *arg_data[NETPLAY_FORK_MAX_ARGS];
#else
   char *arg_data[2];
#endif
   char game_path[PATH_MAX_LENGTH];
   bool verbosity = verbosity_is_enabled();

   verbosity_enable();
#else
   char *arg_data[1];
#endif

   arg_data[0] = NULL;

#ifndef IS_SALAMANDER
   if (should_load_game)
   {
      const char *content = path_get(RARCH_PATH_CONTENT);

#ifdef HAVE_NETWORKING
      if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_GET_FORK_ARGS,
            (void*)arg_data))
#endif
      if (!string_is_empty(content))
      {
         strlcpy(game_path, content, sizeof(game_path));
         arg_data[0] = game_path;
         arg_data[1] = NULL;
      }
   }
#endif

   frontend_ps3_exec_exitspawn(path, arg_data[0] ? arg_data : NULL, NULL);

#ifndef __PSL1GHT__
   sceNpTerm();
#endif
   sys_net_finalize_network();
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);

#ifndef IS_SALAMANDER
   if (verbosity)
      verbosity_enable();
   else
      verbosity_disable();
#endif
}

static void frontend_ps3_exitspawn(char *s, size_t len, char *args)
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

   frontend_ps3_exec(s, should_load_game);

#ifdef IS_SALAMANDER
#ifndef __PSL1GHT__
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
#endif
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

enum frontend_architecture frontend_ps3_get_arch(void)
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

   menu_entries_append(list,
         "/app_home/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_hdd0/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_hdd1/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_bdvd/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/host_root/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_usb000/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_usb001/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_usb002/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_usb003/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_usb004/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_usb005/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "/dev_usb006/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
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

#ifdef HAVE_MEMINFO
static size_t frontend_ps3_get_mem_total(void)
{
   sys_memory_info_t mem_info;
   sys_memory_get_user_memory_size(&mem_info);
   return mem_info.total;
}

static size_t frontend_ps3_get_mem_used(void)
{
   sys_memory_info_t mem_info;
   sys_memory_get_user_memory_size(&mem_info);
   return mem_info.avail;
}
#endif

frontend_ctx_driver_t frontend_ctx_ps3 = {
   frontend_ps3_get_env,
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
   frontend_ps3_get_rating,      /* get_rating */
   NULL,                         /* load_content */
   frontend_ps3_get_arch,        /* get_architecture */
   NULL,                         /* get_powerstate */
   frontend_ps3_parse_drive_list,/* parse_drive_list */
#ifdef HAVE_MEMINFO
   frontend_ps3_get_mem_total,
   frontend_ps3_get_mem_used,
#else
   NULL,                         /* get_total_mem */
   NULL,                         /* get_free_mem */
#endif
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* get_lakka_version */
   NULL,                         /* set_screen_brightness */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   NULL,                         /* is_narrator_running */
   NULL,                         /* accessibility_speak */
   NULL,                         /* set_gamemode */
   "ps3",                        /* ident */
   NULL                          /* get_video_driver */
};
