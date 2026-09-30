/* RETRO_TASK_FLG_MAIN_THREAD: the handler runs on the thread that
 * calls task_queue_check(), never on the worker, and the task
 * otherwise lives like any other.
 *
 * Lanes, on the threaded runner:
 *  - a plain task's handler runs on another thread, a main-thread
 *    task's on this one, and both retire through their callbacks;
 *  - a main-thread task that returns unfinished is called once per
 *    check, on this thread each time, and finishes when it says so;
 *  - a main-thread task with a 'when' in the future is not run before
 *    it is due;
 *  - task_queue_wait() from this thread advances main-thread tasks;
 *  - a main-thread task and a plain task ahead of it in the list do
 *    not block each other: the worker skips past the main-thread one;
 *  - the slow-handler watchdog reports a main-thread handler that
 *    overruns its budget, and not a worker handler that does;
 *  - a main-thread handler that waits on the queue does not run the
 *    main-thread tasks a second time, nor retire them twice;
 *  - one that waits for a worker's task sees it retire from inside the
 *    wait, its callback run once, and goes on.
 * On the unthreaded runner the flag changes nothing: the handler runs
 * here as every handler does.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <queues/task_queue.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

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

static uintptr_t main_id;

struct probe
{
   uintptr_t thread;      /* where the handler last ran */
   unsigned  calls;
   unsigned  finish_after;/* calls before it finishes */
   unsigned  retired;
   bool      off_main;    /* any call ran off the main thread */
};

static void probe_handler(retro_task_t *task)
{
   struct probe *p = (struct probe*)task->user_data;
   p->thread = sthread_get_current_thread_id();
   if (p->thread != main_id)
      p->off_main = true;
   p->calls++;
   if (p->calls >= p->finish_after)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   else
      retro_sleep(1);
}

static void probe_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   struct probe *p = (struct probe*)user_data;
   (void)task; (void)task_data; (void)err;
   p->retired++;
}

