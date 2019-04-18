/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*#define VCOS_INLINE_BODIES */
#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_msgqueue.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <linux/param.h>

/* Cygwin doesn't always have prctl.h and it doesn't have PR_SET_NAME */
#if defined( __linux__ )
# if !defined(HAVE_PRCTL)
#  define HAVE_PRCTL
# endif
#include <sys/prctl.h>
#endif

#ifdef HAVE_CMAKE_CONFIG
#include "cmake_config.h"
#endif

#ifdef HAVE_MTRACE
#include <mcheck.h>
#endif

#if defined(ANDROID)
#include <android/log.h>
#endif

#ifndef VCOS_DEFAULT_STACK_SIZE
#define VCOS_DEFAULT_STACK_SIZE 4096
#endif

static int vcos_argc;
static const char **vcos_argv;

typedef void (*LEGACY_ENTRY_FN_T)(int, void *);

static VCOS_THREAD_ATTR_T default_attrs = {
   .ta_stacksz = VCOS_DEFAULT_STACK_SIZE,
};

/** Singleton global lock used for vcos_global_lock/unlock(). */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

#ifdef ANDROID
static VCOS_MUTEX_T printf_lock;
#endif

/* Create a per-thread key for faking up vcos access
 * on non-vcos threads.
 */
pthread_key_t _vcos_thread_current_key;

static VCOS_UNSIGNED _vcos_thread_current_key_created = 0;
static VCOS_ONCE_T current_thread_key_once;  /* init just once */

static void vcos_thread_cleanup(VCOS_THREAD_T *thread)
{
   vcos_semaphore_delete(&thread->suspend);
   if (thread->task_timer_created)
   {
      vcos_timer_delete(&thread->task_timer);
   }
}

static void vcos_dummy_thread_cleanup(void *cxt)
{
   VCOS_THREAD_T *thread = cxt;
   if (thread->dummy)
   {
      int i;
      /* call termination functions */
      for (i=0; thread->at_exit[i].pfn != NULL; i++)
      {
         thread->at_exit[i].pfn(thread->at_exit[i].cxt);
      }
      vcos_thread_cleanup(thread);
      vcos_free(thread);
   }
}

static void current_thread_key_init(void)
{
   vcos_assert(!_vcos_thread_current_key_created);
   pthread_key_create (&_vcos_thread_current_key, vcos_dummy_thread_cleanup);
   _vcos_thread_current_key_created = 1;
}

/* A VCOS wrapper for the thread which called vcos_init. */
static VCOS_THREAD_T vcos_thread_main;

static void *vcos_thread_entry(void *arg)
{
   int i;
   void *ret;
   VCOS_THREAD_T *thread = (VCOS_THREAD_T *)arg;

   vcos_assert(thread != NULL);
   thread->dummy = 0;

   pthread_setspecific(_vcos_thread_current_key, thread);
#if defined( HAVE_PRCTL ) && defined( PR_SET_NAME )
   /* cygwin doesn't have PR_SET_NAME */
   prctl( PR_SET_NAME, (unsigned long)thread->name, 0, 0, 0 );
#endif
   if (thread->legacy)
   {
      LEGACY_ENTRY_FN_T fn = (LEGACY_ENTRY_FN_T)thread->entry;
      (*fn)(0, thread->arg);
      ret = 0;
   }
   else
   {
      ret = (*thread->entry)(thread->arg);
   }

   /* call termination functions */
   for (i=0; thread->at_exit[i].pfn != NULL; i++)
   {
      thread->at_exit[i].pfn(thread->at_exit[i].cxt);
   }

   return ret;
}

static void _task_timer_expiration_routine(void *cxt)
{
   VCOS_THREAD_T *thread = (VCOS_THREAD_T *)cxt;

   vcos_assert(thread->orig_task_timer_expiration_routine);
   thread->orig_task_timer_expiration_routine(thread->orig_task_timer_context);
   thread->orig_task_timer_expiration_routine = NULL;
}

