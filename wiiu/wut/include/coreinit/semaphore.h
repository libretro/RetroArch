#pragma once
#include <wut.h>
#include "threadqueue.h"

/**
 * \defgroup coreinit_semaphore Semaphore
 * \ingroup coreinit
 *
 * Similar to Windows <a href="https://msdn.microsoft.com/en-us/library/windows/desktop/ms685129(v=vs.85).aspx">Semaphore Objects</a>.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSSemaphore OSSemaphore;

#define OS_SEMAPHORE_TAG 0x73506852u

struct OSSemaphore
{
   //! Should always be set to the value OS_SEMAPHORE_TAG.
   uint32_t tag;

   //! Name set by OSInitMutexEx.
   const char *name;

   UNKNOWN(4);

   //! Current count of semaphore
   int32_t count;

   //! Queue of threads waiting on semaphore object with OSWaitSemaphore
   OSThreadQueue queue;
};
CHECK_OFFSET(OSSemaphore, 0x00, tag);
CHECK_OFFSET(OSSemaphore, 0x04, name);
CHECK_OFFSET(OSSemaphore, 0x0C, count);
CHECK_OFFSET(OSSemaphore, 0x10, queue);
CHECK_SIZE(OSSemaphore, 0x20);


/**
 * Initialise semaphore object with count.
 */
void
OSInitSemaphore(OSSemaphore *semaphore,
                int32_t count);


/**
 * Initialise semaphore object with count and name.
 */
void
OSInitSemaphoreEx(OSSemaphore *semaphore,
                  int32_t count,
                  const char *name);


/**
 * Get the current semaphore count.
 */
int32_t
OSGetSemaphoreCount(OSSemaphore *semaphore);


/**
 * Increase the semaphore value.
 *
 * If any threads are waiting for semaphore, they are woken.
 */
int32_t
OSSignalSemaphore(OSSemaphore *semaphore);


/**
 * Decrease the semaphore value.
 *
 * If the value is less than or equal to zero the current thread will be put to
 * sleep until the count is above zero and it can decrement it safely.
 */
int32_t
OSWaitSemaphore(OSSemaphore *semaphore);


/**
 * Try to decrease the semaphore value.
 *
 * If the value is greater than zero then it will be decremented, else the function
 * will return immediately with a value <= 0 indicating a failure.
 *
 * \return Returns previous semaphore count, before the decrement in this function.
 *         If the value is >0 then it means the call was succesful.
 */
int32_t
OSTryWaitSemaphore(OSSemaphore *semaphore);


#ifdef __cplusplus
}
#endif

/** @} */
