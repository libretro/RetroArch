/* task_queue_budget_test: the per-check budgets of task_queue_check().
 *
 * Retirement (threaded queue): a burst of finished tasks used to be
 * retired in one check however long their callbacks took. With a
 * count budget a check retires that many; with a time budget it stops
 * once the time has gone by; with none it retires everything.
 *
 * Handlers (unthreaded queue): every running task's handler used to
 * run once per check. With a time budget a check runs a few, and the
 * ones it did not run go first next time, so the list rotates rather
 * than starving its tail.
 *
 * Against the old queue the budgeted checks retire or run every task
 * at once, and init(false) after a threaded session comes back
 * threaded on the next check. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_timers.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>
#include <queues/task_queue.h>

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } else { printf("ok   " __VA_ARGS__); printf("\n"); } } while (0)

static void busy_usec(retro_time_t usec)
{
   retro_time_t until = cpu_features_get_time_usec() + usec;
   while (cpu_features_get_time_usec() < until) { }
}

/* --- retirement --- */

static retro_atomic_int_t handled;
static unsigned retired;

static void finish_now(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   retro_atomic_fetch_add_int(&handled, 1);
}

static void slow_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   (void)task; (void)task_data; (void)user_data; (void)error;
   busy_usec(500);
   retired++;
}

static void push_finishers(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      retro_task_t *t = task_init();
      t->handler  = finish_now;
      t->callback = slow_callback;
      task_queue_push(t);
   }
}

/* Waits for the worker to finish every pushed task without retiring
 * any: the retirement is what the lanes below measure. */
static void wait_handled(unsigned n)
{
   retro_time_t until = cpu_features_get_time_usec() + 5000000;
   while ((unsigned)retro_atomic_load_acquire_int(&handled) < n
         && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   retro_sleep(5);
}

static void lane_retire(void)
{
   unsigned before;
   retro_time_t started, took;

   task_queue_init(true, NULL);
   retro_atomic_int_init(&handled, 0);
   retired = 0;

   /* Count budget: 8 of 64 per check. */
   push_finishers(64);
   wait_handled(64);
   task_queue_set_budget(0, 8, 0);
   task_queue_check();
   CHECK(retired == 8, "count budget 8: retired %u of 64 in one check", retired);
   task_queue_check();
   CHECK(retired == 16, "count budget 8: next check retires the next 8 (%u)", retired);

   /* Time budget: 2 ms of 500 us callbacks is a handful, not 48. */
   before = retired;
   task_queue_set_budget(2000, 0, 0);
   started = cpu_features_get_time_usec();
   task_queue_check();
   took = cpu_features_get_time_usec() - started;
   CHECK(retired - before >= 1 && retired - before <= 8 && took < 10000,
         "time budget 2 ms: retired %u in %lld us", retired - before, (long long)took);

   /* No budget: everything left goes in one check. */
   task_queue_set_budget(0, 0, 0);
   task_queue_check();
   CHECK(retired == 64, "no budget: all 64 retired (%u)", retired);

   task_queue_deinit();
}

/* --- handlers --- */

#define N_SPIN 20
static unsigned runs[N_SPIN];
static bool     release;

static void spin_handler(retro_task_t *task)
{
   unsigned idx = (unsigned)(size_t)task->user_data;
   busy_usec(1000);
   runs[idx]++;
   if (release)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static unsigned runs_total(void)
{
   unsigned i, n = 0;
   for (i = 0; i < N_SPIN; i++)
      n += runs[i];
   return n;
}

static void lane_handlers(void)
{
   unsigned i, lo, hi, total;

   /* init(false) after the threaded session above: the queue must
    * stay unthreaded across a check. The flag used to stick at true,
    * and the first check re-initialised the threaded queue. */
   task_queue_init(false, NULL);
   task_queue_check();
   CHECK(!task_queue_is_threaded(), "init(false) after a threaded session stays unthreaded");
   memset(runs, 0, sizeof(runs));
   release = false;

   for (i = 0; i < N_SPIN; i++)
   {
      retro_task_t *t = task_init();
      t->handler   = spin_handler;
      t->user_data = (void*)(size_t)i;
      task_queue_push(t);
   }

   /* 3 ms budget over 1 ms handlers: three or four per check. */
   task_queue_set_budget(0, 0, 3000);
   task_queue_check();
   total = runs_total();
   CHECK(total >= 1 && total <= 5, "handler budget 3 ms: %u of %u handlers ran in one check", total, N_SPIN);

   /* Five more checks: the list rotates, so no task gets far ahead. */
   for (i = 0; i < 5; i++)
      task_queue_check();
   lo = hi = runs[0];
   for (i = 1; i < N_SPIN; i++)
   {
      if (runs[i] < lo) lo = runs[i];
      if (runs[i] > hi) hi = runs[i];
   }
   CHECK(hi - lo <= 1, "rotation: after six checks runs range %u..%u", lo, hi);

   /* No budget: one check runs every handler. */
   task_queue_set_budget(0, 0, 0);
   total = runs_total();
   task_queue_check();
   CHECK(runs_total() - total == N_SPIN, "no budget: %u handlers in one check", runs_total() - total);

   release = true;
   task_queue_wait(NULL, NULL);
   task_queue_deinit();
}

int main(void)
{
   lane_retire();
   lane_handlers();
   printf("%s\n", fails ? "FAILED" : "PASSED");
   return fails ? 1 : 0;
}
