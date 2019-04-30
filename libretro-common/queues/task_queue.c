/* Copyright  (C) 2010-2018 The RetroArch team
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
#define SLOCK_LOCK(x) slock_lock(x)
#define SLOCK_UNLOCK(x) slock_unlock(x)
#else
#define SLOCK_LOCK(x)
#define SLOCK_UNLOCK(x)
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

static retro_task_queue_msg_t msg_push_bak;
static task_queue_t tasks_running  = {NULL, NULL};
static task_queue_t tasks_finished = {NULL, NULL};

static struct retro_task_impl *impl_current = NULL;
static bool task_threaded_enable            = false;

static uint32_t task_count                  = 0;

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
   if (task->title && !task->mute)
   {
      if (task->finished)
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
}

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

static void retro_task_regular_wait(retro_task_condition_fn_t cond, void* data)
{
   while (tasks_running.front && (!cond || cond(data)))
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
   retro_task_t *task          = NULL;
   task_retriever_info_t *tail = NULL;

   /* Parse all running tasks and handle matching handlers */
   for (task = tasks_running.front; task != NULL; task = task->next)
   {
      task_retriever_info_t *info = NULL;
      if (task->handler != data->handler)
         continue;

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
static slock_t *running_lock    = NULL;
static slock_t *finished_lock   = NULL;
static slock_t *property_lock   = NULL;
static slock_t *queue_lock      = NULL;
static scond_t *worker_cond     = NULL;
static sthread_t *worker_thread = NULL;
static bool worker_continue     = true; /* use running_lock when touching it */

static void task_queue_remove(task_queue_t *queue, retro_task_t *task)
{
   retro_task_t     *t = NULL;
   retro_task_t *front = NULL;

   slock_lock(queue_lock);
   front = queue->front;
   slock_unlock(queue_lock);

   /* Remove first element if needed */
   if (task == front)
   {
      slock_lock(queue_lock);
      queue->front = task->next;
      slock_unlock(queue_lock);
      task->next   = NULL;

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
        t->cancelled = true;
        break;
      }
   }

   slock_unlock(running_lock);
}

static void retro_task_threaded_gather(void)
{
   retro_task_t *task = NULL;

   slock_lock(property_lock);
   slock_lock(running_lock);
   for (task = tasks_running.front; task; task = task->next)
      task_queue_push_progress(task);

   slock_unlock(running_lock);

   slock_lock(finished_lock);
   retro_task_internal_gather();
   slock_unlock(finished_lock);
   slock_unlock(property_lock);
}

static void retro_task_threaded_wait(retro_task_condition_fn_t cond, void* data)
{
   bool wait = false;

   do
   {
      retro_task_threaded_gather();

      slock_lock(running_lock);
      wait = (tasks_running.front != NULL) &&
             (!cond || cond(data));
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
      bool finished = false;

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

      slock_lock(property_lock);
      finished = task->finished;
      slock_unlock(property_lock);

      slock_lock(running_lock);
      task_queue_remove(&tasks_running, task);
      slock_unlock(running_lock);

      /* Update queue */
      if (!finished)
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
   }
}

static void retro_task_threaded_init(void)
{
   running_lock  = slock_new();
   finished_lock = slock_new();
   property_lock = slock_new();
   queue_lock    = slock_new();
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
   slock_free(property_lock);
   slock_free(queue_lock);

   worker_thread = NULL;
   worker_cond   = NULL;
   running_lock  = NULL;
   finished_lock = NULL;
   property_lock = NULL;
   queue_lock = NULL;
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
   impl_current = &impl_regular;

#ifdef HAVE_THREADS
   if (threaded)
   {
      task_threaded_enable = true;
      impl_current         = &impl_threaded;
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
   if (!impl_current->find(find_data->func, find_data->userdata))
      return false;
   return true;
}

void task_queue_retrieve(task_retriever_data_t *data)
{
   impl_current->retrieve(data);
}

void task_queue_check(void)
{
#ifdef HAVE_THREADS
   bool current_threaded = (impl_current == &impl_threaded);
   bool want_threaded    = task_queue_is_threaded();

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
      bool found = false;

      SLOCK_LOCK(queue_lock);
      running = tasks_running.front;

      for (; running; running = running->next)
      {
         if (running->type == TASK_TYPE_BLOCKING)
         {
            found = true;
            break;
         }
      }

      SLOCK_UNLOCK(queue_lock);

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

void task_set_finished(retro_task_t *task, bool finished)
{
   SLOCK_LOCK(property_lock);
   task->finished = finished;
   SLOCK_UNLOCK(property_lock);
}

void task_set_mute(retro_task_t *task, bool mute)
{
   SLOCK_LOCK(property_lock);
   task->mute = mute;
   SLOCK_UNLOCK(property_lock);
}

void task_set_error(retro_task_t *task, char *error)
{
   SLOCK_LOCK(property_lock);
   task->error = error;
   SLOCK_UNLOCK(property_lock);
}

void task_set_progress(retro_task_t *task, int8_t progress)
{
   SLOCK_LOCK(property_lock);
   task->progress = progress;
   SLOCK_UNLOCK(property_lock);
}

void task_set_title(retro_task_t *task, char *title)
{
   SLOCK_LOCK(property_lock);
   task->title = title;
   SLOCK_UNLOCK(property_lock);
}

void task_set_data(retro_task_t *task, void *data)
{
   SLOCK_LOCK(running_lock);
   task->task_data = data;
   SLOCK_UNLOCK(running_lock);
}

void task_set_cancelled(retro_task_t *task, bool cancelled)
{
   SLOCK_LOCK(running_lock);
   task->cancelled = cancelled;
   SLOCK_UNLOCK(running_lock);
}

void task_free_title(retro_task_t *task)
{
   SLOCK_LOCK(property_lock);
   if (task->title)
      free(task->title);
   task->title = NULL;
   SLOCK_UNLOCK(property_lock);
}

void* task_get_data(retro_task_t *task)
{
   void *data = NULL;

   SLOCK_LOCK(running_lock);
   data = task->task_data;
   SLOCK_UNLOCK(running_lock);

   return data;
}

bool task_get_cancelled(retro_task_t *task)
{
   bool cancelled = false;

   SLOCK_LOCK(running_lock);
   cancelled = task->cancelled;
   SLOCK_UNLOCK(running_lock);

   return cancelled;
}

bool task_get_finished(retro_task_t *task)
{
   bool finished = false;

   SLOCK_LOCK(property_lock);
   finished = task->finished;
   SLOCK_UNLOCK(property_lock);

   return finished;
}

bool task_get_mute(retro_task_t *task)
{
   bool mute = false;

   SLOCK_LOCK(property_lock);
   mute = task->mute;
   SLOCK_UNLOCK(property_lock);

   return mute;
}

char* task_get_error(retro_task_t *task)
{
   char *error = NULL;

   SLOCK_LOCK(property_lock);
   error = task->error;
   SLOCK_UNLOCK(property_lock);

   return error;
}

int8_t task_get_progress(retro_task_t *task)
{
   int8_t progress = 0;

   SLOCK_LOCK(property_lock);
   progress = task->progress;
   SLOCK_UNLOCK(property_lock);

   return progress;
}

char* task_get_title(retro_task_t *task)
{
   char *title = NULL;

   SLOCK_LOCK(property_lock);
   title = task->title;
   SLOCK_UNLOCK(property_lock);

   return title;
}

retro_task_t *task_init(void)
{
   retro_task_t *task      = (retro_task_t*)calloc(1, sizeof(*task));

   task->ident             = task_count++;

   return task;
}
