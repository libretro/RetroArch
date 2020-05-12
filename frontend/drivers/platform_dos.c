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

#include "../frontend_driver.h"

static void frontend_dos_init(void *data)
{
	printf("Loading RetroArch...\n");
}

static void frontend_dos_shutdown(bool unused)
{
	(void)unused;
}

static int frontend_dos_get_rating(void)
{
	return -1;
}

enum frontend_architecture frontend_dos_get_architecture(void)
{
	return FRONTEND_ARCH_X86;
}

static void frontend_dos_get_env_settings(int *argc, char *argv[],
      void *data, void *params_data)
{
	char base_path[PATH_MAX] = {0};
	int i;

	retro_main_log_file_init("retrodos.txt", false);

	strlcpy(base_path, "retrodos", sizeof(base_path));

	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], base_path,
			   "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], base_path,
			   "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
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
	fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], base_path,
			   "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
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

	for (i = 0; i < DEFAULT_DIR_LAST; i++)
	{
		const char *dir_path = g_defaults.dirs[i];
		if (!string_is_empty(dir_path))
			path_mkdir(dir_path);
	}
}

frontend_ctx_driver_t frontend_ctx_dos = {
	frontend_dos_get_env_settings,/* environment_get */
	frontend_dos_init,            /* init */
	NULL,                         /* deinit */
	NULL,                         /* exitspawn */
	NULL,                         /* process_args */
	NULL,                         /* exec */
	NULL,                         /* set_fork */
	frontend_dos_shutdown,        /* shutdown */
	NULL,                         /* get_name */
	NULL,                         /* get_os */
	frontend_dos_get_rating,      /* get_rating */
	NULL,                         /* load_content */
	frontend_dos_get_architecture,/* get_architecture */
	NULL,                         /* get_powerstate */
	NULL,                         /* parse_drive_list */
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
	NULL,                         /* is_narrator_running */
	NULL,                         /* accessibility_speak */
	"dos",
};
