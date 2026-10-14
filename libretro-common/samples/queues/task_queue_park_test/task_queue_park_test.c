/* task_queue_park() / task_queue_wake(): a handler that returns
 * unfinished because it waits on something is not run again until that
 * something wakes it, it is cancelled, or its horizon passes.
 *
 * Lanes, on the threaded runner (and on GCD when built with GCD=1):
 *  - woken from another thread: the handler runs twice, not once per
 *    worker spin, and is not held to its horizon;
 *  - a wake that lands between the handler's check and its park
 *    cancels that park;
 *  - a parent woken from its child's callback, which the queue runs on
 *    the checking thread at the child's retire;
 *  - a cancel wakes a parked task, and so does a reset;
 *  - task_queue_wait() counts a parked task as running, and sees it
 *    through to finished;
 *  - with no wake, the horizon runs the handler again, and not before;
 *  - deinit does not wait out a parked task, and the next init runs it.
 * On the unthreaded runner a park changes nothing: the handler runs
 * once per check, as every handler does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <queues/task_queue.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

/* Long enough that a lane only passes if something other than the
 * horizon ran the handler again. */
#define LONG_HORIZON  10000000
/* Longest a lane waits on anything before calling it a failure. */
#define LANE_DEADLINE 3000000

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

struct waiter
{
   retro_task_t      *child;      /* parent lane: the task it waits on */
   uint32_t           ident;
   retro_atomic_int_t ready;      /* what the handler waits for */
   retro_atomic_int_t passes;
   retro_atomic_int_t retired;
   retro_time_t       horizon;
   int                wake_inside;/* lost-wakeup lane: wake mid-pass */
};

static void waiter_handler(retro_task_t *task)
{
   struct waiter *w = (struct waiter*)task->user_data;

   retro_atomic_fetch_add_int(&w->passes, 1);

   if (     retro_atomic_load_acquire_int(&w->ready)
         || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   /* The handler has looked and seen nothing; the waker publishes and
    * wakes before the park below is asked for. */
   if (w->wake_inside)
   {
      w->wake_inside = 0;
      retro_atomic_store_release_int(&w->ready, 1);
      task_queue_wake(task->ident);
   }

   task_queue_park(task, w->horizon);
}

static void waiter_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   struct waiter *w = (struct waiter*)user_data;
   (void)task;
   (void)task_data;
   (void)err;
   retro_atomic_fetch_add_int(&w->retired, 1);
}

static retro_task_t *push_waiter_with(struct waiter *w, retro_time_t horizon,
      retro_task_handler_t handler)
{
   retro_task_t *task = task_init();
   if (!task)
      return NULL;
   retro_atomic_int_init(&w->ready, 0);
   retro_atomic_int_init(&w->passes, 0);
   retro_atomic_int_init(&w->retired, 0);
   w->horizon      = horizon;
   w->wake_inside  = 0;
   w->child        = NULL;
   w->ident        = task->ident;
   task->handler   = handler;
   task->callback  = waiter_callback;
   task->user_data = w;
   task->flags    |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
   return task;
}

static retro_task_t *push_waiter(struct waiter *w, retro_time_t horizon)
{
   return push_waiter_with(w, horizon, waiter_handler);
}

/* Checks until @w retires or the deadline; returns how long it took. */
static retro_time_t pump(struct waiter *w)
{
   retro_time_t start = cpu_features_get_time_usec();
   while (!retro_atomic_load_acquire_int(&w->retired))
   {
      if (cpu_features_get_time_usec() - start > LANE_DEADLINE)
         break;
      task_queue_check();
      retro_sleep(1);
   }
   return cpu_features_get_time_usec() - start;
}

/* Lets the worker run the first pass and park. */
static void settle_first_pass(struct waiter *w)
{
   retro_time_t start = cpu_features_get_time_usec();
   while (retro_atomic_load_acquire_int(&w->passes) < 1
         && cpu_features_get_time_usec() - start < LANE_DEADLINE)
   {
      task_queue_check();
      retro_sleep(1);
   }
   retro_sleep(20);
}

/* -- woken from another thread -- */

static void wake_later(void *data)
{
   struct waiter *w = (struct waiter*)data;
   retro_sleep(30);
   retro_atomic_store_release_int(&w->ready, 1);
   task_queue_wake(w->ident);
}

