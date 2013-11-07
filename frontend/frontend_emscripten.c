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

#include <emscripten/emscripten.h>
#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"
#include "../emscripten/RWebAudio.h"

#ifdef HAVE_RGUI
#include "../frontend/menu/rgui.h"
#endif

#if defined(HAVE_RGUI) || defined(HAVE_RMENU) || defined(HAVE_RMENU_XUI)
#define HAVE_MENU
#else
#undef HAVE_MENU
#endif

static bool menuloop;

static void endloop(void)
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

   exit(0);
}

static void mainloop(void)
{
   if (g_extern.system.shutdown)
   {
      endloop();
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
      {
#ifdef RARCH_CONSOLE
         g_extern.lifecycle_state |= (1ULL << MODE_MENU);
#else
         return;
#endif
      }

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
}

int main(int argc, char *argv[])
{
   emscripten_set_canvas_size(800, 600);

   rarch_main_clear_state();
   rarch_init_msg_queue();

   int init_ret;
   if ((init_ret = rarch_main_init(argc, argv))) return init_ret;

#ifdef HAVE_MENU
   menu_init();
   g_extern.lifecycle_state |= 1ULL << MODE_GAME;
   g_extern.lifecycle_state |= 1ULL << MODE_GAME_ONESHOT;

   // If we started a ROM directly from command line,
   // push it to ROM history.
   if (!g_extern.libretro_dummy)
      menu_rom_history_push_current();
#endif

   emscripten_set_main_loop(mainloop, g_settings.video.vsync ? 0 : INT_MAX, 1);

   return 0;
}
