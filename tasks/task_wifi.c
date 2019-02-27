/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Jean-André Santoni
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

#include <string.h>
#include <errno.h>
#include <compat/strl.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../verbosity.h"
#include "../wifi/wifi_driver.h"

static void task_wifi_scan_handler(retro_task_t *task)
{
   driver_wifi_scan();

   task_set_progress(task, 100);
   task_free_title(task);
   task_set_title(task, strdup(msg_hash_to_str(MSG_WIFI_SCAN_COMPLETE)));
   task_set_finished(task, true);
}

bool task_push_wifi_scan(retro_task_callback_t cb)
{
   retro_task_t   *task = task_init();

   if (!task)
      return false;

   /* blocking means no other task can run while this one is running,
    * which is the default */
   task->type           = TASK_TYPE_BLOCKING;
   task->state          = NULL;
   task->handler        = task_wifi_scan_handler;
   task->callback       = cb;
   task->title          = strdup(msg_hash_to_str(
                           MSG_SCANNING_WIRELESS_NETWORKS));

   task_queue_push(task);

   return true;
}