static void lane_wake_from_thread(void)
{
   struct waiter w;
   unsigned      had = failures;
   sthread_t    *th;
   retro_time_t  took;

   CHECK(push_waiter(&w, LONG_HORIZON) != NULL, "push");
   th   = sthread_create(wake_later, &w);
   CHECK(th != NULL, "sthread_create");
   took = pump(&w);
   if (th)
      sthread_join(th);

   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1,
         "the woken task did not retire (%lld us)", (long long)took);
   CHECK(retro_atomic_load_acquire_int(&w.passes) <= 3,
         "the handler ran %d times while parked",
         retro_atomic_load_acquire_int(&w.passes));
   if (failures == had)
      fprintf(stderr, "[pass] wake from another thread: %d passes, %lld us\n",
            retro_atomic_load_acquire_int(&w.passes), (long long)took);
}

/* -- a wake between the check and the park -- */

static void lane_wake_inside_pass(void)
{
   struct waiter w;
   unsigned      had = failures;
   retro_time_t  took;

   CHECK(push_waiter(&w, LONG_HORIZON) != NULL, "push");
   w.wake_inside = 1;
   took = pump(&w);

   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1,
         "a wake during the pass was lost to the park after it (%lld us)",
         (long long)took);
   CHECK(retro_atomic_load_acquire_int(&w.passes) == 2,
         "the handler ran %d times, expected 2",
         retro_atomic_load_acquire_int(&w.passes));
   if (failures == had)
      fprintf(stderr, "[pass] wake during the pass cancels its park\n");
}

/* -- a parent woken by its child's callback -- */

static void child_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void child_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   struct waiter *w = (struct waiter*)user_data;
   (void)task;
   (void)task_data;
   (void)err;
   retro_atomic_store_release_int(&w->ready, 1);
   task_queue_wake(w->ident);
}

static void parent_handler(retro_task_t *task)
{
   struct waiter *w = (struct waiter*)task->user_data;

   if (retro_atomic_fetch_add_int(&w->passes, 1) == 0)
   {
      retro_task_t *child = task_init();
      if (child)
      {
         child->handler   = child_handler;
         child->callback  = child_callback;
         child->user_data = w;
         child->flags    |= RETRO_TASK_FLG_MUTE;
         task_queue_push(child);
      }
   }

   if (retro_atomic_load_acquire_int(&w->ready))
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   task_queue_park(task, w->horizon);
}

static void lane_wake_from_child_callback(void)
{
   struct waiter w;
   unsigned      had = failures;
   retro_task_t *task;
   retro_time_t  took;

   CHECK((task = push_waiter_with(&w, LONG_HORIZON, parent_handler)) != NULL,
         "push");
   took = pump(&w);

   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1,
         "the parent was not woken by its child's callback (%lld us)",
         (long long)took);
   CHECK(retro_atomic_load_acquire_int(&w.passes) <= 3,
         "the parent ran %d times while its child ran",
         retro_atomic_load_acquire_int(&w.passes));
   if (failures == had)
      fprintf(stderr, "[pass] parent woken from its child's callback: %d passes\n",
            retro_atomic_load_acquire_int(&w.passes));
}

/* -- cancel and reset -- */

static void lane_cancel_wakes(bool reset)
{
   struct waiter w;
   unsigned      had = failures;
   retro_task_t *task;
   retro_time_t  took;

   CHECK((task = push_waiter(&w, LONG_HORIZON)) != NULL, "push");
   settle_first_pass(&w);
   if (reset)
      task_queue_reset();
   else
      task_queue_cancel_task(task);
   took = pump(&w);

   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1,
         "a parked task did not see its %s before the horizon (%lld us)",
         reset ? "reset" : "cancel", (long long)took);
   if (failures == had)
      fprintf(stderr, "[pass] %s wakes a parked task\n",
            reset ? "reset" : "cancel");
}

/* -- task_queue_wait() counts a parked task -- */

static bool wait_cond(void *data)
{
   struct waiter *w = (struct waiter*)data;
   return !retro_atomic_load_acquire_int(&w->retired);
}

/* While armed, every queue lock taken off this thread waits 20 ms
 * first, so a runner that let go of one list before taking the next
 * would leave a finishing task in neither for long enough for a wait to
 * see it. */
