/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rthreads.c).
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

#include <stdlib.h>

#include <rthreads/rthreads.h>

#if defined(_WIN32)
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#elif defined(GEKKO)
#include "gx_pthread.h"
#elif defined(PSP)
#include "psp_pthread.h"
#else
#include <pthread.h>
#include <time.h>
#endif

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

struct thread_data
{
   void (*func)(void*);
   void *userdata;
};

struct sthread
{
#ifdef _WIN32
   HANDLE thread;
#else
   pthread_t id;
#endif
};

struct slock
{
#ifdef _WIN32
   HANDLE lock;
#else
   pthread_mutex_t lock;
#endif
};

struct scond
{
#ifdef _WIN32
   HANDLE event;
#else
   pthread_cond_t cond;
#endif
};

#ifdef _WIN32
static DWORD CALLBACK thread_wrap(void *data_)
#else
static void *thread_wrap(void *data_)
#endif
{
   struct thread_data *data = (struct thread_data*)data_;
   if (!data)
	   return 0;
   data->func(data->userdata);
   free(data);
   return 0;
}

/**
 * sthread_create:
 * @start_routine           : thread entry callback function
 * @userdata                : pointer to userdata that will be made
 *                            available in thread entry callback function
 *
 * Create a new thread.
 *
 * Returns: pointer to new thread if successful, otherwise NULL.
 */
sthread_t *sthread_create(void (*thread_func)(void*), void *userdata)
{
   sthread_t *thread = (sthread_t*)calloc(1, sizeof(*thread));
   struct thread_data *data;
   if (!thread)
      return NULL;

   data = (struct thread_data*)calloc(1, sizeof(*data));
   if (!data)
   {
      free(thread);
      return NULL;
   }

   data->func = thread_func;
   data->userdata = userdata;

#ifdef _WIN32
   thread->thread = CreateThread(NULL, 0, thread_wrap, data, 0, NULL);
   if (!thread->thread)
#else
   if (pthread_create(&thread->id, NULL, thread_wrap, data) < 0)
#endif
   {
      free(data);
      free(thread);
      return NULL;
   }

   return thread;
}

/**
 * sthread_detach:
 * @thread                  : pointer to thread object 
 *
 * Detach a thread. When a detached thread terminates, its
 * resource sare automatically released back to the system
 * without the need for another thread to join with the 
 * terminated thread.
 *
 * Returns: 0 on success, otherwise it returns a non-zero error number.
 */
int sthread_detach(sthread_t *thread)
{
#ifdef _WIN32
   CloseHandle(thread->thread);
   free(thread);
   return 0;
#else
   return pthread_detach(thread->id);
#endif
}

/**
 * sthread_join:
 * @thread                  : pointer to thread object 
 *
 * Join with a terminated thread. Waits for the thread specified by
 * @thread to terminate. If that thread has already terminated, then
 * it will return immediately. The thread specified by @thread must
 * be joinable.
 * 
 * Returns: 0 on success, otherwise it returns a non-zero error number.
 */
void sthread_join(sthread_t *thread)
{
#ifdef _WIN32
   WaitForSingleObject(thread->thread, INFINITE);
   CloseHandle(thread->thread);
#else
   pthread_join(thread->id, NULL);
#endif
   free(thread);
}

/**
 * slock_new:
 *
 * Create and initialize a new mutex. Must be manually
 * freed.
 *
 * Returns: pointer to a new mutex if successful, otherwise NULL.
 **/
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)calloc(1, sizeof(*lock));
   if (!lock)
      return NULL;

#ifdef _WIN32
   lock->lock = CreateMutex(NULL, FALSE, NULL);
   if (!lock->lock)
#else
   if (pthread_mutex_init(&lock->lock, NULL) < 0)
#endif
   {
      free(lock);
      return NULL;
   }

   return lock;
}

/**
 * slock_free:
 * @lock                    : pointer to mutex object 
 *
 * Frees a mutex.
 **/
void slock_free(slock_t *lock)
{
   if (!lock)
      return;

#ifdef _WIN32
   CloseHandle(lock->lock);
#else
   pthread_mutex_destroy(&lock->lock);
#endif
   free(lock);
}

/**
 * slock_lock:
 * @lock                    : pointer to mutex object 
 *
 * Locks a mutex. If a mutex is already locked by
 * another thread, the calling thread shall block until
 * the mutex becomes available.
**/
void slock_lock(slock_t *lock)
{
#ifdef _WIN32
   WaitForSingleObject(lock->lock, INFINITE);
#else
   pthread_mutex_lock(&lock->lock);
#endif
}

/**
 * slock_unlock:
 * @lock                    : pointer to mutex object 
 *
 * Unlocks a mutex.
 **/
