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
 *
 * Then parked tasks are woken from other threads while the queue
 * switches back and forth. No switch may free the threaded runner's lock
 * and condition variable under a waker inside task_queue_wake().
 * slock_lock() is wrapped too, so that one waker can be held there
 * while a switch comes due. A wake on the main thread is not held
 * back by that switch, and a switch ends a park.
 *
 * A wake from a task's first pass on the new runner has to end the
 * park that pass asks for. slock_unlock() and sthread_create() are
 * wrapped as well, so that the main thread can be held where the new
 * runner first runs a task.
 *
 * Last, tasks are pushed from other threads: on the unthreaded queue,
 * whose lists take no lock, and all through a run of switches. Such a
 * push is held for the next check, and a second BLOCKING task held
 * there is cancelled, with its callback. A thread's next push does not
 * pass one of its own that is held.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include <features/features_cpu.h>
#include <queues/task_queue.h>
#include <rthreads/rthreads.h>

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
   /* A lane that meets the bug can crash the process. */
   fflush(stdout);
}

/* --- realloc() that records and refuses an absurd request ------------ */

static size_t   largest_request = 0;
static unsigned realloc_calls   = 0;

void *__real_realloc(void *ptr, size_t size);

void *__wrap_realloc(void *ptr, size_t size)
{
   realloc_calls++;
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

/* --- parked tasks woken from other threads across switches ---------- */

#define WAKERS        4
#define SWITCHES      1000
#define LIMIT_US      2000000
#define SHORT_PARK_US 2000
#define LONG_PARK_US  60000000

/* The probe parks briefly and records which side ran it last: 1 for
 * the main thread, 0 for the worker.  The sleeper parks for a minute,
 * so within a lane only a wake runs it again. */
static retro_atomic_int_t probe_ran_on;
static retro_atomic_int_t sleeper_passes;
static retro_atomic_int_t parked_stop;
static retro_atomic_int_t parked_retired;
static retro_atomic_int_t wakers_stop;
static uint32_t           probe_ident;
static uint32_t           sleeper_ident;

static void probe_handler(retro_task_t *task)
{
   if (retro_atomic_load_acquire_int(&parked_stop))
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   retro_atomic_store_release_int(&probe_ran_on,
         task_is_on_main_thread() ? 1 : 0);
   task_queue_park(task, SHORT_PARK_US);
}

/* Armed, the sleeper's next pass off the main thread parks and then
 * wakes itself, as a reader that publishes after the pass looked. */
static retro_atomic_int_t first_pass_armed;
static retro_atomic_int_t first_pass_done;
static int                first_pass_at;

static void sleeper_handler(retro_task_t *task)
{
   if (retro_atomic_load_acquire_int(&parked_stop))
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   retro_atomic_fetch_add_int(&sleeper_passes, 1);
   task_queue_park(task, LONG_PARK_US);
   if (     !task_is_on_main_thread()
         && retro_atomic_load_acquire_int(&first_pass_armed))
   {
      retro_atomic_store_release_int(&first_pass_armed, 0);
      first_pass_at = retro_atomic_load_acquire_int(&sleeper_passes);
      task_queue_wake(sleeper_ident);
      retro_atomic_store_release_int(&first_pass_done, 1);
   }
}

static void parked_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   retro_atomic_fetch_add_int(&parked_retired, 1);
}

static uint32_t push_parked(retro_task_handler_t handler)
{
   retro_task_t *task = task_init();
   uint32_t      ident;

   task->handler  = handler;
   task->callback = parked_callback;
   ident          = task->ident;
   task_queue_push(task);
   return ident;
}

static void one_wake(void *data)
{
   task_queue_wake(*(uint32_t*)data);
}

/* A wake from a thread other than the one that drives the queue. */
static void wake_from_thread(uint32_t ident)
{
   sthread_t *th = sthread_create(one_wake, &ident);

   if (th)
      sthread_join(th);
}

/* Checks the queue until the probe has run on @side, within the limit. */
static bool probe_runs_on(int side)
{
   retro_time_t until = cpu_features_get_time_usec() + LIMIT_US;

   while (retro_atomic_load_acquire_int(&probe_ran_on) != side)
   {
      if (cpu_features_get_time_usec() > until)
         return false;
      task_queue_check();
   }
   return true;
}

