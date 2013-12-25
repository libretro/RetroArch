/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 * Copyright (C) 2012-2013 - Michael Lelli
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

#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#ifdef HAVE_MENU
#include "../frontend/menu/menu_common.h"
#endif

static bool menuloop;

int apple_rarch_iterate_once(void)
{
   if (g_extern.system.shutdown)
   {
      g_extern.system.shutdown = false;
      menu_free();

      if (g_extern.config_save_on_exit && *g_extern.config_path)
         config_save_file(g_extern.config_path);

      if (g_extern.main_is_init)
         rarch_main_deinit();

      rarch_deinit_msg_queue();

#ifdef PERF_TEST
      rarch_perf_log();
#endif

      rarch_main_clear_state();
      return 1;
   }
   else if (menuloop)
   {
      if (!menu_iterate())
      {
         menuloop = false;
         driver_set_nonblock_state(driver.nonblock_state);

         if (driver.audio_data && !audio_start_func())
         {
            RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
            g_extern.audio_active = false;
         }

         g_extern.lifecycle_state &= ~(1ULL << MODE_MENU);
      }
   }
   else if (g_extern.lifecycle_state & (1ULL << MODE_LOAD_GAME))
   {
      load_menu_game_prepare();

      // If ROM load fails, we exit RetroArch. On console it might make more sense to go back to menu though ...
      if (load_menu_game())
         g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      else
         return 2;

      g_extern.lifecycle_state &= ~(1ULL << MODE_LOAD_GAME);
   }
   else if (g_extern.lifecycle_state & (1ULL << MODE_GAME))
   {
      bool r;
      if (g_extern.is_paused && !g_extern.is_oneshot)
         r = rarch_main_idle_iterate();
      else
         r = rarch_main_iterate();
      if (!r)
         g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
   }
   else if (g_extern.lifecycle_state & (1ULL << MODE_MENU))
   {
      g_extern.lifecycle_state |= 1ULL << MODE_MENU_PREINIT;
      // Menu should always run with vsync on.
      video_set_nonblock_state_func(false);

      if (driver.audio_data)
         audio_stop_func();

      menuloop = true;
   }
   else
   {
      g_extern.system.shutdown = true;
   }
   
   return 0;
}
