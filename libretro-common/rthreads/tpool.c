/*
 * Copyright (c) 2010-2020 The RetroArch team
 * Copyright (c) 2017 John Schember <john@nachtimwald.com>
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (tpool.c).
 * ---------------------------------------------------------------------------------------
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */

#include <stdlib.h>
#include <boolean.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/tpool.h>

/* Work object which will sit in a queue
 * waiting for the pool to process it.
 *
 * It is a singly linked list acting as a FIFO queue. */
struct tpool_work
{
   thread_func_t      func;  /* Function to be called. */
   void              *arg;   /* Data to be passed to func. */
   struct tpool_work *next;  /* Next work item in the queue. */
};
typedef struct tpool_work tpool_work_t;

struct tpool
{
   tpool_work_t    *work_first;   /* First work item in the work queue. */
   tpool_work_t    *work_last;    /* Last work item in the work queue. */
   slock_t         *work_mutex;   /* Mutex protecting inserting and removing work from the work queue. */
   scond_t         *work_cond;    /* Conditional to signal when there is work to process. */
   scond_t         *exit_cond;    /* Signalled when the last thread leaves, at teardown. */
   size_t           thread_cnt;   /* Total number of threads within the pool. */
   /* tpool_wait() sleeps here until outstanding reaches zero; the
    * thread that retires the last item wakes it, with no lock. */
   retro_eventcount_t idle;
   /* Read without work_mutex by tpool_help and tpool_wait, so that a
    * pool with an empty queue answers both without touching it.
    * queued, and outstanding's increments, are written under the mutex
    * alongside the queue they count; outstanding is retired without
    * it, see tpool_retire(). */
   retro_atomic_int_t queued;     /* Work items sitting in the queue. */
   retro_atomic_int_t outstanding;/* Queued plus in progress. */
   retro_atomic_int_t stop;       /* Marker to tell the work threads to exit. */
};

static tpool_work_t *tpool_work_create(thread_func_t func, void *arg)
{
   tpool_work_t *work;

   if (!func)
      return NULL;

   work       = (tpool_work_t*)calloc(1, sizeof(*work));
   if (!work)
      return NULL;
   work->func = func;
   work->arg  = arg;
   work->next = NULL;
   return work;
}

static void tpool_work_destroy(tpool_work_t *work)
{
   free(work);
}

/* One item done: the thread that retires the last outstanding one
 * wakes tpool_wait(). The pool lock stays out of it, except on the
 * volatile backend, whose read-modify-writes are not indivisible and
 * so are kept under the mutex the increments are made under. */
static void tpool_retire(tpool_t *tp)
{
   int left;
#ifdef RETRO_ATOMIC_BACKEND_VOLATILE
   slock_lock(tp->work_mutex);
   left = retro_atomic_fetch_sub_int(&tp->outstanding, 1) - 1;
   slock_unlock(tp->work_mutex);
#else
   left = retro_atomic_fetch_sub_int(&tp->outstanding, 1) - 1;
#endif
   if (left == 0)
      retro_eventcount_notify(&tp->idle);
}

/* Pull the first work item out of the queue. */
static tpool_work_t *tpool_work_get(tpool_t *tp)
{
   tpool_work_t *work;

   if (!tp)
      return NULL;

   work = tp->work_first;
   if (!work)
      return NULL;

   if (!work->next)
   {
      tp->work_first = NULL;
      tp->work_last  = NULL;
   }
   else
      tp->work_first = work->next;

   return work;
}

static void tpool_worker(void *arg)
{
   tpool_work_t *work = NULL;
   tpool_t      *tp   = (tpool_t*)arg;

   for (;;)
   {
      slock_lock(tp->work_mutex);

      /* Wait until there is work available or we are told to stop.
       * Loop handles spurious wakeups. */
      while (!tp->work_first && !retro_atomic_load_acquire_int(&tp->stop))
         scond_wait(tp->work_cond, tp->work_mutex);

      /* Re-check stop after waking from the conditional. */
      if (retro_atomic_load_acquire_int(&tp->stop))
         break;

      /* Try to pull work from the queue. */
      work = tpool_work_get(tp);
      if (work)
         retro_atomic_fetch_sub_int(&tp->queued, 1);
      slock_unlock(tp->work_mutex);

      /* Call the work function and let it process. */
      if (work)
      {
         work->func(work->arg);
         tpool_work_destroy(work);
         tpool_retire(tp);
      }
   }

   /* Unlocking is this thread's last touch of tp: tpool_destroy()
    * frees it once thread_cnt reads zero under the same lock. */
   tp->thread_cnt--;
   if (tp->thread_cnt == 0)
      scond_signal(tp->exit_cond);
   slock_unlock(tp->work_mutex);
}

