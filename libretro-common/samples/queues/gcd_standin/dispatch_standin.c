/* The stand-in for libdispatch: see dispatch/dispatch.h.  A pool of
 * threads takes jobs off one list.  A serial queue has one job on the
 * list at a time, which runs its next call and puts itself back while
 * calls are left; a concurrent queue puts each call on the list.  One
 * timer thread fires the sources.  On pthreads rather than rthreads,
 * so that a sample wrapping rthreads calls to hold the queue at a
 * point of its own never holds the stand-in instead. */
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>

#include "dispatch/dispatch.h"

#define STANDIN_THREADS 6

enum standin_kind
{
   STANDIN_GLOBAL = 1,
   STANDIN_SERIAL,
   STANDIN_CONCURRENT,
   STANDIN_TIMER
};

struct standin_call
{
   struct standin_call *next;
   dispatch_function_t  work;
   void                *context;
   /* the queue whose call this is, held, or NULL on the global one */
   struct dispatch_standin_queue *queue;
};

struct dispatch_standin_queue
{
   enum standin_kind    kind;
   int                  refs;
   int                  suspended;
   bool                 draining; /* a serial queue's job is out */
   struct standin_call *head;
   struct standin_call *tail;
};

struct dispatch_standin_source
{
   enum standin_kind              kind;
   int                            refs;
   int                            suspended;
   bool                           cancelled;
   bool                           event_out;
   dispatch_time_t                deadline;
   void                          *context;
   dispatch_function_t            event;
   dispatch_function_t            cancel;
   struct dispatch_standin_queue *target;
   struct dispatch_standin_source *next;
};

static pthread_mutex_t standin_mutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  standin_cond       = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  standin_timer_cond = PTHREAD_COND_INITIALIZER;
static bool            standin_started    = false;
#define slock_lock(l)   pthread_mutex_lock(&standin_mutex)
#define slock_unlock(l) pthread_mutex_unlock(&standin_mutex)
static struct standin_call *jobs_head;
static struct standin_call *jobs_tail;
static struct dispatch_standin_source *sources;
static struct dispatch_standin_queue   standin_global = {
   STANDIN_GLOBAL, 1, 0, false, NULL, NULL };
static retro_atomic_int_t standin_ran = RETRO_ATOMIC_INT_INITIALIZER(0);

static void standin_serial_job(void *data);

static uint64_t standin_now(void)
{
   return (uint64_t)cpu_features_get_time_usec() * NSEC_PER_USEC;
}

/* Under the lock. */
static void standin_job_put(struct standin_call *c)
{
   c->next = NULL;
   if (jobs_tail)
      jobs_tail->next = c;
   else
      jobs_head = c;
   jobs_tail = c;
   pthread_cond_signal(&standin_cond);
}

static struct standin_call *standin_call_new(dispatch_function_t work,
      void *context, struct dispatch_standin_queue *q)
{
   struct standin_call *c = (struct standin_call*)calloc(1, sizeof(*c));
   if (!c)
      abort();
   c->work    = work;
   c->context = context;
   c->queue   = q;
   return c;
}

/* Under the lock: a serial queue with calls, not suspended and with no
 * job out, sends its job. */
static void standin_serial_kick(struct dispatch_standin_queue *q)
{
   if (q->head && !q->suspended && !q->draining)
   {
      q->draining = true;
      q->refs++;
      standin_job_put(standin_call_new(standin_serial_job, q, q));
   }
}

/* Under the lock. */
static void standin_queue_unref(struct dispatch_standin_queue *q)
{
   if (q && q->kind != STANDIN_GLOBAL && --q->refs == 0)
      free(q);
}

static void standin_serial_job(void *data)
{
   struct dispatch_standin_queue *q = (struct dispatch_standin_queue*)data;
   struct standin_call           *c;

   slock_lock(standin_lock);
   c = q->head;
   if (c && !q->suspended)
   {
      if (!(q->head = c->next))
         q->tail = NULL;
   }
   else
      c = NULL;
   slock_unlock(standin_lock);

   if (c)
   {
      c->work(c->context);
      retro_atomic_inc_int(&standin_ran);
      free(c);
   }

   slock_lock(standin_lock);
   q->draining = false;
   standin_serial_kick(q);
   slock_unlock(standin_lock);
}

static void *standin_worker(void *unused)
{
   (void)unused;
   for (;;)
   {
      struct standin_call *c;
      slock_lock(standin_lock);
      while (!jobs_head)
         pthread_cond_wait(&standin_cond, &standin_mutex);
      c = jobs_head;
      if (!(jobs_head = c->next))
         jobs_tail = NULL;
      slock_unlock(standin_lock);

      c->work(c->context);
      if (c->work != standin_serial_job)
         retro_atomic_inc_int(&standin_ran);

      slock_lock(standin_lock);
      standin_queue_unref(c->queue);
      slock_unlock(standin_lock);
      free(c);
   }   return NULL;
}

