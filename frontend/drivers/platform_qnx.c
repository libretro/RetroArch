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
#include "../../verbosity.h"

static void frontend_qnx_init(void *data)
{
   (void)data;
   verbosity_enable();
   bps_initialize();
}

static void frontend_qnx_shutdown(bool unused)
{
   (void)unused;
   bps_shutdown();
}

static int frontend_qnx_get_rating(void)
{
   /* TODO/FIXME - look at unique identifier per device and
    * determine rating for some */
   return -1;
}

static void frontend_qnx_get_environment_settings(int *argc, char *argv[],
      void *data, void *params_data)
{
   unsigned i;
   char data_assets_path[PATH_MAX] = {0};
   char assets_path[PATH_MAX]      = {0};
   char data_path[PATH_MAX]        = {0};
   char user_path[PATH_MAX]        = {0};
   char tmp_path[PATH_MAX]         = {0};
   char workdir[PATH_MAX]          = {0};

   getcwd(workdir, sizeof(workdir));

   if(!string_is_empty(workdir))
   {
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
      snprintf(assets_path, sizeof(data_path), "app/native/assets");
      snprintf(data_path, sizeof(data_path), "data");
      snprintf(user_path, sizeof(user_path), "shared/misc/retroarch");
      snprintf(tmp_path, sizeof(user_path), "tmp");
   }

   /* app data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], data_path,
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], data_path,
         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], data_path,
         "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], data_path,
         "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
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
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADERS], data_path,
         "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADERS]));

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
   fill_pathname_join(g_defaults.dirs[DEFAULT_VIDEO_FILTER], user_path,
         "filters/video", sizeof(g_defaults.dirs[DEFAULT_VIDEO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], user_path,
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
         "saves", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], user_path,
         "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
         "states", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAIL], user_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAIL]));
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
   fill_pathname_join(g_defaults.path.config, user_path,
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));

   /* bundle copy */
   snprintf(data_assets_path,
         sizeof(data_assets_path),
         "%s/%s", data_path, "assets");

   if (!filestream_exists(data_assets_path))
   {
      char copy_command[PATH_MAX] = {0};

      RARCH_LOG( "Copying application assets to data directory...\n" );

      snprintf(copy_command,
            sizeof(copy_command),
            "cp -r %s/. %s", assets_path, data_path);

      if(system(copy_command) == -1)
         RARCH_LOG( "Asset copy failed: Shell could not be run.\n" );
      else
         RARCH_LOG( "Asset copy successful.\n");
   }

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      if (!string_is_empty(dir_path))
         path_mkdir(dir_path);
   }

   /* set glui as default menu */
   snprintf(g_defaults.settings.menu, sizeof(g_defaults.settings.menu), "glui");
}

enum frontend_architecture frontend_qnx_get_architecture(void)
{
   return FRONTEND_ARCH_ARM;
}

frontend_ctx_driver_t frontend_ctx_qnx = {
   frontend_qnx_get_environment_settings,
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
   frontend_qnx_get_architecture,
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
   "qnx",
};
