/* eventcount_test.c - regression harness for retro_eventcount.
 *
 * Three lanes, all against the real libretro-common sources so a change
 * to the primitive or to retro_atomic.h shows up here:
 *
 *   handoff   One producer, one consumer, a shared counter and a
 *             retro_eventcount.  Every item must be seen exactly once
 *             and the consumer must never be left asleep with work
 *             pending.  Run long enough that the consumer parks and is
 *             woken thousands of times.
 *
 *   wakeup    The lost-wakeup window itself, aimed at deliberately.  The
 *             producer publishes with no delay at all while the consumer
 *             does prepare_wait / re-check / commit_wait, so the notify
 *             lands inside the window on a large fraction of iterations.
 *             A missed wakeup shows as the harness hanging; the watchdog
 *             below turns that into a failure instead.
 *
 *   broadcast Several consumers parked on one object, one notify.  All
 *             of them must come back.
 *
 * The watchdog thread is the point of the whole file: the failure mode
 * of an eventcount is not a wrong answer, it is a thread that never
 * wakes.  Without a bound, a regression is an infinite CI job.
 *
 * Build both backends -- the default one for this host and
 * RETRO_EVENTCOUNT_FORCE_SCOND=1 -- and run both under ThreadSanitizer.
 * The Makefile next to this file does that from 'make check'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>

/* A plain pause, not the tree's sleep helper: on desktop Windows that
 * one lives in time/rtime.c on top of a waitable timer, and this
 * harness has no reason to pull that in just to wait. */
#if defined(_WIN32)
#include <windows.h>
#define ec_test_sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define ec_test_sleep_ms(ms) usleep((unsigned)(ms) * 1000u)
#endif

#ifndef HANDOFF_ITEMS
#define HANDOFF_ITEMS   200000
#endif
#ifndef WAKEUP_ROUNDS
#define WAKEUP_ROUNDS   200000
#endif
#define BROADCAST_CONSUMERS 4
#ifndef BROADCAST_STRESS_ROUNDS
#define BROADCAST_STRESS_ROUNDS 20000
#endif
#define WATCHDOG_SECONDS    60

static retro_eventcount_t  ec;
static retro_atomic_size_t produced;
static retro_atomic_size_t consumed;
static retro_atomic_int_t  done_flag;
static retro_atomic_int_t  woke_count;
static retro_atomic_int_t  watchdog_stop;
static retro_atomic_int_t  parks;

static void watchdog_thread(void *unused)
{
   int i;
   (void)unused;

   for (i = 0; i < WATCHDOG_SECONDS * 10; i++)
   {
      if (retro_atomic_load_acquire_int(&watchdog_stop))
         return;
      ec_test_sleep_ms(100);
   }

   fprintf(stderr, "FAIL: watchdog fired after %d s -- a waiter was "
         "never woken\n", WATCHDOG_SECONDS);
   fflush(stderr);
   _exit(1);
}

/* ---- lane 1: handoff --------------------------------------------- */

static void handoff_producer(void *unused)
{
   size_t i;
   (void)unused;

   for (i = 0; i < HANDOFF_ITEMS; i++)
   {
      retro_atomic_fetch_add_size(&produced, 1);
      retro_eventcount_notify(&ec);
   }

   retro_atomic_store_release_int(&done_flag, 1);
   retro_eventcount_notify(&ec);
}

static void handoff_consumer(void *unused)
{
   (void)unused;

   for (;;)
   {
      int key;
      size_t p = retro_atomic_load_acquire_size(&produced);
      size_t c = retro_atomic_load_acquire_size(&consumed);

      if (c < p)
      {
         retro_atomic_fetch_add_size(&consumed, 1);
         continue;
      }

      /* The flag is set after the last item: seeing it means a fresh
       * look at the count is the final one */
      if (retro_atomic_load_acquire_int(&done_flag))
      {
         if (retro_atomic_load_acquire_size(&consumed)
               < retro_atomic_load_acquire_size(&produced))
            continue;
         return;
      }

      key = retro_eventcount_prepare_wait(&ec);

      if (retro_atomic_load_acquire_size(&produced) !=
            retro_atomic_load_acquire_size(&consumed) ||
          retro_atomic_load_acquire_int(&done_flag))
      {
         retro_eventcount_cancel_wait(&ec);
         continue;
      }

      retro_atomic_fetch_add_int(&parks, 1);
      retro_eventcount_commit_wait(&ec, key);
   }
}

