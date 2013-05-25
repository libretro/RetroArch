/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <bps/bps.h>

#include "menu/rgui.h"

int rarch_main(int argc, char *argv[])
{
   //Initialize bps
#ifndef HAVE_BB10
   bps_initialize();
   rarch_main_clear_state();
   strlcpy(g_settings.libretro, "app/native/lib", sizeof(g_settings.libretro));
#endif
   strlcpy(g_extern.config_path, "app/native/retroarch.cfg", sizeof(g_extern.config_path));

   config_load();

   g_extern.verbose = true;

   menu_init();

   g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);

#ifdef HAVE_BB10
   if (!g_extern.libretro_dummy)
      menu_rom_history_push_current();
#endif

   for (;;)
   {
      if (g_extern.system.shutdown)
         break;
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
      {
         load_menu_game_prepare();

         // If ROM load fails, we exit RetroArch. On console it might make more sense to go back to menu though ...
         if (load_menu_game())
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         else
         {
#if defined(RARCH_CONSOLE) || defined(__BLACKBERRY_QNX__)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU);
#else
            return 1;
#endif
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
      {
	     while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
	        g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_mode_state |= 1ULL << MODE_MENU_PREINIT;
         while (!g_extern.system.shutdown && menu_iterate());
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
      }
      else
         break;
   }

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

error:
   bps_shutdown();

   return 0;
}
