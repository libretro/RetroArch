/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <features/features_cpu.h>

#if defined(HAVE_GCD) && !defined(HAVE_THREADS)
#error "gcd uses threads, what are you doing"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef HAVE_GCD
#include <dispatch/dispatch.h>
#endif

typedef struct
{
   retro_task_t *front;
   retro_task_t *back;
} task_queue_t;

struct retro_task_impl
{
   retro_task_queue_msg_t msg_push;
   void (*push_running)(retro_task_t *);
   void (*cancel)(void *);
   void (*reset)(void);
   void (*wait)(retro_task_condition_fn_t, void *);
   void (*gather)(void);
   bool (*find)(retro_task_finder_t, void*);
   void (*retrieve)(task_retriever_data_t *data);
   void (*init)(void);
   void (*deinit)(void);
};

/* TODO/FIXME - static globals */
static retro_task_queue_msg_t msg_push_bak  = NULL;
static task_queue_t tasks_running           = {NULL, NULL};
static task_queue_t tasks_finished          = {NULL, NULL};

static struct retro_task_impl *impl_current = NULL;
static bool task_threaded_enable            = false;

#ifdef HAVE_THREADS
static uintptr_t main_thread_id             = 0;
static slock_t *running_lock                = NULL;
static slock_t *finished_lock               = NULL;
static slock_t *property_lock               = NULL;
static slock_t *queue_lock                  = NULL;
static scond_t *worker_cond                 = NULL;
static sthread_t *worker_thread             = NULL;
static bool worker_continue                 = true;
/* use running_lock when touching it */
#endif

#ifdef HAVE_GCD
static unsigned gcd_queue_count             = 0;
#endif

static void task_queue_msg_push(retro_task_t *task,
      unsigned prio, unsigned duration,
      bool flush, const char *fmt, ...)
{
   char buf[1024];
   va_list ap;

   buf[0] = '\0';

   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);

   if (impl_current->msg_push)
      impl_current->msg_push(task, buf, prio, duration, flush);
}