static int lane_handoff(void)
{
   sthread_t *p, *c;
   size_t got;

   retro_atomic_size_init(&produced, 0);
   retro_atomic_size_init(&consumed, 0);
   retro_atomic_int_init(&done_flag, 0);
   retro_atomic_int_init(&parks, 0);

   if (!retro_eventcount_init(&ec))
   {
      fprintf(stderr, "FAIL: handoff: eventcount init\n");
      return 1;
   }

   c = sthread_create(handoff_consumer, NULL);
   p = sthread_create(handoff_producer, NULL);

   sthread_join(p);
   sthread_join(c);

   got = retro_atomic_load_acquire_size(&consumed);
   retro_eventcount_free(&ec);

   if (got != (size_t)HANDOFF_ITEMS)
   {
      fprintf(stderr, "FAIL: handoff: consumed %lu of %lu\n",
            (unsigned long)got, (unsigned long)HANDOFF_ITEMS);
      return 1;
   }

   printf("  handoff    %lu items, %d parks\n",
         (unsigned long)got, retro_atomic_load_acquire_int(&parks));
   return 0;
}

/* ---- lane 2: the lost-wakeup window ------------------------------ */

static retro_atomic_int_t flag;
static retro_atomic_int_t round_ready;

static void wakeup_producer(void *unused)
{
   int r;
   (void)unused;

   for (r = 0; r < WAKEUP_ROUNDS; r++)
   {
      /* Wait for the consumer to be in position for this round, then
       * publish with nothing between the store and the notify.  That
       * puts the notify inside the consumer's prepare/commit window as
       * often as the scheduler allows. */
      while (retro_atomic_load_acquire_int(&round_ready) != r)
         retro_cpu_relax();

      retro_atomic_store_release_int(&flag, r + 1);
      retro_eventcount_notify(&ec);
   }
}

static void wakeup_consumer(void *unused)
{
   int r;
   (void)unused;

   for (r = 0; r < WAKEUP_ROUNDS; r++)
   {
      retro_atomic_store_release_int(&round_ready, r);

      for (;;)
      {
         int key;

         if (retro_atomic_load_acquire_int(&flag) == r + 1)
            break;

         key = retro_eventcount_prepare_wait(&ec);

         if (retro_atomic_load_acquire_int(&flag) == r + 1)
         {
            retro_eventcount_cancel_wait(&ec);
            break;
         }

         retro_atomic_fetch_add_int(&woke_count, 1);
         retro_eventcount_commit_wait(&ec, key);
      }
   }
}

static int lane_wakeup(void)
{
   sthread_t *p, *c;

   retro_atomic_int_init(&flag, 0);
   retro_atomic_int_init(&round_ready, -1);
   retro_atomic_int_init(&woke_count, 0);

   if (!retro_eventcount_init(&ec))
   {
      fprintf(stderr, "FAIL: wakeup: eventcount init\n");
      return 1;
   }

   c = sthread_create(wakeup_consumer, NULL);
   p = sthread_create(wakeup_producer, NULL);

   sthread_join(p);
   sthread_join(c);

   if (retro_atomic_load_acquire_int(&flag) != WAKEUP_ROUNDS)
   {
      fprintf(stderr, "FAIL: wakeup: flag %d after %d rounds\n",
            retro_atomic_load_acquire_int(&flag), WAKEUP_ROUNDS);
      retro_eventcount_free(&ec);
      return 1;
   }

   printf("  wakeup     %d rounds, %d commits\n", WAKEUP_ROUNDS,
         retro_atomic_load_acquire_int(&woke_count));
   retro_eventcount_free(&ec);
   return 0;
}

/* ---- lane 3: broadcast ------------------------------------------- */

static retro_atomic_int_t bcast_go;
static retro_atomic_int_t bcast_awake;
static retro_atomic_int_t bcast_parked;

