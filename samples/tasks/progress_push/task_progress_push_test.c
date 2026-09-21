/* task_queue_push_progress()'s contract with the frontend's msg_push.
 *
 * The message a task's progress produces is built from the task's title,
 * progress and flags, which a worker thread is free to change - and a
 * worker replacing the title frees the old buffer as it goes (the
 * documented free-then-set pattern). So the message is formatted under
 * the queue's property lock, and the push that carries
 * it to the frontend runs with that lock released, because the frontend's
 * message path inserts into the widget queue and, with accessibility
 * enabled, forks to speak. Parking a worker's task_set_progress() behind
 * a fork is what this contract exists to prevent.
 *
 * The contract has two halves, and this checks both:
 *
 *   1. msg_push() is NOT called with the property lock held. Checked
 *      directly rather than by inspection: msg_push() calls
 *      task_get_progress(), a public getter that takes that same
 *      non-recursive lock. If the push were still made under it, that
 *      call self-deadlocks, and the watchdog below names it. Passing
 *      means the lock was not held.
 *
 *   2. msg_push() gets everything it needs in the message and reads no
 *      property the lock guards. The handler here churns the title on
 *      the worker thread for the whole run - strdup in, free out - so a
 *      push that reached for task->title instead of the string it was
 *      handed is a use-after-free for ASan and a race for TSan, against
 *      a title that is being freed continuously rather than once.
 *
 * Build it with -DPROGRESS_PUSH_DEFECT to have msg_push() read
 * task->title and see the second half fail; the first half fails if
 * push_progress() is changed to hold the lock across the push.
 *
 *   make && ./task_progress_push_test
 *   make SANITIZER=thread && ./task_progress_push_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <features/features_cpu.h>
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

/* How long the worker churns the title for, and how often it changes. */
#define RUN_US        3000000
#define TITLE_EVERY   3

static retro_atomic_int_t pushes;     /* msg_push() calls seen */
static retro_atomic_int_t progress_ok;/* getter calls that returned */
static retro_atomic_int_t worker_done;
static retro_atomic_int_t titles_set;
static retro_atomic_int_t watchdog_stop;
static const char        *phase = "startup";

/* ---------------------------- watchdog ---------------------------- */

#define WATCHDOG_POLL_US 250000
#define WATCHDOG_STALL   40

static void watchdog_loop(void *data)
{
   int last   = -1;
   int stalls = 0;
   (void)data;

   while (!retro_atomic_load_acquire_int(&watchdog_stop))
   {
      int now;
      retro_sleep_us(WATCHDOG_POLL_US);
      /* Any of the three moving counts as progress. */
      now = retro_atomic_load_acquire_int(&pushes)
          + retro_atomic_load_acquire_int(&titles_set)
          + retro_atomic_load_acquire_int(&worker_done);

      if (now != last)
      {
         last   = now;
         stalls = 0;
         continue;
      }

      if (++stalls >= WATCHDOG_STALL)
      {
         fprintf(stderr,
               "FAIL watchdog: stalled %d s in phase \"%s\" - msg_push() "
               "was called with the property lock held, so the getter "
               "inside it deadlocked on the same lock\n",
               (int)((WATCHDOG_POLL_US / 1000000.0) * WATCHDOG_STALL),
               phase);
         fflush(stderr);
         abort();
      }
   }
}

/* --------------------------- the frontend -------------------------- */

static void test_msg_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)prio; (void)duration; (void)flush;

   CHECK(msg != NULL, "msg_push got a NULL message");
   if (msg)
      CHECK(*msg != '\0', "msg_push got an empty message");

   /* Half one of the contract. Takes the property lock; returns only
    * because the push is not made under it. */
   (void)task_get_progress(task);
   retro_atomic_fetch_add_int(&progress_ok, 1);

#ifdef PROGRESS_PUSH_DEFECT
   /* What the contract forbids: reaching for a property the lock
    * guards instead of using the message. The worker is freeing and
    * replacing this pointer throughout the run. */
   {
      const char *t = task->title;
      if (t)
         CHECK(strlen(t) < 4096, "title implausibly long");
   }
