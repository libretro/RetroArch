/* RETRO_TASK_FLG_PARALLEL: the threaded runner's extra workers take
 * the tasks marked so, beside the one worker every other task runs on.
 *
 * Lanes, on the threaded runner, with the number of extra workers set
 * by task_queue_set_parallel_workers():
 *  - four parallel tasks are in their handlers at the same time with
 *    three extra workers beside the one;
 *  - plain tasks never overlap one another while parallel tasks run
 *    beside them, and never run off the one worker;
 *  - a parallel task returning unfinished is never in two handlers at
 *    once;
 *  - a parallel task that is also detachable runs on an extra worker
 *    while the one is busy;
 *  - with none asked for there are no extra workers and every parallel
 *    task still runs, on the one worker;
 *  - a deinit with parallel tasks queued stops every worker, and the
 *    next init starts the extra workers again;
 *  - a task names the extra workers it needs: its push starts those
 *    missing, up to eight, and the pool never shrinks until deinit;
 *  - deinit leaves an extra worker stuck in a detachable handler behind
 *    after its bound, still waits for one in any other handler, and
 *    leaves the one worker and two extras behind together, against one
 *    bound; the last of them out frees the queue's primitives, once;
 *  - a task left behind is cleaned up once, by its worker, after its
 *    handler returns; one not left behind, at its retirement;
 *  - a cleanup that begins while deinit still waits for another worker
 *    keeps property_lock, and a worker left behind stops counting as
 *    one of the queue's.
 * On the unthreaded runner the flag changes nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

/* compiled in, so the lanes can see how many extra workers started */
#include "../../../queues/task_queue.c"

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

/* ---- concurrency ---- */
static retro_atomic_int_t inside;      /* parallel handlers running now */
static retro_atomic_int_t most;        /* the most seen at once */
static retro_atomic_int_t serial_in;   /* plain handlers running now */
static retro_atomic_int_t serial_most;
static retro_atomic_int_t retired;
static uintptr_t          worker0;     /* where the plain tasks run */
static retro_atomic_int_t off_worker0; /* plain handlers seen elsewhere */
static unsigned           gather_goal; /* how many must meet at once */

struct job
{
   retro_atomic_int_t in;      /* this task's handlers running now */
   retro_atomic_int_t twice;   /* seen in two at once */
   unsigned           calls;
   unsigned           finish_after;
   uintptr_t          thread;
};

static void note_most(retro_atomic_int_t *now, retro_atomic_int_t *max)
{
   int n = retro_atomic_fetch_add_int(now, 1) + 1;
   for (;;)
   {
      int m = retro_atomic_load_acquire_int(max);
      if (n <= m || retro_atomic_cas_int(max, m, n))
         break;
   }
}

/* Waits, with a bound, for gather_goal parallel handlers to be inside
 * at once, then leaves: four of them meeting proves four workers. */
