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

#include <stdint.h>
#include <string.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../bluetooth/bluetooth_driver.h"

/* Two steps, BLUETOOTH_SCAN_WINDOW_US apart. The first starts
 * discovery and reschedules the task for the end of the window; the
 * task queue keeps a scheduled task behind the ready ones, so other
 * tasks run in the meantime. The scan used to sleep through the window
 * inside one call, holding the task thread - and every download,
 * thumbnail and sync queued behind it - for ten seconds. A cancel ends
 * the window early. */
static void task_bluetooth_scan_handler(retro_task_t *task)
{
   if (!task->state)
   {
      driver_bluetooth_scan_begin();
      task->state = (void*)(uintptr_t)1;
      task->when  = cpu_features_get_time_usec() + BLUETOOTH_SCAN_WINDOW_US;
      return;
   }

   if (     !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
         && cpu_features_get_time_usec() < task->when)
      return;

   driver_bluetooth_scan_end();
   task->state = NULL;

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
