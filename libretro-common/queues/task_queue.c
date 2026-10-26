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
#include <string.h>

#include <queues/task_queue.h>

#include <features/features_cpu.h>
#include <retro_atomic.h>

#if defined(HAVE_GCD) && !defined(HAVE_THREADS)
#error "gcd uses threads, what are you doing"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#ifdef __EMSCRIPTEN__
#include <retro_timers.h>
#endif
/* Horizon on 3DS and Switch never time-slices: of two ready threads on
 * a core, the higher priority one keeps it until it waits, and an equal
 * one waits its turn.  The worker shares a core with the main thread
 * there (Old 3DS has no other, and on Switch both take the process's
 * default core), so it runs one step below its creator.  The samples
 * build the Switch form on a host with TASK_QUEUE_FORCE_SWITCH_PRIO,
 * standing the two system calls in. */
#if defined(HAVE_THREADS) && defined(_3DS)
#include <3ds/types.h>
#include <3ds/svc.h>
#define TASK_WORKER_BELOW_CREATOR 1
#elif defined(HAVE_THREADS) \
      && (defined(__SWITCH__) || defined(TASK_QUEUE_FORCE_SWITCH_PRIO))
#include <switch/types.h>
#include <switch/kernel/svc.h>
#define TASK_WORKER_BELOW_CREATOR 1
#endif
#ifdef HAVE_GCD
#include <dispatch/dispatch.h>
#include <pthread.h>
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
   bool (*cancel_ident)(uint32_t ident);
   void (*wake)(uint32_t ident);
   void (*reset)(void);
   void (*wait)(retro_task_condition_fn_t, void *);
   void (*gather)(void);
   bool (*find)(retro_task_finder_t, void*);
   void (*retrieve)(task_retriever_data_t *data);
   bool (*init)(void);
   void (*deinit)(void);
};

/* TODO/FIXME - static globals */
static retro_task_queue_msg_t msg_push_bak  = NULL;
static task_queue_t tasks_running           = {NULL, NULL};
static task_queue_t tasks_finished          = {NULL, NULL};
#ifdef HAVE_THREADS
/* Tasks whose callbacks are currently being run by
 * retro_task_threaded_gather(); guarded by finished_lock. Keeps
 * such tasks visible to retro_task_threaded_find() from any thread
 * until they are fully retired, without holding finished_lock
 * across the callbacks themselves. */
static task_queue_t tasks_retiring          = {NULL, NULL};
/* The sizes of tasks_running and tasks_finished, for
 * retro_task_threaded_gather(), which runs every frame and returns
 * without a lock while both are zero. Changed only where a task enters
 * or leaves those queues, under the lock that guards the queue; a
 * finishing task counts as finished before it stops counting as
 * running, so both are never zero while one is in flight. */
static retro_atomic_int_t tasks_running_count  = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t tasks_finished_count = RETRO_ATOMIC_INT_INITIALIZER(0);
#endif

static struct retro_task_impl *impl_current = NULL;
/* Read by task handlers on the worker. */
static retro_atomic_int_t task_threaded_enable =
   RETRO_ATOMIC_INT_INITIALIZER(0);

#ifdef HAVE_THREADS
static uintptr_t main_thread_id             = 0;
static slock_t *running_lock                = NULL;
static slock_t *finished_lock               = NULL;
static slock_t *property_lock               = NULL;
static scond_t *worker_cond                 = NULL;
/* Signalled by a worker each time it moves a task onto the finished
 * queue.  The blocking waiters sleep on it instead of spinning. */
static scond_t *finished_cond               = NULL;
#ifndef HAVE_GCD
static sthread_t *worker_thread             = NULL;
#endif
static bool worker_continue                 = true;
/* use running_lock when touching it */

/* Where a worker is, for deinit. Per worker rather than global: a
 * worker deinit gave up on still owns its record after the next init
 * has started a new one, and must not read the new worker's. */
enum task_worker_state
{
   TASK_WORKER_IDLE = 0,   /* in the queue's own code */
   TASK_WORKER_IN_HANDLER, /* inside task->handler() */
   TASK_WORKER_ORPHANED,   /* deinit let go of it; it owns the rest */
   TASK_WORKER_EXITED      /* left the loop; joinable */
};

/* The primitives of one threaded init.  Deinit holds a reference, and
 * so does each worker it leaves behind in a detachable handler; the
 * last of them to let go frees them.  refs is under running_lock. */
struct task_generation
{
   slock_t           *running_lock;
   slock_t           *finished_lock;
   scond_t           *worker_cond;
   scond_t           *finished_cond;
   unsigned           refs;
};

struct task_worker
{
   retro_atomic_int_t state;
   /* Takes only RETRO_TASK_FLG_PARALLEL tasks; set at start. */
   bool               parallel_only;
   /* Its entry in task_worker_tid: 0 for the one worker, 1 + i for
    * extra worker i. */
   unsigned           slot;
   /* The task whose handler it is in; written under running_lock. */
   retro_task_t      *task;
   /* The init this worker was started for, so an orphan can find its
    * own generation's primitives after deinit has cleared the globals.
    * Set at start, never changed. */
   struct task_generation *gen;
#ifdef TASK_WORKER_BELOW_CREATOR
   /* The priority the worker runs at: one step below its creator.
    * Level with it, a handler that returns without waiting - polling
    * for a callback the main thread runs - would hold the core, and
    * the callback would never come; above it, a handler that keeps
    * working would hold it until its task finished.  Below it, tasks
    * run whenever the main thread waits - for the next frame, for
    * audio, or in task_queue_wait. */
   s32                prio;
#endif
};

#ifndef HAVE_GCD
static struct task_worker     *worker_self  = NULL;
static struct task_generation *task_gen     = NULL;
#endif

/* The extra workers for RETRO_TASK_FLG_PARALLEL tasks, started by the
 * push that first needs them.  parallel_started only grows until
 * deinit: a push that needs more raises it under running_lock and
 * fills the entries it claimed, also under running_lock.  Atomic, so
 * a push with enough skips the lock.  Read by deinit, which no push
 * runs beside. */
#define TASK_PARALLEL_MAX 8
#ifndef HAVE_GCD
static struct task_worker *parallel_self[TASK_PARALLEL_MAX];
static sthread_t          *parallel_thread[TASK_PARALLEL_MAX];
static unsigned            parallel_count   = 0;
static retro_atomic_int_t  parallel_started = RETRO_ATOMIC_INT_INITIALIZER(0);
#endif
static retro_atomic_int_t  parallel_wanted  = RETRO_ATOMIC_INT_INITIALIZER(0);

/* How long deinit waits for the worker before it may leave one stuck
 * in a detachable task's handler behind. */
#define TASK_QUEUE_DETACH_AFTER_US 1000000

/* Once a worker has been left behind, its handler may still be
 * setting task properties, which takes property_lock. The lock is
 * then kept for the life of the process and reused by every later
 * init instead of being freed. */
static bool property_lock_pinned            = false;
#ifdef RETRO_ATOMIC_LOCK_FREE
#define TASK_WAKE_GATE
/* Wakers on other threads against a switch of runner: a switch that
 * finds one inside waits a check, and a waker that finds a switch
 * leaves its task to it.  Lock-free only: a count that the volatile
 * fallback lost would hold every later switch off. */
static retro_atomic_int_t task_wakers    = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t task_switching = RETRO_ATOMIC_INT_INITIALIZER(0);
#ifdef RETRO_ATOMIC_HAS_PTR
#define TASK_PUSH_DEFER
/* Tasks pushed from other threads that may not reach the runner, for
 * the next gather. */
static retro_atomic_ptr_t task_deferred;
/* Cancels by ident from other threads, for the next gather. */
static retro_atomic_ptr_t task_cancels;
/* The threaded workers' thread ids, one slot each, and on GCD a key
 * set while a handler runs: a find from any of them skips the gate,
 * since deinit waits for them. */
static retro_atomic_ptr_t task_worker_tid[1 + TASK_PARALLEL_MAX];
#ifdef HAVE_GCD
static pthread_key_t      task_gcd_key;
static retro_atomic_int_t task_gcd_key_made = RETRO_ATOMIC_INT_INITIALIZER(0);
#endif

struct task_cancel_node
{
   struct task_cancel_node *next;
   uint32_t                 ident;
};
#endif
#endif
static void task_wake_gate_open(void);
#endif

#ifdef HAVE_GCD
static unsigned gcd_queue_count             = 0;
#endif

/* task->flags and task->progress are atomics and take no lock. Only
 * a backend whose read-modify-write is not atomic keeps the flag
 * updates under property_lock. */
#if defined(HAVE_THREADS) && !defined(RETRO_ATOMIC_LOCK_FREE)
#define TASK_FLAGS_RMW_LOCK()   slock_lock(property_lock)
#define TASK_FLAGS_RMW_UNLOCK() slock_unlock(property_lock)
#else
#define TASK_FLAGS_RMW_LOCK()   ((void)0)
#define TASK_FLAGS_RMW_UNLOCK() ((void)0)
#endif

static void task_flags_set(retro_task_t *task, int flags)
{
   TASK_FLAGS_RMW_LOCK();
   retro_atomic_fetch_or_int(&task->flags, flags);
   TASK_FLAGS_RMW_UNLOCK();
}

static void task_flags_clear(retro_task_t *task, int flags)
{
   TASK_FLAGS_RMW_LOCK();
   retro_atomic_fetch_and_int(&task->flags, ~flags);
   TASK_FLAGS_RMW_UNLOCK();
}

static void task_queue_msg_format(char *s, size_t len, const char *fmt, ...)
{
   va_list ap;

   va_start(ap, fmt);
   vsnprintf(s, len, fmt, ap);
   va_end(ap);
}

/* Builds this frame's message from the task's properties and hands it to
 * the frontend.  The flags are read first, so a finished task is seen
 * with the error it set before finishing.  The message is formatted
 * under property_lock, because a
 * worker replacing the title frees the old buffer as it goes
 * (task_free_title() then task_set_title()), and then pushed with the
 * lock released: msg_push() reaches the frontend's
 * message path, which inserts into the widget queue and, on a build with
 * accessibility enabled, forks to speak - none of which a worker's
 * task_set_progress() should be parked behind.  The push therefore
 * carries the formatted string and nothing the lock guards, so
 * msg_push() must not read the task's title or error; see
 * runloop_task_msg_queue_push(). */
