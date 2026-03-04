/* Copyright  (C) 2010-2020 The RetroArch team
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

#ifdef __unix__
#ifndef __sun__
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309
#endif
#endif
#endif

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>

/* with RETRO_WIN32_USE_PTHREADS, pthreads can be used even on win32. Maybe only supported in MSVC>=2005  */

#if defined(_WIN32) && !defined(RETRO_WIN32_USE_PTHREADS)
#define USE_WIN32_THREADS
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 /*_WIN32_WINNT_WIN2K */
#endif
#include <windows.h>
#include <mmsystem.h>
#endif
#elif defined(GEKKO)
#include <ogc/lwp_watchdog.h>
#include "gx_pthread.h"
#elif defined(_3DS)
#include "ctr_pthread.h"
#else
#include <pthread.h>
#include <time.h>
#endif

#if defined(VITA) || defined(BSD) || defined(ORBIS) || defined(__mips__) || defined(_3DS)
#include <sys/time.h>
#endif

#if defined(PS2)
#include <ps2sdkapi.h>
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
#ifdef USE_WIN32_THREADS
   HANDLE thread;
   DWORD  id;
#else
   pthread_t id;
#endif
};

struct slock
{
#ifdef USE_WIN32_THREADS
   CRITICAL_SECTION lock;
#else
   pthread_mutex_t lock;
#endif
};

#ifdef USE_WIN32_THREADS
/* Linked-list queue entry for waiting threads */
struct queue_entry
{
   struct queue_entry *next;
};
#endif

struct scond
{
#ifdef USE_WIN32_THREADS
   /* See _scond_wait_win32 for the algorithm overview. */
   HANDLE            hot_potato; /* passed among waiters to serialize queue progress   */
   HANDLE            event;      /* set when a genuine wakeup is available              */
   struct queue_entry *head;     /* front of the FIFO waiter queue; NULL when empty     */
   int               waiters;    /* number of threads currently waiting                 */
   int               wakens;     /* pending wakeups not yet consumed                    */
   CRITICAL_SECTION  cs;         /* guards this scond's internal state                  */
#else
   pthread_cond_t cond;
#endif
};

/* ---------------------------------------------------------------------------
 * thread_wrap
 * Trampoline executed inside the new OS thread. Calls the user function then
 * frees the heap-allocated thread_data so the caller doesn't have to track it.
 * --------------------------------------------------------------------------- */
#ifdef USE_WIN32_THREADS
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

sthread_t *sthread_create(void (*thread_func)(void*), void *userdata)
{
   return sthread_create_with_priority(thread_func, userdata, 0);
}

/* TODO/FIXME - this needs to be implemented for Switch/3DS */
#if !defined(SWITCH) && !defined(USE_WIN32_THREADS) && !defined(_3DS) && !defined(GEKKO) && !defined(__HAIKU__) && !defined(EMSCRIPTEN)
#define HAVE_THREAD_ATTR
#endif

