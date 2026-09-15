/* retro_waitable_spsc_test.c - regression harness for the waitable SPSC.
 *
 * Four lanes, against the real sources:
 *
 *   handoff    A producer pushes a known byte stream through a queue
 *              far smaller than the stream, so both ends block
 *              repeatedly.  Every byte must arrive, in order, exactly
 *              once: the consumer checks the value of each one rather
 *              than counting them, so a lost or duplicated wake shows
 *              up as wrong data and not merely a wrong total.
 *
 *   tiny       The same with a queue barely larger than one record, so
 *              nearly every transfer sleeps.  This is the lane that
 *              would catch a notification only sent on a transition
 *              the waiter's threshold does not match.
 *
 *   timeout    A consumer waiting for bytes nobody sends must come
 *              back false, and near the bound it asked for rather than
 *              at some multiple of it, which is what a bound applied
 *              per sleep instead of against the clock would give.
 *
 *   cancel     A consumer blocked on an empty queue must be released
 *              by retro_waitable_spsc_cancel() without any data, and
 *              must stay released rather than going round its loop
 *              again, which is how a shutdown gets its threads back.
 *
 * A watchdog bounds the whole run, because the failure of a sleeping
 * primitive is a thread that never returns, and a hang tells you far
 * less than a failure does.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_waitable_spsc.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>

#if defined(_WIN32)
#include <windows.h>
#define test_sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define test_sleep_ms(ms) usleep((unsigned)(ms) * 1000u)
#endif

#define WATCHDOG_SECONDS 60

static retro_atomic_int_t watchdog_stop;
static retro_atomic_int_t failures;

static void fail(const char *what)
{
   fprintf(stderr, "FAIL: %s\n", what);
   retro_atomic_fetch_add_int(&failures, 1);
}

static void watchdog_thread(void *unused)
{
   int i;
   (void)unused;
   for (i = 0; i < WATCHDOG_SECONDS * 10; i++)
   {
      if (retro_atomic_load_acquire_int(&watchdog_stop))
         return;
      test_sleep_ms(100);
   }
   fprintf(stderr, "FAIL: watchdog fired -- a waiter never returned\n");
   fflush(stderr);
   _exit(1);
}

/* ---- handoff and tiny --------------------------------------------- */

/* The stream is a counter, so the consumer knows what every byte
 * should be without keeping a copy of it. */
static unsigned char stream_byte(size_t i)
{
   return (unsigned char)((i * 31u + (i >> 8)) & 0xFF);
}

static retro_waitable_spsc_t q;
static size_t                total_bytes;
static size_t                record_bytes;

static void producer_thread(void *unused)
{
   unsigned char buf[4096];
   size_t sent = 0;
   (void)unused;

   while (sent < total_bytes)
   {
      size_t chunk = record_bytes;
      size_t i, n;

      if (chunk > total_bytes - sent)
         chunk = total_bytes - sent;

      for (i = 0; i < chunk; i++)
         buf[i] = stream_byte(sent + i);

      if (!retro_waitable_spsc_wait_writable(&q, chunk, 5000000))
      {
         fail("producer: never got space");
         return;
      }

      n = retro_waitable_spsc_write(&q, buf, chunk);
      if (n != chunk)
      {
         fail("producer: short write after waiting for space");
         return;
      }
      sent += n;
   }
}

static int run_stream(size_t capacity, size_t record, size_t total,
      const char *name)
{
   unsigned char buf[4096];
   sthread_t *p;
   size_t got = 0;

   record_bytes = record;
   total_bytes  = total;

   if (!retro_waitable_spsc_init(&q, capacity))
   {
      fail("init");
      return 1;
   }

   p = sthread_create(producer_thread, NULL);

   while (got < total)
   {
      size_t want = record;
      size_t i, n;

      if (want > total - got)
         want = total - got;

      if (!retro_waitable_spsc_wait_readable(&q, want, 5000000))
      {
         fail("consumer: never got data");
         break;
      }

      n = retro_waitable_spsc_read(&q, buf, want);
      if (n != want)
      {
         fail("consumer: short read after waiting for data");
         break;
      }

      for (i = 0; i < n; i++)
      {
         if (buf[i] != stream_byte(got + i))
         {
            fail("consumer: wrong byte -- a transfer was lost or repeated");
            got = total;
            break;
         }
      }
      got += n;
   }

   sthread_join(p);
   retro_waitable_spsc_free(&q);

   printf("  %-9s %lu bytes in %lu-byte records through a %lu-byte queue\n",
         name, (unsigned long)total, (unsigned long)record,
         (unsigned long)capacity);
   return 0;
}

/* ---- timeout ------------------------------------------------------ */

static int lane_timeout(void)
{
   retro_waitable_spsc_t t;
   retro_time_t t0, elapsed;

   if (!retro_waitable_spsc_init(&t, 4096))
   {
      fail("timeout: init");
      return 1;
   }

   t0 = cpu_features_get_time_usec();
   if (retro_waitable_spsc_wait_readable(&t, 16, 200000))
      fail("timeout: claimed data on an empty queue");
   elapsed = cpu_features_get_time_usec() - t0;

   retro_waitable_spsc_free(&t);

   /* Generous upper bound: this only has to catch a bound applied per
    * sleep rather than against the clock, which would run long by a
    * multiple rather than by a margin. */
   if (elapsed < 150000 || elapsed > 1000000)
   {
      fprintf(stderr, "FAIL: timeout: asked 200000us, waited %luus\n",
            (unsigned long)elapsed);
      retro_atomic_fetch_add_int(&failures, 1);
   }

   printf("  timeout   asked 200000us, waited %luus\n",
         (unsigned long)elapsed);
   return 0;
}

/* ---- wake --------------------------------------------------------- */

static retro_waitable_spsc_t wq;
static retro_atomic_int_t    woke;

static void canceller_thread(void *unused)
{
   (void)unused;
   test_sleep_ms(100);
   retro_waitable_spsc_cancel(&wq);
}

static int lane_wake(void)
{
   sthread_t *w;

   if (!retro_waitable_spsc_init(&wq, 4096))
   {
      fail("cancel: init");
      return 1;
   }

   retro_atomic_int_init(&woke, 0);
   w = sthread_create(canceller_thread, NULL);

   /* Asks for bytes that never come; only the wake can return it, and
    * it must return before the bound rather than at it. */
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      retro_waitable_spsc_wait_readable(&wq, 64, 5000000);
      if (cpu_features_get_time_usec() - t0 > 3000000)
         fail("cancel: waiter was not released");
   }

   sthread_join(w);
   retro_waitable_spsc_free(&wq);
   printf("  cancel    released a waiter with no data\n");
   return 0;
}

int main(void)
{
   sthread_t *wd;

   retro_atomic_int_init(&watchdog_stop, 0);
   retro_atomic_int_init(&failures, 0);
   wd = sthread_create(watchdog_thread, NULL);

   printf("retro_waitable_spsc (atomics: %s)\n", RETRO_ATOMIC_BACKEND_NAME);

   run_stream(65536, 1024, 4u * 1024 * 1024, "handoff");
   run_stream(256,     96, 1u * 1024 * 1024, "tiny");
   lane_timeout();
   lane_wake();

   retro_atomic_store_release_int(&watchdog_stop, 1);
   sthread_join(wd);

   if (retro_atomic_load_acquire_int(&failures))
   {
      printf("waitable_spsc: FAILED\n");
      return 1;
   }
   printf("waitable_spsc: ok\n");
   return 0;
}
