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
 * 3. It is reentrant. Several threads issue barriers at once, each
 *    against the others. Every eventcount built on the barrier calls it
 *    from its own waiter, so concurrent callers are the normal case in a
 *    process with more than one of them, not an edge. A tier whose
 *    callers can stop or signal each other into a cycle deadlocks here,
 *    and one that shares per-call state between callers either returns
 *    early or faults. Bounded by a watchdog, so a deadlock is reported
 *    as a failure rather than a harness kill.
 *
 * 4. It survives threads exiting mid-walk. A walk can pick a thread,
 *    interrupt it, and have it exit before it ever answers; the barrier
 *    must see that the thread is gone rather than wait on it. Short-lived
 *    threads are started and joined continuously while barriers run.
 *
 * 5. It tolerates threads that block every signal, the way a GL or
 *    Vulkan driver's workers do. The signal tier must pass them by: one
 *    it waited on would never acknowledge, and the barrier would never
 *    return. POSIX only; Windows threads have no signal mask.
 *
 * 6. It survives a thread that blocks the signal after being picked. A
 *    busy thread flips its mask between all-blocked and open every few
 *    microseconds while barriers run, so walks catch it in every state,
 *    including with the signal queued and then blocked. Whether that
 *    signal is later delivered or never is, no walk may wait on it and
 *    nothing may be touched by a delivery that comes after the walk.
 *
 * 7. It fences. This is the one that matters and the one most tests of
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
#include <signal.h>
#include <pthread.h>
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

#define CONC_CALLERS 4
#define CONC_ITERS   20000
#define CONC_LIMIT_S 30

static retro_atomic_int_t g_conc_done;

static void barrier_caller(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < CONC_ITERS; i++)
      retro_procbarrier();
   retro_atomic_fetch_add_int(&g_conc_done, 1);
}

static int check_concurrent(void)
{
   sthread_t *callers[CONC_CALLERS], *busy;
   int i, waited_ms, done = 0;

   if (retro_procbarrier_tier() == RETRO_PROCBARRIER_NONE)
      return 1;

   retro_atomic_store_relaxed_int(&g_stop, 0);
   retro_atomic_store_relaxed_int(&g_conc_done, 0);
   /* One thread that never calls the barrier, so every walk has a
    * running target besides the other callers. */
   busy = sthread_create(busy_thread, NULL);
   for (i = 0; i < CONC_CALLERS; i++)
      callers[i] = sthread_create(barrier_caller, NULL);

   for (waited_ms = 0; waited_ms < CONC_LIMIT_S * 1000; waited_ms += 10)
   {
      done = retro_atomic_load_acquire_int(&g_conc_done);
      if (done >= CONC_CALLERS)
         break;
      sleep_ms(10);
   }
   if (done < CONC_CALLERS)
   {
      /* The stuck callers cannot be joined. Report and leave; exit
       * takes the process, and them, down with it. */
      printf("  FAIL: concurrent barriers stalled: %d of %d callers "
             "finished %d barriers each within %d s\n",
             done, CONC_CALLERS, CONC_ITERS, CONC_LIMIT_S);
      printf("procbarrier: FAILED\n");
      exit(1);
   }
   for (i = 0; i < CONC_CALLERS; i++)
      sthread_join(callers[i]);
   retro_atomic_store_relaxed_int(&g_stop, 1);
   sthread_join(busy);
   printf("  concurrent: %d callers x %d barriers, no stall\n",
          CONC_CALLERS, CONC_ITERS);
   return 1;
}

static void shortlived_thread(void *arg)
{
   volatile unsigned x = 0;
   (void)arg;
   while (x < 20000u)
      x++;
}

static void churn_thread(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_relaxed_int(&g_stop))
      sthread_join(sthread_create(shortlived_thread, NULL));
}

static int check_exiting(void)
{
   sthread_t *churn[2], *caller;
   int i, waited_ms, done = 0;

   if (retro_procbarrier_tier() == RETRO_PROCBARRIER_NONE)
      return 1;

   retro_atomic_store_relaxed_int(&g_stop, 0);
   retro_atomic_store_relaxed_int(&g_conc_done, 0);
   for (i = 0; i < 2; i++)
      churn[i] = sthread_create(churn_thread, NULL);
   caller = sthread_create(barrier_caller, NULL);

   for (waited_ms = 0; waited_ms < CONC_LIMIT_S * 1000; waited_ms += 10)
   {
      done = retro_atomic_load_acquire_int(&g_conc_done);
      if (done >= 1)
         break;
      sleep_ms(10);
   }
   if (done < 1)
   {
      printf("  FAIL: barriers stalled while threads were exiting "
             "(%d barriers not done within %d s)\n",
             CONC_ITERS, CONC_LIMIT_S);
      printf("procbarrier: FAILED\n");
      exit(1);
   }
   sthread_join(caller);
   retro_atomic_store_relaxed_int(&g_stop, 1);
   for (i = 0; i < 2; i++)
      sthread_join(churn[i]);
   printf("  exiting: %d barriers while threads were started and "
          "exiting\n", CONC_ITERS);
   return 1;
}

