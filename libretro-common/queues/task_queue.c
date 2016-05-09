/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (task_queue.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <queues/task_queue.h>

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
   void (*cancel)(void *);
   void (*reset)(void);
   void (*wait)(void);
   void (*gather)(void);
   bool (*find)(retro_task_finder_t, void*);
   void (*retrieve)(task_retriever_data_t *data);
   void (*init)(void);
   void (*deinit)(void);
};

static task_queue_t tasks_running  = {NULL, NULL};
static task_queue_t tasks_finished = {NULL, NULL};

#ifndef RARCH_INTERNAL
static void task_queue_msg_push(unsigned prio, unsigned duration,
      bool flush, const char *fmt, ...)
{
   char buf[1024];
   va_list ap;
   
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);

   /* print something here */
}

void task_queue_push_progress(retro_task_t *task)
{
   if (task->title)
   {
      if (task->finished)
      {
         if (task->error)
            task_queue_msg_push(1, 60, true, "%s: %s",
               "Task failed\n", task->title);
         else
            task_queue_msg_push(1, 60, true, "100%%: %s", task->title);
      }
      else
      {
         if (task->progress >= 0 && task->progress <= 100)
            task_queue_msg_push(1, 60, true, "%i%%: %s",
                  task->progress, task->title);
         else
            task_queue_msg_push(1, 60, true, "%s...", task->title);
      }
   }
}
#endif

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
      task_queue_push_progress(task);

      if (task->callback)
         task->callback(task->task_data, task->user_data, task->error);

      if (task->error)
         free(task->error);

      if (task->title)
         free(task->title);

      free(task);
   }
}

static void retro_task_regular_push_running(retro_task_t *task)
{
   task_queue_put(&tasks_running, task);
}

static void retro_task_regular_cancel(void *task)
{
   retro_task_t *t = (retro_task_t*)task;
   t->cancelled    = true;
}
 
static void retro_task_regular_gather(void)
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

      task_queue_push_progress(task);

      if (task->finished)
         task_queue_put(&tasks_finished, task);
      else
         retro_task_regular_push_running(task);
   }

   retro_task_internal_gather();
}

static void retro_task_regular_wait(void)
{
   while (tasks_running.front)
      retro_task_regular_gather();
}

static void retro_task_regular_reset(void)
{
   retro_task_t *task = tasks_running.front;

   for (; task; task = task->next)
      task->cancelled = true;
}

static void retro_task_regular_init(void)
{
}

static void retro_task_regular_deinit(void)
{
}

static bool retro_task_regular_find(retro_task_finder_t func, void *user_data)
{
   retro_task_t *task = tasks_running.front;

   for (; task; task = task->next)
   {
      if (func(task, user_data))
         return true;
   }

   return false;
}

static void retro_task_regular_retrieve(task_retriever_data_t *data)
{
   retro_task_t *task;
   task_retriever_info_t *info;
   task_retriever_info_t *tail = NULL;

   /* Parse all running tasks and handle matching handlers */
   for (task = tasks_running.front; task != NULL; task = task->next)
      if (task->handler == data->handler)
      {
         /* Create new link */
         info       = (task_retriever_info_t*)malloc(sizeof(task_retriever_info_t));
         info->data = malloc(data->element_size);
         info->next = NULL;

         /* Call retriever function and fill info-specific data */
         if (!data->func(task, info->data))
         {
            free(info->data);
            free(info);
            continue;
         }

         /* Add link to list */
         if (data->list == NULL)
         {
            data->list = info;
            tail = data->list;
         }
         else
         {
            tail->next = info;
            tail = tail->next;
         }
      }
}

static struct retro_task_impl impl_regular = {
   retro_task_regular_push_running,
   retro_task_regular_cancel,
   retro_task_regular_reset,
   retro_task_regular_wait,
   retro_task_regular_gather,
   retro_task_regular_find,
   retro_task_regular_retrieve,
   retro_task_regular_init,
   retro_task_regular_deinit
};

