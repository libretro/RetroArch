/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Higor Euripedes
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../runloop.h"

static void task_queue_msg_push(unsigned prio, unsigned duration,
      bool flush, const char *fmt, ...)
{
   char buf[1024];
   va_list ap;
   
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   runloop_msg_queue_push(buf, prio, duration, flush);
}

void task_queue_push_progress(retro_task_t *task)
{
   if (task->title)
   {
      if (task->finished)
      {
         if (task->error)
            task_queue_msg_push(1, 60, true, "%s: %s",
               msg_hash_to_str(MSG_TASK_FAILED), task->title);
         else
            task_queue_msg_push(1, 60, true, "100%%: %s", task->title);
      }
      else
      {
         if (task->progress >= 0 && task->progress <= 100)
            task_queue_msg_push(1, 60, true, "%i%%: %s",
                  task->progress, task->title);
         else
            task_queue_msg_push(1, 60, true, "%s...", task->title);
      }
   }
}
