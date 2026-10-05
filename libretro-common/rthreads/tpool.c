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

/* A job waiting on the locked list: the whole queue on a backend
 * without real atomics, the overflow of the ring on one with them. */
struct tpool_work
{
   thread_func_t      func;  /* Function to be called. */
   void              *arg;   /* Data to be passed to func. */
   struct tpool_work *next;  /* Next work item in the queue. */
};
typedef struct tpool_work tpool_work_t;

/* On a backend with real atomics the pool takes no lock to hand a job
 * over: the queue is a ring of slots that producers and workers claim
 * with a compare-and-swap, and a worker with nothing to do parks on
 * an eventcount of its own, so posting a job wakes one worker and no
 * more. The mutex is left for teardown, where the last thread out and
 * the thread freeing the pool have to agree, and for the jobs posted
 * while the ring is full. Other backends keep the locked queue
 * further down. */
#ifdef RETRO_ATOMIC_LOCK_FREE

/* Jobs the ring holds, a power of two. Past that many waiting, jobs
 * go on a list under the mutex until the ring has been emptied. */
#define TPOOL_QUEUE_SIZE 256

/* Overflow nodes kept for reuse once taken; past this many, freed. */
#define TPOOL_OVER_SPARE (TPOOL_QUEUE_SIZE * 4)

/* enq (producers), deq (workers) and the two counts (both) each on
 * lines of their own; two 64-byte lines, for CPUs that fetch pairs. */
#define TPOOL_LINE 128

typedef union tpool_line_int
{
   retro_atomic_int_t v;
   char               pad[TPOOL_LINE];
} tpool_line_int_t;

/* Test builds yield at the points where the order of two threads'
 * steps matters, to visit the orders a quiet machine never does. */
#ifdef TPOOL_FUZZ_SCHEDULE
static retro_atomic_int_t tpool_fuzz_seed;
static void tpool_fuzz(void)
{
   unsigned r = (unsigned)retro_atomic_fetch_add_int(&tpool_fuzz_seed,
         (int)0x9E3779B1u) * 1103515245u + 12345u;
   if (!((r >> 16) & 3))
      sthread_yield();
}
#define TPOOL_FUZZ() tpool_fuzz()
#else
#define TPOOL_FUZZ() ((void)0)
#endif

/* A slot's seq says whose turn it is: equal to a position, that
 * position may be filled; one more, it may be emptied. */
typedef struct tpool_slot
{
   retro_atomic_int_t seq;
   thread_func_t      func;
   void              *arg;
} tpool_slot_t;

typedef struct tpool_thread
{
   struct tpool      *tp;
   sthread_t         *thread;  /* Joined by tpool_destroy(). */
   retro_eventcount_t wake;    /* Where this thread parks. */
   retro_atomic_int_t parked;  /* 1 from saying so until woken or back. */
   bool               wake_ok;
} tpool_thread_t;

struct tpool
{
   tpool_line_int_t   enq;        /* Next position to fill. */
   tpool_line_int_t   deq;        /* Next position to empty. */
   /* queued: jobs posted and not yet taken. outstanding: queued plus
    * in progress. */
   union
   {
      struct
      {
         retro_atomic_int_t queued;
         retro_atomic_int_t outstanding;
      } n;
      char pad[TPOOL_LINE];
   } count;
   slock_t           *work_mutex; /* The overflow. */
   tpool_work_t      *over_first; /* Jobs posted with the ring full, */
   tpool_work_t      *over_last;  /* under work_mutex. */
   tpool_work_t      *over_free;  /* Taken nodes kept for reuse, */
   size_t             over_spare; /* how many, under work_mutex. */
   retro_atomic_int_t over;       /* How many: read without the mutex. */
   size_t             thread_num; /* Entries in threads. */
   tpool_thread_t    *threads;
   /* tpool_wait() sleeps here until outstanding reaches zero. */
   retro_eventcount_t idle;
   retro_atomic_int_t stop;       /* Tells the threads to exit. */
   tpool_slot_t       slots[TPOOL_QUEUE_SIZE];
};

static bool tpool_queue_put(tpool_t *tp, thread_func_t func, void *arg)
{
   tpool_slot_t *slot;
   unsigned pos = (unsigned)retro_atomic_load_relaxed_int(&tp->enq.v);

   for (;;)
   {
      int dif;
      slot = &tp->slots[pos & (TPOOL_QUEUE_SIZE - 1)];
      dif  = (int)((unsigned)retro_atomic_load_acquire_int(&slot->seq) - pos);
      if (dif < 0)
         return false;
      if (     dif == 0
            && retro_atomic_cas_int(&tp->enq.v, (int)pos, (int)(pos + 1)))
         break;
      pos  = (unsigned)retro_atomic_load_relaxed_int(&tp->enq.v);
   }
   TPOOL_FUZZ();
   slot->func = func;
   slot->arg  = arg;
   retro_atomic_store_release_int(&slot->seq, (int)(pos + 1));
   return true;
}