sthread_t *sthread_create_with_priority(void (*thread_func)(void*), void *userdata, int thread_priority)
{
#ifdef HAVE_THREAD_ATTR
   pthread_attr_t thread_attr;
   bool           thread_attr_needed = false;
#endif
   bool             thread_created   = false;
   struct thread_data *data          = NULL;
   sthread_t          *thread        = (sthread_t*)malloc(sizeof(*thread));

   if (!thread)
      return NULL;

   data = (struct thread_data*)malloc(sizeof(*data));
   if (!data)
   {
      free(thread);
      return NULL;
   }

   data->func     = thread_func;
   data->userdata = userdata;
   thread->id     = 0;

#ifdef USE_WIN32_THREADS
   /* CreateThread with default security/stack; id written to thread->id */
   thread->thread = CreateThread(NULL, 0, thread_wrap, data, 0, &thread->id);
   thread_created = !!thread->thread;

#else  /* pthreads path */

#ifdef HAVE_THREAD_ATTR
   pthread_attr_init(&thread_attr);

   if ((thread_priority >= 1) && (thread_priority <= 100))
   {
      struct sched_param sp;
      memset(&sp, 0, sizeof(sp));
      sp.sched_priority = thread_priority;
      pthread_attr_setschedpolicy(&thread_attr, SCHED_RR);
      pthread_attr_setschedparam(&thread_attr, &sp);
      thread_attr_needed = true;
   }

#if defined(VITA)
   pthread_attr_setstacksize(&thread_attr, 0x10000);
   thread_attr_needed = true;
#elif defined(__APPLE__)
   /* Default Apple stack is 512 KB; we want 2 MB for disc-scanning etc. */
   pthread_attr_setstacksize(&thread_attr, 0x200000);
   thread_attr_needed = true;
#endif

   thread_created = pthread_create(
         &thread->id,
         thread_attr_needed ? &thread_attr : NULL,
         thread_wrap,
         data) == 0;

   pthread_attr_destroy(&thread_attr);

#else  /* !HAVE_THREAD_ATTR */
   thread_created = pthread_create(&thread->id, NULL, thread_wrap, data) == 0;
#endif /* HAVE_THREAD_ATTR */

#endif /* USE_WIN32_THREADS */

   if (thread_created)
      return thread;

   free(data);
   free(thread);
   return NULL;
}

int sthread_detach(sthread_t *thread)
{
#ifdef USE_WIN32_THREADS
   CloseHandle(thread->thread);
   free(thread);
   return 0;
#else
   int ret = pthread_detach(thread->id);
   free(thread);
   return ret;
#endif
}

void sthread_join(sthread_t *thread)
{
   if (!thread)
      return;
#ifdef USE_WIN32_THREADS
   WaitForSingleObject(thread->thread, INFINITE);
   CloseHandle(thread->thread);
#else
   pthread_join(thread->id, NULL);
#endif
   free(thread);
}

#if !defined(GEKKO)
bool sthread_isself(sthread_t *thread)
{
#ifdef USE_WIN32_THREADS
   return thread ? (GetCurrentThreadId() == thread->id)          : false;
#else
   return thread ? (pthread_equal(pthread_self(), thread->id) != 0) : false;
#endif
}
#endif

/* ---------------------------------------------------------------------------
 * Mutex (slock)
 * ---------------------------------------------------------------------------
 * On Win32 we use CRITICAL_SECTION which is faster than a kernel mutex for
 * uncontended cases (it spins briefly before entering the kernel).
 * On pthreads we initialise with NULL attrs so the implementation picks the
 * fastest default (usually a futex on Linux).
 * --------------------------------------------------------------------------- */

slock_t *slock_new(void)
{
   /* calloc zeroes the memory, satisfying any platform that requires a
    * zero-initialised mutex/critical-section structure before init. */
   slock_t *lock = (slock_t*)calloc(1, sizeof(*lock));
   if (!lock)
      return NULL;

#ifdef USE_WIN32_THREADS
   /* Use spin-count to reduce kernel transitions under brief contention.
    * 1500 spins is a commonly recommended value for short critical sections. */
   if (!InitializeCriticalSectionAndSpinCount(&lock->lock, 1500))
   {
      free(lock);
      return NULL;
   }
#else
   if (pthread_mutex_init(&lock->lock, NULL) != 0)
   {
      free(lock);
      return NULL;
   }
#endif
   return lock;
}

void slock_free(slock_t *lock)
{
   if (!lock)
      return;
#ifdef USE_WIN32_THREADS
   DeleteCriticalSection(&lock->lock);
#else
   pthread_mutex_destroy(&lock->lock);
#endif
   free(lock);
}

void slock_lock(slock_t *lock)
{
   if (!lock)
      return;
#ifdef USE_WIN32_THREADS
   EnterCriticalSection(&lock->lock);
#else
   pthread_mutex_lock(&lock->lock);
#endif
}

