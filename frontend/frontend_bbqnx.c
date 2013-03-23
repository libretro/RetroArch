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

int rarch_main(int argc, char *argv[])
{
   //Initialize bps
   bps_initialize();

   rarch_main_clear_state();

   g_extern.verbose = true;

#ifdef HAVE_RGUI
   menu_init();
   g_extern.lifecycle_mode_state |= 1ULL << MODE_INIT;

   for (;;)
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
      {
	     while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
	        g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_INIT))
      {
	     if (g_extern.main_is_init)
	        rarch_main_deinit();

	     struct rarch_main_wrap args = {0};

	     args.verbose = g_extern.verbose;
	     args.sram_path = NULL;
	     args.state_path = NULL;
	     args.rom_path = "shared/documents/roms/snes9x-next/ChronoTrigger.smc";
	     args.libretro_path = "app/native/lib/test.so";
	     args.config_path = "app/native/retroarch.cfg";

	     int init_ret = rarch_main_init_wrap(&args);
	     if (init_ret == 0)
	     {
	        RARCH_LOG("rarch_main_init() succeeded.\n");
	        g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
	     }
	     else
	     {
	        RARCH_ERR("rarch_main_init() failed.\n");
	        g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU);
	     }

	     g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INIT);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_mode_state |= 1ULL << MODE_MENU_PREINIT;
         while (menu_iterate());
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
      }
      else
         break;
   }

   menu_free();
   if (g_extern.main_is_init)
      rarch_main_deinit();
#else
   struct rarch_main_wrap args = {0};

   args.verbose = g_extern.verbose;
   args.sram_path = NULL;
   args.state_path = NULL;
   args.rom_path = "shared/documents/roms/snes9x-next/ChronoTrigger.smc";
   args.libretro_path = "app/native/lib/test.so";
   args.config_path = "app/native/retroarch.cfg";

   rarch_init_msg_queue();

   int init_ret;
   if ((init_ret = rarch_main_init_wrap(&args))) return init_ret;

   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
   rarch_main_deinit();
#endif

   rarch_deinit_msg_queue();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

error:
   bps_shutdown();

   return 0;
}