static bool tpool_queue_take(tpool_t *tp, thread_func_t *func, void **arg)
{
   tpool_slot_t *slot;
   unsigned pos = (unsigned)retro_atomic_load_relaxed_int(&tp->deq.v);

   for (;;)
   {
      int dif;
      slot = &tp->slots[pos & (TPOOL_QUEUE_SIZE - 1)];
      dif  = (int)((unsigned)retro_atomic_load_acquire_int(&slot->seq)
            - (pos + 1));
      if (dif < 0)
         return false;
      if (     dif == 0
            && retro_atomic_cas_int(&tp->deq.v, (int)pos, (int)(pos + 1)))
         break;
      pos  = (unsigned)retro_atomic_load_relaxed_int(&tp->deq.v);
   }
   TPOOL_FUZZ();
   *func = slot->func;
   *arg  = slot->arg;
   retro_atomic_store_release_int(&slot->seq, (int)(pos + TPOOL_QUEUE_SIZE));
   return true;
}

/* The ring, unless jobs are already waiting behind it: those went in
 * first and the ring is left to empty ahead of them. */
static bool tpool_post(tpool_t *tp, thread_func_t func, void *arg)
{
   tpool_work_t *work;

   if (     !retro_atomic_load_acquire_int(&tp->over)
         && tpool_queue_put(tp, func, arg))
      return true;

   slock_lock(tp->work_mutex);
   if ((work = tp->over_free))
   {
      tp->over_free = work->next;
      tp->over_spare--;
   }
   else
   {
      slock_unlock(tp->work_mutex);
      if (!(work = (tpool_work_t*)malloc(sizeof(*work))))
         return false;
      slock_lock(tp->work_mutex);
   }
   work->func = func;
   work->arg  = arg;
   work->next = NULL;
   if (tp->over_last)
      tp->over_last->next = work;
   else
      tp->over_first      = work;
   tp->over_last          = work;
   retro_atomic_fetch_add_int(&tp->over, 1);
   slock_unlock(tp->work_mutex);
   return true;
}

static bool tpool_take(tpool_t *tp, thread_func_t *func, void **arg)
{
   tpool_work_t *work;

   if (tpool_queue_take(tp, func, arg))
      return true;
   if (!retro_atomic_load_acquire_int(&tp->over))
      return false;

   slock_lock(tp->work_mutex);
   if ((work = tp->over_first))
   {
      if (!(tp->over_first = work->next))
         tp->over_last = NULL;
      retro_atomic_fetch_sub_int(&tp->over, 1);
      *func = work->func;
      *arg  = work->arg;
      if (tp->over_spare < TPOOL_OVER_SPARE)
      {
         work->next    = tp->over_free;
         tp->over_free = work;
         tp->over_spare++;
         work          = NULL;
      }
      slock_unlock(tp->work_mutex);
      free(work);
      return true;
   }
   slock_unlock(tp->work_mutex);
   return false;
}

/* One item done: the thread that retires the last outstanding one
 * wakes tpool_wait(). */
static void tpool_retire(tpool_t *tp)
{
   if (retro_atomic_fetch_sub_int(&tp->count.n.outstanding, 1) == 1)
      retro_eventcount_notify(&tp->idle);
}

/* After a job is posted and counted: wake one thread that said it was
 * parking. The poster bumps queued and then reads parked; the thread
 * bumps parked and then reads queued; each pair is sequentially
 * consistent, so they cannot both miss the other. */
static void tpool_wake_one(tpool_t *tp)
{
   size_t i;
   for (i = 0; i < tp->thread_num; i++)
   {
      tpool_thread_t *t = &tp->threads[i];
      if (     retro_atomic_load_seq_cst_int(&t->parked)
            && retro_atomic_cas_int(&t->parked, 1, 0))
      {
         retro_eventcount_notify(&t->wake);
         return;
      }
   }
}

