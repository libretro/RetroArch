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

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("task queue switch keeps its counts\n");
   return 0;
}
