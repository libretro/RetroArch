/* Test for vblank_clock_publish() / vblank_clock_snapshot() in
 * gfx/common/vblank_clock.h: the seqlock Scanline Sync on Windows uses
 * to hand the vblank clock from the kernel-event thread to the frame
 * thread without a lock.
 *
 * One writer publishes as fast as it can, each clock built so its four
 * fields agree with each other (all derived from one counter); readers
 * on other threads snapshot as fast as they can and check that every
 * snapshot they are given is one the writer published whole - never
 * fields from two publishes. Run under ThreadSanitizer in CI, which
 * also checks the pattern is free of formal data races.
 *
 *   every snapshot is internally consistent
 *   snapshots move forward, never back
 *   readers get snapshots (the retry bound does not starve them)
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "gfx/common/vblank_clock.h"

#ifndef VBLANK_CLOCK_HAS_PUB
int main(void) { printf("no lock-free 64-bit atomics here: skipped\n"); return 0; }
#else

#define PUBLISHES 2000000
#define READERS   3

static vblank_clock_pub_t pub;
static retro_atomic_int_t writer_done;

static void make(vblank_clock_t *c, int64_t n)
{
   c->last_us    = 1000000 + n * 16667;
   c->period_us  = 16000.0 + (double)(n % 1000);
   c->nominal_us = c->period_us * 2.0;
   c->good       = (unsigned)(n % 997);
}

static void *writer(void *arg)
{
   int64_t n;
   vblank_clock_t c;
   (void)arg;
   for (n = 1; n <= PUBLISHES; n++)
   {
      make(&c, n);
      vblank_clock_publish(&pub, &c);
   }
   retro_atomic_store_release_int(&writer_done, 1);
   return NULL;
}

typedef struct { long got, torn, backwards, starved; } result_t;

static void *reader(void *arg)
{
   result_t *r = (result_t*)arg;
   int64_t prev = 0;
   while (!retro_atomic_load_acquire_int(&writer_done))
   {
      vblank_clock_t s, want;
      int64_t n;
      if (!vblank_clock_snapshot(&pub, &s))
      {
         r->starved++;
         continue;
      }
      r->got++;
      n = (s.last_us - 1000000) / 16667;
      make(&want, n);
      if (     s.last_us   != want.last_us
            || s.period_us != want.period_us
            || s.nominal_us != want.nominal_us
            || s.good      != want.good)
         r->torn++;
      if (n < prev)
         r->backwards++;
      prev = n;
   }
   return NULL;
}

int main(void)
{
   pthread_t w, rd[READERS];
   result_t res[READERS];
   long got = 0, torn = 0, back = 0, starved = 0;
   int i;
   vblank_clock_t first;

   make(&first, 0);
   vblank_clock_publish(&pub, &first);
   for (i = 0; i < READERS; i++)
   {
      res[i].got = res[i].torn = res[i].backwards = res[i].starved = 0;
      pthread_create(&rd[i], NULL, reader, &res[i]);
   }
   pthread_create(&w, NULL, writer, NULL);
   pthread_join(w, NULL);
   for (i = 0; i < READERS; i++)
   {
      pthread_join(rd[i], NULL);
      got += res[i].got; torn += res[i].torn;
      back += res[i].backwards; starved += res[i].starved;
   }
   printf("%d publishes; readers took %ld snapshots, %ld gave up a try-bound, "
          "%ld torn, %ld went backwards\n", PUBLISHES, got, starved, torn, back);
   if (torn || back || !got)
   {
      printf("FAIL\n");
      return 1;
   }
   printf("all passed\n");
   return 0;
}
#endif
