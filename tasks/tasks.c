
#include <stdlib.h>

#include "../general.h"
#include "../verbosity.h"
#include "tasks.h"

#ifdef HAVE_THREADS
#include "rthreads/rthreads.h"
#endif

static rarch_task_t *running_queue  = NULL;
static rarch_task_t *finished_queue = NULL;
static bool thread_enabled = false;

struct rarch_task_impl {
   void (*push_running)(rarch_task_t *);
   void (*push_finished)(rarch_task_t *);
   void (*gather)(void);
   void (*init)(void);
   void (*deinit)(void);
};

static struct rarch_task_impl *impl_current = NULL;

static void _push_task(rarch_task_t **q, rarch_task_t *task)
{
   if (*q == NULL)
      *q = task;
   else
   {
      rarch_task_t *last = *q;
      while (last->next)
         last = last->next;

      last->next = task;
   }

   task->next = NULL;
}

static rarch_task_t *_pop_task(rarch_task_t **q)
{
   rarch_task_t *task = *q;

   if (*q == NULL)
      return NULL;

   *q = task->next;
   task->next = NULL;

   return task;
}

static void rarch_task_internal_gather(void)
{
   rarch_task_t *next = NULL;
   while ((next = _pop_task(&finished_queue)) != NULL)
   {
      rarch_task_t *task = next;
      next = next->next;

      if (task->callback)
         task->callback(task->task_data, task->user_data, task->error);

      if (task->error)
         free(task->error);

      free(task);
   }
}

static void regular_push_finished(rarch_task_t *task)
{
   _push_task(&finished_queue, task);
}

static void regular_push_running(rarch_task_t *task)
{
   _push_task(&running_queue, task);
}

static void regular_gather(void)
{
   rarch_task_t *task  = NULL;
   rarch_task_t *queue = NULL;
   rarch_task_t *next  = NULL;

   /* mimics threaded_gather() for compatibility, a faster implementation
    * can be written for systems without HAVE_THREADS if necessary. */
   while ((task = _pop_task(&running_queue)) != NULL)
   {
      task->next = queue;
      queue = task;
   }

   for (task = queue; task; task = next)
   {
      next = task->next;
      task->handler(task);

      if (task->finished)
         regular_push_finished(task);
      else
         regular_push_running(task);
   }

   rarch_task_internal_gather();
}

static void regular_init(void)
{
}

static void regular_deinit(void)
{
}

static struct rarch_task_impl impl_regular = {
   regular_push_running,
   regular_push_finished,
   regular_gather,
   regular_init,
   regular_deinit
};

#ifdef HAVE_THREADS
static slock_t *running_lock  = NULL;
static slock_t *finished_lock = NULL;
static scond_t *worker_cond   = NULL;
static sthread_t *worker_thread = NULL;
static bool worker_continue = true; /* use running_lock when touching it */

static void threaded_push_finished(rarch_task_t *task)
{
   slock_lock(finished_lock);
   _push_task(&finished_queue, task);
   slock_unlock(finished_lock);
}

static void threaded_push_running(rarch_task_t *task)
{
   slock_lock(running_lock);
   _push_task(&running_queue, task);
   scond_signal(worker_cond);
   slock_unlock(running_lock);
}

static void threaded_gather(void)
{
   slock_lock(finished_lock);
   rarch_task_internal_gather();
   slock_unlock(finished_lock);
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

      while ((task = _pop_task(&running_queue)) != NULL)
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
            threaded_push_finished(task);
         else
            threaded_push_running(task);
      }
   }

   slock_unlock(running_lock);
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
   threaded_push_finished,
   threaded_gather,
   threaded_init,
   threaded_deinit
};
#endif

void rarch_task_init(void)
{
#ifdef HAVE_THREADS
   if (config_get_ptr()->threaded_data_runloop_enable)
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

void rarch_task_push(rarch_task_t *task)
{
   impl_current->push_running(task);
}
