/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 * Copyright (C) 2013      - Jason Fetters
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

#include "../menu/menu_common.h"
#include "../../apple/common/rarch_wrapper.h"
#include "../../apple/common/apple_export.h"
#include "../../apple/common/setting_data.h"

#include "../frontend_context.h"

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

extern bool apple_is_running;

void apple_event_basic_command(enum basic_event_t action)
{
   switch (action)
   {
      case RESET:
         rarch_game_reset();
         return;
      case LOAD_STATE:
         rarch_load_state();
         return;
      case SAVE_STATE:
         rarch_save_state();
         return;
      case QUIT:
         g_extern.system.shutdown = true;
         return;
   }
}

void apple_refresh_config()
{
   // Little nudge to prevent stale values when reloading the confg file
   g_extern.block_config_read = false;
   memset(g_settings.input.overlay, 0, sizeof(g_settings.input.overlay));
   memset(g_settings.video.shader_path, 0, sizeof(g_settings.video.shader_path));

   if (apple_is_running)
   {
      uninit_drivers();
      config_load();
      init_drivers();
   }
}

int apple_rarch_load_content(int argc, char* argv[])
{
   rarch_main_clear_state();
   rarch_init_msg_queue();
   
   if (rarch_main_init(argc, argv))
      return 1;
   
   menu_init();
   g_extern.lifecycle_state |= 1ULL << MODE_GAME;
   g_extern.lifecycle_state |= 1ULL << MODE_GAME_ONESHOT;
   
   return 0;
}

const frontend_ctx_driver_t frontend_ctx_apple = {
   NULL,                         /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   NULL,                         /* exec */
   NULL,                         /* shutdown */
   "apple",
};