static void standin_source_event(void *data)
{
   struct dispatch_standin_source *s = (struct dispatch_standin_source*)data;
   bool run;
   slock_lock(standin_lock);
   s->event_out = false;
   run          = !s->cancelled && s->event;
   slock_unlock(standin_lock);
   if (run)
      s->event(s->context);
}

static void standin_source_unref(struct dispatch_standin_source *s)
{
   struct dispatch_standin_source **link;
   if (--s->refs)
      return;
   for (link = &sources; *link; link = &(*link)->next)
      if (*link == s)
      {
         *link = s->next;
         break;
      }
   standin_queue_unref(s->target);
   free(s);
}

static void standin_source_cancelled(void *data)
{
   struct dispatch_standin_source *s = (struct dispatch_standin_source*)data;
   if (s->cancel)
      s->cancel(s->context);
   slock_lock(standin_lock);
   standin_source_unref(s);
   slock_unlock(standin_lock);
}

/* Under the lock: a call on @q, as dispatch_async_f(). */
static void standin_async_locked(struct dispatch_standin_queue *q,
      void *context, dispatch_function_t work)
{
   struct standin_call *c = standin_call_new(work, context, NULL);
   if (q->kind == STANDIN_SERIAL)
   {
      if (q->tail)
         q->tail->next = c;
      else
         q->head = c;
      q->tail = c;
      standin_serial_kick(q);
      return;
   }
   if (q->kind == STANDIN_CONCURRENT)
   {
      q->refs++;
      c->queue = q;
      if (q->suspended)
      {
         if (q->tail)
            q->tail->next = c;
         else
            q->head = c;
         q->tail = c;
         return;
      }
   }
   standin_job_put(c);
}

static void *standin_timer_thread(void *unused)
{
   (void)unused;
   slock_lock(standin_lock);
   for (;;)
   {
      struct dispatch_standin_source *s;
      uint64_t now  = standin_now();
      uint64_t next = DISPATCH_TIME_FOREVER;

      for (s = sources; s; s = s->next)
      {
         if (s->suspended || s->cancelled
               || s->deadline == DISPATCH_TIME_FOREVER)
            continue;
         if (s->deadline <= now)
         {
            /* One shot: the runner sets no interval. */
            s->deadline = DISPATCH_TIME_FOREVER;
            if (!s->event_out)
            {
               s->event_out = true;
               standin_async_locked(s->target, s,
                     standin_source_event);
            }
         }
         else if (s->deadline < next)
            next = s->deadline;
      }
      if (next == DISPATCH_TIME_FOREVER)
         pthread_cond_wait(&standin_timer_cond, &standin_mutex);
      else
      {
         struct timespec ts;
         uint64_t        wait = next - now;
         clock_gettime(CLOCK_REALTIME, &ts);
         ts.tv_sec  += (time_t)(wait / (uint64_t)1000000000);
         ts.tv_nsec += (long)(wait % (uint64_t)1000000000);
         if (ts.tv_nsec >= 1000000000L)
         {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
         }
         pthread_cond_timedwait(&standin_timer_cond, &standin_mutex, &ts);
      }
   }   return NULL;
}

/* The first call comes from the thread that inits the queue, before
 * any other can. */
static void standin_start(void)
{
   pthread_t thread;
   unsigned  i;
   if (standin_started)
      return;
   standin_started = true;
   for (i = 0; i <= STANDIN_THREADS; i++)
   {
      if (pthread_create(&thread, NULL,
               i < STANDIN_THREADS ? standin_worker : standin_timer_thread,
               NULL))
         abort();
      pthread_detach(thread);
   }
}

dispatch_queue_t dispatch_get_global_queue(long identifier,
      unsigned long flags)
{
   (void)identifier;
   (void)flags;
   standin_start();
   return &standin_global;
}

dispatch_queue_t dispatch_queue_create(const char *label,
      dispatch_queue_attr_t attr)
{
   struct dispatch_standin_queue *q = (struct dispatch_standin_queue*)
      calloc(1, sizeof(*q));
   (void)label;
   standin_start();
   if (!q)
      return NULL;
   q->kind = (attr == DISPATCH_QUEUE_CONCURRENT)
      ? STANDIN_CONCURRENT : STANDIN_SERIAL;
   q->refs = 1;
   return q;
}

void dispatch_async_f(dispatch_queue_t queue, void *context,
      dispatch_function_t work)
{
   slock_lock(standin_lock);
   standin_async_locked(queue, context, work);
   slock_unlock(standin_lock);
}

