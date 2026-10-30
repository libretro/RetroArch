/* Which tasks the threaded runner lets run at the same time, on its own
 * workers and, built with GCD=1 or GCD=standin, on GCD:
 *  - plain tasks never overlap one another, and run in the order they
 *    were pushed, each pass of one coming back behind the others;
 *  - parallel tasks overlap one another;
 *  - on GCD, plain tasks go on while every parallel task is held (the
 *    one worker takes parallel tasks too, so it may hold one);
 *  - a task pushed for later runs no earlier, while the others go on;
 *  - pushes from several threads at once all run, with each push
 *    stopped now and then between taking its place in the ring and
 *    filling it, so that a later one is in before it.
 */

#include <stdio.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <features/features_cpu.h>
#include <queues/task_queue.h>

#define LIMIT_US 5000000

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

static retro_atomic_int_t retired;

static void count_retired(retro_task_t *task, void *td, void *ud,
      const char *err)
{
   (void)task;
   (void)td;
   (void)ud;
   (void)err;
   retro_atomic_inc_int(&retired);
}

static bool retire_until(int n)
{
   retro_time_t until = cpu_features_get_time_usec() + LIMIT_US;
   while (retro_atomic_load_acquire_int(&retired) < n)
   {
      if (cpu_features_get_time_usec() > until)
         return false;
      task_queue_check();
      retro_sleep(1);
   }
   return true;
}

static retro_task_t *push(retro_task_handler_t handler, void *state,
      int flags)
{
   retro_task_t *task = task_init();
   task->handler      = handler;
   task->callback     = count_retired;
   task->state        = state;
   task->flags       |= RETRO_TASK_FLG_MUTE | flags;
   task_queue_push(task);
   return task;
}

/* --- plain tasks, one at a time and in turn --- */
#define PLAIN_TASKS  6
#define PLAIN_PASSES 4

static retro_atomic_int_t plain_inside;
static retro_atomic_int_t plain_overlaps;
static retro_atomic_int_t plain_seq;
static retro_atomic_int_t plain_go;
static int                plain_order[PLAIN_TASKS * PLAIN_PASSES];

