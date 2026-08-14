/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (task_nbio_slice.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The shared per-frame I/O window used by budgeted task handlers:
 * the file-transfer spine (task_file_transfer.c) and the manual
 * content scanner's BEGIN states (task_database.c).  A TU of its
 * own so its only dependencies are the clock and the task queue,
 * which lets test harnesses link the real window rather than a
 * reimplementation. */

#include <features/features_cpu.h>
#include <queues/task_queue.h>

#include "tasks_internal.h"

/* At most NBIO_XFER_TICK_USEC of budgeted work per
 * NBIO_XFER_TICK_PERIOD_USEC of wall time, shared by every
 * participating task in a tick rather than handed to each one
 * separately.
 *
 * retro_task_regular_gather() calls every running task's handler once
 * per task_queue_check(), i.e. once per frame, so a per-task slice
 * multiplies: eight concurrent loads spent 30.65 ms in a single
 * frame, four already spent 15.07 ms, and it scaled linearly with
 * the task count.  A window is the fix - whatever the number of
 * tasks, the frame pays the window once.
 *
 * A tick has no identity this file can see, so the window is a plain
 * fixed one rather than something keyed to the gather: consumed
 * microseconds accumulate until a period has elapsed, then reset.
 * It over-counts nothing (only time actually spent working is
 * charged) and needs no cooperation from task_queue.
 *
 * Every open grants a floor of one work item regardless of the
 * window, or a queue whose budget is already spent would make no
 * progress at all.  The frame is then bounded by the window plus one
 * work item per task - a few hundred microseconds for a queue of
 * dozens - rather than by the task count times the whole slice. */
#define NBIO_XFER_TICK_USEC        4000
#define NBIO_XFER_TICK_PERIOD_USEC 16666

/* Window state.  Only ever touched on the thread that runs handlers,
 * and only when that is the frame thread - see
 * task_nbio_slice_open(). */
static retro_time_t nbio_slice_start;
static retro_time_t nbio_slice_used;

/* The budget is handed to the work loop rather than checked around
 * it, so the deadline is noticed between the loop's own items - not
 * once per whole chunk, however long a chunk takes to arrive. */
bool task_nbio_slice_within_budget(void *ud, size_t avail,
      size_t len)
{
   nbio_budget_t *b = (nbio_budget_t*)ud;
   (void)avail;
   (void)len;
   if (b->floor)
   {
      b->floor = 0;
      return true;
   }
   return cpu_features_get_time_usec() - b->start < b->allowance;
}

/* Claim this task's share of the window.
 *
 * Declined entirely under a threaded task queue.  There the handler
 * runs on the worker thread, which rotates the task and re-enters
 * immediately; there is no frame to protect, slicing buys only
 * round-robin fairness between tasks, and a shared static across
 * threads would be a race for no benefit.  Each task gets the whole
 * slice there, which is what it effectively had before. */
void task_nbio_slice_open(nbio_budget_t *b)
{
   retro_time_t now = cpu_features_get_time_usec();

   b->start = now;
   b->floor = 1;

   if (task_queue_is_threaded())
   {
      b->allowance = NBIO_XFER_TICK_USEC;
      return;
   }
   if (now - nbio_slice_start >= (retro_time_t)NBIO_XFER_TICK_PERIOD_USEC)
   {
      nbio_slice_start = now;
      nbio_slice_used  = 0;
   }
   b->allowance = (nbio_slice_used < (retro_time_t)NBIO_XFER_TICK_USEC)
      ? (retro_time_t)NBIO_XFER_TICK_USEC - nbio_slice_used
      : 0;
}

/* Charge back what the work actually spent, including the floor item
 * that ran outside the allowance. */
void task_nbio_slice_close(nbio_budget_t *b)
{
   if (task_queue_is_threaded())
      return;
   nbio_slice_used += cpu_features_get_time_usec() - b->start;
}