/* Checks the queue until the sleeper has run more than @passes times. */
static bool sleeper_runs_past(int passes)
{
   retro_time_t until = cpu_features_get_time_usec() + LIMIT_US;

   while (retro_atomic_load_acquire_int(&sleeper_passes) <= passes)
   {
      if (cpu_features_get_time_usec() > until)
         return false;
      task_queue_check();
      retro_sleep(1);
   }
   return true;
}

/* While armed, the holder's next queue lock waits for the release.  It
 * is then inside task_queue_wake(), with the runner's lock in hand but
 * not taken. */
static retro_atomic_int_t hold_armed;
static retro_atomic_int_t hold_inside;
static retro_atomic_int_t hold_release;
static uintptr_t          holder_tid;

void __real_slock_lock(slock_t *lock);

void __wrap_slock_lock(slock_t *lock)
{
   if (     retro_atomic_load_acquire_int(&hold_armed)
         && sthread_get_current_thread_id() == holder_tid)
   {
      retro_atomic_store_release_int(&hold_armed, 0);
      retro_atomic_store_release_int(&hold_inside, 1);
      while (!retro_atomic_load_acquire_int(&hold_release))
         retro_sleep(1);
   }
   __real_slock_lock(lock);
}

static void held_waker(void *data)
{
   (void)data;
   holder_tid = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&hold_armed, 1);
   task_queue_wake(sleeper_ident);
}

/* The setting asks for a switch while a waker is inside: the switch
 * waits, a wake on the main thread still goes through, and the setting
 * taken back opens the queue to wakes again. */