static void task_queue_push_progress(retro_task_t *task)
{
   char buf[1024];
   bool have_msg = false;
   bool flush    = false;
   int flg       = retro_atomic_load_acquire_int(&task->flags);
   bool finished = (flg & RETRO_TASK_FLG_FINISHED) != 0;
   bool mute     = (flg & RETRO_TASK_FLG_MUTE) != 0;

   buf[0] = '\0';

#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif

   if (task->title && !mute)
   {
      have_msg = true;

      if (finished)
      {
         if (task->error)
         {
            flush = true;
            task_queue_msg_format(buf, sizeof(buf), "%s: %s",
                  "Task failed", task->title);
         }
         else
            task_queue_msg_format(buf, sizeof(buf), "100%%: %s", task->title);
      }
      else
      {
         int progress = retro_atomic_load_acquire_int(&task->progress);
         if (progress >= 0 && progress <= 100)
         {
            flush = true;
            task_queue_msg_format(buf, sizeof(buf), "%i%%: %s",
                  progress, task->title);
         }
         else
            task_queue_msg_format(buf, sizeof(buf), "%s...", task->title);
      }
   }

   /* Messages are gated on the title above; the callback is for
    * code, so it needs only the mute opt-out. It stays under the lock,
    * so it may read the title, and it pokes the display server rather
    * than going through the message path. */
   if (task->progress_cb && !mute)
      task->progress_cb(task);

#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif

   /* Retirement must reach an attached frontend even when text is
    * suppressed or a replacement title allocation failed. Whether one
    * is attached is the frontend's to read: frontend_userdata is its
    * link, cleared on whatever thread it frees its side on, so a
    * finished task always pushes and the frontend ignores an empty
    * message with nothing attached. */
   if (impl_current->msg_push && (have_msg || finished))
      impl_current->msg_push(task, buf, 1, 60, flush);
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

/* Split in two so the threaded gather can run the callback while the
 * task is still findable (callers rely on find() staying truthful
 * until a task is fully retired) but defer the teardown until after
 * it has been pruned from the retiring list.
 *
 * cleanup() is where a task releases task->state, and finders
 * dereference task->state -- tasks/task_http.c's does.  Running
 * cleanup while the task is still reachable from tasks_retiring, and
 * with neither lock held, races any concurrent find(). */
static void retro_task_internal_retire_callback(retro_task_t *task)
{
   task_queue_push_progress(task);

   if (task->callback)
      task->callback(task, task->task_data, task->user_data, task->error);
}

static void retro_task_internal_retire_cleanup(retro_task_t *task)
{
   if (task->cleanup)
       task->cleanup(task);

#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   if (task->error)
   {
      free(task->error);
      task->error = NULL;
   }

   if (task->title)
   {
      free(task->title);
      task->title = NULL;
   }
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

/* Per-check budgets (task_queue_set_budget). Read and written on the
 * thread that drives task_queue_check(); zero means unbounded, the
 * behaviour before budgets existed. */
static retro_time_t task_retire_budget_usec;
static unsigned     task_retire_budget_max;
static retro_time_t task_handler_budget_usec;

void task_queue_set_budget(retro_time_t retire_usec, unsigned retire_max,
      retro_time_t handler_usec)
{
   task_retire_budget_usec  = retire_usec;
   task_retire_budget_max   = retire_max;
   task_handler_budget_usec = handler_usec;
}

/* Whether one more retirement fits: always the first, then while
 * under the count and time budgets. */
static bool task_retire_budget_allows(unsigned done, retro_time_t started)
{
   if (!done)
      return true;
   if (task_retire_budget_max && done >= task_retire_budget_max)
      return false;
   if (task_retire_budget_usec
         && cpu_features_get_time_usec() - started >= task_retire_budget_usec)
      return false;
   return true;
}

static void retro_task_internal_gather(void)
{
   retro_task_t *task   = NULL;
   unsigned      done   = 0;
   retro_time_t  started = (task_retire_budget_usec)
      ? cpu_features_get_time_usec() : 0;

   while (task_retire_budget_allows(done, started)
         && (task = task_queue_get(&tasks_finished)))
   {
      /* Already popped off the queue, so it is unreachable either way
       * and the split is unobservable here. */
      retro_task_internal_retire_callback(task);
      retro_task_internal_retire_cleanup(task);
      free(task);
      done++;
   }
}

static void retro_task_regular_push_running(retro_task_t *task)
{
   task_queue_put(&tasks_running, task);
}

static void retro_task_regular_cancel(void *task)
{
   task_flags_set((retro_task_t*)task, RETRO_TASK_FLG_CANCELLED);
}

static bool retro_task_regular_cancel_ident(uint32_t ident)
{
   retro_task_t *task = tasks_running.front;

   for (; task; task = task->next)
   {
      if (task->ident == ident)
      {
         task_flags_set(task, RETRO_TASK_FLG_CANCELLED);
         return true;
      }
   }
   return false;
}

/* This runner calls each handler once per check, which is already the
 * wait a park asks for, so there is nothing to wake. */
static void retro_task_regular_wake(uint32_t ident)
{
   (void)ident;
}

/* Slow-handler watchdog.  Both are read and written on the thread
 * that drives the untheaded queue - the same thread that registers
 * them - so they need no locking. */
static retro_task_slow_handler_t task_slow_handler_cb;
static retro_time_t              task_slow_handler_budget;

void task_queue_set_slow_handler_cb(retro_task_slow_handler_t cb,
      retro_time_t budget_usec)
{
   task_slow_handler_cb     = cb;
   task_slow_handler_budget = (budget_usec < 1) ? 1 : budget_usec;
}

static void task_queue_push_deferred(void);
static void task_queue_cancel_deferred(void);

static void retro_task_regular_gather(void)
{
   retro_task_t *task     = NULL;
   retro_task_t *queue    = NULL;
   retro_task_t *tail     = NULL;
   retro_task_t *next     = NULL;
   retro_task_t *ran      = NULL;
   retro_task_t *ran_tail = NULL;
   retro_time_t  started  = 0;
   unsigned      n_ran    = 0;
   bool          over     = false;

   task_queue_push_deferred();
   task_queue_cancel_deferred();

   /* Detach the running list, in order. */
   while ((task = task_queue_get(&tasks_running)))
   {
      if (tail)
         tail->next = task;
      else
         queue      = task;
      tail = task;
   }

   if (task_handler_budget_usec)
      started = cpu_features_get_time_usec();

   for (task = queue; task; task = next)
   {
      next = task->next;

      /* Past the budget, the rest of the list waits for the next
       * check. At least one due handler always runs, so a budget too
       * small for any single handler still makes progress. */
      if (   !over && n_ran && task_handler_budget_usec
          && cpu_features_get_time_usec() - started >= task_handler_budget_usec)
         over = true;

      if (over)
      {
         /* Unrun tasks stay at the front, so a busy queue rotates
          * through them rather than starving the tail. */
         task_queue_put(&tasks_running, task);
         continue;
      }

      if (!task->when || task->when < cpu_features_get_time_usec())
      {
         /* Time the handler only when someone is listening: with no
          * callback registered this costs nothing at all. */
         if (task_slow_handler_cb)
         {
            retro_time_t h_started = cpu_features_get_time_usec();
            retro_time_t took;

            task->handler(task);

            took = cpu_features_get_time_usec() - h_started;
            if (took > task_slow_handler_budget)
               task_slow_handler_cb(task, took);
         }
         else
            task->handler(task);
         n_ran++;
         task->park = 0;
      }

      /* This runner is already on the pumping thread. Retirement
       * publishes finished tasks; publish running tasks here. */
      if (retro_atomic_load_acquire_int(&task->flags) & RETRO_TASK_FLG_FINISHED)
         task_queue_put(&tasks_finished, task);
      else
      {
         task_queue_push_progress(task);
         task->next = NULL;
         if (ran_tail)
            ran_tail->next = task;
         else
            ran            = task;
         ran_tail = task;
      }
   }

   /* The tasks that ran go behind the ones that did not. */
   for (task = ran; task; task = next)
   {
      next = task->next;
      task_queue_put(&tasks_running, task);
   }

   retro_task_internal_gather();
}

static void retro_task_regular_wait(retro_task_condition_fn_t cond, void* data)
{
   /* A held push counts as running. */
   task_queue_push_deferred();
   while ((tasks_running.front && !tasks_running.front->when) && (!cond || cond(data)))
      retro_task_regular_gather();
}

static void retro_task_regular_reset(void)
{
   retro_task_t *task = tasks_running.front;

   for (; task; task = task->next)
      task_flags_set(task, RETRO_TASK_FLG_CANCELLED);
}

static bool retro_task_regular_init(void) { return true; }
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

      /* Create new link.  NULL-check both allocations: the previous
       * form dereferenced info on the very next line, and passed
       * info->data (potentially NULL from the second malloc) into
       * data->func which is free to dereference it.  On OOM just
       * skip this task - the retriever already tolerates tasks
       * being absent from the result list (data->func returning
       * false is the documented 'skip' signal). */
      if (!(info = (task_retriever_info_t*)
            malloc(sizeof(task_retriever_info_t))))
         continue;
      if (!(info->data = malloc(data->element_size)))
      {
         free(info);
         continue;
      }
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
   retro_task_regular_cancel_ident,
   retro_task_regular_wake,
   retro_task_regular_reset,
   retro_task_regular_wait,
   retro_task_regular_gather,
   retro_task_regular_find,
   retro_task_regular_retrieve,
   retro_task_regular_init,
   retro_task_regular_deinit
};

#ifdef HAVE_THREADS

/* 'running_lock' must be held for the duration of this function */
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
            queue->back = t;
         break;
      }
 
      /* Update iterator */
      t = t->next;
   }
}

#ifndef HAVE_GCD
static void retro_task_threaded_start_parallel(unsigned needed);
static bool task_is_parallel(retro_task_t *task);

