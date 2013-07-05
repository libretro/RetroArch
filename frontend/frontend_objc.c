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
#include <pthread.h>

#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#ifdef IOS
#include "../ios/RetroArch/rarch_wrapper.h"
#endif

#ifdef HAVE_RGUI
#include "../frontend/menu/rgui.h"
#endif

static pthread_mutex_t apple_event_queue_lock = PTHREAD_MUTEX_INITIALIZER;

static struct
{
   void (*function)(void*);
   void* userdata;
} apple_event_queue[16];

static uint32_t apple_event_queue_size;

void apple_frontend_post_event(void (*fn)(void*), void* userdata)
{
   pthread_mutex_lock(&apple_event_queue_lock);

   if (apple_event_queue_size < 16)
   {
      apple_event_queue[apple_event_queue_size].function = fn;
      apple_event_queue[apple_event_queue_size].userdata = userdata;
      apple_event_queue_size ++;
   }

   pthread_mutex_unlock(&apple_event_queue_lock);
}

static void process_events()
{
   pthread_mutex_lock(&apple_event_queue_lock);

   for (int i = 0; i < apple_event_queue_size; i ++)
      apple_event_queue[i].function(apple_event_queue[i].userdata);

   apple_event_queue_size = 0;

   pthread_mutex_unlock(&apple_event_queue_lock);
}

static void apple_free_main_wrap(struct rarch_main_wrap* wrap)
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

void* rarch_main_apple(void* args)
{
   struct rarch_main_wrap* argdata = (struct rarch_main_wrap*)args;
   int init_ret = rarch_main_init_wrap(argdata);
   apple_free_main_wrap(argdata);

   if (init_ret)
   {
      rarch_main_clear_state();
      dispatch_async_f(dispatch_get_main_queue(), (void*)1, apple_rarch_exited);
      return 0;
   }

#ifdef HAVE_RGUI
   char* system_directory = ios_get_rarch_system_directory();
   strlcpy(g_extern.savestate_dir, system_directory, sizeof(g_extern.savestate_dir));
   strlcpy(g_extern.savefile_dir, system_directory, sizeof(g_extern.savefile_dir));
   free(system_directory);

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
#ifdef RARCH_CONSOLE
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU);
#else
            // This needs to be here to tell the GUI thread that the emulator loop has stopped,
            // the (void*)1 makes it display the 'Failed to load game' message.
            dispatch_async_f(dispatch_get_main_queue(), (void*)1, apple_rarch_exited);
            return 1;
#endif
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
      {
         while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate())
         {
            process_events();

            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_GAME)))
               break;
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_mode_state |= 1ULL << MODE_MENU_PREINIT;
         while (!g_extern.system.shutdown && menu_iterate())
         {
            process_events();

            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_MENU)))
               break;
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

   dispatch_async_f(dispatch_get_main_queue(), 0, apple_rarch_exited);
   return 0;
}
