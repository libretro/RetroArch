/* RETRO_TASK_FLG_PARALLEL: the threaded runner's extra workers take
 * the tasks marked so, beside the one worker every other task runs on.
 *
 * Lanes, on the threaded runner, with the number of extra workers set
 * by task_queue_set_parallel_workers():
 *  - four parallel tasks are in their handlers at the same time with
 *    three extra workers beside the one;
 *  - plain tasks never overlap one another while parallel tasks run
 *    beside them, and never run off the one worker;
 *  - a parallel task returning unfinished is never in two handlers at
 *    once;
 *  - a parallel task that is also detachable runs on the one worker;
 *  - with none asked for there are no extra workers and every parallel
 *    task still runs, on the one worker;
 *  - a deinit with parallel tasks queued stops every worker, and the
 *    next init starts the extra workers again.
 * On the unthreaded runner the flag changes nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

/* compiled in, so the lanes can see how many extra workers started */
#include "../../../queues/task_queue.c"

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

/* ---- concurrency ---- */
static retro_atomic_int_t inside;      /* parallel handlers running now */
static retro_atomic_int_t most;        /* the most seen at once */
static retro_atomic_int_t serial_in;   /* plain handlers running now */
static retro_atomic_int_t serial_most;
static retro_atomic_int_t retired;
static uintptr_t          worker0;     /* where the plain tasks run */
static retro_atomic_int_t off_worker0; /* plain handlers seen elsewhere */
static unsigned           gather_goal; /* how many must meet at once */

struct job
{
   retro_atomic_int_t in;      /* this task's handlers running now */
   retro_atomic_int_t twice;   /* seen in two at once */
   unsigned           calls;
   unsigned           finish_after;
   uintptr_t          thread;
};

static void note_most(retro_atomic_int_t *now, retro_atomic_int_t *max)
{
   int n = retro_atomic_fetch_add_int(now, 1) + 1;
   for (;;)
   {
      int m = retro_atomic_load_acquire_int(max);
      if (n <= m || retro_atomic_cas_int(max, m, n))
         break;
   }
}

/* Waits, with a bound, for gather_goal parallel handlers to be inside
 * at once, then leaves: four of them meeting proves four workers. */
