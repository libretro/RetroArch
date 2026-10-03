/* Regression test for how task_queue_deinit() stops the threaded
 * worker in libretro-common/queues/task_queue.c.
 *
 * Deinit used to join the worker with no limit, so a handler blocked
 * in an OS call - a DirectInput device walk held by a Bluetooth stack
 * still coming up, say - held quit and every reinit of the queue for
 * as long as it stayed blocked.
 *
 * The contract this pins:
 *
 *   idle worker                -> deinit returns at once
 *   detachable handler stuck   -> deinit returns after its bound; the
 *                                 task is dropped, its callback never
 *                                 runs, a new queue works meanwhile,
 *                                 and the handler finishing later
 *                                 touches nothing of it
 *   other handler past bound   -> deinit still waits for it, since
 *                                 its handler may use what teardown
 *                                 frees next
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_timers.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>
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

/* --- a handler that blocks until released ---------------------------- */

static retro_atomic_int_t blocked_entered  = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t blocked_release  = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t blocked_returned = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t blocked_callback = RETRO_ATOMIC_INT_INITIALIZER(0);

static void blocked_handler(retro_task_t *task)
{
   retro_atomic_store_release_int(&blocked_entered, 1);
   while (!retro_atomic_load_acquire_int(&blocked_release))
      retro_sleep(1);
   /* What an orphaned handler is still allowed to do. */
   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   retro_atomic_store_release_int(&blocked_returned, 1);
}

static void blocked_cb(retro_task_t *task, void *td, void *ud, const char *e)
{
   retro_atomic_store_release_int(&blocked_callback, 1);
}

/* --- a handler that takes a fixed time, then finishes ---------------- */

#define SLOW_HANDLER_MS 1500

static retro_atomic_int_t slow_entered  = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t slow_returned = RETRO_ATOMIC_INT_INITIALIZER(0);

static void slow_handler(retro_task_t *task)
{
   retro_atomic_store_release_int(&slow_entered, 1);
   retro_sleep(SLOW_HANDLER_MS);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   retro_atomic_store_release_int(&slow_returned, 1);
}

/* --- a quick task, to prove a queue works ---------------------------- */

static retro_atomic_int_t quick_callback = RETRO_ATOMIC_INT_INITIALIZER(0);

static void quick_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void quick_cb(retro_task_t *task, void *td, void *ud, const char *e)
{
   retro_atomic_store_release_int(&quick_callback, 1);
}

static void push(retro_task_handler_t h, retro_task_callback_t cb,
      bool detachable)
{
   retro_task_t *task = task_init();
   task->handler      = h;
   task->callback     = cb;
   task->flags       |= RETRO_TASK_FLG_MUTE;
   if (detachable)
      task->flags    |= RETRO_TASK_FLG_DETACHABLE;
   task_queue_push(task);
}

static void wait_for(retro_atomic_int_t *flag)
{
   while (!retro_atomic_load_acquire_int(flag))
      retro_sleep(1);
}

static int64_t ms_since(retro_time_t start)
{
   return (int64_t)((cpu_features_get_time_usec() - start) / 1000);
}

int main(void)
{
   retro_time_t start;
   int64_t      took;
   int          i;

   printf("idle worker\n");
   task_queue_init(true, NULL);
   check(task_queue_is_threaded(), "queue is threaded");
   start = cpu_features_get_time_usec();
   task_queue_deinit();
   took  = ms_since(start);
   check(took < 200, "deinit of an idle worker returns at once");

   printf("detachable handler stuck past the bound\n");
   task_queue_init(true, NULL);
   push(blocked_handler, blocked_cb, true);
   wait_for(&blocked_entered);
   start = cpu_features_get_time_usec();
   task_queue_deinit();
   took  = ms_since(start);
   check(took >= 900 && took < 3000, "deinit gives up after its bound");
   check(!retro_atomic_load_acquire_int(&blocked_returned),
         "the handler was still blocked when deinit returned");

   /* A new queue while the orphan is still running. */
   task_queue_init(true, NULL);
   push(quick_handler, quick_cb, false);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&quick_callback); i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   check(retro_atomic_load_acquire_int(&quick_callback),
         "a new queue runs tasks while the orphan is blocked");

   /* Let the orphan finish while the new queue is live; it may set its
    * own task's properties and must touch nothing else. */
   retro_atomic_store_release_int(&blocked_release, 1);
   wait_for(&blocked_returned);
   for (i = 0; i < 200; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   check(!retro_atomic_load_acquire_int(&blocked_callback),
         "the dropped task's callback never runs");

   start = cpu_features_get_time_usec();
   task_queue_deinit();
   check(ms_since(start) < 200, "the new queue still deinits at once");

   printf("non-detachable handler past the bound\n");
   task_queue_init(true, NULL);
   push(slow_handler, NULL, false);
   wait_for(&slow_entered);
   start = cpu_features_get_time_usec();
   task_queue_deinit();
   took  = ms_since(start);
   check(retro_atomic_load_acquire_int(&slow_returned),
         "deinit waited for a handler that was not detachable");
   check(took >= SLOW_HANDLER_MS - 200, "and waited past the bound to do it");

   /* Give a leftover orphan, if any, time to be seen by the sanitizers. */
   retro_sleep(50);

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
