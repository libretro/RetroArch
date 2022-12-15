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
#include <libgen.h>
#include <dirent.h>

#include <bps/bps.h>
#include <packageinfo.h>

#include <boolean.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../defaults.h"
#include "../../dynamic.h"
#include "../../paths.h"
#include "../../verbosity.h"

static void frontend_qnx_init(void *data)
{
   verbosity_enable();
   bps_initialize();
}

static void frontend_qnx_shutdown(bool unused)
{
   bps_shutdown();
}

static int frontend_qnx_get_rating(void)
{
   /* TODO/FIXME - look at unique identifier per device and
    * determine rating for some */
   return -1;
}

static void frontend_qnx_get_env_settings(int *argc, char *argv[],
      void *data, void *params_data)
{
   unsigned i;
   char assets_path[PATH_MAX];
   char data_path[PATH_MAX];
   char user_path[PATH_MAX];
   char tmp_path[PATH_MAX];
   char data_assets_path[PATH_MAX];
   char workdir[PATH_MAX]          = {0};

   getcwd(workdir, sizeof(workdir));

   if (!string_is_empty(workdir))
   {
      assets_path[0]               = '\0';
      data_path[0]                 = '\0';
      user_path[0]                 = '\0';
      tmp_path[0]                  = '\0';
      snprintf(assets_path, sizeof(data_path),
            "%s/app/native/assets", workdir);
      snprintf(data_path, sizeof(data_path),
            "%s/data", workdir);
      snprintf(user_path, sizeof(user_path),
            "%s/shared/misc/retroarch", workdir);
      snprintf(tmp_path, sizeof(user_path),
            "%s/tmp", workdir);
   }
   else
   {
      strlcpy(assets_path, "app/native/assets", sizeof(assets_path));
      strlcpy(data_path, "data", sizeof(data_path));
      strlcpy(user_path, "shared/misc/retroarch", sizeof(user_path));
      strlcpy(tmp_path, "tmp", sizeof(user_path));
   }

   /* app data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], data_path,
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], data_path,
         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], data_path,
         "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], data_path,
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], data_path,
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], data_path,
         "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], data_path,
         "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif

   /* user data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT], user_path,
         "content", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], user_path,
         "filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
         "saves", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], user_path,
         "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
         "states", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS], user_path,
         "wallpapers", sizeof(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path,
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

   /* tmp */
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CACHE],
         tmp_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));

   /* history and main config */
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
         user_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path_config, user_path,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));

   /* bundle copy */
   fill_pathname_join_special(data_assets_path,
		   data_path, "assets", sizeof(data_assets_path));

   if (!filestream_exists(data_assets_path))
   {
      char copy_command[PATH_MAX] = {0};

      snprintf(copy_command,
            sizeof(copy_command),
            "cp -r %s/. %s", assets_path, data_path);

      if (system(copy_command) == -1)
         RARCH_ERR("Asset copy failed: Shell could not be run.\n" );
      else
         RARCH_LOG( "Asset copy successful.\n");
   }

   /* set GLUI as default menu */
   strlcpy(g_defaults.settings_menu, "glui", sizeof(g_defaults.settings_menu));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

enum frontend_architecture frontend_qnx_get_arch(void)
{
   return FRONTEND_ARCH_ARM;
}

frontend_ctx_driver_t frontend_ctx_qnx = {
   frontend_qnx_get_env_settings,
   frontend_qnx_init,
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_qnx_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_qnx_get_rating,
   NULL,                         /* load_content */
   frontend_qnx_get_arch,        /* get_architecture */
   NULL,                         /* get_powerstate */
   NULL,                         /* parse_drive_list */
   NULL,                         /* get_total_mem */
   NULL,                         /* get_free_mem */
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
   "qnx",                        /* ident               */
   NULL                          /* get_video_driver    */
};
