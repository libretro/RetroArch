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

#include <rthreads/rthreads.h>
#include <stdlib.h>

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
   data->func(data->userdata);
   free(data);
   return 0;
}

sthread_t *sthread_create(void (*thread_func)(void*), void *userdata)
{
   sthread_t *thread = (sthread_t*)calloc(1, sizeof(*thread));
   if (!thread)
      return NULL;

   struct thread_data *data = (struct thread_data*)calloc(1, sizeof(*data));
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

slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)calloc(1, sizeof(*lock));
   if (!lock)
      return NULL;

#ifdef _WIN32
   lock->lock = CreateMutex(NULL, FALSE, "RetroArchMutex");
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

void slock_lock(slock_t *lock)
{
#ifdef _WIN32
   WaitForSingleObject(lock->lock, INFINITE);
#else
   pthread_mutex_lock(&lock->lock);
#endif
}

void slock_unlock(slock_t *lock)
{
#ifdef _WIN32
   ReleaseMutex(lock->lock);
#else
   pthread_mutex_unlock(&lock->lock);
#endif
}

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

void scond_wait(scond_t *cond, slock_t *lock)
{
#ifdef _WIN32
   WaitForSingleObject(cond->event, 0);
   
#if MSC_VER <= 1310
   slock_unlock(lock);
   WaitForSingleObject(cond->event, INFINITE);
#else
   SignalObjectAndWait(lock->lock, cond->event, INFINITE, FALSE);
#endif
   slock_lock(lock);
#else
   pthread_cond_wait(&cond->cond, &lock->lock);
#endif
}

/* FIXME _WIN32 - check how this function should differ 
 * from scond_signal implementation. */

int scond_broadcast(scond_t *cond)
{
#ifdef _WIN32
   SetEvent(cond->event);
   return 0;
#else
   return pthread_cond_broadcast(&cond->cond);
#endif
}

void scond_signal(scond_t *cond)
{
#ifdef _WIN32
   SetEvent(cond->event);
#else
   pthread_cond_signal(&cond->cond);
#endif
}

bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us)
{
#ifdef _WIN32
   DWORD ret;

   WaitForSingleObject(cond->event, 0);
#if MSC_VER <= 1310
   slock_unlock(lock);
   ret = WaitForSingleObject(cond->event, (DWORD)(timeout_us) / 1000);
#else
   ret = SignalObjectAndWait(lock->lock, cond->event,
         (DWORD)(timeout_us) / 1000, FALSE);
#endif

   slock_lock(lock);
   return ret == WAIT_OBJECT_0;
#else
   int ret;
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

   now.tv_sec += timeout_us / 1000000LL;
   now.tv_nsec += timeout_us * 1000LL;

   now.tv_sec += now.tv_nsec / 1000000000LL;
   now.tv_nsec = now.tv_nsec % 1000000000LL;

   ret = pthread_cond_timedwait(&cond->cond, &lock->lock, &now);
   return (ret == 0);
#endif
}
