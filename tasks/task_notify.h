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

#ifndef __TASK_NOTIFY_H
#define __TASK_NOTIFY_H

#include <stddef.h>

#include <retro_common_api.h>
#include <retro_inline.h>
#include <queues/task_queue.h>

RETRO_BEGIN_DECLS

/* A caller waiting to be told that some work is through, as a task
 * callback is: once, with error set if it failed. */
typedef struct task_notify
{
   retro_task_callback_t cb;
   void *user_data;
} task_notify_t;

/* Tells the caller waiting on @n, if any. Cleared first, so that the
 * callback may wait on @n again. */
static INLINE void task_notify_fire(task_notify_t *n, void *task_data,
      const char *error)
{
   retro_task_callback_t cb = n->cb;
   n->cb                    = NULL;
   if (cb)
      cb(NULL, task_data, n->user_data, error);
}

/* Has @cb wait on @n; a caller already waiting is told it was
 * superseded. */
static INLINE void task_notify_set(task_notify_t *n,
      retro_task_callback_t cb, void *user_data)
{
   task_notify_fire(n, NULL, "Superseded by another request.");
   n->cb        = cb;
   n->user_data = user_data;
}

RETRO_END_DECLS

#endif
