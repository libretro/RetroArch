/* Regression test for switching libretro-common/queues/task_queue.c
 * from the regular implementation to the threaded one while tasks are
 * queued.
 *
 * task_queue_check() switches implementations when the threaded flag
 * changes, as the Threaded Data Runloop setting does at runtime. The
 * regular queue leaves its tasks on the shared lists, and the threaded
 * one takes them over. It counted only the tasks pushed to it, so each
 * inherited task took the running count one below zero when it
 * finished. The gather sizes its report list from that count plus a
 * margin; past the margin the unsigned size wrapped and it asked
 * realloc() for about 32 GiB on every check.
 *
 * realloc() is wrapped: a request that size is recorded and refused,
 * as it would be on a machine without the memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include <queues/task_queue.h>

/* More than the gather's margin of eight, so an undercount wraps. */
#define TASKS 16

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   if (cond)
      printf("  [pass] %s\n", what);
   else
   {
      printf("  [FAIL] %s\n", what);
      failures++;
   }
}

/* --- realloc() that records and refuses an absurd request ------------ */

static size_t largest_request = 0;

void *__real_realloc(void *ptr, size_t size);

void *__wrap_realloc(void *ptr, size_t size)
{
   if (size > largest_request)
      largest_request = size;
   if (size > ((size_t)1 << 30))
      return NULL;
   return __real_realloc(ptr, size);
}

/* --- tasks that finish on their first run ---------------------------- */

static unsigned tasks_done = 0;

static void test_task_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void test_task_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   tasks_done++;
}

static void push_tasks(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      retro_task_t *task = task_init();
      task->handler      = test_task_handler;
      task->callback     = test_task_callback;
      task_queue_push(task);
   }
}

static void drain(unsigned want)
{
   unsigned i;
   for (i = 0; i < 2000 && tasks_done < want; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
}

int main(void)
{
   printf("regular queue with tasks queued, then switched to threaded\n");
   task_queue_init(false, NULL);
   push_tasks(TASKS);
   task_queue_set_threaded();
   drain(TASKS);
   check(task_queue_is_threaded(), "the check switched the queue to threaded");
   check(tasks_done == TASKS,
         "every task queued before the switch reached its callback");
   check(largest_request <= ((size_t)1 << 20),
         "the gather never sized its report list from a negative count");

   /* The count has to be back at zero, not merely below the margin:
    * a queue that keeps working afterwards shows it. */
   tasks_done      = 0;
   largest_request = 0;
   push_tasks(TASKS);
   drain(TASKS);
   check(tasks_done == TASKS, "tasks pushed after the switch run");
   check(largest_request <= ((size_t)1 << 20),
         "and the report list stays small");
   task_queue_deinit();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("task queue switch keeps its counts\n");
   return 0;
}
