/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gx_pthread.h).
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

#ifndef _CTR_PTHREAD_WRAP_CTR_
#define _CTR_PTHREAD_WRAP_CTR_

#include <3ds/thread.h>
#include <3ds/synchronization.h>
#include <3ds/svc.h>
#include <time.h>
#include <errno.h>
#include <retro_inline.h>

#define STACKSIZE (4 * 1024)

typedef Thread     pthread_t;
typedef LightLock  pthread_mutex_t;
typedef void*      pthread_mutexattr_t;
typedef int        pthread_attr_t;
typedef LightEvent pthread_cond_t;
typedef int        pthread_condattr_t;

/* libctru threads return void but pthreads return void pointer */
static bool mutex_inited = false;
static LightLock safe_double_thread_launch;
static void *(*start_routine_jump)(void*);

static void ctr_thread_launcher(void* data)
{
	void *(*start_routine_jump_safe)(void*) = start_routine_jump;
	LightLock_Unlock(&safe_double_thread_launch);
	start_routine_jump_safe(data);
}

static INLINE int pthread_create(pthread_t *thread,
      const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{
   s32 prio = 0;
   Thread new_ctr_thread;

   if (!mutex_inited)
   {
	   LightLock_Init(&safe_double_thread_launch);
	   mutex_inited = true;
   }

   /*Must wait if attempting to launch 2 threads at once to prevent corruption of function pointer*/
   while (LightLock_TryLock(&safe_double_thread_launch) != 0);

   svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

   start_routine_jump = start_routine;
   new_ctr_thread     = threadCreate(ctr_thread_launcher, arg, STACKSIZE, prio - 1, -1/*No affinity, use any CPU*/, false);

   if (!new_ctr_thread)
   {
	   LightLock_Unlock(&safe_double_thread_launch);
	   return EAGAIN;
   }

   *thread = new_ctr_thread;
   return 0;
}

static INLINE pthread_t pthread_self(void)
{
   return threadGetCurrent();
}

static INLINE int pthread_mutex_init(pthread_mutex_t *mutex,
      const pthread_mutexattr_t *attr)
{
   LightLock_Init(mutex);
   return 0;
}

static INLINE int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
   /*Nothing to destroy*/
   return 0;
}

static INLINE int pthread_mutex_lock(pthread_mutex_t *mutex)
{
   return LightLock_TryLock(mutex);
}

static INLINE int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
   LightLock_Unlock(mutex);
   return 0;
}

static INLINE void pthread_exit(void *retval)
{
   /*Yes the pointer to int cast is not ideal*/
   /*threadExit((int)retval);*/
   (void)retval;
}

static INLINE int pthread_detach(pthread_t thread)
{
   /* FIXME: pthread_detach equivalent missing? */
   (void)thread;
   return 0;
}

static INLINE int pthread_join(pthread_t thread, void **retval)
{
   /*retval is ignored*/
   return threadJoin(thread, U64_MAX);
}

static INLINE int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
   return LightLock_TryLock(mutex);
}

static INLINE int pthread_cond_wait(pthread_cond_t *cond,
      pthread_mutex_t *mutex)
{
   LightEvent_Wait(cond);
   return 0;
}

static INLINE int pthread_cond_timedwait(pthread_cond_t *cond,
      pthread_mutex_t *mutex, const struct timespec *abstime)
{
   while (true)
   {
       struct timespec now = {0};
	    /* Missing clock_gettime*/
       struct timeval tm;

       gettimeofday(&tm, NULL);
       now.tv_sec = tm.tv_sec;
       now.tv_nsec = tm.tv_usec * 1000;
       if (LightEvent_TryWait(cond) != 0 || now.tv_sec > abstime->tv_sec || (now.tv_sec == abstime->tv_sec && now.tv_nsec > abstime->tv_nsec))
		   break;
   }

   return 0;
}

static INLINE int pthread_cond_init(pthread_cond_t *cond,
      const pthread_condattr_t *attr)
{
   LightEvent_Init(cond, RESET_ONESHOT);
   return 0;
}

static INLINE int pthread_cond_signal(pthread_cond_t *cond)
{
   LightEvent_Signal(cond);
   return 0;
}

static INLINE int pthread_cond_broadcast(pthread_cond_t *cond)
{
   LightEvent_Signal(cond);
   return 0;
}

static INLINE int pthread_cond_destroy(pthread_cond_t *cond)
{
   /*nothing to destroy*/
   return 0;
}

static INLINE int pthread_equal(pthread_t t1, pthread_t t2)
{
	if (threadGetHandle(t1) == threadGetHandle(t2))
		return 1;
	return 0;
}

#endif
