#pragma once
#include <wiiu/types.h>
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_SEMAPHORE_TAG 0x73506852u
typedef struct
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown;
   int32_t count;
   OSThreadQueue queue;
}OSSemaphore;

void OSInitSemaphore(OSSemaphore *semaphore, int32_t count);
void OSInitSemaphoreEx(OSSemaphore *semaphore, int32_t count, const char *name);
int32_t OSGetSemaphoreCount(OSSemaphore *semaphore);
int32_t OSSignalSemaphore(OSSemaphore *semaphore);
int32_t OSWaitSemaphore(OSSemaphore *semaphore);
int32_t OSTryWaitSemaphore(OSSemaphore *semaphore);

#ifdef __cplusplus
}
#endif