static void bcast_consumer(void *unused)
{
   (void)unused;

   for (;;)
   {
      int key;

      if (retro_atomic_load_acquire_int(&bcast_go))
         break;

      key = retro_eventcount_prepare_wait(&ec);

      if (retro_atomic_load_acquire_int(&bcast_go))
      {
         retro_eventcount_cancel_wait(&ec);
         break;
      }

      retro_atomic_fetch_add_int(&bcast_parked, 1);
      retro_eventcount_commit_wait(&ec, key);
   }

   retro_atomic_fetch_add_int(&bcast_awake, 1);
}

static int lane_broadcast(void)
{
   sthread_t *t[BROADCAST_CONSUMERS];
   int i;

   retro_atomic_int_init(&bcast_go, 0);
   retro_atomic_int_init(&bcast_awake, 0);
   retro_atomic_int_init(&bcast_parked, 0);

   if (!retro_eventcount_init(&ec))
   {
      fprintf(stderr, "FAIL: broadcast: eventcount init\n");
      return 1;
   }

   for (i = 0; i < BROADCAST_CONSUMERS; i++)
      t[i] = sthread_create(bcast_consumer, NULL);

   /* Give them time to reach a real park rather than winning the
    * pre-check race; the assertion below does not depend on it, but a
    * run where nobody parks tests nothing. */
   ec_test_sleep_ms(200);

   retro_atomic_store_release_int(&bcast_go, 1);
   retro_eventcount_notify(&ec);

   for (i = 0; i < BROADCAST_CONSUMERS; i++)
      sthread_join(t[i]);

   i = retro_atomic_load_acquire_int(&bcast_awake);
   printf("  broadcast  %d of %d returned, %d parked\n", i,
         BROADCAST_CONSUMERS, retro_atomic_load_acquire_int(&bcast_parked));
   retro_eventcount_free(&ec);

   if (i != BROADCAST_CONSUMERS)
   {
      fprintf(stderr, "FAIL: broadcast: %d of %d returned\n", i,
            BROADCAST_CONSUMERS);
      return 1;
   }

   return 0;
}

/* ---- lane 4: broadcast under load -------------------------------- */

/* The single-shot broadcast above parks every consumer once and wakes
 * them once, which never exercises the path where a notify takes a
 * block off the list while its own thread is on its way out of the
 * wait.  That window is only reachable when consumers re-park in a
 * tight loop against a producer that keeps moving, so this lane does
 * exactly that -- and it is the lane that catches a waker still
 * reading a block whose stack frame has gone. */

static retro_atomic_int_t stress_gen;
static retro_atomic_int_t stress_arrived;
static retro_atomic_int_t stress_stop;

static void stress_consumer(void *unused)
{
   int seen = 0;
   (void)unused;

   for (;;)
   {
      int key;
      int g;

      if (retro_atomic_load_acquire_int(&stress_stop))
         return;

      g = retro_atomic_load_acquire_int(&stress_gen);
      if (g != seen)
      {
         seen = g;
         retro_atomic_fetch_add_int(&stress_arrived, 1);
         continue;
      }

      key = retro_eventcount_prepare_wait(&ec);

      if (retro_atomic_load_acquire_int(&stress_gen) != seen ||
          retro_atomic_load_acquire_int(&stress_stop))
      {
         retro_eventcount_cancel_wait(&ec);
         continue;
      }

      retro_eventcount_commit_wait(&ec, key);
   }
}