static void task_queue_push_progress(retro_task_t *task)
{
#ifdef HAVE_THREADS
   /* msg_push callback interacts directly with the task properties (particularly title).
    * make sure another thread doesn't modify them while rendering
    */
   slock_lock(property_lock);
#endif

   if (task->title && (!((task->flags & RETRO_TASK_FLG_MUTE) > 0)))
   {
      if ((task->flags & RETRO_TASK_FLG_FINISHED) > 0)
      {
         if (task->error)
            task_queue_msg_push(task, 1, 60, true, "%s: %s",
               "Task failed", task->title);
         else
            task_queue_msg_push(task, 1, 60, false, "100%%: %s", task->title);
      }
      else
      {
         if (task->progress >= 0 && task->progress <= 100)
            task_queue_msg_push(task, 1, 60, true, "%i%%: %s",
                  task->progress, task->title);
         else
            task_queue_msg_push(task, 1, 60, false, "%s...", task->title);
      }

      if (task->progress_cb)
         task->progress_cb(task);
   }

#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

static void task_queue_put(task_queue_t *queue, retro_task_t *task)
{
   task->next                   = NULL;

   if (queue->front)
   {
      /* Make sure to insert in order - the queue is
       * sorted by 'when' so items that aren't scheduled
       * to run immediately are at the back of the queue.
       * Items with the same 'when' are inserted after
       * all the other items with the same 'when'.
       * This primarily affects items with a 'when' of 0.
       */
      if (queue->back)
      {
         if (queue->back->when > task->when)
         {
            retro_task_t** prev = &queue->front;
            while (*prev && (*prev)->when <= task->when)
               prev             = &((*prev)->next);

            task->next          = *prev;
            *prev               = task;
            return;
         }

         queue->back->next      = task;
      }
   }
   else
      queue->front              = task;

   queue->back                  = task;
}

static retro_task_t *task_queue_get(task_queue_t *queue)
{
   retro_task_t *task = queue->front;

   if (task)
   {
      queue->front = task->next;
      task->next   = NULL;
   }

   return task;
}

static void retro_task_internal_gather(void)
{
   retro_task_t *task = NULL;
   while ((task = task_queue_get(&tasks_finished)))
   {
      task_queue_push_progress(task);

      if (task->callback)
         task->callback(task, task->task_data, task->user_data, task->error);

      if (task->cleanup)
          task->cleanup(task);

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
   t->flags       |= RETRO_TASK_FLG_CANCELLED;
}

static void retro_task_regular_gather(void)
{
   retro_task_t *task  = NULL;
   retro_task_t *queue = NULL;
   retro_task_t *next  = NULL;

   while ((task = task_queue_get(&tasks_running)))
   {
      task->next = queue;
      queue = task;
   }

   for (task = queue; task; task = next)
   {
      next = task->next;

      if (!task->when || task->when < cpu_features_get_time_usec())
      {
         task->handler(task);

         task_queue_push_progress(task);
      }

      if ((task->flags & RETRO_TASK_FLG_FINISHED) > 0)
         task_queue_put(&tasks_finished, task);
      else
         task_queue_put(&tasks_running, task);
   }

   retro_task_internal_gather();
}

static void retro_task_regular_wait(retro_task_condition_fn_t cond, void* data)
{
   while ((tasks_running.front && !tasks_running.front->when) && (!cond || cond(data)))
      retro_task_regular_gather();
}

static void retro_task_regular_reset(void)
{
   retro_task_t *task = tasks_running.front;

   for (; task; task = task->next)
      task->flags |= RETRO_TASK_FLG_CANCELLED;
}

static void retro_task_regular_init(void) { }
static void retro_task_regular_deinit(void) { }

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
   retro_task_t *task          = NULL;
   task_retriever_info_t *tail = NULL;

   /* Parse all running tasks and handle matching handlers */
   for (task = tasks_running.front; task != NULL; task = task->next)
   {
      task_retriever_info_t *info = NULL;
      if (task->handler != data->handler)
         continue;

      /* Create new link */
      info       = (task_retriever_info_t*)
         malloc(sizeof(task_retriever_info_t));
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
      if (data->list)
      {
         if (tail)
         {
            tail->next = info;
            tail       = tail->next;
         }
         else
            tail       = info;
      }
      else
      {
         data->list    = info;
         tail          = data->list;
      }
   }
}

static struct retro_task_impl impl_regular = {
   NULL,
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

/* 'queue_lock' must be held for the duration of this function */
static void task_queue_remove(task_queue_t *queue, retro_task_t *task)
{
   retro_task_t     *t = NULL;
   retro_task_t *front = queue->front;

   /* Remove first element if needed */
   if (task == front)
   {
      queue->front     = task->next;
      if (queue->back == task) /* if only element, also update back */
         queue->back   = NULL;
      task->next       = NULL;
      return;
   }

   /* Parse queue */
   t = front;

   while (t && t->next)
   {
      /* Remove task and update queue */
      if (t->next == task)
      {
         t->next    = task->next;
         task->next = NULL;

         /* When removing the tail of the queue, update the tail pointer */
         if (queue->back == task)
         {
            if (queue->back == task)
               queue->back = t;
         }
         break;
      }

      /* Update iterator */
      t = t->next;
   }
}

static void retro_task_threaded_push_running(retro_task_t *task)
{
   slock_lock(running_lock);
   slock_lock(queue_lock);
   task_queue_put(&tasks_running, task);
   scond_signal(worker_cond);
   slock_unlock(queue_lock);
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
        t->flags |= RETRO_TASK_FLG_CANCELLED;
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

static void retro_task_threaded_wait(retro_task_condition_fn_t cond, void* data)
{
   bool wait = false;

   do
   {
      retro_task_threaded_gather();

      slock_lock(running_lock);
      wait = (tasks_running.front && !tasks_running.front->when);
      slock_unlock(running_lock);

      if (!wait)
      {
         slock_lock(finished_lock);
         wait = (tasks_finished.front && !tasks_finished.front->when);
         slock_unlock(finished_lock);
      }
   } while (wait && (!cond || cond(data)));
}

static void retro_task_threaded_reset(void)
{
   retro_task_t *task = NULL;

   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      task->flags |= RETRO_TASK_FLG_CANCELLED;
   slock_unlock(running_lock);
}

static bool retro_task_threaded_find(
      retro_task_finder_t func, void *user_data)
{
   retro_task_t *task = NULL;
   bool        result = false;

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
   for (;;)
   {
      retro_task_t *task  = NULL;
      bool       finished = false;

      slock_lock(running_lock);

      if (!worker_continue)
      {
         slock_unlock(running_lock);
         break; /* should we keep running until all tasks finished? */
      }

      /* Get first task to run */
      if (!(task = tasks_running.front))
      {
         scond_wait(worker_cond, running_lock);
         slock_unlock(running_lock);
         continue;
      }

      if (task->when)
      {
         retro_time_t now   = cpu_features_get_time_usec();
         retro_time_t delay = task->when - now - 500; /* allow half a millisecond for context switching */
         if (delay > 0)
         {
            scond_wait_timeout(worker_cond, running_lock, delay);
            slock_unlock(running_lock);
            continue;
         }
      }

      slock_unlock(running_lock);

      task->handler(task);

      slock_lock(property_lock);
      finished = ((task->flags & RETRO_TASK_FLG_FINISHED) > 0) ? true : false;
      slock_unlock(property_lock);

      /* Update queue */
      if (!finished)
      {
         /* Move the task to the back of the queue */
         /* mimics retro_task_threaded_push_running,
          * but also includes a task_queue_remove */
         slock_lock(running_lock);
         slock_lock(queue_lock);

         /* do nothing if only item in queue */
         if (task->next)
         {
            task_queue_remove(&tasks_running, task);
            task_queue_put(&tasks_running, task);
            scond_signal(worker_cond);
         }
         slock_unlock(queue_lock);
         slock_unlock(running_lock);
      }
      else
      {
         /* Remove task from running queue */
         slock_lock(running_lock);
         slock_lock(queue_lock);
         task_queue_remove(&tasks_running, task);
         slock_unlock(queue_lock);
         slock_unlock(running_lock);

         /* Add task to finished queue */
         slock_lock(finished_lock);
         task_queue_put(&tasks_finished, task);
         slock_unlock(finished_lock);
      }
   }
}

static void retro_task_threaded_init(void)
{
   running_lock    = slock_new();
   finished_lock   = slock_new();
   property_lock   = slock_new();
   queue_lock      = slock_new();
   worker_cond     = scond_new();

   slock_lock(running_lock);
   worker_continue = true;
   slock_unlock(running_lock);

   worker_thread   = sthread_create(threaded_worker, NULL);
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
   slock_free(property_lock);
   slock_free(queue_lock);

   worker_thread   = NULL;
   worker_cond     = NULL;
   running_lock    = NULL;
   finished_lock   = NULL;
   property_lock   = NULL;
   queue_lock      = NULL;
}

static struct retro_task_impl impl_threaded = {
   NULL,
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

#ifdef HAVE_GCD

static void gcd_worker(retro_task_t *task)
{
   bool       finished = false;
   slock_lock(running_lock);

   if (!worker_continue)
   {
      gcd_queue_count--;
      if (!gcd_queue_count)
         scond_signal(worker_cond);
      slock_unlock(running_lock);
      return;
   }

   if (task->when)
   {
      retro_time_t now   = cpu_features_get_time_usec();
      retro_time_t delay = task->when - now - 500;
      if (delay > 0)
      {
         dispatch_time_t after = dispatch_time(DISPATCH_TIME_NOW, delay);
         dispatch_after(after, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                        ^{ gcd_worker(task); });
         slock_unlock(running_lock);
         return;
      }
   }

   slock_unlock(running_lock);

   task->handler(task);

   slock_lock(property_lock);
   finished = ((task->flags & RETRO_TASK_FLG_FINISHED) > 0) ? true : false;
   slock_unlock(property_lock);

   if (!finished)
      dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                     ^{ gcd_worker(task); });
   else
   {
      /* Remove task from running queue */
      slock_lock(running_lock);
      slock_lock(queue_lock);
      gcd_queue_count--;
      if (!gcd_queue_count)
         scond_signal(worker_cond);
      task_queue_remove(&tasks_running, task);
      slock_unlock(queue_lock);
      slock_unlock(running_lock);

      /* Add task to finished queue */
      slock_lock(finished_lock);
      task_queue_put(&tasks_finished, task);
      slock_unlock(finished_lock);
   }
}

static void retro_task_gcd_push_running(retro_task_t *task)
{
   slock_lock(running_lock);
   slock_lock(queue_lock);
   task_queue_put(&tasks_running, task);
   gcd_queue_count++;
   dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                  ^{ gcd_worker(task); });
   slock_unlock(queue_lock);
   slock_unlock(running_lock);
}

static void retro_task_gcd_wait(retro_task_condition_fn_t cond, void* data)
{
   bool wait = false;

   do
   {
      retro_task_t *task = NULL;
      retro_task_threaded_gather();

      slock_lock(running_lock);
      wait = false;
      /* can't just look at the first task like threaded, they're not sorted by when */
      for (task = tasks_running.front; !wait && task; task = task->next)
         wait |= !task->when;
      slock_unlock(running_lock);

      if (!wait)
      {
         slock_lock(finished_lock);
         for (task = tasks_finished.front; !wait && task; task = task->next)
            wait |= !task->when;
         slock_unlock(finished_lock);
      }
   } while (wait && (!cond || cond(data)));
}

static void retro_task_gcd_init(void)
{
   retro_task_t *task = NULL;

   running_lock    = slock_new();
   finished_lock   = slock_new();
   property_lock   = slock_new();
   queue_lock      = slock_new();
   worker_cond     = scond_new();

   slock_lock(running_lock);
   worker_continue = true;
   for (task = tasks_running.front; task; task = task->next)
   {
      gcd_queue_count++;
      dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                     ^{ gcd_worker(task); });
   };
   slock_unlock(running_lock);
}

static void retro_task_gcd_deinit(void)
{
   slock_lock(running_lock);
   worker_continue = false;
   if (gcd_queue_count)
      scond_wait(worker_cond, running_lock);
   slock_unlock(running_lock);

   scond_free(worker_cond);
   slock_free(running_lock);
   slock_free(finished_lock);
   slock_free(property_lock);
   slock_free(queue_lock);

   worker_cond     = NULL;
   running_lock    = NULL;
   finished_lock   = NULL;
   property_lock   = NULL;
   queue_lock      = NULL;
}

static struct retro_task_impl impl_gcd = {
   NULL,
   retro_task_gcd_push_running,
   retro_task_threaded_cancel,
   retro_task_threaded_reset,
   retro_task_gcd_wait,
   retro_task_threaded_gather,
   retro_task_threaded_find,
   retro_task_threaded_retrieve,
   retro_task_gcd_init,
   retro_task_gcd_deinit
};
#endif

/* Deinitializes the task system.
 * This deinitializes the task system.
 * The tasks that are running at
 * the moment will stay on hold */
void task_queue_deinit(void)
{
   if (impl_current)
      impl_current->deinit();
   impl_current = NULL;
}

void task_queue_init(bool threaded, retro_task_queue_msg_t msg_push)
{
   impl_current   = &impl_regular;
#ifdef HAVE_THREADS
   main_thread_id = sthread_get_current_thread_id();
   if (threaded)
   {
      task_threaded_enable = true;
#ifdef HAVE_GCD
      impl_current         = &impl_gcd;
#else
      impl_current         = &impl_threaded;
#endif
   }
#endif

   msg_push_bak            = msg_push;

   impl_current->msg_push  = msg_push;
   impl_current->init();
}

void task_queue_set_threaded(void)
{
   task_threaded_enable = true;
}

void task_queue_unset_threaded(void)
{
   task_threaded_enable = false;
}

bool task_queue_is_threaded(void)
{
   return task_threaded_enable;
}

bool task_queue_find(task_finder_data_t *find_data)
{
   return impl_current->find(find_data->func, find_data->userdata);
}

void task_queue_retrieve(task_retriever_data_t *data)
{
   impl_current->retrieve(data);
}

void task_queue_check(void)
{
#ifdef HAVE_THREADS
   bool current_threaded = (impl_current != &impl_regular);
   bool want_threaded    = task_threaded_enable;

   if (want_threaded != current_threaded)
      task_queue_deinit();

   if (!impl_current)
      task_queue_init(want_threaded, msg_push_bak);
#endif

   impl_current->gather();
}

bool task_queue_push(retro_task_t *task)
{
   /* Ignore this task if a related one is already running */
   if (task->type == TASK_TYPE_BLOCKING)
   {
      retro_task_t *running = NULL;
      bool            found = false;

#ifdef HAVE_THREADS
      slock_lock(queue_lock);
#endif
      running = tasks_running.front;

      for (; running; running = running->next)
      {
         if (running->type == TASK_TYPE_BLOCKING)
         {
            found = true;
            break;
         }
      }

#ifdef HAVE_THREADS
      slock_unlock(queue_lock);
#endif

      /* skip this task, user must try again later */
      if (found)
         return false;
   }

   /* The lack of NULL checks in the following functions
    * is proposital to ensure correct control flow by the users. */
   impl_current->push_running(task);

   return true;
}

void task_queue_wait(retro_task_condition_fn_t cond, void* data)
{
   impl_current->wait(cond, data);
}

void task_queue_reset(void)
{
   impl_current->reset();
}

/**
 * Signals a task to end without waiting for
 * it to complete. */
void task_queue_cancel_task(void *task)
{
   impl_current->cancel(task);
}

void *task_queue_retriever_info_next(task_retriever_info_t **link)
{
   /* Grab data and move to next link */
   if (*link)
   {
      *link = (*link)->next;
      return (*link)->data;
   }
   return NULL;
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

bool task_is_on_main_thread(void)
{
#ifdef HAVE_THREADS
   return sthread_get_current_thread_id() == main_thread_id;
#else
   return true;
#endif
}

void task_set_error(retro_task_t *task, char *error)
{
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   task->error = error;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

void task_set_progress(retro_task_t *task, int8_t progress)
{
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   task->progress = progress;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

void task_set_title(retro_task_t *task, char *title)
{
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   task->title = title;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

void task_set_data(retro_task_t *task, void *data)
{
#ifdef HAVE_THREADS
   slock_lock(running_lock);
#endif
   task->task_data = data;
#ifdef HAVE_THREADS
   slock_unlock(running_lock);
#endif
}

void task_free_title(retro_task_t *task)
{
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   if (task->title)
      free(task->title);
   task->title = NULL;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

void* task_get_data(retro_task_t *task)
{
   void *data = NULL;

#ifdef HAVE_THREADS
   slock_lock(running_lock);
#endif
   data = task->task_data;
#ifdef HAVE_THREADS
   slock_unlock(running_lock);
#endif

   return data;
}

void task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   if (set)
      task->flags |=  (flags);
   else
      task->flags &= ~(flags);
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

uint8_t task_get_flags(retro_task_t *task)
{
   uint8_t _flags = 0;
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   _flags = task->flags;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
   return _flags;
}

char* task_get_error(retro_task_t *task)
{
   char *error = NULL;

#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   error = task->error;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif

   return error;
}

int8_t task_get_progress(retro_task_t *task)
{
   int8_t progress = 0;

#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   progress = task->progress;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif

   return progress;
}

char* task_get_title(retro_task_t *task)
{
   char *title = NULL;

#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   title = task->title;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif

   return title;
}

retro_task_t *task_init(void)
{
   /* TODO/FIXME - static local global */
   static uint32_t task_count = 0;
   retro_task_t *task         = (retro_task_t*)malloc(sizeof(*task));

   if (!task)
      return NULL;

   task->handler           = NULL;
   task->callback          = NULL;
   task->cleanup           = NULL;
   task->flags             = 0;
   task->task_data         = NULL;
   task->user_data         = NULL;
   task->state             = NULL;
   task->error             = NULL;
   task->progress          = 0;
   task->progress_cb       = NULL;
   task->title             = NULL;
   task->type              = TASK_TYPE_NONE;
   task->style             = TASK_STYLE_NONE;
   task->ident             = task_count++;
   task->frontend_userdata = NULL;
   task->next              = NULL;
   task->when              = 0;

   return task;
}
