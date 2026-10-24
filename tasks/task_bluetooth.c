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
#include <stdlib.h>
#include <string.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../bluetooth/bluetooth_driver.h"

/* The first step starts discovery; the task is then parked, so other
 * tasks run in the meantime, and looked at every BLUETOOTH_SCAN_POLL_US
 * until the driver says it has found all it will or the window,
 * BLUETOOTH_SCAN_WINDOW_US, is out. The scan used to sleep through the
 * window inside one call, holding the task thread - and every
 * download, thumbnail and sync queued behind it - for ten seconds. A
 * cancel ends the window early. */
/* the window's end: the task is blocking, so there is one scan at
 * a time */
static retro_time_t task_bluetooth_scan_deadline;

typedef struct
{
   struct bluetooth_hold *hold;
   bool begun;
} task_bluetooth_scan_t;

static void task_bluetooth_scan_handler(retro_task_t *task)
{
   task_bluetooth_scan_t *scan = (task_bluetooth_scan_t*)task->state;

   if (!scan->begun)
   {
      driver_bluetooth_scan_begin(scan->hold);
      scan->begun = true;
      task_bluetooth_scan_deadline = cpu_features_get_time_usec()
         + BLUETOOTH_SCAN_WINDOW_US;
      task_queue_park(task, BLUETOOTH_SCAN_POLL_US);
      return;
   }

   /* The rest of the window waited out parked, looked at every
    * BLUETOOTH_SCAN_POLL_US where the driver can say it has found all
    * it will.  A cancel wakes it and ends the window. */
   if (     !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
         && !driver_bluetooth_scan_done(scan->hold))
   {
      retro_time_t now = cpu_features_get_time_usec();
      /* the queue runs a parked task up to half a millisecond early:
       * what is left within a millisecond of the end is not waited */
      if (now + 1000 < task_bluetooth_scan_deadline)
      {
         retro_time_t wait = task_bluetooth_scan_deadline - now;
         if (wait > BLUETOOTH_SCAN_POLL_US)
            wait = BLUETOOTH_SCAN_POLL_US;
         task_queue_park(task, wait);
         return;
      }
   }

   driver_bluetooth_scan_end(scan->hold);
   driver_bluetooth_scan_release(scan->hold);
   free(scan);
   task->state = NULL;

   task_set_progress(task, 100);
   task_free_title(task);
   task_set_title(task, strdup(msg_hash_to_str(MSG_BLUETOOTH_SCAN_COMPLETE)));
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

bool task_push_bluetooth_scan(retro_task_callback_t cb)
{
   retro_task_t   *task = task_init();
   task_bluetooth_scan_t *scan;

   if (!task)
      return false;
   if (!(scan = (task_bluetooth_scan_t*)calloc(1, sizeof(*scan))))
   {
      free(task);
      return false;
   }

   /* blocking means no other task can run while this one is running,
    * which is the default */
   task->type           = TASK_TYPE_BLOCKING;
   task->state          = scan;
   task->handler        = task_bluetooth_scan_handler;
   task->callback       = cb;
   task->title          = strdup(msg_hash_to_str(
                           MSG_SCANNING_BLUETOOTH_DEVICES));

   /* taken here, on the frontend's thread, where the driver is
    * stopped: the scan's thread cannot find it half gone */
   scan->hold = driver_bluetooth_scan_hold();
   if (task_queue_push(task))
      return true;

   /* Another blocking task is already active */
   driver_bluetooth_scan_release(scan->hold);
   free(scan);
   task_free_title(task);
   free(task);
   return false;
}
