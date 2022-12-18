/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2016-2019 - Brad Parker
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <process.h>

#include <string/stdstring.h>
#include <file/file_path.h>

#include "../frontend_driver.h"
#include "../../command.h"
#include "../../defaults.h"
#include "../../paths.h"

static enum frontend_fork dos_fork_mode = FRONTEND_FORK_NONE;

static void frontend_dos_init(void *data)
{
	/* Keep a call to time() as otherwise we trigger some obscure bug in
	 * djgpp libc code and time(NULL) return only -1 */
	printf("Loading RetroArch. Time is @%ld...\n", (long) time(NULL));
}

static void frontend_dos_shutdown(bool unused)
{
	(void)unused;
}

static int frontend_dos_get_rating(void)
{
	return -1;
}

enum frontend_architecture frontend_dos_get_arch(void)
{
	return FRONTEND_ARCH_X86;
}

static void frontend_dos_get_env_settings(int *argc, char *argv[],
      void *data, void *params_data)
{
   char *slash;
	char base_path[PATH_MAX];

	retro_main_log_file_init("retrodos.txt", false);

	strlcpy(base_path, argv[0], sizeof(base_path));
	if ((slash = strrchr(base_path, '/')))
	  *slash = '\0';
	slash = strrchr(base_path, '/');
	if (slash && strcasecmp(slash, "/cores"))
	  *slash = '\0';

	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], base_path,
			   "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], base_path,
			   "coreinfo", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], base_path,
			   "autoconf", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));

	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], base_path,
			   "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));

	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], base_path,
			   "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
			   g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
			   "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], base_path,
			   "playlist", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG], base_path,
			   "recrdcfg", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT], base_path,
			   "records", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], base_path,
			   "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], base_path,
			   "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], base_path,
			   "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], base_path,
			   "overlay", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], base_path,
			   "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], base_path,
			   "download", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], base_path,
			   "scrnshot", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], base_path,
			   "thumbs", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], base_path,
			   "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static void frontend_dos_exec(const char *path, bool should_load_game)
{
	char *newargv[]    = { NULL, NULL };
	size_t len         = strlen(path);

	newargv[0] = (char*)malloc(len);

	strlcpy(newargv[0], path, len);

	execv(path, newargv);
}

static void frontend_dos_exitspawn(char *s, size_t len, char *args)
{
	bool should_load_content = false;

	if (dos_fork_mode == FRONTEND_FORK_NONE)
		return;
	
	switch (dos_fork_mode)
	{
	case FRONTEND_FORK_CORE_WITH_ARGS:
		should_load_content = true;
		break;
	case FRONTEND_FORK_NONE:
	default:
		break;
	}

	frontend_dos_exec(s, should_load_content);
}

static bool frontend_dos_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         dos_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         dos_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         dos_fork_mode  = FRONTEND_FORK_CORE;

         {
            char executable_path[PATH_MAX_LENGTH] = {0};
            fill_pathname_application_path(executable_path,
                  sizeof(executable_path));
            path_set(RARCH_PATH_CORE, executable_path);
         }
         command_event(CMD_EVENT_QUIT, NULL);
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}

frontend_ctx_driver_t frontend_ctx_dos = {
	frontend_dos_get_env_settings,/* environment_get */
	frontend_dos_init,            /* init */
	NULL,                         /* deinit */
	frontend_dos_exitspawn,       /* exitspawn */
	NULL,                         /* process_args */
	frontend_dos_exec,            /* exec */
	frontend_dos_set_fork,        /* set_fork */
	frontend_dos_shutdown,        /* shutdown */
	NULL,                         /* get_name */
	NULL,                         /* get_os */
	frontend_dos_get_rating,      /* get_rating */
	NULL,                         /* content_loaded   */
	frontend_dos_get_arch,        /* get_architecture */
	NULL,                         /* get_powerstate */
	NULL,                         /* parse_drive_list */
	NULL,                         /* get_total_mem */
	NULL,                         /* get_free_mem  */
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
	NULL,                         /* set_gamemode        */
	"dos",                        /* ident               */
   NULL                          /* get_video_driver    */
};
