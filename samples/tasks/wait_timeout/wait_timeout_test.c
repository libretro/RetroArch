/* Oracle for task_queue_wait_timeout().
 *
 * task_queue_wait() waits indefinitely, which is correct for work
 * that is certain to finish and wrong for anything depending on
 * something outside the machine.  The netplay MITM query is the
 * motivating case: it waits on an HTTP round trip to the lobby
 * server, and a server that never answers hangs the frontend with no
 * way out.
 *
 * The bound is implemented by wrapping the caller's condition in one
 * that also goes false at a deadline, so each queue implementation -
 * regular, threaded, GCD - keeps its own waiting behaviour and no
 * platform-specific code is involved.  What has to hold:
 *
 *   completes  - a wait whose work finishes returns as soon as it
 *                does, reports success, and does NOT sit out the
 *                remaining timeout.
 *   times out  - a wait whose condition never clears returns anyway,
 *                reports failure, and takes about the timeout rather
 *                than forever.
 *   no-op      - a condition already false returns immediately.
 *   threaded   - the same holds on the threaded queue, where the
 *                waiting is a different implementation.
 *
 * The timing assertions are deliberately loose at the top end: this
 * runs on shared CI machines and the point is "bounded, not
 * indefinite", not a precise duration.  The lower bounds are tight,
 * because returning EARLY would silently break the caller.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <queues/task_queue.h>
#include <features/features_cpu.h>
#include <retro_timers.h>

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

#define TIMEOUT_USEC 300000   /* 0.3s - long enough to measure */

/* ------------------------------------------------------------------ */
/* A task that finishes after N ticks, and one that never finishes    */
/* ------------------------------------------------------------------ */

static int  ticks_remaining;
static bool work_pending;

/* Handlers run on the WORKER thread under the threaded queue, so
 * nothing here touches state the waiting thread reads.  The flag is
 * cleared in the callback below - which is where the code being
 * modelled clears it, netplay_mitm_query_cb(), and callbacks run on
 * the thread pumping the queue.
 *
 * Clearing it here was both a data race and a misrepresentation of
 * the thing under test. */
static void finite_handler(retro_task_t *task)
{
   if (ticks_remaining > 0)
   {
      ticks_remaining--;
      return;
   }
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void endless_handler(retro_task_t *task)
{
   /* Models a network wait: the task stays in the queue, and the
    * thing the caller is waiting for never arrives. */
}

/* Runs on the thread pumping the queue - the waiting thread - just
 * as the netplay query callback does. */
static void cb(retro_task_t *task, void *task_data, void *user_data,
      const char *error)
{
   work_pending = false;
}

/* The caller's own condition, as a netplay-style "is it still
 * pending" flag rather than a queue lookup. */
static bool still_pending(void *data)
{
   return work_pending;
}

static bool push(void (*handler)(retro_task_t*))
{
   retro_task_t *task = task_init();
   if (!task)
      return false;
   task->handler  = handler;
   task->callback = cb;
   return task_queue_push(task);
}

/* ------------------------------------------------------------------ */
/* Lanes                                                               */
/* ------------------------------------------------------------------ */

static void lane_completes_before_timeout(bool threaded)
{
   unsigned had = failures;
   retro_time_t started, elapsed;
   bool ok;

   task_queue_init(threaded, NULL);
   ticks_remaining = 3;
   work_pending    = true;

   CHECK(push(finite_handler), "push failed");

   started = cpu_features_get_time_usec();
   ok      = task_queue_wait_timeout(still_pending, NULL, TIMEOUT_USEC);
   elapsed = cpu_features_get_time_usec() - started;

   task_queue_deinit();

   CHECK(ok, "%s: reported failure for work that finished",
         threaded ? "threaded" : "regular");
   CHECK(!work_pending, "%s: work did not actually finish",
         threaded ? "threaded" : "regular");
   /* The point of the bound is that it does not delay the ordinary
    * case; sitting out the full timeout would be a regression that
    * still "passes" a pass/fail check. */
   CHECK(elapsed < (TIMEOUT_USEC / 2),
         "%s: took %lldus for work that finished immediately - the "
         "wait is sitting out its timeout instead of returning",
         threaded ? "threaded" : "regular", (long long)elapsed);

   if (failures == had)
      fprintf(stderr, "[pass] completes-before-timeout (%s, %lldus)\n",
            threaded ? "threaded" : "regular", (long long)elapsed);
}

static void lane_times_out(bool threaded)
{
   unsigned had = failures;
   retro_time_t started, elapsed;
   bool ok;

   task_queue_init(threaded, NULL);
   work_pending = true;

   CHECK(push(endless_handler), "push failed");

   started = cpu_features_get_time_usec();
   ok      = task_queue_wait_timeout(still_pending, NULL, TIMEOUT_USEC);
   elapsed = cpu_features_get_time_usec() - started;

   /* Abandon the endless task so deinit is not left holding it. */
   task_queue_reset();
   task_queue_check();
   task_queue_deinit();

   CHECK(!ok, "%s: reported success though the work never finished",
         threaded ? "threaded" : "regular");
   /* Tight lower bound: giving up early would cut short a round trip
    * that was about to succeed. */
   CHECK(elapsed >= (TIMEOUT_USEC - (TIMEOUT_USEC / 10)),
         "%s: gave up after %lldus, well before the %dus timeout",
         threaded ? "threaded" : "regular", (long long)elapsed,
         TIMEOUT_USEC);
   /* Loose upper bound: the machine is shared, the claim is only
    * that this terminates rather than hangs. */
   CHECK(elapsed < (TIMEOUT_USEC * 10),
         "%s: took %lldus against a %dus timeout - not bounded",
         threaded ? "threaded" : "regular", (long long)elapsed,
         TIMEOUT_USEC);

   if (failures == had)
      fprintf(stderr, "[pass] times-out (%s, %lldus)\n",
            threaded ? "threaded" : "regular", (long long)elapsed);
}

static void lane_already_done(void)
{
   unsigned had = failures;
   retro_time_t started, elapsed;
   bool ok;

   task_queue_init(false, NULL);
   work_pending = false;

   started = cpu_features_get_time_usec();
   ok      = task_queue_wait_timeout(still_pending, NULL, TIMEOUT_USEC);
   elapsed = cpu_features_get_time_usec() - started;

   task_queue_deinit();

   CHECK(ok, "reported failure for a condition already false");
   CHECK(elapsed < (TIMEOUT_USEC / 2),
         "waited %lldus for something already finished",
         (long long)elapsed);

   if (failures == had)
      fprintf(stderr, "[pass] already-done lane\n");
}

int main(void)
{
   lane_completes_before_timeout(false);
   lane_times_out(false);
   lane_already_done();

   lane_completes_before_timeout(true);
   lane_times_out(true);

   if (failures)
   {
      fprintf(stderr, "FAIL wait_timeout_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS wait_timeout_test\n");
   return 0;
}
