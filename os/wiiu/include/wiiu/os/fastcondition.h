#pragma once
#include <wiiu/types.h>
#include "fastmutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_FAST_CONDITION_TAG 0x664E6456u

typedef struct OSFastCondition
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown;
   OSThreadQueue queue;
}OSFastCondition;

void OSFastCond_Init(OSFastCondition *condition, const char *name);
void OSFastCond_Wait(OSFastCondition *condition, OSFastMutex *mutex);
void OSFastCond_Signal(OSFastCondition *condition);

#ifdef __cplusplus
}
#endif
