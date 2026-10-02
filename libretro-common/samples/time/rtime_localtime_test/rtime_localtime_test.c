/* rtime_localtime() from four threads at once, with and without
 * rtime_init() having run: each thread converts its own timestamp and
 * must get back exactly what a lone caller gets for it. localtime()
 * fills one static struct, so two callers let in together hand each
 * other's fields back. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <time/rtime.h>

#define THREADS 4
#define ROUNDS  50000

static time_t when[THREADS];
static struct tm expect[THREADS];
static retro_atomic_int_t go;
static retro_atomic_int_t wrong;

static int same(const struct tm *a, const struct tm *b)
{
   return a->tm_year == b->tm_year && a->tm_mon  == b->tm_mon
       && a->tm_mday == b->tm_mday && a->tm_hour == b->tm_hour
       && a->tm_min  == b->tm_min  && a->tm_sec  == b->tm_sec;
}

static void worker(void *data)
{
   int t = (int)(size_t)data;
   int i;
   while (!retro_atomic_load_acquire_int(&go)) { }
   for (i = 0; i < ROUNDS; i++)
   {
      struct tm got;
      memset(&got, 0, sizeof(got));
      rtime_localtime(&when[t], &got);
      if (!same(&got, &expect[t]))
         retro_atomic_inc_int(&wrong);
   }
}

static int run(const char *label)
{
   sthread_t *th[THREADS];
   int t, n;
   retro_atomic_store_release_int(&go, 0);
   retro_atomic_store_release_int(&wrong, 0);
   for (t = 0; t < THREADS; t++)
      th[t] = sthread_create(worker, (void*)(size_t)t);
   retro_atomic_store_release_int(&go, 1);
   for (t = 0; t < THREADS; t++)
      sthread_join(th[t]);
   n = retro_atomic_load_acquire_int(&wrong);
   if (n)
   {
      printf("FAIL  %s: %d conversions came back with another thread's fields\n",
            label, n);
      return 1;
   }
   printf("ok    %s: %d threads x %d conversions\n", label, THREADS, ROUNDS);
   return 0;
}

int main(void)
{
   int t, fails = 0;
   for (t = 0; t < THREADS; t++)
   {
      /* far apart: every field differs from the others' */
      when[t] = (time_t)(86400L * 397L * (t + 1) + 3661L * (t + 1) * 7L);
      rtime_localtime(&when[t], &expect[t]);
   }
   fails += run("before rtime_init");
   rtime_init();
   fails += run("after rtime_init");
   rtime_deinit();
   fails += run("after rtime_deinit");
   return fails ? 1 : 0;
}