#ifdef HAVE_THREADS
static slock_t *running_lock    = NULL;
static slock_t *finished_lock   = NULL;
static scond_t *worker_cond     = NULL;
static sthread_t *worker_thread = NULL;
static bool worker_continue     = true; /* use running_lock when touching it */

static void task_queue_remove(task_queue_t *queue, retro_task_t *task)
{
   retro_task_t *t = NULL;

   /* Remove first element if needed */
   if (task == queue->front)
   {
      queue->front = task->next;
      task->next   = NULL;
      return;
   }

   /* Parse queue */
   t = queue->front;
   while (t && t->next)
   {
      /* Remove task and update queue */
      if (t->next == task)
      {
         t->next    = task->next;
         task->next = NULL;
         break;
      }

      /* Update iterator */
      t = t->next;
   }
}

static void retro_task_threaded_push_running(retro_task_t *task)
{
   slock_lock(running_lock);
   task_queue_put(&tasks_running, task);
   scond_signal(worker_cond);
   slock_unlock(running_lock);
}

static void retro_task_threaded_cancel(void *task)
{
   retro_task_t *t;

   slock_lock(running_lock);
   
   for (t = tasks_running.front; t; t = t->next)
   {
      if (t == task)
      {
        t->cancelled = true;
        break;
      }
   }

   slock_unlock(running_lock);
}

static void retro_task_threaded_gather(void)
{
   retro_task_t *task = NULL;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      task_queue_push_progress(task);

   slock_unlock(running_lock);

   slock_lock(finished_lock);
   retro_task_internal_gather();
   slock_unlock(finished_lock);
}

static void retro_task_threaded_wait(void)
{
   bool wait = false;

   do
   {
      retro_task_threaded_gather();

      slock_lock(running_lock);
      wait = (tasks_running.front != NULL);
      slock_unlock(running_lock);
   } while (wait);
}

static void retro_task_threaded_reset(void)
{
   retro_task_t *task = NULL;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      task->cancelled = true;
   slock_unlock(running_lock);
}


static bool retro_task_threaded_find(
      retro_task_finder_t func, void *user_data)
{
   retro_task_t *task = NULL;
   bool result = false;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
   {
      if (func(task, user_data))
      {
         result = true;
         break;
      }
   }
   slock_unlock(running_lock);

   return result;
}

static void retro_task_threaded_retrieve(task_retriever_data_t *data)
{
   /* Protect access to running tasks */
   slock_lock(running_lock);

   /* Call regular retrieve function */
   retro_task_regular_retrieve(data);

   /* Release access to running tasks */
   slock_unlock(running_lock);
}

static void threaded_worker(void *userdata)
{
   (void)userdata;

   for (;;)
   {
      retro_task_t *task  = NULL;

      if (!worker_continue)
         break; /* should we keep running until all tasks finished? */

      slock_lock(running_lock);

      /* Get first task to run */
      task = tasks_running.front;
      if (task == NULL)
      {
         scond_wait(worker_cond, running_lock);
         slock_unlock(running_lock);
         continue;
      }

      slock_unlock(running_lock);

      task->handler(task);

      slock_lock(running_lock);
      task_queue_remove(&tasks_running, task);
      slock_unlock(running_lock);

      /* Update queue */
      if (!task->finished)
      {
         /* Re-add task to running queue */
         retro_task_threaded_push_running(task);
      }
      else
      {
         /* Add task to finished queue */
         slock_lock(finished_lock);
         task_queue_put(&tasks_finished, task);
         slock_unlock(finished_lock);
      }

#if 0
      retro_sleep(10);
#endif
   }

   slock_unlock(running_lock);
}

static void retro_task_threaded_init(void)
{
   running_lock  = slock_new();
   finished_lock = slock_new();
   worker_cond   = scond_new();

   slock_lock(running_lock);
   worker_continue = true;
   slock_unlock(running_lock);

   worker_thread = sthread_create(threaded_worker, NULL);
}

