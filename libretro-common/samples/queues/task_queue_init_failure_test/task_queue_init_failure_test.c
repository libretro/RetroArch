/* Regression test for threaded initialisation failure in
 * libretro-common/queues/task_queue.c.
 *
 * task_queue_init() selects the threaded implementation before calling
 * its init hook, so an init that could not produce a worker used to
 * leave the queue pointed at an implementation with nobody servicing
 * it: tasks were accepted and then sat there forever. slock_new() and
 * scond_new() return NULL on allocation or platform failure, and
 * sthread_create() returns NULL when a thread cannot be started, so
 * this is reachable without anything exotic.
 *
 * The contract this pins:
 *
 *   init succeeds  -> the queue is threaded and the worker runs tasks
 *   init fails     -> the queue reports itself not threaded, and tasks
 *                     pushed to it still run on the caller's thread
 *
 * sthread_create() is wrapped so the failure is produced on demand
 * rather than waited for. The same binary exercises both paths.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <queues/task_queue.h>

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

/* --- forced sthread_create() failure --------------------------------- */

static bool deny_thread_create = false;

sthread_t *__real_sthread_create(void (*thread_func)(void*), void *userdata);

sthread_t *__wrap_sthread_create(void (*thread_func)(void*), void *userdata)
{
   if (deny_thread_create)
      return NULL;
   return __real_sthread_create(thread_func, userdata);
}

/* --- a task that records whether it ran ------------------------------ */

static bool task_ran      = false;
static bool task_finished = false;

static void test_task_handler(retro_task_t *task)
{
   task_ran        = true;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void test_task_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   task_finished = true;
}

static void push_one_task(void)
{
   retro_task_t *task = task_init();

   task_ran        = false;
   task_finished   = false;

   task->handler   = test_task_handler;
   task->callback  = test_task_callback;

   task_queue_push(task);
}

/* Drains the queue. The threaded worker needs a moment; the inline
 * implementation completes within the first check. */
static void drain(void)
{
   unsigned i;

   for (i = 0; i < 200 && !task_finished; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
}

int main(void)
{
   printf("threaded init succeeds\n");
   deny_thread_create = false;
   task_queue_init(true, NULL);
   check(task_queue_is_threaded(),
         "a queue that started its worker reports itself threaded");
   push_one_task();
   drain();
   check(task_ran, "the worker ran the task");
   check(task_finished, "the task reached its callback");
   task_queue_deinit();

   printf("threaded init cannot start a worker\n");
   deny_thread_create = true;
   task_queue_init(true, NULL);
   check(!task_queue_is_threaded(),
         "a queue with no worker does not report itself threaded");
   push_one_task();
   drain();
   check(task_ran,
         "the task still ran, rather than being queued for a worker that does not exist");
   check(task_finished, "the task reached its callback");
   task_queue_deinit();

   /* A failed attempt must not poison the next one. */
   printf("recovery after a failed init\n");
   deny_thread_create = false;
   task_queue_init(true, NULL);
   check(task_queue_is_threaded(),
         "a later init starts a worker again");
   push_one_task();
   drain();
   check(task_finished, "the task reached its callback");
   task_queue_deinit();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("task queue init failure handled\n");
   return 0;
}
