/* retro_eventcount_bench.c - what the parking backend actually costs.
 *
 * A benchmark, not a test.  It exists to answer one question on
 * Windows: is going straight to rthreads' scond faster than going
 * through WaitOnAddress, given that scond already resolves
 * NtWaitForAlertByThreadId itself and skips the address hash?
 *
 * Build it twice and compare:
 *
 *   make                                   the platform's address-wait
 *                                          backend (WaitOnAddress on
 *                                          Windows, futex on Linux)
 *   make clean && make SCOND=1             rthreads scond
 *
 * and on Windows, scond's own tier is selectable at runtime, so the
 * interesting sweep is:
 *
 *   retro_eventcount_bench                          (WaitOnAddress)
 *   RTHREADS_SCOND=alert retro_eventcount_bench_scond
 *   RTHREADS_SCOND=keyed retro_eventcount_bench_scond
 *
 * Three cases, because they stress different things and only one of
 * them is the hot path for a threaded renderer:
 *
 *   idle_notify   notify() with no consumer parked.  This is the case
 *                 that dominates: a producer publishes thousands of
 *                 times a frame and the consumer is usually awake.
 *                 Neither backend is invoked here -- the waiters check
 *                 short-circuits before any OS call -- so the two
 *                 builds should land on top of each other.  If they
 *                 do not, something other than the backend is being
 *                 measured.
 *
 *   pingpong      Round trip with the consumer genuinely parked every
 *                 round: producer notifies, consumer wakes, replies,
 *                 producer waits.  Divided by two this is wake-to-run
 *                 latency, which is where the backends can actually
 *                 differ -- WakeByAddressAll hashes the address and
 *                 takes a bucket lock shared with every other waited
 *                 address in the process, scond walks a list private
 *                 to the object.
 *
 *   broadcast     N consumers parked, one notify, all of them out.
 *                 Scales the per-waiter wake cost, and is the shape a
 *                 drain barrier with several readers would take.
 *
 * Pin the process to at least two physical cores before believing any
 * of it, and run each build several times: these are syscall-bound
 * numbers and the variance is not small.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <features/features_cpu.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef IDLE_NOTIFIES
#define IDLE_NOTIFIES   20000000
#endif
#ifndef PINGPONG_ROUNDS
#define PINGPONG_ROUNDS 100000
#endif
#ifndef BROADCAST_ROUNDS
#define BROADCAST_ROUNDS 20000
#endif
#define BROADCAST_WAITERS 4

static retro_eventcount_t ec;
static retro_atomic_int_t turn;
static retro_atomic_int_t stop;
static retro_atomic_int_t arrived;

/* ---- idle notify -------------------------------------------------- */

static void bench_idle(void)
{
   retro_time_t t0, t1;
   int i;

   if (!retro_eventcount_init(&ec))
      return;

   t0 = cpu_features_get_time_usec();
   for (i = 0; i < IDLE_NOTIFIES; i++)
      retro_eventcount_notify(&ec);
   t1 = cpu_features_get_time_usec();

   retro_eventcount_free(&ec);

   printf("  idle_notify   %8.2f ns/notify  (%d notifies, no waiter)\n",
         (double)(t1 - t0) * 1000.0 / (double)IDLE_NOTIFIES,
         IDLE_NOTIFIES);
}

/* ---- ping-pong ---------------------------------------------------- */

/* turn == 0: the pong thread runs.  turn == 1: the ping thread runs. */

static void pong_thread(void *unused)
{
   (void)unused;

   for (;;)
   {
      int key;

      if (retro_atomic_load_acquire_int(&stop))
         return;

      if (retro_atomic_load_acquire_int(&turn) == 0)
      {
         retro_atomic_store_release_int(&turn, 1);
         retro_eventcount_notify(&ec);
         continue;
      }

      key = retro_eventcount_prepare_wait(&ec);

      if (retro_atomic_load_acquire_int(&turn) == 0 ||
          retro_atomic_load_acquire_int(&stop))
      {
         retro_eventcount_cancel_wait(&ec);
         continue;
      }

      retro_eventcount_commit_wait(&ec, key);
   }
}

