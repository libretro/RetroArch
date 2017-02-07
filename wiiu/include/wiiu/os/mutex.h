#pragma once
#include <wiiu/types.h>
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSMutex OSMutex;

typedef struct
{
   OSMutex *next;
   OSMutex *prev;
} OSMutexLink;

#define OS_MUTEX_TAG 0x6D557458u
typedef struct OSMutex
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown;
   OSThreadQueue queue;
   OSThread *owner;
   int32_t count;
   OSMutexLink link;
} OSMutex;

void OSInitMutex(OSMutex *mutex);
void OSInitMutexEx(OSMutex *mutex, const char *name);
void OSLockMutex(OSMutex *mutex);
BOOL OSTryLockMutex(OSMutex *mutex);
void OSUnlockMutex(OSMutex *mutex);

#ifdef __cplusplus
}
#endif

/** @} */