bool slock_try_lock(slock_t *lock)
{
#ifdef USE_WIN32_THREADS
   return lock && TryEnterCriticalSection(&lock->lock);
#else
   return lock && (pthread_mutex_trylock(&lock->lock) == 0);
#endif
}

void slock_unlock(slock_t *lock)
{
   if (!lock)
      return;
#ifdef USE_WIN32_THREADS
   LeaveCriticalSection(&lock->lock);
#else
   pthread_mutex_unlock(&lock->lock);
#endif
}

/* ---------------------------------------------------------------------------
 * Condition variable (scond)
 * --------------------------------------------------------------------------- */

scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)calloc(1, sizeof(*cond));
   if (!cond)
      return NULL;

#ifdef USE_WIN32_THREADS
   /* This is very complex because recreating condition variable semantics
    * with Win32 parts is not easy.
    *
    * The main problem is that a condition variable can't be used to
    * "pre-wake" a thread (it will get wakened only after it's waited).
    *
    * Whereas a win32 event can pre-wake a thread (the event will be set
    * in advance, so a 'waiter' won't even have to wait on it).
    *
    * Keep in mind a condition variable can apparently pre-wake a thread,
    * insofar as spurious wakeups are always possible,
    * but nobody will be expecting this and it does not need to be simulated.
    *
    * Moreover, we won't be doing this, because it counts as a spurious wakeup
    * -- someone else with a genuine claim must get wakened, in any case.
    *
    * Therefore we choose to wake only one of the correct waiting threads.
    * So at the very least, we need to do something clever. But there's
    * bigger problems.
    * We don't even have a straightforward way in win32 to satisfy
    * pthread_cond_wait's atomicity requirement. The bulk of this
    * algorithm is solving that.
    *
    * Note: We might could simplify this using vista+ condition variables,
    * but we wanted an XP compatible solution. */
   cond->event = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!cond->event)
   {
      free(cond);
      return NULL;
   }
   cond->hot_potato = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!cond->hot_potato)
   {
      CloseHandle(cond->event);
      free(cond);
      return NULL;
   }

   /* Spin briefly before entering the kernel for uncontended access. */
   if (!InitializeCriticalSectionAndSpinCount(&cond->cs, 1500))
   {
      CloseHandle(cond->hot_potato);
      CloseHandle(cond->event);
      free(cond);
      return NULL;
   }
#else
   if (pthread_cond_init(&cond->cond, NULL) != 0)
   {
      free(cond);
      return NULL;
   }
#endif

   return cond;
}

void scond_free(scond_t *cond)
{
   if (!cond)
      return;
#ifdef USE_WIN32_THREADS
   CloseHandle(cond->event);
   CloseHandle(cond->hot_potato);
   DeleteCriticalSection(&cond->cs);
#else
   pthread_cond_destroy(&cond->cond);
#endif
   free(cond);
}

/* ---------------------------------------------------------------------------
 * _scond_wait_win32  (Win32 only)
 *
 * Implements condition-wait semantics on top of Win32 events using a FIFO
 * queue.  The "hot potato" event serialises queue progress: it is passed from
 * thread to thread until the head of the queue holds it and may claim the
 * main event.
 *
 * Performance notes vs. the original:
 *  - The spin-count on the CRITICAL_SECTIONs (set in slock_new/scond_new)
 *    reduces kernel transitions for brief contention.
 *  - The QPC-based elapsed-time computation has been refactored to avoid a
 *    redundant multiply-then-divide when INFINITE is requested.
 *  - performanceCounterFrequency is queried once and cached; the double-check
 *    (QuadPart == 0) guard is retained for correctness on pathological HW.
 * --------------------------------------------------------------------------- */
