/*
 * pte_cancellable_wait.c
 *
 * Description:
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef _GENERIC_OS_SUPPORT_H_
#define _GENERIC_OS_SUPPORT_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/** @name Misc */
//@{
typedef enum pte_osResult
{

  /** Operation completed successfully */
  PTE_OS_OK = 0,

  /** Operation failed because there insufficient resources */
  PTE_OS_NO_RESOURCES,

  /** Operation failed due to a general failure */
  PTE_OS_GENERAL_FAILURE,

  /** Operation did not complete because a user specified timeout expired. */
  PTE_OS_TIMEOUT,

  /** The operation was interrupted before it could complete. */
  PTE_OS_INTERRUPTED,

  /** An invalid parameter was specified */
  PTE_OS_INVALID_PARAM


} pte_osResult;

/**
 * Provides a hook for the OSAL to implement any OS specific initialization.  This is guaranteed to be
 * called before any other OSAL function.
 */
pte_osResult pte_osInit(void);
//@}

/** @name Mutexes */
//@{

/**
 * Creates a mutex
 *
 * @param pHandle  Set to the handle of the newly created mutex.
 *
 * @return PTE_OS_OK - Mutex successfully created
 * @return PTE_OS_NO_RESOURCESs - Insufficient resources to create mutex
 */
pte_osResult pte_osMutexCreate(pte_osMutexHandle *pHandle);

/**
 * Deletes a mutex and frees any associated resources.
 *
 * @param handle Handle of mutex to delete.
 *
 * @return PTE_OS_OK - Mutex successfully deleted.
 */
pte_osResult pte_osMutexDelete(pte_osMutexHandle handle);

/**
 * Locks the mutex
 *
 * @param handle Handle of mutex to lock.
 *
 * @return PTE_OS_OK - Mutex successfully locked.
 */
pte_osResult pte_osMutexLock(pte_osMutexHandle handle);

/**
 * Locks the mutex, returning after @p timeoutMsecs if the resources is not
 * available.  Can be used for polling mutex by using @p timeoutMsecs of zero.
 *
 * @param handle Handle of mutex to lock.
 * @param timeoutMsecs Number of milliseconds to wait for resource before returning.
 *
 * @return PTE_OS_OK - Mutex successfully locked.
 * @return PTE_OS_TIMEOUT - Timeout expired before lock was obtained.
 */
pte_osResult pte_osMutexTimedLock(pte_osMutexHandle handle, unsigned int timeoutMsecs);

/**
 * Unlocks the mutex
 *
 * @param handle Handle of mutex to unlock
 *
 * @return PTE_OS_OK - Mutex successfully unlocked.
 */
pte_osResult pte_osMutexUnlock(pte_osMutexHandle handle);
//@}

/** @name Threads */
//@{
typedef int (*pte_osThreadEntryPoint)(void *params);


/**
 * Creates a new thread.  The thread must be started in a suspended state - it will be
 * explicitly started when pte_osThreadStart() is called.
 *
 * @param entryPoint Entry point to the new thread.
 * @param stackSize The initial stack size, in bytes.  Note that this can be considered a minimum -
 *                  for instance if the OS requires a larger stack space than what the caller specified.
 * @param initialPriority The priority that the new thread should be initially set to.
 * @param argv Parameter to pass to the new thread.
 * @param ppte_osThreadHandle set to the handle of the new thread.
 *
 * @return PTE_OS_OK - New thread successfully created.
 * @return PTE_OS_NO_RESOURCESs - Insufficient resources to create thread
 */
pte_osResult pte_osThreadCreate(pte_osThreadEntryPoint entryPoint,
                                int stackSize,
                                int initialPriority,
                                void *argv,
                                pte_osThreadHandle* ppte_osThreadHandle);

/**
 * Starts executing the specified thread.
 *
 * @param osThreadHandle handle of the thread to start.
 *
 * @return PTE_OS_OK - thread successfully started.
 */
pte_osResult pte_osThreadStart(pte_osThreadHandle osThreadHandle);

/**
 * Causes the current thread to stop executing.
 *
 * @return Never returns (thread terminated)
 */
void pte_osThreadExit();