dispatch_source_t dispatch_source_create(dispatch_source_type_t type,
      uintptr_t handle, unsigned long mask, dispatch_queue_t queue)
{
   struct dispatch_standin_source *s;
   (void)handle;
   (void)mask;
   if (type != DISPATCH_SOURCE_TYPE_TIMER
         || !(s = (struct dispatch_standin_source*)calloc(1, sizeof(*s))))
      return NULL;
   s->kind      = STANDIN_TIMER;
   s->refs      = 1;
   s->suspended = 1;
   s->deadline  = DISPATCH_TIME_FOREVER;
   s->target    = queue;
   slock_lock(standin_lock);
   if (queue->kind != STANDIN_GLOBAL)
      queue->refs++;
   s->next = sources;
   sources = s;
   slock_unlock(standin_lock);
   return s;
}

void dispatch_source_set_event_handler_f(dispatch_source_t source,
      dispatch_function_t handler)
{
   slock_lock(standin_lock);
   source->event = handler;
   slock_unlock(standin_lock);
}

void dispatch_source_set_cancel_handler_f(dispatch_source_t source,
      dispatch_function_t handler)
{
   slock_lock(standin_lock);
   source->cancel = handler;
   slock_unlock(standin_lock);
}

void dispatch_source_set_timer(dispatch_source_t source,
      dispatch_time_t start, uint64_t interval, uint64_t leeway)
{
   (void)interval;
   (void)leeway;
   slock_lock(standin_lock);
   source->deadline = (start == DISPATCH_TIME_NOW) ? standin_now() : start;
   pthread_cond_signal(&standin_timer_cond);
   slock_unlock(standin_lock);
}

void dispatch_source_cancel(dispatch_source_t source)
{
   slock_lock(standin_lock);
   if (!source->cancelled)
   {
      source->cancelled = true;
      source->refs++;
      /* Behind any event already on its way, on the same queue; a
       * suspended source gets no handler until it is resumed, which
       * the runner never leaves it at. */
      standin_async_locked(source->target, source,
            standin_source_cancelled);
   }
   slock_unlock(standin_lock);
}

dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta)
{
   if (when == DISPATCH_TIME_FOREVER)
      return when;
   if (when == DISPATCH_TIME_NOW)
      when = standin_now();
   return when + (uint64_t)delta;
}

void dispatch_standin_set_target_queue(void *object, dispatch_queue_t q)
{
   /* Every queue here runs on the one pool. */
   (void)object;
   (void)q;
}

void dispatch_standin_set_context(void *object, void *context)
{
   struct dispatch_standin_source *s = (struct dispatch_standin_source*)
      object;
   if (s->kind == STANDIN_TIMER)
      s->context = context;
}

void dispatch_standin_suspend(void *object)
{
   enum standin_kind kind = *(enum standin_kind*)object;
   slock_lock(standin_lock);
   if (kind == STANDIN_TIMER)
      ((struct dispatch_standin_source*)object)->suspended++;
   else
      ((struct dispatch_standin_queue*)object)->suspended++;
   slock_unlock(standin_lock);
}

void dispatch_standin_resume(void *object)
{
   enum standin_kind kind = *(enum standin_kind*)object;
   slock_lock(standin_lock);
   if (kind == STANDIN_TIMER)
   {
      ((struct dispatch_standin_source*)object)->suspended--;
      pthread_cond_signal(&standin_timer_cond);
   }
   else
   {
      struct dispatch_standin_queue *q =
         (struct dispatch_standin_queue*)object;
      if (!--q->suspended)
      {
         if (q->kind == STANDIN_SERIAL)
            standin_serial_kick(q);
         else
         {
            struct standin_call *c = q->head;
            q->head = q->tail = NULL;
            while (c)
            {
               struct standin_call *next = c->next;
               standin_job_put(c);
               c = next;
            }
         }
      }
   }
   slock_unlock(standin_lock);
}

void dispatch_standin_release(void *object)
{
   enum standin_kind kind = *(enum standin_kind*)object;
   slock_lock(standin_lock);
   /* GCD crashes on the last release of an object still suspended,
    * a source never resumed among them. */
   if (kind == STANDIN_TIMER)
   {
      struct dispatch_standin_source *src =
         (struct dispatch_standin_source*)object;
      if (src->suspended)
         abort();
   }
   else if (((struct dispatch_standin_queue*)object)->suspended)
      abort();
   if (kind == STANDIN_TIMER)
      standin_source_unref((struct dispatch_standin_source*)object);
   else
      standin_queue_unref((struct dispatch_standin_queue*)object);
   slock_unlock(standin_lock);
}

unsigned long dispatch_standin_calls(void)
{
   return (unsigned long)retro_atomic_load_acquire_int(&standin_ran);
}
