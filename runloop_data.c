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

#include <compat/strl.h>

#include "runloop_data.h"
#include "runloop.h"

#include "tasks/tasks.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

static char data_runloop_msg[PATH_MAX_LENGTH];

void rarch_main_data_deinit(void)
{
   rarch_task_deinit();
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

void data_runloop_osd_msg(const char *msg, size_t len)
{
   strlcpy(data_runloop_msg, msg, len);
}
