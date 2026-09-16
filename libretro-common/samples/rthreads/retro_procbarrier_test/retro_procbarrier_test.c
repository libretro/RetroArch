/* retro_procbarrier_test.c - does the process-wide barrier fence?
 *
 * Three things are checked.
 *
 * 1. Resolution. init() picks a tier, and it is a tier this platform
 *    can have: no membarrier on Windows, no FlushProcessWriteBuffers on
 *    Linux, no page flip on ARM.
 *
 * 2. It returns. The chosen tier issues a barrier and comes back, with
 *    other threads busy, sleeping and parked. A signal-tier bug that
 *    waited for a sleeping thread would hang here.
 *
 * 3. It fences. This is the one that matters and the one most tests of
 *    this kind skip. The asymmetric protocol the barrier enables is:
 *
 *      producer:  seq = seq + 1  (release)        consumer:  parked = 1  (relaxed)
 *                 if (parked) wake()                         procbarrier()
 *                                                            if (seq unchanged) sleep()
 *
 *    Without the barrier the consumer's store to parked can sit in its
 *    store buffer while it loads seq, and the producer's store to seq
 *    can sit in the producer's while it loads parked: each sees the
 *    other's old value, the producer does not wake, the consumer sleeps
 *    forever. The barrier drains the producer's buffer before the
 *    consumer decides, so the consumer either sees the new seq or the
 *    producer sees parked.
 *
 *    The test runs that protocol hard: a producer publishing as fast as
 *    it can and a consumer parking on a real semaphore each round, with
 *    the barrier in place. If the barrier is a no-op on this platform
 *    the consumer eventually sleeps through a publish and the round times
 *    out. On a uniprocessor the race cannot happen and the test passes
 *    trivially, which is correct: no barrier is needed there.
 *
 *    A timeout, not a hang: a lost wake is reported as a failure with
 *    the round number, so a broken tier fails fast.
 *
 *    ONE CORE IS NOT ENOUGH. The race needs two threads genuinely on two
 *    CPUs; on a single core the scheduler serialises them and the
 *    barrier is never load-bearing. Checked by replacing the barrier
 *    with a no-op on a one-core box: all 5000 rounds still pass. So a
 *    green fence check on a uniprocessor says nothing, and the test
 *    prints the CPU count so nobody reads it as more than it is. Run it
 *    on real SMP to mean anything.
 *
 * RETRO_PROCBARRIER=membarrier|fpwb|pageflip|signal forces a tier, so
 * every tier a platform has can be exercised on one machine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_procbarrier.h>
#include <retro_timers.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_ms(unsigned ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleep_ms(unsigned ms) { usleep(ms * 1000u); }
#endif

/* ---- the protocol under test --------------------------------------- */

static retro_atomic_int_t g_seq;
static retro_atomic_int_t g_parked;
static retro_atomic_int_t g_stop;
static retro_atomic_int_t g_wakes;
static slock_t           *g_lock;
static scond_t           *g_cond;
static int                g_posted;   /* under g_lock */

static void producer(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_relaxed_int(&g_stop))
   {
      int s;
      /* Paced. An unpaced producer on its own core moves seq every few
       * nanoseconds, so the consumer never finds it unchanged and never
       * parks -- the withdraw path gets 5000 rounds and the park path
       * none. A short spin between publishes is what makes the park
       * happen on real SMP, where it is the path under test. */
      {
         volatile int spin = 400;
         while (spin--) ;
      }
      s = retro_atomic_load_relaxed_int(&g_seq);
      /* Plain release store, plain relaxed load: no locked instruction.
       * This is the whole point of the asymmetric design. The sequence
       * wraps through the unsigned domain: it publishes many millions of
       * times a round, and only "changed" matters, not the value. */
      retro_atomic_store_release_int(&g_seq, (int)((unsigned)s + 1u));
      if (retro_atomic_load_relaxed_int(&g_parked))
      {
         /* Claim the wake so there is one post per park. */
         if (retro_atomic_fetch_add_int(&g_parked, -1) == 1)
         {
            retro_atomic_fetch_add_int(&g_wakes, 1);
            slock_lock(g_lock);
            g_posted = 1;
            scond_signal(g_cond);
            slock_unlock(g_lock);
         }
         else
            retro_atomic_fetch_add_int(&g_parked, 1);
      }
   }
}

/* One consumer round: announce parking, barrier, re-check, sleep if
 * nothing arrived. Returns 1 if woken, 0 if the wake was lost. */
static int consumer_round(int last_seen, int *seen_out)
{
   int s;
   /* Arithmetic, never a blind store: the producer's stale-claim
    * un-claim is a -1/+1 transient on this counter, and a store of 1
    * landing between those two halves left g_parked at 2 - a value
    * from which fetch_add(-1) never returns 1, so no claim could
    * succeed and no post could come while the producer demonstrably
    * published millions of sequence steps. That is the 'lost wake at
    * round N' CI failure, three in five thousand rounds, self-healing
    * on the next round's store: a wake the harness lost itself, not
    * the barrier. The increment composes with every transient - the
    * counter is conserved, and a producer that steals the fresh park
    * through a stale read just produces a post this round absorbs. */
   retro_atomic_fetch_add_int(&g_parked, 1);
   retro_procbarrier();
   s = retro_atomic_load_acquire_int(&g_seq);
   if (s != last_seen)
   {
      /* Something arrived during the announce: withdraw. If the
       * producer already claimed the park it will post; take it. */
      if (retro_atomic_fetch_add_int(&g_parked, -1) != 1)
      {
         /* The producer claimed the park and will post. Bounded: no wait
          * in this test may hang, since a hang is a timeout kill with
          * the output lost, and this is the one that would. */
         int got;
         retro_atomic_fetch_add_int(&g_parked, 1);
         slock_lock(g_lock);
         if (!g_posted)
            scond_wait_timeout(g_cond, g_lock, 2000000);
         got = g_posted;
         g_posted = 0;
         slock_unlock(g_lock);
         if (!got)
         {
            *seen_out = s;
            return 0;   /* a claimed park whose post never came */
         }
      }
      *seen_out = s;
      return 1;
   }
   /* Nothing new: really sleep on the condvar, with a 2 s timeout so a
    * lost wake is a failure rather than a hang. The producer publishes
    * continuously, so a correct barrier means a wake within microseconds. */
   slock_lock(g_lock);
   while (!g_posted)
   {
      if (!scond_wait_timeout(g_cond, g_lock, 2000000))
         break;
   }
   if (g_posted)
   {
      g_posted = 0;
      slock_unlock(g_lock);
      *seen_out = retro_atomic_load_acquire_int(&g_seq);
      return 1;
   }
   slock_unlock(g_lock);
   *seen_out = last_seen;
   return 0;
}

