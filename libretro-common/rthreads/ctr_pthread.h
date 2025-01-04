/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <3ds/services/apt.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <retro_inline.h>

#define STACKSIZE (32 * 1024)

#ifndef PTHREAD_SCOPE_PROCESS
/* An earlier version of devkitARM does not define the pthread types. Can remove in r54+. */

typedef Thread     pthread_t;
typedef LightLock  pthread_mutex_t;
typedef void*      pthread_mutexattr_t;
typedef int        pthread_attr_t;
typedef uint32_t   pthread_cond_t;
typedef int        pthread_condattr_t;
#endif

#ifndef USE_CTRULIB_2
/* Backported CondVar API from libctru 2.0, and under its license:
   https://github.com/devkitPro/libctru
   Slightly modified for compatibility with older libctru. */

typedef s32 CondVar;

static INLINE Result syncArbitrateAddress(s32* addr, ArbitrationType type, s32 value)
{
   return svcArbitrateAddress(__sync_get_arbiter(), (u32)addr, type, value, 0);
}

static INLINE Result syncArbitrateAddressWithTimeout(s32* addr, ArbitrationType type, s32 value, s64 timeout_ns)
{
   return svcArbitrateAddress(__sync_get_arbiter(), (u32)addr, type, value, timeout_ns);
}

static INLINE void __dmb(void)
{
	__asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 5" :: [val] "r" (0) : "memory");
}

static INLINE void CondVar_BeginWait(CondVar* cv, LightLock* lock)
{
	s32 val;
	do
		val = __ldrex(cv) - 1;
	while (__strex(cv, val));
	LightLock_Unlock(lock);
}

static INLINE bool CondVar_EndWait(CondVar* cv, s32 num_threads)
{
	bool hasWaiters;
	s32 val;

	do {
		val = __ldrex(cv);
		hasWaiters = val < 0;
		if (hasWaiters)
		{
			if (num_threads < 0)
				val = 0;
			else if (val <= -num_threads)
				val += num_threads;
			else
				val = 0;
		}
	} while (__strex(cv, val));

	return hasWaiters;
}

static INLINE void CondVar_Init(CondVar* cv)
{
	*cv = 0;
}

static INLINE void CondVar_Wait(CondVar* cv, LightLock* lock)
{
	CondVar_BeginWait(cv, lock);
	syncArbitrateAddress(cv, ARBITRATION_WAIT_IF_LESS_THAN, 0);
	LightLock_Lock(lock);
}

static INLINE int CondVar_WaitTimeout(CondVar* cv, LightLock* lock, s64 timeout_ns)
{
	CondVar_BeginWait(cv, lock);

	bool timedOut = false;
	Result rc = syncArbitrateAddressWithTimeout(cv, ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, 0, timeout_ns);
	if (R_DESCRIPTION(rc) == RD_TIMEOUT)
	{
		timedOut = CondVar_EndWait(cv, 1);
		__dmb();
	}

	LightLock_Lock(lock);
	return timedOut;
}

static INLINE void CondVar_WakeUp(CondVar* cv, s32 num_threads)
{
	__dmb();
	if (CondVar_EndWait(cv, num_threads))
		syncArbitrateAddress(cv, ARBITRATION_SIGNAL, num_threads);
	else
		__dmb();
}

static INLINE void CondVar_Signal(CondVar* cv)
{
	CondVar_WakeUp(cv, 1);
}

static INLINE void CondVar_Broadcast(CondVar* cv)
{
	CondVar_WakeUp(cv, ARBITRATION_SIGNAL_ALL);
}
/* End libctru 2.0 backport */
#endif

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
   int procnum = -2; /* use default cpu */
   bool isNew3DS;

   APT_CheckNew3DS(&isNew3DS);

   if (isNew3DS)
      procnum = 2;

   if (!mutex_inited)
   {
      LightLock_Init(&safe_double_thread_launch);
      mutex_inited = true;
   }

   /* Must wait if attempting to launch 2 threads at once to prevent corruption of function pointer*/
   while (LightLock_TryLock(&safe_double_thread_launch) != 0);

   svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

   start_routine_jump = start_routine;
   new_ctr_thread     = threadCreate(ctr_thread_launcher, arg, STACKSIZE, prio - 1, procnum, false);

   if (!new_ctr_thread)
   {
      LightLock_Unlock(&safe_double_thread_launch);
      return EAGAIN;
   }

   *thread = (pthread_t)new_ctr_thread;
   return 0;
}

static INLINE pthread_t pthread_self(void)
{
   return (pthread_t)threadGetCurrent();
}

static INLINE int pthread_mutex_init(pthread_mutex_t *mutex,
      const pthread_mutexattr_t *attr)
{
   LightLock_Init((LightLock *)mutex);
   return 0;
}

static INLINE int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
   /*Nothing to destroy*/
   return 0;
}

static INLINE int pthread_mutex_lock(pthread_mutex_t *mutex)
{
   LightLock_Lock((LightLock *)mutex);
   return 0;
}

static INLINE int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
   LightLock_Unlock((LightLock *)mutex);
   return 0;
}

static INLINE void pthread_exit(void *retval)
{
   /*Yes the pointer to int cast is not ideal*/
   /*threadExit((int)retval);*/
   (void)retval;

   threadExit(0);
}

static INLINE int pthread_detach(pthread_t thread)
{
   threadDetach((Thread)thread);
   return 0;
}

static INLINE int pthread_join(pthread_t thread, void **retval)
{
   /*retval is ignored*/
   if (threadJoin((Thread)thread, INT64_MAX))
      return -1;

   threadFree((Thread)thread);

   return 0;
}

static INLINE int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
   return LightLock_TryLock((LightLock *)mutex);
}

static INLINE int pthread_cond_wait(pthread_cond_t *cond,
      pthread_mutex_t *mutex)
{
   CondVar_Wait((CondVar *)cond, (LightLock *)mutex);
   return 0;
}

static INLINE int pthread_cond_timedwait(pthread_cond_t *cond,
      pthread_mutex_t *mutex, const struct timespec *abstime)
{
   /* Missing clock_gettime*/
   struct timespec now;
   struct timeval tm;
   int retval = 0;

   do
   {
      s64 timeout;
      gettimeofday(&tm, NULL);
      now.tv_sec  = tm.tv_sec;
      now.tv_nsec = tm.tv_usec * 1000;

      if ((timeout = (abstime->tv_sec - now.tv_sec) * 1000000000 +
(abstime->tv_nsec - now.tv_nsec)) < 0)
      {
         retval = ETIMEDOUT;
         break;
      }

      if (!CondVar_WaitTimeout((CondVar *)cond, (LightLock *)mutex, timeout))
         break;
   } while (1);

   return retval;
}

static INLINE int pthread_cond_init(pthread_cond_t *cond,
      const pthread_condattr_t *attr)
{
   CondVar_Init((CondVar *)cond);
   return 0;
}

static INLINE int pthread_cond_signal(pthread_cond_t *cond)
{
   CondVar_Signal((CondVar *)cond);
   return 0;
}

static INLINE int pthread_cond_broadcast(pthread_cond_t *cond)
{
   CondVar_Broadcast((CondVar *)cond);
   return 0;
}

static INLINE int pthread_cond_destroy(pthread_cond_t *cond)
{
   /*Nothing to destroy*/
   return 0;
}

static INLINE int pthread_equal(pthread_t t1, pthread_t t2)
{
	if (threadGetHandle((Thread)t1) == threadGetHandle((Thread)t2))
		return 1;
	return 0;
}

#endif