static void parallel_handler(retro_task_t *task)
{
   struct job *j = (struct job*)task->user_data;
   retro_time_t deadline = cpu_features_get_time_usec() + 3000000;

   if (retro_atomic_fetch_add_int(&j->in, 1))
      retro_atomic_store_release_int(&j->twice, 1);
   j->thread = sthread_get_current_thread_id();
   note_most(&inside, &most);
   while (   (unsigned)retro_atomic_load_acquire_int(&most) < gather_goal
          && !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
          && cpu_features_get_time_usec() < deadline)
      retro_sleep(1);
   retro_atomic_fetch_add_int(&inside, -1);
   retro_atomic_fetch_add_int(&j->in, -1);

   if (     ++j->calls >= j->finish_after
         || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void serial_handler(retro_task_t *task)
{
   struct job *j = (struct job*)task->user_data;
   note_most(&serial_in, &serial_most);
   j->thread = sthread_get_current_thread_id();
   if (worker0 && j->thread != worker0)
      retro_atomic_fetch_add_int(&off_worker0, 1);
   retro_sleep(2);
   retro_atomic_fetch_add_int(&serial_in, -1);
   if (++j->calls >= j->finish_after)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* ---- detachable parallel tasks ---- */
#define SLOW_MS 1500

struct stuck
{
   retro_atomic_int_t entered;
   retro_atomic_int_t returned;
   retro_atomic_int_t cleaned;
   uintptr_t          thread;
   uintptr_t          clean_thread;
   bool               clean_after_return;
};

static retro_atomic_int_t stuck_release;
static retro_atomic_int_t hog_inside;
static retro_atomic_int_t hog_release;
static retro_atomic_int_t hog_done;
static uintptr_t          hog_thread;  /* the one worker, while hogged */

/* Blocks until released: past deinit's bound if the test holds it. */
static void stuck_handler(retro_task_t *task)
{
   struct stuck *s = (struct stuck*)task->user_data;
   s->thread = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&s->entered, 1);
   while (!retro_atomic_load_acquire_int(&stuck_release))
      retro_sleep(1);
   /* what a handler left behind may still do */
   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   retro_atomic_store_release_int(&s->returned, 1);
}

/* Frees the state push_stuck() gave the task: LeakSanitizer sees a
 * cleanup that never runs. */
static void stuck_cleanup(retro_task_t *task)
{
   struct stuck *s = (struct stuck*)task->user_data;
   free(task->state);
   task->state           = NULL;
   s->clean_thread       = sthread_get_current_thread_id();
   s->clean_after_return =
      retro_atomic_load_acquire_int(&s->returned) != 0;
   retro_atomic_fetch_add_int(&s->cleaned, 1);
}

static void slow_handler(retro_task_t *task)
{
   struct stuck *s = (struct stuck*)task->user_data;
   s->thread = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&s->entered, 1);
   retro_sleep(SLOW_MS);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   retro_atomic_store_release_int(&s->returned, 1);
}

/* Holds the one worker, so that a parallel task runs on an extra. */
static void hog_handler(retro_task_t *task)
{
   hog_thread = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&hog_inside, 1);
   while (!retro_atomic_load_acquire_int(&hog_release))
      retro_sleep(1);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   retro_atomic_store_release_int(&hog_done, 1);
}

/* Whether *flag was set within three seconds. */
static bool flag_within(retro_atomic_int_t *flag)
{
   retro_time_t until = cpu_features_get_time_usec() + 3000000;
   while (!retro_atomic_load_acquire_int(flag))
   {
      if (cpu_features_get_time_usec() > until)
         return false;
      task_queue_check();
      retro_sleep(1);
   }
   return true;
}

/* The same with no queue up, which a check would start. */
static bool set_within(retro_atomic_int_t *flag)
{
   retro_time_t until = cpu_features_get_time_usec() + 3000000;
   while (!retro_atomic_load_acquire_int(flag))
   {
      if (cpu_features_get_time_usec() > until)
         return false;
      retro_sleep(1);
   }
   return true;
}

static void hog_start(void)
{
   retro_task_t *task = task_init();
   retro_atomic_store_release_int(&hog_inside, 0);
   retro_atomic_store_release_int(&hog_release, 0);
   retro_atomic_store_release_int(&hog_done, 0);
   task->handler = hog_handler;
   task_queue_push(task);
   CHECK(flag_within(&hog_inside), "the one worker is held");
}

static void hog_stop(void)
{
   retro_atomic_store_release_int(&hog_release, 1);
   CHECK(flag_within(&hog_done), "the one worker is let go");
   task_queue_check();
}

static void stuck_init(struct stuck *s)
{
   memset(s, 0, sizeof(*s));
   retro_atomic_int_init(&s->entered, 0);
   retro_atomic_int_init(&s->returned, 0);
   retro_atomic_int_init(&s->cleaned, 0);
}

/* Whether s's cleanup ran once, on @thread, after its handler
 * returned; waits for it without a check. */
static bool cleaned_on(struct stuck *s, uintptr_t thread)
{
   return set_within(&s->cleaned)
      && retro_atomic_load_acquire_int(&s->cleaned) == 1
      && s->clean_thread == thread
      && s->clean_after_return;
}

/* Whether the queue still takes @thread for one of its workers. */
static bool tid_registered(uintptr_t thread)
{
   unsigned i;
   for (i = 0; i < 1 + TASK_PARALLEL_MAX; i++)
      if ((uintptr_t)retro_atomic_load_acquire_ptr(&task_worker_tid[i])
            == thread)
         return true;
   return false;
}

/* ---- a cleanup still running when deinit returns ---- */
static struct stuck        late_st;
static struct task_worker *late_worker;
static retro_atomic_int_t  late_go;
static retro_atomic_int_t  late_clean_in;
static retro_atomic_int_t  late_deinit_done;
static retro_atomic_int_t  late_clean_done;
static bool                late_lock_kept;

/* As stuck_handler(), on a flag of its own: if the lane fails, a task
 * it leaves queued holds up no later lane. */
