/* Oracle for task ordering guards that ask "is another task still in
 * progress?" from inside a task handler.
 *
 * The movie-record task defers itself until a pending state load has
 * been applied.  That guard used to ask a queue finder, and on the
 * unthreaded scheduler it did not work: the record task started
 * first and the load landed afterwards, which an in-tree FIXME had
 * recorded as an unexplained scheduler oddity, worked around by
 * waiting for the entire task queue to drain.
 *
 * The explanation is in retro_task_regular_gather(): every running
 * task is lifted OFF tasks_running into a local list before any
 * handler is invoked, so for the whole of that pass the queue looks
 * empty to a finder - including a finder called from inside one of
 * those very handlers.  A sibling task is invisible exactly when a
 * handler most needs to see it.
 *
 * These lanes pin the behaviour of the queue that makes this true,
 * and the fix that follows from it:
 *
 *   blindness   - a finder run from inside a handler cannot see a
 *                 sibling task that is queued and unfinished.  This
 *                 lane documents the queue's actual contract; if it
 *                 ever changes, that is worth knowing deliberately.
 *   flag-works  - a plain main-thread flag, set at push and cleared
 *                 in the callback, IS visible from inside a handler,
 *                 which is why the guards now use one.
 *   ordering    - a task guarded by such a flag genuinely defers
 *                 until the task it waits on has run its callback,
 *                 which is the ordering the FIXME wanted.
 *
 * No threads: the unthreaded queue is driven explicitly, which is
 * also the configuration the bug appeared in.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <queues/task_queue.h>

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
/* A "load" task: takes a few invocations, then a callback applies    */
/* its result - the shape of a state load.                            */
/* ------------------------------------------------------------------ */

static int      load_ticks_remaining;
static bool     load_applied;
static bool     load_pending;      /* the main-thread flag */
static unsigned order_counter;
static unsigned load_applied_at;
static unsigned dependent_ran_at;

static void load_handler(retro_task_t *task)
{
   if (load_ticks_remaining > 0)
   {
      load_ticks_remaining--;
      return;
   }
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void load_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   load_applied    = true;
   load_pending    = false;
   load_applied_at = ++order_counter;
}

/* ------------------------------------------------------------------ */
/* Observations made from inside a dependent handler                  */
/* ------------------------------------------------------------------ */

static bool finder_missed_sibling;
static bool flag_saw_sibling;
static bool dependent_done;

static bool load_finder(retro_task_t *task, void *user_data)
{
   return (task && task->handler == load_handler);
}

static bool finder_says_in_progress(void)
{
   task_finder_data_t find_data;
   find_data.func     = load_finder;
   find_data.userdata = NULL;
   return task_queue_find(&find_data);
}

/* Records what each mechanism reports from inside a handler, then -
 * like the real record task - defers itself while the flag says a
 * load is pending. */
static void dependent_handler(retro_task_t *task)
{
   /* Both mechanisms are asked the same question at the same
    * moment: "is the load still pending?"  The flag is the truth,
    * so any invocation where the finder disagrees with it is a
    * moment the old guard would have got wrong. */
   if (load_pending && !finder_says_in_progress())
      finder_missed_sibling = true;
   if (load_pending)
      flag_saw_sibling = true;

   /* The guard the record task now uses. */
   if (load_pending)
      return;

   dependent_ran_at = ++order_counter;
   dependent_done   = true;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void dependent_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
}

/* ------------------------------------------------------------------ */

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

static void push(void (*handler)(retro_task_t*),
      retro_task_callback_t cb)
{
   retro_task_t *task = task_init();
   if (!task)
   {
      CHECK(false, "task_init failed");
      return;
   }
   task->handler  = handler;
   task->callback = cb;
   task_queue_push(task);
}

static void reset_all(void)
{
   load_ticks_remaining = 3;
   load_applied         = false;
   load_pending         = false;
   order_counter        = 0;
   load_applied_at      = 0;
   dependent_ran_at     = 0;
   finder_missed_sibling = false;
   flag_saw_sibling     = false;
   dependent_done       = false;
}

static void run_both(void)
{
   unsigned ticks = 0;

   task_queue_init(false, NULL);
   reset_all();

   /* Push the load first, exactly as the real sequence does, and set
    * the flag at push time the way the real push site does. */
   load_pending = true;
   push(load_handler, load_cb);
   push(dependent_handler, dependent_cb);

   while (queue_busy() && ticks < 1000)
   {
      task_queue_check();
      ticks++;
   }

   task_queue_deinit();
}

/* ------------------------------------------------------------------ */
/* Lanes                                                              */
/* ------------------------------------------------------------------ */

static void lane_finder_is_unreliable_inside_a_handler(void)
{
   unsigned had = failures;

   run_both();

   CHECK(load_applied, "the load task never completed");

   /* The finder is not uniformly blind - it is worse than that, it
    * is position dependent.  retro_task_regular_gather() lifts every
    * running task off the queue and then puts each one BACK as it
    * processes it, so whether a handler's finder sees a sibling
    * depends on whether that sibling has been processed yet in this
    * pass.  At least one invocation therefore reports "no load in
    * progress" while the load is genuinely pending, and that is all
    * an ordering guard needs to get wrong once. */
   CHECK(finder_missed_sibling,
         "the finder agreed with the flag on every invocation; if the "
         "gather's put-back behaviour has changed, guards written "
         "against it - and this lane - need revisiting");

   if (failures == had)
      fprintf(stderr, "[pass] finder-unreliability lane\n");
}

static void lane_flag_is_visible(void)
{
   unsigned had = failures;

   run_both();

   CHECK(flag_saw_sibling,
         "a main-thread flag was NOT visible from inside a handler; "
         "the guards depend on it being so");

   if (failures == had)
      fprintf(stderr, "[pass] flag-visibility lane\n");
}

static void lane_ordering_holds(void)
{
   unsigned had = failures;

   run_both();

   CHECK(dependent_done, "the dependent task never ran");
   CHECK(load_applied_at > 0 && dependent_ran_at > 0,
         "ordering was not recorded");
   CHECK(load_applied_at < dependent_ran_at,
         "the dependent task ran at %u, before the load was applied "
         "at %u - this is the ordering bug the whole-queue wait was "
         "papering over", dependent_ran_at, load_applied_at);

   if (failures == had)
      fprintf(stderr, "[pass] ordering lane\n");
}

int main(void)
{
   lane_finder_is_unreliable_inside_a_handler();
   lane_flag_is_visible();
   lane_ordering_holds();

   if (failures)
   {
      fprintf(stderr, "FAIL task_ordering_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS task_ordering_test\n");
   return 0;
}
