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
#include <string/stdstring.h>

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
#include "../../command.h"
#include "../../tasks/tasks_internal.h"
#include "../../file_path_special.h"

static void emscripten_mainloop(void)
{
   unsigned sleep_ms = 0;
   int           ret = runloop_iterate(&sleep_ms);

   if (ret == 1 && sleep_ms > 0)
      retro_sleep(sleep_ms);

   task_queue_check();

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

   unsigned i;
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

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], base_path,
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));

   /* bundle data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], base_path,
         "bundle/assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], base_path,
         "bundle/autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], base_path,
         "bundle/database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], base_path,
         "bundle/database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], base_path,
         "bundle/info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], base_path,
         "bundle/overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], base_path,
         "bundle/shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));

   /* user data dirs */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT], user_path,
         "content", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
         "content/downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
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
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], user_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));

   /* cache dir */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], "/tmp/",
         "retroarch", sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));

   /* history and main config */
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
         user_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path.config, user_path,
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      if (!string_is_empty(dir_path))
         path_mkdir(dir_path);
   }

   snprintf(g_defaults.settings.menu, sizeof(g_defaults.settings.menu), "rgui");
}

int main(int argc, char *argv[])
{
   settings_t *settings = config_get_ptr();

   emscripten_set_canvas_size(800, 600);
   rarch_main(argc, argv, NULL);
   emscripten_set_main_loop(emscripten_mainloop,
         settings->bools.video_vsync ? 0 : INT_MAX, 1);

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