static void late_handler(retro_task_t *task)
{
   struct stuck *s = (struct stuck*)task->user_data;
   s->thread = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&s->entered, 1);
   while (!retro_atomic_load_acquire_int(&late_go))
      retro_sleep(1);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* Takes property_lock as the accessors do, and holds on to it until
 * deinit has returned. */
static void late_cleanup(retro_task_t *task)
{
   slock_t *lock = property_lock;

   task_free_title(task);
   retro_atomic_store_release_int(&late_clean_in, 1);
   set_within(&late_deinit_done);
   late_lock_kept = property_lock == lock;
   /* what an accessor that loaded the lock before deinit let go reads */
   (void)*(volatile const char*)lock;
   (void)task_get_title(task);
   retro_atomic_store_release_int(&late_clean_done, 1);
}

/* Lets the stuck handler go once deinit has left its worker behind,
 * then the one worker once the cleanup has begun. */
static void late_release(void *data)
{
   retro_time_t until = cpu_features_get_time_usec() + 3000000;
   (void)data;
   while (   late_worker
          && retro_atomic_load_acquire_int(&late_worker->state)
               != TASK_WORKER_ORPHANED
          && cpu_features_get_time_usec() < until)
      retro_sleep(1);
   retro_atomic_store_release_int(&late_go, 1);
   set_within(&late_clean_in);
   retro_sleep(50);
   retro_atomic_store_release_int(&hog_release, 1);
}

static void count_retired(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retro_atomic_fetch_add_int(&retired, 1);
}

static void push_asking(retro_task_handler_t handler, struct job *j,
      int flags, unsigned workers)
{
   retro_task_t *task     = task_init();
   task->handler          = handler;
   task->callback         = count_retired;
   task->user_data        = j;
   task->flags           |= flags | RETRO_TASK_FLG_MUTE;
   task->parallel_workers = workers;
   task_queue_push(task);
}

static void push(retro_task_handler_t handler, struct job *j, int flags)
{
   push_asking(handler, j, flags, 0);
}

static bool not_all_retired(void *data)
{
   return retro_atomic_load_acquire_int(&retired) < *(int*)data;
}

static void push_stuck(retro_task_handler_t handler, struct stuck *s,
      int flags)
{
   retro_task_t *task = task_init();
   task->handler   = handler;
   task->callback  = count_retired;
   task->cleanup   = stuck_cleanup;
   task->state     = malloc(64);
   task->user_data = s;
   task->flags    |= flags | RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
}

static int64_t ms_since(retro_time_t start)
{
   return (int64_t)((cpu_features_get_time_usec() - start) / 1000);
}

static void reset_counts(void)
{
   retro_atomic_store_release_int(&inside, 0);
   retro_atomic_store_release_int(&most, 0);
   retro_atomic_store_release_int(&serial_in, 0);
   retro_atomic_store_release_int(&serial_most, 0);
   retro_atomic_store_release_int(&retired, 0);
   retro_atomic_store_release_int(&off_worker0, 0);
}

static void wait_retired(int n)
{
   task_queue_wait(not_all_retired, &n);
   task_queue_check();
}

static void job_init(struct job *j, unsigned finish_after)
{
   memset(j, 0, sizeof(*j));
   retro_atomic_int_init(&j->in, 0);
   retro_atomic_int_init(&j->twice, 0);
   j->finish_after = finish_after;
}

