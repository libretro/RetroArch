/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <file/file_path.h>

#include "menu_content.h"
#include "menu_driver.h"
#include "menu_display.h"
#include "menu_shader.h"

#include "../configuration.h"
#include "../defaults.h"
#include "../frontend/frontend.h"
#include "../playlist.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"

static void menu_content_push_to_history_playlist(void)
{
   struct retro_system_info *system = NULL;
   settings_t *settings        = config_get_ptr();
   char *fullpath              = NULL;

   if (!settings->history_list_enable)
      return;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (*fullpath)
   {
      char tmp[PATH_MAX_LENGTH];
      char str[PATH_MAX_LENGTH];

      fill_pathname_base(tmp, fullpath, sizeof(tmp));
      snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
      menu_display_msg_queue_push(str, 1, 1, false);
   }

   menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET,
         &system);

   content_playlist_push(g_defaults.history,
         fullpath,
         NULL,
         settings->libretro,
         system->library_name,
         NULL,
         NULL);

   content_playlist_write_file(g_defaults.history);
}

static void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   char *fullpath       = NULL;
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();
    
   if (!wrap_args)
      return;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   wrap_args->no_content       = menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL);
   if (!global->has_set.verbosity)
      wrap_args->verbose       = *retro_main_verbosity();

   wrap_args->config_path      = *global->path.config   ? global->path.config   : NULL;
   wrap_args->sram_path        = *global->dir.savefile  ? global->dir.savefile  : NULL;
   wrap_args->state_path       = *global->dir.savestate ? global->dir.savestate : NULL;
   wrap_args->content_path     = *fullpath              ? fullpath              : NULL;

   if (!global->has_set.libretro)
      wrap_args->libretro_path = *settings->libretro ? settings->libretro : NULL;
   wrap_args->touched       = true;
}

/**
 * menu_content_load:
 *
 * Loads content into currently selected core.
 * Will also optionally push the content entry to the history playlist.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/

bool menu_content_load(void)
{
   bool msg_force       = true;
   char *fullpath       = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
   /* redraw menu frame */
   menu_display_ctl(MENU_DISPLAY_CTL_SET_MSG_FORCE, &msg_force);
   menu_driver_ctl(RARCH_MENU_CTL_RENDER, NULL);

   if (!(main_load_content(0, NULL, NULL, menu_content_environment_get)))
   {
      char name[PATH_MAX_LENGTH];
      char msg[PATH_MAX_LENGTH];

      fill_pathname_base(name, fullpath, sizeof(name));
      snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
      menu_display_msg_queue_push(msg, 1, 90, false);

      return false;
   }

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_MANAGER_INIT, NULL);

   event_cmd_ctl(EVENT_CMD_HISTORY_INIT, NULL);

   if (*fullpath || menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL))
      menu_content_push_to_history_playlist();

   event_cmd_ctl(EVENT_CMD_VIDEO_SET_ASPECT_RATIO, NULL);
   event_cmd_ctl(EVENT_CMD_RESUME, NULL);

   return true;
}
