
#include <stdlib.h>

#include "../general.h"
#include "../verbosity.h"
#include "../msg_hash.h"
#include "tasks.h"

#ifdef HAVE_THREADS
#include "rthreads/rthreads.h"
#endif

typedef struct {
   rarch_task_t *front;
   rarch_task_t *back;
} task_queue_t;

struct rarch_task_impl {
   void (*push_running)(rarch_task_t *);
   void (*reset)(void);
   void (*wait)(void);
   void (*gather)(void);
   bool (*find)(rarch_task_finder_t, void*);
   void (*init)(void);
   void (*deinit)(void);
};
static task_queue_t tasks_running  = {NULL, NULL};
static task_queue_t tasks_finished = {NULL, NULL};

static struct rarch_task_impl *impl_current = NULL;

static void task_queue_put(task_queue_t *queue, rarch_task_t *task)
{
   task->next = NULL;

   if (queue->front == NULL)
      queue->front = task;
   else
      queue->back->next = task;

   queue->back = task;
}

static rarch_task_t *task_queue_get(task_queue_t *queue)
{
   rarch_task_t *task = queue->front;

   if (task)
   {
      queue->front = task->next;
      task->next = NULL;
   }

   return task;
}

static void rarch_task_internal_gather(void)
{
   rarch_task_t *task;
   while ((task = task_queue_get(&tasks_finished)) != NULL)
   {
      if (task->callback)
         task->callback(task->task_data, task->user_data, task->error);

      if (task->error)
         free(task->error);

      if (task->title)
         free(task->title);

      free(task);
   }
}

static void push_task_progress(rarch_task_t *task)
{
   if (task->title)
   {
      if (task->finished)
      {
         if (task->error)
            rarch_main_msg_queue_pushf(1, 60, true, "%s: %s",
               msg_hash_to_str(MSG_TASK_FAILED), task->title);
         else
            rarch_main_msg_queue_pushf(1, 60, true, "100%%: %s", task->title);
      }
      else
      {
         if (task->progress >= 0 && task->progress <= 100)
            rarch_main_msg_queue_pushf(1, 10, true, "%i%%: %s", task->progress, task->title);
         else
            rarch_main_msg_queue_pushf(1, 10, true, "%s...", task->title);
      }
   }
}

static void regular_push_running(rarch_task_t *task)
{
   task_queue_put(&tasks_running, task);
}

static void regular_gather(void)
{
   rarch_task_t *task  = NULL;
   rarch_task_t *queue = NULL;
   rarch_task_t *next  = NULL;

   while ((task = task_queue_get(&tasks_running)) != NULL)
   {
      task->next = queue;
      queue = task;
   }

   for (task = queue; task; task = next)
   {
      next = task->next;
      task->handler(task);

      push_task_progress(task);

      if (task->finished)
         task_queue_put(&tasks_finished, task);
      else
         regular_push_running(task);
   }

   rarch_task_internal_gather();
}

static void regular_wait(void)
{
   while (tasks_running.front)
      regular_gather();
}

static void regular_reset()
{
   rarch_task_t *task;

   for (task = tasks_running.front; task; task = task->next)
      task->cancelled = true;
}

static void regular_init(void)
{
}

static void regular_deinit(void)
{
}

static bool regular_find(rarch_task_finder_t func, void *user_data)
{
   rarch_task_t *task;

   for (task = tasks_running.front; task; task = task->next)
   {
      if (func(task, user_data))
         return true;
   }

   return false;
}

static struct rarch_task_impl impl_regular = {
   regular_push_running,
   regular_reset,
   regular_wait,
   regular_gather,
   regular_find,
   regular_init,
   regular_deinit
};

#ifdef HAVE_THREADS
static slock_t *running_lock  = NULL;
static slock_t *finished_lock = NULL;
static scond_t *worker_cond   = NULL;
static sthread_t *worker_thread = NULL;
static bool worker_continue = true; /* use running_lock when touching it */

static void threaded_push_running(rarch_task_t *task)
{
   slock_lock(running_lock);
   task_queue_put(&tasks_running, task);
   scond_signal(worker_cond);
   slock_unlock(running_lock);
}