static void retro_task_threaded_push_running(retro_task_t *task)
{
   if (task_is_parallel(task))
      retro_task_threaded_start_parallel(task->parallel_workers);
   slock_lock(running_lock);
   task_queue_put(&tasks_running, task);
   retro_atomic_fetch_add_int(&tasks_running_count, 1);
   /* Every worker: the one that wakes must be one that may take it */
   scond_broadcast(worker_cond);
   slock_unlock(running_lock);
}
#endif

#ifdef HAVE_GCD
static void gcd_wake_locked(retro_task_t *task);
#endif

/* Makes @task due now, whether it was parked or scheduled for later,
 * and tells whoever runs it. Under running_lock. */
static void retro_task_threaded_wake_locked(retro_task_t *task)
{
   task->woken = true;
   if (task->when)
   {
      task_queue_remove(&tasks_running, task);
      task->when   = 0;
      task->parked = false;
      task_queue_put(&tasks_running, task);
   }
#ifdef HAVE_GCD
   if (task->sched)
      gcd_wake_locked(task);
#endif
   scond_broadcast(worker_cond);
}

/* @task is about to run: a park it was on is over, so it counts as
 * running again, and a wake from before this pass is spent. Under
 * running_lock. */
static void retro_task_threaded_unpark_locked(retro_task_t *task)
{
   if (task->parked)
   {
      task_queue_remove(&tasks_running, task);
      task->when   = 0;
      task->parked = false;
      task_queue_put(&tasks_running, task);
   }
   task->woken = false;
}

/* After an unfinished pass: the park the handler asked for, unless a
 * wake came in while it ran. Under running_lock; returns whether the
 * task was parked. */
static bool retro_task_threaded_park_locked(retro_task_t *task)
{
   bool parked = false;
   if (task->park && !task->woken)
   {
      task_queue_remove(&tasks_running, task);
      task->when   = cpu_features_get_time_usec() + task->park;
      task->parked = true;
      task_queue_put(&tasks_running, task);
      parked       = true;
   }
   task->park  = 0;
   task->woken = false;
   return parked;
}

/* At a switch, once no runner thread is left: a park ends with the
 * runner that set it.  The next runner opens the wake gate before it
 * runs a task, so a parked task loses nothing to a wake the gate
 * dropped. */
static void retro_task_threaded_unpark_all(void)
{
   retro_task_t *task = tasks_running.front;

   while (task)
   {
      retro_task_t *next = task->next;
      if (task->parked)
      {
         task_queue_remove(&tasks_running, task);
         task->when   = 0;
         task->parked = false;
         task_queue_put(&tasks_running, task);
      }
      task = next;
   }
}

static void retro_task_threaded_wake(uint32_t ident)
{
   retro_task_t *t;

   slock_lock(running_lock);
   for (t = tasks_running.front; t; t = t->next)
   {
      if (t->ident == ident)
      {
         retro_task_threaded_wake_locked(t);
         break;
      }
   }
   slock_unlock(running_lock);
}

/* A cancelled task is woken, so a parked one sees the flag now rather
 * than at its horizon. */
static void retro_task_threaded_cancel(void *task)
{
   retro_task_t *t;

   slock_lock(running_lock);

   for (t = tasks_running.front; t; t = t->next)
   {
      if (t == task)
      {
        task_flags_set(t, RETRO_TASK_FLG_CANCELLED);
        retro_task_threaded_wake_locked(t);
        break;
      }
   }

   slock_unlock(running_lock);
}

/* As retro_task_threaded_cancel(), for the task with @ident. */
static bool retro_task_threaded_cancel_ident(uint32_t ident)
{
   retro_task_t *t;
   bool          found = false;

   slock_lock(running_lock);
   for (t = tasks_running.front; t; t = t->next)
   {
      if (t->ident == ident)
      {
         task_flags_set(t, RETRO_TASK_FLG_CANCELLED);
         retro_task_threaded_wake_locked(t);
         found = true;
         break;
      }
   }
   slock_unlock(running_lock);
   return found;
}

static void retro_task_threaded_run_main(unsigned due);
static unsigned retro_task_threaded_due_main(retro_task_t *task,
      retro_time_t *now);

/* The running tasks gather() reports on after dropping running_lock;
 * main thread only, grown as needed and freed at deinit. */
static retro_task_t **task_report    = NULL;
static unsigned       task_report_cap = 0;

static void retro_task_threaded_gather(void)
{
   retro_task_t *task = NULL;

   task_queue_push_deferred();
   task_queue_cancel_deferred();

   /* Nothing running and nothing finished, as on nearly every frame.
    * A task that finishes just after this test retires on the next
    * call. */
   if (   !retro_atomic_load_acquire_int(&tasks_running_count)
       && !retro_atomic_load_acquire_int(&tasks_finished_count))
      return;

   {
      /* One pass under the lock counts the main-thread tasks that are
       * due and takes down which tasks to report on; the reports -
       * formatting, and the push into the frontend's message path -
       * are made after it, so a worker picking up or settling a task
       * never waits on them. A task in the running list is freed only
       * by a retire on this thread, so the pointers stay good until
       * the retire below. The list they are taken down in grows to the
       * number running, before the lock; a task pushed in between
       * waits for the next pass. */
      retro_time_t now     = 0;
      unsigned     due     = 0;
      unsigned     reports = 0;
      unsigned     want    = (unsigned)retro_atomic_load_acquire_int(
            &tasks_running_count) + 8;
      unsigned     i;
      if (want > task_report_cap)
      {
         retro_task_t **grown = (retro_task_t**)realloc(task_report,
               want * sizeof(*grown));
         if (grown)
         {
            task_report     = grown;
            task_report_cap = want;
         }
      }
      slock_lock(running_lock);
      for (task = tasks_running.front; task; task = task->next)
      {
         if (reports < task_report_cap)
            task_report[reports++] = task;
         due += retro_task_threaded_due_main(task, &now);
      }
      slock_unlock(running_lock);

      for (i = 0; i < reports; i++)
         task_queue_push_progress(task_report[i]);

      if (due)
         retro_task_threaded_run_main(due);
   }

   /* Detach the entire finished list under the lock, then retire
    * the tasks (progress push, callback, cleanup, free) outside
    * it. Task callbacks routinely push follow-up tasks - which
    * takes running_lock - so holding finished_lock across them
    * would invert the running_lock -> finished_lock nesting order
    * used by the worker's finished-move and by
    * retro_task_threaded_find(), a classic ABBA deadlock.
    *
    * Tasks move to the retiring list (still under finished_lock)
    * for the span between 'finished' and 'fully retired', so
    * find() remains truthful from any thread throughout. */
   {
      /* Task callbacks can transitively call task_queue_check()
       * (and thus re-enter this function on the same thread); a
       * nested invocation walking and freeing the retiring list
       * would invalidate the outer one. Gather only ever runs on
       * the task-callback thread, so a simple recursion guard
       * suffices: the nested call becomes a no-op and the outer
       * loop - which re-reads the list head every iteration -
       * picks the work up. */
      static bool in_gather = false;
      retro_task_t *task    = NULL;

      if (in_gather)
         return;
      in_gather = true;

      /* Move every finished task to the retiring list while
       * holding the lock, so it stays visible to find() from any
       * thread for the whole span between 'finished' and 'fully
       * retired' */
      slock_lock(finished_lock);
      while ((task = task_queue_get(&tasks_finished)))
         task_queue_put(&tasks_retiring, task);
      retro_atomic_store_release_int(&tasks_finished_count, 0);
      slock_unlock(finished_lock);

      /* Retire outside the lock (callbacks may push tasks, taking
       * running_lock; see the deadlock note above), pruning each
       * task from the retiring list - under the lock - before it
       * is freed. Bounded by the retire budget: what is left on the
       * retiring list stays findable and is picked up next check. */
      {
      unsigned     done    = 0;
      retro_time_t started = (task_retire_budget_usec)
         ? cpu_features_get_time_usec() : 0;
      for (;;)
      {
         if (!task_retire_budget_allows(done, started))
         {
            /* Leftovers: keep the early-out at the top of this
             * function from skipping the next check. */
            retro_atomic_store_release_int(&tasks_finished_count, 1);
            break;
         }

         slock_lock(finished_lock);
         task = tasks_retiring.front;
         slock_unlock(finished_lock);

         if (!task)
            break;

         /* Callback first, while the task is still findable: it may
          * push follow-up tasks (taking running_lock), so it cannot
          * run under finished_lock. */
         retro_task_internal_retire_callback(task);

         slock_lock(finished_lock);
         task_queue_remove(&tasks_retiring, task);
         slock_unlock(finished_lock);

         /* Only now is the task unreachable from find(), so this is
          * the first point at which releasing task->state is safe. */
         retro_task_internal_retire_cleanup(task);

         free(task);
         done++;
      }
      }

      in_gather = false;
   }
}

/* Park a blocking waiter until a worker retires a task.
 *
 * The waiters below used to loop on gather() with nothing in between,
 * which pinned a core for as long as the task ran - half the CPU of a
 * CLI --scan was the main thread taking and releasing the queue
 * locks.  Sleep on finished_cond instead, with a short timeout as the
 * net for the states the signal does not cover (the caller's own
 * condition flipping, a task that keeps yielding without finishing).
 * If something is already sitting on the finished queue there is
 * nothing to wait for - gather() will retire it on the next pass. */
static void retro_task_threaded_park(void)
{
   slock_lock(finished_lock);
   if (!tasks_finished.front)
      scond_wait_timeout(finished_cond, finished_lock, 1000);
   slock_unlock(finished_lock);
}

#ifndef HAVE_GCD
/* Whether @queue holds a task a wait should wait for: one due now or
 * one parked on a wait. A task scheduled for later does not count.
 * Under the queue's lock. */
static bool task_queue_has_due(const task_queue_t *queue)
{
   const retro_task_t *t;
   for (t = queue->front; t; t = t->next)
      if (!t->when || t->parked)
         return true;
   return false;
}

