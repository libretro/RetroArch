/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Higor Euripedes
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "tasks.h"

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

typedef struct
{
   retro_task_t *front;
   retro_task_t *back;
} task_queue_t;

struct retro_task_impl
{
   void (*push_running)(retro_task_t *);
   void (*reset)(void);
   void (*wait)(void);
   void (*gather)(void);
   bool (*find)(retro_task_finder_t, void*);
   void (*init)(void);
   void (*deinit)(void);
};

static task_queue_t tasks_running  = {NULL, NULL};
static task_queue_t tasks_finished = {NULL, NULL};

static void task_queue_put(task_queue_t *queue, retro_task_t *task)
{
   task->next = NULL;

   if (queue->front)
      queue->back->next = task;
   else
      queue->front = task;

   queue->back = task;
}

static retro_task_t *task_queue_get(task_queue_t *queue)
{
   retro_task_t *task = queue->front;

   if (task)
   {
      queue->front = task->next;
      task->next = NULL;
   }

   return task;
}

static void retro_task_internal_gather(void)
{
   retro_task_t *task = NULL;
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

static void regular_push_running(retro_task_t *task)
{
   task_queue_put(&tasks_running, task);
}

static void regular_gather(void)
{
   retro_task_t *task  = NULL;
   retro_task_t *queue = NULL;
   retro_task_t *next  = NULL;

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

   retro_task_internal_gather();
}

static void regular_wait(void)
{
   while (tasks_running.front)
      regular_gather();
}

static void regular_reset(void)
{
   retro_task_t *task = tasks_running.front;

   for (; task; task = task->next)
      task->cancelled = true;
}

static void regular_init(void)
{
}

static void regular_deinit(void)
{
}

static bool regular_find(retro_task_finder_t func, void *user_data)
{
   retro_task_t *task = tasks_running.front;

   for (; task; task = task->next)
   {
      if (func(task, user_data))
         return true;
   }

   return false;
}

static struct retro_task_impl impl_regular = {
   regular_push_running,
   regular_reset,
   regular_wait,
   regular_gather,
   regular_find,
   regular_init,
   regular_deinit
};

#ifdef HAVE_THREADS
static slock_t *running_lock    = NULL;
static slock_t *finished_lock   = NULL;
static scond_t *worker_cond     = NULL;
static sthread_t *worker_thread = NULL;
static bool worker_continue     = true; /* use running_lock when touching it */

static void threaded_push_running(retro_task_t *task)
{
   slock_lock(running_lock);
   task_queue_put(&tasks_running, task);
   scond_signal(worker_cond);
   slock_unlock(running_lock);
}

static void threaded_gather(void)
{
   retro_task_t *task = NULL;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      push_task_progress(task);

   slock_unlock(running_lock);

   slock_lock(finished_lock);
   retro_task_internal_gather();
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
   retro_task_t *task = NULL;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      task->cancelled = true;
   slock_unlock(running_lock);
}

static void threaded_worker(void *userdata)
{
   (void)userdata;

   for (;;)
   {
      retro_task_t *queue = NULL;
      retro_task_t *task  = NULL;
      retro_task_t *next  = NULL;

      /* pop all into a local queue,
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

static bool threaded_find(retro_task_finder_t func, void *user_data)
{
   retro_task_t *task = NULL;

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

static struct retro_task_impl impl_threaded = {
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
   static struct retro_task_impl *impl_current = NULL;
   static bool task_threaded_enable            = false;

   switch (state)
   {
      case TASK_CTL_DEINIT:
         if (impl_current)
            impl_current->deinit();
         impl_current = NULL;
         break;
      case TASK_CTL_SET_THREADED:
         task_threaded_enable = true;
         break;
      case TASK_CTL_UNSET_THREADED:
         task_threaded_enable = false;
         break;
      case TASK_CTL_IS_THREADED:
         return task_threaded_enable;
      case TASK_CTL_INIT:
         {
            bool *boolean_val = (bool*)data;

            impl_current = &impl_regular;
#ifdef HAVE_THREADS
            if (*boolean_val)
            {
               task_ctl(TASK_CTL_SET_THREADED, NULL);
               impl_current = &impl_threaded;
            }
#endif

            impl_current->init();
         }
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
            bool want_threaded    = task_ctl(TASK_CTL_IS_THREADED, NULL);

            if (want_threaded != current_threaded)
               task_ctl(TASK_CTL_DEINIT, NULL);

            if (!impl_current)
               task_ctl(TASK_CTL_INIT, NULL);
#endif

            impl_current->gather();
         }
         break;
      case TASK_CTL_PUSH:
         {
            /* The lack of NULL checks in the following functions 
             * is proposital to ensure correct control flow by the users. */
            retro_task_t *task = (retro_task_t*)data;
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