static void plain_handler(retro_task_t *task)
{
   int id   = (int)(intptr_t)task->state;
   int pass = task_get_progress(task);
   int at;
   retro_time_t until = cpu_features_get_time_usec() + LIMIT_US;

   /* until every one is pushed */
   while (     !retro_atomic_load_acquire_int(&plain_go)
         && cpu_features_get_time_usec() < until)
      retro_sleep(1);

   if (retro_atomic_fetch_add_int(&plain_inside, 1))
      retro_atomic_inc_int(&plain_overlaps);
   retro_sleep(1);
   at = retro_atomic_fetch_add_int(&plain_seq, 1);
   if (at < PLAIN_TASKS * PLAIN_PASSES)
      plain_order[at] = id;
   retro_atomic_fetch_add_int(&plain_inside, -1);

   task_set_progress(task, (int8_t)(pass + 1));
   if (pass + 1 == PLAIN_PASSES)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void lane_plain(void)
{
   int  i;
   bool in_turn = true;

   retro_atomic_int_init(&retired, 0);
   retro_atomic_int_init(&plain_inside, 0);
   retro_atomic_int_init(&plain_overlaps, 0);
   retro_atomic_int_init(&plain_seq, 0);
   retro_atomic_int_init(&plain_go, 0);
   for (i = 0; i < PLAIN_TASKS; i++)
      push(plain_handler, (void*)(intptr_t)i, 0);
   retro_atomic_store_release_int(&plain_go, 1);
   check(retire_until(PLAIN_TASKS), "every plain task finishes");
   check(!retro_atomic_load_acquire_int(&plain_overlaps),
         "no two plain tasks are in their handlers at once");
   for (i = 0; i < PLAIN_TASKS * PLAIN_PASSES; i++)
      if (plain_order[i] != i % PLAIN_TASKS)
         in_turn = false;
   check(in_turn, "plain passes run in push order, each behind the rest");
}

/* --- parallel tasks meet, and plain ones go on beside them --- */
#define MEET 4

static retro_atomic_int_t meet_inside;
static retro_atomic_int_t meet_most;
static retro_atomic_int_t meet_release;

static void meet_handler(retro_task_t *task)
{
   int now  = retro_atomic_fetch_add_int(&meet_inside, 1) + 1;
   int most = retro_atomic_load_acquire_int(&meet_most);
   retro_time_t until = cpu_features_get_time_usec() + LIMIT_US;
   (void)task;
   while (now > most && !retro_atomic_cas_int(&meet_most, most, now))
      most = retro_atomic_load_acquire_int(&meet_most);
   while (     !retro_atomic_load_acquire_int(&meet_release)
         && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   retro_atomic_fetch_add_int(&meet_inside, -1);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void quick_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void lane_parallel(void)
{
   retro_time_t until;
   int          i;

   retro_atomic_int_init(&retired, 0);
   retro_atomic_int_init(&meet_inside, 0);
   retro_atomic_int_init(&meet_most, 0);
   retro_atomic_int_init(&meet_release, 0);
   for (i = 0; i < MEET; i++)
      push(meet_handler, NULL, RETRO_TASK_FLG_PARALLEL);

   until = cpu_features_get_time_usec() + LIMIT_US;
   while (     retro_atomic_load_acquire_int(&meet_most) < MEET
         && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   check(retro_atomic_load_acquire_int(&meet_most) == MEET,
         "four parallel tasks are in their handlers at once");

   for (i = 0; i < 3; i++)
      push(quick_handler, NULL, 0);
#ifdef HAVE_GCD
   check(retire_until(3), "plain tasks finish while the parallel ones wait");
#endif

   retro_atomic_store_release_int(&meet_release, 1);
   check(retire_until(3 + MEET), "every task finishes once they are let go");
}

/* --- a task for later --- */
#define LATER_US 100000

static retro_atomic_int_t later_ran_at;

static void later_handler(retro_task_t *task)
{
   retro_atomic_store_release_int(&later_ran_at,
         (int)(cpu_features_get_time_usec() - (retro_time_t)task->state));
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void lane_later(void)
{
   retro_task_t *task = task_init();
   retro_time_t  now  = cpu_features_get_time_usec();

   retro_atomic_int_init(&retired, 0);
   retro_atomic_int_init(&later_ran_at, -1);
   task->handler  = later_handler;
   task->callback = count_retired;
   task->state    = (void*)(intptr_t)now;
   task->when     = now + LATER_US;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
   push(quick_handler, NULL, 0);
   check(retire_until(1) && retro_atomic_load_acquire_int(&later_ran_at) < 0,
         "a task due now runs while one for later waits");
   check(retire_until(2), "the task for later runs");
   /* the keeper wakes half a millisecond early */
   check(retro_atomic_load_acquire_int(&later_ran_at) >= LATER_US - 1000,
         "and not before its time");
}

/* --- pushes from several threads, some stopped half way --- */
#define PUSHERS        4
#define PUSHES_EACH    250

static retro_atomic_int_t hook_on;
static retro_atomic_int_t hook_count;

/* Called by the ring between a push taking its place and filling it. */
void overlap_ring_hook(void);
void overlap_ring_hook(void)
{
   if (     retro_atomic_load_acquire_int(&hook_on)
         && !(retro_atomic_fetch_add_int(&hook_count, 1) & 7))
      retro_sleep(1);
}

static void pusher(void *data)
{
   int i;
   (void)data;
   for (i = 0; i < PUSHES_EACH; i++)
      push(quick_handler, NULL, (i & 1) ? RETRO_TASK_FLG_PARALLEL : 0);
}

static void lane_pushers(void)
{
   sthread_t *th[PUSHERS];
   int        i;

   retro_atomic_int_init(&retired, 0);
   retro_atomic_int_init(&hook_count, 0);
   retro_atomic_store_release_int(&hook_on, 1);
   for (i = 0; i < PUSHERS; i++)
      th[i] = sthread_create(pusher, NULL);
   for (i = 0; i < PUSHES_EACH; i++)
      push(quick_handler, NULL, (i & 1) ? RETRO_TASK_FLG_PARALLEL : 0);
   for (i = 0; i < PUSHERS; i++)
      if (th[i])
         sthread_join(th[i]);
   retro_atomic_store_release_int(&hook_on, 0);
   check(retire_until((PUSHERS + 1) * PUSHES_EACH),
         "every task pushed from five threads at once runs");
}

int main(void)
{
   task_queue_set_parallel_workers(MEET - 1);
   task_queue_init(true, NULL);
   check(task_queue_is_threaded(), "the queue is threaded");

   printf("plain tasks\n");
   lane_plain();
   printf("parallel tasks\n");
   lane_parallel();
   printf("a task for later\n");
   lane_later();
   printf("pushes from several threads\n");
   lane_pushers();

   task_queue_deinit();
   task_queue_set_parallel_workers(0);

   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