static void lane_held_waker(void)
{
   sthread_t   *th;
   retro_time_t until;
   unsigned     i;
   int          passes;
   bool         on_main = false;

   passes = retro_atomic_load_acquire_int(&sleeper_passes);
   th     = sthread_create(held_waker, NULL);
   until  = cpu_features_get_time_usec() + LIMIT_US;
   while (     th
            && !retro_atomic_load_acquire_int(&hold_inside)
            && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   check(retro_atomic_load_acquire_int(&hold_inside) == 1,
         "a waker is held inside the threaded queue");

   retro_atomic_store_release_int(&probe_ran_on, -1);
   task_queue_unset_threaded();
   for (i = 0; i < 100; i++)
   {
      task_queue_check();
      if (retro_atomic_load_acquire_int(&probe_ran_on) == 1)
         on_main = true;
      retro_sleep(1);
   }
   check(!on_main, "the switch waits while the waker is inside");

   passes = retro_atomic_load_acquire_int(&sleeper_passes);
   task_queue_wake(sleeper_ident);
   check(sleeper_runs_past(passes),
         "a wake on the main thread reaches its task while the switch waits");

   task_queue_set_threaded();
   task_queue_check();
   passes = retro_atomic_load_acquire_int(&sleeper_passes);
   retro_atomic_store_release_int(&hold_release, 1);
   if (th)
      sthread_join(th);
   check(sleeper_runs_past(passes), "the held wake reaches its task");

   passes = retro_atomic_load_acquire_int(&sleeper_passes);
   wake_from_thread(sleeper_ident);
   check(sleeper_runs_past(passes),
         "with the switch taken back, a wake reaches its task again");
}

static void waker(void *data)
{
   (void)data;
   while (!retro_atomic_load_acquire_int(&wakers_stop))
      task_queue_wake(probe_ident);
}

/* Wakers on other threads all through a run of switches: no wake may
 * reach a runner being torn down, and none may hold a switch off. */
static void lane_switch_storm(void)
{
   sthread_t   *th[WAKERS];
   retro_time_t started = cpu_features_get_time_usec();
   unsigned     i;
   unsigned     done = 0;
   int          passes;

   for (i = 0; i < WAKERS; i++)
      th[i] = sthread_create(waker, NULL);

   for (i = 0; i < SWITCHES; i++)
   {
      int side = (i & 1) ? 0 : 1;

      retro_atomic_store_release_int(&probe_ran_on, -1);
      if (side)
         task_queue_unset_threaded();
      else
         task_queue_set_threaded();
      if (!probe_runs_on(side))
         break;
      done++;
   }

   retro_atomic_store_release_int(&wakers_stop, 1);
   for (i = 0; i < WAKERS; i++)
      if (th[i])
         sthread_join(th[i]);
   printf("  [info] %u of %u switches in %lld ms\n", done, SWITCHES,
         (long long)((cpu_features_get_time_usec() - started) / 1000));
   check(done == SWITCHES,
         "every switch takes effect while other threads wake");

   passes = retro_atomic_load_acquire_int(&sleeper_passes);
   wake_from_thread(sleeper_ident);
   check(sleeper_runs_past(passes), "a wake reaches its task after them");
}

/* The sleeper, parked on the worker for a minute, runs at once when the
 * queue goes unthreaded.  Then back, so that it retires on the worker. */
static void lane_park_across_switch(void)
{
   int passes = retro_atomic_load_acquire_int(&sleeper_passes);

   retro_atomic_store_release_int(&probe_ran_on, -1);
   task_queue_unset_threaded();
   probe_runs_on(1);
   check(sleeper_runs_past(passes),
         "a park carried across a switch to unthreaded runs at once");

   retro_atomic_store_release_int(&probe_ran_on, -1);
   task_queue_set_threaded();
   probe_runs_on(0);
}

/* While set, the main thread waits after each queue unlock and each
 * thread start, until the armed pass has run or the hold runs out. */
#define HOLD_US 500000

static retro_atomic_int_t hold_for_pass;
static uintptr_t          main_tid;

static void hold_main_for_pass(void)
{
   retro_time_t until;

   if (     !retro_atomic_load_acquire_int(&hold_for_pass)
         || sthread_get_current_thread_id() != main_tid)
      return;
   until = cpu_features_get_time_usec() + HOLD_US;
   while (     !retro_atomic_load_acquire_int(&first_pass_done)
            && cpu_features_get_time_usec() < until)
      retro_sleep(1);
}

void __real_slock_unlock(slock_t *lock);

void __wrap_slock_unlock(slock_t *lock)
{
   __real_slock_unlock(lock);
   hold_main_for_pass();
}

sthread_t *__real_sthread_create(void (*thread_func)(void*), void *userdata);

sthread_t *__wrap_sthread_create(void (*thread_func)(void*), void *userdata)
{
   sthread_t *th = __real_sthread_create(thread_func, userdata);
   hold_main_for_pass();
   return th;
}

/* The switch to the threaded queue holds the main thread where the new
 * runner can first run a task, while the sleeper's first pass there
 * wakes itself.  A runner that ran it before taking wakes drops that
 * wake, and the sleeper keeps its minute. */
static void lane_first_pass_wake(void)
{
   retro_time_t until;

   retro_atomic_store_release_int(&probe_ran_on, -1);
   task_queue_unset_threaded();
   probe_runs_on(1);

   retro_atomic_store_release_int(&first_pass_done, 0);
   retro_atomic_store_release_int(&first_pass_armed, 1);
   retro_atomic_store_release_int(&hold_for_pass, 1);
   task_queue_set_threaded();
   task_queue_check();
   retro_atomic_store_release_int(&hold_for_pass, 0);

   until = cpu_features_get_time_usec() + LIMIT_US;
   while (     !retro_atomic_load_acquire_int(&first_pass_done)
            && cpu_features_get_time_usec() < until)
   {
      task_queue_check();
      retro_sleep(1);
   }
   check(retro_atomic_load_acquire_int(&first_pass_done) == 1,
         "the sleeper's first pass on the threaded queue wakes it");
   check(retro_atomic_load_acquire_int(&first_pass_done) == 1
         && sleeper_runs_past(first_pass_at),
         "and that wake ends the park the pass asked for");
}

static void retire_parked(void)
{
   retro_time_t until = cpu_features_get_time_usec() + LIMIT_US;

   retro_atomic_store_release_int(&parked_stop, 1);
   task_queue_wake(probe_ident);
   task_queue_wake(sleeper_ident);
   while (     retro_atomic_load_acquire_int(&parked_retired) < 2
            && cpu_features_get_time_usec() < until)
   {
      task_queue_check();
      retro_sleep(1);
   }
   check(retro_atomic_load_acquire_int(&parked_retired) == 2,
         "both parked tasks retire");
}

/* --- tasks pushed from other threads ---------------------------------- */

#define PUSHERS         4
#define PUSHES_A_SWITCH 4
#define PUSHES          (PUSHES_A_SWITCH * SWITCHES)
#define DRAIN_US        5000000

/* How often each pushed task reached its callback, and in what order.
 * Callbacks run on the main thread. */
static unsigned           pushed_runs[PUSHERS * PUSHES];
static int                pushed_last[PUSHERS];
static unsigned           pushed_done;
static unsigned           pushed_out_of_order;
static unsigned           pusher_ids[PUSHERS];
/* The switches made so far.  A pusher keeps within PUSHES_A_SWITCH a
 * switch of it, so that its pushes fall all through the switches. */
static retro_atomic_int_t switches_done;

static void pushed_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   unsigned idx = (unsigned)((unsigned*)user_data - pushed_runs);
   unsigned who = idx / PUSHES;
   int      seq = (int)(idx % PUSHES);

   pushed_runs[idx]++;
   pushed_done++;
   if (seq != pushed_last[who] + 1)
      pushed_out_of_order++;
   pushed_last[who] = seq;
}

