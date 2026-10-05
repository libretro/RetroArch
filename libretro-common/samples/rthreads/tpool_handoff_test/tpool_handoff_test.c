/* tpool_handoff_test.c -- every job posted is run, once, in order,
 * and no worker sleeps on one.
 *
 * The pool hands jobs over without a lock: producers and workers claim
 * queue slots with a compare-and-swap, and a worker with nothing to do
 * parks by itself and is woken by the post that needs it. What can go
 * wrong there is a matter of two threads' steps falling in one
 * particular order - a worker parking just as a job is posted, and the
 * job sitting unrun - so tpool.c is built here with its test yields
 * on, which make those orders common instead of rare.
 *
 *   1  Jobs posted one at a time to a pool that is mostly asleep: each
 *      must be run before the next is posted, within a deadline. A
 *      lost wake-up is a job that never runs.
 *   2  Several producers posting more jobs than the queue's ring
 *      holds: every job runs exactly once.
 *   3  One producer, one worker: the jobs run in the order posted,
 *      through the ring filling up and emptying again.
 *   4  A pool destroyed with jobs waiting frees them.
 *   5  Every worker stalled while a backlog four times the ring builds
 *      up, several rounds: each job runs exactly once, and a round the
 *      size of one before it allocates no overflow node - it reuses
 *      the ones the earlier round left.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <features/features_cpu.h>

#define TPOOL_FUZZ_SCHEDULE
#include "../../../rthreads/tpool.c"

#define WAKE_ROUNDS   5000
#define PRODUCERS     3
#define JOBS_EACH     5000
#define ORDER_JOBS    3000
#define DEADLINE_US   10000000

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-58s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

/* 1 */
static retro_atomic_int_t woke;

static void wake_job(void *arg)
{
   (void)arg;
   retro_atomic_store_release_int(&woke, 1);
}

static void test_no_lost_wakeup(void)
{
   unsigned i, lost = 0;
   tpool_t *tp = tpool_create(3);
   if (!tp)
   {
      check("pool created", 0);
      return;
   }
   for (i = 0; i < WAKE_ROUNDS && !lost; i++)
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      retro_atomic_store_release_int(&woke, 0);
      if (!tpool_add_work(tp, wake_job, NULL))
      {
         lost = 1;
         break;
      }
      while (!retro_atomic_load_acquire_int(&woke))
      {
         if (cpu_features_get_time_usec() - t0 > DEADLINE_US)
         {
            lost = 1;
            break;
         }
         sthread_yield();
      }
   }
   check("a job posted to a sleeping pool is always run", !lost);
   if (!lost)
      tpool_destroy(tp);
}

/* 2 */
static retro_atomic_int_t ran[PRODUCERS * JOBS_EACH];
static retro_atomic_int_t ran_total;
static tpool_t           *once_pool;

static void once_job(void *arg)
{
   retro_atomic_fetch_add_int((retro_atomic_int_t*)arg, 1);
   retro_atomic_fetch_add_int(&ran_total, 1);
}

static void once_producer(void *arg)
{
   unsigned i, base = (unsigned)(size_t)arg * JOBS_EACH;
   for (i = 0; i < JOBS_EACH; i++)
      if (!tpool_add_work(once_pool, once_job, &ran[base + i]))
         once_job(&ran[base + i]);
}

static void test_exactly_once(void)
{
   unsigned   i, wrong = 0;
   sthread_t *threads[PRODUCERS];

   if (!(once_pool = tpool_create(4)))
   {
      check("pool created", 0);
      return;
   }
   for (i = 0; i < PRODUCERS; i++)
      threads[i] = sthread_create(once_producer, (void*)(size_t)i);
   for (i = 0; i < PRODUCERS; i++)
      if (threads[i])
         sthread_join(threads[i]);
      else
         wrong++;
   /* A job nobody woke for would leave tpool_wait() waiting too */
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      while (retro_atomic_load_acquire_int(&ran_total)
            < PRODUCERS * JOBS_EACH)
      {
         if (cpu_features_get_time_usec() - t0 > DEADLINE_US)
         {
            check("every job from several producers was run", 0);
            printf("tpool_handoff_test: FAIL\n");
            exit(1);
         }
         sthread_yield();
      }
   }
   tpool_wait(once_pool);
   for (i = 0; i < PRODUCERS * JOBS_EACH; i++)
      if (retro_atomic_load_acquire_int(&ran[i]) != 1)
         wrong++;
   check("every job from several producers ran exactly once", !wrong);
   tpool_destroy(once_pool);
}

/* 3 */
static int order_next;
static int order_wrong;

static void order_job(void *arg)
{
   /* one worker: nothing else touches these */
   if ((int)(size_t)arg != order_next)
      order_wrong++;
   order_next++;
}

