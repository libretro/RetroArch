/* Oracle for the task queue's slow-handler watchdog
 * (task_queue_set_slow_handler_cb), compiled against the shipping
 * libretro-common/queues/task_queue.c.
 *
 * With Threaded Tasks off, task handlers run on the thread that also
 * drives the frame loop, so a handler that does not return promptly
 * is a visible stall - and the queue is the only place that can
 * attribute one to a specific task rather than to "something in the
 * frame".  The watchdog exists so that a regression in a budgeted
 * handler is reported by the build rather than found by a user.
 *
 * What these lanes pin:
 *
 *   quiet     - a handler that returns within its budget produces no
 *               report, however many times it runs.
 *   fires     - a handler that overruns produces one, attributed to
 *               that task, with a duration at least as long as the
 *               overrun.
 *   threaded  - nothing is measured on the threaded queue, where a
 *               long handler is the entire point of having a worker.
 *   opt-out   - unregistering stops reporting, and is the default:
 *               an unregistered watchdog must never fire.
 *
 * The handler durations here are produced by sleeping, so the lanes
 * assert only "at least the budget" rather than exact values - the
 * scheduler decides the rest.  No shared state between the test and
 * a worker thread, so the sweep is ASan+UBSan+LSan.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <queues/task_queue.h>
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

/* ------------------------------------------------------------------ */

#define BUDGET_USEC 20000   /* 20 ms */

static unsigned     reports;
static retro_time_t last_usec;
static retro_task_handler_t last_handler;

static void slow_cb(retro_task_t *task, retro_time_t usec)
{
   reports++;
   last_usec    = usec;
   last_handler = task ? task->handler : NULL;
}

/* A handler that finishes immediately. */
static void fast_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* A handler that overruns the budget by a comfortable margin, so the
 * lane does not depend on scheduler precision. */
static void slow_handler(retro_task_t *task)
{
   retro_sleep(80);   /* ms */
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void push(void (*handler)(retro_task_t*))
{
   retro_task_t *task = task_init();
   if (!task)
   {
      CHECK(false, "task_init failed");
      return;
   }
   task->handler = handler;
   task_queue_push(task);
}

static bool any_finder(retro_task_t *task, void *userdata)
{
   return true;
}

static bool queue_busy(void)
{
   task_finder_data_t find_data;
   find_data.func     = any_finder;
   find_data.userdata = NULL;
   return task_queue_find(&find_data);
}

static void drain(unsigned max_ticks)
{
   unsigned i;
   for (i = 0; i < max_ticks; i++)
   {
      task_queue_check();
      if (!queue_busy())
         break;
      retro_sleep(1);
   }
}

static void reset_counters(void)
{
   reports      = 0;
   last_usec    = 0;
   last_handler = NULL;
}

/* ------------------------------------------------------------------ */

static void lane_fast_handler_is_quiet(void)
{
   unsigned had = failures;
   unsigned i;

   task_queue_init(false, NULL);
   task_queue_set_slow_handler_cb(slow_cb, BUDGET_USEC);
   reset_counters();

   for (i = 0; i < 8; i++)
   {
      push(fast_handler);
      drain(64);
   }

   CHECK(reports == 0,
         "a handler inside its budget produced %u reports", reports);

   task_queue_set_slow_handler_cb(NULL, 0);
   task_queue_deinit();

   if (failures == had)
      fprintf(stderr, "[pass] fast-handler quiet lane\n");
}

static void lane_slow_handler_reports(void)
{
   unsigned had = failures;

   task_queue_init(false, NULL);
   task_queue_set_slow_handler_cb(slow_cb, BUDGET_USEC);
   reset_counters();

   push(slow_handler);
   drain(256);

   CHECK(reports == 1, "expected exactly one report, got %u", reports);
   CHECK(last_handler == slow_handler,
         "report was not attributed to the slow handler");
   CHECK(last_usec > BUDGET_USEC,
         "reported duration %d us is not above the %d us budget",
         (int)last_usec, (int)BUDGET_USEC);

   task_queue_set_slow_handler_cb(NULL, 0);
   task_queue_deinit();

   if (failures == had)
      fprintf(stderr, "[pass] slow-handler report lane (%d ms)\n",
            (int)(last_usec / 1000));
}

static void lane_threaded_queue_is_not_measured(void)
{
   unsigned had = failures;

   /* On the threaded queue the handler runs on a worker, where a
    * long handler is exactly what the worker is for. */
   task_queue_init(true, NULL);
   task_queue_set_slow_handler_cb(slow_cb, BUDGET_USEC);
   reset_counters();

   push(slow_handler);
   drain(512);

   CHECK(reports == 0,
         "the threaded queue produced %u reports; worker handlers "
         "are not main-thread stalls", reports);

   task_queue_set_slow_handler_cb(NULL, 0);
   task_queue_deinit();

   if (failures == had)
      fprintf(stderr, "[pass] threaded-queue lane\n");
}

static void lane_unregistered_is_silent(void)
{
   unsigned had = failures;

   task_queue_init(false, NULL);
   /* Never registered: the default state. */
   reset_counters();

   push(slow_handler);
   drain(256);

   CHECK(reports == 0,
         "an unregistered watchdog fired %u times", reports);

   /* Registering and then unregistering must also go quiet. */
   task_queue_set_slow_handler_cb(slow_cb, BUDGET_USEC);
   task_queue_set_slow_handler_cb(NULL, 0);
   push(slow_handler);
   drain(256);

   CHECK(reports == 0, "unregistering did not stop reporting");

   task_queue_deinit();

   if (failures == had)
      fprintf(stderr, "[pass] opt-out lane\n");
}

int main(void)
{
   lane_fast_handler_is_quiet();
   lane_slow_handler_reports();
   lane_threaded_queue_is_not_measured();
   lane_unregistered_is_silent();

   if (failures)
   {
      fprintf(stderr, "FAIL task_watchdog_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS task_watchdog_test\n");
   return 0;
}
