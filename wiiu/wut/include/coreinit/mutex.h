#pragma once
#include <wut.h>
#include "threadqueue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSThread OSThread;

typedef struct OSMutex OSMutex;
typedef struct OSMutexLink
{
   OSMutex *next;
   OSMutex *prev;
}OSMutexLink;

#define OS_MUTEX_TAG 0x6D557458u

typedef struct OSMutex
{
   //! Should always be set to the value OS_MUTEX_TAG.
   uint32_t tag;

   //! Name set by OSInitMutexEx.
   const char *name;

   uint32_t __unknown;

   //! Queue of threads waiting for this mutex to unlock.
   OSThreadQueue queue;

   //! Current owner of mutex.
   OSThread *owner;

   //! Current recursion lock count of mutex.
   int32_t count;

   //! Link used inside OSThread's mutex queue.
   OSMutexLink link;
}OSMutex;


/**
 * Initialise a mutex structure.
 */
void
OSInitMutex(OSMutex *mutex);


/**
 * Initialise a mutex structure with a name.
 */
void
OSInitMutexEx(OSMutex *mutex,
              const char *name);


/**
 * Lock the mutex.
 *
 * If no one owns the mutex, set current thread as owner.
 *
 * If the lock is owned by the current thread, increase the recursion count.
 *
 * If the lock is owned by another thread, the current thread will sleep until
 * the owner has unlocked this mutex.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/recursive_mutex/lock">std::recursive_mutex::lock</a>.
 */
void
OSLockMutex(OSMutex *mutex);


/**
 * Try to lock a mutex.
 *
 * If no one owns the mutex, set current thread as owner.
 *
 * If the lock is owned by the current thread, increase the recursion count.
 *
 * If the lock is owned by another thread, do not block, return FALSE.
 *
 * \return TRUE if the mutex is locked, FALSE if the mutex is owned by another thread.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/recursive_mutex/try_lock">std::recursive_mutex::try_lock</a>.
 */
BOOL
OSTryLockMutex(OSMutex *mutex);


/**
 * Unlocks the mutex.
 *
 * Will decrease the recursion count, will only unlock the mutex when the
 * recursion count reaches 0.
 *
 * If any other threads are waiting to lock the mutex they will be woken.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/recursive_mutex/unlock">std::recursive_mutex::unlock</a>.
 */
void
OSUnlockMutex(OSMutex *mutex);


#ifdef __cplusplus
}
#endif

/** @} */