void slock_unlock(slock_t *lock)
{
#ifdef _WIN32
   ReleaseMutex(lock->lock);
#else
   pthread_mutex_unlock(&lock->lock);
#endif
}

/**
 * scond_new:
 *
 * Creates and initializes a condition variable. Must
 * be manually freed.
 *
 * Returns: pointer to new condition variable on success,
 * otherwise NULL.
 **/
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)calloc(1, sizeof(*cond));
   if (!cond)
      return NULL;

#ifdef _WIN32
   cond->event = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!cond->event)
#else
   if (pthread_cond_init(&cond->cond, NULL) < 0)
#endif
   {
      free(cond);
      return NULL;
   }

   return cond;
}

/**
 * scond_free:
 * @cond                    : pointer to condition variable object 
 *
 * Frees a condition variable.
**/
void scond_free(scond_t *cond)
{
   if (!cond)
      return;

#ifdef _WIN32
   CloseHandle(cond->event);
#else
   pthread_cond_destroy(&cond->cond);
#endif
   free(cond);
}

/**
 * scond_wait:
 * @cond                    : pointer to condition variable object 
 * @lock                    : pointer to mutex object 
 *
 * Block on a condition variable (i.e. wait on a condition). 
 **/
void scond_wait(scond_t *cond, slock_t *lock)
{
#ifdef _WIN32
   WaitForSingleObject(cond->event, 0);
   
   SignalObjectAndWait(lock->lock, cond->event, INFINITE, FALSE);
   slock_lock(lock);
#else
   pthread_cond_wait(&cond->cond, &lock->lock);
#endif
}

/**
 * scond_broadcast:
 * @cond                    : pointer to condition variable object 
 *
 * Broadcast a condition. Unblocks all threads currently blocked
 * on the specified condition variable @cond. 
 **/
int scond_broadcast(scond_t *cond)
{
#ifdef _WIN32
   /* FIXME _- check how this function should differ 
    * from scond_signal implementation. */
   SetEvent(cond->event);
   return 0;
#else
   return pthread_cond_broadcast(&cond->cond);
#endif
}

/**
 * scond_signal:
 * @cond                    : pointer to condition variable object 
 *
 * Signal a condition. Unblocks at least one of the threads currently blocked
 * on the specified condition variable @cond. 
 **/
void scond_signal(scond_t *cond)
{
#ifdef _WIN32
   SetEvent(cond->event);
#else
   pthread_cond_signal(&cond->cond);
#endif
}

/**
 * scond_wait_timeout:
 * @cond                    : pointer to condition variable object 
 * @lock                    : pointer to mutex object 
 * @timeout_us              : timeout (in microseconds)
 *
 * Try to block on a condition variable (i.e. wait on a condition) until
 * @timeout_us elapses.
 *
 * Returns: false (0) if timeout elapses before condition variable is
 * signaled or broadcast, otherwise true (1).
 **/
bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us)
{
#ifdef _WIN32
   DWORD ret;

   WaitForSingleObject(cond->event, 0);
   ret = SignalObjectAndWait(lock->lock, cond->event,
         (DWORD)(timeout_us) / 1000, FALSE);

   slock_lock(lock);
   return ret == WAIT_OBJECT_0;
#else
   int ret;
   int64_t seconds, remainder;
   struct timespec now = {0};

#ifdef __MACH__
   /* OSX doesn't have clock_gettime. */
   clock_serv_t cclock;
   mach_timespec_t mts;

   host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
   clock_get_time(cclock, &mts);
   mach_port_deallocate(mach_task_self(), cclock);
   now.tv_sec = mts.tv_sec;
   now.tv_nsec = mts.tv_nsec;
#elif defined(__CELLOS_LV2__)
   sys_time_sec_t s;
   sys_time_nsec_t n;

   sys_time_get_current_time(&s, &n);
   now.tv_sec  = s;
   now.tv_nsec = n;
#elif defined(__mips__)
   struct timeval tm;

   gettimeofday(&tm, NULL);
   now.tv_sec = tm.tv_sec;
   now.tv_nsec = tm.tv_usec * 1000;
#elif !defined(GEKKO)
   /* timeout on libogc is duration, not end time. */
   clock_gettime(CLOCK_REALTIME, &now);
#endif

   seconds      = timeout_us / INT64_C(1000000);
   remainder    = timeout_us % INT64_C(1000000);

   now.tv_sec  += seconds;
   now.tv_nsec += remainder * INT64_C(1000);

   ret = pthread_cond_timedwait(&cond->cond, &lock->lock, &now);
   return (ret == 0);
#endif
}