static void retro_task_threaded_wait(retro_task_condition_fn_t cond, void* data)
{
   bool wait = false;

   do
   {
      retro_task_threaded_gather();

      slock_lock(running_lock);
      wait = task_queue_has_due(&tasks_running);
      slock_unlock(running_lock);

      if (!wait)
      {
         slock_lock(finished_lock);
         wait = (tasks_finished.front && !tasks_finished.front->when);
         slock_unlock(finished_lock);
      }

      if (wait)
         retro_task_threaded_park();
   } while (wait && (!cond || cond(data)));
}
#endif

static void retro_task_threaded_reset(void)
{
   retro_task_t *task  = NULL;
   retro_task_t *next  = NULL;
   retro_task_t *queue = NULL;

   /* Every task is cancelled and due now; put back in order, since
    * all of them now share a 'when' of 0. */
   slock_lock(running_lock);
   queue              = tasks_running.front;
   tasks_running.front = NULL;
   tasks_running.back  = NULL;
   for (task = queue; task; task = next)
   {
      next         = task->next;
      task_flags_set(task, RETRO_TASK_FLG_CANCELLED);
      task->woken  = true;
      task->when   = 0;
      task->parked = false;
      task_queue_put(&tasks_running, task);
#ifdef HAVE_GCD
      if (task->sched)
         gcd_wake_locked(task);
#endif
   }
   scond_broadcast(worker_cond);
   slock_unlock(running_lock);
}

