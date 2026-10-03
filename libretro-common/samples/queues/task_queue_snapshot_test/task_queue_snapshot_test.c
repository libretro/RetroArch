#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include <queues/task_queue.h>

static unsigned fail_allocation;
static void *snapshot_malloc(size_t size)
{
   if (fail_allocation && --fail_allocation == 0)
      return NULL;
   return malloc(size);
}

/* Exercise allocation failure in the production copy routine. */
#define malloc snapshot_malloc
#include "../../../queues/task_queue.c"
#undef malloc

static unsigned regular_updates;
static void regular_progress_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)prio;
   (void)duration;
   (void)flush;
   regular_updates++;
   assert(msg && *msg);
   assert(task_get_progress(task) == (int8_t)(regular_updates * 25));
   assert(((task_get_flags(task) & RETRO_TASK_FLG_FINISHED) != 0)
         == (regular_updates == 3));
}

static void regular_progress_handler(retro_task_t *task)
{
   int8_t progress = task_get_progress(task) + 25;
   task_set_progress(task, progress);
   if (progress == 75)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void test_regular_progress(void)
{
   retro_task_t *task;
   unsigned i;
   task_queue_init(false, regular_progress_push);
   task = task_init();
   assert(task);
   task->title = strdup("regular progress");
   assert(task->title);
   task->progress = 0;
   task->handler = regular_progress_handler;
   regular_updates = 0;
   task_queue_push(task);
   for (i = 1; i <= 3; i++)
   {
      task_queue_check();
      assert(regular_updates == i);
   }
   task_queue_check();
   assert(regular_updates == 3);
   task_queue_deinit();
}

static unsigned completion_updates;
static void completion_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)prio;
   (void)duration;
   (void)flush;
   assert(msg && !*msg);
   assert(task_get_flags(task) & RETRO_TASK_FLG_FINISHED);
   assert(task_get_progress(task) == 37);
   completion_updates++;
}

static void test_suppressed_completion(bool threaded)
{
   retro_task_t task;
   unsigned mute, title;
   task_queue_init(threaded, completion_push);
   for (mute = 0; mute < 2; mute++)
      for (title = 0; title < 2; title++)
      {
         if (!mute && title)
            continue;
         memset(&task, 0, sizeof(task));
         task.title = title ? "muted title" : NULL;
         task.progress = 37;
         task.flags = mute ? RETRO_TASK_FLG_MUTE : 0;
         task.frontend_userdata = &task;
         completion_updates = 0;
         task_queue_push_progress(&task);
         assert(completion_updates == 0);
         /* Finished, with or without anything attached: the frontend
          * owns that link and is the one to read it, so the empty
          * retirement push goes out either way */
         task.flags |= RETRO_TASK_FLG_FINISHED;
         task.frontend_userdata = NULL;
         task_queue_push_progress(&task);
         assert(completion_updates == 1);
         task.frontend_userdata = &task;
         task_queue_push_progress(&task);
         assert(completion_updates == 2);
      }
   task_queue_deinit();
}

static slock_t *gate_lock;
static scond_t *gate_cond;
static unsigned stage;
static retro_task_t task;

static char *copy_string(const char *s)
{
   size_t len = strlen(s) + 1;
   char *copy = (char*)malloc(len);
   assert(copy);
   memcpy(copy, s, len);
   return copy;
}

static void free_snapshot(task_progress_snapshot_t *snapshot)
{
   free(snapshot->title);
   free(snapshot->error);
}

static void replace_properties(void *unused)
{
   (void)unused;
   slock_lock(gate_lock);
   while (!stage)
      scond_wait(gate_cond, gate_lock);
   slock_unlock(gate_lock);
   task_free_title(&task);
   task_set_title(&task, copy_string("replacement title"));
   task_free_error(&task);
   task_set_error(&task, copy_string("replacement error"));
   task_set_progress(&task, 100);
   task_set_flags(&task, RETRO_TASK_FLG_FINISHED, true);
   slock_lock(gate_lock);
   stage = 2;
   scond_signal(gate_cond);
   slock_unlock(gate_lock);
}

static void churn_properties(void *unused)
{
   unsigned i;
   (void)unused;
   for (i = 0; i < 10000; i++)
   {
      task_free_title(&task);
      task_set_title(&task, copy_string("concurrent title"));
      task_free_error(&task);
      task_set_error(&task, copy_string("concurrent error"));
      task_set_progress(&task, (int8_t)(i % 101));
   }
}

/* The progress reports gather() makes - formatting, and the push into
 * the frontend's message path - run without running_lock held: a
 * frontend push that takes its time must not hold up a thread queueing
 * a task. Here the push for a titled task queues another task from a
 * second thread and waits for that to return. */
static retro_task_t *report_task;
static retro_atomic_int_t report_release;
static int report_probed;
static retro_atomic_int_t report_pushed;
static bool report_pushed_in_time;
static sthread_t *report_thread;

static void report_quick_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void report_hold_handler(retro_task_t *task)
{
   if (retro_atomic_load_acquire_int(&report_release))
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void report_queue_other(void *unused)
{
   retro_task_t *other = task_init();
   (void)unused;
   assert(other);
   other->handler = report_quick_handler;
   task_queue_push(other);
   retro_atomic_store_release_int(&report_pushed, 1);
}

static void report_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   unsigned i;
   (void)msg;
   (void)prio;
   (void)duration;
   (void)flush;
   if (task != report_task || report_probed)
      return;
   report_probed = 1;
   report_thread = sthread_create(report_queue_other, NULL);
   assert(report_thread);
   for (i = 0; i < 1000
         && !retro_atomic_load_acquire_int(&report_pushed); i++)
   {
      slock_t *l = slock_new();
      scond_t *c = scond_new();
      slock_lock(l);
      scond_wait_timeout(c, l, 1000);
      slock_unlock(l);
      scond_free(c);
      slock_free(l);
   }
   report_pushed_in_time = retro_atomic_load_acquire_int(&report_pushed) != 0;
}