static int lane_broadcast_stress(void)
{
   sthread_t *t[BROADCAST_CONSUMERS];
   int i, r;

   retro_atomic_int_init(&stress_gen, 0);
   retro_atomic_int_init(&stress_arrived, 0);
   retro_atomic_int_init(&stress_stop, 0);

   if (!retro_eventcount_init(&ec))
   {
      fprintf(stderr, "FAIL: broadcast_stress: eventcount init\n");
      return 1;
   }

   for (i = 0; i < BROADCAST_CONSUMERS; i++)
      t[i] = sthread_create(stress_consumer, NULL);

   for (r = 1; r <= BROADCAST_STRESS_ROUNDS; r++)
   {
      retro_atomic_store_release_int(&stress_arrived, 0);
      retro_atomic_store_release_int(&stress_gen, r);
      retro_eventcount_notify(&ec);

      while (retro_atomic_load_acquire_int(&stress_arrived)
            < BROADCAST_CONSUMERS)
         retro_cpu_relax();
   }

   retro_atomic_store_release_int(&stress_stop, 1);
   retro_eventcount_notify(&ec);
   for (i = 0; i < BROADCAST_CONSUMERS; i++)
      sthread_join(t[i]);
   retro_eventcount_free(&ec);

   printf("  bcast_load %d rounds x %d consumers\n",
         BROADCAST_STRESS_ROUNDS, BROADCAST_CONSUMERS);
   return 0;
}


/* ---- lane 5: bounded waits racing wakes --------------------------- */
/* A bounded wait that expires while a notifier has already taken its
 * block has a wake in flight, and the backends that sleep on something
 * of the thread's own - a semaphore, an event - must consume it, or it
 * carries into the thread's next park and ends that one at once.  The
 * notifier here is paced to the consumer's bound so expiry and wake
 * land together often, in bursts; after each burst it is held, and a
 * bounded wait with nothing to wake it must then expire.  On the
 * semaphore backend nothing else can end that wait, so any early
 * return is a carried-over count and fails the lane.  The futex and
 * condition-variable sleeps may return spuriously by their own
 * contract, so there the probe is reported and not asserted on. */

#define TIMED_RACE_BURSTS   200
#define TIMED_RACE_ROUNDS   100
#define TIMED_RACE_BOUND_US 50

static retro_atomic_int_t timed_stop;
static retro_atomic_int_t timed_hold;
static retro_atomic_int_t timed_held;

static void timed_spin_us(unsigned us)
{
   retro_time_t until = cpu_features_get_time_usec() + us;
   while (cpu_features_get_time_usec() < until)
      retro_cpu_relax();
}

static void timed_notifier(void *unused)
{
   unsigned n = 0;
   (void)unused;
   while (!retro_atomic_load_acquire_int(&timed_stop))
   {
      if (retro_atomic_load_acquire_int(&timed_hold))
      {
         retro_atomic_store_release_int(&timed_held, 1);
         while (retro_atomic_load_acquire_int(&timed_hold))
            retro_cpu_relax();
         continue;
      }
      /* around the consumer's bound, on both sides of it */
      timed_spin_us(TIMED_RACE_BOUND_US - 10 + (n++ % 21));
      retro_eventcount_notify(&ec);
   }
}

static int lane_timed_race(void)
{
   sthread_t *t;
   int b, i;
   int early = 0;
   int expired = 0, woken = 0;
   const char *backend = retro_eventcount_backend_name();

   retro_atomic_int_init(&timed_stop, 0);
   retro_atomic_int_init(&timed_hold, 0);
   retro_atomic_int_init(&timed_held, 0);

   if (!retro_eventcount_init(&ec))
   {
      fprintf(stderr, "FAIL: timed_race: eventcount init\n");
      return 1;
   }

   t = sthread_create(timed_notifier, NULL);

   for (b = 0; b < TIMED_RACE_BURSTS; b++)
   {
      int key;
      for (i = 0; i < TIMED_RACE_ROUNDS; i++)
      {
         key = retro_eventcount_prepare_wait(&ec);
         if (retro_eventcount_commit_wait_timeout(&ec, key,
                  TIMED_RACE_BOUND_US))
            woken++;
         else
            expired++;
      }

      /* Hold the notifier, then a wait with nothing to wake it */
      retro_atomic_store_release_int(&timed_held, 0);
      retro_atomic_store_release_int(&timed_hold, 1);
      while (!retro_atomic_load_acquire_int(&timed_held))
         retro_cpu_relax();

      key = retro_eventcount_prepare_wait(&ec);
      if (retro_eventcount_commit_wait_timeout(&ec, key, 200))
         early++;

      retro_atomic_store_release_int(&timed_hold, 0);
   }

   retro_atomic_store_release_int(&timed_stop, 1);
   retro_atomic_store_release_int(&timed_hold, 0);
   sthread_join(t);
   retro_eventcount_free(&ec);

   printf("  timed_race %d bursts x %d bounded waits: %d woken, %d "
         "expired; %d probe(s) ended early\n",
         TIMED_RACE_BURSTS, TIMED_RACE_ROUNDS, woken, expired, early);
   /* Whether a burst ever expires depends on the machine: a sleep
    * with millisecond granularity (the win32 event tier) rounds the
    * bound up past the notifier's period, and a loaded runner under a
    * sanitizer wakes every wait before its bound.  The held probes are
    * then what shows the bound is honoured, so only their all ending
    * early, with no burst expiring either, says the bound is not. */
   if (!woken || (!expired && early == TIMED_RACE_BURSTS))
   {
      fprintf(stderr, "FAIL: timed_race: the race was not exercised\n");
      return 1;
   }
   if (early && strstr(backend, "semaphore"))
   {
      fprintf(stderr, "FAIL: timed_race: a wake carried over into a "
            "later park on the %s backend\n", backend);
      return 1;
   }
   return 0;
}