static bool retro_task_threaded_find(
      retro_task_finder_t func, void *user_data)
{
   retro_task_t *task = NULL;
   bool ret = false;

   /* A task is 'findable' until it has been fully retired,
    * i.e. including the window where the worker has finished it
    * but its main-thread callback has not yet run. Callers use
    * find() for deduplication and liveness ('is this work fully
    * done?'), and reporting 'not found' in that window lets a
    * duplicate task be pushed - racing the first over shared
    * destinations - or lets a find-based task_queue_wait()
    * condition return before the callback has run (a teardown
    * ordering hazard). Both queues are scanned under both locks
    * (nesting order running_lock -> finished_lock, matching the
    * worker's finished-move), which makes the scan atomic with
    * respect to the worker: a task missing from the running queue
    * while running_lock is held is either already in the finished
    * queue or fully retired (the gather pops it and runs its
    * callback while holding finished_lock, so once it is absent
    * from both queues, its callback has completed). Tasks whose
    * callback is currently executing block this function on
    * finished_lock until the callback has finished - which
    * reports 'not found', correctly. */
   slock_lock(running_lock);
   slock_lock(finished_lock);

   for (task = tasks_running.front; task; task = task->next)
   {
      if (func(task, user_data))
      {
         ret = true;
         break;
      }
   }

   if (!ret)
   {
      for (task = tasks_finished.front; task; task = task->next)
      {
         if (func(task, user_data))
         {
            ret = true;
            break;
         }
      }
   }

   if (!ret)
   {
      for (task = tasks_retiring.front; task; task = task->next)
      {
         if (func(task, user_data))
         {
            ret = true;
            break;
         }
      }
   }

   slock_unlock(finished_lock);
   slock_unlock(running_lock);

   return ret;
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

/* Set on the main thread, read by the worker as it starts */
static retro_atomic_int_t task_worker_prefer_fast_cores =
   RETRO_ATOMIC_INT_INITIALIZER(0);

void task_queue_set_parallel_workers(unsigned count)
{
   retro_atomic_store_release_int(&parallel_wanted,
         count > TASK_PARALLEL_MAX ? TASK_PARALLEL_MAX : (int)count);
}

void task_queue_set_prefer_fast_cores(bool prefer)
{
   retro_atomic_store_release_int(&task_worker_prefer_fast_cores,
         prefer ? 1 : 0);
}

/* Main-thread tasks are run by retro_task_threaded_gather(), never by
 * the worker.  The bit is set before the push and never changes. */
static bool task_is_main_thread_task(retro_task_t *task)
{
   return (retro_atomic_load_acquire_int(&task->flags)
         & RETRO_TASK_FLG_MAIN_THREAD) != 0;
}

#ifndef HAVE_GCD
/* A task an extra worker may take.  Set before the push and never
 * changed. */
static bool task_is_parallel(retro_task_t *task)
{
   int flags = retro_atomic_load_acquire_int(&task->flags);
   return (flags & RETRO_TASK_FLG_PARALLEL)
      && !(flags & RETRO_TASK_FLG_MAIN_THREAD);
}
#endif

/* After a handler returned: a finished task moves to the finished
 * list, an unfinished one to the back of the running list.  Called
 * without running_lock held, by the worker for its tasks and by the
 * gather for main-thread ones. */
static void retro_task_threaded_settle(retro_task_t *task)
{
   bool finished;

   finished = (retro_atomic_load_acquire_int(&task->flags)
         & RETRO_TASK_FLG_FINISHED) != 0;

   if (!finished)
   {
      slock_lock(running_lock);

      /* A parked task waits behind the due ones until its horizon or a
       * wake; any other goes to the back of the tasks due now, unless
       * it is the only item in the queue. */
      task->busy = false;
      if (!retro_task_threaded_park_locked(task) && task->next)
      {
         task_queue_remove(&tasks_running, task);
         task_queue_put(&tasks_running, task);
         scond_broadcast(worker_cond);
      }
      slock_unlock(running_lock);
   }
   else
   {
      /* Move the task from the running queue to the finished
       * queue while continuously holding running_lock: if the
       * locks were dropped between the remove and the put, the
       * task would briefly be in neither queue, and
       * retro_task_threaded_find() - which scans both queues
       * under both locks - could miss a task that is not yet
       * fully retired. Lock nesting order is
       * running_lock -> finished_lock, matching the find
       * function; no other path nests these locks. */
      slock_lock(running_lock);
      task->busy = false;
      task_queue_remove(&tasks_running, task);

      /* Add task to finished queue */
      slock_lock(finished_lock);
      task_queue_put(&tasks_finished, task);
      retro_atomic_fetch_add_int(&tasks_finished_count, 1);
      retro_atomic_fetch_sub_int(&tasks_running_count, 1);
      scond_signal(finished_cond);
      slock_unlock(finished_lock);
      slock_unlock(running_lock);
   }
}

/* 1 when @task is a main-thread task that is due; @now is read
 * once, the first time a 'when' needs it.  Called under
 * running_lock. */
static unsigned retro_task_threaded_due_main(retro_task_t *task,
      retro_time_t *now)
{
   if (!task_is_main_thread_task(task))
      return 0;
   if (task->when)
   {
      if (!*now)
         *now = cpu_features_get_time_usec();
      if (task->when > *now)
         return 0;
   }
   return 1;
}

/* Runs @due main-thread tasks once each, within the handler budget.
 * Each one processed goes to the back of the running list (or off
 * it), so the count taken beforehand bounds the pass; one pushed by
 * a handler meanwhile waits for the next check. */
static void retro_task_threaded_run_main(unsigned due)
{
   /* A main-thread handler that waits on the queue gathers again from
    * inside this pass.  The task it runs in, and every other one this
    * pass took, is still on the running list; running them from the
    * nested gather would run them twice and settle them twice.  The
    * pass further up the stack runs them.  Only the checking thread
    * gets here, so a plain flag does. */
   static bool in_run_main = false;
   retro_time_t started = 0;
   unsigned     n_ran   = 0;
   retro_task_t *task   = NULL;

   if (in_run_main)
      return;
   in_run_main = true;

   if (task_handler_budget_usec)
      started = cpu_features_get_time_usec();

   while (due--)
   {
      /* Past the budget, the rest wait for the next check.  At
       * least one due handler always runs. */
      if (   n_ran && task_handler_budget_usec
          && cpu_features_get_time_usec() - started >= task_handler_budget_usec)
         break;

      slock_lock(running_lock);
      for (task = tasks_running.front; task; task = task->next)
      {
         if (!task_is_main_thread_task(task))
            continue;
         if (task->when && task->when > cpu_features_get_time_usec())
            continue;
         break;
      }
      if (task)
         retro_task_threaded_unpark_locked(task);
      slock_unlock(running_lock);
      if (!task)
         break;

      /* On the checking thread, as on the unthreaded runner: timed
       * when someone is listening. */
      if (task_slow_handler_cb)
      {
         retro_time_t h_started = cpu_features_get_time_usec();
         retro_time_t took;
         task->handler(task);
         took = cpu_features_get_time_usec() - h_started;
         if (took > task_slow_handler_budget)
            task_slow_handler_cb(task, took);
      }
      else
         task->handler(task);
      n_ran++;
      retro_task_threaded_settle(task);
   }

   in_run_main = false;
}

#ifndef HAVE_GCD
static void retro_task_generation_free(struct task_generation *gen)
{
   scond_free(gen->worker_cond);
   scond_free(gen->finished_cond);
   slock_free(gen->running_lock);
   slock_free(gen->finished_lock);
   free(gen);
}

/* For task_is_queue_thread(): each worker sets its own id before it
 * runs a handler and clears it before it reports that it left, and
 * deinit clears the id of one it leaves behind, so a thread that
 * reuses an id is not taken for a worker. */
static void task_worker_tid_set(unsigned slot, void *tid)
{
#ifdef TASK_PUSH_DEFER
   retro_atomic_store_release_ptr(&task_worker_tid[slot], tid);
#else
   (void)slot;
   (void)tid;
#endif
}

static void threaded_worker(void *userdata)
{
   struct task_worker *self = (struct task_worker*)userdata;

   sthread_setname("ra-task");
   task_worker_tid_set(self->slot, (void*)sthread_get_current_thread_id());
#ifdef TASK_WORKER_BELOW_CREATOR
   /* Refused outside the process's priority range, which leaves the
    * worker level with its creator. */
   svcSetThreadPriority(CUR_THREAD_HANDLE, self->prio);
#endif
   if (retro_atomic_load_acquire_int(&task_worker_prefer_fast_cores))
      sthread_prefer_fast_cores();

   for (;;)
   {
      retro_task_t *task  = NULL;

      slock_lock(running_lock);

      if (!worker_continue)
      {
         /* Tell deinit, which may be waiting on worker_cond with a
          * bound, that there is nothing left to wait for. */
         task_worker_tid_set(self->slot, NULL);
         retro_atomic_store_release_int(&self->state, TASK_WORKER_EXITED);
         scond_broadcast(worker_cond);
         slock_unlock(running_lock);
         /* No: draining is the caller's job, done while the
          * subsystems that finish callbacks reach are still alive
          * (RetroArch's exit path cancels and drains, bounded,
          * before any teardown). Whatever is still here when deinit
          * runs is deliberately abandoned - running its handlers
          * during teardown would be worse. */
         break;
      }

      /* Get first task to run; main-thread tasks are the gather's, one
       * in another worker's handler is that worker's, and an extra
       * worker takes only the tasks marked for it */
      for (task = tasks_running.front; task; task = task->next)
         if (     !task_is_main_thread_task(task)
               && !task->busy
               && (!self->parallel_only || task_is_parallel(task)))
            break;
      if (!task)
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

      self->task = task;
      task->busy = true;
      retro_task_threaded_unpark_locked(task);
      retro_atomic_store_release_int(&self->state, TASK_WORKER_IN_HANDLER);
      slock_unlock(running_lock);
      task->handler(task);

      /* Deinit gave up on this handler and took the task off the
       * queue; the globals may belong to a new init by now.  Clean up
       * while the generation is held, so that deinit keeps
       * property_lock for the cleanup's accessors, then let go of it
       * under its own running_lock, which deinit holds until it has
       * let go too.  No global is touched and no callback runs. */
      if (!retro_atomic_cas_int(&self->state,
               TASK_WORKER_IN_HANDLER, TASK_WORKER_IDLE))
      {
         struct task_generation *gen = self->gen;
         unsigned                refs;

         if (task->cleanup)
            task->cleanup(task);
         slock_lock(gen->running_lock);
         refs = --gen->refs;
         slock_unlock(gen->running_lock);
         if (!refs)
            retro_task_generation_free(gen);
         free(task->title);
         free(task->error);
         free(task);
         free(self);
         return;
      }
#ifdef __EMSCRIPTEN__
      /* Workaround emscripten pthread bug where not parking the
         thread will prevent other important stuff from
         happening. Maybe due to lack of signals implementation in
         emscripten's pthreads?  The extras' passes are short. */
      if (!self->parallel_only)
         retro_sleep(1);
#endif

      retro_task_threaded_settle(task);
   }
}
#endif

/* Releases whatever the shared setup managed to create.  Separate from
 * the deinit hooks because those join a worker, and the failure paths
 * below run before there is one. */
static void retro_task_sync_primitives_free(void)
{
   scond_free(worker_cond);
   scond_free(finished_cond);
   slock_free(running_lock);
   slock_free(finished_lock);
   if (!property_lock_pinned)
   {
      slock_free(property_lock);
      property_lock = NULL;
   }

   worker_cond     = NULL;
   finished_cond   = NULL;
   running_lock    = NULL;
   finished_lock   = NULL;
}

/* slock_new() and scond_new() return NULL when the allocation or the
 * platform primitive fails.  slock_lock() tolerates NULL but
 * scond_signal() does not, so a partially built set has to be taken
 * back rather than left for the first task to run into. */
static bool retro_task_sync_primitives_new(void)
{
   running_lock    = slock_new();
   finished_lock   = slock_new();
   if (!property_lock_pinned)
      property_lock = slock_new();
   worker_cond     = scond_new();
   finished_cond   = scond_new();

   if (     running_lock
         && finished_lock
         && property_lock
         && worker_cond
         && finished_cond)
      return true;

   retro_task_sync_primitives_free();
   return false;
}

/* A switch from the regular queue hands over tasks it never counted.
 * Called before any worker can touch the lists. */
static void retro_task_threaded_count_lists(void)
{
   retro_task_t *task;
   int running  = 0;
   int finished = 0;
   for (task = tasks_running.front; task; task = task->next)
      running++;
   for (task = tasks_finished.front; task; task = task->next)
      finished++;
   /* Leftovers of an earlier threaded gather, as it marks them */
   if (!finished && tasks_retiring.front)
      finished = 1;
   retro_atomic_store_release_int(&tasks_running_count,  running);
   retro_atomic_store_release_int(&tasks_finished_count, finished);
}

#ifndef HAVE_GCD
/* Starts on the pushing thread whatever extra workers are missing;
 * running_lock decides which push starts which. */
static void retro_task_threaded_start_parallel(unsigned needed)
{
   unsigned want = (unsigned)retro_atomic_load_acquire_int(&parallel_wanted);
   unsigned from, i;

   if (needed > want)
      want = needed;
   if (want > TASK_PARALLEL_MAX)
      want = TASK_PARALLEL_MAX;
   if ((unsigned)retro_atomic_load_acquire_int(&parallel_started) >= want)
      return;

   slock_lock(running_lock);
   from = (unsigned)retro_atomic_load_acquire_int(&parallel_started);
   if (from < want)
      retro_atomic_store_release_int(&parallel_started, (int)want);
   slock_unlock(running_lock);

   for (i = from; i < want; i++)
   {
      struct task_worker *w = (struct task_worker*)calloc(1, sizeof(*w));
      sthread_t          *t;
      if (!w)
         break;
      retro_atomic_store_release_int(&w->state, TASK_WORKER_IDLE);
      w->parallel_only = true;
      w->slot          = 1 + i;
      w->gen           = task_gen;
#ifdef TASK_WORKER_BELOW_CREATOR
      w->prio          = worker_self->prio;
#endif
      if (!(t = sthread_create(threaded_worker, w)))
      {
         free(w);
         break;
      }
      slock_lock(running_lock);
      parallel_self[i]   = w;
      parallel_thread[i] = t;
      parallel_count++;
      slock_unlock(running_lock);
   }
}

/* Joins the extra workers deinit did not leave behind, and lets the
 * next parallel push start them again.  Each has left its loop. */
static void retro_task_threaded_join_parallel(void)
{
   unsigned i;
   for (i = 0; i < TASK_PARALLEL_MAX; i++)
   {
      if (!parallel_thread[i])
         continue;
      sthread_join(parallel_thread[i]);
      free(parallel_self[i]);
      parallel_thread[i] = NULL;
      parallel_self[i]   = NULL;
   }
   parallel_count = 0;
   retro_atomic_store_release_int(&parallel_started, 0);
}

static bool retro_task_threaded_init(void)
{
   if (!retro_task_sync_primitives_new())
      return false;

   retro_task_threaded_count_lists();

   worker_self = (struct task_worker*)calloc(1, sizeof(*worker_self));
   task_gen    = (struct task_generation*)calloc(1, sizeof(*task_gen));
   if (!worker_self || !task_gen)
   {
      free(worker_self);
      free(task_gen);
      worker_self = NULL;
      task_gen    = NULL;
      retro_task_sync_primitives_free();
      return false;
   }
   task_gen->running_lock     = running_lock;
   task_gen->finished_lock    = finished_lock;
   task_gen->worker_cond      = worker_cond;
   task_gen->finished_cond    = finished_cond;
   task_gen->refs             = 1;
   retro_atomic_store_release_int(&worker_self->state, TASK_WORKER_IDLE);
   worker_self->gen           = task_gen;
#ifdef TASK_WORKER_BELOW_CREATOR
   worker_self->prio          = 0x2C;
   svcGetThreadPriority(&worker_self->prio, CUR_THREAD_HANDLE);
   /* 0x3F is the lowest priority either kernel has. */
   if (worker_self->prio < 0x3F)
      worker_self->prio++;
#endif

   slock_lock(running_lock);
   worker_continue = true;

   /* The worker reads the globals above, so they are in place before it
    * starts.  Without a worker there is nobody to run what gets queued,
    * which is why this reports failure rather than leaving the queue
    * pointed at an implementation that cannot service it. */
   if ((worker_thread = sthread_create(threaded_worker, worker_self)))
   {
      /* The worker runs nothing before the unlock. */
      task_wake_gate_open();
      slock_unlock(running_lock);
      return true;
   }
   slock_unlock(running_lock);

   worker_continue = false;
   free(worker_self);
   free(task_gen);
   worker_self     = NULL;
   task_gen        = NULL;
   retro_task_sync_primitives_free();
   return false;
}

/* Waits under running_lock, until @deadline, for @w to leave its loop;
 * only a worker stuck in a handler can outlast the bound.  If that
 * task is detachable, its handler touches nothing but the task: the
 * task is taken off the queue and the worker, holding a reference to
 * its generation, is left to finish alone, and this returns true.
 * Any other handler may hold state teardown is about to free, so the
 * wait goes on. */
static bool retro_task_threaded_stop_worker(struct task_worker *w,
      retro_time_t deadline)
{
   for (;;)
   {
      int          state = retro_atomic_load_acquire_int(&w->state);
      retro_time_t left;

      if (state == TASK_WORKER_EXITED)
         return false;

      left = deadline - cpu_features_get_time_usec();
      if (left > 0)
      {
         scond_wait_timeout(worker_cond, running_lock, left);
         continue;
      }

      if (state == TASK_WORKER_IN_HANDLER)
      {
         retro_task_t *task = w->task;
         bool detachable;

         detachable = (retro_atomic_load_acquire_int(&task->flags)
               & RETRO_TASK_FLG_DETACHABLE) != 0;

         if (detachable && retro_atomic_cas_int(&w->state,
                  TASK_WORKER_IN_HANDLER, TASK_WORKER_ORPHANED))
         {
            task_worker_tid_set(w->slot, NULL);
            task_queue_remove(&tasks_running, task);
            retro_atomic_fetch_sub_int(&tasks_running_count, 1);
            w->gen->refs++;
            return true;
         }
      }

      /* Not in a detachable handler: it is either leaving or can only
       * leave by returning. Wait for its signal, without a bound. */
      scond_wait(worker_cond, running_lock);
   }
}

/* Stops every worker, the extra ones first, against one bound. A
 * worker left behind owns its generation's primitives with deinit
 * until both let go, and property_lock is then kept for the life of
 * the process: its handler or cleanup may still be setting task
 * properties through it. */
static void retro_task_threaded_deinit(void)
{
   struct task_generation *gen = task_gen;
   retro_time_t            deadline;
   unsigned                refs, i;
   bool                    left_behind;

   slock_lock(running_lock);
   worker_continue = false;
   scond_broadcast(worker_cond);

   deadline = cpu_features_get_time_usec() + TASK_QUEUE_DETACH_AFTER_US;
   for (i = 0; i < TASK_PARALLEL_MAX; i++)
   {
      if (     parallel_self[i]
            && retro_task_threaded_stop_worker(parallel_self[i], deadline))
      {
         sthread_detach(parallel_thread[i]);
         parallel_self[i]   = NULL;
         parallel_thread[i] = NULL;
      }
   }
   if ((left_behind = retro_task_threaded_stop_worker(worker_self, deadline)))
      sthread_detach(worker_thread);
   refs = --gen->refs;
   slock_unlock(running_lock);

   retro_task_threaded_join_parallel();
   if (!left_behind)
   {
      sthread_join(worker_thread);
      retro_task_threaded_unpark_all();
      free(worker_self);
   }

   if (refs)
   {
      property_lock_pinned = true;
      worker_cond          = NULL;
      finished_cond        = NULL;
      running_lock         = NULL;
      finished_lock        = NULL;
   }
   else
   {
      retro_task_sync_primitives_free();
      free(gen);
   }

   task_gen        = NULL;
   worker_thread   = NULL;
   worker_self     = NULL;

   free(task_report);
   task_report     = NULL;
   task_report_cap = 0;
}

static struct retro_task_impl impl_threaded = {
   NULL,
   retro_task_threaded_push_running,
   retro_task_threaded_cancel,
   retro_task_threaded_cancel_ident,
   retro_task_threaded_wake,
   retro_task_threaded_reset,
   retro_task_threaded_wait,
   retro_task_threaded_gather,
   retro_task_threaded_find,
   retro_task_threaded_retrieve,
   retro_task_threaded_init,
   retro_task_threaded_deinit
};
#endif
#endif

#ifdef HAVE_GCD

/* How the GCD backend runs a task.  At most one run is pending or in
 * progress per task: a task due now is handed to the global queue with
 * dispatch_async(), one parked or scheduled for later waits on a timer
 * source it is given the first time it needs one, and a wake turns a
 * wait on the timer into a dispatch_async().  All of it is under
 * running_lock.  The timer's handler holds this record rather than the
 * task, and the record outlives the task: it is detached when the task
 * stops and freed by the timer's cancel handler, which GCD runs after
 * any handler still in flight, so a late fire finds it detached. */
enum gcd_run_state
{
   GCD_RUN_DISPATCHED = 0, /* a dispatch_async() is on its way */
   GCD_RUN_TIMER,          /* waiting on the timer */
   GCD_RUN_RUNNING         /* in gcd_worker() */
};

typedef struct
{
   retro_task_t      *task;  /* NULL once the task has stopped */
   dispatch_source_t  timer; /* NULL until the task first waits */
   enum gcd_run_state state;
} gcd_sched_t;

static void gcd_worker(retro_task_t *task);

static void gcd_dispatch_locked(retro_task_t *task)
{
   ((gcd_sched_t*)task->sched)->state = GCD_RUN_DISPATCHED;
   dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
         ^{ gcd_worker(task); });
}