static void tpool_worker(void *arg)
{
   tpool_thread_t *self = (tpool_thread_t*)arg;
   tpool_t        *tp   = self->tp;

   for (;;)
   {
      int           key;
      thread_func_t func;
      void         *work_arg;

      if (tpool_take(tp, &func, &work_arg))
      {
         retro_atomic_fetch_sub_int(&tp->count.n.queued, 1);
         func(work_arg);
         tpool_retire(tp);
         continue;
      }
      if (retro_atomic_load_acquire_int(&tp->stop))
         break;

      /* Nothing to take: say so, then look once more before sleeping.
       * A poster may already have picked this thread to wake and
       * cleared the mark - before the wait was open, so its notify is
       * gone, and the job it posted may be in other hands by now. The
       * cleared mark is then all that is left of the wake-up, and
       * sleeping on it would be sleeping where no poster looks. */
      retro_atomic_fetch_add_seq_cst_int(&self->parked, 1);
      TPOOL_FUZZ();
      key = retro_eventcount_prepare_wait(&self->wake);
      if (     retro_atomic_load_seq_cst_int(&tp->count.n.queued) > 0
            || !retro_atomic_load_seq_cst_int(&self->parked)
            || retro_atomic_load_acquire_int(&tp->stop))
      {
         /* A job counted but not yet takeable is a step from being
          * so: its poster or its taker is mid-way. */
         retro_eventcount_cancel_wait(&self->wake);
         (void)retro_atomic_cas_int(&self->parked, 1, 0);
         sthread_yield();
         continue;
      }
      TPOOL_FUZZ();
      retro_eventcount_commit_wait(&self->wake, key);
      (void)retro_atomic_cas_int(&self->parked, 1, 0);
   }

   /* Nothing to say on the way out: tpool_destroy() joins this thread,
    * so tp outlives it without it having to count itself out. */
}

static void tpool_free(tpool_t *tp)
{
   size_t i;
   for (i = 0; i < tp->thread_num; i++)
      if (tp->threads[i].wake_ok)
         retro_eventcount_free(&tp->threads[i].wake);
   if (tp->work_mutex)
      slock_free(tp->work_mutex);
   while (tp->over_free)
   {
      tpool_work_t *next = tp->over_free->next;
      free(tp->over_free);
      tp->over_free = next;
   }
   retro_eventcount_free(&tp->idle);
   free(tp->threads);
   free(tp);
}

tpool_t *tpool_create_with_stack_size(size_t num, size_t stack_size)
{
   tpool_t *tp;
   size_t   i;
   size_t   made = 0;

   if (num == 0)
      num = 2;

   if (!(tp = (tpool_t*)calloc(1, sizeof(*tp))))
      return NULL;

   tp->threads    = (tpool_thread_t*)calloc(num, sizeof(*tp->threads));
   tp->work_mutex = slock_new();

   if (     !tp->threads || !tp->work_mutex
         || !retro_eventcount_init(&tp->idle))
   {
      tpool_free(tp);
      return NULL;
   }
   tp->thread_num = num;

   for (i = 0; i < TPOOL_QUEUE_SIZE; i++)
      retro_atomic_int_init(&tp->slots[i].seq, (int)i);

   /* Create the requested number of threads. They are kept, not
    * detached: tpool_destroy() joins each one, so none of them is
    * still running when it returns. */
   for (i = 0; i < num; i++)
   {
      tpool_thread_t *t = &tp->threads[i];

      t->tp = tp;
      if (!(t->wake_ok = retro_eventcount_init(&t->wake)))
         continue;
      t->thread = stack_size
            ? sthread_create_with_stack_size(tpool_worker, t, stack_size)
            : sthread_create(tpool_worker, t);
      if (t->thread)
         made++;
   }

   /* If no threads were created, clean up and fail. */
   if (made == 0)
   {
      tpool_free(tp);
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
   size_t        i;
   thread_func_t func;
   void         *arg;

   if (!tp)
      return;

   /* Take all work out of the queue and drop it. */
   while (tpool_take(tp, &func, &arg))
   {
      retro_atomic_fetch_sub_int(&tp->count.n.queued, 1);
      retro_atomic_fetch_sub_int(&tp->count.n.outstanding, 1);
   }

   /* Tell the worker threads to stop. */
   retro_atomic_store_release_int(&tp->stop, 1);
   for (i = 0; i < tp->thread_num; i++)
      if (tp->threads[i].wake_ok)
         retro_eventcount_notify(&tp->threads[i].wake);
   /* The queue just emptied without passing through tpool_retire(). */
   retro_eventcount_notify(&tp->idle);

   /* Wait for every thread to end - the thread itself, not a count it
    * lowers on its way out, which left it running past this point and
    * into whatever the caller did next, the end of the process
    * included. */
   for (i = 0; i < tp->thread_num; i++)
      if (tp->threads[i].thread)
         sthread_join(tp->threads[i].thread);

   tpool_free(tp);
}

bool tpool_add_work(tpool_t *tp, thread_func_t func, void *arg)
{
   if (!tp || !func)
      return false;

   /* Counted before it can be taken, so it is never retired first. */
   retro_atomic_fetch_add_int(&tp->count.n.outstanding, 1);
   if (!tpool_post(tp, func, arg))
   {
      tpool_retire(tp);
      return false;
   }
   TPOOL_FUZZ();
   retro_atomic_fetch_add_seq_cst_int(&tp->count.n.queued, 1);
   TPOOL_FUZZ();
   tpool_wake_one(tp);
   return true;
}

bool tpool_help(tpool_t *tp)
{
   thread_func_t func;
   void         *arg;

   /* An empty queue has nothing to hand over. */
   if (     !tp
         || !retro_atomic_load_acquire_int(&tp->count.n.queued)
         || retro_atomic_load_acquire_int(&tp->stop)
         || !tpool_take(tp, &func, &arg))
      return false;
   retro_atomic_fetch_sub_int(&tp->count.n.queued, 1);
   func(arg);
   tpool_retire(tp);
   return true;
}

void tpool_wait(tpool_t *tp)
{
   if (!tp)
      return;

   /* Teardown is tpool_destroy()'s to wait out: it joins the threads. */
   if (retro_atomic_load_acquire_int(&tp->stop))
      return;

   /* Nothing queued and nothing in progress. outstanding counts both,
    * from the moment tpool_add_work() returns until the item's
    * function has. */
   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&tp->count.n.outstanding))
         return;
      key = retro_eventcount_prepare_wait(&tp->idle);
      if (!retro_atomic_load_acquire_int(&tp->count.n.outstanding))
      {
         retro_eventcount_cancel_wait(&tp->idle);
         return;
      }
      retro_eventcount_commit_wait(&tp->idle, key);
   }
}

