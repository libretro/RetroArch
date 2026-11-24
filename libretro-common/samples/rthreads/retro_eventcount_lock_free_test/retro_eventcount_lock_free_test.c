/* retro_eventcount_init_lock_free, and the builds with no host clock.
 *
 * init_lock_free sets an eventcount up only where it parks without a
 * lock; where it would need the mutex/condition-variable fallback it
 * fails instead (EXPECT_LOCK_FREE=0, the forced condition-variable
 * build). An eventcount it sets up then carries a two-thread ping-pong
 * of many rounds with no lost wake.
 *
 * Built again with RETRO_DISABLE_HOST_CLOCK and the fixed spin counts
 * (RETRO_EVENTCOUNT_FIXED_SPIN, RTHREADS_SCOND_FIXED_SPIN), it has to
 * build without the timed waits and spin exactly the count it was given.
 */
#include <stdio.h>
#include <stdlib.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>

#ifndef EXPECT_LOCK_FREE
#define EXPECT_LOCK_FREE 1
#endif
#define ROUNDS 100000

static retro_eventcount_t ec;
static retro_atomic_int_t turn;   /* 0: the main thread's, 1: the other's */

static void wait_for(int want)
{
   for (;;)
   {
      int key;
      if (retro_atomic_load_acquire_int(&turn) == want)
         return;
      key = retro_eventcount_prepare_wait(&ec);
      if (retro_atomic_load_acquire_int(&turn) == want)
      {
         retro_eventcount_cancel_wait(&ec);
         return;
      }
      retro_eventcount_commit_wait(&ec, key);
   }
}

static void other(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < ROUNDS; i++)
   {
      wait_for(1);
      retro_atomic_store_release_int(&turn, 0);
      retro_eventcount_notify(&ec);
   }
}

int main(void)
{
   sthread_t *t;
   int i;
   bool ok = retro_eventcount_init_lock_free(&ec);

   printf("init_lock_free: %s (expected %s)\n", ok ? "set up" : "refused",
         EXPECT_LOCK_FREE ? "set up" : "refused");
   if (ok != EXPECT_LOCK_FREE)
   {
      printf("retro_eventcount_lock_free_test: FAIL\n");
      return 1;
   }
#if defined(RETRO_EVENTCOUNT_FIXED_SPIN)
   if (retro_eventcount_spin_iters() != 0
         && retro_eventcount_spin_iters() != RETRO_EVENTCOUNT_FIXED_SPIN)
   {
      printf("spin: %u, not the fixed %u\n", retro_eventcount_spin_iters(),
            (unsigned)RETRO_EVENTCOUNT_FIXED_SPIN);
      printf("retro_eventcount_lock_free_test: FAIL\n");
      return 1;
   }
#endif
   if (!ok)
   {
      /* refused: the object is not to be used or freed */
      printf("retro_eventcount_lock_free_test: PASS\n");
      return 0;
   }
   retro_atomic_int_init(&turn, 0);
   t = sthread_create(other, NULL);
   for (i = 0; i < ROUNDS; i++)
   {
      wait_for(0);
      retro_atomic_store_release_int(&turn, 1);
      retro_eventcount_notify(&ec);
   }
   wait_for(0);
   sthread_join(t);
   retro_eventcount_free(&ec);
   printf("%d rounds, spin %u\n", ROUNDS, retro_eventcount_spin_iters());
   printf("retro_eventcount_lock_free_test: PASS\n");
   return 0;
}