static void gcd_timer_fired(gcd_sched_t *sched)
{
   retro_task_t *task;
   slock_lock(running_lock);
   task = sched->task;
   /* Stopped, or overtaken by a wake: nothing to run. */
   if (!task || sched->state != GCD_RUN_TIMER)
   {
      slock_unlock(running_lock);
      return;
   }
   sched->state = GCD_RUN_DISPATCHED;
   slock_unlock(running_lock);
   gcd_worker(task);
}

/* The timer's last act: account for the task and free the record. */
static void gcd_timer_cancelled(gcd_sched_t *sched)
{
   dispatch_release(sched->timer);
   free(sched);
   slock_lock(running_lock);
   gcd_queue_count--;
   if (!gcd_queue_count)
      scond_signal(worker_cond);
   slock_unlock(running_lock);
}

/* Waits on the timer until @task->when, or runs it now when it has no
 * start time. */
static void gcd_schedule_locked(retro_task_t *task)
{
   gcd_sched_t    *sched = (gcd_sched_t*)task->sched;
   retro_time_t    delay;
   dispatch_time_t start;

   if (!task->when)
   {
      gcd_dispatch_locked(task);
      return;
   }

   if (!sched->timer)
   {
      if (!(sched->timer = dispatch_source_create(
            DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0))))
      {
         /* No timer to wait on: run it, and it waits again. */
         gcd_dispatch_locked(task);
         return;
      }
      dispatch_source_set_event_handler(sched->timer,
            ^{ gcd_timer_fired(sched); });
      dispatch_source_set_cancel_handler(sched->timer,
            ^{ gcd_timer_cancelled(sched); });
      dispatch_resume(sched->timer);
   }

   start = DISPATCH_TIME_NOW;
   delay = task->when - cpu_features_get_time_usec();
   if (delay > 0)
      start = dispatch_time(DISPATCH_TIME_NOW,
            (int64_t)delay * (int64_t)NSEC_PER_USEC);
   sched->state = GCD_RUN_TIMER;
   dispatch_source_set_timer(sched->timer, start, DISPATCH_TIME_FOREVER, 0);
}

static void gcd_wake_locked(retro_task_t *task)
{
   /* A run already on its way or in progress sees the wake itself. */
   if (((gcd_sched_t*)task->sched)->state == GCD_RUN_TIMER)
      gcd_dispatch_locked(task);
}

static bool gcd_start_locked(retro_task_t *task)
{
   gcd_sched_t *sched = (gcd_sched_t*)calloc(1, sizeof(*sched));
   if (!sched)
      return false;
   sched->task = task;
   task->sched = sched;
   gcd_queue_count++;
   gcd_schedule_locked(task);
   return true;
}

static void gcd_stop_locked(retro_task_t *task)
{
   gcd_sched_t *sched = (gcd_sched_t*)task->sched;
   task->sched        = NULL;
   sched->task        = NULL;
   if (sched->timer)
   {
      dispatch_source_cancel(sched->timer);
      return;
   }
   free(sched);
   gcd_queue_count--;
   if (!gcd_queue_count)
      scond_signal(worker_cond);
}

/* Marks the thread as one of the queue's own while a handler runs. */
static void gcd_mark_handler(void *mark)
{
#ifdef TASK_PUSH_DEFER
   if (retro_atomic_load_acquire_int(&task_gcd_key_made))
      pthread_setspecific(task_gcd_key, mark);
#else
   (void)mark;
#endif
}

static void gcd_worker(retro_task_t *task)
{
   bool finished = false;
   slock_lock(running_lock);

   if (!worker_continue)
   {
      gcd_stop_locked(task);
      slock_unlock(running_lock);
      return;
   }

   /* Ahead of its start time: wait out the rest. */
   if (task->when)
   {
      retro_time_t delay = task->when - cpu_features_get_time_usec() - 500;
      if (delay > 0)
      {
         gcd_schedule_locked(task);
         slock_unlock(running_lock);
         return;
      }
   }

   ((gcd_sched_t*)task->sched)->state = GCD_RUN_RUNNING;
   retro_task_threaded_unpark_locked(task);
   slock_unlock(running_lock);

   gcd_mark_handler(task);
   task->handler(task);
   gcd_mark_handler(NULL);

   finished = (retro_atomic_load_acquire_int(&task->flags)
         & RETRO_TASK_FLG_FINISHED) != 0;

   slock_lock(running_lock);
   if (!finished)
   {
      /* Parked: at its horizon. A start time the handler set itself:
       * at that. Otherwise: again now. */
      retro_task_threaded_park_locked(task);
      if (worker_continue)
         gcd_schedule_locked(task);
      else
         gcd_stop_locked(task);
      slock_unlock(running_lock);
   }
   else
   {
      /* From the running queue to the finished one under both locks, as
       * the threaded settle does, so find() and the waits never see it
       * in neither. */
      task_queue_remove(&tasks_running, task);
      gcd_stop_locked(task);
      slock_lock(finished_lock);
      task_queue_put(&tasks_finished, task);
      retro_atomic_fetch_add_int(&tasks_finished_count, 1);
      retro_atomic_fetch_sub_int(&tasks_running_count, 1);
      scond_signal(finished_cond);
      slock_unlock(finished_lock);
      slock_unlock(running_lock);
   }
}

static void retro_task_gcd_push_running(retro_task_t *task)
{
   slock_lock(running_lock);
   task_queue_put(&tasks_running, task);
   retro_atomic_fetch_add_int(&tasks_running_count, 1);
   /* A main-thread task is the gather's; only the rest go to GCD.  One
    * that GCD cannot take is cancelled and finished, so its callback
    * still runs. */
   if (!task_is_main_thread_task(task) && !gcd_start_locked(task))
   {
      task_flags_set(task, RETRO_TASK_FLG_CANCELLED | RETRO_TASK_FLG_FINISHED);
      task_queue_remove(&tasks_running, task);
      slock_lock(finished_lock);
      task_queue_put(&tasks_finished, task);
      retro_atomic_fetch_add_int(&tasks_finished_count, 1);
      retro_atomic_fetch_sub_int(&tasks_running_count, 1);
      scond_signal(finished_cond);
      slock_unlock(finished_lock);
   }
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
         wait |= !task->when || task->parked;
      slock_unlock(running_lock);

      if (!wait)
      {
         slock_lock(finished_lock);
         for (task = tasks_finished.front; !wait && task; task = task->next)
            wait |= !task->when;
         slock_unlock(finished_lock);
      }

      if (wait)
         retro_task_threaded_park();
   } while (wait && (!cond || cond(data)));
}

