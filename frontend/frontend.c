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

#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#ifdef HAVE_RGUI
#include "../frontend/menu/rgui.h"
#endif

#if defined(__APPLE__) && !defined(OSX)
#include "SDL.h" 
// OSX seems to really need -lSDLmain, 
// so we include SDL.h here so it can hack our main.
// We want to use -mconsole in Win32, so we need main().
#elif defined(__QNX__)
#include <bps/bps.h>
#endif

#if defined(HAVE_RGUI) || defined(HAVE_RMENU) || defined(HAVE_RMENU_XUI)
#define HAVE_MENU
#else
#undef HAVE_MENU
#endif

static int rarch_preinit(void)
{
#if !defined(HAVE_BB10)
   //Initialize BPS libraries
   bps_initialize();
   rarch_main_clear_state();
   strlcpy(g_settings.libretro, "app/native/lib", sizeof(g_settings.libretro));
   strlcpy(g_extern.config_path, "app/native/retroarch.cfg", sizeof(g_extern.config_path));
   strlcpy(g_settings.video.shader_dir, "app/native/shaders_glsl", sizeof(g_settings.video.shader_dir));

   config_load();

   g_extern.verbose = true;
#endif
}

int rarch_main(int argc, char *argv[])
{
   rarch_preinit();

   rarch_main_clear_state();
   rarch_init_msg_queue();

   int init_ret;
   if ((init_ret = rarch_main_init(argc, argv))) return init_ret;

#ifdef HAVE_MENU
   menu_init();
   g_extern.lifecycle_mode_state |= 1ULL << MODE_GAME;

   // If we started a ROM directly from command line,
   // push it to ROM history.
   if (!g_extern.libretro_dummy)
      menu_rom_history_push_current();

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
#if defined(RARCH_CONSOLE) || defined(__QNX__)
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
         // Menu should always run with vsync on.
         video_set_nonblock_state_func(false);

         if (driver.audio_data)
            audio_stop_func();

         while (menu_iterate());

         driver_set_nonblock_state(driver.nonblock_state);

         if (driver.audio_data && !audio_start_func())
         {
            RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
            g_extern.audio_active = false;
         }

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
#else
   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
   rarch_main_deinit();
#endif

   rarch_deinit_msg_queue();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

   rarch_main_clear_state();

error:
#ifdef __QNX__
   bps_shutdown();
#endif

   return 0;
}

int main(int argc, char *argv[])
{
   return rarch_main(argc, argv);
}