static void sleepy_handler(retro_task_t *task)
{
   struct probe *p = (struct probe*)task->user_data;
   p->thread = sthread_get_current_thread_id();
   if (p->thread != main_id)
      p->off_main = true;
   p->calls++;
   retro_sleep(20);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static retro_task_t *push_h(struct probe *p, bool main_thread,
      unsigned finish_after, retro_time_t when, retro_task_handler_t h)
{
   retro_task_t *task = task_init();
   if (!task)
      return NULL;
   memset(p, 0, sizeof(*p));
   p->finish_after = finish_after;
   task->handler   = h;
   task->callback  = probe_callback;
   task->user_data = p;
   task->when      = when;
   task->flags    |= RETRO_TASK_FLG_MUTE;
   if (main_thread)
      task->flags |= RETRO_TASK_FLG_MAIN_THREAD;
   task_queue_push(task);
   return task;
}

static retro_task_t *push(struct probe *p, bool main_thread,
      unsigned finish_after, retro_time_t when)
{
   return push_h(p, main_thread, finish_after, when, probe_handler);
}

/* Checks until the probe retires, bounded. */
static unsigned pump_until_retired(struct probe *p, unsigned max_checks)
{
   unsigned n;
   for (n = 0; n < max_checks && !p->retired; n++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   return n;
}

static void lane_where_it_runs(bool threaded)
{
   struct probe plain, mainp;
   unsigned had = failures;

   CHECK(push(&plain, false, 1, 0) != NULL, "push");
   CHECK(push(&mainp, true,  1, 0) != NULL, "push");
   pump_until_retired(&plain, 2000);
   pump_until_retired(&mainp, 2000);
   CHECK(plain.retired == 1 && mainp.retired == 1, "a task did not retire");
   CHECK(!mainp.off_main, "the main-thread task's handler ran off the main thread");
   CHECK(mainp.thread == main_id, "the main-thread task did not run here");
   if (threaded)
      CHECK(plain.thread != main_id, "the plain task ran on the main thread under the threaded runner");
   else
      CHECK(plain.thread == main_id, "the plain task did not run here on the unthreaded runner");
   if (failures == had)
      fprintf(stderr, "[pass] where-it-runs lane (%s)\n", threaded ? "threaded" : "unthreaded");
}

static void lane_once_per_check(void)
{
   struct probe p;
   unsigned had = failures;
   unsigned i;

   CHECK(push(&p, true, 3, 0) != NULL, "push");
   for (i = 1; i <= 3; i++)
   {
      task_queue_check();
      CHECK(p.calls == i, "check %u: handler called %u times", i, p.calls);
   }
   task_queue_check();
   CHECK(p.retired == 1, "the task did not retire once finished");
   CHECK(!p.off_main, "a call ran off the main thread");
   if (failures == had)
      fprintf(stderr, "[pass] once-per-check lane\n");
}

static void lane_when(void)
{
   struct probe p;
   unsigned had = failures;
   retro_time_t due = cpu_features_get_time_usec() + 30000;

   CHECK(push(&p, true, 1, due) != NULL, "push");
   task_queue_check();
   CHECK(p.calls == 0, "ran before it was due");
   while (cpu_features_get_time_usec() < due)
      retro_sleep(1);
   pump_until_retired(&p, 2000);
   CHECK(p.calls == 1 && p.retired == 1 && !p.off_main, "did not run once due, here");
   if (failures == had)
      fprintf(stderr, "[pass] when lane\n");
}

static void lane_wait(void)
{
   struct probe p;
   unsigned had = failures;

   CHECK(push(&p, true, 2, 0) != NULL, "push");
   task_queue_wait(NULL, NULL);
   CHECK(p.retired == 1 && !p.off_main, "wait did not advance the main-thread task here");
   if (failures == had)
      fprintf(stderr, "[pass] wait lane\n");
}

static void lane_interleaved(void)
{
   struct probe a, b, c;
   unsigned had = failures;

   /* main, plain, main: the worker must reach b past a. */
   CHECK(push(&a, true,  2, 0) != NULL, "push");
   CHECK(push(&b, false, 1, 0) != NULL, "push");
   CHECK(push(&c, true,  2, 0) != NULL, "push");
   pump_until_retired(&b, 2000);
   pump_until_retired(&a, 2000);
   pump_until_retired(&c, 2000);
   CHECK(a.retired && b.retired && c.retired, "not all retired (%u %u %u)",
         a.retired, b.retired, c.retired);
   CHECK(!a.off_main && !c.off_main, "a main-thread task ran off the main thread");
   if (failures == had)
      fprintf(stderr, "[pass] interleaved lane\n");
}

static retro_task_t *slow_seen;
static unsigned      slow_reports;

static void slow_report(retro_task_t *task, retro_time_t took)
{
   (void)took;
   slow_seen = task;
   slow_reports++;
}

static void lane_slow_main_handler(void)
{
   struct probe plain, mainp;
   retro_task_t *mt;
   unsigned had = failures;

   slow_seen    = NULL;
   slow_reports = 0;
   task_queue_set_slow_handler_cb(slow_report, 5000);

   CHECK(push_h(&plain, false, 1, 0, sleepy_handler) != NULL, "push");
   pump_until_retired(&plain, 2000);
   CHECK(plain.retired == 1 && plain.off_main, "the slow plain task did not run on the worker");
   CHECK(slow_reports == 0, "a worker handler was reported");

   mt = push_h(&mainp, true, 1, 0, sleepy_handler);
   CHECK(mt != NULL, "push");
   pump_until_retired(&mainp, 2000);
   CHECK(mainp.retired == 1 && !mainp.off_main, "the slow main-thread task did not run here");
   CHECK(slow_reports == 1, "the slow main-thread handler was reported %u times", slow_reports);
   CHECK(slow_seen == mt, "the report named another task");

   task_queue_set_slow_handler_cb(NULL, 0);
   if (failures == had)
      fprintf(stderr, "[pass] slow main-thread handler lane\n");
}

static bool never(void *data)
{
   (void)data;
   return false;
}

static void waiting_handler(retro_task_t *task)
{
   struct probe *p = (struct probe*)task->user_data;
   p->calls++;
   /* As a content load's stage does when it deinitialises a core:
    * a wait from inside the handler, here with nothing to wait for. */
   task_queue_wait(never, NULL);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void lane_wait_inside_main(void)
{
   struct probe waiter, other;
   unsigned had = failures;
   unsigned i;

   CHECK(push_h(&waiter, true, 1, 0, waiting_handler) != NULL, "push");
   CHECK(push_h(&other,  true, 1000, 0, probe_handler) != NULL, "push");
   for (i = 1; i <= 4; i++)
   {
      task_queue_check();
      CHECK(other.calls == i, "check %u: the other main-thread task ran %u times",
            i, other.calls);
   }
   CHECK(waiter.calls == 1, "the waiting handler ran %u times", waiter.calls);
   CHECK(waiter.retired == 1, "the waiting task retired %u times", waiter.retired);
   other.finish_after = 1;
   pump_until_retired(&other, 2000);
   CHECK(other.retired == 1, "the other task retired %u times", other.retired);
   if (failures == had)
      fprintf(stderr, "[pass] wait-inside-main-handler lane\n");
}

static struct probe *awaited;
static bool awaited_pending(void *data)
{
   (void)data;
   return awaited && !awaited->retired;
}

static void sleepy_worker(retro_task_t *task)
{
   struct probe *p = (struct probe*)task->user_data;
   p->thread = sthread_get_current_thread_id();
   if (p->thread != main_id)
      p->off_main = true;
   p->calls++;
   retro_sleep(20);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static unsigned waiter_saw_retired;
static bool     waiter_wait_ended;
static void awaiting_handler(retro_task_t *task)
{
   struct probe *p = (struct probe*)task->user_data;
   p->calls++;
   /* Bounded, so a queue that never retires it fails here, not hangs */
   waiter_wait_ended  = task_queue_wait_timeout(awaited_pending, NULL, 5000000);
   waiter_saw_retired = awaited ? awaited->retired : 0;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void lane_wait_for_worker_inside_main(void)
{
   struct probe worker, waiter;
   unsigned had = failures;
   unsigned i;

   waiter_saw_retired = 0;
   waiter_wait_ended  = false;
   CHECK(push_h(&worker, false, 1, 0, sleepy_worker) != NULL, "push");
   awaited = &worker;
   CHECK(push_h(&waiter, true, 1, 0, awaiting_handler) != NULL, "push");
   pump_until_retired(&waiter, 2000);
   for (i = 0; i < 5; i++)
      task_queue_check();
   CHECK(waiter.calls == 1, "the waiting handler ran %u times", waiter.calls);
   CHECK(waiter_wait_ended, "the wait inside the handler never saw the worker's task retire");
   CHECK(waiter_saw_retired == 1,
         "inside the wait the worker's task had retired %u times", waiter_saw_retired);
   CHECK(worker.retired == 1, "the worker's task retired %u times", worker.retired);
   CHECK(waiter.retired == 1, "the waiting task retired %u times", waiter.retired);
   CHECK(worker.off_main, "the worker's task ran on the main thread");
   awaited = NULL;
   if (failures == had)
      fprintf(stderr, "[pass] wait-for-worker-inside-main-handler lane\n");
}

int main(void)
{
   main_id = sthread_get_current_thread_id();

   task_queue_init(true, NULL);
   if (!task_queue_is_threaded())
   {
      fprintf(stderr, "no threaded queue on this host\n");
      return 1;
   }
   lane_where_it_runs(true);
   lane_once_per_check();
   lane_when();
   lane_wait();
   lane_interleaved();
   lane_slow_main_handler();
   lane_wait_inside_main();
   lane_wait_for_worker_inside_main();
   task_queue_deinit();

   task_queue_init(false, NULL);
   lane_where_it_runs(false);
   lane_once_per_check();
   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}
