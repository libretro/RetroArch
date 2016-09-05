/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <file/config_file.h>
#include <queues/task_queue.h>

#include "../../configuration.h"
#include "../../defaults.h"
#include "../../general.h"
#include "../../content.h"
#include "../frontend.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../frontend_driver.h"
#include "../../command.h"

#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include <retro_stat.h>

static void emscripten_mainloop(void)
{
   unsigned sleep_ms = 0;
   int ret = runloop_iterate(&sleep_ms);
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
   command_event(CMD_EVENT_SAVEFILES, NULL);
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
   const char *xdg          = getenv("XDG_CONFIG_HOME");
   const char *home         = getenv("HOME");

   if (xdg)
      snprintf(base_path, sizeof(base_path),
            "%s/retroarch", xdg);
   else if (home)
      snprintf(base_path, sizeof(base_path),
            "%s/.config/retroarch", home);
   else
      snprintf(base_path, sizeof(base_path), "retroarch");

   fill_pathname_join(g_defaults.dir.core, base_path,
         "cores", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.core_info, base_path,
         "cores", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.autoconfig, base_path,
         "autoconfig", sizeof(g_defaults.dir.autoconfig));

   fill_pathname_join(g_defaults.dir.menu_config, base_path,
         "config", sizeof(g_defaults.dir.menu_config));
   fill_pathname_join(g_defaults.dir.remap, g_defaults.dir.menu_config,
         "remaps", sizeof(g_defaults.dir.remap));
   fill_pathname_join(g_defaults.dir.playlist, base_path,
         "playlists", sizeof(g_defaults.dir.playlist));
   fill_pathname_join(g_defaults.dir.cursor, base_path,
         "database/cursors", sizeof(g_defaults.dir.cursor));
   fill_pathname_join(g_defaults.dir.database, base_path,
         "database/rdb", sizeof(g_defaults.dir.database));
   fill_pathname_join(g_defaults.dir.shader, base_path,
         "shaders", sizeof(g_defaults.dir.shader));
   fill_pathname_join(g_defaults.dir.cheats, base_path,
         "cheats", sizeof(g_defaults.dir.cheats));
   fill_pathname_join(g_defaults.dir.overlay, base_path,
         "overlay", sizeof(g_defaults.dir.overlay));
   fill_pathname_join(g_defaults.dir.osk_overlay, base_path,
         "overlay", sizeof(g_defaults.dir.osk_overlay));
   fill_pathname_join(g_defaults.dir.core_assets, base_path,
         "downloads", sizeof(g_defaults.dir.core_assets));
   fill_pathname_join(g_defaults.dir.screenshot, base_path,
         "screenshots", sizeof(g_defaults.dir.screenshot));
   fill_pathname_join(g_defaults.dir.thumbnails, base_path,
         "thumbnails", sizeof(g_defaults.dir.thumbnails));
   fill_pathname_join(g_defaults.dir.sram, base_path,
         "saves", sizeof(g_defaults.dir.sram));
   fill_pathname_join(g_defaults.dir.savestate, base_path,
         "states", sizeof(g_defaults.dir.savestate));

   /* don't use XDG for these, we don't want these to Sync to cloud storage*/
   fill_pathname_join(g_defaults.dir.menu_content, "/",
         "content", sizeof(g_defaults.dir.thumbnails));
   fill_pathname_join(g_defaults.dir.assets, "/",
         "assets", sizeof(g_defaults.dir.assets));

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
   "emscripten"
};
