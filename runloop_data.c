/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <retro_miscellaneous.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#include <file/file_path.h>

#include "general.h"
#include "verbosity.h"

#include "tasks/tasks.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

static char data_runloop_msg[PATH_MAX_LENGTH];

void rarch_main_data_deinit(void)
{
   rarch_task_deinit();
}

bool rarch_main_data_active(void)
{
   return false;
}

#ifdef HAVE_MENU
static void rarch_main_data_menu_iterate(void)
{
   menu_iterate_render();
}
#endif

void rarch_main_data_iterate(void)
{
#ifdef HAVE_MENU
   rarch_main_data_menu_iterate();
#endif

   if (data_runloop_msg[0] != '\0')
   {
      rarch_main_msg_queue_push(data_runloop_msg, 1, 10, true);
      data_runloop_msg[0] = '\0';
   }

   rarch_task_check();
}

static void rarch_main_data_init(void)
{
   rarch_task_init();
}

void rarch_main_data_clear_state(void)
{
   rarch_main_data_deinit();
   rarch_main_data_init();
}

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush)
{
   char new_msg[PATH_MAX_LENGTH];
   msg_queue_t *queue            = NULL;
   settings_t     *settings      = config_get_ptr();

   (void)settings;

   switch(type)
   {
      case DATA_TYPE_NONE:
         break;
      case DATA_TYPE_FILE:
         break;
      case DATA_TYPE_IMAGE:
         break;
#ifdef HAVE_NETWORKING
      case DATA_TYPE_HTTP:
         break;
#endif
#ifdef HAVE_OVERLAY
      case DATA_TYPE_OVERLAY:
         break;
#endif
      case DATA_TYPE_DB:
      default:
         break;
   }

   if (!queue)
      return;

   if (flush)
      msg_queue_clear(queue);
   msg_queue_push(queue, new_msg, prio, duration);

}

void data_runloop_osd_msg(const char *msg, size_t len)
{
   strlcpy(data_runloop_msg, msg, len);
}