#ifdef USE_WIN32_THREADS
static bool _scond_wait_win32(scond_t *cond, slock_t *lock, DWORD dwMilliseconds)
{
   struct queue_entry  myentry;
   struct queue_entry **ptr;
   DWORD               waitResult;
   DWORD               dwFinalTimeout = dwMilliseconds;

#if _WIN32_WINNT >= 0x0500 || defined(_XBOX)
   /* Cache the QPC frequency (queried once per process lifetime). */
   static LARGE_INTEGER performanceCounterFrequency;
   static bool          freq_initialised = false;
   LARGE_INTEGER        tsBegin;

   if (!freq_initialised)
   {
      /* Zero-init so the QuadPart == 0 guard below triggers on first call */
      performanceCounterFrequency.QuadPart = 0;
      freq_initialised = true;
   }
   if (performanceCounterFrequency.QuadPart == 0)
      QueryPerformanceFrequency(&performanceCounterFrequency);
#else
   static bool beginPeriod = false;
   DWORD       tsBegin;
#endif

   /* Reminder: `lock` is held by the caller before this is invoked. */
   /* However, scond_signal may be called without holding `lock`, so
    * protect the condvar's internal state with its own critical section. */
   EnterCriticalSection(&cond->cs);

#if _WIN32_WINNT >= 0x0500 || defined(_XBOX)
   /* Snapshot the current time only when we actually need a deadline. */
   if (dwMilliseconds != INFINITE)
      QueryPerformanceCounter(&tsBegin);
#else
   if (!beginPeriod)
   {
      beginPeriod = true;
      timeBeginPeriod(1);
   }
   if (dwMilliseconds != INFINITE)
      tsBegin = timeGetTime();
#endif

   /* Enqueue ourselves at the tail of the waiter list. */
   ptr = &cond->head;
   while (*ptr)
      ptr = &(*ptr)->next;

   myentry.next = NULL;
   *ptr         = &myentry;
   cond->waiters++;

   /* Wait for our turn at the head of the queue.
    *
    * Invariant: only the head-of-queue thread may wait on cond->event.
    * Everyone else waits on the hot_potato and yields until they move
    * to the front. */
   while (cond->head != &myentry)
   {
      DWORD timeout = INFINITE;

      /* Keep the potato moving as long as there are pending wakeups. */
      if (cond->wakens > 0)
         SetEvent(cond->hot_potato);

      /* Compute remaining budget (skip arithmetic when INFINITE). */
      if (dwMilliseconds != INFINITE)
      {
#if _WIN32_WINNT >= 0x0500 || defined(_XBOX)
         LARGE_INTEGER now;
         LONGLONG      elapsed_ms;

         QueryPerformanceCounter(&now);
         elapsed_ms  = (now.QuadPart - tsBegin.QuadPart) * 1000;
         elapsed_ms /= performanceCounterFrequency.QuadPart;

         if (elapsed_ms >= (LONGLONG)dwMilliseconds)
            elapsed_ms  = (LONGLONG)dwMilliseconds; /* clamp */

         timeout = (DWORD)(dwMilliseconds - elapsed_ms);
#else
         DWORD now     = timeGetTime();
         DWORD elapsed = now - tsBegin;

         if (elapsed >= dwMilliseconds)
            elapsed = dwMilliseconds;

         timeout = dwMilliseconds - elapsed;
#endif
      }

      /* Yield both locks while waiting, then re-acquire them afterward. */
      LeaveCriticalSection(&lock->lock);
      LeaveCriticalSection(&cond->cs);

      /* Brief voluntary yield before blocking so the OS can schedule the
       * thread that actually holds the potato right now. */
      Sleep(0);
      waitResult = WaitForSingleObject(cond->hot_potato, timeout);

      EnterCriticalSection(&lock->lock);
      EnterCriticalSection(&cond->cs);

      if (waitResult == WAIT_TIMEOUT)
      {
         if (cond->head == &myentry)
         {
            /* We're at the head and timed out: one last try with 0 ms. */
            dwFinalTimeout = 0;
            break;
         }
         else
         {
            /* Not at the head and out of time: remove ourselves and bail. */
            struct queue_entry *curr = cond->head;
            while (curr->next != &myentry)
               curr = curr->next;
            curr->next = myentry.next;
            cond->waiters--;
            LeaveCriticalSection(&cond->cs);
            return false;
         }
      }
   }

   /* We are now at the head of the queue and hold the potato. */

   /* Release both locks so the signalling thread can set cond->event. */
   LeaveCriticalSection(&lock->lock);
   LeaveCriticalSection(&cond->cs);

   /* Wait for the actual signal. Only the head-of-queue thread waits here. */
   waitResult = WaitForSingleObject(cond->event, dwFinalTimeout);

   /* Re-acquire locks before mutating shared state. */
   EnterCriticalSection(&lock->lock);
   EnterCriticalSection(&cond->cs);

   /* Dequeue ourselves. */
   cond->head = myentry.next;
   cond->waiters--;

   if (waitResult == WAIT_TIMEOUT)
   {
      LeaveCriticalSection(&cond->cs);
      return false;
   }

   /* Propagate any remaining pending wakeups to the next waiter. */
   cond->wakens--;
   if (cond->wakens > 0)
   {
      SetEvent(cond->event);
      SetEvent(cond->hot_potato);
   }

   LeaveCriticalSection(&cond->cs);
   return true;
}
#endif /* USE_WIN32_THREADS */

