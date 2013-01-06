/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdint.h>
#include <stddef.h>

#include "rarch_console_main_wrap.h"

bool rarch_startup (const char * config_file)
{
   struct rarch_main_wrap args = {0};

   args.verbose = g_extern.verbose;
   args.config_path = config_file;
   args.sram_path = g_extern.console.main_wrap.state.default_sram_dir.enable ? g_extern.console.main_wrap.paths.default_sram_dir : NULL,
      args.state_path = g_extern.console.main_wrap.state.default_savestate_dir.enable ? g_extern.console.main_wrap.paths.default_savestate_dir : NULL,
      args.rom_path = g_extern.file_state.rom_path;
   args.libretro_path = g_settings.libretro;

   int init_ret = rarch_main_init_wrap(&args);

   if (init_ret == 0)
      RARCH_LOG("rarch_main_init succeeded.\n");
   else
   {
      RARCH_ERR("rarch_main_init failed.\n");
      return false;
   }

   return true;
}