int main(void)
{
   struct job par[4], ser[5], multi, det, probe;
   struct stuck st[3];
   retro_time_t start;
   int64_t took;
   unsigned i;

   retro_atomic_int_init(&inside, 0);
   retro_atomic_int_init(&most, 0);
   retro_atomic_int_init(&serial_in, 0);
   retro_atomic_int_init(&serial_most, 0);
   retro_atomic_int_init(&retired, 0);
   retro_atomic_int_init(&off_worker0, 0);

   task_queue_set_parallel_workers(3);
   task_queue_init(true, NULL);

   /* where the one worker is: a plain task's handler */
   job_init(&probe, 1);
   push(serial_handler, &probe, 0);
   wait_retired(1);
   worker0 = probe.thread;
   CHECK(worker0 && worker0 != sthread_get_current_thread_id(),
         "a plain task runs on the worker");

   /* four parallel tasks meet; five plain ones run beside, one at a time */
   reset_counts();
   gather_goal = 4;
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 1);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   for (i = 0; i < 5; i++)
   {
      job_init(&ser[i], 3);
      push(serial_handler, &ser[i], 0);
   }
   wait_retired(9);
   CHECK(retro_atomic_load_acquire_int(&most) == 4,
         "three extra workers: four parallel handlers at once (saw %d)",
         retro_atomic_load_acquire_int(&most));
   CHECK(parallel_count == 3, "three extra workers started (%u)",
         parallel_count);
   CHECK(retro_atomic_load_acquire_int(&serial_most) == 1,
         "plain tasks never overlap (saw %d at once)",
         retro_atomic_load_acquire_int(&serial_most));
   CHECK(retro_atomic_load_acquire_int(&off_worker0) == 0,
         "plain tasks never leave the one worker (%d did)",
         retro_atomic_load_acquire_int(&off_worker0));

   /* one parallel task called eight times, with others to tempt the
    * idle workers: never in two handlers at once */
   reset_counts();
   gather_goal = 1;
   job_init(&multi, 8);
   push(parallel_handler, &multi, RETRO_TASK_FLG_PARALLEL);
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 2);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   wait_retired(5);
   CHECK(multi.calls == 8, "the task was called eight times (%u)", multi.calls);
   CHECK(!retro_atomic_load_acquire_int(&multi.twice),
         "a parallel task is never in two handlers at once");

   /* detachable too: an extra worker takes it while the one is busy */
   reset_counts();
   hog_start();
   job_init(&det, 1);
   push(serial_handler, &det,
         RETRO_TASK_FLG_PARALLEL | RETRO_TASK_FLG_DETACHABLE);
   CHECK(flag_within(&retired), "the detachable parallel task ran");
   CHECK(det.thread && det.thread != worker0,
         "a detachable parallel task runs on an extra worker");
   hog_stop();

   /* a deinit with parallel tasks queued: every worker stops; then with
    * none asked for: no extra workers, and parallel tasks still run */
   reset_counts();
   gather_goal = 99; /* they wait until cancelled: inside at the reset */
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 1000);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   retro_sleep(20);
   task_queue_reset();
   wait_retired(4);
   task_queue_deinit();
   CHECK(parallel_count == 0 && !retro_atomic_load_acquire_int(&parallel_started),
         "deinit stopped the extra workers");

   task_queue_set_parallel_workers(0);
   task_queue_init(true, NULL);
   reset_counts();
   gather_goal = 1;
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 2);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   wait_retired(4);
   CHECK(parallel_count == 0, "none asked for: no extra workers (%u)", parallel_count);
   CHECK(retro_atomic_load_acquire_int(&most) == 1,
         "none asked for: one handler at a time (saw %d)",
         retro_atomic_load_acquire_int(&most));
   task_queue_deinit();

   /* the next init starts them again */
   task_queue_set_parallel_workers(2);
   task_queue_init(true, NULL);
   reset_counts();
   gather_goal = 3;
   for (i = 0; i < 3; i++)
   {
      job_init(&par[i], 1);
      push(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL);
   }
   wait_retired(3);
   CHECK(parallel_count == 2, "two asked for after a re-init: two extra workers (%u)",
         parallel_count);
   CHECK(retro_atomic_load_acquire_int(&most) == 3,
         "two extra workers: three handlers at once (saw %d)",
         retro_atomic_load_acquire_int(&most));
   task_queue_deinit();

   /* a task names the extra workers it needs; without the flag the
    * number is ignored */
   task_queue_set_parallel_workers(0);
   task_queue_init(true, NULL);
   reset_counts();
   job_init(&probe, 1);
   push_asking(serial_handler, &probe, 0, 3);
   wait_retired(1);
   CHECK(parallel_count == 0, "a plain task asking three starts none (%u)",
         parallel_count);

   reset_counts();
   gather_goal = 4;
   for (i = 0; i < 4; i++)
   {
      job_init(&par[i], 1);
      push_asking(parallel_handler, &par[i], RETRO_TASK_FLG_PARALLEL, 3);
   }
   wait_retired(4);
   CHECK(parallel_count == 3, "a task asking three starts three extra workers (%u)",
         parallel_count);
   CHECK(retro_atomic_load_acquire_int(&most) == 4,
         "three extra workers: four handlers at once (saw %d)",
         retro_atomic_load_acquire_int(&most));

   /* grow-only: fewer starts none, more starts the rest, up to eight */
   reset_counts();
   gather_goal = 1;
   job_init(&par[0], 1);
   push_asking(parallel_handler, &par[0], RETRO_TASK_FLG_PARALLEL, 2);
   wait_retired(1);
   CHECK(parallel_count == 3, "a later task asking two starts none (%u)",
         parallel_count);
   reset_counts();
   job_init(&par[0], 1);
   push_asking(parallel_handler, &par[0], RETRO_TASK_FLG_PARALLEL, 20);
   wait_retired(1);
   CHECK(parallel_count == TASK_PARALLEL_MAX,
         "one asking twenty brings them to eight (%u)", parallel_count);
   task_queue_deinit();

   /* a task that leaves the number alone asks for none, whatever its
    * memory held before task_init() */
   task_queue_init(true, NULL);
   reset_counts();
   {
      void *junk = malloc(sizeof(retro_task_t));
      if (junk)
      {
         memset(junk, 0x7f, sizeof(retro_task_t));
         free(junk);
      }
   }
   job_init(&par[0], 1);
   {
      retro_task_t *task = task_init();
      task->handler      = parallel_handler;
      task->callback     = count_retired;
      task->user_data    = &par[0];
      task->flags       |= RETRO_TASK_FLG_PARALLEL | RETRO_TASK_FLG_MUTE;
      task_queue_push(task);
   }
   wait_retired(1);
   CHECK(parallel_count == 0, "a task that names no number starts none (%u)",
         parallel_count);
   task_queue_deinit();
   CHECK(parallel_count == 0 && !retro_atomic_load_acquire_int(&parallel_started),
         "deinit stops every one of them");

   /* the frontend's number holds for a task asking fewer */
   task_queue_set_parallel_workers(2);
   task_queue_init(true, NULL);
   reset_counts();
   job_init(&par[0], 1);
   push_asking(parallel_handler, &par[0], RETRO_TASK_FLG_PARALLEL, 1);
   wait_retired(1);
   CHECK(parallel_count == 2,
         "a task asking one, with two from the frontend, starts two (%u)",
         parallel_count);
   task_queue_deinit();
   task_queue_set_parallel_workers(0);

   /* an extra left behind returns and begins its cleanup while deinit
    * still waits for the one worker, in a plain handler: the lock its
    * accessors take outlives the cleanup */
   CHECK(!property_lock_pinned, "nothing has pinned property_lock yet");
   task_queue_set_parallel_workers(1);
   task_queue_init(true, NULL);
   reset_counts();
   retro_atomic_int_init(&late_go, 0);
   retro_atomic_int_init(&late_clean_in, 0);
   retro_atomic_int_init(&late_deinit_done, 0);
   retro_atomic_int_init(&late_clean_done, 0);
   hog_start();
   stuck_init(&late_st);
   {
      retro_task_t *task = task_init();
      task->handler      = late_handler;
      task->cleanup      = late_cleanup;
      task->user_data    = &late_st;
      task->flags       |= RETRO_TASK_FLG_PARALLEL
         | RETRO_TASK_FLG_DETACHABLE | RETRO_TASK_FLG_MUTE;
      task_queue_push(task);
   }
   CHECK(flag_within(&late_st.entered) && late_st.thread != hog_thread,
         "a detachable parallel task is stuck beside the held one worker");
   late_worker = parallel_self[0];
   {
      sthread_t *helper = sthread_create(late_release, NULL);
      task_queue_deinit();
      retro_atomic_store_release_int(&late_deinit_done, 1);
      sthread_join(helper);
   }
   CHECK(retro_atomic_load_acquire_int(&late_clean_in),
         "its cleanup begins before deinit returns");
   CHECK(set_within(&late_clean_done) && late_lock_kept,
         "property_lock outlives a cleanup that began while deinit waited");
   task_queue_set_parallel_workers(0);

   /* a detachable parallel task stuck on an extra worker: deinit leaves
    * it behind after its bound; once its handler returns, its worker
    * cleans it up and frees it, and its callback never runs */
   task_queue_set_parallel_workers(2);
   task_queue_init(true, NULL);
   reset_counts();
   retro_atomic_store_release_int(&stuck_release, 0);
   hog_start();
   stuck_init(&st[0]);
   push_stuck(stuck_handler, &st[0],
         RETRO_TASK_FLG_PARALLEL | RETRO_TASK_FLG_DETACHABLE);
   CHECK(flag_within(&st[0].entered) && st[0].thread != hog_thread,
         "a detachable parallel task is stuck on an extra worker");
   hog_stop();
   start = cpu_features_get_time_usec();
   task_queue_deinit();
   took  = ms_since(start);
   CHECK(took >= 900 && took < 2000,
         "deinit leaves it behind after its bound (%d ms)", (int)took);
   CHECK(!retro_atomic_load_acquire_int(&st[0].returned),
         "it was still in its handler when deinit returned");
   retro_atomic_store_release_int(&stuck_release, 1);
   CHECK(set_within(&st[0].returned), "its handler returns once let go");
   retro_sleep(50);
   CHECK(!retro_atomic_load_acquire_int(&retired),
         "the task it was left with never reaches its callback");
   CHECK(cleaned_on(&st[0], st[0].thread),
         "its cleanup runs once, on that worker, after its handler");
   CHECK(!tid_registered(st[0].thread),
         "the queue no longer takes that worker's id for its own");

   /* one that is not detachable is still waited for */
   task_queue_init(true, NULL);
   hog_start();
   stuck_init(&st[1]);
   push_stuck(slow_handler, &st[1], RETRO_TASK_FLG_PARALLEL);
   CHECK(flag_within(&st[1].entered) && st[1].thread != hog_thread,
         "a slow parallel task runs on an extra worker");
   hog_stop();
   start = cpu_features_get_time_usec();
   task_queue_deinit();
   took  = ms_since(start);
   CHECK(retro_atomic_load_acquire_int(&st[1].returned)
         && took >= SLOW_MS - 300,
         "deinit waits for a parallel handler that is not detachable (%d ms)",
         (int)took);

   /* the one worker and two extras stuck in detachable handlers: all
    * three are left behind against one bound, a new queue runs beside
    * them, and the last of them out frees their primitives, once */
   retro_atomic_store_release_int(&stuck_release, 0);
   task_queue_init(true, NULL);
   task_queue_check(); /* the slow task's callback, carried over */
   reset_counts();
   for (i = 0; i < 3; i++)
      stuck_init(&st[i]);
   push_stuck(stuck_handler, &st[0], RETRO_TASK_FLG_DETACHABLE);
   CHECK(flag_within(&st[0].entered), "a detachable task holds the one worker");
   push_stuck(stuck_handler, &st[1],
         RETRO_TASK_FLG_PARALLEL | RETRO_TASK_FLG_DETACHABLE);
   push_stuck(stuck_handler, &st[2],
         RETRO_TASK_FLG_PARALLEL | RETRO_TASK_FLG_DETACHABLE);
   CHECK(flag_within(&st[1].entered) && flag_within(&st[2].entered),
         "two detachable parallel tasks are stuck on the extra workers");
   start = cpu_features_get_time_usec();
   task_queue_deinit();
   took  = ms_since(start);
   CHECK(took >= 900 && took < 2000,
         "deinit leaves all three behind after one bound (%d ms)", (int)took);
   task_queue_init(true, NULL);
   job_init(&par[0], 1);
   gather_goal = 1;
   push(parallel_handler, &par[0], RETRO_TASK_FLG_PARALLEL);
   CHECK(flag_within(&retired) && par[0].calls == 1,
         "a new queue runs a parallel task while they are stuck");
   retro_atomic_store_release_int(&stuck_release, 1);
   for (i = 0; i < 3; i++)
      CHECK(flag_within(&st[i].returned), "left-behind handler %u returns", i);
   retro_sleep(100);
   CHECK(retro_atomic_load_acquire_int(&retired) == 1,
         "none of their callbacks runs");
   for (i = 0; i < 3; i++)
      CHECK(cleaned_on(&st[i], st[i].thread),
            "left-behind task %u is cleaned up once, on its worker, after "
            "its handler", i);

   /* one not left behind is cleaned up at its retirement */
   stuck_init(&st[0]);
   push_stuck(stuck_handler, &st[0],
         RETRO_TASK_FLG_PARALLEL | RETRO_TASK_FLG_DETACHABLE);
   wait_retired(2);
   CHECK(cleaned_on(&st[0], sthread_get_current_thread_id()),
         "a detachable task not left behind is cleaned up once, here");
   task_queue_deinit();
   task_queue_set_parallel_workers(0);

   /* unthreaded: the flag changes nothing */
   task_queue_init(false, NULL);
   reset_counts();
   gather_goal = 1;
   job_init(&par[0], 2);
   push(parallel_handler, &par[0], RETRO_TASK_FLG_PARALLEL);
   wait_retired(1);
   CHECK(par[0].thread == sthread_get_current_thread_id(),
         "unthreaded: the handler runs here");
   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "task_queue_parallel_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("task_queue_parallel_test: OK\n");
   return 0;
}
