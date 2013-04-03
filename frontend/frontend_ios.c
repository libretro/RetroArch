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

#include <dispatch/dispatch.h>

#include "../ios/RetroArch/rarch_wrapper.h"
#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#ifdef HAVE_RGUI
#include "../frontend/menu/rgui.h"
#endif

static void ios_free_main_wrap(struct rarch_main_wrap* wrap)
{
   if (wrap)
   {
      free((char*)wrap->libretro_path);
      free((char*)wrap->rom_path);
      free((char*)wrap->sram_path);
      free((char*)wrap->state_path);
      free((char*)wrap->config_path);
   }

   free(wrap);
}

void rarch_main_ios(void* args)
{
   struct rarch_main_wrap* argdata = (struct rarch_main_wrap*)args;
   int init_ret = rarch_main_init_wrap(argdata);
   ios_free_main_wrap(argdata);

   if (init_ret)
   {
      dispatch_async_f(dispatch_get_main_queue(), (void*)1, ios_rarch_exited);
      return;
   }

#ifdef HAVE_RGUI
   menu_init();
   g_extern.lifecycle_mode_state |= 1ULL << MODE_GAME;

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

         args.verbose       = g_extern.verbose;
         args.config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
         args.sram_path     = NULL;
         args.state_path    = NULL;
         args.rom_path      = g_extern.fullpath;
         args.libretro_path = g_settings.libretro;

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
   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
   rarch_main_deinit();
#endif
   
   rarch_deinit_msg_queue();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

   rarch_main_clear_state();

   dispatch_async_f(dispatch_get_main_queue(), 0, ios_rarch_exited);
}
