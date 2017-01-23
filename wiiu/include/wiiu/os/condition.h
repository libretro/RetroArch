#pragma once
#include <wiiu/types.h>
#include "mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_CONDITION_TAG 0x634E6456u

typedef struct OSCondition
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown;
   OSThreadQueue queue;
}OSCondition;

void OSInitCond(OSCondition *condition);
void OSInitCondEx(OSCondition *condition, const char *name);
void OSWaitCond(OSCondition *condition, OSMutex *mutex);
void OSSignalCond(OSCondition *condition);

#ifdef __cplusplus
}
#endif