#endif

   retro_atomic_fetch_add_int(&pushes, 1);
}

/* ---------------------------- the worker --------------------------- */

static void churn_handler(retro_task_t *task)
{
   retro_time_t deadline = cpu_features_get_time_usec() + RUN_US;
   unsigned     n        = 0;
   char         buf[64];

   while (cpu_features_get_time_usec() < deadline)
   {
      /* Progress moves every iteration so every gather has a message
       * to build, and the title is replaced often enough that a push
       * reading it directly lands on a freed buffer. */
      task_set_progress(task, (int8_t)(n % 101));

      if ((n % TITLE_EVERY) == 0)
      {
         snprintf(buf, sizeof(buf), "churning title %u", n);
         /* task_set_title() does not free the previous title - the
          * documented pattern is free-then-set, and it is the free that
          * makes a push reading task->title a use-after-free. */
         task_free_title(task);
         task_set_title(task, strdup(buf));
         retro_atomic_fetch_add_int(&titles_set, 1);
      }

      n++;
      retro_sleep_us(10);
   }

   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   retro_atomic_store_release_int(&worker_done, 1);
}

int main(void)
{
   sthread_t    *watchdog = NULL;
   retro_task_t *task     = NULL;
   retro_time_t  deadline;

   retro_atomic_store_release_int(&pushes,        0);
   retro_atomic_store_release_int(&progress_ok,   0);
   retro_atomic_store_release_int(&worker_done,   0);
   retro_atomic_store_release_int(&titles_set,    0);
   retro_atomic_store_release_int(&watchdog_stop, 0);

   watchdog = sthread_create(watchdog_loop, NULL);

   task_queue_init(true, test_msg_push);
   if (!task_queue_is_threaded())
   {
      fprintf(stderr, "SKIP task_progress_push: queue is not threaded\n");
      retro_atomic_store_release_int(&watchdog_stop, 1);
      sthread_join(watchdog);
      task_queue_deinit();
      return 0;
   }

   if (!(task = task_init()))
   {
      fprintf(stderr, "FAIL could not allocate a task\n");
      retro_atomic_store_release_int(&watchdog_stop, 1);
      sthread_join(watchdog);
      task_queue_deinit();
      return 1;
   }

   task->handler = churn_handler;
   task->title   = strdup("initial title");
   task_queue_push(task);

   /* The main thread's side: gather every pass, as the runloop does. */
   phase    = "churning";
   deadline = cpu_features_get_time_usec() + RUN_US + 2000000;

   while (     !retro_atomic_load_acquire_int(&worker_done)
            && cpu_features_get_time_usec() < deadline)
   {
      task_queue_check();
      retro_sleep_us(500);
   }

   phase = "draining";
   /* Retire it, so the finished-path push runs too. */
   deadline = cpu_features_get_time_usec() + 2000000;
   while (cpu_features_get_time_usec() < deadline)
   {
      task_queue_check();
      retro_sleep_us(1000);
   }

   phase = "deinit";
   task_queue_deinit();

   retro_atomic_store_release_int(&watchdog_stop, 1);
   sthread_join(watchdog);

   CHECK(retro_atomic_load_acquire_int(&worker_done) == 1,
         "the worker never finished");
   CHECK(retro_atomic_load_acquire_int(&pushes) > 0,
         "msg_push was never called - the lane proved nothing");
   CHECK(retro_atomic_load_acquire_int(&titles_set) > 1,
         "the title was replaced %d time(s); the churn is what makes a "
         "direct read land on freed memory",
         retro_atomic_load_acquire_int(&titles_set));
   CHECK(retro_atomic_load_acquire_int(&progress_ok)
         == retro_atomic_load_acquire_int(&pushes),
         "the getter inside msg_push returned %d times for %d pushes",
         retro_atomic_load_acquire_int(&progress_ok),
         retro_atomic_load_acquire_int(&pushes));

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }

   printf("task_progress_push: all lanes passed (%d pushes, %d titles)\n",
         retro_atomic_load_acquire_int(&pushes),
         retro_atomic_load_acquire_int(&titles_set));
   return 0;
}