/**
 * Waits for the specified thread to end.  If the thread has already terminated, this returns
 * immediately.
 *
 * @param threadHandle Handle fo thread to wait for.
 *
 * @return PTE_OS_OK - specified thread terminated.
 */
pte_osResult pte_osThreadWaitForEnd(pte_osThreadHandle threadHandle);

/**
 * Returns the handle of the currently executing thread.
 */
pte_osThreadHandle pte_osThreadGetHandle(void);

/**
 * Returns the priority of the specified thread.
 */
int pte_osThreadGetPriority(pte_osThreadHandle threadHandle);

/**
 * Sets the priority of the specified thread.
 *
 * @return PTE_OS_OK - thread priority successfully set
 */
pte_osResult pte_osThreadSetPriority(pte_osThreadHandle threadHandle, int newPriority);

/**
 * Frees resources associated with the specified thread.  This is called after the thread has terminated
 * and is no longer needed (e.g. after pthread_join returns).  This call will always be made
 * from a different context than that of the target thread.
 */
pte_osResult pte_osThreadDelete(pte_osThreadHandle handle);

/**
 * Frees resources associated with the specified thread and then causes the thread to exit.
 * This is called after the thread has terminated and is no longer needed (e.g. after
 * pthread_join returns).  This call will always be made from the context of the target thread.
 */
pte_osResult pte_osThreadExitAndDelete(pte_osThreadHandle handle);

/**
 * Cancels the specified thread.  This should cause pte_osSemaphoreCancellablePend() and for pte_osThreadCheckCancel()
 * to return @p PTE_OS_INTERRUPTED.
 *
 * @param threadHandle handle to the thread to cancel.
 *
 * @return Thread successfully canceled.
 */
pte_osResult pte_osThreadCancel(pte_osThreadHandle threadHandle);

/**
 * Check if pte_osThreadCancel() has been called on the specified thread.
 *
 * @param threadHandle handle of thread to check the state of.
 *
 * @return PTE_OS_OK - Thread has not been cancelled
 * @return PTE_OS_INTERRUPTED - Thread has been cancelled.
 */
pte_osResult pte_osThreadCheckCancel(pte_osThreadHandle threadHandle);

/**
 * Causes the current thread to sleep for the specified number of milliseconds.
 */
void pte_osThreadSleep(unsigned int msecs);

/**
 * Returns the maximum allowable priority
 */
int pte_osThreadGetMaxPriority();

/**
 * Returns the minimum allowable priority
 */
int pte_osThreadGetMinPriority();

/**
 * Returns the priority that should be used if the caller to pthread_create doesn't
 * explicitly set one.
 */
int pte_osThreadGetDefaultPriority();

//@}


/** @name Semaphores */
//@{

/**
 * Creates a semaphore
 *
 * @param initialValue Initial value of the semaphore
 * @param pHandle  Set to the handle of the newly created semaphore.
 *
 * @return PTE_OS_OK - Semaphore successfully created
 * @return PTE_OS_NO_RESOURCESs - Insufficient resources to create semaphore
 */
pte_osResult pte_osSemaphoreCreate(int initialValue, pte_osSemaphoreHandle *pHandle);

/**
 * Deletes a semaphore and frees any associated resources.
 *
 * @param handle Handle of semaphore to delete.
 *
 * @return PTE_OS_OK - Semaphore successfully deleted.
 */
pte_osResult pte_osSemaphoreDelete(pte_osSemaphoreHandle handle);

/**
 * Posts to the semaphore
 *
 * @param handle Semaphore to release
 * @param count  Amount to increment the semaphore by.
 *
 * @return PTE_OS_OK - semaphore successfully released.
 */
pte_osResult pte_osSemaphorePost(pte_osSemaphoreHandle handle, int count);

/**
 * Acquire a semaphore, returning after @p timeoutMsecs if the semaphore is not
 * available.  Can be used for polling a semaphore by using @p timeoutMsecs of zero.
 *
 * @param handle Handle of semaphore to acquire.
 * @param pTimeout Pointer to the number of milliseconds to wait to acquire the semaphore
 *                 before returning.  If set to NULL, wait forever.
 *
 * @return PTE_OS_OK - Semaphore successfully acquired.
 * @return PTE_OS_TIMEOUT - Timeout expired before semaphore was obtained.
 */
