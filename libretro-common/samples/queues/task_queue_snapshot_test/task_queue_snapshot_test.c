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
   puts("task progress snapshot tests passed");
   return 0;
}