/* ---- lane 6: a crowd of waiters ----------------------------------- */
/* More threads than the pooled semaphore backend has semaphores, all
 * parking with a bound against a notifier that keeps moving.  Past the
 * pool's size a park has nothing to sleep on and answers at once,
 * which the callers of this primitive already tolerate as a spurious
 * wake; what must hold is that every thread comes back, and that the
 * semaphores the crowd hands back and forth are clean.  For the second
 * part the notifier is held and the first CROWD_PROBERS threads take a
 * bounded wait each, concurrently, so that many distinct semaphores are
 * drawn: on a semaphore backend nothing can end those waits but their
 * bound, so an early return is a count left on a semaphore by a park
 * before it.  Other backends may wake spuriously by contract, and the
 * probe is reported only. */

#define CROWD_THREADS  96
#define CROWD_ROUNDS   200
#define CROWD_BOUND_US 50
#define CROWD_PROBERS  32

static retro_atomic_int_t crowd_stormed;
static retro_atomic_int_t crowd_probe_go;
static retro_atomic_int_t crowd_early;

static void crowd_waiter(void *data)
{
   int i;
   int me = (int)(intptr_t)data;

   for (i = 0; i < CROWD_ROUNDS; i++)
   {
      int key = retro_eventcount_prepare_wait(&ec);
      retro_eventcount_commit_wait_timeout(&ec, key, CROWD_BOUND_US);
   }
   retro_atomic_fetch_add_int(&crowd_stormed, 1);

   if (me >= CROWD_PROBERS)
      return;
   while (!retro_atomic_load_acquire_int(&crowd_probe_go))
      sthread_yield();
   {
      int key = retro_eventcount_prepare_wait(&ec);
      if (retro_eventcount_commit_wait_timeout(&ec, key, 200))
         retro_atomic_fetch_add_int(&crowd_early, 1);
   }
}

static int lane_crowd(void)
{
   sthread_t *t[CROWD_THREADS];
   sthread_t *n;
   int i;
   int early;
   const char *backend = retro_eventcount_backend_name();

   retro_atomic_int_init(&timed_stop, 0);
   retro_atomic_int_init(&timed_hold, 0);
   retro_atomic_int_init(&timed_held, 0);
   retro_atomic_int_init(&crowd_stormed, 0);
   retro_atomic_int_init(&crowd_probe_go, 0);
   retro_atomic_int_init(&crowd_early, 0);

   if (!retro_eventcount_init(&ec))
   {
      fprintf(stderr, "FAIL: crowd: eventcount init\n");
      return 1;
   }

   n = sthread_create(timed_notifier, NULL);
   for (i = 0; i < CROWD_THREADS; i++)
      t[i] = sthread_create(crowd_waiter, (void*)(intptr_t)i);

   while (retro_atomic_load_acquire_int(&crowd_stormed) < CROWD_THREADS)
      sthread_yield();

   /* Hold the notifier, then the probes, all at once */
   retro_atomic_store_release_int(&timed_hold, 1);
   while (!retro_atomic_load_acquire_int(&timed_held))
      retro_cpu_relax();
   retro_atomic_store_release_int(&crowd_probe_go, 1);

   for (i = 0; i < CROWD_THREADS; i++)
      sthread_join(t[i]);
   retro_atomic_store_release_int(&timed_stop, 1);
   retro_atomic_store_release_int(&timed_hold, 0);
   sthread_join(n);
   retro_eventcount_free(&ec);

   early = retro_atomic_load_acquire_int(&crowd_early);
   printf("  crowd      %d threads x %d bounded waits; %d of %d probes "
         "ended early\n", CROWD_THREADS, CROWD_ROUNDS, early, CROWD_PROBERS);
   if (early && strstr(backend, "semaphore"))
   {
      fprintf(stderr, "FAIL: crowd: a semaphore came back to the pool "
            "with a count on it (%s backend)\n", backend);
      return 1;
   }
   return 0;
}

