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
/* The threaded runner without a lock wherever the atomics have
 * compare-and-swap and pointer operations, on its own workers or on
 * GCD's. */
#if defined(HAVE_THREADS) \
      && defined(RETRO_ATOMIC_LOCK_FREE) \
      && defined(RETRO_ATOMIC_HAS_CAS) && defined(RETRO_ATOMIC_HAS_PTR)
#define TASK_QUEUE_LF 1
#include <rthreads/retro_eventcount.h>
#endif
#if defined(HAVE_GCD) && !defined(TASK_QUEUE_LF)
#error "the GCD runner needs lock-free atomics with compare-and-swap and pointers"
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
#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
/* Tasks whose callbacks are currently being run by
 * retro_task_threaded_gather(); guarded by finished_lock. Keeps
 * such tasks visible to retro_task_threaded_find() from any thread
 * until they are fully retired, without holding finished_lock
 * across the callbacks themselves. */
static task_queue_t tasks_retiring          = {NULL, NULL};
#endif
#ifdef HAVE_THREADS
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
#ifndef TASK_QUEUE_LF
static slock_t *property_lock               = NULL;
static slock_t *running_lock                = NULL;
static slock_t *finished_lock               = NULL;
static scond_t *worker_cond                 = NULL;
/* Signalled by a worker each time it moves a task onto the finished
 * queue.  The blocking waiters sleep on it instead of spinning. */
static scond_t *finished_cond               = NULL;
static bool worker_continue                 = true;
/* use running_lock when touching it */
#endif
#ifndef HAVE_GCD
static sthread_t *worker_thread             = NULL;
#endif

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

#ifdef TASK_QUEUE_LF
/* One threaded init.  Deinit holds a reference, and so does each
 * worker; the last to let go frees it. */
struct task_generation
{
   retro_atomic_int_t refs;
};
#else
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
#endif