VCOS_STATUS_T vcos_thread_create(VCOS_THREAD_T *thread,
                                 const char *name,
                                 VCOS_THREAD_ATTR_T *attrs,
                                 VCOS_THREAD_ENTRY_FN_T entry,
                                 void *arg)
{
   VCOS_STATUS_T st;
   pthread_attr_t pt_attrs;
   VCOS_THREAD_ATTR_T *local_attrs = attrs ? attrs : &default_attrs;
   int rc;

   vcos_assert(thread);
   memset(thread, 0, sizeof(VCOS_THREAD_T));

   rc = pthread_attr_init(&pt_attrs);
   if (rc < 0)
      return VCOS_ENOMEM;

   st = vcos_semaphore_create(&thread->suspend, NULL, 0);
   if (st != VCOS_SUCCESS)
   {
      pthread_attr_destroy(&pt_attrs);
      return st;
   }

   pthread_attr_setstacksize(&pt_attrs, local_attrs->ta_stacksz);
#if VCOS_CAN_SET_STACK_ADDR
   if (local_attrs->ta_stackaddr)
   {
      pthread_attr_setstackaddr(&pt_attrs, local_attrs->ta_stackaddr);
   }
#else
   vcos_demand(local_attrs->ta_stackaddr == 0);
#endif

   /* pthread_attr_setpriority(&pt_attrs, local_attrs->ta_priority); */

   vcos_assert(local_attrs->ta_stackaddr == 0); /* Not possible */

   thread->entry = entry;
   thread->arg = arg;
   thread->legacy = local_attrs->legacy;

   strncpy(thread->name, name, sizeof(thread->name));
   thread->name[sizeof(thread->name)-1] = '\0';
   memset(thread->at_exit, 0, sizeof(thread->at_exit));

   rc = pthread_create(&thread->thread, &pt_attrs, vcos_thread_entry, thread);

   pthread_attr_destroy(&pt_attrs);

   if (rc < 0)
   {
      vcos_semaphore_delete(&thread->suspend);
      return VCOS_ENOMEM;
   }
   else
   {
      return VCOS_SUCCESS;
   }
}

void vcos_thread_join(VCOS_THREAD_T *thread,
                             void **pData)
{
   pthread_join(thread->thread, pData);
   vcos_thread_cleanup(thread);
}

VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_thread_create_classic(VCOS_THREAD_T *thread,
                                                            const char *name,
                                                            void *(*entry)(void *arg),
                                                            void *arg,
                                                            void *stack,
                                                            VCOS_UNSIGNED stacksz,
                                                            VCOS_UNSIGNED priaff,
                                                            VCOS_UNSIGNED timeslice,
                                                            VCOS_UNSIGNED autostart)
{
   VCOS_THREAD_ATTR_T attrs;
   vcos_thread_attr_init(&attrs);
   vcos_thread_attr_setstacksize(&attrs, stacksz);
   vcos_thread_attr_setpriority(&attrs, priaff & ~_VCOS_AFFINITY_MASK);
   vcos_thread_attr_setaffinity(&attrs, priaff & _VCOS_AFFINITY_MASK);
   (void)timeslice;
   (void)autostart;

   if (VCOS_CAN_SET_STACK_ADDR)
   {
      vcos_thread_attr_setstack(&attrs, stack, stacksz);
   }

   return vcos_thread_create(thread, name, &attrs, entry, arg);
}

uint64_t vcos_getmicrosecs64_internal(void)
{
   struct timeval tv;
   uint64_t tm = 0;

   if (!gettimeofday(&tv, NULL))
   {
      tm = (tv.tv_sec * 1000000LL) + tv.tv_usec;
   }

   return tm;
}

#ifdef ANDROID

static int log_prio[] =
{
   ANDROID_LOG_INFO,    /* VCOS_LOG_UNINITIALIZED */
   ANDROID_LOG_INFO,    /* VCOS_LOG_NEVER */
   ANDROID_LOG_ERROR,   /* VCOS_LOG_ERROR */
   ANDROID_LOG_WARN,    /* VCOS_LOG_WARN */
   ANDROID_LOG_INFO,    /* VCOS_LOG_INFO */
   ANDROID_LOG_DEBUG    /* VCOS_LOG_TRACE */
};

int vcos_use_android_log = 1;
int vcos_log_to_file = 0;
#else
int vcos_use_android_log = 0;
int vcos_log_to_file = 0;
#endif

