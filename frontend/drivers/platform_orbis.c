/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#if defined(HAVE_OOSDK)
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>
#elif defined(HAVE_LIBORBIS)
#include <kernel.h>
#include <systemservice.h>
#include <orbis2d.h>
#include <orbisPad.h>
#include <orbisAudio.h>
#include <modplayer.h>
#include <ps4link.h>
#include <orbisKeyboard.h>
#include <debugnet.h>
#include <orbisFile.h>
#endif

#include <signal.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <libSceUserService.h>
#include <libSceSystemService.h>
#include <libSceSysmodule.h>
#include <defines/ps4_defines.h>

#include "../../memory/ps4/user_mem.h"

#include <pthread.h>

#include <string/stdstring.h>
#include <boolean.h>
#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include "../../retroarch.h"
#include "../../paths.h"
#include "../../verbosity.h"

#if defined(HAVE_LIBORBIS)
#define CONTENT_PATH_ARG_INDEX 2
#define EBOOT_PATH "host0:app"
#define USER_PATH "host0:app/data/retroarch/"
#define CORE_PATH EBOOT_PATH
#define CORE_DIR ""
#define CORE_INFO_PATH EBOOT_PATH
#else
#define CONTENT_PATH_ARG_INDEX 1
#define EBOOT_PATH "/app0/"
#define USER_PATH "/data/retroarch/"
#define CORE_DIR "cores"
#define CORE_INFO_PATH USER_PATH
#if defined(BUNDLE_CORES)
#define CORE_PATH EBOOT_PATH
#else
#define CORE_PATH "/data/self/retroarch/"
#endif
#endif
#define MODULE_PATH "/data/self/system/common/lib/"
#define MODULE_PATH_EXT "/app0/sce_module/"

#if defined(HAVE_LIBORBIS)
typedef struct OrbisGlobalConf
{
	Orbis2dConfig *conf;
	OrbisPadConfig *confPad;
	OrbisAudioConfig *confAudio;
	OrbisKeyboardConfig *confKeyboard;
	ps4LinkConfiguration *confLink;
	int orbisLinkFlag;
}OrbisGlobalConf;

OrbisGlobalConf *myConf;
#endif

#if defined(HAVE_OOSDK)
FILE _Stdin, _Stderr, _Stdout;
#endif
char eboot_path[512];
char user_path[512];
SceKernelModule s_piglet_module;
SceKernelModule s_shacc_module;

static enum frontend_fork orbis_fork_mode = FRONTEND_FORK_NONE;

#if defined(HAVE_TAUON_SDK)
void catchReturnFromMain(int exit_code)
{
  kill(getpid(), SIGTERM);
}
#endif

static void frontend_orbis_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   unsigned i;
   struct rarch_main_wrap *params = NULL;

   (void)args;

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
#if defined(HAVE_LIBORBIS)
   retro_main_log_file_init("host0:app/temp/retroarch-log.txt");
#else
   retro_main_log_file_init("/data/retroarch/temp/retroarch-log.txt");
#endif
#endif
#endif

   int ret;

   sceSystemServiceHideSplashScreen();

#if defined(HAVE_LIBORBIS)
	uintptr_t intptr=0;
	sscanf(argv[1],"%p",&intptr);
   argv[1] = NULL;
	myConf=(OrbisGlobalConf *)intptr;
	ret=ps4LinkInitWithConf(myConf->confLink);
	if(!ret)
	{
		ps4LinkFinish();
		return;
	}
   orbisFileInit();
   orbisPadInitWithConf(myConf->confPad);
   scePadClose(myConf->confPad->padHandle);
#else
   // SceUserServiceInitializeParams param;
   // memset(&param, 0, sizeof(param));
   // param.priority = SCE_KERNEL_PRIO_FIFO_DEFAULT;
   // sceUserServiceInitialize(&param);
#endif

   strlcpy(eboot_path, EBOOT_PATH, sizeof(eboot_path));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_PORT], eboot_path, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   strlcpy(user_path, USER_PATH, sizeof(user_path));

   RARCH_LOG("port dir: [%s]\n", g_defaults.dirs[DEFAULT_DIR_PORT]);

   /* bundle data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], CORE_PATH,
         CORE_DIR, sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], CORE_INFO_PATH,
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   /* user data*/
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], user_path,
         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], user_path,
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], user_path,
         "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], user_path,
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
         "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
         "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], user_path,
	       "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], user_path,
         "temp", sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], user_path,
         "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], user_path,
         "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], user_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path,
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
         user_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path_config, user_path,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));

#ifndef IS_SALAMANDER
   params          = (struct rarch_main_wrap*)params_data;
   params->verbose = true;

   if (!string_is_empty(argv[CONTENT_PATH_ARG_INDEX]))
   {
      static char path[PATH_MAX_LENGTH] = {0};
      struct rarch_main_wrap      *args =
         (struct rarch_main_wrap*)params_data;

      if (args)
      {
         strlcpy(path, argv[CONTENT_PATH_ARG_INDEX], sizeof(path));

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

         RARCH_LOG("Auto-start game %s.\n", argv[CONTENT_PATH_ARG_INDEX]);
      }
   }

   dir_check_defaults("host0:app/custom.ini");