#if !defined(_WIN32)
static retro_atomic_int_t g_masked_ready;

static void masked_busy_thread(void *arg)
{
   sigset_t all;
   volatile unsigned x = 0;
   (void)arg;
   sigfillset(&all);
   pthread_sigmask(SIG_BLOCK, &all, NULL);
   retro_atomic_store_release_int(&g_masked_ready, 1);
   while (!retro_atomic_load_relaxed_int(&g_stop))
      x++;
}

static void flipping_thread(void *arg)
{
   sigset_t all, none;
   volatile unsigned x = 0;
   (void)arg;
   sigfillset(&all);
   sigemptyset(&none);
   while (!retro_atomic_load_relaxed_int(&g_stop))
   {
      pthread_sigmask(SIG_SETMASK, &all, NULL);
      for (x = 0; x < 2000u; x++) ;
      pthread_sigmask(SIG_SETMASK, &none, NULL);
      for (x = 0; x < 2000u; x++) ;
   }
}

static int check_flipping_thread(void)
{
   sthread_t *flip, *caller;
   int waited_ms, done = 0;

   if (retro_procbarrier_tier() == RETRO_PROCBARRIER_NONE)
      return 1;

   retro_atomic_store_relaxed_int(&g_stop, 0);
   retro_atomic_store_relaxed_int(&g_conc_done, 0);
   flip   = sthread_create(flipping_thread, NULL);
   caller = sthread_create(barrier_caller, NULL);

   for (waited_ms = 0; waited_ms < CONC_LIMIT_S * 1000; waited_ms += 10)
   {
      done = retro_atomic_load_acquire_int(&g_conc_done);
      if (done >= 1)
         break;
      sleep_ms(10);
   }
   if (done < 1)
   {
      printf("  FAIL: barriers stalled on a thread flipping its signal "
             "mask (%d barriers not done within %d s)\n",
             CONC_ITERS, CONC_LIMIT_S);
      printf("procbarrier: FAILED\n");
      exit(1);
   }
   sthread_join(caller);
   retro_atomic_store_relaxed_int(&g_stop, 1);
   sthread_join(flip);
   printf("  flipping: %d barriers past a busy thread flipping its "
          "signal mask\n", CONC_ITERS);
   return 1;
}

static int check_masked_thread(void)
{
   sthread_t *masked, *caller;
   int waited_ms, done = 0;

   if (retro_procbarrier_tier() == RETRO_PROCBARRIER_NONE)
      return 1;

   retro_atomic_store_relaxed_int(&g_stop, 0);
   retro_atomic_store_relaxed_int(&g_conc_done, 0);
   retro_atomic_store_relaxed_int(&g_masked_ready, 0);
   masked = sthread_create(masked_busy_thread, NULL);
   while (!retro_atomic_load_acquire_int(&g_masked_ready))
      sleep_ms(1);
   caller = sthread_create(barrier_caller, NULL);

   for (waited_ms = 0; waited_ms < CONC_LIMIT_S * 1000; waited_ms += 10)
   {
      done = retro_atomic_load_acquire_int(&g_conc_done);
      if (done >= 1)
         break;
      sleep_ms(10);
   }
   if (done < 1)
   {
      printf("  FAIL: barriers stalled on a thread with every signal "
             "blocked (%d barriers not done within %d s)\n",
             CONC_ITERS, CONC_LIMIT_S);
      printf("procbarrier: FAILED\n");
      exit(1);
   }
   sthread_join(caller);
   retro_atomic_store_relaxed_int(&g_stop, 1);
   sthread_join(masked);
   printf("  masked: %d barriers past a busy thread with every signal "
          "blocked\n", CONC_ITERS);
   return 1;
}
#endif

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
   ok &= check_concurrent();
   ok &= check_exiting();
#if !defined(_WIN32)
   ok &= check_masked_thread();
   ok &= check_flipping_thread();
#endif
   ok &= check_fences();
   printf(ok ? "procbarrier: ok\n" : "procbarrier: FAILED\n");
   return ok ? 0 : 1;
}
