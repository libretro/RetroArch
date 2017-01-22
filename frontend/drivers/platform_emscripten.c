/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Michael Lelli
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

#include <emscripten/emscripten.h>
#include <string.h>

#include <file/config_file.h>
#include <queues/task_queue.h>
#include <retro_stat.h>
#include <file/file_path.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../configuration.h"
#include "../../defaults.h"
#include "../../content.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../command.h"
#include "../../tasks/tasks_internal.h"
#include "../../file_path_special.h"

static void emscripten_mainloop(void)
{
   unsigned sleep_ms = 0;
   int           ret = runloop_iterate(&sleep_ms);

   if (ret == 1 && sleep_ms > 0)
      retro_sleep(sleep_ms);
   task_queue_ctl(TASK_QUEUE_CTL_CHECK, NULL);
   if (ret != -1)
      return;

   main_exit(NULL);
   exit(0);
}

void cmd_savefiles(void)
{
   event_save_files();
}

void cmd_save_state(void)
{
   command_event(CMD_EVENT_SAVE_STATE, NULL);
}

void cmd_load_state(void)
{
   command_event(CMD_EVENT_LOAD_STATE, NULL);
}

void cmd_take_screenshot(void)
{
   command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);
}

static void frontend_emscripten_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   (void)args;

   char base_path[PATH_MAX] = {0};
   char user_path[PATH_MAX] = {0};
   const char *home         = getenv("HOME");

   if (home)
   {
      snprintf(base_path, sizeof(base_path),
            "%s/retroarch", home);
      snprintf(user_path, sizeof(user_path),
            "%s/retroarch/userdata", home);
   }
   else
   {
      snprintf(base_path, sizeof(base_path), "retroarch");
      snprintf(user_path, sizeof(user_path), "retroarch/userdata");
   }

   fill_pathname_join(g_defaults.dir.core, base_path,
         "cores", sizeof(g_defaults.dir.core));

   /* bundle data */
   fill_pathname_join(g_defaults.dir.assets, base_path,
         "bundle/assets", sizeof(g_defaults.dir.assets));
   fill_pathname_join(g_defaults.dir.autoconfig, base_path,
         "bundle/autoconfig", sizeof(g_defaults.dir.autoconfig));
   fill_pathname_join(g_defaults.dir.cursor, base_path,
         "bundle/database/cursors", sizeof(g_defaults.dir.cursor));
   fill_pathname_join(g_defaults.dir.database, base_path,
         "bundle/database/rdb", sizeof(g_defaults.dir.database));
   fill_pathname_join(g_defaults.dir.core_info, base_path,
         "bundle/info", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.overlay, base_path,
         "bundle/overlays", sizeof(g_defaults.dir.overlay));
   fill_pathname_join(g_defaults.dir.shader, base_path,
         "bundle/shaders", sizeof(g_defaults.dir.shader));

   /* user data dirs */
   fill_pathname_join(g_defaults.dir.cheats, user_path,
         "cheats", sizeof(g_defaults.dir.cheats));
   fill_pathname_join(g_defaults.dir.menu_config, user_path,
         "config", sizeof(g_defaults.dir.menu_config));
   fill_pathname_join(g_defaults.dir.menu_content, user_path,
         "content", sizeof(g_defaults.dir.menu_content));
   fill_pathname_join(g_defaults.dir.core_assets, user_path,
         "content/downloads", sizeof(g_defaults.dir.core_assets));
   fill_pathname_join(g_defaults.dir.playlist, user_path,
         "playlists", sizeof(g_defaults.dir.playlist));
   fill_pathname_join(g_defaults.dir.remap, g_defaults.dir.menu_config,
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

   /* cache dir */
   fill_pathname_join(g_defaults.dir.cache, "/tmp/",
         "retroarch", sizeof(g_defaults.dir.cache));

   /* history and main config */
   strlcpy(g_defaults.dir.content_history,
         user_path, sizeof(g_defaults.dir.content_history));
   fill_pathname_join(g_defaults.path.config, user_path,
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));

   /* create user data dirs */
   path_mkdir(g_defaults.dir.cheats);
   path_mkdir(g_defaults.dir.core_assets);
   path_mkdir(g_defaults.dir.menu_config);
   path_mkdir(g_defaults.dir.menu_content);
   path_mkdir(g_defaults.dir.playlist);
   path_mkdir(g_defaults.dir.remap);
   path_mkdir(g_defaults.dir.savestate);
   path_mkdir(g_defaults.dir.screenshot);
   path_mkdir(g_defaults.dir.sram);
   path_mkdir(g_defaults.dir.system);
   path_mkdir(g_defaults.dir.thumbnails);

   /* create cache dir */
   path_mkdir(g_defaults.dir.cache);

   snprintf(g_defaults.settings.menu, sizeof(g_defaults.settings.menu), "rgui");
}

int main(int argc, char *argv[])
{
   settings_t *settings = config_get_ptr();

   emscripten_set_canvas_size(800, 600);
   rarch_main(argc, argv, NULL);
   emscripten_set_main_loop(emscripten_mainloop,
         settings->video.vsync ? 0 : INT_MAX, 1);

   return 0;
}

frontend_ctx_driver_t frontend_ctx_emscripten = {
   frontend_emscripten_get_env,  /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   NULL,                         /* get_rating */
   NULL,                         /* load_content */
   NULL,                         /* get_architecture */
   NULL,                         /* get_powerstate */
   NULL,                         /* parse_drive_list */
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_used */
   NULL,                         /* install_sighandlers */
   NULL,                         /* get_signal_handler_state */
   NULL,                         /* set_signal_handler_state */
   NULL,                         /* destroy_signal_handler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   "emscripten"
};