#endif
}

static void frontend_orbis_deinit(void *data)
{
   (void)data;
#if defined(HAVE_LIBORBIS)
	ps4LinkFinish();
#endif
}

static void frontend_orbis_shutdown(bool unused)
{
   (void)unused;
   return;
}

static void frontend_orbis_init(void *data)
{
   int ret=initOrbisLinkAppVanillaGl();

   sceSystemServiceHideSplashScreen();


   logger_init();
   RARCH_LOG("[%s][%s][%d] Hello from retroarch level info\n",__FILE__,__PRETTY_FUNCTION__,__LINE__);
   RARCH_ERR("[%s][%s][%d] Hello from retroarch level error\n",__FILE__,__PRETTY_FUNCTION__,__LINE__);
   RARCH_WARN("[%s][%s][%d] Hello from retroarch level warning no warning level on debugnet yet\n",__FILE__,__PRETTY_FUNCTION__,__LINE__);
   RARCH_DBG("[%s][%s][%d] Hello from retroarch level debug\n",__FILE__,__PRETTY_FUNCTION__,__LINE__);

   ret=sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIO_OUT);
    if (ret) 
    {
        RARCH_LOG("sceSysmoduleLoadModuleInternal(%s) failed: 0x%08X\n", "SCE_SYSMODULE_INTERNAL_AUDIO_OUT", ret);

    }
   

   verbosity_enable();
}

static void frontend_orbis_exec(const char *path, bool should_load_game)
{
   int ret;
   char argp[512] = {0};
   int   args = 0;

#if !defined(HAVE_LIBORBIS)
   // SceKernelStat sb;
   // sceKernelStat(path, &sb);
   // if (!(sb.st_mode & S_IXUSR))
   //    sceKernelChmod(path, S_IRWXU);
#endif

#ifndef IS_SALAMANDER
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      char game_path[PATH_MAX_LENGTH];
      strlcpy(game_path, path_get(RARCH_PATH_CONTENT), sizeof(game_path));
      const char * const argp[] = {
         eboot_path,
         game_path,
         NULL
      };
      args = 2;
      RARCH_LOG("Attempt to load executable: %d [%s].\n", args, argp);
      // ret = sceSystemServiceLoadExec(path, (char *const *)argp);
   }
   else
#endif
   {
      // ret =  sceSystemServiceLoadExec(path, NULL);
   }
   //RARCH_LOG("Attempt to load executable: [%d].\n", ret);
}

#ifndef IS_SALAMANDER
static bool frontend_orbis_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         orbis_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         orbis_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         orbis_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_orbis_exitspawn(char *s, size_t len, char *args)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (orbis_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (orbis_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }
#endif
   frontend_orbis_exec(s, should_load_game);
}

static int frontend_orbis_get_rating(void)
{
   return 6; /* Go with a conservative figure for now. */
}

enum frontend_architecture frontend_orbis_get_arch(void)
{
   return FRONTEND_ARCH_X86_64;
}

static int frontend_orbis_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

#if defined(HAVE_LIBORBIS)
   menu_entries_append_enum(list,
         "host0:app",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#else
   menu_entries_append_enum(list,
         "/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);

   menu_entries_append_enum(list,
         "/data",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);

   menu_entries_append_enum(list,
         "/usb0",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif
#endif
   return 0;
}

// static size_t frontend_orbis_get_mem_total(void)
// {
//   size_t max_mem = 0, cur_mem = 0;
//   get_user_mem_size(&max_mem, &cur_mem);
//   return max_mem;
// }

// static size_t frontend_orbis_get_mem_used(void)
// {
//   size_t max_mem = 0, cur_mem = 0;
//   get_user_mem_size(&max_mem, &cur_mem);
//   return cur_mem;
// }

frontend_ctx_driver_t frontend_ctx_orbis = {
   NULL, /*frontend_orbis_get_env,*/
   frontend_orbis_init,
   frontend_orbis_deinit,
   frontend_orbis_exitspawn,
   NULL,                         /* process_args */
   frontend_orbis_exec,
#ifdef IS_SALAMANDER
   NULL,
#else
   frontend_orbis_set_fork,
#endif
   frontend_orbis_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_orbis_get_rating,
   NULL,                         /* content_loaded */
   frontend_orbis_get_arch,
   NULL,
   frontend_orbis_parse_drive_list,
   NULL, /* TODO: frontend_orbis_get_mem_total,*/
   NULL, /* TODO: frontend_orbis_get_mem_used,*/
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
   "orbis",                      /* ident */
   NULL                          /* get_video_driver */
};