struct task_worker
{
   retro_atomic_int_t state;
   /* Takes only RETRO_TASK_FLG_PARALLEL tasks; set at start. */
   bool               parallel_only;
   /* Which ring the one worker tries first this time. */
   bool               turn;
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
#ifdef TASK_QUEUE_LF
static retro_atomic_int_t  parallel_created = RETRO_ATOMIC_INT_INITIALIZER(0);
#define parallel_count ((unsigned)retro_atomic_load_acquire_int(&parallel_created))
#else
static unsigned            parallel_count   = 0;
#endif
static retro_atomic_int_t  parallel_started = RETRO_ATOMIC_INT_INITIALIZER(0);
#endif
static retro_atomic_int_t  parallel_wanted  = RETRO_ATOMIC_INT_INITIALIZER(0);

/* How long deinit waits for the worker before it may leave one stuck
 * in a detachable task's handler behind. */
#define TASK_QUEUE_DETACH_AFTER_US 1000000

#ifndef TASK_QUEUE_LF
/* Once a worker has been left behind, its handler may still be
 * setting task properties, which takes property_lock. The lock is
 * then kept for the life of the process and reused by every later
 * init instead of being freed. */
static bool property_lock_pinned            = false;
#endif
#ifdef RETRO_ATOMIC_LOCK_FREE
#define TASK_WAKE_GATE
/* Wakers on other threads against a switch of runner: a switch that
 * finds one inside waits a check, and a waker that finds a switch
 * leaves its task to it.  Lock-free only: a count that the volatile
 * fallback lost would hold every later switch off. */
static retro_atomic_int_t task_wakers    = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t task_switching = RETRO_ATOMIC_INT_INITIALIZER(0);
#ifdef TASK_QUEUE_LF
#define TASK_GATE_WAITS
/* For a deinit to wait out the wakers inside; made by the first init
 * and kept, since a waker may be on its way out of it at any time. */
static retro_eventcount_t task_gate_ec;
static bool               task_gate_ec_ready = false;
#endif
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


#ifdef TASK_QUEUE_LF
/* A task's title and error, as published for other threads to read:
 * read inside a read epoch, and a string replaced under a reader is
 * freed once it is out. */
static int  task_ep_enter(void);
static void task_ep_leave(int e);
static void task_str_retire(char *s);
#define TASK_SHOWN(t, f) ((const char*)retro_atomic_load_acquire_ptr(&(t)->f))
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
 * inside a read epoch (under property_lock without the lock-free
 * runner), because a worker replacing the title frees the old buffer
 * as it goes (task_free_title() then task_set_title()), and then
 * pushed outside it: msg_push() reaches the frontend's
 * message path, which inserts into the widget queue and, on a build with
 * accessibility enabled, forks to speak - none of which a worker's
 * task_set_progress() should be parked behind.  The push therefore
 * carries the formatted string and nothing the epoch guards, so
 * msg_push() must not read the task's title or error; see
 * runloop_task_msg_queue_push(). */
static void task_queue_push_progress(retro_task_t *task)
{
   char buf[1024];
   const char *title;
   const char *error;
   bool have_msg = false;
   bool flush    = false;
   int flg       = retro_atomic_load_acquire_int(&task->flags);
   bool finished = (flg & RETRO_TASK_FLG_FINISHED) != 0;
   bool mute     = (flg & RETRO_TASK_FLG_MUTE) != 0;
#ifdef TASK_QUEUE_LF
   int e         = task_ep_enter();
   title         = TASK_SHOWN(task, title_shown);
   error         = TASK_SHOWN(task, error_shown);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   title         = task->title;
   error         = task->error;
#endif

   buf[0] = '\0';

   if (title && !mute)
   {
      have_msg = true;

      if (finished)
      {
         if (error)
         {
            flush = true;
            task_queue_msg_format(buf, sizeof(buf), "%s: %s",
                  "Task failed", title);
         }
         else
            task_queue_msg_format(buf, sizeof(buf), "100%%: %s", title);
      }
      else
      {
         int progress = retro_atomic_load_acquire_int(&task->progress);
         if (progress >= 0 && progress <= 100)
         {
            flush = true;
            task_queue_msg_format(buf, sizeof(buf), "%i%%: %s",
                  progress, title);
         }
         else
            task_queue_msg_format(buf, sizeof(buf), "%s...", title);
      }
   }

#ifdef TASK_QUEUE_LF
   task_ep_leave(e);
#endif

   /* Messages are gated on the title above; the callback is for
    * code, so it needs only the mute opt-out. It reads the task's
    * atomic properties, not its strings, and it pokes the display
    * server rather than going through the message path. */
   if (task->progress_cb && !mute)
      task->progress_cb(task);

#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
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

/* By then nothing else can read the task: with the lock-free runner,
 * not even a finder. */
static void retro_task_internal_retire_cleanup(retro_task_t *task)
{
   if (task->cleanup)
       task->cleanup(task);

#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
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
#ifdef RETRO_ATOMIC_HAS_PTR
   retro_atomic_store_release_ptr(&task->title_shown, NULL);
   retro_atomic_store_release_ptr(&task->error_shown, NULL);
#endif
#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
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

/* A task an extra worker may take.  Set before the push and never
 * changed. */
static bool task_is_parallel(retro_task_t *task)
{
   int flags = retro_atomic_load_acquire_int(&task->flags);
   return (flags & RETRO_TASK_FLG_PARALLEL)
      && !(flags & RETRO_TASK_FLG_MAIN_THREAD);
}

#ifndef HAVE_GCD
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
#endif
#endif

#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)

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

static void retro_task_generation_free(struct task_generation *gen)
{
   scond_free(gen->worker_cond);
   scond_free(gen->finished_cond);
   slock_free(gen->running_lock);
   slock_free(gen->finished_lock);
   free(gen);
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

#ifdef TASK_QUEUE_LF
/* The threaded runner, without a lock.
 *
 * Every task is on the live list from its push until it is retired:
 * pushes link it at the head from any thread, and only the checking
 * thread unlinks one.  find(), retrieve() and the lookups behind wake
 * and cancel walk it from any thread inside a read epoch, so a task
 * they may still be looking at is freed - and cleaned up, since
 * finders read task->state - only once they are out; the checking
 * thread, the one unlinker, walks it without one.
 *
 * Where a task waits to run is a hand-off between threads, each with
 * one owner at a time: a worker's ready tasks are in one of two
 * bounded MPMC rings, the plain ring that only the one worker takes
 * from and the parallel ring every worker takes from; a worker's task
 * with a 'when' to wait for is with the timer keeper, whichever idle
 * worker holds task_timer_token; a main-thread task is the checking
 * thread's; a finished one goes to the checking thread to retire.
 * Each hand-off is a lock-free stack or ring, and nobody waits on
 * another thread except for work: workers on task_work_ec, the
 * checking thread's task_queue_wait() on task_done_ec.  On GCD the
 * workers and the keeper are GCD's queues; see "on GCD" below. */

#define TASK_Q_LIVE 0
#define TASK_Q_DONE 1

#ifndef HAVE_GCD
static void retro_task_threaded_start_parallel(unsigned needed);
#endif
static void task_orphans_reclaim(void);

/* --- read epochs over the live list --- */
static retro_atomic_int_t task_ep_epoch      = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t task_ep_readers[2] = {
   RETRO_ATOMIC_INT_INITIALIZER(0), RETRO_ATOMIC_INT_INITIALIZER(0) };
/* Unlinked while a reader may still hold them; the checking thread's. */
static retro_task_t *task_ep_limbo[2]        = { NULL, NULL };

static int task_ep_enter(void)
{
   for (;;)
   {
      int e = retro_atomic_load_acquire_int(&task_ep_epoch);
      retro_atomic_fetch_add_seq_cst_int(&task_ep_readers[e & 1], 1);
      /* Counted against the epoch the reclaimer will check, or it has
       * moved on and this tries again. */
      if (retro_atomic_load_seq_cst_int(&task_ep_epoch) == e)
         return e;
      retro_atomic_fetch_sub_int(&task_ep_readers[e & 1], 1);
   }
}

static void task_ep_leave(int e)
{
   retro_atomic_fetch_sub_int(&task_ep_readers[e & 1], 1);
}

/* A title or error replaced while a reader may hold it, on its way
 * from whichever thread replaced it to the checking thread's limbo. */
struct task_str_node
{
   struct task_str_node *next;
   char                 *s;
};
static retro_atomic_ptr_t    task_str_retired;
static struct task_str_node *task_str_limbo[2] = { NULL, NULL };

/* After unpublishing: with no reader inside it goes now. */
static void task_str_retire(char *s)
{
   struct task_str_node *node;
   void                 *head;
   if (!s)
      return;
   retro_atomic_thread_fence_seq_cst();
   if (     !retro_atomic_load_seq_cst_int(&task_ep_readers[0])
         && !retro_atomic_load_seq_cst_int(&task_ep_readers[1]))
   {
      free(s);
      return;
   }
   /* Out of memory, the string is leaked rather than waited on. */
   if (!(node = (struct task_str_node*)malloc(sizeof(*node))))
      return;
   node->s = s;
   do
   {
      head       = retro_atomic_load_acquire_ptr(&task_str_retired);
      node->next = (struct task_str_node*)head;
   } while (!retro_atomic_cas_ptr(&task_str_retired, head, node));
}

static void task_str_free_list(struct task_str_node *node)
{
   while (node)
   {
      struct task_str_node *next = node->next;
      free(node->s);
      free(node);
      node = next;
   }
}

static void task_retired_free(retro_task_t *task)
{
   retro_task_internal_retire_cleanup(task);
   free(task);
}

/* After an unlink: with no reader inside, none can reach the task
 * any more and it goes now; otherwise it waits for them. */
static void task_ep_retire(retro_task_t *task)
{
   retro_atomic_thread_fence_seq_cst();
   if (     !retro_atomic_load_seq_cst_int(&task_ep_readers[0])
         && !retro_atomic_load_seq_cst_int(&task_ep_readers[1]))
   {
      task_retired_free(task);
      return;
   }
   {
      int e                = retro_atomic_load_relaxed_int(&task_ep_epoch);
      task->next           = task_ep_limbo[e & 1];
      task_ep_limbo[e & 1] = task;
   }
}

/* Frees what the readers of the last epoch have let go of, and opens
 * the next one.  Checking thread only. */
static void task_ep_reclaim(void)
{
   int e   = retro_atomic_load_relaxed_int(&task_ep_epoch);
   int old = (e + 1) & 1;
   retro_task_t *task;
   if (retro_atomic_load_acquire_ptr(&task_str_retired))
   {
      struct task_str_node *node = (struct task_str_node*)
            retro_atomic_exchange_ptr(&task_str_retired, NULL);
      while (node)
      {
         struct task_str_node *next = node->next;
         node->next          = task_str_limbo[e & 1];
         task_str_limbo[e & 1] = node;
         node                = next;
      }
   }
   if (     !task_ep_limbo[0]    && !task_ep_limbo[1]
         && !task_str_limbo[0]   && !task_str_limbo[1])
      return;
   if (retro_atomic_load_seq_cst_int(&task_ep_readers[old]))
      return;
   task               = task_ep_limbo[old];
   task_ep_limbo[old] = NULL;
   while (task)
   {
      retro_task_t *next = task->next;
      task_retired_free(task);
      task = next;
   }
   task_str_free_list(task_str_limbo[old]);
   task_str_limbo[old] = NULL;
   retro_atomic_fetch_add_seq_cst_int(&task_ep_epoch, 1);
}

/* --- the live list --- */
static retro_atomic_ptr_t task_live;

#define TASK_LIVE_NEXT(t) \
   ((retro_task_t*)retro_atomic_load_acquire_ptr(&(t)->live_next))

static void task_live_insert(retro_task_t *task)
{
   void *head;
   retro_atomic_store_relaxed_int(&task->qstate, TASK_Q_LIVE);
   do
   {
      head = retro_atomic_load_acquire_ptr(&task_live);
      retro_atomic_store_relaxed_ptr(&task->live_next, head);
   } while (!retro_atomic_cas_ptr(&task_live, head, task));
}

/* Checking thread only: pushes only ever change the head. */
static void task_live_unlink(retro_task_t *task)
{
   retro_task_t *prev;
   if (retro_atomic_cas_ptr(&task_live, task, TASK_LIVE_NEXT(task)))
      return;
   prev = (retro_task_t*)retro_atomic_load_acquire_ptr(&task_live);
   while (prev && TASK_LIVE_NEXT(prev) != task)
      prev = TASK_LIVE_NEXT(prev);
   if (prev)
      retro_atomic_store_release_ptr(&prev->live_next, TASK_LIVE_NEXT(task));
}

/* --- lock-free stacks, linked through task->next --- */
static void task_stack_push(retro_atomic_ptr_t *stack, retro_task_t *task)
{
   void *head;
   do
   {
      head       = retro_atomic_load_acquire_ptr(stack);
      task->next = (retro_task_t*)head;
   } while (!retro_atomic_cas_ptr(stack, head, task));
}

/* The whole stack, oldest first. */
static retro_task_t *task_stack_take(retro_atomic_ptr_t *stack)
{
   retro_task_t *task;
   retro_task_t *fifo = NULL;
   if (!retro_atomic_load_acquire_ptr(stack))
      return NULL;
   task = (retro_task_t*)retro_atomic_exchange_ptr(stack, NULL);
   while (task)
   {
      retro_task_t *next = task->next;
      task->next         = fifo;
      fifo               = task;
      task               = next;
   }
   return fifo;
}

/* --- the run rings: bounded MPMC, positions as wrapping ints --- */
#define TASK_RING_SIZE 1024

struct task_ring_cell
{
   retro_atomic_int_t seq;
   retro_task_t      *task;
};

struct task_ring
{
   retro_atomic_int_t    head;
   uint8_t               pad0[60];
   retro_atomic_int_t    tail;
   uint8_t               pad1[60];
   /* What did not fit, for a taker that finds the ring empty. */
   retro_atomic_ptr_t    overflow;
   struct task_ring_cell cell[TASK_RING_SIZE];
};

#define TASK_RING_DIFF(a, b) ((int)((unsigned)(a) - (unsigned)(b)))
#define TASK_RING_ADD(a, n)  ((int)((unsigned)(a) + (unsigned)(n)))

static struct task_ring *task_ring_plain    = NULL;
static struct task_ring *task_ring_parallel = NULL;

static struct task_ring *task_ring_new(void)
{
   unsigned i;
   struct task_ring *r = (struct task_ring*)calloc(1, sizeof(*r));
   if (!r)
      return NULL;
   retro_atomic_int_init(&r->head, 0);
   retro_atomic_int_init(&r->tail, 0);
   retro_atomic_ptr_init(&r->overflow, NULL);
   for (i = 0; i < TASK_RING_SIZE; i++)
      retro_atomic_int_init(&r->cell[i].seq, (int)i);
   return r;
}

/* A sample can stop a push between taking its place and filling it. */
#ifdef TASK_RING_PUT_HOOK
void TASK_RING_PUT_HOOK(void);
#endif

static bool task_ring_put(struct task_ring *r, retro_task_t *task)
{
   int pos = retro_atomic_load_relaxed_int(&r->head);
   struct task_ring_cell *c;
   for (;;)
   {
      int dif;
      c   = &r->cell[(unsigned)pos & (TASK_RING_SIZE - 1)];
      dif = TASK_RING_DIFF(retro_atomic_load_acquire_int(&c->seq), pos);
      if (!dif)
      {
         if (retro_atomic_cas_int(&r->head, pos, TASK_RING_ADD(pos, 1)))
            break;
      }
      else if (dif < 0)
         return false;
      pos = retro_atomic_load_relaxed_int(&r->head);
   }
#ifdef TASK_RING_PUT_HOOK
   TASK_RING_PUT_HOOK();
#endif
   c->task = task;
   retro_atomic_store_release_int(&c->seq, TASK_RING_ADD(pos, 1));
   return true;
}

static retro_task_t *task_ring_get(struct task_ring *r)
{
   int pos = retro_atomic_load_relaxed_int(&r->tail);
   struct task_ring_cell *c;
   retro_task_t *task;
   for (;;)
   {
      int dif;
      c   = &r->cell[(unsigned)pos & (TASK_RING_SIZE - 1)];
      dif = TASK_RING_DIFF(retro_atomic_load_acquire_int(&c->seq),
            TASK_RING_ADD(pos, 1));
      if (!dif)
      {
         if (retro_atomic_cas_int(&r->tail, pos, TASK_RING_ADD(pos, 1)))
            break;
      }
      else if (dif < 0)
         return NULL;
      pos = retro_atomic_load_relaxed_int(&r->tail);
   }
   task = c->task;
   retro_atomic_store_release_int(&c->seq, TASK_RING_ADD(pos, TASK_RING_SIZE));
   return task;
}

/* Behind whatever has overflowed already, so the order holds. */
static void task_ring_push(struct task_ring *r, retro_task_t *task)
{
   if (     retro_atomic_load_acquire_ptr(&r->overflow)
         || !task_ring_put(r, task))
      task_stack_push(&r->overflow, task);
}

/* The ring's next task; the overflow is moved in, oldest first, when
 * the ring runs empty. */
static retro_task_t *task_ring_pop(struct task_ring *r)
{
   retro_task_t *task = task_ring_get(r);
   if (!task && retro_atomic_load_acquire_ptr(&r->overflow))
   {
      retro_task_t *over = task_stack_take(&r->overflow);
      while (over)
      {
         retro_task_t *next = over->next;
         if (!task)
            task = over;
         else if (!task_ring_put(r, over))
            task_stack_push(&r->overflow, over);
         over = next;
      }
   }
   return task;
}

#ifndef HAVE_GCD
static bool task_ring_empty(struct task_ring *r)
{
   return     retro_atomic_load_acquire_int(&r->head)
           == retro_atomic_load_acquire_int(&r->tail)
      && !retro_atomic_load_acquire_ptr(&r->overflow);
}
#endif

/* --- the rest of the runner's state --- */
/* Workers waiting for work; the checking thread waiting in
 * task_queue_wait(); deinit waiting for workers to leave. */
#ifndef HAVE_GCD
static retro_eventcount_t task_work_ec;
static retro_eventcount_t task_exit_ec;
#endif
static retro_eventcount_t task_done_ec;
static bool               task_ec_ready = false;

/* Tasks to wait for: a worker's go to the keeper, a main-thread
 * task's to the checking thread. */
static retro_atomic_ptr_t task_timed_inbox;
static retro_atomic_ptr_t task_main_inbox;
/* Finished tasks for the checking thread. */
static retro_atomic_ptr_t task_finished_stack;
/* Detachable tasks deinit left behind, given back by their workers. */
static retro_atomic_ptr_t task_orphans;

/* The keeper's: worker tasks waiting for their 'when', in order. */
static retro_task_t      *task_timed     = NULL;
#ifndef HAVE_GCD
static retro_atomic_int_t task_timer_token = RETRO_ATOMIC_INT_INITIALIZER(0);
#endif
/* Bumped by every wake; a keeper or the checking thread that sees it
 * move looks for woken tasks among those it holds. */
static retro_atomic_int_t task_timer_wakes = RETRO_ATOMIC_INT_INITIALIZER(0);
static int                task_keeper_wakes_seen = 0;

/* The checking thread's: main-thread tasks due, main-thread tasks
 * waiting, and finished tasks to retire, each in order. */
static task_queue_t       task_main_ready = {NULL, NULL};
static retro_task_t      *task_main_timed = NULL;
static int                task_main_wakes_seen = 0;
static task_queue_t       task_retire     = {NULL, NULL};

/* Tasks waiting on a 'when' their pusher set rather than a park:
 * task_queue_wait() does not wait for them. */
static retro_atomic_int_t tasks_scheduled_count = RETRO_ATOMIC_INT_INITIALIZER(0);

#ifndef HAVE_GCD
static retro_atomic_int_t worker_continue = RETRO_ATOMIC_INT_INITIALIZER(0);
#endif
/* Advanced by each gather with work in flight and by each pass as it
 * ends, and read before each pass: whatever the checking thread wrote
 * before a check, a pass that starts after it sees, and what a pass
 * read is behind whatever the checking thread writes after its next
 * check, as when both took the running lock. Handlers read state the
 * frame loop writes - the frame count a load waits on. */
static retro_atomic_int_t task_gather_seq = RETRO_ATOMIC_INT_INITIALIZER(0);
/* Set once init has opened the wake gate: a worker runs nothing before,
 * so a wake from its first pass is not turned away. */
#ifndef HAVE_GCD
static retro_atomic_int_t worker_go       = RETRO_ATOMIC_INT_INITIALIZER(0);
#endif

#ifdef HAVE_GCD
static void task_gcd_post_ready(bool parallel);
static void task_gcd_poke_keeper(void);
#endif

/* --- placing a task --- */
static void task_timed_insert(retro_task_t **list, retro_task_t *task)
{
   while (*list && (*list)->when <= task->when)
      list = &(*list)->next;
   task->next = *list;
   *list      = task;
}

/* On GCD each push of a task due now makes a call to take one. */
static void task_ready_push(retro_task_t *task)
{
   bool parallel = task_is_parallel(task);
   task_ring_push(parallel ? task_ring_parallel : task_ring_plain, task);
#ifdef HAVE_GCD
   task_gcd_post_ready(parallel);
#endif
}

/* A task coming off a wait: due now, a park or a start time over. */
static void task_release(retro_task_t *task)
{
   if (task->when && !task->parked)
      retro_atomic_fetch_sub_int(&tasks_scheduled_count, 1);
   task->when   = 0;
   task->parked = false;
   if (task_is_main_thread_task(task))
      task_queue_put(&task_main_ready, task);
   else
      task_ready_push(task);
}

/* Where a task goes when it is pushed or comes back from a pass. */
static void task_place(retro_task_t *task)
{
   if (task_is_main_thread_task(task))
   {
      task_stack_push(&task_main_inbox, task);
      retro_eventcount_notify(&task_done_ec);
      return;
   }
#ifdef HAVE_GCD
   if (task->when)
   {
      task_stack_push(&task_timed_inbox, task);
      task_gcd_poke_keeper();
   }
   else
      task_ready_push(task);
#else
   if (task->when)
      task_stack_push(&task_timed_inbox, task);
   else
      task_ready_push(task);
   retro_eventcount_notify(&task_work_ec);
#endif
}

/* Moves what has come in into @list, and from it whatever is due or
 * woken, counting those in @released; returns the earliest 'when'
 * left, or 0. */
static retro_time_t task_timers_run(retro_atomic_ptr_t *inbox,
      retro_task_t **list, int *wakes_seen, unsigned *released)
{
   retro_time_t  now  = 0;
   retro_task_t *task = task_stack_take(inbox);
   int wakes          = retro_atomic_load_acquire_int(&task_timer_wakes);

   while (task)
   {
      retro_task_t *next = task->next;
      if (retro_atomic_load_acquire_int(&task->wake))
      {
         task_release(task);
         (*released)++;
      }
      else
         task_timed_insert(list, task);
      task = next;
   }

   if (wakes != *wakes_seen)
   {
      retro_task_t **link = list;
      *wakes_seen = wakes;
      while (*link)
      {
         task = *link;
         if (retro_atomic_load_acquire_int(&task->wake))
         {
            *link = task->next;
            task_release(task);
            (*released)++;
         }
         else
            link = &task->next;
      }
   }

   while (*list)
   {
      task = *list;
      if (!now)
         now = cpu_features_get_time_usec();
      /* Half a millisecond early, for the context switch. */
      if (task->when - 500 > now)
         return task->when;
      *list = task->next;
      task_release(task);
      (*released)++;
   }
   return 0;
}

/* Makes @task due now, whether it is parked, waiting for its start or
 * in its handler, from any thread. */
static void task_wake_task(retro_task_t *task)
{
   retro_atomic_store_release_int(&task->wake, 1);
   retro_atomic_fetch_add_seq_cst_int(&task_timer_wakes, 1);
#ifdef HAVE_GCD
   task_gcd_poke_keeper();
#else
   retro_eventcount_notify(&task_work_ec);
#endif
   retro_eventcount_notify(&task_done_ec);
}

/* --- push, wake, cancel, reset, find, retrieve --- */
static void retro_task_threaded_push_running(retro_task_t *task)
{
#ifndef HAVE_GCD
   if (task_is_parallel(task))
      retro_task_threaded_start_parallel(task->parallel_workers);
#endif
   retro_atomic_store_relaxed_int(&task->wake, 0);
   task->parked = false;
   if (task->when)
      retro_atomic_fetch_add_int(&tasks_scheduled_count, 1);
   retro_atomic_fetch_add_int(&tasks_running_count, 1);
   task_live_insert(task);
   task_place(task);
}

/* Calls @fn on each task still to be retired that @match accepts,
 * until it returns true. */
static bool task_live_each(bool done_too,
      bool (*match)(retro_task_t *task, void *data), void *data)
{
   int  e     = task_ep_enter();
   bool found = false;
   retro_task_t *task = (retro_task_t*)
      retro_atomic_load_acquire_ptr(&task_live);
   for (; task; task = TASK_LIVE_NEXT(task))
   {
      if (     !done_too
            && retro_atomic_load_acquire_int(&task->qstate) != TASK_Q_LIVE)
         continue;
      if (match(task, data))
      {
         found = true;
         break;
      }
   }
   task_ep_leave(e);
   return found;
}

static bool task_match_wake(retro_task_t *task, void *data)
{
   if (task->ident != *(uint32_t*)data)
      return false;
   task_wake_task(task);
   return true;
}

static void retro_task_threaded_wake(uint32_t ident)
{
   task_live_each(false, task_match_wake, &ident);
}

static bool task_match_cancel(retro_task_t *task, void *data)
{
   if (task != (retro_task_t*)data)
      return false;
   task_flags_set(task, RETRO_TASK_FLG_CANCELLED);
   task_wake_task(task);
   return true;
}

/* A cancelled task is woken, so a parked one sees the flag now rather
 * than at its horizon. */
static void retro_task_threaded_cancel(void *task)
{
   task_live_each(false, task_match_cancel, task);
}

static bool task_match_cancel_ident(retro_task_t *task, void *data)
{
   if (task->ident != *(uint32_t*)data)
      return false;
   task_flags_set(task, RETRO_TASK_FLG_CANCELLED);
   task_wake_task(task);
   return true;
}

static bool retro_task_threaded_cancel_ident(uint32_t ident)
{
   return task_live_each(false, task_match_cancel_ident, &ident);
}

static bool task_match_reset(retro_task_t *task, void *data)
{
   (void)data;
   task_flags_set(task, RETRO_TASK_FLG_CANCELLED);
   task_wake_task(task);
   return false;
}

/* Every task is cancelled and due now. */
static void retro_task_threaded_reset(void)
{
   task_live_each(false, task_match_reset, NULL);
}

struct task_find_args
{
   retro_task_finder_t func;
   void               *user_data;
};

static bool task_match_find(retro_task_t *task, void *data)
{
   struct task_find_args *a = (struct task_find_args*)data;
   return a->func(task, a->user_data);
}

/* Findable until fully retired: until its callback has run. */
static bool retro_task_threaded_find(
      retro_task_finder_t func, void *user_data)
{
   struct task_find_args a;
   a.func      = func;
   a.user_data = user_data;
   return task_live_each(true, task_match_find, &a);
}

static bool task_match_blocking(retro_task_t *task, void *data)
{
   (void)data;
   return task->type == TASK_TYPE_BLOCKING;
}

static bool task_threaded_has_blocking(void)
{
   return task_live_each(false, task_match_blocking, NULL);
}

struct task_retrieve_args
{
   task_retriever_data_t *data;
   task_retriever_info_t *tail;
};

static bool task_match_retrieve(retro_task_t *task, void *user)
{
   struct task_retrieve_args *a = (struct task_retrieve_args*)user;
   task_retriever_data_t *data  = a->data;
   task_retriever_info_t *info;
   if (task->handler != data->handler)
      return false;
   if (!(info = (task_retriever_info_t*)malloc(sizeof(*info))))
      return false;
   if (!(info->data = malloc(data->element_size)))
   {
      free(info);
      return false;
   }
   info->next = NULL;
   if (!data->func(task, info->data))
   {
      free(info->data);
      free(info);
      return false;
   }
   if (a->tail)
      a->tail->next = info;
   else
      data->list    = info;
   a->tail = info;
   return false;
}

static void retro_task_threaded_retrieve(task_retriever_data_t *data)
{
   struct task_retrieve_args a;
   a.data = data;
   a.tail = NULL;
   for (a.tail = data->list; a.tail && a.tail->next; a.tail = a.tail->next) { }
   task_live_each(false, task_match_retrieve, &a);
}

/* --- after a pass --- */
/* A finished task goes to the checking thread; an unfinished one parks
 * unless a wake came in while it ran, or goes to the back of the tasks
 * due now.  By whichever thread ran the pass. */
static void retro_task_threaded_settle(retro_task_t *task)
{
   if (retro_atomic_load_acquire_int(&task->flags) & RETRO_TASK_FLG_FINISHED)
   {
      retro_atomic_store_release_int(&task->qstate, TASK_Q_DONE);
      retro_atomic_fetch_add_int(&tasks_finished_count, 1);
      retro_atomic_fetch_sub_int(&tasks_running_count, 1);
      task_stack_push(&task_finished_stack, task);
      retro_eventcount_notify(&task_done_ec);
      return;
   }
   if (task->park && !retro_atomic_load_seq_cst_int(&task->wake))
   {
      task->when   = cpu_features_get_time_usec() + task->park;
      task->parked = true;
   }
   task->park = 0;
   task_place(task);
}

/* --- the checking thread's main-thread tasks --- */
/* Runs each main-thread task due at the start of the pass once,
 * within the handler budget; unfinished ones go back behind it. */
static void retro_task_threaded_run_main(void)
{
   /* A main-thread handler that waits on the queue gathers again from
    * inside this pass; the pass further up the stack runs the tasks
    * it took. */
   static bool in_run_main = false;
   retro_time_t started = 0;
   unsigned     n_ran   = 0;
   retro_task_t *pass;

   if (in_run_main || !task_main_ready.front)
      return;
   in_run_main = true;

   pass                  = task_main_ready.front;
   task_main_ready.front = NULL;
   task_main_ready.back  = NULL;

   if (task_handler_budget_usec)
      started = cpu_features_get_time_usec();

   while (pass)
   {
      retro_task_t *task = pass;

      if (   n_ran && task_handler_budget_usec
          && cpu_features_get_time_usec() - started >= task_handler_budget_usec)
      {
         /* The rest go first next time. */
         retro_task_t *rest = pass;
         while (rest)
         {
            retro_task_t *next = rest->next;
            task_queue_put(&task_main_ready, rest);
            rest = next;
         }
         break;
      }
      pass = task->next;
      task->next = NULL;

      retro_atomic_store_release_int(&task->wake, 0);
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

      if (retro_atomic_load_acquire_int(&task->flags) & RETRO_TASK_FLG_FINISHED)
         retro_task_threaded_settle(task);
      else
      {
         if (task->park && !retro_atomic_load_seq_cst_int(&task->wake))
         {
            task->when   = cpu_features_get_time_usec() + task->park;
            task->parked = true;
            task->park   = 0;
            task_timed_insert(&task_main_timed, task);
         }
         else
         {
            task->park = 0;
            task_queue_put(&task_main_ready, task);
         }
      }
   }

   in_run_main = false;
}

/* --- the gather --- */
static void retro_task_threaded_gather(void)
{
   task_queue_push_deferred();
   task_queue_cancel_deferred();
   if (retro_atomic_load_acquire_ptr(&task_orphans))
      task_orphans_reclaim();
   task_ep_reclaim();

   /* Nothing running and nothing finished, as on nearly every frame. */
   if (   !retro_atomic_load_acquire_int(&tasks_running_count)
       && !retro_atomic_load_acquire_int(&tasks_finished_count)
       && !task_retire.front)
      return;

   retro_atomic_fetch_add_int(&task_gather_seq, 1);

   {
      retro_task_t *task = (retro_task_t*)
         retro_atomic_load_acquire_ptr(&task_live);
      for (; task; task = TASK_LIVE_NEXT(task))
         if (retro_atomic_load_acquire_int(&task->qstate) == TASK_Q_LIVE)
            task_queue_push_progress(task);
   }

   {
      unsigned released = 0;
      task_timers_run(&task_main_inbox, &task_main_timed,
            &task_main_wakes_seen, &released);
   }
   retro_task_threaded_run_main();

   {
      /* Task callbacks can re-enter this function; the nested call
       * leaves the retiring to the outer one. */
      static bool in_gather = false;
      unsigned     done    = 0;
      retro_time_t started = (task_retire_budget_usec)
         ? cpu_features_get_time_usec() : 0;
      retro_task_t *task;

      if (in_gather)
         return;
      in_gather = true;

      task = task_stack_take(&task_finished_stack);
      while (task)
      {
         retro_task_t *next = task->next;
         task_queue_put(&task_retire, task);
         retro_atomic_fetch_sub_int(&tasks_finished_count, 1);
         task = next;
      }

      while (task_retire.front)
      {
         if (!task_retire_budget_allows(done, started))
            break;
         task = task_queue_get(&task_retire);
         if (!task_retire.front)
            task_retire.back = NULL;
         /* Still findable while its callback runs. */
         retro_task_internal_retire_callback(task);
         task_live_unlink(task);
         task_ep_retire(task);
         done++;
      }

      in_gather = false;
   }
}

/* Whether a task_queue_wait() has something to wait for: a task due,
 * running or parked, or one finished and not yet retired. */
static bool task_threaded_busy(void)
{
   return retro_atomic_load_acquire_int(&tasks_running_count)
         > retro_atomic_load_acquire_int(&tasks_scheduled_count)
      || retro_atomic_load_acquire_ptr(&task_finished_stack)
      || task_retire.front;
}

static void retro_task_threaded_wait(retro_task_condition_fn_t cond, void* data)
{
   bool wait = false;

   do
   {
      retro_task_threaded_gather();

      if ((wait = task_threaded_busy()))
      {
         /* Woken by a finish or a wake; the bound covers the caller's
          * own condition flipping, and a task that keeps yielding. */
         int key = retro_eventcount_prepare_wait(&task_done_ec);
         if (retro_atomic_load_acquire_ptr(&task_finished_stack))
            retro_eventcount_cancel_wait(&task_done_ec);
         else
            retro_eventcount_commit_wait_timeout(&task_done_ec, key, 1000);
      }
   } while (wait && (!cond || cond(data)));
}

#ifndef HAVE_GCD
/* --- workers --- */
static bool task_worker_has_work(const struct task_worker *self)
{
   return !task_ring_empty(task_ring_parallel)
      || (!self->parallel_only && !task_ring_empty(task_ring_plain));
}

static retro_task_t *task_worker_take(struct task_worker *self)
{
   retro_task_t *task = NULL;
   if (self->parallel_only)
      return task_ring_pop(task_ring_parallel);
   /* The one worker takes from both, starting with the other one each
    * time, so neither ring waits behind the other. */
   self->turn = !self->turn;
   if (self->turn)
   {
      if (!(task = task_ring_pop(task_ring_plain)))
         task = task_ring_pop(task_ring_parallel);
   }
   else if (!(task = task_ring_pop(task_ring_parallel)))
      task = task_ring_pop(task_ring_plain);
   return task;
}

static void threaded_worker(void *userdata)
{
   struct task_worker *self = (struct task_worker*)userdata;
   bool keeper              = false;

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
      retro_task_t *task = NULL;
      retro_time_t  next = 0;

      if (!retro_atomic_load_acquire_int(&worker_continue))
         break;
      if (!retro_atomic_load_acquire_int(&worker_go))
      {
         int key = retro_eventcount_prepare_wait(&task_work_ec);
         if (     retro_atomic_load_acquire_int(&worker_go)
               || !retro_atomic_load_acquire_int(&worker_continue))
            retro_eventcount_cancel_wait(&task_work_ec);
         else
            retro_eventcount_commit_wait(&task_work_ec, key);
         continue;
      }

      /* Whoever holds the token keeps the timers, including while it
       * waits for work, and hands it on before it runs a task. */
      if (!keeper)
         keeper = retro_atomic_cas_int(&task_timer_token, 0, 1);
      if (keeper)
      {
         unsigned released = 0;
         next = task_timers_run(&task_timed_inbox, &task_timed,
               &task_keeper_wakes_seen, &released);
         /* The one worker may be waiting for a plain task. */
         if (released)
            retro_eventcount_notify(&task_work_ec);
      }

      if (!(task = task_worker_take(self)))
      {
         int key = retro_eventcount_prepare_wait(&task_work_ec);
         if (     !retro_atomic_load_acquire_int(&worker_continue)
               || task_worker_has_work(self)
               || (keeper
                  && (retro_atomic_load_acquire_ptr(&task_timed_inbox)
                     ||    retro_atomic_load_acquire_int(&task_timer_wakes)
                        != task_keeper_wakes_seen))
               || (!keeper
                  && !retro_atomic_load_acquire_int(&task_timer_token)))
            retro_eventcount_cancel_wait(&task_work_ec);
         else if (keeper && next)
            retro_eventcount_commit_wait_timeout(&task_work_ec, key,
                  next - 500 - cpu_features_get_time_usec());
         else
            retro_eventcount_commit_wait(&task_work_ec, key);
         continue;
      }

      if (keeper)
      {
         keeper = false;
         retro_atomic_store_release_int(&task_timer_token, 0);
         /* Another idle worker keeps the timers meanwhile. */
         retro_eventcount_notify(&task_work_ec);
      }

      self->task = task;
      retro_atomic_store_release_int(&task->wake, 0);
      (void)retro_atomic_load_acquire_int(&task_gather_seq);
      retro_atomic_store_release_int(&self->state, TASK_WORKER_IN_HANDLER);
      task->handler(task);
      retro_atomic_fetch_add_int(&task_gather_seq, 1);

      /* Deinit gave up on this handler and took the task off the
       * queue; the globals may belong to a new init by now.  Clean up,
       * give the task back to be freed once no finder holds it, and let
       * go of the generation.  No callback runs. */
      if (!retro_atomic_cas_int(&self->state,
               TASK_WORKER_IN_HANDLER, TASK_WORKER_IDLE))
      {
         struct task_generation *gen = self->gen;
         if (task->cleanup)
            task->cleanup(task);
         task->cleanup = NULL;
         task_stack_push(&task_orphans, task);
         if (retro_atomic_fetch_sub_int(&gen->refs, 1) == 1)
            free(gen);
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

   if (keeper)
   {
      retro_atomic_store_release_int(&task_timer_token, 0);
      retro_eventcount_notify(&task_work_ec);
   }
   task_worker_tid_set(self->slot, NULL);
   retro_atomic_store_release_int(&self->state, TASK_WORKER_EXITED);
   retro_eventcount_notify(&task_exit_ec);
}
#endif

/* --- init and deinit --- */
static void task_lf_structures_free(void)
{
   free(task_ring_plain);
   free(task_ring_parallel);
   task_ring_plain    = NULL;
   task_ring_parallel = NULL;
   if (task_ec_ready)
   {
#ifndef HAVE_GCD
      retro_eventcount_free(&task_work_ec);
      retro_eventcount_free(&task_exit_ec);
#endif
      retro_eventcount_free(&task_done_ec);
      task_ec_ready = false;
   }
}

static bool task_lf_structures_new(void)
{
   task_ring_plain    = task_ring_new();
   task_ring_parallel = task_ring_new();
#ifdef HAVE_GCD
   if (     task_ring_plain && task_ring_parallel
         && retro_eventcount_init(&task_done_ec))
   {
      task_ec_ready = true;
      return true;
   }
#else
   if (     task_ring_plain && task_ring_parallel
         && retro_eventcount_init(&task_work_ec))
   {
      if (retro_eventcount_init(&task_done_ec))
      {
         if (retro_eventcount_init(&task_exit_ec))
         {
            task_ec_ready = true;
            return true;
         }
         retro_eventcount_free(&task_done_ec);
      }
      retro_eventcount_free(&task_work_ec);
   }
#endif
   free(task_ring_plain);
   free(task_ring_parallel);
   task_ring_plain    = NULL;
   task_ring_parallel = NULL;
   return false;
}

/* Takes over the regular queue's lists: every task onto the live list
 * and where it waits.  Before any worker runs. */
static void retro_task_threaded_take_lists(void)
{
   retro_task_t *task;
   int running  = 0;
   int finished = 0;

   while ((task = task_queue_get(&tasks_finished)))
   {
      task_live_insert(task);
      retro_atomic_store_relaxed_int(&task->qstate, TASK_Q_DONE);
      task_queue_put(&task_retire, task);
      finished++;
   }
   while ((task = task_queue_get(&tasks_running)))
   {
      retro_atomic_store_relaxed_int(&task->wake, 0);
      task->parked = false;
      if (task->when)
         retro_atomic_fetch_add_int(&tasks_scheduled_count, 1);
      task_live_insert(task);
      task_place(task);
      running++;
   }
   retro_atomic_store_release_int(&tasks_running_count,  running);
   retro_atomic_store_release_int(&tasks_finished_count, 0);
   (void)finished;
}

/* Hands every task back to the regular queue's lists, once no worker
 * runs.  A park ends with the runner that set it. */
static void retro_task_threaded_give_lists(void)
{
   retro_task_t *task;
   retro_task_t *list;

   while ((task = task_queue_get(&task_retire)))
      task_queue_put(&tasks_finished, task);
   for (task = task_stack_take(&task_finished_stack); task; task = list)
   {
      list = task->next;
      task_queue_put(&tasks_finished, task);
   }

#define TASK_GIVE(t) \
   do { \
      if ((t)->parked) \
      { \
         (t)->when   = 0; \
         (t)->parked = false; \
      } \
      task_queue_put(&tasks_running, (t)); \
   } while (0)

   while ((task = task_queue_get(&task_main_ready)))
      TASK_GIVE(task);
   for (task = task_main_timed; task; task = list)
   {
      list = task->next;
      TASK_GIVE(task);
   }
   task_main_timed = NULL;
   for (task = task_stack_take(&task_main_inbox); task; task = list)
   {
      list = task->next;
      TASK_GIVE(task);
   }
   for (task = task_timed; task; task = list)
   {
      list = task->next;
      TASK_GIVE(task);
   }
   task_timed = NULL;
   for (task = task_stack_take(&task_timed_inbox); task; task = list)
   {
      list = task->next;
      TASK_GIVE(task);
   }
   while ((task = task_ring_pop(task_ring_plain)))
      TASK_GIVE(task);
   while ((task = task_ring_pop(task_ring_parallel)))
      TASK_GIVE(task);
#undef TASK_GIVE

   retro_atomic_store_release_ptr(&task_live, NULL);
   retro_atomic_store_release_int(&tasks_scheduled_count, 0);
#ifndef HAVE_GCD
   retro_atomic_store_release_int(&task_timer_token, 0);
#endif
}

/* The tasks the workers deinit left behind gave back, freed as any
 * retired one is. */
static void task_orphans_reclaim(void)
{
   retro_task_t *task = task_stack_take(&task_orphans);
   while (task)
   {
      retro_task_t *next = task->next;
      task_ep_retire(task);
      task = next;
   }
}

#ifndef HAVE_GCD
/* Starts on the pushing thread whatever extra workers are missing; the
 * compare-and-swap decides which push starts which. */
static void retro_task_threaded_start_parallel(unsigned needed)
{
   unsigned want = (unsigned)retro_atomic_load_acquire_int(&parallel_wanted);
   unsigned from, i;

   if (needed > want)
      want = needed;
   if (want > TASK_PARALLEL_MAX)
      want = TASK_PARALLEL_MAX;

   for (;;)
   {
      from = (unsigned)retro_atomic_load_acquire_int(&parallel_started);
      if (from >= want)
         return;
      if (retro_atomic_cas_int(&parallel_started, (int)from, (int)want))
         break;
   }

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
      retro_atomic_fetch_add_int(&task_gen->refs, 1);
#ifdef TASK_WORKER_BELOW_CREATOR
      w->prio          = worker_self->prio;
#endif
      parallel_self[i] = w;
      if (!(t = sthread_create(threaded_worker, w)))
      {
         parallel_self[i] = NULL;
         retro_atomic_fetch_sub_int(&task_gen->refs, 1);
         free(w);
         break;
      }
      parallel_thread[i] = t;
      retro_atomic_fetch_add_int(&parallel_created, 1);
   }
}

static void retro_task_threaded_join_parallel(void)
{
   unsigned i;
   for (i = 0; i < TASK_PARALLEL_MAX; i++)
   {
      if (!parallel_thread[i])
         continue;
      sthread_join(parallel_thread[i]);
      free(parallel_self[i]);
      retro_atomic_fetch_sub_int(&task_gen->refs, 1);
      parallel_thread[i] = NULL;
      parallel_self[i]   = NULL;
   }
   retro_atomic_store_release_int(&parallel_created, 0);
   retro_atomic_store_release_int(&parallel_started, 0);
}

static bool retro_task_threaded_init(void)
{
   if (!task_lf_structures_new())
      return false;

   worker_self = (struct task_worker*)calloc(1, sizeof(*worker_self));
   task_gen    = (struct task_generation*)calloc(1, sizeof(*task_gen));
   if (!worker_self || !task_gen)
   {
      free(worker_self);
      free(task_gen);
      worker_self = NULL;
      task_gen    = NULL;
      task_lf_structures_free();
      return false;
   }
   retro_atomic_int_init(&task_gen->refs, 1);
   retro_atomic_store_release_int(&worker_self->state, TASK_WORKER_IDLE);
   worker_self->gen = task_gen;
#ifdef TASK_WORKER_BELOW_CREATOR
   worker_self->prio          = 0x2C;
   svcGetThreadPriority(&worker_self->prio, CUR_THREAD_HANDLE);
   /* 0x3F is the lowest priority either kernel has. */
   if (worker_self->prio < 0x3F)
      worker_self->prio++;
#endif

   retro_task_threaded_take_lists();
   retro_atomic_store_release_int(&worker_go, 0);
   retro_atomic_store_release_int(&worker_continue, 1);

   /* Without a worker there is nobody to run what gets queued, which
    * is why this reports failure rather than leaving the queue pointed
    * at an implementation that cannot service it. */
   if ((worker_thread = sthread_create(threaded_worker, worker_self)))
   {
      retro_atomic_fetch_add_int(&task_gen->refs, 1);
      task_wake_gate_open();
      retro_atomic_store_release_int(&worker_go, 1);
      retro_eventcount_notify(&task_work_ec);
      return true;
   }

   retro_atomic_store_release_int(&worker_continue, 0);
   retro_task_threaded_give_lists();
   free(worker_self);
   free(task_gen);
   worker_self = NULL;
   task_gen    = NULL;
   task_lf_structures_free();
   return false;
}

/* Waits, until @deadline, for @w to leave its loop; only a worker stuck
 * in a handler can outlast the bound.  If that task is detachable, its
 * handler touches nothing but the task: the task is taken off the
 * queue and the worker left to finish alone, and this returns true.
 * Any other handler may hold state teardown is about to free, so the
 * wait goes on. */
static bool retro_task_threaded_stop_worker(struct task_worker *w,
      retro_time_t deadline)
{
   for (;;)
   {
      int          key   = retro_eventcount_prepare_wait(&task_exit_ec);
      int          state = retro_atomic_load_acquire_int(&w->state);
      retro_time_t left;

      if (state == TASK_WORKER_EXITED)
      {
         retro_eventcount_cancel_wait(&task_exit_ec);
         return false;
      }

      left = deadline - cpu_features_get_time_usec();
      if (left > 0)
      {
         retro_eventcount_commit_wait_timeout(&task_exit_ec, key, left);
         continue;
      }

      if (state == TASK_WORKER_IN_HANDLER)
      {
         retro_task_t *task = w->task;
         bool detachable    = (retro_atomic_load_acquire_int(&task->flags)
               & RETRO_TASK_FLG_DETACHABLE) != 0;

         if (detachable && retro_atomic_cas_int(&w->state,
                  TASK_WORKER_IN_HANDLER, TASK_WORKER_ORPHANED))
         {
            retro_eventcount_cancel_wait(&task_exit_ec);
            task_worker_tid_set(w->slot, NULL);
            task_live_unlink(task);
            retro_atomic_fetch_sub_int(&tasks_running_count, 1);
            return true;
         }
      }

      /* Not in a detachable handler: it is either leaving or can only
       * leave by returning. */
      retro_eventcount_commit_wait(&task_exit_ec, key);
   }
}

/* Stops every worker, the extra ones first, against one bound.  A
 * worker left behind keeps its generation until it lets go. */
static void retro_task_threaded_deinit(void)
{
   struct task_generation *gen = task_gen;
   retro_time_t            deadline;
   unsigned                i;
   bool                    left_behind;

   retro_atomic_store_release_int(&worker_continue, 0);
   retro_eventcount_notify(&task_work_ec);

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

   retro_task_threaded_join_parallel();
   if (!left_behind)
   {
      sthread_join(worker_thread);
      retro_atomic_fetch_sub_int(&gen->refs, 1);
      free(worker_self);
   }

   retro_task_threaded_give_lists();
   task_orphans_reclaim();
   /* Both epochs' worth, with no finder left inside. */
   task_ep_reclaim();
   task_ep_reclaim();
   task_lf_structures_free();
   if (retro_atomic_fetch_sub_int(&gen->refs, 1) == 1)
      free(gen);

   task_gen        = NULL;
   worker_thread   = NULL;
   worker_self     = NULL;
}
#else
/* --- on GCD ---
 * GCD runs the passes.  Each push of a task due now makes a call that
 * takes tasks from its ring: from the plain ring on a serial queue, so
 * plain tasks never overlap, and from the parallel ring on a
 * concurrent one.  A serial keeper queue holds the tasks waiting for
 * their 'when', with one timer for the earliest; a push of one and
 * every wake nudge it.  An init is a generation that each call holds:
 * a call that finds it stopped does nothing, so deinit waits only for
 * the calls past that check. */
struct task_gcd
{
   retro_atomic_int_t refs;
   retro_atomic_int_t live;
   /* Calls past their check of live. */
   retro_atomic_int_t busy;
   /* For deinit: busy falls, or a handler starts. */
   retro_eventcount_t idle_ec;
   dispatch_queue_t   plain;
   dispatch_queue_t   parallel;
   dispatch_queue_t   keeper;
   dispatch_source_t  timer;
};

static struct task_gcd *task_gcd = NULL;

#define TASK_RUN_IDLE     0
#define TASK_RUN_HANDLER  1
#define TASK_RUN_ORPHANED 2

static void task_gcd_unref(void *data)
{
   struct task_gcd *g = (struct task_gcd*)data;
   if (retro_atomic_fetch_sub_int(&g->refs, 1) != 1)
      return;
   dispatch_release(g->timer);
   dispatch_release(g->keeper);
   dispatch_release(g->parallel);
   dispatch_release(g->plain);
   retro_eventcount_free(&g->idle_ec);
   free(g);
}

static bool task_gcd_enter(struct task_gcd *g)
{
   retro_atomic_fetch_add_seq_cst_int(&g->busy, 1);
   return retro_atomic_load_seq_cst_int(&g->live) != 0;
}

static void task_gcd_leave(struct task_gcd *g)
{
   retro_atomic_fetch_add_seq_cst_int(&g->busy, -1);
   if (!retro_atomic_load_seq_cst_int(&g->live))
      retro_eventcount_notify(&g->idle_ec);
}

/* Marks the thread as one of the queue's own while a handler runs. */
static void task_gcd_mark(retro_task_t *task)
{
#ifdef TASK_PUSH_DEFER
   if (retro_atomic_load_acquire_int(&task_gcd_key_made))
      pthread_setspecific(task_gcd_key, task);
#else
   (void)task;
#endif
}

/* Takes tasks from the ring until it finds none.  A pop can come back
 * empty while a later task is in: behind a place another push has
 * taken and not yet filled, or while another call refills the ring
 * from its overflow.  That push makes its own call once it is in, and
 * the call refilling goes on taking; so every task is taken by the
 * call its push made or by one still taking.  A pass that comes back
 * unfinished and due goes back in for this call to take again, with
 * no call of its own, so calls never pile up behind a running one. */
static void task_gcd_run(struct task_gcd *g, bool parallel)
{
   struct task_ring *ring;
   retro_task_t     *task;

   if (!task_gcd_enter(g))
   {
      task_gcd_leave(g);
      task_gcd_unref(g);
      return;
   }
   ring = parallel ? task_ring_parallel : task_ring_plain;

   while (     retro_atomic_load_seq_cst_int(&g->live)
         && (task = task_ring_pop(ring)))
   {
      int flags;

      retro_atomic_store_release_int(&task->wake, 0);
      (void)retro_atomic_load_acquire_int(&task_gather_seq);
      retro_atomic_fetch_add_seq_cst_int(&task->run, TASK_RUN_HANDLER);
      if (!retro_atomic_load_seq_cst_int(&g->live))
         retro_eventcount_notify(&g->idle_ec);
      task_gcd_mark(task);
      task->handler(task);
      task_gcd_mark(NULL);
      retro_atomic_fetch_add_int(&task_gather_seq, 1);

      /* Deinit let go of the task and of this call: clean up and give
       * the task back to be freed once no finder holds it.  No
       * callback runs. */
      if (!retro_atomic_cas_int(&task->run,
               TASK_RUN_HANDLER, TASK_RUN_IDLE))
      {
         if (task->cleanup)
            task->cleanup(task);
         task->cleanup = NULL;
         task_stack_push(&task_orphans, task);
         task_gcd_unref(g);
         return;
      }

      flags = retro_atomic_load_acquire_int(&task->flags);
      if (     !(flags & RETRO_TASK_FLG_FINISHED) && !task->when
            && !(task->park && !retro_atomic_load_seq_cst_int(&task->wake)))
      {
         task->park = 0;
         task_ring_push(ring, task);
      }
      else
         retro_task_threaded_settle(task);
   }
   task_gcd_leave(g);
   task_gcd_unref(g);
}

static void task_gcd_run_plain(void *data)
{
   task_gcd_run((struct task_gcd*)data, false);
}

static void task_gcd_run_parallel(void *data)
{
   task_gcd_run((struct task_gcd*)data, true);
}

static void task_gcd_post_ready(bool parallel)
{
   struct task_gcd *g = task_gcd;
   retro_atomic_fetch_add_int(&g->refs, 1);
   if (parallel)
      dispatch_async_f(g->parallel, g, task_gcd_run_parallel);
   else
      dispatch_async_f(g->plain, g, task_gcd_run_plain);
}

/* Releases what is due or woken, and sets the timer for the rest.
 * Each nudge is a call of its own, so no thread that has yet to make
 * its call holds back another's. */
static void task_gcd_keep(void *data)
{
   struct task_gcd *g = (struct task_gcd*)data;

   if (task_gcd_enter(g))
   {
      unsigned        released = 0;
      retro_time_t    next;
      dispatch_time_t start    = DISPATCH_TIME_FOREVER;

      if ((next = task_timers_run(&task_timed_inbox, &task_timed,
               &task_keeper_wakes_seen, &released)))
      {
         retro_time_t delay = next - 500 - cpu_features_get_time_usec();
         start              = DISPATCH_TIME_NOW;
         if (delay > 0)
            start = dispatch_time(DISPATCH_TIME_NOW,
                  (int64_t)delay * (int64_t)NSEC_PER_USEC);
      }
      dispatch_source_set_timer(g->timer, start, DISPATCH_TIME_FOREVER, 0);
   }
   task_gcd_leave(g);
}

static void task_gcd_keep_posted(void *data)
{
   task_gcd_keep(data);
   task_gcd_unref(data);
}

static void task_gcd_poke_keeper(void)
{
   struct task_gcd *g = task_gcd;
   retro_atomic_fetch_add_int(&g->refs, 1);
   dispatch_async_f(g->keeper, g, task_gcd_keep_posted);
}

static bool retro_task_threaded_init(void)
{
   dispatch_queue_t global =
      dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);
   struct task_gcd *g;

   if (!task_lf_structures_new())
      return false;
   if (!(g = (struct task_gcd*)calloc(1, sizeof(*g))))
   {
      task_lf_structures_free();
      return false;
   }
   if (!retro_eventcount_init(&g->idle_ec))
   {
      free(g);
      task_lf_structures_free();
      return false;
   }
   g->plain    = dispatch_queue_create("org.libretro.tasks",
         DISPATCH_QUEUE_SERIAL);
   g->parallel = dispatch_queue_create("org.libretro.tasks.parallel",
         DISPATCH_QUEUE_CONCURRENT);
   g->keeper   = dispatch_queue_create("org.libretro.tasks.timers",
         DISPATCH_QUEUE_SERIAL);
   /* Last: a source is made suspended, and GCD will not release one
    * that is. */
   if (g->plain && g->parallel && g->keeper)
      g->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
            0, 0, g->keeper);
   if (!g->timer)
   {
      if (g->keeper)
         dispatch_release(g->keeper);
      if (g->parallel)
         dispatch_release(g->parallel);
      if (g->plain)
         dispatch_release(g->plain);
      retro_eventcount_free(&g->idle_ec);
      free(g);
      task_lf_structures_free();
      return false;
   }
#ifdef TASK_PUSH_DEFER
   /* Made once and never deleted, so that no finder meets a key on its
    * way out. */
   if (     !retro_atomic_load_relaxed_int(&task_gcd_key_made)
         && !pthread_key_create(&task_gcd_key, NULL))
      retro_atomic_store_release_int(&task_gcd_key_made, 1);
#endif

   /* Nothing runs before the wake gate is open, so a wake from a
    * first pass is not turned away. */
   dispatch_set_target_queue(g->plain,    global);
   dispatch_set_target_queue(g->parallel, global);
   dispatch_set_target_queue(g->keeper,   global);
   dispatch_suspend(g->plain);
   dispatch_suspend(g->parallel);
   dispatch_suspend(g->keeper);
   dispatch_set_context(g->timer, g);
   dispatch_source_set_event_handler_f(g->timer, task_gcd_keep);
   dispatch_source_set_cancel_handler_f(g->timer, task_gcd_unref);
   /* Deinit's and the timer's. */
   retro_atomic_int_init(&g->refs, 2);
   retro_atomic_int_init(&g->live, 1);
   retro_atomic_int_init(&g->busy, 0);
   task_gcd = g;

   retro_task_threaded_take_lists();
   dispatch_resume(g->timer);
   task_wake_gate_open();
   dispatch_resume(g->keeper);
   dispatch_resume(g->parallel);
   dispatch_resume(g->plain);
   return true;
}

/* Past the bound: lets go of each task in a detachable handler, and of
 * the call it runs on, which owns it from then on.  Any other handler
 * may hold state teardown is about to free, so it is waited for. */
static bool task_gcd_let_go(struct task_gcd *g)
{
   bool          any  = false;
   retro_task_t *next;
   retro_task_t *task = (retro_task_t*)
      retro_atomic_load_acquire_ptr(&task_live);

   for (; task; task = next)
   {
      next = TASK_LIVE_NEXT(task);
      if (     (retro_atomic_load_acquire_int(&task->flags)
               & RETRO_TASK_FLG_DETACHABLE)
            && retro_atomic_cas_int(&task->run,
               TASK_RUN_HANDLER, TASK_RUN_ORPHANED))
      {
         task_live_unlink(task);
         retro_atomic_fetch_sub_int(&tasks_running_count, 1);
         retro_atomic_fetch_add_seq_cst_int(&g->busy, -1);
         any = true;
      }
   }
   return any;
}

/* Stops the generation and waits for its calls past their check, up to
 * the bound for those in a detachable handler. */
static void retro_task_threaded_deinit(void)
{
   struct task_gcd *g        = task_gcd;
   retro_time_t     deadline = cpu_features_get_time_usec()
      + TASK_QUEUE_DETACH_AFTER_US;

   retro_atomic_store_release_int(&g->live, 0);
   retro_atomic_thread_fence_seq_cst();
   dispatch_source_cancel(g->timer);

   for (;;)
   {
      int          key = retro_eventcount_prepare_wait(&g->idle_ec);
      retro_time_t left;

      if (!retro_atomic_load_seq_cst_int(&g->busy))
      {
         retro_eventcount_cancel_wait(&g->idle_ec);
         break;
      }
      left = deadline - cpu_features_get_time_usec();
      if (left > 0)
         retro_eventcount_commit_wait_timeout(&g->idle_ec, key, left);
      else if (task_gcd_let_go(g))
         retro_eventcount_cancel_wait(&g->idle_ec);
      else
         retro_eventcount_commit_wait(&g->idle_ec, key);
   }

   retro_task_threaded_give_lists();
   task_orphans_reclaim();
   /* Both epochs' worth, with no finder left inside. */
   task_ep_reclaim();
   task_ep_reclaim();
   task_lf_structures_free();
   task_gcd = NULL;
   task_gcd_unref(g);
}
#endif

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
static void task_wake_gate_leave(void);

static bool task_wake_gate_enter(void)
{
#ifdef TASK_WAKE_GATE
   if (retro_atomic_load_acquire_int(&task_switching))
      return false;
   retro_atomic_fetch_add_seq_cst_int(&task_wakers, 1);
   if (retro_atomic_load_seq_cst_int(&task_switching))
   {
      task_wake_gate_leave();
      return false;
   }
#endif
   return true;
}

static void task_wake_gate_leave(void)
{
#ifdef TASK_WAKE_GATE
#ifdef TASK_GATE_WAITS
   retro_atomic_fetch_add_seq_cst_int(&task_wakers, -1);
   if (retro_atomic_load_seq_cst_int(&task_switching))
      retro_eventcount_notify(&task_gate_ec);
#else
   retro_atomic_fetch_sub_int(&task_wakers, 1);
#endif
#endif
}

/* Waits for every waker past the gate to leave it.  The gate is shut,
 * so no other gets in, and the ones inside only run queue code. */
static void task_wake_gate_drain(void)
{
#ifdef TASK_GATE_WAITS
   if (!task_gate_ec_ready)
      return;
   for (;;)
   {
      int key = retro_eventcount_prepare_wait(&task_gate_ec);
      if (!retro_atomic_load_seq_cst_int(&task_wakers))
      {
         retro_eventcount_cancel_wait(&task_gate_ec);
         return;
      }
      retro_eventcount_commit_wait(&task_gate_ec, key);
   }
#endif
}

/* Deinitializes the task system.
 * This deinitializes the task system.
 * The tasks that are running at
 * the moment will stay on hold */
void task_queue_deinit(void)
{
   task_wake_gate_close();
   /* What the runner frees, a waker inside may still be using. */
   task_wake_gate_drain();
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
#ifdef TASK_GATE_WAITS
   if (!task_gate_ec_ready)
      task_gate_ec_ready = retro_eventcount_init(&task_gate_ec);
#endif
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
      impl_current         = &impl_threaded;
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
   /* not one deinit has let go of */
   if (retro_atomic_load_acquire_int(&task_gcd_key_made))
   {
      retro_task_t *task = (retro_task_t*)pthread_getspecific(task_gcd_key);
      if (task)
         return retro_atomic_load_acquire_int(&task->run)
            == TASK_RUN_HANDLER;
   }
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
#ifdef TASK_QUEUE_LF
   /* for a title or error assigned to the field before the push */
   retro_atomic_store_release_ptr(&task->title_shown, task->title);
   retro_atomic_store_release_ptr(&task->error_shown, task->error);
#endif
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

#ifdef TASK_QUEUE_LF
      if (impl_current == &impl_threaded)
         found = task_threaded_has_blocking();
      else
#elif defined(HAVE_THREADS)
      slock_lock(running_lock);
#endif
      for (running = tasks_running.front; running; running = running->next)
      {
         if (running->type == TASK_TYPE_BLOCKING)
         {
            found = true;
            break;
         }
      }

#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
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
   const char *title;
   const char *error;
#ifdef TASK_QUEUE_LF
   int e;
#endif

   retro_task_t *t = (retro_task_t*)task;

   snapshot->title    = NULL;
   snapshot->error    = NULL;
   /* before the strings: see the header */
   snapshot->flags    = (uint8_t)retro_atomic_load_acquire_int(&t->flags);
   snapshot->progress = (int8_t)retro_atomic_load_acquire_int(&t->progress);
#ifdef TASK_QUEUE_LF
   e     = task_ep_enter();
   title = TASK_SHOWN(t, title_shown);
   error = TASK_SHOWN(t, error_shown);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   title = task->title;
   error = task->error;
#endif
   if (title)
   {
      len = strlen(title) + 1;
      if ((snapshot->title = (char*)malloc(len)))
         memcpy(snapshot->title, title, len);
      else
         success = false;
   }
   if (success && error)
   {
      len = strlen(error) + 1;
      if ((snapshot->error = (char*)malloc(len)))
         memcpy(snapshot->error, error, len);
      else
         success = false;
   }
#ifdef TASK_QUEUE_LF
   task_ep_leave(e);
#elif defined(HAVE_THREADS)
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
#ifdef TASK_QUEUE_LF
   task->error = err;
   retro_atomic_store_release_ptr(&task->error_shown, err);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   task->error = err;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
#endif
}

void task_set_progress(retro_task_t *task, int8_t progress)
{
   retro_atomic_store_release_int(&task->progress, progress);
}

void task_set_title(retro_task_t *task, char *title)
{
#ifdef TASK_QUEUE_LF
   task->title = title;
   retro_atomic_store_release_ptr(&task->title_shown, title);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   task->title = title;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
#endif
}

/* With the lock-free runner the task's hand-off to the thread that
 * retires it is what orders the store. */
void task_set_data(retro_task_t *task, void *data)
{
#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
   slock_lock(running_lock);
#endif
   task->task_data = data;
#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
   slock_unlock(running_lock);
#endif
}

void task_free_title(retro_task_t *task)
{
#ifdef TASK_QUEUE_LF
   char *old   = task->title;
   task->title = NULL;
   retro_atomic_store_release_ptr(&task->title_shown, NULL);
   task_str_retire(old);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   if (task->title)
      free(task->title);
   task->title = NULL;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
#endif
}

void task_free_error(retro_task_t *task)
{
#ifdef TASK_QUEUE_LF
   char *old   = task->error;
   task->error = NULL;
   retro_atomic_store_release_ptr(&task->error_shown, NULL);
   task_str_retire(old);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   if (task->error)
      free(task->error);
   task->error = NULL;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
#endif
}

void* task_get_data(retro_task_t *task)
{
   void *data = NULL;

#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
   slock_lock(running_lock);
#endif
   data = task->task_data;
#if defined(HAVE_THREADS) && !defined(TASK_QUEUE_LF)
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
#ifdef TASK_QUEUE_LF
   s = (char*)retro_atomic_load_acquire_ptr(&task->error_shown);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   s = task->error;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
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

#ifdef TASK_QUEUE_LF
   title = (char*)retro_atomic_load_acquire_ptr(&task->title_shown);
#else
#ifdef HAVE_THREADS
   slock_lock(property_lock);
#endif
   title = task->title;
#ifdef HAVE_THREADS
   slock_unlock(property_lock);
#endif
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
#ifdef RETRO_ATOMIC_HAS_PTR
   retro_atomic_ptr_init(&task->live_next, NULL);
   retro_atomic_ptr_init(&task->title_shown, NULL);
   retro_atomic_ptr_init(&task->error_shown, NULL);
#else
   task->live_next         = NULL;
   task->title_shown       = NULL;
   task->error_shown       = NULL;
#endif
   retro_atomic_int_init(&task->qstate, 0);
   retro_atomic_int_init(&task->run, 0);
   retro_atomic_int_init(&task->wake, 0);
   task->when              = 0;
   task->park              = 0;
   task->parked            = false;
   task->woken             = false;
   task->busy              = false;
   task->parallel_workers  = 0;

   return task;
}