static void pusher(void *data)
{
   unsigned who = *(unsigned*)data;
   unsigned i;

   for (i = 0; i < PUSHES; i++)
   {
      retro_task_t *task;

      while ((int)(i / PUSHES_A_SWITCH)
            > retro_atomic_load_acquire_int(&switches_done))
         retro_sleep(1);
      task            = task_init();
      task->handler   = test_task_handler;
      task->callback  = pushed_callback;
      task->user_data = &pushed_runs[who * PUSHES + i];
      task_queue_push(task);
   }
}

static void pushers_start(sthread_t **th, bool paced)
{
   unsigned i;

   memset(pushed_runs, 0, sizeof(pushed_runs));
   for (i = 0; i < PUSHERS; i++)
   {
      pusher_ids[i]  = i;
      pushed_last[i] = -1;
   }
   pushed_done         = 0;
   pushed_out_of_order = 0;
   retro_atomic_store_release_int(&switches_done, paced ? 0 : SWITCHES);
   for (i = 0; i < PUSHERS; i++)
      th[i] = sthread_create(pusher, &pusher_ids[i]);
}

/* Joins the pushers and checks the queue until every task they pushed
 * has reached its callback: true when each did so once. */
static bool pushers_finish(sthread_t **th)
{
   retro_time_t until;
   unsigned     i;

   retro_atomic_store_release_int(&switches_done, SWITCHES);
   for (i = 0; i < PUSHERS; i++)
      if (th[i])
         sthread_join(th[i]);
   until = cpu_features_get_time_usec() + DRAIN_US;
   while (     pushed_done < PUSHERS * PUSHES
            && cpu_features_get_time_usec() < until)
   {
      task_queue_check();
      retro_sleep(1);
   }
   for (i = 0; i < PUSHERS * PUSHES; i++)
      if (pushed_runs[i] != 1)
         return false;
   return true;
}

static retro_atomic_int_t pushed_from_thread;

static void push_one(void *data)
{
   retro_atomic_store_release_int(&pushed_from_thread,
         task_queue_push((retro_task_t*)data) ? 1 : 0);
}

/* A push from a thread other than the one that drives the queue.  A
 * task it refuses is still the caller's. */
static bool push_from_thread(retro_task_t *task)
{
   sthread_t *th;

   retro_atomic_store_release_int(&pushed_from_thread, 0);
   th = sthread_create(push_one, task);
   if (!th)
      return false;
   sthread_join(th);
   return retro_atomic_load_acquire_int(&pushed_from_thread) == 1;
}

static retro_atomic_int_t blocker_release;
static unsigned           blocker_retired;
static unsigned           refused_runs;
static unsigned           refused_callbacks;
static bool               refused_cancelled;

static void blocker_handler(retro_task_t *task)
{
   if (retro_atomic_load_acquire_int(&blocker_release))
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void blocker_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   blocker_retired++;
}

static void refused_handler(retro_task_t *task)
{
   refused_runs++;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void refused_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   refused_callbacks++;
   refused_cancelled = (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED) != 0;
}

static retro_task_t *blocking_task(retro_task_handler_t handler,
      retro_task_callback_t callback)
{
   retro_task_t *task = task_init();

   task->type     = TASK_TYPE_BLOCKING;
   task->handler  = handler;
   task->callback = callback;
   return task;
}

/* On the unthreaded queue, with a BLOCKING task running: a second one
 * pushed on the main thread is refused at once.  One pushed
 * from another thread is taken, and the next check cancels it. */