void scond_wait(scond_t *cond, slock_t *lock)
{
#ifdef USE_WIN32_THREADS
   _scond_wait_win32(cond, lock, INFINITE);
#else
   pthread_cond_wait(&cond->cond, &lock->lock);
#endif
}

int scond_broadcast(scond_t *cond)
{
#ifdef USE_WIN32_THREADS
   /* Caller must hold the associated mutex. */
   if (cond->waiters != 0)
   {
      if (cond->wakens == 0)
         SetEvent(cond->event);
      cond->wakens = cond->waiters;
      SetEvent(cond->hot_potato);
   }
   return 0;
#else
   return pthread_cond_broadcast(&cond->cond);
#endif
}

void scond_signal(scond_t *cond)
{
#ifdef USE_WIN32_THREADS
   /* scond_signal may be called without holding the associated mutex,
    * so guard the condvar's own state with its internal critical section. */
   EnterCriticalSection(&cond->cs);

   if (cond->waiters == 0)
   {
      LeaveCriticalSection(&cond->cs);
      return;
   }

   if (cond->wakens == 0)
      SetEvent(cond->event);

   cond->wakens++;

   /* Release cs before SetEvent so that a woken thread doesn't stall
    * immediately trying to re-acquire it. */
   LeaveCriticalSection(&cond->cs);
   SetEvent(cond->hot_potato);

#else
   pthread_cond_signal(&cond->cond);
#endif
}