static FILE * log_fhandle = NULL;

void vcos_vlog_default_impl(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, va_list args)
{
   (void)_level;

#ifdef ANDROID
   if ( vcos_use_android_log )
   {
      __android_log_vprint(log_prio[_level], cat->name, fmt, args);
   }
   else
   {
      vcos_mutex_lock(&printf_lock);
#endif
      if(NULL != log_fhandle)
      {
         if (cat->flags.want_prefix)
            fprintf( log_fhandle, "%s: ", cat->name );
         vfprintf(log_fhandle, fmt, args);
         fputs("\n", log_fhandle);
         fflush(log_fhandle);
      }
#ifdef ANDROID
      vcos_mutex_unlock(&printf_lock);
   }
#endif
}

void _vcos_log_platform_init(void)
{
   if(vcos_log_to_file)
   {
      char log_fname[100];
#ifdef ANDROID
      snprintf(log_fname, 100, "/data/log/vcos_log%u.txt", vcos_process_id_current());
#else
      snprintf(log_fname, 100, "/var/log/vcos_log%u.txt", vcos_process_id_current());
#endif
      log_fhandle = fopen(log_fname, "w");
   }
   else
      log_fhandle = stderr;
}

/* Flags for init/deinit components */
enum
{
   VCOS_INIT_NAMED_SEM   = (1 << 0),
   VCOS_INIT_PRINTF_LOCK = (1 << 1),
   VCOS_INIT_MAIN_SEM    = (1 << 2),
   VCOS_INIT_MSGQ        = (1 << 3),
   VCOS_INIT_ALL         = 0xffffffff
};

static void vcos_term(uint32_t flags)
{
   if (flags & VCOS_INIT_MSGQ)
      vcos_msgq_deinit();

   if (flags & VCOS_INIT_MAIN_SEM)
      vcos_semaphore_delete(&vcos_thread_main.suspend);

#ifdef ANDROID
   if (flags & VCOS_INIT_PRINTF_LOCK)
      vcos_mutex_delete(&printf_lock);
#endif

   if (flags & VCOS_INIT_NAMED_SEM)
      _vcos_named_semaphore_deinit();
}

VCOS_STATUS_T vcos_platform_init(void)
{
   VCOS_STATUS_T st;
   uint32_t flags = 0;
   int pst;

   st = _vcos_named_semaphore_init();
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   flags |= VCOS_INIT_NAMED_SEM;

#ifdef HAVE_MTRACE
   /* enable glibc memory debugging, if the environment
    * variable MALLOC_TRACE names a valid file.
    */
   mtrace();
#endif

#ifdef ANDROID
   st = vcos_mutex_create(&printf_lock, "printf");
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   flags |= VCOS_INIT_PRINTF_LOCK;
#endif

   st = vcos_once(&current_thread_key_once, current_thread_key_init);
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   /* Initialise a VCOS wrapper for the thread which called vcos_init. */
   st = vcos_semaphore_create(&vcos_thread_main.suspend, NULL, 0);
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   flags |= VCOS_INIT_MAIN_SEM;

   vcos_thread_main.thread = pthread_self();

   pst = pthread_setspecific(_vcos_thread_current_key, &vcos_thread_main);
   if (!vcos_verify(pst == 0))
   {
      st = VCOS_EINVAL;
      goto end;
   }

   st = vcos_msgq_init();
   if (!vcos_verify(st == VCOS_SUCCESS))
      goto end;

   flags |= VCOS_INIT_MSGQ;

   vcos_logging_init();

end:
   if (st != VCOS_SUCCESS)
      vcos_term(flags);

   return st;
}

void vcos_platform_deinit(void)
{
   vcos_term(VCOS_INIT_ALL);
}

void vcos_global_lock(void)
{
   pthread_mutex_lock(&lock);
}

void vcos_global_unlock(void)
{
   pthread_mutex_unlock(&lock);
}

void vcos_thread_exit(void *arg)
{
   VCOS_THREAD_T *thread = vcos_thread_current();

   if ( thread && thread->dummy )
   {
      vcos_free ( (void*) thread );
      thread = NULL;
   }

   pthread_exit(arg);
}