static void bench_pingpong(void)
{
   sthread_t *t;
   retro_time_t t0, t1;
   int i;

   if (!retro_eventcount_init(&ec))
      return;

   retro_atomic_int_init(&turn, 1);
   retro_atomic_int_init(&stop, 0);

   t = sthread_create(pong_thread, NULL);

   /* Let it reach a real park rather than racing the first round. */
#if defined(_WIN32)
   Sleep(50);
#else
   usleep(50000);
#endif

   t0 = cpu_features_get_time_usec();
   for (i = 0; i < PINGPONG_ROUNDS; i++)
   {
      retro_atomic_store_release_int(&turn, 0);
      retro_eventcount_notify(&ec);

      for (;;)
      {
         int key;

         if (retro_atomic_load_acquire_int(&turn) == 1)
            break;

         key = retro_eventcount_prepare_wait(&ec);

         if (retro_atomic_load_acquire_int(&turn) == 1)
         {
            retro_eventcount_cancel_wait(&ec);
            break;
         }

         retro_eventcount_commit_wait(&ec, key);
      }
   }
   t1 = cpu_features_get_time_usec();

   retro_atomic_store_release_int(&stop, 1);
   retro_atomic_store_release_int(&turn, 0);
   retro_eventcount_notify(&ec);
   sthread_join(t);
   retro_eventcount_free(&ec);

   printf("  pingpong      %8.2f us/round   %8.2f us wake-to-run"
          "  (%d rounds)\n",
         (double)(t1 - t0) / (double)PINGPONG_ROUNDS,
         (double)(t1 - t0) / (double)PINGPONG_ROUNDS / 2.0,
         PINGPONG_ROUNDS);
}

/* ---- broadcast ---------------------------------------------------- */

static retro_atomic_int_t gen;

static void bcast_thread(void *unused)
{
   int seen = 0;
   (void)unused;

   for (;;)
   {
      int key;
      int g;

      if (retro_atomic_load_acquire_int(&stop))
         return;

      g = retro_atomic_load_acquire_int(&gen);
      if (g != seen)
      {
         seen = g;
         retro_atomic_fetch_add_int(&arrived, 1);
         continue;
      }

      key = retro_eventcount_prepare_wait(&ec);

      if (retro_atomic_load_acquire_int(&gen) != seen ||
          retro_atomic_load_acquire_int(&stop))
      {
         retro_eventcount_cancel_wait(&ec);
         continue;
      }

      retro_eventcount_commit_wait(&ec, key);
   }
}

static void bench_broadcast(void)
{
   sthread_t *t[BROADCAST_WAITERS];
   retro_time_t t0, t1;
   int i, r;

   if (!retro_eventcount_init(&ec))
      return;

   retro_atomic_int_init(&gen, 0);
   retro_atomic_int_init(&stop, 0);
   retro_atomic_int_init(&arrived, 0);

   for (i = 0; i < BROADCAST_WAITERS; i++)
      t[i] = sthread_create(bcast_thread, NULL);

#if defined(_WIN32)
   Sleep(50);
#else
   usleep(50000);
#endif

   t0 = cpu_features_get_time_usec();
   for (r = 1; r <= BROADCAST_ROUNDS; r++)
   {
      retro_atomic_store_release_int(&arrived, 0);
      retro_atomic_store_release_int(&gen, r);
      retro_eventcount_notify(&ec);

      while (retro_atomic_load_acquire_int(&arrived) < BROADCAST_WAITERS)
         retro_cpu_relax();
   }
   t1 = cpu_features_get_time_usec();

   retro_atomic_store_release_int(&stop, 1);
   retro_eventcount_notify(&ec);
   for (i = 0; i < BROADCAST_WAITERS; i++)
      sthread_join(t[i]);
   retro_eventcount_free(&ec);

   printf("  broadcast     %8.2f us/round   %8.2f us/waiter"
          "  (%d rounds, %d waiters)\n",
         (double)(t1 - t0) / (double)BROADCAST_ROUNDS,
         (double)(t1 - t0) / (double)BROADCAST_ROUNDS
            / (double)BROADCAST_WAITERS,
         BROADCAST_ROUNDS, BROADCAST_WAITERS);
}

/* ---- what the spin itself costs ---------------------------------- */

/* The spin is free when the wake arrives during it, which is what the
 * lanes above measure.  A consumer that parks because there really is
 * no work pays the whole thing before sleeping, every time, and on a
 * battery-powered target that is burn on the path that should be idle.
 *
 * The count is also not portable: a PAUSE is worth an order of
 * magnitude more cycles on some processors than others, so the only
 * way to know what the default spin means on this machine is to time
 * one. */
static void bench_spin_cost(void)
{
   const long   n = 50000000;
   retro_time_t t0, t1;
   double       per;
   long         i;

   t0 = cpu_features_get_time_usec();
   for (i = 0; i < n; i++)
      retro_cpu_relax();
   t1 = cpu_features_get_time_usec();

   per = (double)(t1 - t0) * 1000.0 / (double)n;

   printf("  spin_cost     %8.2f ns/relax    %8.2f us for the %u "
          "iterations this backend chose\n",
         per, per * (double)retro_eventcount_spin_iters() / 1000.0,
         retro_eventcount_spin_iters());
}

int main(void)
{
   const char *tier = getenv("RTHREADS_SCOND");

   printf("retro_eventcount backend: %s (atomics: %s)%s%s\n",
         retro_eventcount_backend_name(), RETRO_ATOMIC_BACKEND_NAME,
         tier ? "  RTHREADS_SCOND=" : "", tier ? tier : "");

   bench_idle();
   bench_spin_cost();
   bench_pingpong();
   bench_broadcast();

   return 0;
}