bool scond_wait_timeout(scond_t *cond, slock_t *lock, int64_t timeout_us)
{
#ifdef USE_WIN32_THREADS
   /* Convert microseconds to milliseconds, handling edge cases:
    *   0 us  -> always time out immediately (matches pthreads semantics).
    *   1-999 us -> round up to 1 ms so we don't accidentally return at once.
    *   >= 1000 us -> truncate to ms (slight under-wait is acceptable). */
   if (timeout_us == 0)
      return false;
   if (timeout_us < 1000)
      return _scond_wait_win32(cond, lock, 1);
   return _scond_wait_win32(cond, lock, (DWORD)(timeout_us / 1000));
#else
   int64_t        seconds;
   int64_t        remainder;
   struct timespec now;

#ifdef __MACH__
   /* macOS does not expose clock_gettime before 10.12; use Mach clock. */
   clock_serv_t    cclock;
   mach_timespec_t mts;
   host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
   clock_get_time(cclock, &mts);
   mach_port_deallocate(mach_task_self(), cclock);
   now.tv_sec  = mts.tv_sec;
   now.tv_nsec = mts.tv_nsec;
#elif !defined(__PSL1GHT__) && defined(__PS3__)
   sys_time_sec_t  s;
   sys_time_nsec_t n;
   sys_time_get_current_time(&s, &n);
   now.tv_sec  = s;
   now.tv_nsec = n;
#elif defined(PS2)
   {
      int tickms   = ps2_clock();
      now.tv_sec   = tickms / 1000;
      now.tv_nsec  = (tickms % 1000) * 1000000; /* correct ms -> ns */
   }
#elif !defined(DINGUX_BETA) && (defined(__mips__) || defined(VITA) || defined(_3DS))
   {
      struct timeval tm;
      gettimeofday(&tm, NULL);
      now.tv_sec  = tm.tv_sec;
      now.tv_nsec = tm.tv_usec * 1000;
   }
#elif defined(RETRO_WIN32_USE_PTHREADS)
   _ftime64_s(&now);
#elif defined(GEKKO)
   {
      /* gettimeofday is reportedly broken on Wii; use the hardware timer. */
      const uint64_t tickms = gettime() / TB_TIMER_CLOCK;
      now.tv_sec  = (time_t)(tickms / 1000);
      now.tv_nsec = (long)((tickms % 1000) * 1000000); /* correct ms -> ns */
   }
#else
   clock_gettime(CLOCK_REALTIME, &now);
#endif

   seconds   = timeout_us / INT64_C(1000000);
   remainder = timeout_us % INT64_C(1000000);

   now.tv_sec  += (time_t)seconds;
   now.tv_nsec += (long)(remainder * INT64_C(1000));

   /* Normalise: tv_nsec must stay in [0, 999999999]. */
   if (now.tv_nsec >= 1000000000L)
   {
      now.tv_nsec -= 1000000000L;
      now.tv_sec  += 1;
   }

   return (pthread_cond_timedwait(&cond->cond, &lock->lock, &now) == 0);
#endif
}

/* ---------------------------------------------------------------------------
 * Thread-local storage  (optional, gated by HAVE_THREAD_STORAGE)
 * --------------------------------------------------------------------------- */
#ifdef HAVE_THREAD_STORAGE
bool sthread_tls_create(sthread_tls_t *tls)
{
#ifdef USE_WIN32_THREADS
   return (*tls = TlsAlloc()) != TLS_OUT_OF_INDEXES;
#else
   return pthread_key_create((pthread_key_t*)tls, NULL) == 0;
#endif
}

bool sthread_tls_delete(sthread_tls_t *tls)
{
#ifdef USE_WIN32_THREADS
   return TlsFree(*tls) != 0;
#else
   /* TODO/FIXME - broken for UCRT */
   return pthread_key_delete(*tls) == 0;
#endif
}

void *sthread_tls_get(sthread_tls_t *tls)
{
#ifdef USE_WIN32_THREADS
   return TlsGetValue(*tls);
#else
   /* TODO/FIXME - broken for UCRT */
   return pthread_getspecific(*tls);
#endif
}

bool sthread_tls_set(sthread_tls_t *tls, const void *data)
{
#ifdef USE_WIN32_THREADS
   return TlsSetValue(*tls, (void*)data) != 0;
#else
   /* TODO/FIXME - broken for UCRT */
   return pthread_setspecific(*tls, data) == 0;
#endif
}
#endif /* HAVE_THREAD_STORAGE */

/* ---------------------------------------------------------------------------
 * Thread ID helpers
 * --------------------------------------------------------------------------- */

uintptr_t sthread_get_thread_id(sthread_t *thread)
{
   if (!thread)
      return 0;
   return (uintptr_t)thread->id;
}

uintptr_t sthread_get_current_thread_id(void)
{
#ifdef USE_WIN32_THREADS
   return (uintptr_t)GetCurrentThreadId();
#else
   return (uintptr_t)pthread_self();
#endif
}