void vcos_thread_attr_init(VCOS_THREAD_ATTR_T *attrs)
{
   *attrs = default_attrs;
}

VCOS_STATUS_T vcos_pthreads_map_error(int error)
{
   switch (error)
   {
   case ENOMEM:
      return VCOS_ENOMEM;
   case ENXIO:
      return VCOS_ENXIO;
   case EAGAIN:
      return VCOS_EAGAIN;
   case ENOSPC:
      return VCOS_ENOSPC;
   default:
      return VCOS_EINVAL;
   }
}

VCOS_STATUS_T vcos_pthreads_map_errno(void)
{
   return vcos_pthreads_map_error(errno);
}

void _vcos_task_timer_set(void (*pfn)(void*), void *cxt, VCOS_UNSIGNED ms)
{
   VCOS_THREAD_T *thread = vcos_thread_current();

   if (thread == NULL)
      return;

   vcos_assert(thread->orig_task_timer_expiration_routine == NULL);

   if (!thread->task_timer_created)
   {
      VCOS_STATUS_T st = vcos_timer_create(&thread->task_timer, NULL,
                                _task_timer_expiration_routine, thread);
      (void)st;
      vcos_assert(st == VCOS_SUCCESS);
      thread->task_timer_created = 1;
   }

   thread->orig_task_timer_expiration_routine = pfn;
   thread->orig_task_timer_context = cxt;

   vcos_timer_set(&thread->task_timer, ms);
}

void _vcos_task_timer_cancel(void)
{
   VCOS_THREAD_T *thread = vcos_thread_current();

   if (thread == NULL || !thread->task_timer_created)
     return;

   vcos_timer_cancel(&thread->task_timer);
   thread->orig_task_timer_expiration_routine = NULL;
}

int vcos_vsnprintf( char *buf, size_t buflen, const char *fmt, va_list ap )
{
   return vsnprintf( buf, buflen, fmt, ap );
}

int vcos_snprintf(char *buf, size_t buflen, const char *fmt, ...)
{
   int ret;
   va_list ap;
   va_start(ap,fmt);
   ret = vsnprintf(buf, buflen, fmt, ap);
   va_end(ap);
   return ret;
}

int vcos_have_rtos(void)
{
   return 1;
}

const char * vcos_thread_get_name(const VCOS_THREAD_T *thread)
{
   return thread->name;
}

#ifdef VCOS_HAVE_BACKTRACK
void __attribute__((weak)) vcos_backtrace_self(void);
#endif

void vcos_pthreads_logging_assert(const char *file, const char *func, unsigned int line, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, "assertion failure:%s:%d:%s():",
           file, line, func);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fprintf(stderr, "\n");

#ifdef VCOS_HAVE_BACKTRACK
   if (vcos_backtrace_self)
      vcos_backtrace_self();
#endif
   abort();
}

extern VCOS_STATUS_T vcos_thread_at_exit(void (*pfn)(void*), void *cxt)
{
   int i;
   VCOS_THREAD_T *self = vcos_thread_current();
   if (!self)
   {
      vcos_assert(0);
      return VCOS_EINVAL;
   }
   for (i=0; i<VCOS_MAX_EXIT_HANDLERS; i++)
   {
      if (self->at_exit[i].pfn == NULL)
      {
         self->at_exit[i].pfn = pfn;
         self->at_exit[i].cxt = cxt;
         return VCOS_SUCCESS;
      }
   }
   return VCOS_ENOSPC;
}

void vcos_set_args(int argc, const char **argv)
{
   vcos_argc = argc;
   vcos_argv = argv;
}

int vcos_get_argc(void)
{
   return vcos_argc;
}

const char ** vcos_get_argv(void)
{
   return vcos_argv;
}

/* we can't inline this, because HZ comes from sys/param.h which
 * dumps all sorts of junk into the global namespace, notable MIN and
 * MAX.
 */
uint32_t _vcos_get_ticks_per_second(void)
{
   return HZ;
}

VCOS_STATUS_T vcos_once(VCOS_ONCE_T *once_control,
                        void (*init_routine)(void))
{
   int rc = pthread_once(once_control, init_routine);
   if (rc != 0)
   {
      switch (errno)
      {
      case EINVAL:
         return VCOS_EINVAL;
      default:
         vcos_assert(0);
         return VCOS_EACCESS;
      }
   }
   else
   {
      return VCOS_SUCCESS;
   }
}