static void lane_push_blocking(void)
{
   retro_task_t *task;
   bool          taken;
   unsigned      i;

   task_queue_push(blocking_task(blocker_handler, blocker_callback));
   task = blocking_task(refused_handler, NULL);
   if (!task_queue_push(task))
   {
      free(task);
      task = NULL;
   }
   check(!task, "a second BLOCKING push on the main thread is refused at once");

   task  = blocking_task(refused_handler, refused_callback);
   taken = push_from_thread(task);
   if (!taken)
      free(task);
   check(taken, "one from another thread is taken");
   for (i = 0; i < 10 && !refused_callbacks; i++)
      task_queue_check();
   check(refused_callbacks == 1 && refused_cancelled && !refused_runs,
         "the next check cancels it, and its callback runs");

   retro_atomic_store_release_int(&blocker_release, 1);
   for (i = 0; i < 100 && !blocker_retired; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   check(blocker_retired == 1, "the running BLOCKING task retires");
}

/* The unthreaded queue, checked all the while: its lists take no lock,
 * so no other thread may touch them. */
static void lane_push_unthreaded(void)
{
   sthread_t   *th[PUSHERS];
   retro_time_t until;

   pushers_start(th, false);
   until = cpu_features_get_time_usec() + LIMIT_US;
   while (     pushed_done < PUSHERS * PUSHES
            && cpu_features_get_time_usec() < until)
      task_queue_check();
   check(pushers_finish(th),
         "every push from another thread on the unthreaded queue runs once");
   check(!pushed_out_of_order, "in the order each thread pushed them");
}

/* Pushers on other threads all through a run of switches: no push may
 * reach a runner being torn down, nor the unthreaded queue's lists,
 * and none may be lost. */
static void lane_push_storm(void)
{
   sthread_t   *th[PUSHERS];
   retro_time_t started = cpu_features_get_time_usec();
   unsigned     i;
   unsigned     done = 0;

   pushers_start(th, true);
   for (i = 0; i < SWITCHES; i++)
   {
      int side = (i & 1) ? 1 : 0;

      retro_atomic_store_release_int(&probe_ran_on, -1);
      if (side)
         task_queue_unset_threaded();
      else
         task_queue_set_threaded();
      if (!probe_runs_on(side))
         break;
      done++;
      retro_atomic_store_release_int(&switches_done, (int)done);
   }
   printf("  [info] %u of %u switches with pushes in %lld ms\n", done,
         SWITCHES,
         (long long)((cpu_features_get_time_usec() - started) / 1000));
   check(done == SWITCHES,
         "every switch takes effect while other threads push");
   check(pushers_finish(th),
         "every task pushed from another thread reaches its callback once");
}

/* A push from another thread, held by the unthreaded queue, runs on
 * the threaded queue that the next check switches to. */
static void lane_push_carried(void)
{
   retro_task_t *task = task_init();
   retro_time_t  until;
   bool          taken;

   task->handler  = test_task_handler;
   task->callback = test_task_callback;
   tasks_done     = 0;
   taken          = push_from_thread(task);
   if (!taken)
      free(task);
   check(taken, "a push from another thread is held by the unthreaded queue");
   retro_atomic_store_release_int(&probe_ran_on, -1);
   task_queue_set_threaded();
   until = cpu_features_get_time_usec() + LIMIT_US;
   while (!tasks_done && cpu_features_get_time_usec() < until)
   {
      task_queue_check();
      retro_sleep(1);
   }
   check(tasks_done == 1,
         "a push held by the unthreaded queue runs after the switch to threaded");
   probe_runs_on(0);
}

static retro_atomic_int_t order_first_pushed;
static retro_atomic_int_t order_saw_hold;
static retro_atomic_int_t order_overtook;
static unsigned           order_retired;

static void order_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   order_retired++;
}

static bool order_finder(retro_task_t *task, void *data)
{
   return task == (retro_task_t*)data;
}

static retro_task_t *order_task(void)
{
   retro_task_t *task = task_init();

   task->handler  = test_task_handler;
   task->callback = order_callback;
   return task;
}

/* Its first push meets the gate shut and is held.  The second waits
 * until the main thread is held in the gather that opens the gate
 * again, before that gather has pushed the first, and then looks for
 * itself in the runner. */