static retro_atomic_int_t widen;
static uintptr_t          main_tid;
#ifdef PARK_TEST_WRAP_SLOCK
void __real_slock_lock(slock_t *lock);
void __wrap_slock_lock(slock_t *lock)
{
   if (     retro_atomic_load_acquire_int(&widen)
         && sthread_get_current_thread_id() != main_tid)
      retro_sleep(20);
   __real_slock_lock(lock);
}
#endif

static void lane_wait_counts_parked(void)
{
   struct waiter w;
   unsigned      had = failures;
   sthread_t    *th;

   CHECK(push_waiter(&w, LONG_HORIZON) != NULL, "push");
   settle_first_pass(&w);
   th = sthread_create(wake_later, &w);
   CHECK(th != NULL, "sthread_create");
   /* A parked task is still running: the wait has to see it through,
    * and the task never leaves both lists on its way to finished. */
   main_tid = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&widen, 1);
   task_queue_wait(wait_cond, &w);
   retro_atomic_store_release_int(&widen, 0);
   if (th)
      sthread_join(th);

   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1,
         "task_queue_wait() returned with a parked task still running");
   if (failures == had)
      fprintf(stderr, "[pass] task_queue_wait() waits out a parked task\n");
}

/* -- the horizon is a watchdog -- */

static void lane_horizon(void)
{
   struct waiter w;
   unsigned      had = failures;
   retro_time_t  start;
   int           passes;

   CHECK(push_waiter(&w, 50000) != NULL, "push");
   start = cpu_features_get_time_usec();
   while (cpu_features_get_time_usec() - start < 300000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   passes = retro_atomic_load_acquire_int(&w.passes);
   /* 300 ms at 50 ms a park: about six passes; a spinning worker makes
    * thousands, and one that never came back makes one. */
   CHECK(passes >= 3 && passes <= 10,
         "%d passes in 300 ms with a 50 ms horizon", passes);
   retro_atomic_store_release_int(&w.ready, 1);
   task_queue_wake(w.ident);
   pump(&w);
   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1, "did not retire");
   if (failures == had)
      fprintf(stderr, "[pass] horizon: %d passes in 300 ms\n", passes);
}

/* -- the queue going down with a task parked -- */

/* Deinit does not wait out a parked task's horizon; the task stays on
 * the list, and the next init runs it again. */
static void lane_deinit_while_parked(void)
{
   struct waiter w;
   unsigned      had = failures;
   retro_time_t  start;
   retro_time_t  took;

   CHECK(push_waiter(&w, LONG_HORIZON) != NULL, "push");
   settle_first_pass(&w);

   start = cpu_features_get_time_usec();
   task_queue_deinit();
   took  = cpu_features_get_time_usec() - start;
   CHECK(took < 500000,
         "deinit waited %lld us on a parked task", (long long)took);

   task_queue_init(true, NULL);
   retro_atomic_store_release_int(&w.ready, 1);
   task_queue_wake(w.ident);
   pump(&w);
   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1,
         "the parked task did not run again after a new init");
   if (failures == had)
      fprintf(stderr, "[pass] deinit lets a parked task go (%lld us)\n",
            (long long)took);
}

/* -- unthreaded: a park changes nothing -- */

static void lane_unthreaded(void)
{
   struct waiter w;
   unsigned      had = failures;
   unsigned      i;

   CHECK(push_waiter(&w, LONG_HORIZON) != NULL, "push");
   for (i = 1; i <= 3; i++)
   {
      task_queue_check();
      CHECK(retro_atomic_load_acquire_int(&w.passes) == (int)i,
            "check %u: handler ran %d times", i,
            retro_atomic_load_acquire_int(&w.passes));
   }
   retro_atomic_store_release_int(&w.ready, 1);
   task_queue_wake(w.ident);
   task_queue_check();
   task_queue_check();
   CHECK(retro_atomic_load_acquire_int(&w.retired) == 1, "did not retire");
   if (failures == had)
      fprintf(stderr, "[pass] unthreaded: a park leaves once-per-check alone\n");
}

int main(void)
{
   task_queue_init(true, NULL);
   lane_wake_from_thread();
   lane_wake_inside_pass();
   lane_wake_from_child_callback();
   lane_cancel_wakes(false);
   lane_cancel_wakes(true);
   lane_wait_counts_parked();
   lane_horizon();
   lane_deinit_while_parked();
   task_queue_deinit();

   task_queue_init(false, NULL);
   lane_unthreaded();
   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}