static bool retro_task_gcd_init(void)
{
   retro_task_t *task = NULL;

   if (!retro_task_sync_primitives_new())
      return false;
#ifdef TASK_PUSH_DEFER
   /* Made once and never deleted, so that no finder meets a key on its
    * way out. */
   if (     !retro_atomic_load_relaxed_int(&task_gcd_key_made)
         && !pthread_key_create(&task_gcd_key, NULL))
      retro_atomic_store_release_int(&task_gcd_key_made, 1);
#endif

   slock_lock(running_lock);
   worker_continue = true;
   retro_task_threaded_count_lists();
   for (task = tasks_running.front; task; task = task->next)
   {
      if (task_is_main_thread_task(task))
         continue;
      /* Left on the list: it runs again if a later init can start it. */
      gcd_start_locked(task);
   }
   /* No run reaches a task before the unlock. */
   task_wake_gate_open();
   slock_unlock(running_lock);

   return true;
}

static void retro_task_gcd_deinit(void)
{
   retro_task_t *task = NULL;

   slock_lock(running_lock);
   worker_continue = false;
   /* Run every task waiting on its timer now, so each sees the stop
    * and lets go. */
   for (task = tasks_running.front; task; task = task->next)
      if (task->sched)
         gcd_wake_locked(task);
   while (gcd_queue_count)
      scond_wait(worker_cond, running_lock);
   retro_task_threaded_unpark_all();
   slock_unlock(running_lock);

   scond_free(worker_cond);
   scond_free(finished_cond);
   slock_free(running_lock);
   slock_free(finished_lock);
   slock_free(property_lock);

   worker_cond     = NULL;
   finished_cond   = NULL;
   running_lock    = NULL;
   finished_lock   = NULL;
   property_lock   = NULL;
}

static struct retro_task_impl impl_gcd = {
   NULL,
   retro_task_gcd_push_running,
   retro_task_threaded_cancel,
   retro_task_threaded_cancel_ident,
   retro_task_threaded_wake,
   retro_task_threaded_reset,
   retro_task_gcd_wait,
   retro_task_threaded_gather,
   retro_task_threaded_find,
   retro_task_threaded_retrieve,
   retro_task_gcd_init,
   retro_task_gcd_deinit
};
#endif

/* Only the thread that drives the queue writes task_switching. */
static void task_wake_gate_close(void)
{
#ifdef TASK_WAKE_GATE
   if (!retro_atomic_load_relaxed_int(&task_switching))
      retro_atomic_fetch_add_seq_cst_int(&task_switching, 1);
#endif
}

static void task_wake_gate_open(void)
{
#ifdef TASK_WAKE_GATE
   if (retro_atomic_load_relaxed_int(&task_switching))
      retro_atomic_fetch_add_seq_cst_int(&task_switching, -1);
#endif
}

/* For a thread other than the one that drives the queue: true when no
 * switch is under way, and the runner then stays up until
 * task_wake_gate_leave().  One turned away is not counted, so a stream
 * of them cannot hold a switch off. */
static bool task_wake_gate_enter(void)
{
#ifdef TASK_WAKE_GATE
   if (retro_atomic_load_acquire_int(&task_switching))
      return false;
   retro_atomic_fetch_add_seq_cst_int(&task_wakers, 1);
   if (retro_atomic_load_seq_cst_int(&task_switching))
   {
      retro_atomic_fetch_sub_int(&task_wakers, 1);
      return false;
   }
#endif
   return true;
}

static void task_wake_gate_leave(void)
{
#ifdef TASK_WAKE_GATE
   retro_atomic_fetch_sub_int(&task_wakers, 1);
#endif
}

/* Deinitializes the task system.
 * This deinitializes the task system.
 * The tasks that are running at
 * the moment will stay on hold */
void task_queue_deinit(void)
{
   task_wake_gate_close();
   if (impl_current)
   {
      /* Held pushes wait for the next runner's first gather, so no
       * callback runs here; held cancels still reach them. */
      task_queue_cancel_deferred();
      impl_current->deinit();
   }
   impl_current = NULL;
}

void task_queue_init(bool threaded, retro_task_queue_msg_t msg_push)
{
   impl_current   = &impl_regular;
#ifdef HAVE_THREADS
   /* Other threads read it through task_is_on_main_thread(), so a
    * switch leaves it alone. */
   if (main_thread_id != sthread_get_current_thread_id())
      main_thread_id = sthread_get_current_thread_id();
   /* The flag follows the argument both ways: it used to be set only
    * on true, so init(false) after a threaded session left it on and
    * the next task_queue_check() re-initialised the threaded queue
    * behind the caller's back. */
   retro_atomic_store_release_int(&task_threaded_enable, threaded ? 1 : 0);
   if (threaded)
   {
#ifdef HAVE_GCD
      impl_current         = &impl_gcd;
#else
      impl_current         = &impl_threaded;
#endif
   }
#endif

   msg_push_bak            = msg_push;

   impl_current->msg_push  = msg_push;

   if (impl_current->init())
   {
      task_wake_gate_open();
      return;
   }

#ifdef HAVE_THREADS
   /* Nothing services a threaded queue without its primitives and its
    * worker, so run the tasks on the caller's thread instead of
    * accepting work that would never be picked up.  Callers see the
    * outcome through task_queue_is_threaded(). */
   retro_atomic_store_release_int(&task_threaded_enable, 0);
   impl_current            = &impl_regular;
   impl_current->msg_push  = msg_push;
   impl_current->init();
#endif
   task_wake_gate_open();
}

void task_queue_set_threaded(void)
{
   retro_atomic_store_release_int(&task_threaded_enable, 1);
}

void task_queue_unset_threaded(void)
{
   retro_atomic_store_release_int(&task_threaded_enable, 0);
}

bool task_queue_is_threaded(void)
{
   return retro_atomic_load_acquire_int(&task_threaded_enable) != 0;
}

#ifdef TASK_PUSH_DEFER
/* A handler on the queue's own worker or GCD threads: deinit waits for
 * it before it frees anything, so it never meets a switch. */
static bool task_is_queue_thread(void)
{
   void    *self = (void*)sthread_get_current_thread_id();
   unsigned i;
#ifdef HAVE_GCD
   if (     retro_atomic_load_acquire_int(&task_gcd_key_made)
         && pthread_getspecific(task_gcd_key))
      return true;
#endif
   for (i = 0; i < 1 + TASK_PARALLEL_MAX; i++)
      if (retro_atomic_load_acquire_ptr(&task_worker_tid[i]) == self)
         return true;
   return false;
}

/* Pushes held for the gather.  Only the thread that drives the queue
 * drains them, so only it may walk them. */
static bool task_queue_find_deferred(retro_task_finder_t func,
      void *user_data)
{
   retro_task_t *task = (retro_task_t*)
      retro_atomic_load_acquire_ptr(&task_deferred);

   for (; task; task = task->next)
   {
      if (func(task, user_data))
         return true;
   }
   return false;
}
#endif

bool task_queue_find(task_finder_data_t *find_data)
{
#ifdef TASK_PUSH_DEFER
   if (task_is_on_main_thread())
      return impl_current->find(find_data->func, find_data->userdata)
         || task_queue_find_deferred(find_data->func, find_data->userdata);
   if (!task_is_queue_thread())
   {
      bool found = false;

      /* The unthreaded queue's lists take no lock. */
      if (task_wake_gate_enter())
      {
         if (impl_current && impl_current != &impl_regular)
            found = impl_current->find(find_data->func,
                  find_data->userdata);
         task_wake_gate_leave();
      }
      return found;
   }
#endif
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
   bool want_threaded    =
      retro_atomic_load_acquire_int(&task_threaded_enable) != 0;

   if (want_threaded != current_threaded)
   {
      /* A waker still inside keeps the old runner one more check. */
      task_wake_gate_close();
#ifdef TASK_WAKE_GATE
      if (!retro_atomic_load_seq_cst_int(&task_wakers))
#endif
         task_queue_deinit();
   }
   else if (impl_current)
      task_wake_gate_open(); /* a put-off switch, taken back */

   if (!impl_current)
      task_queue_init(want_threaded, msg_push_bak);
#endif

   impl_current->gather();
}