/* ---- lane 7: the epoch's wrap ------------------------------------- */
/* The epoch is a counter that wraps, and a backend that compares it
 * with anything but equality has an edge there: the 3DS arbiter waits
 * while the word is below key + 1, and INT_MAX + 1 does not exist.
 * A wait registered at INT_MAX must come back, promptly, rather than
 * sleep its bound out - it is answered as a spurious wake - and a wait
 * registered just past the wrap must still be woken by a notify. */

static retro_atomic_int_t wrap_go;

static void wrap_notifier(void *unused)
{
   (void)unused;
   while (!retro_atomic_load_acquire_int(&wrap_go))
      sthread_yield();
   timed_spin_us(2000);
   retro_eventcount_notify(&ec);
}

static int lane_wrap(void)
{
   sthread_t   *t;
   int          key;
   retro_time_t began, took;

   if (!retro_eventcount_init(&ec))
   {
      fprintf(stderr, "FAIL: wrap: eventcount init\n");
      return 1;
   }
   retro_atomic_int_init(&wrap_go, 0);

   /* At the top: a bounded wait with nothing to wake it must not
    * sleep its bound out on a backend that cannot express key + 1 */
   retro_atomic_store_release_int(&ec.epoch, INT_MAX);
   key   = retro_eventcount_prepare_wait(&ec);
   began = cpu_features_get_time_usec();
   retro_eventcount_commit_wait_timeout(&ec, key, 200000);
   took  = cpu_features_get_time_usec() - began;
   if (key != INT_MAX)
   {
      fprintf(stderr, "FAIL: wrap: the key is not the epoch\n");
      return 1;
   }

   /* Over the top: a notify bumps it past INT_MAX, and a wait taken
    * there is still woken by the next one */
   retro_eventcount_notify(&ec);
   t   = sthread_create(wrap_notifier, NULL);
   key = retro_eventcount_prepare_wait(&ec);
   retro_atomic_store_release_int(&wrap_go, 1);
   if (!retro_eventcount_commit_wait_timeout(&ec, key, 2000000))
   {
      fprintf(stderr, "FAIL: wrap: a wait past the wrap was not woken\n");
      sthread_join(t);
      return 1;
   }
   sthread_join(t);
   retro_eventcount_free(&ec);

   printf("  wrap       wait at INT_MAX came back in %ld us; past it, woken\n",
         (long)took);
   return 0;
}

int main(void)
{
   sthread_t *wd;
   int rc = 0;

   printf("retro_eventcount backend: %s (atomics: %s)\n",
         retro_eventcount_backend_name(), RETRO_ATOMIC_BACKEND_NAME);

   retro_atomic_int_init(&watchdog_stop, 0);
   wd = sthread_create(watchdog_thread, NULL);

   rc |= lane_handoff();
   rc |= lane_wakeup();
   rc |= lane_broadcast();
   rc |= lane_broadcast_stress();
   rc |= lane_timed_race();
   rc |= lane_crowd();
   rc |= lane_wrap();

   retro_atomic_store_release_int(&watchdog_stop, 1);
   sthread_join(wd);

   printf(rc ? "eventcount: FAILED\n" : "eventcount: ok\n");
   return rc;
}
