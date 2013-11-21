/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 * Copyright (C) 2013      - Jason Fetters
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <dispatch/dispatch.h>
#include <pthread.h>
#include "../../apple/common/rarch_wrapper.h"

#include "../frontend_context.h"

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

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

void apple_event_basic_command(void* userdata)
{
   switch ((enum basic_event_t)userdata)
   {
      case RESET:
         rarch_game_reset();
         return;
      case LOAD_STATE:
         rarch_load_state();
         return;
      case SAVE_STATE:
         rarch_save_state();
         return;
      case QUIT:
         g_extern.system.shutdown = true;
         return;
   }
}

void apple_event_set_state_slot(void* userdata)
{
   g_extern.state_slot = (uint32_t)userdata;
}

void apple_event_show_rgui(void* userdata)
{
   const bool in_menu = g_extern.lifecycle_state & (1 << MODE_MENU);
   g_extern.lifecycle_state &= ~(1ULL << (in_menu ? MODE_MENU : MODE_GAME));
   g_extern.lifecycle_state |=  (1ULL << (in_menu ? MODE_GAME : MODE_MENU));
}

static void event_reload_config(void* userdata)
{
   objc_clear_config_hack();

   uninit_drivers();
   config_load();
   init_drivers();
}

void apple_refresh_config()
{
   if (apple_is_running)
      apple_frontend_post_event(&event_reload_config, 0);
   else
      objc_clear_config_hack();
}

pthread_mutex_t stasis_mutex = PTHREAD_MUTEX_INITIALIZER;

static void event_stasis(void* userdata)
{
   uninit_drivers();
   pthread_mutex_lock(&stasis_mutex);
   pthread_mutex_unlock(&stasis_mutex);
   init_drivers();
}

void apple_enter_stasis()
{
   if (apple_is_running)
   {
      pthread_mutex_lock(&stasis_mutex);
      apple_frontend_post_event(event_stasis, 0);
   }
}

void apple_exit_stasis(bool reload_config)
{
   if (reload_config)
   {
      objc_clear_config_hack();
      config_load();
   }

   if (apple_is_running)
      pthread_mutex_unlock(&stasis_mutex);
}

static int process_events(void *data)
{
   (void)data;
   pthread_mutex_lock(&apple_event_queue_lock);

   for (int i = 0; i < apple_event_queue_size; i ++)
      apple_event_queue[i].function(apple_event_queue[i].userdata);

   apple_event_queue_size = 0;

   pthread_mutex_unlock(&apple_event_queue_lock);
   return 0;
}

static void system_shutdown(bool force)
{
   /* force set to true makes it display the 'Failed to load game' message. */
   if (force)
      dispatch_async_f(dispatch_get_main_queue(), (void*)1, apple_rarch_exited);
   else
      dispatch_async_f(dispatch_get_main_queue(), 0, apple_rarch_exited);
}

const frontend_ctx_driver_t frontend_ctx_apple = {
   NULL,                         /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   process_events,               /* process_events */
   NULL,                         /* exec */
   system_shutdown,              /* shutdown */
   "apple",
};