static bool task_queue_push_now(retro_task_t *task)
{
   /* Ignore this task if a related one is already running.
    *
    * The scan walks tasks_running, so it takes the lock that guards
    * that queue's structure. push_running() below takes the same lock,
    * so a caller must not hold it either way - a task handler or
    * callback pushing a follow-up runs outside it. */
   if (task->type == TASK_TYPE_BLOCKING)
   {
      retro_task_t *running = NULL;
      bool            found = false;

#ifdef HAVE_THREADS
      slock_lock(running_lock);
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
      slock_unlock(running_lock);
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

#ifdef TASK_PUSH_DEFER
/* Newest first.  A swap that succeeds has linked the task to the true
 * head, whatever came and went before it. */
static void task_queue_defer(retro_task_t *task)
{
   void *head;

   do
   {
      head       = retro_atomic_load_acquire_ptr(&task_deferred);
      task->next = (retro_task_t*)head;
   } while (!retro_atomic_cas_ptr(&task_deferred, head, task));
}
#endif

/* At the top of each gather, on the thread that drives the queue, so
 * it never meets a switch.  The tasks stay on the list until they are
 * pushed: a push from another thread is held while it is not empty, so
 * it cannot pass one its own thread made earlier. */
static void task_queue_push_deferred(void)
{
#ifdef TASK_PUSH_DEFER
   retro_task_t  *snap;
   retro_task_t  *task;
   retro_task_t  *fifo        = NULL;
   retro_task_t  *refused     = NULL;
   retro_task_t **refused_end = &refused;

   snap = (retro_task_t*)retro_atomic_load_acquire_ptr(&task_deferred);
   if (!snap)
      return;
   task = snap;
   while (task)
   {
      retro_task_t *next = task->next;
      task->next         = fifo;
      fifo               = task;
      task               = next;
   }
   while (fifo)
   {
      retro_task_t *next = fifo->next;
      /* Cancelled while held: retired below without running. */
      if (   (retro_atomic_load_acquire_int(&fifo->flags)
               & RETRO_TASK_FLG_CANCELLED)
          || !task_queue_push_now(fifo))
      {
         fifo->next   = NULL;
         *refused_end = fifo;
         refused_end  = &fifo->next;
      }
      fifo = next;
   }
   /* Cut them off below any task held since. */
   if (!retro_atomic_cas_ptr(&task_deferred, snap, NULL))
   {
      task = (retro_task_t*)retro_atomic_load_acquire_ptr(&task_deferred);
      while (task->next != snap)
         task = task->next;
      task->next = NULL;
   }
   /* A second BLOCKING task, or one cancelled while held, is retired
    * cancelled, so that its callback still runs.  Not before the cut:
    * freed, its address could come back as the head. */
   while (refused)
   {
      retro_task_t *next = refused->next;
      task_flags_set(refused,
            RETRO_TASK_FLG_CANCELLED | RETRO_TASK_FLG_FINISHED);
      retro_task_internal_retire_callback(refused);
      retro_task_internal_retire_cleanup(refused);
      free(refused);
      refused = next;
   }
#endif
}

bool task_queue_push(retro_task_t *task)
{
#ifdef TASK_PUSH_DEFER
   /* The thread that drives the queue is the one that switches it. */
   if (!task_is_on_main_thread())
   {
      bool pushed = true;

      if (!task_wake_gate_enter())
         task_queue_defer(task);
      else
      {
         /* The unthreaded queue's lists take no lock, and a held task
          * may be this thread's own. */
         if (   impl_current == &impl_regular
             || retro_atomic_load_acquire_ptr(&task_deferred))
            task_queue_defer(task);
         else
            pushed = task_queue_push_now(task);
         task_wake_gate_leave();
      }
      return pushed;
   }
#endif
   return task_queue_push_now(task);
}

void task_queue_wait(retro_task_condition_fn_t cond, void* data)
{
   impl_current->wait(cond, data);
}

struct task_wait_deadline
{
   retro_task_condition_fn_t cond;
   void *data;
   retro_time_t deadline;
};

/* Wraps the caller's condition so that it also goes false once the
 * deadline passes.  Doing it this way rather than adding a wait
 * variant to the implementation vtable means every implementation -
 * regular, threaded and GCD - keeps its own waiting behaviour, and
 * no platform-specific code has to be touched to gain a bound. */
static bool task_queue_deadline_cond(void *data)
{
   struct task_wait_deadline *d = (struct task_wait_deadline*)data;

   if (cpu_features_get_time_usec() >= d->deadline)
      return false;

   return d->cond ? d->cond(d->data) : true;
}

bool task_queue_wait_timeout(retro_task_condition_fn_t cond, void *data,
      retro_time_t timeout_usec)
{
   struct task_wait_deadline d;

   d.cond     = cond;
   d.data     = data;
   d.deadline = cpu_features_get_time_usec() + timeout_usec;

   task_queue_wait(task_queue_deadline_cond, &d);

   /* The wait also ends when the queue drains, so "did the deadline
    * pass" is not the same question as "is the caller still waiting
    * for something".  Ask the caller's own condition. */
   if (cond && cond(data))
      return false;

   return true;
}

void task_queue_reset(void)
{
   /* Held pushes are cancelled with the queued tasks. */
   task_queue_push_deferred();
   impl_current->reset();
}

/**
 * Signals a task to end without waiting for
 * it to complete. */
void task_queue_cancel_task(void *task)
{
   impl_current->cancel(task);
}

/* On the thread that drives the queue: a running task, or one held for
 * the gather, which the gather then retires without running it. */
static bool task_queue_cancel_ident_now(uint32_t ident)
{
#ifdef TASK_PUSH_DEFER
   retro_task_t *task;
#endif

   if (!impl_current)
      return false;
   if (impl_current->cancel_ident(ident))
      return true;
#ifdef TASK_PUSH_DEFER
   for (task = (retro_task_t*)retro_atomic_load_acquire_ptr(&task_deferred);
         task; task = task->next)
   {
      if (task->ident == ident)
      {
         task_flags_set(task, RETRO_TASK_FLG_CANCELLED);
         return true;
      }
   }
#endif
   return false;
}

#ifdef TASK_PUSH_DEFER
static bool task_queue_defer_cancel(uint32_t ident)
{
   void                    *head;
   struct task_cancel_node *node = (struct task_cancel_node*)
      malloc(sizeof(*node));

   if (!node)
      return false;
   node->ident = ident;
   do
   {
      head       = retro_atomic_load_acquire_ptr(&task_cancels);
      node->next = (struct task_cancel_node*)head;
   } while (!retro_atomic_cas_ptr(&task_cancels, head, node));
   return true;
}
#endif

/* After the pushes in each gather, so that a cancel finds a push its
 * own thread made first. */
static void task_queue_cancel_deferred(void)
{
#ifdef TASK_PUSH_DEFER
   struct task_cancel_node *node;

   if (!retro_atomic_load_acquire_ptr(&task_cancels))
      return;
   node = (struct task_cancel_node*)
      retro_atomic_exchange_ptr(&task_cancels, NULL);
   while (node)
   {
      struct task_cancel_node *next = node->next;
      task_queue_cancel_ident_now(node->ident);
      free(node);
      node = next;
   }
#endif
}

bool task_queue_cancel_ident(uint32_t ident)
{
#ifdef TASK_PUSH_DEFER
   /* The thread that drives the queue is the one that switches it. */
   if (!task_is_on_main_thread())
   {
      if (task_wake_gate_enter())
      {
         /* A held push may be this thread's own, which the running
          * list does not have yet. */
         bool direct =    impl_current
                       && impl_current != &impl_regular
                       && !retro_atomic_load_acquire_ptr(&task_deferred);
         if (direct)
            impl_current->cancel_ident(ident);
         task_wake_gate_leave();
         if (direct)
            return true;
      }
      return task_queue_defer_cancel(ident);
   }
#endif
   task_queue_cancel_ident_now(ident);
   return true;
}

void task_queue_park(retro_task_t *task, retro_time_t horizon_usec)
{
   /* Applied when the pass ends, by whichever runner made it. */
   task->park = (horizon_usec > 0) ? horizon_usec : 1;
}

void task_queue_wake(uint32_t ident)
{
   /* The thread that drives the queue is the one that switches it. */
   if (task_is_on_main_thread())
   {
      if (impl_current)
         impl_current->wake(ident);
   }
   else if (task_wake_gate_enter())
   {
      if (impl_current)
         impl_current->wake(ident);
      task_wake_gate_leave();
   }
}

void *task_queue_retriever_info_next(task_retriever_info_t **link)
{
   void *data = NULL;

   /* Grab data from current link, then advance */
   if (*link)
   {
      data  = (*link)->data;
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

bool task_is_on_main_thread(void)
{
#ifdef HAVE_THREADS
   return sthread_get_current_thread_id() == main_thread_id;
#else
   return true;
#endif
}

bool task_get_progress_snapshot(const retro_task_t *task,
      task_progress_snapshot_t *snapshot)
{
   bool success = true;
   size_t len;

   retro_task_t *t = (retro_task_t*)task;

   snapshot->title    = NULL;
   snapshot->error    = NULL;
   /* before the strings: see the header */
   snapshot->flags    = (uint8_t)retro_atomic_load_acquire_int(&t->flags);
   snapshot->progress = (int8_t)retro_atomic_load_acquire_int(&t->progress);
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   if (task->title)
   {
      len = strlen(task->title) + 1;
      if ((snapshot->title = (char*)malloc(len)))
         memcpy(snapshot->title, task->title, len);
      else
         success = false;
   }
   if (success && task->error)
   {
      len = strlen(task->error) + 1;
      if ((snapshot->error = (char*)malloc(len)))
         memcpy(snapshot->error, task->error, len);
      else
         success = false;
   }
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
   if (!success)
   {
      free(snapshot->title);
      free(snapshot->error);
      snapshot->title = NULL;
      snapshot->error = NULL;
   }
   return success;
}

void task_set_error(retro_task_t *task, char *err)
{
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   task->error = err;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
}

void task_set_progress(retro_task_t *task, int8_t progress)
{
   retro_atomic_store_release_int(&task->progress, progress);
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

void task_free_error(retro_task_t *task)
{
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   if (task->error)
      free(task->error);
   task->error = NULL;
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
   if (set)
      task_flags_set(task, flags);
   else
      task_flags_clear(task, flags);
}

uint8_t task_get_flags(retro_task_t *task)
{
   return (uint8_t)retro_atomic_load_acquire_int(&task->flags);
}

char* task_get_error(retro_task_t *task)
{
   char *s = NULL;
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   s = task->error;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
   return s;
}

int8_t task_get_progress(retro_task_t *task)
{
   return (int8_t)retro_atomic_load_acquire_int(&task->progress);
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
   /* TODO/FIXME - static local global
    *
    * Tasks are pushed from whatever thread happens to want one - the
    * main thread, the menu, netplay, a core's own worker - so this
    * counter is incremented concurrently.  A plain read-modify-write
    * is a data race, and hands out duplicate idents when two pushes
    * interleave.  An acq_rel fetch_add serialises the handout and
    * gives every task a distinct value.
    *
    * int rather than uint32_t because that is the width retro_atomic
    * offers; ident is an opaque tag, so the eventual wrap at INT_MAX
    * (2.1 billion task creations) is of no consequence, and the cast
    * back to uint32_t is well defined for the non-negative values
    * this can produce before then. */
   static retro_atomic_int_t task_count;
   retro_task_t *task         = (retro_task_t*)malloc(sizeof(*task));

   if (!task)
      return NULL;

   task->handler           = NULL;
   task->callback          = NULL;
   task->cleanup           = NULL;
   retro_atomic_int_init(&task->flags, 0);
   task->task_data         = NULL;
   task->user_data         = NULL;
   task->state             = NULL;
   task->error             = NULL;
   retro_atomic_int_init(&task->progress, 0);
   task->progress_cb       = NULL;
   task->title             = NULL;
   task->type              = TASK_TYPE_NONE;
   task->style             = TASK_STYLE_NONE;
   task->ident             = (uint32_t)
      retro_atomic_fetch_add_int(&task_count, 1);
   task->frontend_userdata = NULL;
   task->next              = NULL;
   task->sched             = NULL;
   task->when              = 0;
   task->park              = 0;
   task->parked            = false;
   task->woken             = false;
   task->busy              = false;
   task->parallel_workers  = 0;

   return task;
}
