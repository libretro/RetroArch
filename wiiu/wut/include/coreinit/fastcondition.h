#pragma once
#include <wut.h>
#include "threadqueue.h"

/**
 * \defgroup coreinit_fastcond Fast Condition Variable
 * \ingroup coreinit
 *
 * A condition variable to be used with an OSFastMutex.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSFastCondition OSFastCondition;
typedef struct OSFastMutex OSFastMutex;

#define OS_FAST_CONDITION_TAG 0x664E6456u

struct OSFastCondition
{
   uint32_t tag;
   const char *name;
   UNKNOWN(4);
   OSThreadQueue queue;
};
CHECK_OFFSET(OSFastCondition, 0x00, tag);
CHECK_OFFSET(OSFastCondition, 0x04, name);
CHECK_OFFSET(OSFastCondition, 0x0c, queue);
CHECK_SIZE(OSFastCondition, 0x1c);

void
OSFastCond_Init(OSFastCondition *condition,
                const char *name);

void
OSFastCond_Wait(OSFastCondition *condition,
                OSFastMutex *mutex);

void
OSFastCond_Signal(OSFastCondition *condition);

#ifdef __cplusplus
}
#endif

/** @} */