static void order_pusher(void *data)
{
   retro_task_t      *first  = order_task();
   retro_task_t      *second = order_task();
   task_finder_data_t find;
   retro_time_t       until;

   (void)data;
   task_queue_push(first);
   retro_atomic_store_release_int(&order_first_pushed, 1);
   until = cpu_features_get_time_usec() + LIMIT_US;
   while (     !retro_atomic_load_acquire_int(&hold_inside)
            && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   retro_atomic_store_release_int(&order_saw_hold,
         retro_atomic_load_acquire_int(&hold_inside));
   task_queue_push(second);
   find.func     = order_finder;
   find.userdata = second;
   if (task_queue_find(&find))
   {
      find.userdata = first;
      retro_atomic_store_release_int(&order_overtook,
            task_queue_find(&find) ? 0 : 1);
   }
   retro_atomic_store_release_int(&hold_release, 1);
}

/* A thread's push held at a put-off switch is not passed by its next
 * one, which meets the gate open again before the gather that opened
 * it has pushed the first. */
static void lane_push_order(void)
{
   sthread_t   *waker_th;
   sthread_t   *th;
   retro_time_t until;

   retro_atomic_store_release_int(&order_first_pushed, 0);
   retro_atomic_store_release_int(&order_saw_hold, 0);
   retro_atomic_store_release_int(&order_overtook, 0);
   order_retired = 0;

   /* A waker held inside puts the switch to unthreaded off, with the
    * gate shut. */
   retro_atomic_store_release_int(&hold_inside, 0);
   retro_atomic_store_release_int(&hold_release, 0);
   waker_th = sthread_create(held_waker, NULL);
   until    = cpu_features_get_time_usec() + LIMIT_US;
   while (     waker_th
            && !retro_atomic_load_acquire_int(&hold_inside)
            && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   check(retro_atomic_load_acquire_int(&hold_inside) == 1,
         "a waker is held inside the threaded queue");
   task_queue_unset_threaded();
   task_queue_check();
   retro_atomic_store_release_int(&hold_release, 1);
   if (waker_th)
      sthread_join(waker_th);

   retro_atomic_store_release_int(&hold_inside, 0);
   retro_atomic_store_release_int(&hold_release, 0);
   th    = sthread_create(order_pusher, NULL);
   until = cpu_features_get_time_usec() + LIMIT_US;
   while (     th
            && !retro_atomic_load_acquire_int(&order_first_pushed)
            && cpu_features_get_time_usec() < until)
      retro_sleep(1);

   /* Taken back: the check opens the gate, and its gather stops at its
    * first queue lock until the pusher has pushed again. */
   if (th)
   {
      holder_tid = sthread_get_current_thread_id();
      retro_atomic_store_release_int(&hold_armed, 1);
   }
   task_queue_set_threaded();
   task_queue_check();
   if (th)
      sthread_join(th);
   retro_atomic_store_release_int(&hold_armed, 0);

   until = cpu_features_get_time_usec() + LIMIT_US;
   while (order_retired < 2 && cpu_features_get_time_usec() < until)
   {
      task_queue_check();
      retro_sleep(1);
   }
   check(retro_atomic_load_acquire_int(&order_saw_hold) == 1,
         "the gather that takes a put-off switch back is held at its first queue lock");
   check(!retro_atomic_load_acquire_int(&order_overtook),
         "a thread's next push does not reach the runner before its held one");
   check(order_retired == 2, "both of its pushes run");
}

int main(void)
{
   main_tid = sthread_get_current_thread_id();
   printf("regular queue with tasks queued, then switched to threaded\n");
   task_queue_init(false, NULL);
   push_tasks(TASKS);
   task_queue_set_threaded();
   drain(TASKS);
   check(task_queue_is_threaded(), "the check switched the queue to threaded");
   check(tasks_done == TASKS,
         "every task queued before the switch reached its callback");
   check(realloc_calls > 0, "the gather's realloc() reached the wrapper");
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

   printf("parked tasks woken from other threads while the queue switches\n");
   retro_atomic_store_release_int(&probe_ran_on, -1);
   task_queue_init(true, NULL);
   probe_ident   = push_parked(probe_handler);
   sleeper_ident = push_parked(sleeper_handler);
   check(probe_runs_on(0) && sleeper_runs_past(0),
         "both parked tasks run on the worker");
   lane_held_waker();
   lane_switch_storm();
   lane_park_across_switch();
   lane_first_pass_wake();
   retire_parked();
   task_queue_deinit();

   printf("tasks pushed from other threads, also while the queue switches\n");
   retro_atomic_store_release_int(&parked_stop, 0);
   retro_atomic_store_release_int(&parked_retired, 0);
   retro_atomic_store_release_int(&probe_ran_on, -1);
   task_queue_init(false, NULL);
   probe_ident   = push_parked(probe_handler);
   sleeper_ident = push_parked(sleeper_handler);
   check(probe_runs_on(1), "the probe runs on the unthreaded queue");
   lane_push_blocking();
   lane_push_unthreaded();
   lane_push_storm();
   lane_push_carried();
   lane_push_order();
   retire_parked();
   task_queue_deinit();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("task queue switch keeps its counts\n");
   return 0;
}
