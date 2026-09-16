/* retro_asym_eventcount_test.c - the asymmetric eventcount, in both of
 * its modes.
 *
 * The protocol under test is the one every eventcount user writes:
 *
 *   producer:                          consumer:
 *      publish work                       for (;;) {
 *      notify(ec)                            if (work) { take; continue; }
 *                                            key = prepare_wait(ec)
 *                                            if (work) { cancel_wait(ec); continue; }
 *                                            commit_wait(ec, key)
 *                                         }
 *
 * The bug it exists to catch is the lost wakeup: the producer publishes
 * and notifies while the consumer is between its last check and its
 * commit, and the consumer sleeps through it. The symmetric eventcount
 * prevents that with a sequentially-consistent pair on each side; this
 * one prevents it with retro_procbarrier() in prepare_wait, and the
 * test is what shows that the substitution holds.
 *
 * It runs the protocol hard -- a producer publishing as fast as it can,
 * a consumer that really parks on the condvar each round -- and reports
 * the first lost wake, with a timeout so a broken primitive fails rather
 * than hangs.
 *
 * Both modes are run: the natural resolution, then the symmetric
 * fallback forced on the same machine, so the path the multi-core
 * consoles take is exercised here too and not only there.
 *
 * As with the procbarrier test, ONE CORE IS NOT ENOUGH for the race to
 * exist: on a uniprocessor the scheduler serialises the two sides and
 * neither ordering mechanism is ever load-bearing. The CPU count is
 * printed so a green run on one core is not read as evidence.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_procbarrier.h>
#include <rthreads/retro_asym_eventcount.h>

#if defined(_WIN32)
#include <windows.h>
static long cpu_count(void) { SYSTEM_INFO si; GetSystemInfo(&si); return (long)si.dwNumberOfProcessors; }
#else
#include <unistd.h>
static long cpu_count(void) { return sysconf(_SC_NPROCESSORS_ONLN); }
#endif

static retro_asym_eventcount_t g_ec;
static retro_atomic_int_t      g_work;     /* items published, not yet taken */
static retro_atomic_int_t      g_stop;
static retro_atomic_int_t      g_published;

static void producer(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_relaxed_int(&g_stop))
   {
      /* Publish one item, then notify. The item is the payload the
       * epoch's release store is meant to make visible. */
      retro_atomic_fetch_add_int(&g_work, 1);
      retro_atomic_fetch_add_int(&g_published, 1);
      retro_asym_eventcount_notify(&g_ec);
      /* Pace it: a producer that never yields on one core starves the
       * consumer of the chance to park at all, and the park is the
       * path under test. */
      {
         volatile int spin = 200;
         while (spin--) ;
      }
   }
}

/* Take one item, parking if none is there. Returns 1 if an item was
 * taken, 0 if the wait timed out with work published but unseen -- a
 * lost wake. */
static int consume_one(void)
{
   int key;

   if (retro_atomic_load_acquire_int(&g_work) > 0)
   {
      retro_atomic_fetch_sub_int(&g_work, 1);
      return 1;
   }
   key = retro_asym_eventcount_prepare_wait(&g_ec);
   if (retro_atomic_load_acquire_int(&g_work) > 0)
   {
      retro_asym_eventcount_cancel_wait(&g_ec);
      retro_atomic_fetch_sub_int(&g_work, 1);
      return 1;
   }
   /* Nothing there: park. Two seconds is forever for a producer that
    * publishes every few hundred cycles; a timeout is a lost wake. */
   if (!retro_asym_eventcount_commit_wait_timeout(&g_ec, key, 2000000))
   {
      if (retro_atomic_load_acquire_int(&g_work) > 0)
         return 0;   /* work was there and we slept through the notify */
      return 1;      /* a genuine idle timeout; the producer is paused */
   }
   if (retro_atomic_load_acquire_int(&g_work) > 0)
   {
      retro_atomic_fetch_sub_int(&g_work, 1);
      return 1;
   }
   return 1;   /* spurious wake; harmless */
}

static int run_mode(const char *label, int rounds, int force_symmetric)
{
   sthread_t *p;
   int r, lost = 0;

   if (!retro_asym_eventcount_init(&g_ec))
   {
      printf("  %s: init failed\n", label);
      return 0;
   }
   /* The fallback is forced by the test, not by a hook in the primitive:
    * the object is simply told after init that it has no barrier, which
    * is what a multi-core console's init does for itself. */
   if (force_symmetric)
      g_ec.asymmetric = 0;
   printf("  %s: object is %s\n", label,
          retro_asym_eventcount_is_asymmetric(&g_ec) ? "asymmetric" : "symmetric");

   retro_atomic_store_relaxed_int(&g_work, 0);
   retro_atomic_store_relaxed_int(&g_stop, 0);
   retro_atomic_store_relaxed_int(&g_published, 0);
   p = sthread_create(producer, NULL);

   for (r = 0; r < rounds; r++)
   {
      if (!consume_one())
      {
         lost++;
         if (lost <= 3)
            printf("  %s: lost wake at round %d\n", label, r);
      }
   }
   retro_atomic_store_relaxed_int(&g_stop, 1);
   sthread_join(p);
   retro_asym_eventcount_free(&g_ec);

   printf("  %s: %d rounds, %d published, %d lost wakes\n", label, rounds,
          retro_atomic_load_relaxed_int(&g_published), lost);
   return lost == 0;
}

int main(void)
{
   int ok = 1;
   long n = cpu_count();

   printf("retro_asym_eventcount\n");
   printf("  cpus: %ld%s\n", n,
          n > 1 ? "" : "  (the lost-wake check is not load-bearing on one core)");

   /* Natural resolution first, then the symmetric fallback on the same
    * machine, so both protocols are exercised wherever this runs. */
   ok &= run_mode("natural",          20000, 0);
   ok &= run_mode("forced-symmetric", 20000, 1);

   printf(ok ? "asym_eventcount: ok\n" : "asym_eventcount: FAILED\n");
   return ok ? 0 : 1;
}
