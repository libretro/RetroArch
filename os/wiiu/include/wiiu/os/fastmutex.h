#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSFastMutex OSFastMutex;
typedef struct
{
   OSFastMutex *next;
   OSFastMutex *prev;
}OSFastMutexLink;

#define OS_FAST_MUTEX_TAG 0x664D7458u
typedef struct OSFastMutex
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown0;
   OSThreadSimpleQueue queue;
   OSFastMutexLink link;
   uint32_t __unknown1[4];
}OSFastMutex;

void OSFastMutex_Init(OSFastMutex *mutex, const char *name);
void OSFastMutex_Lock(OSFastMutex *mutex);
void OSFastMutex_Unlock(OSFastMutex *mutex);
BOOL OSFastMutex_TryLock(OSFastMutex *mutex);

#ifdef __cplusplus
}
#endif
