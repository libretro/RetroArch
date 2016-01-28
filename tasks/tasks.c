/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdarg.h>

#include "../general.h"
#include "../verbosity.h"
#include "../msg_hash.h"
#include "tasks.h"

#ifdef HAVE_THREADS
#include "rthreads/rthreads.h"
#endif

typedef struct
{
   rarch_task_t *front;
   rarch_task_t *back;
} task_queue_t;

struct rarch_task_impl
{
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


static void task_queue_put(task_queue_t *queue, rarch_task_t *task)
{
   task->next = NULL;

   if (queue->front)
      queue->back->next = task;
   else
      queue->front = task;

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

void task_msg_queue_pushf(unsigned prio, unsigned duration,
      bool flush, const char *fmt, ...)
{
   char buf[1024];
   va_list ap;
   
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   runloop_msg_queue_push(buf, prio, duration, flush);
}

static void push_task_progress(rarch_task_t *task)
{
   if (task->title)
   {
      if (task->finished)
      {
         if (task->error)
            task_msg_queue_pushf(1, 60, true, "%s: %s",
               msg_hash_to_str(MSG_TASK_FAILED), task->title);
         else
            task_msg_queue_pushf(1, 60, true, "100%%: %s", task->title);
      }
      else
      {
         if (task->progress >= 0 && task->progress <= 100)
            task_msg_queue_pushf(1, 60, true, "%i%%: %s",
                  task->progress, task->title);
         else
            task_msg_queue_pushf(1, 60, true, "%s...", task->title);
      }
   }
}

static void rarch_task_internal_gather(void)
{
   rarch_task_t *task;
   while ((task = task_queue_get(&tasks_finished)) != NULL)
   {
      push_task_progress(task);

      if (task->callback)
         task->callback(task->task_data, task->user_data, task->error);

      if (task->error)
         free(task->error);

      if (task->title)
         free(task->title);

      free(task);
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

static void regular_reset(void)
{
   rarch_task_t *task = NULL;

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
   rarch_task_t *task = NULL;

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
   rarch_task_t *task = NULL;

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
   bool wait = false;

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
   rarch_task_t *task = NULL;

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
   rarch_task_t *task = NULL;

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

bool task_ctl(enum task_ctl_state state, void *data)
{
   static struct rarch_task_impl *impl_current = NULL;
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();
#endif

   switch (state)
   {
      case TASK_CTL_DEINIT:
         if (!impl_current)
            return false;

         impl_current->deinit();
         impl_current = NULL;
         break;
      case TASK_CTL_INIT:
         impl_current = &impl_regular;
#ifdef HAVE_THREADS
         if (settings->threaded_data_runloop_enable)
            impl_current = &impl_threaded;
#endif

         impl_current->init();
         break;
      case TASK_CTL_FIND:
         {
            task_finder_data_t *find_data = (task_finder_data_t*)data;
            if (!impl_current->find(find_data->func, find_data->userdata))
               return false;
         }
         break;
      case TASK_CTL_CHECK:
         {
#ifdef HAVE_THREADS
            bool current_threaded = (impl_current == &impl_threaded);
            bool want_threaded    = settings->threaded_data_runloop_enable;

            if (want_threaded != current_threaded)
            {
               RARCH_LOG("Switching rarch_task implementation.\n");
               task_ctl(TASK_CTL_DEINIT, NULL);
            }

            if (!impl_current)
               task_ctl(TASK_CTL_INIT, NULL);
#endif

            impl_current->gather();
         }
         break;
      case TASK_CTL_PUSH:
         {
            /* The lack of NULL checks in the following functions is proposital
             * to ensure correct control flow by the users. */
            rarch_task_t *task = (rarch_task_t*)data;
            impl_current->push_running(task);
            break;
         }
      case TASK_CTL_RESET:
         impl_current->reset();
         break;
      case TASK_CTL_WAIT:
         impl_current->wait();
         break;
      case TASK_CTL_NONE:
      default:
         break;
   }

   return true;
}
