#pragma once
#include <wut.h>
#include "threadqueue.h"

/**
 * \defgroup coreinit_cond Condition Variable
 * \ingroup coreinit
 *
 * Standard condition variable implementation.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/condition_variable">std::condition_variable</a>.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSCondition OSCondition;
typedef struct OSMutex OSMutex;

#define OS_CONDITION_TAG 0x634E6456u

struct OSCondition
{
   //! Should always be set to the value OS_CONDITION_TAG.
   uint32_t tag;

   //! Name set by OSInitCondEx.
   const char *name;

   UNKNOWN(4);

   //! Queue of threads currently waiting on condition with OSWaitCond.
   OSThreadQueue queue;
};
CHECK_OFFSET(OSCondition, 0x00, tag);
CHECK_OFFSET(OSCondition, 0x04, name);
CHECK_OFFSET(OSCondition, 0x0c, queue);
CHECK_SIZE(OSCondition, 0x1c);


/**
 * Initialise a condition variable structure.
 */
void
OSInitCond(OSCondition *condition);


/**
 * Initialise a condition variable structure with a name.
 */
void
OSInitCondEx(OSCondition *condition,
             const char *name);


/**
 * Sleep the current thread until the condition variable has been signalled.
 *
 * The mutex must be locked when entering this function.
 * Will unlock the mutex and then sleep, reacquiring the mutex when woken.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/condition_variable/wait">std::condition_variable::wait</a>.
 */
void
OSWaitCond(OSCondition *condition,
           OSMutex *mutex);


/**
 * Will wake up any threads waiting on the condition with OSWaitCond.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/condition_variable/notify_all">std::condition_variable::notify_all</a>.
 */
void
OSSignalCond(OSCondition *condition);


#ifdef __cplusplus
}
#endif

/** @} */