#else /* !RETRO_ATOMIC_LOCK_FREE */

struct tpool
{
   tpool_work_t    *work_first;   /* First work item in the work queue. */
   tpool_work_t    *work_last;    /* Last work item in the work queue. */
   slock_t         *work_mutex;   /* Mutex protecting inserting and removing work from the work queue. */
   scond_t         *work_cond;    /* Conditional to signal when there is work to process. */
   sthread_t      **threads;      /* The pool's threads, joined by tpool_destroy(). */
   size_t           thread_num;   /* Entries in threads; unmade ones are NULL. */
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

   /* Nothing to say on the way out: tpool_destroy() joins this thread,
    * so tp outlives it without it having to count itself out. */
   slock_unlock(tp->work_mutex);
}

tpool_t *tpool_create_with_stack_size(size_t num, size_t stack_size)
{
   tpool_t   *tp;
   size_t     i;
   size_t     made = 0;

   if (num == 0)
      num = 2;

   tp               = (tpool_t*)calloc(1, sizeof(*tp));
   if (!tp)
      return NULL;

   tp->threads      = (sthread_t**)calloc(num, sizeof(*tp->threads));
   tp->work_mutex   = slock_new();
   tp->work_cond    = scond_new();

   if (     !tp->threads || !tp->work_mutex || !tp->work_cond
         || !retro_eventcount_init(&tp->idle))
   {
      if (tp->work_mutex)
         slock_free(tp->work_mutex);
      if (tp->work_cond)
         scond_free(tp->work_cond);
      free(tp->threads);
      free(tp);
      return NULL;
   }

   tp->thread_num   = num;
   tp->work_first   = NULL;
   tp->work_last    = NULL;

   /* Create the requested number of threads. They are kept, not
    * detached: tpool_destroy() joins each one, so none of them is
    * still running when it returns. */
   for (i = 0; i < num; i++)
   {
      tp->threads[i] = stack_size
            ? sthread_create_with_stack_size(tpool_worker, tp, stack_size)
            : sthread_create(tpool_worker, tp);
      if (tp->threads[i])
         made++;
   }

   /* If no threads were created, clean up and fail. */
   if (made == 0)
   {
      slock_free(tp->work_mutex);
      scond_free(tp->work_cond);
      retro_eventcount_free(&tp->idle);
      free(tp->threads);
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
   size_t        i;
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

   /* Wait for every thread to end - the thread itself, not a count it
    * lowers on its way out, which left it running past this point and
    * into whatever the caller did next. */
   for (i = 0; i < tp->thread_num; i++)
      if (tp->threads[i])
         sthread_join(tp->threads[i]);

   slock_free(tp->work_mutex);
   scond_free(tp->work_cond);
   retro_eventcount_free(&tp->idle);

   free(tp->threads);
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

   /* Teardown is tpool_destroy()'s to wait out: it joins the threads. */
   if (retro_atomic_load_acquire_int(&tp->stop))
      return;

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

#endif /* RETRO_ATOMIC_LOCK_FREE */