static void test_in_order(void)
{
   unsigned i;
   tpool_t *tp = tpool_create(1);
   if (!tp)
   {
      check("pool created", 0);
      return;
   }
   for (i = 0; i < ORDER_JOBS; i++)
      if (!tpool_add_work(tp, order_job, (void*)(size_t)i))
         order_wrong++;
   tpool_wait(tp);
   check("one producer's jobs run in the order posted",
         !order_wrong && order_next == ORDER_JOBS);
   tpool_destroy(tp);
}

/* 4 */
static void idle_job(void *arg)
{
   (void)arg;
   sthread_yield();
}

static void test_destroy_pending(void)
{
   unsigned i, c;
   for (c = 0; c < 50; c++)
   {
      tpool_t *tp = tpool_create(2);
      if (!tp)
      {
         check("pool created", 0);
         return;
      }
      for (i = 0; i < 600; i++)
         tpool_add_work(tp, idle_job, NULL);
      tpool_destroy(tp);
   }
   check("a pool destroyed with jobs waiting comes apart", 1);
}

/* 5 */
#define SAT_WORKERS 2
#define SAT_JOBS    (TPOOL_QUEUE_SIZE * 4)
#define SAT_ROUNDS  4

static retro_atomic_int_t sat_gate;
static retro_atomic_int_t sat_parked;
static retro_atomic_int_t sat_ran[SAT_JOBS];

static void sat_stall(void *arg)
{
   (void)arg;
   retro_atomic_fetch_add_int(&sat_parked, 1);
   while (!retro_atomic_load_acquire_int(&sat_gate))
      sthread_yield();
}

static void sat_job(void *arg)
{
   retro_atomic_fetch_add_int((retro_atomic_int_t*)arg, 1);
}

/* The spare nodes, as a set to compare between rounds. */
static size_t sat_spares(tpool_t *tp, tpool_work_t **out, size_t max)
{
   size_t        n = 0;
   tpool_work_t *w;
   slock_lock(tp->work_mutex);
   for (w = tp->over_free; w && n < max; w = w->next)
      out[n++] = w;
   slock_unlock(tp->work_mutex);
   return n;
}

static int sat_cmp(const void *a, const void *b)
{
   uintptr_t x = (uintptr_t)*(tpool_work_t* const*)a;
   uintptr_t y = (uintptr_t)*(tpool_work_t* const*)b;
   return (x > y) - (x < y);
}

static void test_saturation(void)
{
   static tpool_work_t *before[TPOOL_OVER_SPARE];
   static tpool_work_t *after[TPOOL_OVER_SPARE];
   size_t   nb = 0, na, k;
   unsigned r, i, wrong = 0, overflowed = 1, reused = 1;
   tpool_t *tp = tpool_create(SAT_WORKERS);
   if (!tp)
   {
      check("pool created", 0);
      return;
   }
   for (r = 0; r < SAT_ROUNDS; r++)
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      retro_atomic_store_release_int(&sat_gate, 0);
      retro_atomic_store_release_int(&sat_parked, 0);
      for (i = 0; i < SAT_WORKERS; i++)
         tpool_add_work(tp, sat_stall, NULL);
      while (retro_atomic_load_acquire_int(&sat_parked) < SAT_WORKERS
            && cpu_features_get_time_usec() - t0 < DEADLINE_US)
         sthread_yield();
      for (i = 0; i < SAT_JOBS; i++)
      {
         retro_atomic_store_release_int(&sat_ran[i], 0);
         if (!tpool_add_work(tp, sat_job, &sat_ran[i]))
            wrong++;
      }
      if (!retro_atomic_load_acquire_int(&tp->over))
         overflowed = 0;
      retro_atomic_store_release_int(&sat_gate, 1);
      tpool_wait(tp);
      for (i = 0; i < SAT_JOBS; i++)
         if (retro_atomic_load_acquire_int(&sat_ran[i]) != 1)
            wrong++;
      na = sat_spares(tp, after, TPOOL_OVER_SPARE);
      qsort(after, na, sizeof(after[0]), sat_cmp);
      if (r > 0)
      {
         /* nothing new: every spare now was a spare before */
         if (na != nb)
            reused = 0;
         for (k = 0; k < na && reused; k++)
            if (after[k] != before[k])
               reused = 0;
      }
      memcpy(before, after, na * sizeof(after[0]));
      nb = na;
   }
   check("a backlog past the ring builds with every worker stalled",
         overflowed);
   check("every job of a stalled backlog ran exactly once", !wrong);
   check("a repeat backlog reuses the overflow nodes it left", reused && nb);
   tpool_destroy(tp);
}

int main(void)
{
   test_no_lost_wakeup();
   test_exactly_once();
   test_in_order();
   test_destroy_pending();
   test_saturation();
   if (fails)
   {
      printf("tpool_handoff_test: FAIL (%d)\n", fails);
      return 1;
   }
   printf("tpool_handoff_test: PASS\n");
   return 0;
}
