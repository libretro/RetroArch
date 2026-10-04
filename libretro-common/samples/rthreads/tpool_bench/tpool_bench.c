/* tpool_bench.c -- what a short job costs to hand to the pool.
 *
 * P producer threads each post JOBS jobs that do next to nothing, to a
 * pool of W workers, and the run is timed from the first post to the
 * last job done. With jobs this short the time is the pool's own: the
 * queue, the wake-ups, and whatever the producers and workers contend
 * on. Not a test - it prints, it does not judge.
 *
 * Usage: tpool_bench [workers [producers [jobs-per-producer]]]
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <features/features_cpu.h>
#include <rthreads/tpool.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

static tpool_t           *pool;
static unsigned           jobs_each = 200000;
static retro_atomic_int_t done;
static retro_atomic_int_t refused;
static retro_atomic_int_t go;

static void job(void *arg)
{
   (void)arg;
   retro_atomic_fetch_add_int(&done, 1);
}

static void producer(void *arg)
{
   unsigned i;
   (void)arg;
   while (!retro_atomic_load_acquire_int(&go))
      sthread_yield();
   for (i = 0; i < jobs_each; i++)
      if (!tpool_add_work(pool, job, NULL))
      {
         /* what a caller does with a refusal: the work itself */
         retro_atomic_fetch_add_int(&refused, 1);
         job(NULL);
      }
}

static double now_sec(void)
{
   return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(int argc, char **argv)
{
   unsigned   workers   = (argc > 1) ? (unsigned)atoi(argv[1]) : 4;
   unsigned   producers = (argc > 2) ? (unsigned)atoi(argv[2]) : 1;
   sthread_t *threads[64];
   retro_time_t t0, t1;
   double     wall, cpu0, cpu1;
   unsigned   i;

   if (argc > 3)
      jobs_each = (unsigned)atoi(argv[3]);
   if (!workers || !producers || producers > 64
         || !(pool = tpool_create(workers)))
   {
      printf("usage: tpool_bench [workers [producers [jobs]]]\n");
      return 1;
   }

   for (i = 0; i < producers; i++)
      if (!(threads[i] = sthread_create(producer, NULL)))
         return 1;

   cpu0 = now_sec();
   t0 = cpu_features_get_time_usec();
   retro_atomic_store_release_int(&go, 1);
   for (i = 0; i < producers; i++)
      sthread_join(threads[i]);
   tpool_wait(pool);
   t1 = cpu_features_get_time_usec();
   cpu1 = now_sec();

   wall = (double)(t1 - t0) / 1e6;
   printf("%u workers, %u producers, %u jobs: %.3f s wall, %.3f s cpu, "
         "%.0f ns/job, %d run by their producer\n",
         workers, producers, producers * jobs_each, wall, cpu1 - cpu0,
         wall * 1e9 / (double)(producers * jobs_each),
         (int)retro_atomic_load_acquire_int(&refused));
   if ((unsigned)retro_atomic_load_acquire_int(&done) != producers * jobs_each)
   {
      printf("FAIL: %d jobs ran\n", (int)retro_atomic_load_acquire_int(&done));
      return 1;
   }
   tpool_destroy(pool);
   return 0;
}
