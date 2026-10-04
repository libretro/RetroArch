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

int main(void)
{
   test_no_lost_wakeup();
   test_exactly_once();
   test_in_order();
   test_destroy_pending();
   if (fails)
   {
      printf("tpool_handoff_test: FAIL (%d)\n", fails);
      return 1;
   }
   printf("tpool_handoff_test: PASS\n");
   return 0;
}
