/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
 * Copyright (C) 2012-2014 - Jason Fetters
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
#include "../../settings_data.h"

#include "../frontend.h"

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

static CFRunLoopObserverRef iterate_observer;

extern bool apple_is_running;

extern void apple_rarch_exited(void);

static void do_iteration(void)
{
   if (!(iterate_observer && apple_is_running && !g_extern.is_paused))
      return;

   if (main_entry_iterate(0, NULL, NULL))
   {
      main_exit(NULL);
      apple_rarch_exited();
      return;
   }

   CFRunLoopWakeUp(CFRunLoopGetMain());
}

void apple_start_iteration(void)
{
   if (iterate_observer)
       return;
    
    iterate_observer = CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting, true, 0, (CFRunLoopObserverCallBack)do_iteration, 0);
    CFRunLoopAddObserver(CFRunLoopGetMain(), iterate_observer, kCFRunLoopCommonModes);
}

void apple_stop_iteration(void)
{
   if (!iterate_observer)
       return;
    
    CFRunLoopObserverInvalidate(iterate_observer);
    CFRelease(iterate_observer);
    iterate_observer = 0;
}

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

void apple_refresh_config(void)
{
   // Little nudge to prevent stale values when reloading the config file
   g_extern.block_config_read = false;
   memset(g_settings.input.overlay, 0, sizeof(g_settings.input.overlay));
   memset(g_settings.video.shader_path, 0, sizeof(g_settings.video.shader_path));

   if (!apple_is_running)
      return;

   uninit_drivers();
   config_load();
   init_drivers();
}

int apple_rarch_load_content(int *argc, char* argv[])
{
   rarch_main_clear_state();
   rarch_init_msg_queue();
   
   if (rarch_main_init(*argc, argv))
      return 1;
   
   if (!g_extern.libretro_dummy)
      menu_rom_history_push_current();
   
   g_extern.lifecycle_state |= 1ULL << MODE_GAME;
   
   return 0;
}

static int frontend_apple_get_rating(void)
{
   /* TODO/FIXME - look at unique identifier per device and 
    * determine rating for some */
   return -1;
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
   NULL,                         /* get_name */
   frontend_apple_get_rating,    /* get_rating */
   "apple",
};