pte_osResult pte_osSemaphorePend(pte_osSemaphoreHandle handle, unsigned int *pTimeout);

/**
 * Acquire a semaphore, returning after @p timeoutMsecs if the semaphore is not
 * available.  Can be used for polling a semaphore by using @p timeoutMsecs of zero.
 * Call must return immediately if pte_osThreadCancel() is called on the thread waiting for
 * the semaphore.
 *
 * @param handle Handle of semaphore to acquire.
 * @param pTimeout Pointer to the number of milliseconds to wait to acquire the semaphore
 *                 before returning.  If set to NULL, wait forever.
 *
 * @return PTE_OS_OK - Semaphore successfully acquired.
 * @return PTE_OS_TIMEOUT - Timeout expired before semaphore was obtained.
 */
pte_osResult pte_osSemaphoreCancellablePend(pte_osSemaphoreHandle handle, unsigned int *pTimeout);
//@}


/** @name Thread Local Storage */
//@{
/**
 * Sets the thread specific value for the specified key for the
 * currently executing thread.
 *
 * @param index The TLS key for the value.
 * @param value The value to save
 */
pte_osResult pte_osTlsSetValue(unsigned int key, void * value);

/**
 * Retrieves the thread specific value for the specified key for
 * the currently executing thread.  If a value has not been set
 * for this key, NULL should be returned (i.e. TLS values default
 * to NULL).
 *
 * @param index The TLS key for the value.
 *
 * @return The value associated with @p key for the current thread.
 */
void * pte_osTlsGetValue(unsigned int key);

/**
 * Initializes the OS TLS support.  This is called by the PTE library
 * prior to performing ANY TLS operation.
 */
void pte_osTlsInit(void);

/**
 * Allocates a new TLS key.
 *
 * @param pKey On success will be set to the newly allocated key.
 *
 * @return PTE_OS_OK - TLS key successfully allocated.
 * @return PTE_OS_NO_RESOURCESs - Insufficient resources to allocate key (e.g.
 *                         maximum number of keys reached).
 */
pte_osResult pte_osTlsAlloc(unsigned int *pKey);

/**
 * Frees the specified TLS key.
 *
 * @param index TLS key to free
 *
 * @return PTE_OS_OK - TLS key was successfully freed.
 */
pte_osResult pte_osTlsFree(unsigned int key);
//@}

/** @name Atomic operations */
//@{

/**
 * Sets the target to the specified value as an atomic operation.
 *
 * \code
 * origVal = *ptarg
 * *ptarg = val
 * return origVal
 * \endcode
 *
 * @param pTarg Pointer to the value to be exchanged.
 * @param val Value to be exchanged
 *
 * @return original value of destination
 */
int pte_osAtomicExchange(int *pTarg, int val);

/**
 * Performs an atomic compare-and-exchange oepration on the specified
 * value.  That is:
 *
 * \code
 * origVal = *pdest
 * if (*pdest == comp)
 *   then *pdest = exchange
 * return origVal
 * \endcode
 *
 * @param pdest Pointer to the destination value.
 * @param exchange Exchange value (value to set destination to if destination == comparand)
 * @param comp The value to compare to destination.
 *
 * @return Original value of destination
 */
int pte_osAtomicCompareExchange(int *pdest, int exchange, int comp);

/**
 * Adds the value to target as an atomic operation
 *
 * \code
 * origVal = *pdest
 * *pAddend += value
 * return origVal
 * \endcode
 *
 * @param pdest Pointer to the variable to be updated.
 * @param value Value to be added to the variable.
 *
 * @return Original value of destination
 */
int  pte_osAtomicExchangeAdd(int volatile* pdest, int value);

/**
 * Decrements the destination.
 *
 * \code
 * origVal = *pdest
 * *pdest++
 * return origVal
 * \endcode
 *
 * @param pdest Destination value to decrement
 *
 * @return Original destination value
 */
int pte_osAtomicDecrement(int *pdest);

/**
 * Increments the destination value
 *
 * \code
 * origVal = *pdest;
 * *pdest++;
 * return origVal;
 */
int pte_osAtomicIncrement(int *pdest);
//@}

struct timeb;

int ftime(struct timeb *tb);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _OS_SUPPORT_H_
