/* tpool_parallel_test.c -- work posted to a pool of N runs on N
 * threads at once, not one after another.
 *
 * Four jobs, each spinning for a fixed spell and reporting the moment
 * it started and stopped, posted to a pool of four. If the workers
 * wake and take the work, the four run together and the whole lot
 * lasts about one spell; if only one worker ever wakes - a lost
 * wake-up in the pool or the condition variable beneath it - they run
 * back to back and last four. The counts of jobs running at once say
 * which, and the test fails on fewer than two: a decoder that posts
 * its pictures here would then decode them one at a time and gain
 * nothing from the cores.
 */

#include <stdio.h>
#include <stdlib.h>

#include <rthreads/tpool.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>

#define JOBS 4
#define SPELL_US 300000

static retro_atomic_int_t running;
static retro_atomic_int_t running_max;
static retro_atomic_int_t done;

/* A spell of work that costs CPU time rather than waiting for the
 * clock: a job that waited on the clock would finish on time however
 * many cores it shared, and say nothing. */
static volatile unsigned sink;
static unsigned spins_per_spell;

static void spell(void)
{
   unsigned i, x = 1;
   for (i = 0; i < spins_per_spell; i++)
      x = x * 1664525u + 1013904223u;
   sink = x;
}

static void job(void *arg)
{
   int now_running = retro_atomic_fetch_add_int(&running, 1) + 1;
   int m;
   (void)arg;
   /* record the most seen at once */
   do
   {
      m = retro_atomic_load_acquire_int(&running_max);
      if (now_running <= m)
         break;
   } while (!retro_atomic_cas_int(&running_max, m, now_running));
   spell();
   retro_atomic_fetch_sub_int(&running, 1);
   retro_atomic_fetch_add_int(&done, 1);
}

int main(void)
{
   tpool_t *tp = tpool_create_with_stack_size(JOBS, 512 * 1024);
   int64_t t0, ms;
   int i, most, rc = 0;
   if (!tp)
   {
      printf("tpool_parallel_test: no pool\n");
      return 2;
   }
   /* size the spell to about SPELL_US on this machine, alone */
   spins_per_spell = 1u << 24;
   t0 = cpu_features_get_time_usec();
   spell();
   {
      int64_t one = cpu_features_get_time_usec() - t0;
      if (one < 1) one = 1;
      spins_per_spell = (unsigned)((double)spins_per_spell * SPELL_US / (double)one);
   }
   t0 = cpu_features_get_time_usec();
   for (i = 0; i < JOBS; i++)
      if (!tpool_add_work(tp, job, NULL))
      {
         printf("tpool_parallel_test: add_work refused\n");
         return 2;
      }
   tpool_wait(tp);
   ms = (cpu_features_get_time_usec() - t0) / 1000;
   most = retro_atomic_load_acquire_int(&running_max);
   printf("%d jobs of %d ms on a pool of %d: %d ran at once at most, "
         "all done in %lld ms (one core would take about %d)\n", JOBS,
         SPELL_US / 1000, JOBS, most, (long long)ms, JOBS * SPELL_US / 1000);
   if (retro_atomic_load_acquire_int(&done) != JOBS)
   {
      printf("FAIL: %d of %d jobs ran\n", retro_atomic_load_acquire_int(&done), JOBS);
      rc = 1;
   }
   else if (most < 2)
   {
      printf("FAIL: the jobs ran one after another - the pool's workers "
            "are not being woken\n");
      rc = 1;
   }
   else if (ms > JOBS * SPELL_US / 1000 * 3 / 4)
      printf("NOTE: they overlapped but took about a core's time: this "
            "machine gave the process one core\n");
   else
      printf("PASS\n");
   tpool_destroy(tp);
   return rc;
}