static void parallel_handler(retro_task_t *task)
{
   struct job *j = (struct job*)task->user_data;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;

   if (retro_atomic_fetch_add_int(&j->in, 1))
      retro_atomic_store_release_int(&j->twice, 1);
   j->thread = sthread_get_current_thread_id();
   note_most(&inside, &most);
   while (   (unsigned)retro_atomic_load_acquire_int(&most) < gather_goal
          && !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
          && cpu_features_get_time_usec() < deadline)
      retro_sleep(1);
   retro_atomic_fetch_add_int(&inside, -1);
   retro_atomic_fetch_add_int(&j->in, -1);

   if (     ++j->calls >= j->finish_after
         || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void serial_handler(retro_task_t *task)
{
   struct job *j = (struct job*)task->user_data;
   note_most(&serial_in, &serial_most);
   j->thread = sthread_get_current_thread_id();
   if (worker0 && j->thread != worker0)
      retro_atomic_fetch_add_int(&off_worker0, 1);
   retro_sleep(2);
   retro_atomic_fetch_add_int(&serial_in, -1);
   if (++j->calls >= j->finish_after)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void count_retired(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retro_atomic_fetch_add_int(&retired, 1);
}

static void push(retro_task_handler_t handler, struct job *j, int flags)
{
   retro_task_t *task = task_init();
   task->handler   = handler;
   task->callback  = count_retired;
   task->user_data = j;
   task->flags    |= flags | RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
}

static bool not_all_retired(void *data)
{
   return retro_atomic_load_acquire_int(&retired) < *(int*)data;
}

static void reset_counts(void)
{
   retro_atomic_store_release_int(&inside, 0);
   retro_atomic_store_release_int(&most, 0);
   retro_atomic_store_release_int(&serial_in, 0);
   retro_atomic_store_release_int(&serial_most, 0);
   retro_atomic_store_release_int(&retired, 0);
   retro_atomic_store_release_int(&off_worker0, 0);
}

static void wait_retired(int n)
{
   task_queue_wait(not_all_retired, &n);
   task_queue_check();
}

static void job_init(struct job *j, unsigned finish_after)
{
   memset(j, 0, sizeof(*j));
   retro_atomic_int_init(&j->in, 0);
   retro_atomic_int_init(&j->twice, 0);
   j->finish_after = finish_after;
}

int main(void)
{
   struct job par[4], ser[5], multi, det, probe;
   unsigned i;

   retro_atomic_int_init(&inside, 0);
   retro_atomic_int_init(&most, 0);
   retro_atomic_int_init(&serial_in, 0);
   retro_atomic_int_init(&serial_most, 0);
   retro_atomic_int_init(&retired, 0);
   retro_atomic_int_init(&off_worker0, 0);

   task_queue_set_parallel_workers(3);
   task_queue_init(true, NULL);

   /* where the one worker is: a plain task's handler */
   job_init(&probe, 1);
   push(serial_handler, &probe, 0);
   wait_retired(1);
   worker0 = probe.thread;
   CHECK(worker0 && worker0 != sthread_get_current_thread_id(),
         "a plain task runs on the worker");

   /* four parallel tasks meet; five plain ones run beside, one at a time */
   reset_counts();
   gather_goal = 4;
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 1);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   for (i = 0; i < 5; i++)
   {
      job_init(&ser[i], 3);
      push(serial_handler, &ser[i], 0);
   }
   wait_retired(9);
   CHECK(retro_atomic_load_acquire_int(&most) == 4,
         "three extra workers: four parallel handlers at once (saw %d)",
         retro_atomic_load_acquire_int(&most));
   CHECK(parallel_count == 3, "three extra workers started (%u)",
         parallel_count);
   CHECK(retro_atomic_load_acquire_int(&serial_most) == 1,
         "plain tasks never overlap (saw %d at once)",
         retro_atomic_load_acquire_int(&serial_most));
   CHECK(retro_atomic_load_acquire_int(&off_worker0) == 0,
         "plain tasks never leave the one worker (%d did)",
         retro_atomic_load_acquire_int(&off_worker0));

   /* one parallel task called eight times, with others to tempt the
    * idle workers: never in two handlers at once */
   reset_counts();
   gather_goal = 1;
   job_init(&multi, 8);
   push(parallel_handler, &multi, RETRO_TASK_FLG_PARALLEL);
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 2);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   wait_retired(5);
   CHECK(multi.calls == 8, "the task was called eight times (%u)", multi.calls);
   CHECK(!retro_atomic_load_acquire_int(&multi.twice),
         "a parallel task is never in two handlers at once");

   /* detachable wins: the one worker */
   reset_counts();
   job_init(&det, 1);
   push(serial_handler, &det,
         RETRO_TASK_FLG_PARALLEL | RETRO_TASK_FLG_DETACHABLE);
   wait_retired(1);
   CHECK(det.thread == worker0, "a detachable parallel task runs on the one worker");

   /* a deinit with parallel tasks queued: every worker stops; then with
    * none asked for: no extra workers, and parallel tasks still run */
   reset_counts();
   gather_goal = 99; /* they wait until cancelled: inside at the reset */
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 1000);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   retro_sleep(20);
   task_queue_reset();
   wait_retired(4);
   task_queue_deinit();
   CHECK(parallel_count == 0 && !parallel_started, "deinit stopped the extra workers");

   task_queue_set_parallel_workers(0);
   task_queue_init(true, NULL);
   reset_counts();
   gather_goal = 1;
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 2);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   wait_retired(4);
   CHECK(parallel_count == 0, "none asked for: no extra workers (%u)", parallel_count);
   CHECK(retro_atomic_load_acquire_int(&most) == 1,
         "none asked for: one handler at a time (saw %d)",
         retro_atomic_load_acquire_int(&most));
   task_queue_deinit();

   /* the next init starts them again */
   task_queue_set_parallel_workers(2);
   task_queue_init(true, NULL);
   reset_counts();
   gather_goal = 3;
   for (i = 0; i < 3; i++)
   {
      job_init(&par[i], 1);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   wait_retired(3);
   CHECK(parallel_count == 2, "two asked for after a re-init: two extra workers (%u)",
         parallel_count);
   CHECK(retro_atomic_load_acquire_int(&most) == 3,
         "two extra workers: three handlers at once (saw %d)",
         retro_atomic_load_acquire_int(&most));
   task_queue_deinit();

   /* unthreaded: the flag changes nothing */
   task_queue_init(false, NULL);
   reset_counts();
   gather_goal = 1;
   job_init(&par[0], 2);
   push(parallel_handler, &par[0], RETRO_TASK_FLG_PARALLEL);
   wait_retired(1);
   CHECK(par[0].thread == sthread_get_current_thread_id(),
         "unthreaded: the handler runs here");
   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "task_queue_parallel_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("task_queue_parallel_test: OK\n");
   return 0;
}
