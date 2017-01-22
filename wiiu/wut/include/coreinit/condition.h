#pragma once
#include <wut.h>
#include "threadqueue.h"
#include "mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_CONDITION_TAG 0x634E6456u

typedef struct OSCondition
{
   //! Should always be set to the value OS_CONDITION_TAG.
   uint32_t tag;

   //! Name set by OSInitCondEx.
   const char *name;

   uint32_t __unknown;

   //! Queue of threads currently waiting on condition with OSWaitCond.
   OSThreadQueue queue;
}OSCondition;

/**
 * Initialise a condition variable structure.
 */
void OSInitCond(OSCondition *condition);


/**
 * Initialise a condition variable structure with a name.
 */
void OSInitCondEx(OSCondition *condition,
             const char *name);


/**
 * Sleep the current thread until the condition variable has been signalled.
 *
 * The mutex must be locked when entering this function.
 * Will unlock the mutex and then sleep, reacquiring the mutex when woken.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/condition_variable/wait">std::condition_variable::wait</a>.
 */
void OSWaitCond(OSCondition *condition,
           OSMutex *mutex);


/**
 * Will wake up any threads waiting on the condition with OSWaitCond.
 *
 * Similar to <a href="http://en.cppreference.com/w/cpp/thread/condition_variable/notify_all">std::condition_variable::notify_all</a>.
 */
void OSSignalCond(OSCondition *condition);


#ifdef __cplusplus
}
#endif
