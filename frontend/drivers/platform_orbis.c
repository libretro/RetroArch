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

#if defined(HAVE_LIBORBIS)
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
#include <libSceLibcInternal.h>
#include <defines/ps4_defines.h>
#include <user_mem.h>

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
#define MODULE_PATH "/data/self/system/common/lib/"
#define MODULE_PATH_EXT "/app0/sce_module/"

char eboot_path[512];
char user_path[512];
SceKernelModule s_piglet_module;
SceKernelModule s_shacc_module;

static enum frontend_fork orbis_fork_mode = FRONTEND_FORK_NONE;

#define MEM_SIZE (3UL * 1024 * 1024 * 1024) /* 2600 MiB */
#define MEM_ALIGN (16UL * 1024)

/* TODO/FIXME: INCLUDING <orbislink.h> produces duplication errors */
int initOrbisLinkAppVanillaGl(void);

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

   strlcpy(eboot_path, EBOOT_PATH, sizeof(eboot_path));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_PORT], eboot_path, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   strlcpy(user_path, USER_PATH, sizeof(user_path));

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
      }
   }

   dir_check_defaults("host0:app/custom.ini");
#endif
}

static void frontend_orbis_deinit(void *data) { }
static void frontend_orbis_shutdown(bool unused) { }

static bool frontend_orbis_init_app(void)
{
	if (initOrbisLinkAppVanillaGl() == 0)
	{
		debugNetInit(PC_DEVELOPMENT_IP_ADDRESS,PC_DEVELOPMENT_UDP_PORT,3);
		debugNetPrintf(DEBUGNET_INFO,"Ready to have a lot of fun\n");
		sceSystemServiceHideSplashScreen();
		return true;
	}
	return false;
}

static void frontend_orbis_init(void *data)
{
   frontend_orbis_init_app();
   sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIO_OUT);
   verbosity_enable();
}

static void frontend_orbis_exec(const char *path, bool should_load_game)
{
   int ret;
   char argp[512] = {0};
   int   args     = 0;

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
   }
#endif
}

#ifndef IS_SALAMANDER
static bool frontend_orbis_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         orbis_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         orbis_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
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
   /* TODO/FIXME - needs a different rating */
   return 6;
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

   menu_entries_append(list,
         "/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);

   menu_entries_append(list,
         "/data",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);

   menu_entries_append(list,
         "/usb0",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
#endif
   return 0;
}

static size_t frontend_orbis_get_mem_total(void)
{
  size_t max_mem = 0, cur_mem = 0;
  get_user_mem_size(&max_mem, &cur_mem);
  return max_mem;
}

static size_t frontend_orbis_get_mem_used(void)
{
  size_t max_mem = 0, cur_mem = 0;
  get_user_mem_size(&max_mem, &cur_mem);
  return cur_mem;
}

frontend_ctx_driver_t frontend_ctx_orbis = {
   frontend_orbis_get_env,
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
   frontend_orbis_get_mem_total,
   frontend_orbis_get_mem_used,
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
