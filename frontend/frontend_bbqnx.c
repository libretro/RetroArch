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

   int init_ret;
   struct rarch_main_wrap args = {0};

   args.verbose = g_extern.verbose;
   args.sram_path = NULL;
   args.state_path = NULL;
   args.rom_path = "/accounts/1000/shared/documents/roms/quake/pak0.pak";
   args.libretro_path = "/accounts/1000/appdata/com.RetroArch.testDev_m_RetroArch181dafc7/app/native/lib/test.so";
   args.config_path = "/accounts/1000/appdata/com.RetroArch.testDev_m_RetroArch181dafc7/app/native/retroarch.cfg";

   if ((init_ret = rarch_main_init_wrap(&args)))
   {
      return init_ret;
   }
   rarch_init_msg_queue();
   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
   rarch_main_deinit();
   rarch_deinit_msg_queue();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

error:
   bps_shutdown();

   return 0;
}
