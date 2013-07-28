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
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <dispatch/dispatch.h>
#include <pthread.h>
#include "../../apple/RetroArch/rarch_wrapper.h"

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

static void process_events(void)
{
   pthread_mutex_lock(&apple_event_queue_lock);

   for (int i = 0; i < apple_event_queue_size; i ++)
      apple_event_queue[i].function(apple_event_queue[i].userdata);

   apple_event_queue_size = 0;

   pthread_mutex_unlock(&apple_event_queue_lock);
}

static void system_shutdown(bool force)
{
   /* force set to true makes it display the 'Failed to load game' message. */
   if (force)
      dispatch_async_f(dispatch_get_main_queue(), (void*)1, apple_rarch_exited);
   else
      dispatch_async_f(dispatch_get_main_queue(), 0, apple_rarch_exited);
}

static void environment_get(int argc, char *argv[])
{
   (void)argc;
   (void)argv;

#ifdef IOS
   char* system_directory = ios_get_rarch_system_directory();
   strlcpy(g_extern.savestate_dir, system_directory, sizeof(g_extern.savestate_dir));
   strlcpy(g_extern.savefile_dir, system_directory, sizeof(g_extern.savefile_dir));
   free(system_directory);
#endif
}

const frontend_ctx_driver_t frontend_ctx_apple = {
   environment_get,              /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   process_events,               /* process_events */
   NULL,                         /* exec */
   system_shutdown,              /* shutdown */
   "apple",
};