/* ---- checks ---------------------------------------------------------- */

static int check_resolution(void)
{
   enum retro_procbarrier_tier t = retro_procbarrier_init(0);
   printf("  tier: %s\n", retro_procbarrier_tier_name(t));

#if defined(_WIN32)
   if (t == RETRO_PROCBARRIER_MEMBARRIER || t == RETRO_PROCBARRIER_SIGNAL)
   {
      printf("  FAIL: Windows resolved a POSIX tier\n");
      return 0;
   }
#else
   if (t == RETRO_PROCBARRIER_FLUSHWRITEBUFFERS)
   {
      printf("  FAIL: POSIX resolved a Windows tier\n");
      return 0;
   }
#endif
#if !(defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64))
   if (t == RETRO_PROCBARRIER_PAGEFLIP)
   {
      printf("  FAIL: page flip chosen on non-x86, where it fences nothing\n");
      return 0;
   }
#endif
   if (t == RETRO_PROCBARRIER_NONE)
      printf("  note: no tier on this platform; the fence check is skipped\n");
   return 1;
}

static void busy_thread(void *arg)
{
   volatile unsigned x = 0;
   (void)arg;
   while (!retro_atomic_load_relaxed_int(&g_stop))
      x++;
}

static void sleeping_thread(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_relaxed_int(&g_stop))
      sleep_ms(5);
}

static int check_returns(void)
{
   sthread_t *busy[2], *slp;
   int i;
   retro_atomic_store_relaxed_int(&g_stop, 0);
   for (i = 0; i < 2; i++)
      busy[i] = sthread_create(busy_thread, NULL);
   slp = sthread_create(sleeping_thread, NULL);
   sleep_ms(20);
   for (i = 0; i < 200; i++)
      retro_procbarrier();
   retro_atomic_store_relaxed_int(&g_stop, 1);
   for (i = 0; i < 2; i++)
      sthread_join(busy[i]);
   sthread_join(slp);
   printf("  returns: 200 barriers with busy and sleeping threads alive\n");
   return 1;
}

static int check_fences(void)
{
   sthread_t *p;
   int rounds = 5000, r, last = 0, seen, lost = 0;

   if (retro_procbarrier_tier() == RETRO_PROCBARRIER_NONE)
      return 1;

   g_lock   = slock_new();
   g_cond   = scond_new();
   g_posted = 0;
   retro_atomic_store_relaxed_int(&g_seq, 0);
   retro_atomic_store_relaxed_int(&g_parked, 0);
   retro_atomic_store_relaxed_int(&g_stop, 0);
   retro_atomic_store_relaxed_int(&g_wakes, 0);
   p = sthread_create(producer, NULL);

   for (r = 0; r < rounds; r++)
   {
      if (!consumer_round(last, &seen))
      {
         lost++;
         printf("  lost wake at round %d (seq %d)\n", r, last);
         seen = retro_atomic_load_acquire_int(&g_seq);
         /* Each lost wake is a two-second timeout. Three is a result;
          * five thousand is a harness kill with the result lost. */
         if (lost >= 3)
         {
            printf("  stopping after %d lost wakes\n", lost);
            break;
         }
      }
      last = seen;
   }
   retro_atomic_store_relaxed_int(&g_stop, 1);
   sthread_join(p);
   scond_free(g_cond);
   slock_free(g_lock);

   printf("  fences: %d rounds, %d lost wakes, %d posts\n",
          rounds, lost, retro_atomic_load_relaxed_int(&g_wakes));
   return lost == 0;
}

int main(void)
{
   int ok = 1;
   /* Unbuffered: a run the harness kills on timeout must still show
    * which phase it was in and what it had found. */
   setvbuf(stdout, NULL, _IONBF, 0);
   printf("retro_procbarrier\n");
#if defined(_WIN32)
   { SYSTEM_INFO si; GetSystemInfo(&si);
     printf("  cpus: %u%s\n", (unsigned)si.dwNumberOfProcessors,
            si.dwNumberOfProcessors > 1 ? "" : "  (fence check is not load-bearing on one core)"); }
#else
   { long n = sysconf(_SC_NPROCESSORS_ONLN);
     printf("  cpus: %ld%s\n", n,
            n > 1 ? "" : "  (fence check is not load-bearing on one core)"); }
#endif
   ok &= check_resolution();
   ok &= check_returns();
   ok &= check_fences();
   printf(ok ? "procbarrier: ok\n" : "procbarrier: FAILED\n");
   return ok ? 0 : 1;
}
