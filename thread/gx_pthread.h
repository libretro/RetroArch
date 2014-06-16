/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GX_PTHREAD_WRAP_GX_
#define _GX_PTHREAD_WRAP_GX_

#include <ogcsys.h>
#include <gccore.h>
#include <ogc/cond.h>

#ifndef OSThread
#define OSThread lwp_t
#endif

#ifndef OSCond
#define OSCond lwpq_t
#endif

#ifndef OSThreadQueue
#define OSThreadQueue lwpq_t
#endif

#ifndef OSInitMutex
#define OSInitMutex(mutex) LWP_MutexInit(mutex, 0)
#endif

#ifndef OSLockMutex
#define OSLockMutex(mutex) LWP_MutexLock(mutex)
#endif

#ifndef OSUnlockMutex
#define OSUnlockMutex(mutex) LWP_MutexUnlock(mutex)
#endif

#ifndef OSTryLockMutex
#define OSTryLockMutex(mutex) LWP_MutexTryLock(mutex)
#endif

#ifndef OSInitCond
#define OSInitCond(cond) LWP_CondInit(cond)
#endif

#ifndef OSSignalCond
#define OSSignalCond(cond) LWP_ThreadSignal(cond)
#endif

#ifndef OSWaitCond
#define OSWaitCond(cond, mutex) LWP_CondWait(cond, mutex)
#endif

#ifndef OSInitThreadQueue
#define OSInitThreadQueue(queue) LWP_InitQueue(queue)
#endif

#ifndef OSSleepThread
#define OSSleepThread(queue) LWP_ThreadSleep(queue)
#endif

#ifndef OSJoinThread
#define OSJoinThread(thread, val) LWP_JoinThread(thread, val)
#endif

#ifndef OSCreateThread
#define OSCreateThread(thread, func, intarg, ptrarg, stackbase, stacksize, priority, attrs) LWP_CreateThread(thread, func, ptrarg, stackbase, stacksize, priority)
#endif

#define STACKSIZE (8 * 1024)

typedef OSThread pthread_t;
typedef mutex_t pthread_mutex_t;
typedef void* pthread_mutexattr_t;
typedef int pthread_attr_t;
typedef cond_t pthread_cond_t;
typedef cond_t pthread_condattr_t;

static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{
   *thread = 0;
   return OSCreateThread(thread, start_routine, 0 /* unused */, arg, 0, STACKSIZE, 64, 0 /* unused */);
}

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
   return OSInitMutex(mutex);
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
   return LWP_MutexDestroy(*mutex);
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex)
{
   return OSLockMutex(*mutex);
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
   return OSUnlockMutex(*mutex);
}

static inline void pthread_exit(void *retval)
{
   /* FIXME: No LWP equivalent for this? */
   (void)retval;
}

static inline int pthread_detach(pthread_t thread)
{
   /* FIXME: pthread_detach equivalent missing? */
   (void)thread;
   return 0;
}

static inline int pthread_join(pthread_t thread, void **retval)
{
   return OSJoinThread(thread, retval);
}

static inline int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
   return OSTryLockMutex(*mutex);
}

static inline int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
   return OSWaitCond(*cond, *mutex);
}

static inline int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
   return LWP_CondTimedWait(*cond, *mutex, abstime);
}

static inline int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
   return OSInitCond(cond);
}

static inline int pthread_cond_signal(pthread_cond_t *cond)
{
   return LWP_CondSignal(*cond);
}

static inline int pthread_cond_broadcast(pthread_cond_t *cond)
{
   return LWP_CondBroadcast(*cond);
}

static inline int pthread_cond_destroy(pthread_cond_t *cond)
{
   return LWP_CondDestroy(*cond);
}

#endif
