#pragma once
#include <wiiu/types.h>
#include "time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   uint32_t owner;
   uint32_t __unknown0;
   uint32_t recursion;
   uint32_t __unknown1;
} OSSpinLock;

void OSInitSpinLock(OSSpinLock *spinlock);
BOOL OSAcquireSpinLock(OSSpinLock *spinlock);
BOOL OSTryAcquireSpinLock(OSSpinLock *spinlock);
BOOL OSTryAcquireSpinLockWithTimeout(OSSpinLock *spinlock, OSTime timeout);
BOOL OSReleaseSpinLock(OSSpinLock *spinlock);
BOOL OSUninterruptibleSpinLock_Acquire(OSSpinLock *spinlock);
BOOL OSUninterruptibleSpinLock_TryAcquire(OSSpinLock *spinlock);
BOOL OSUninterruptibleSpinLock_TryAcquireWithTimeout(OSSpinLock *spinlock, OSTime timeout);
BOOL OSUninterruptibleSpinLock_Release(OSSpinLock *spinlock);

#ifdef __cplusplus
}
#endif

/** @} */
