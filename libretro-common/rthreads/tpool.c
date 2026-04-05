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

#include <rthreads/rthreads.h>
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
   scond_t         *working_cond; /* Conditional to signal when there is no work processing.
                                       This will also signal when there are no threads running. */
   size_t           working_cnt;  /* The number of threads processing work (Not waiting for work). */
   size_t           thread_cnt;   /* Total number of threads within the pool. */
   bool             stop;         /* Marker to tell the work threads to exit. */
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
      while (!tp->work_first && !tp->stop)
         scond_wait(tp->work_cond, tp->work_mutex);

      /* Re-check stop after waking from the conditional. */
      if (tp->stop)
         break;

      /* Try to pull work from the queue. */
      work = tpool_work_get(tp);
      if (work)
         tp->working_cnt++;
      slock_unlock(tp->work_mutex);

      /* Call the work function and let it process. */
      if (work)
      {
         work->func(work->arg);
         tpool_work_destroy(work);

         slock_lock(tp->work_mutex);
         tp->working_cnt--;
         /* Since we're in a lock no work can be added or removed from the queue.
          * Also, the working_cnt can't be changed (except the thread holding the lock).
          * At this point if there isn't any work processing and if there is no work
          * signal this is the case. */
         if (!tp->stop && tp->working_cnt == 0 && !tp->work_first)
            scond_signal(tp->working_cond);
         slock_unlock(tp->work_mutex);
      }
   }

   tp->thread_cnt--;
   if (tp->thread_cnt == 0)
      scond_signal(tp->working_cond);
   slock_unlock(tp->work_mutex);
}

tpool_t *tpool_create(size_t num)
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
   tp->working_cond = scond_new();

   if (!tp->work_mutex || !tp->work_cond || !tp->working_cond)
   {
      if (tp->work_mutex)
         slock_free(tp->work_mutex);
      if (tp->work_cond)
         scond_free(tp->work_cond);
      if (tp->working_cond)
         scond_free(tp->working_cond);
      free(tp);
      return NULL;
   }

   tp->work_first   = NULL;
   tp->work_last    = NULL;

   /* Create the requested number of threads and detach them. */
   tp->thread_cnt   = 0;
   for (i = 0; i < num; i++)
   {
      thread = sthread_create(tpool_worker, tp);
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
      scond_free(tp->working_cond);
      free(tp);
      return NULL;
   }

   return tp;
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
   }
   tp->work_first = NULL;
   tp->work_last  = NULL;

   /* Tell the worker threads to stop. */
   tp->stop = true;
   scond_broadcast(tp->work_cond);
   slock_unlock(tp->work_mutex);

   /* Wait for all threads to stop. */
   tpool_wait(tp);

   slock_free(tp->work_mutex);
   scond_free(tp->work_cond);
   scond_free(tp->working_cond);

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

   scond_signal(tp->work_cond);
   slock_unlock(tp->work_mutex);

   return true;
}

void tpool_wait(tpool_t *tp)
{
   if (!tp)
      return;

   slock_lock(tp->work_mutex);

   for (;;)
   {
      /* working_cond is dual use. It signals when we're not stopping but the
       * working_cnt is 0 indicating there isn't any work processing. If we
       * are stopping it will trigger when there aren't any threads running. */
      if ((!tp->stop && tp->working_cnt != 0) || (tp->stop && tp->thread_cnt != 0))
         scond_wait(tp->working_cond, tp->work_mutex);
      else
         break;
   }

   slock_unlock(tp->work_mutex);
}