static void threaded_gather(void)
{
   rarch_task_t *task;
   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      push_task_progress(task);

   slock_unlock(running_lock);

   slock_lock(finished_lock);
   rarch_task_internal_gather();
   slock_unlock(finished_lock);
}

static void threaded_wait(void)
{
   bool wait;
   do
   {
      threaded_gather();

      slock_lock(running_lock);
      wait = (tasks_running.front != NULL);
      slock_unlock(running_lock);
   } while (wait);
}

static void threaded_reset(void)
{
   rarch_task_t *task;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      task->cancelled = true;
   slock_unlock(running_lock);
}

static void threaded_worker(void *userdata)
{
   (void)userdata;

   RARCH_LOG("Threaded rarch_task started\n");

   for (;;)
   {
      rarch_task_t *queue = NULL;
      rarch_task_t *task  = NULL;
      rarch_task_t *next  = NULL;

      /* pop all into a local queue to avoid trouble with rarch_task_push(),
       * tasks are in the reverse order here. */
      slock_lock(running_lock);

      if (!worker_continue)
         break; /* should we keep running until all tasks finished? */

      while ((task = task_queue_get(&tasks_running)) != NULL)
      {
         task->next = queue;
         queue = task;
      }

      if (queue == NULL) /* no tasks running, lets wait a bit */
      {
         scond_wait(worker_cond, running_lock);
         slock_unlock(running_lock);
         continue;
      }

      slock_unlock(running_lock);

      for (task = queue; task; task = next)
      {
         next = task->next;
         task->handler(task);

         if (task->finished)
         {
            slock_lock(finished_lock);
            task_queue_put(&tasks_finished, task);
            slock_unlock(finished_lock);
         }
         else
            threaded_push_running(task);
      }
   }

   slock_unlock(running_lock);
}

static bool threaded_find(rarch_task_finder_t func, void *user_data)
{
   rarch_task_t *task;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
   {
      if (func(task, user_data))
         return true;
   }
   slock_unlock(running_lock);

   return false;
}

static void threaded_init(void)
{
   running_lock  = slock_new();
   finished_lock = slock_new();
   worker_cond   = scond_new();

   slock_lock(running_lock);
   worker_continue = true;
   slock_unlock(running_lock);

   worker_thread = sthread_create(threaded_worker, NULL);
}

static void threaded_deinit(void)
{
   slock_lock(running_lock);
   worker_continue = false;
   scond_signal(worker_cond);
   slock_unlock(running_lock);

   sthread_join(worker_thread);

   scond_free(worker_cond);
   slock_free(running_lock);
   slock_free(finished_lock);

   worker_thread = NULL;
   worker_cond   = NULL;
   running_lock  = NULL;
   finished_lock = NULL;
}

static struct rarch_task_impl impl_threaded = {
   threaded_push_running,
   threaded_reset,
   threaded_wait,
   threaded_gather,
   threaded_find,
   threaded_init,
   threaded_deinit
};
#endif

void rarch_task_init(void)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();
   if (settings->threaded_data_runloop_enable)
      impl_current = &impl_threaded;
   else
#endif
      impl_current = &impl_regular;

   impl_current->init();
}

void rarch_task_deinit(void)
{
   if (!impl_current)
      return;

   impl_current->deinit();
   impl_current = NULL;
}

void rarch_task_check(void)
{
#ifdef HAVE_THREADS
   bool current_threaded = impl_current == &impl_threaded;
   bool want_threaded = config_get_ptr()->threaded_data_runloop_enable;

   if (want_threaded != current_threaded) {
      RARCH_LOG("Switching rarch_task implementation.\n");
      rarch_task_deinit();
   }

   if (impl_current == NULL)
      rarch_task_init();
#endif

   impl_current->gather();
}

/* The lack of NULL checks in the following functions is proposital
 * to ensure correct control flow by the users. */
void rarch_task_push(rarch_task_t *task)
{
   impl_current->push_running(task);
}

void rarch_task_wait(void)
{
   impl_current->wait();
}

void rarch_task_reset(void)
{
   impl_current->reset();
}

bool rarch_task_find(rarch_task_finder_t func, void *user_data)
{
   return impl_current->find(func, user_data);
}