static void test_report_outside_running_lock(void)
{
   unsigned i;
   task_queue_init(true, report_push);
   report_task = task_init();
   assert(report_task);
   report_task->title   = strdup("held task");
   report_task->handler = report_hold_handler;
   retro_atomic_store_release_int(&report_release, 0);
   report_probed        = 0;
   retro_atomic_store_release_int(&report_pushed, 0);
   task_queue_push(report_task);
   for (i = 0; i < 2000 && !report_probed; i++)
      task_queue_check();
   assert(report_probed);
   sthread_join(report_thread);
   if (!report_pushed_in_time)
   {
      printf("FAIL: a task push waited on the frontend's progress report\n");
      exit(1);
   }
   retro_atomic_store_release_int(&report_release, 1);
   task_queue_wait(NULL, NULL);
   task_queue_deinit();
}

/* More tasks running than one pass reports on: each pass reports the
 * next batch along, so every task is reported within a few passes. */
#define MANY_TASKS 80
static retro_task_t *many[MANY_TASKS];
static unsigned many_seen[MANY_TASKS];

static void many_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   unsigned i;
   (void)msg;
   (void)prio;
   (void)duration;
   (void)flush;
   for (i = 0; i < MANY_TASKS; i++)
      if (many[i] == task)
         many_seen[i]++;
}

static void test_report_many(void)
{
   unsigned i, pass, unseen = 0;
   task_queue_init(true, many_push);
   retro_atomic_store_release_int(&report_release, 0);
   for (i = 0; i < MANY_TASKS; i++)
   {
      many[i] = task_init();
      assert(many[i]);
      many[i]->title   = strdup("one of many");
      many[i]->handler = report_hold_handler;
      many_seen[i]     = 0;
      task_queue_push(many[i]);
   }
   /* Three passes cover 80 tasks at 32 a pass. */
   for (pass = 0; pass < 3; pass++)
      task_queue_check();
   for (i = 0; i < MANY_TASKS; i++)
      if (!many_seen[i])
         unseen++;
   if (unseen)
   {
      printf("FAIL: %u of %u running tasks never reported\n",
            unseen, MANY_TASKS);
      exit(1);
   }
   retro_atomic_store_release_int(&report_release, 1);
   task_queue_wait(NULL, NULL);
   task_queue_deinit();
}

int main(void)
{
   task_progress_snapshot_t snapshot;
   sthread_t *thread;
   unsigned i;

   task_queue_init(true, NULL);
   assert(task_queue_is_threaded());
   gate_lock = slock_new();
   gate_cond = scond_new();
   assert(gate_lock && gate_cond);
   task_set_title(&task, copy_string("original title"));
   task_set_error(&task, copy_string("original error"));
   task_set_progress(&task, 25);

   thread = sthread_create(replace_properties, NULL);
   assert(thread);
   assert(task_get_progress_snapshot(&task, &snapshot));
   slock_lock(gate_lock);
   stage = 1;
   scond_signal(gate_cond);
   for (i = 0; stage != 2 && i < 5; i++)
      scond_wait_timeout(gate_cond, gate_lock, 1000000);
   assert(stage == 2);
   slock_unlock(gate_lock);

   /* The worker replaced and freed both source strings while the
    * frontend retained its snapshot; it must still see the old values. */
   assert(strcmp(snapshot.title, "original title") == 0);
   assert(strcmp(snapshot.error, "original error") == 0);
   assert(snapshot.progress == 25);
   assert(!(snapshot.flags & RETRO_TASK_FLG_FINISHED));
   free_snapshot(&snapshot);
   sthread_join(thread);

   for (i = 1; i <= 2; i++)
   {
      fail_allocation = i;
      assert(!task_get_progress_snapshot(&task, &snapshot));
      assert(!snapshot.title && !snapshot.error);
      assert(snapshot.flags & RETRO_TASK_FLG_FINISHED);
      assert(snapshot.progress == 100);
   }
   fail_allocation = 0;
   task_free_title(&task);
   task_free_error(&task);
   assert(task_get_progress_snapshot(&task, &snapshot));
   assert(!snapshot.title && !snapshot.error);
   free_snapshot(&snapshot);

   thread = sthread_create(churn_properties, NULL);
   assert(thread);
   for (i = 0; i < 10000; i++)
   {
      assert(task_get_progress_snapshot(&task, &snapshot));
      if (snapshot.title)
         assert(strcmp(snapshot.title, "concurrent title") == 0);
      if (snapshot.error)
         assert(strcmp(snapshot.error, "concurrent error") == 0);
      assert(snapshot.progress >= 0 && snapshot.progress <= 100);
      free_snapshot(&snapshot);
   }
   sthread_join(thread);
   task_free_title(&task);
   task_free_error(&task);
   scond_free(gate_cond);
   slock_free(gate_lock);
   task_queue_deinit();

   /* The same API is available without a threaded runner. */
   task_queue_init(false, NULL);
   task_set_title(&task, copy_string("unthreaded"));
   assert(task_get_progress_snapshot(&task, &snapshot));
   task_free_title(&task);
   assert(strcmp(snapshot.title, "unthreaded") == 0);
   free_snapshot(&snapshot);
   task_queue_deinit();
   test_regular_progress();
   test_suppressed_completion(false);
   test_suppressed_completion(true);
   test_report_outside_running_lock();
   test_report_many();
   puts("task progress snapshot tests passed");
   return 0;
}
