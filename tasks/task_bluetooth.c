/*  RetroArch - A frontend for libretro.
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
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../bluetooth/bluetooth_driver.h"

static void task_bluetooth_scan_handler(retro_task_t *task)
{
   driver_bluetooth_scan();

   task_set_progress(task, 100);
   task_free_title(task);
   task_set_title(task, strdup(msg_hash_to_str(MSG_BLUETOOTH_SCAN_COMPLETE)));
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

bool task_push_bluetooth_scan(retro_task_callback_t cb)
{
   retro_task_t   *task = task_init();

   if (!task)
      return false;

   /* blocking means no other task can run while this one is running,
    * which is the default */
   task->type           = TASK_TYPE_BLOCKING;
   task->state          = NULL;
   task->handler        = task_bluetooth_scan_handler;
   task->callback       = cb;
   task->title          = strdup(msg_hash_to_str(
                           MSG_SCANNING_BLUETOOTH_DEVICES));

   task_queue_push(task);

   return true;
}