tpool_t *tpool_create_with_stack_size(size_t num, size_t stack_size)
{
   tpool_t   *tp;
   sthread_t *thread;
   size_t     i;

   if (num == 0)
      num = 2;

   tp               = (tpool_t*)calloc(1, sizeof(*tp));
   if (!tp)
      return NULL;

   tp->thread_cnt   = num;

   tp->work_mutex   = slock_new();
   tp->work_cond    = scond_new();
   tp->exit_cond    = scond_new();

   if (     !tp->work_mutex || !tp->work_cond || !tp->exit_cond
         || !retro_eventcount_init(&tp->idle))
   {
      if (tp->work_mutex)
         slock_free(tp->work_mutex);
      if (tp->work_cond)
         scond_free(tp->work_cond);
      if (tp->exit_cond)
         scond_free(tp->exit_cond);
      free(tp);
      return NULL;
   }

   tp->work_first   = NULL;
   tp->work_last    = NULL;

   /* Create the requested number of threads and detach them. */
   tp->thread_cnt   = 0;
   for (i = 0; i < num; i++)
   {
      thread = stack_size
            ? sthread_create_with_stack_size(tpool_worker, tp, stack_size)
            : sthread_create(tpool_worker, tp);
      if (!thread)
         continue;
      tp->thread_cnt++;
      sthread_detach(thread);
   }

   /* If no threads were created, clean up and fail. */
   if (tp->thread_cnt == 0)
   {
      slock_free(tp->work_mutex);
      scond_free(tp->work_cond);
      scond_free(tp->exit_cond);
      retro_eventcount_free(&tp->idle);
      free(tp);
      return NULL;
   }

   return tp;
}

tpool_t *tpool_create(size_t num)
{
   return tpool_create_with_stack_size(num, 0);
}

void tpool_destroy(tpool_t *tp)
{
   tpool_work_t *work;
   tpool_work_t *work2;

   if (!tp)
      return;

   /* Take all work out of the queue and destroy it. */
   slock_lock(tp->work_mutex);
   work = tp->work_first;
   while (work)
   {
      work2 = work->next;
      tpool_work_destroy(work);
      work = work2;
      retro_atomic_fetch_sub_int(&tp->queued, 1);
      retro_atomic_fetch_sub_int(&tp->outstanding, 1);
   }
   tp->work_first = NULL;
   tp->work_last  = NULL;

   /* Tell the worker threads to stop. */
   retro_atomic_store_release_int(&tp->stop, 1);
   scond_broadcast(tp->work_cond);
   slock_unlock(tp->work_mutex);
   /* The queue just emptied without passing through tpool_retire(). */
   retro_eventcount_notify(&tp->idle);

   /* Wait for all threads to stop. */
   tpool_wait(tp);

   slock_free(tp->work_mutex);
   scond_free(tp->work_cond);
   scond_free(tp->exit_cond);
   retro_eventcount_free(&tp->idle);

   free(tp);
}

bool tpool_add_work(tpool_t *tp, thread_func_t func, void *arg)
{
   tpool_work_t *work;

   if (!tp)
      return false;

   work = tpool_work_create(func, arg);
   if (!work)
      return false;

   slock_lock(tp->work_mutex);
   if (!tp->work_first)
   {
      tp->work_first      = work;
      tp->work_last       = tp->work_first;
   }
   else
   {
      tp->work_last->next = work;
      tp->work_last       = work;
   }

   retro_atomic_fetch_add_int(&tp->queued, 1);
   retro_atomic_fetch_add_int(&tp->outstanding, 1);
   scond_signal(tp->work_cond);
   slock_unlock(tp->work_mutex);

   return true;
}

bool tpool_help(tpool_t *tp)
{
   tpool_work_t *work;
   if (!tp)
      return false;
   /* An empty queue has nothing to hand over, and the counter says so
    * without the mutex: a caller looping here while it waits on work
    * in other hands leaves the queue's lock to the threads using it. */
   if (!retro_atomic_load_acquire_int(&tp->queued))
      return false;
   slock_lock(tp->work_mutex);
   work = retro_atomic_load_acquire_int(&tp->stop)
        ? NULL : tpool_work_get(tp);
   if (work)
      retro_atomic_fetch_sub_int(&tp->queued, 1);
   slock_unlock(tp->work_mutex);
   if (!work)
      return false;
   work->func(work->arg);
   tpool_work_destroy(work);
   tpool_retire(tp);
   return true;
}

void tpool_wait(tpool_t *tp)
{
   if (!tp)
      return;

   /* Teardown: wait for every thread to leave. Stays under the lock -
    * see the end of tpool_worker(). */
   if (retro_atomic_load_acquire_int(&tp->stop))
   {
      slock_lock(tp->work_mutex);
      while (tp->thread_cnt != 0)
         scond_wait(tp->exit_cond, tp->work_mutex);
      slock_unlock(tp->work_mutex);
      return;
   }

   /* Nothing queued and nothing in progress. outstanding counts both,
    * from the moment tpool_add_work() returns until the item's
    * function has. */
   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&tp->outstanding))
         return;
      key = retro_eventcount_prepare_wait(&tp->idle);
      if (!retro_atomic_load_acquire_int(&tp->outstanding))
      {
         retro_eventcount_cancel_wait(&tp->idle);
         return;
      }
      retro_eventcount_commit_wait(&tp->idle, key);
   }
}
