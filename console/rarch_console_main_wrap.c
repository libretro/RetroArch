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

#define MAX_ARGS 32

static int rarch_main_init_wrap(const struct rarch_main_wrap *args)
{
   int argc = 0;
   char *argv[MAX_ARGS] = {NULL};

   argv[argc++] = strdup("retroarch");
   
   if (args->rom_path)
      argv[argc++] = strdup(args->rom_path);

   if (args->sram_path)
   {
      argv[argc++] = strdup("-s");
      argv[argc++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[argc++] = strdup("-S");
      argv[argc++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[argc++] = strdup("-c");
      argv[argc++] = strdup(args->config_path);
   }

#ifdef HAVE_DYLIB
   if (args->libretro_path)
   {
      argv[argc++] = strdup("-L");
      argv[argc++] = strdup(args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[argc++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   RARCH_LOG("foo\n");
   for(int i = 0; i < argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
   RARCH_LOG("bar\n");
#endif

   int ret = rarch_main_init(argc, argv);

   char **tmp = argv;
   while (*tmp)
   {
      free(*tmp);
      tmp++;
   }

   return ret;
}

bool rarch_startup (void)
{
   const char *config_path = g_extern.config_path;

   if(g_extern.main_is_init)
      rarch_main_deinit();

   struct rarch_main_wrap args = {0};

   args.verbose = g_extern.verbose;
   args.config_path = config_path;
   args.sram_path = g_extern.console.main_wrap.state.default_sram_dir.enable ? g_extern.console.main_wrap.paths.default_sram_dir : NULL,
      args.state_path = g_extern.console.main_wrap.state.default_savestate_dir.enable ? g_extern.console.main_wrap.paths.default_savestate_dir : NULL,
      args.rom_path = g_extern.file_state.rom_path;
   args.libretro_path = g_settings.libretro;

   int init_ret = rarch_main_init_wrap(&args);
   (void)init_ret;

   if(init_ret == 0)
      g_extern.console.rmenu.mode = MODE_EMULATION;
   else
   {
      //failed to load the ROM for whatever reason
      g_extern.console.rmenu.mode = MODE_MENU;
      rarch_settings_msg(S_MSG_ROM_LOADING_ERROR, S_DELAY_180);

      return false;
   }

   return true;
}
