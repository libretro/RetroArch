#pragma once
#include <wut.h>
#include "threadqueue.h"

/**
 * \defgroup coreinit_mutex Mutex
 * \ingroup coreinit
 *
 * Standard mutex object, supports recursive locking.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/recursive_mutex">std::recursive_mutex</a>.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSThread OSThread;

typedef struct OSMutex OSMutex;
typedef struct OSMutexLink OSMutexLink;

struct OSMutexLink
{
   OSMutex *next;
   OSMutex *prev;
};
CHECK_OFFSET(OSMutexLink, 0x00, next);
CHECK_OFFSET(OSMutexLink, 0x04, prev);
CHECK_SIZE(OSMutexLink, 0x8);

#define OS_MUTEX_TAG 0x6D557458u

struct OSMutex
{
   //! Should always be set to the value OS_MUTEX_TAG.
   uint32_t tag;

   //! Name set by OSInitMutexEx.
   const char *name;

   UNKNOWN(4);

   //! Queue of threads waiting for this mutex to unlock.
   OSThreadQueue queue;

   //! Current owner of mutex.
   OSThread *owner;

   //! Current recursion lock count of mutex.
   int32_t count;

   //! Link used inside OSThread's mutex queue.
   OSMutexLink link;
};
CHECK_OFFSET(OSMutex, 0x00, tag);
CHECK_OFFSET(OSMutex, 0x04, name);
CHECK_OFFSET(OSMutex, 0x0c, queue);
CHECK_OFFSET(OSMutex, 0x1c, owner);
CHECK_OFFSET(OSMutex, 0x20, count);
CHECK_OFFSET(OSMutex, 0x24, link);
CHECK_SIZE(OSMutex, 0x2c);


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