VCOS_THREAD_T *vcos_dummy_thread_create(void)
{
   VCOS_STATUS_T st;
   VCOS_THREAD_T *thread_hndl = NULL;
   int rc;

   thread_hndl = (VCOS_THREAD_T *)vcos_malloc(sizeof(VCOS_THREAD_T), NULL);
   vcos_assert(thread_hndl != NULL);

   memset(thread_hndl, 0, sizeof(VCOS_THREAD_T));

   thread_hndl->dummy = 1;
   thread_hndl->thread = pthread_self();

   st = vcos_semaphore_create(&thread_hndl->suspend, NULL, 0);
   if (st != VCOS_SUCCESS)
   {
      vcos_free(thread_hndl);
      return( thread_hndl );
   }

   vcos_once(&current_thread_key_once, current_thread_key_init);

   rc = pthread_setspecific(_vcos_thread_current_key,
                            thread_hndl);
   (void)rc;

   return( thread_hndl );
}

/***********************************************************
 *
 * Timers
 *
 ***********************************************************/

/* On Linux we could use POSIX timers with a bit of synchronization.
 * Unfortunately POSIX timers on Bionic are NOT POSIX compliant
 * what makes that option not viable.
 * That's why we ended up with our own implementation of timers.
 * NOTE: That condition variables on Bionic are also buggy and
 * they work incorrectly with CLOCK_MONOTONIC, so we have to
 * use CLOCK_REALTIME (and hope that no one will change the time
 * significantly after the timer has been set up
 */
#define NSEC_IN_SEC  (1000*1000*1000)
#define MSEC_IN_SEC  (1000)
#define NSEC_IN_MSEC (1000*1000)

static int _timespec_is_zero(struct timespec *ts)
{
   return ((ts->tv_sec == 0) && (ts->tv_nsec == 0));
}

static void _timespec_set_zero(struct timespec *ts)
{
   ts->tv_sec = ts->tv_nsec = 0;
}

/* Adds left to right and stores the result in left */
static void _timespec_add(struct timespec *left, struct timespec *right)
{
   left->tv_sec += right->tv_sec;
   left->tv_nsec += right->tv_nsec;
   if (left->tv_nsec >= (NSEC_IN_SEC))
   {
      left->tv_nsec -= NSEC_IN_SEC;
      left->tv_sec++;
   }
}

static int _timespec_is_larger(struct timespec *left, struct timespec *right)
{
   if (left->tv_sec != right->tv_sec)
      return left->tv_sec > right->tv_sec;
   else
      return left->tv_nsec > right->tv_nsec;
}

static void* _timer_thread(void *arg)
{
   VCOS_TIMER_T *timer = (VCOS_TIMER_T*)arg;

   pthread_mutex_lock(&timer->lock);
   while (!timer->quit)
   {
      struct timespec now;

      /* Wait until next expiry time, or until timer's settings are changed */
      if (_timespec_is_zero(&timer->expires))
         pthread_cond_wait(&timer->settings_changed, &timer->lock);
      else
         pthread_cond_timedwait(&timer->settings_changed, &timer->lock, &timer->expires);

      /* See if the timer has expired - reloop if it didn't */
      clock_gettime(CLOCK_REALTIME, &now);
      if (_timespec_is_zero(&timer->expires) || _timespec_is_larger(&timer->expires, &now))
         continue;

      /* The timer has expired. Clear the expiry time and call the
       * expiration routine
       */
      _timespec_set_zero(&timer->expires);
      timer->orig_expiration_routine(timer->orig_context);
   }
   pthread_mutex_unlock(&timer->lock);

   return NULL;
}

VCOS_STATUS_T vcos_timer_init(void)
{
   return VCOS_SUCCESS;
}