static void retro_task_threaded_deinit(void)
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
   retro_task_threaded_push_running,
   retro_task_threaded_cancel,
   retro_task_threaded_reset,
   retro_task_threaded_wait,
   retro_task_threaded_gather,
   retro_task_threaded_find,
   retro_task_threaded_retrieve,
   retro_task_threaded_init,
   retro_task_threaded_deinit
};
#endif

bool task_queue_ctl(enum task_queue_ctl_state state, void *data)
{
   static struct retro_task_impl *impl_current = NULL;
   static bool task_threaded_enable            = false;

   switch (state)
   {
      case TASK_QUEUE_CTL_DEINIT:
         if (impl_current)
            impl_current->deinit();
         impl_current = NULL;
         break;
      case TASK_QUEUE_CTL_SET_THREADED:
         task_threaded_enable = true;
         break;
      case TASK_QUEUE_CTL_UNSET_THREADED:
         task_threaded_enable = false;
         break;
      case TASK_QUEUE_CTL_IS_THREADED:
         return task_threaded_enable;
      case TASK_QUEUE_CTL_INIT:
         {
#ifdef HAVE_THREADS
            bool *boolean_val = (bool*)data;
#endif

            impl_current = &impl_regular;
#ifdef HAVE_THREADS
            if (boolean_val && *boolean_val)
            {
               task_queue_ctl(TASK_QUEUE_CTL_SET_THREADED, NULL);
               impl_current = &impl_threaded;
            }
#endif

            impl_current->init();
         }
         break;
      case TASK_QUEUE_CTL_FIND:
         {
            task_finder_data_t *find_data = (task_finder_data_t*)data;
            if (!impl_current->find(find_data->func, find_data->userdata))
               return false;
         }
         break;
      case TASK_QUEUE_CTL_RETRIEVE:
         impl_current->retrieve((task_retriever_data_t*)data);
         break;
      case TASK_QUEUE_CTL_CHECK:
         {
#ifdef HAVE_THREADS
            bool current_threaded = (impl_current == &impl_threaded);
            bool want_threaded    = 
               task_queue_ctl(TASK_QUEUE_CTL_IS_THREADED, NULL);

            if (want_threaded != current_threaded)
               task_queue_ctl(TASK_QUEUE_CTL_DEINIT, NULL);

            if (!impl_current)
               task_queue_ctl(TASK_QUEUE_CTL_INIT, NULL);
#endif

            impl_current->gather();
         }
         break;
      case TASK_QUEUE_CTL_PUSH:
         {
            /* The lack of NULL checks in the following functions 
             * is proposital to ensure correct control flow by the users. */
            retro_task_t *task = (retro_task_t*)data;
            impl_current->push_running(task);
            break;
         }
      case TASK_QUEUE_CTL_RESET:
         impl_current->reset();
         break;
      case TASK_QUEUE_CTL_WAIT:
         impl_current->wait();
         break;
      case TASK_QUEUE_CTL_CANCEL:
         impl_current->cancel(data);
         break;
      case TASK_QUEUE_CTL_NONE:
      default:
         break;
   }

   return true;
}

void task_queue_cancel_task(void *task)
{
   task_queue_ctl(TASK_QUEUE_CTL_CANCEL, task);
}

void *task_queue_retriever_info_next(task_retriever_info_t **link)
{
   void *data = NULL;

   /* Grab data and move to next link */
   if (*link)
   {
      data = (*link)->data;
      *link = (*link)->next;
   }

   return data;
}

void task_queue_retriever_info_free(task_retriever_info_t *list)
{
   task_retriever_info_t *info;

   /* Free links including retriever-specific data */
   while (list)
   {
      info = list->next;
      free(list->data);
      free(list);
      list = info;
   }
}

