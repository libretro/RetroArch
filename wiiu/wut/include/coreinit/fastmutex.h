#pragma once
#include <wut.h>
#include "threadqueue.h"

/**
 * \defgroup coreinit_fastmutex Fast Mutex
 * \ingroup coreinit
 *
 * Similar to OSMutex but tries to acquire the mutex without using the global
 * scheduler lock, and does not test for thread cancel.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSFastMutex OSFastMutex;
typedef struct OSFastMutexLink OSFastMutexLink;
typedef struct OSFastCondition OSFastCondition;

struct OSFastMutexLink
{
   OSFastMutex *next;
   OSFastMutex *prev;
};
CHECK_OFFSET(OSFastMutexLink, 0x00, next);
CHECK_OFFSET(OSFastMutexLink, 0x04, prev);
CHECK_SIZE(OSFastMutexLink, 0x08);

#define OS_FAST_MUTEX_TAG 0x664D7458u

struct OSFastMutex
{
   uint32_t tag;
   const char *name;
   UNKNOWN(4);
   OSThreadSimpleQueue queue;
   OSFastMutexLink link;
   UNKNOWN(16);
};
CHECK_OFFSET(OSFastMutex, 0x00, tag);
CHECK_OFFSET(OSFastMutex, 0x04, name);
CHECK_OFFSET(OSFastMutex, 0x0c, queue);
CHECK_OFFSET(OSFastMutex, 0x14, link);
CHECK_SIZE(OSFastMutex, 0x2c);

void
OSFastMutex_Init(OSFastMutex *mutex,
                 const char *name);

void
OSFastMutex_Lock(OSFastMutex *mutex);

void
OSFastMutex_Unlock(OSFastMutex *mutex);

BOOL
OSFastMutex_TryLock(OSFastMutex *mutex);

#ifdef __cplusplus
}
#endif

/** @} */
