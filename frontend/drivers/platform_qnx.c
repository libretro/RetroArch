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

#include <boolean.h>

#include <bps/bps.h>
#include <packageinfo.h>

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
    char assets_path[PATH_MAX] = {0};
    char data_path[PATH_MAX] = {0};
    char user_path[PATH_MAX] = {0};
    char tmp_path[PATH_MAX] = {0};

    char workdir[PATH_MAX] = {0};
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

   // app data
   fill_pathname_join(g_defaults.dir.core, data_path,
         "cores", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.assets, data_path,
         "assets", sizeof(g_defaults.dir.assets));
   fill_pathname_join(g_defaults.dir.autoconfig, data_path,
         "autoconfig", sizeof(g_defaults.dir.autoconfig));
   fill_pathname_join(g_defaults.dir.cursor, data_path,
         "database/cursors", sizeof(g_defaults.dir.cursor));
   fill_pathname_join(g_defaults.dir.database, data_path,
         "database/rdb", sizeof(g_defaults.dir.database));
   fill_pathname_join(g_defaults.dir.core_info, data_path,
         "info", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.overlay, data_path,
         "overlays", sizeof(g_defaults.dir.overlay));
   fill_pathname_join(g_defaults.dir.shader, data_path,
         "shaders", sizeof(g_defaults.dir.shader));

   // user data
   fill_pathname_join(g_defaults.dir.cheats, user_path,
         "cheats", sizeof(g_defaults.dir.cheats));
   fill_pathname_join(g_defaults.dir.menu_config, user_path,
         "config", sizeof(g_defaults.dir.menu_config));
   fill_pathname_join(g_defaults.dir.menu_content, user_path,
         "content", sizeof(g_defaults.dir.menu_content));
   fill_pathname_join(g_defaults.dir.core_assets, user_path,
         "downloads", sizeof(g_defaults.dir.core_assets));
   fill_pathname_join(g_defaults.dir.audio_filter, user_path,
         "filters/audio", sizeof(g_defaults.dir.audio_filter));
   fill_pathname_join(g_defaults.dir.video_filter, user_path,
         "filters/video", sizeof(g_defaults.dir.video_filter));
   fill_pathname_join(g_defaults.dir.playlist, user_path,
         "playlists", sizeof(g_defaults.dir.playlist));
   fill_pathname_join(g_defaults.dir.remap, user_path,
         "remaps", sizeof(g_defaults.dir.remap));
   fill_pathname_join(g_defaults.dir.sram, user_path,
         "saves", sizeof(g_defaults.dir.sram));
   fill_pathname_join(g_defaults.dir.screenshot, user_path,
         "screenshots", sizeof(g_defaults.dir.screenshot));
   fill_pathname_join(g_defaults.dir.savestate, user_path,
         "states", sizeof(g_defaults.dir.savestate));
   fill_pathname_join(g_defaults.dir.system, user_path,
         "system", sizeof(g_defaults.dir.system));
   fill_pathname_join(g_defaults.dir.thumbnails, user_path,
         "thumbnails", sizeof(g_defaults.dir.thumbnails));
   fill_pathname_join(g_defaults.dir.wallpapers, user_path,
         "wallpapers", sizeof(g_defaults.dir.wallpapers));

   // tmp
   strlcpy(g_defaults.dir.cache, tmp_path, sizeof(g_defaults.dir.cache));

   // history and main config
   strlcpy(g_defaults.dir.content_history,
         user_path, sizeof(g_defaults.dir.content_history));
   fill_pathname_join(g_defaults.path.config, user_path,
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));

   // bundle copy
   char data_assets_path[PATH_MAX] = {0};
   sprintf(data_assets_path, "%s/%s", data_path, "assets");
   if(!path_file_exists(data_assets_path))
   {
       RARCH_LOG( "Copying application assets to data directory...\n" );

       char copy_command[PATH_MAX] = {0};
       sprintf(copy_command, "cp -r %s/. %s", assets_path, data_path);

       if(system(copy_command) == -1) {
         RARCH_LOG( "Asset copy failed: Shell could not be run.\n" );
       } else {
           RARCH_LOG( "Asset copy successful.\n");
       }
   }

   // create user data dirs
   path_mkdir(g_defaults.dir.cheats);
   path_mkdir(g_defaults.dir.menu_config);
   path_mkdir(g_defaults.dir.menu_content);
   path_mkdir(g_defaults.dir.core_assets);
   path_mkdir(g_defaults.dir.playlist);
   path_mkdir(g_defaults.dir.remap);
   path_mkdir(g_defaults.dir.savestate);
   path_mkdir(g_defaults.dir.screenshot);
   path_mkdir(g_defaults.dir.sram);
   path_mkdir(g_defaults.dir.system);
   path_mkdir(g_defaults.dir.thumbnails);

   // set glui as default menu
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
   "qnx",
};
