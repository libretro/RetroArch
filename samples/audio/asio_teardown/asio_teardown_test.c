/* The ASIO teardown's wait for a callback that outlives ASIOStop, the
 * driver's code pulled out of audio/drivers/asio.c.
 *
 * Held to: the wait ends as the last callback leaves, not on a timer;
 * never while one is inside; and at 200 ms, with a warning, for a
 * callback that never leaves.
 *
 * Whether it ended as the callback left is told by how the wait woke,
 * not by the clock: a bound that runs out on a count already at zero
 * slept through the last callback's notify. A loaded host can be late
 * to wake the waiter without making it wrong.
 *
 * ASIO_TEST_NO_OUT_NOTIFY leaves the last callback out silent, and the
 * test must fail. */
#include <stdio.h>
#include <stdlib.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <features/features_cpu.h>

static retro_atomic_int_t warnings;
#define RARCH_WARN(...) retro_atomic_fetch_add_int(&warnings, 1)

/* The driver's bounded wait, watched: a bound that ran out with no
 * callback left inside is a notify the wait never got. */
static retro_atomic_int_t missed_notify;
static retro_atomic_int_t *watched_count;
static bool watched_commit_wait_timeout(retro_eventcount_t *ec, int key,
      int64_t timeout_us)
{
   bool woken = retro_eventcount_commit_wait_timeout(ec, key, timeout_us);
   if (!woken && watched_count
         && retro_atomic_load_acquire_int(watched_count) <= 0)
      retro_atomic_fetch_add_int(&missed_notify, 1);
   return woken;
}
#define retro_eventcount_commit_wait_timeout watched_commit_wait_timeout

#include "asio_teardown_driver.h"
#undef retro_eventcount_commit_wait_timeout

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
   exit(1); } } while (0)

static retro_atomic_int_t release_cb, in_cb, waited;
static retro_time_t       returned_at, wait_began;

/* asio_cb_buffer_switch's entry and exit, held in between. */
static void callback(void *data)
{
   (void)data;
   retro_atomic_fetch_add_int(&g_asio_in_callback, 1);
   retro_atomic_store_release_int(&in_cb, 1);
   while (!retro_atomic_load_acquire_int(&release_cb))
      retro_sleep(1);
   asio_callback_out();
}

static void teardown(void *data)
{
   (void)data;
   wait_began  = cpu_features_get_time_usec();
   asio_wait_callbacks_out();
   returned_at = cpu_features_get_time_usec();
   retro_atomic_store_release_int(&waited, 1);
}

/* Callbacks entering and leaving as a driver's would, two at a time
 * where one overlaps the next. */
static retro_atomic_int_t churn_stop;
static void churn(void *data)
{
   (void)data;
   while (!retro_atomic_load_acquire_int(&churn_stop))
   {
      retro_atomic_fetch_add_int(&g_asio_in_callback, 1);
      asio_callback_out();
   }
}

int main(void)
{
   retro_time_t start, released;
   sthread_t   *cb, *td, *ch;
   unsigned     i;

   retro_atomic_int_init(&g_asio_in_callback, 0);
   retro_atomic_int_init(&warnings, 0);
   retro_atomic_int_init(&release_cb, 0);
   retro_atomic_int_init(&in_cb, 0);
   retro_atomic_int_init(&waited, 0);
   retro_atomic_int_init(&churn_stop, 0);
   retro_atomic_int_init(&missed_notify, 0);
   CHECK(retro_eventcount_init(&g_asio_out));
   watched_count = &g_asio_in_callback;

   /* nothing inside: no wait */
   start = cpu_features_get_time_usec();
   asio_wait_callbacks_out();
   CHECK(cpu_features_get_time_usec() - start < 20000);
   printf("none inside: returned at once\n");

   /* one inside: the wait ends as it leaves */
   cb = sthread_create(callback, NULL);
   CHECK(cb);
   while (!retro_atomic_load_acquire_int(&in_cb))
      retro_sleep(1);
   td = sthread_create(teardown, NULL);
   CHECK(td);
   retro_sleep(30);
   CHECK(!retro_atomic_load_acquire_int(&waited));
   released = cpu_features_get_time_usec();
   retro_atomic_store_release_int(&release_cb, 1);
   sthread_join(cb);
   sthread_join(td);
   printf("one inside: wait ended %lld us after it left\n",
         (long long)(returned_at - released));
   /* ended by the callback's notify, not by the bound running out */
   CHECK(returned_at >= released);
   CHECK(!retro_atomic_load_acquire_int(&missed_notify));
   /* no warning, unless the host held the release back past the
    * deadline itself */
   if (released - wait_began < 190000)
      CHECK(!retro_atomic_load_acquire_int(&warnings));
   else
      retro_atomic_store_release_int(&warnings, 0);

   /* callbacks coming and going: every wait still ends */
   ch = sthread_create(churn, NULL);
   CHECK(ch);
   for (i = 0; i < 2000; i++)
      asio_wait_callbacks_out();
   retro_atomic_store_release_int(&churn_stop, 1);
   sthread_join(ch);
   CHECK(!retro_atomic_load_acquire_int(&missed_notify));
   printf("churn: %u warnings\n",
         (unsigned)retro_atomic_load_acquire_int(&warnings));
   CHECK(!retro_atomic_load_acquire_int(&warnings));

   /* one that never leaves: torn down at the deadline, and said so */
   retro_atomic_fetch_add_int(&g_asio_in_callback, 1);
   start = cpu_features_get_time_usec();
   asio_wait_callbacks_out();
   start = cpu_features_get_time_usec() - start;
   printf("stuck: gave up after %lld ms\n", (long long)(start / 1000));
   CHECK(start >= 190000 && start < 1000000);
   CHECK(retro_atomic_load_acquire_int(&warnings) == 1);

   (void)g_asio_out_ready;
   printf("PASS: asio teardown\n");
   return 0;
}