VCOS_STATUS_T vcos_pthreads_timer_create(VCOS_TIMER_T *timer,
                                const char *name,
                                void (*expiration_routine)(void *context),
                                void *context)
{
   pthread_mutexattr_t lock_attr;
   VCOS_STATUS_T result = VCOS_SUCCESS;
   int settings_changed_initialized = 0;
   int lock_attr_initialized = 0;
   int lock_initialized = 0;

   (void)name;

   vcos_assert(timer);
   vcos_assert(expiration_routine);

   memset(timer, 0, sizeof(VCOS_TIMER_T));

   timer->orig_expiration_routine = expiration_routine;
   timer->orig_context = context;

   /* Create conditional variable for notifying the timer's thread
    * when settings change.
    */
   if (result == VCOS_SUCCESS)
   {
      int rc = pthread_cond_init(&timer->settings_changed, NULL);
      if (rc == 0)
         settings_changed_initialized = 1;
      else
         result = vcos_pthreads_map_error(rc);
   }

   /* Create attributes for the lock (we want it to be recursive) */
   if (result == VCOS_SUCCESS)
   {
      int rc = pthread_mutexattr_init(&lock_attr);
      if (rc == 0)
      {
         pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
         lock_attr_initialized = 1;
      }
      else
      {
         result = vcos_pthreads_map_error(rc);
      }
   }

   /* Create lock for the timer structure */
   if (result == VCOS_SUCCESS)
   {
      int rc = pthread_mutex_init(&timer->lock, &lock_attr);
      if (rc == 0)
         lock_initialized = 1;
      else
         result = vcos_pthreads_map_error(rc);
   }

   /* Lock attributes are no longer needed */
   if (lock_attr_initialized)
      pthread_mutexattr_destroy(&lock_attr);

   /* Create the underlying thread */
   if (result == VCOS_SUCCESS)
   {
      int rc = pthread_create(&timer->thread, NULL, _timer_thread, timer);
      if (rc != 0)
         result = vcos_pthreads_map_error(rc);
   }

   /* Clean up if anything went wrong */
   if (result != VCOS_SUCCESS)
   {
      if (lock_initialized)
         pthread_mutex_destroy(&timer->lock);

      if (settings_changed_initialized)
         pthread_cond_destroy(&timer->settings_changed);
   }

   return result;
}

void vcos_pthreads_timer_set(VCOS_TIMER_T *timer, VCOS_UNSIGNED delay_ms)
{
   struct timespec now;

   vcos_assert(timer);

   /* Other implementations of this function do undefined things
    * when delay_ms is 0. This implementation will simply assert and return
    */
   vcos_assert(delay_ms != 0);
   if (delay_ms == 0)
      return;

   pthread_mutex_lock(&timer->lock);

   /* Calculate the new absolute expiry time */
   clock_gettime(CLOCK_REALTIME, &now);
   timer->expires.tv_sec = delay_ms / MSEC_IN_SEC;
   timer->expires.tv_nsec = (delay_ms % MSEC_IN_SEC) * NSEC_IN_MSEC;
   _timespec_add(&timer->expires, &now);

   /* Notify the timer's thread about the change */
   pthread_cond_signal(&timer->settings_changed);

   pthread_mutex_unlock(&timer->lock);
}

void vcos_pthreads_timer_cancel(VCOS_TIMER_T *timer)
{
   vcos_assert(timer);

   pthread_mutex_lock(&timer->lock);

   _timespec_set_zero(&timer->expires);
   pthread_cond_signal(&timer->settings_changed);

   pthread_mutex_unlock(&timer->lock);
}

void vcos_pthreads_timer_delete(VCOS_TIMER_T *timer)
{
   vcos_assert(timer);

   pthread_mutex_lock(&timer->lock);

   /* Other implementation of this function (e.g. ThreadX)
    * disallow it being called from the expiration routine
    */
   vcos_assert(pthread_self() != timer->thread);

   /* Stop the timer and set flag telling the timer thread to quit */
   _timespec_set_zero(&timer->expires);
   timer->quit = 1;

   /* Notify the timer's thread about the change */
   pthread_cond_signal(&timer->settings_changed);

   /* Release the lock, so that the timer's thread can quit */
   pthread_mutex_unlock(&timer->lock);

   /* Wait for the timer thread to finish */
   pthread_join(timer->thread, NULL);

   /* Free resources used by the timer */
   pthread_mutex_destroy(&timer->lock);
   pthread_cond_destroy(&timer->settings_changed);
}
