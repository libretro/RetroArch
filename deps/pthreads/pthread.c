/*
 * pthread.c
 *
 * Description:
 * POSIX thread functions related to threads.
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *
 *      Based upon Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 *
 *      Contact Email: rpj@callisto.canberra.edu.au
 *
 *      The original list of contributors to the Pthreads-win32 project
 *      is contained in the file CONTRIBUTORS.ptw32 included with the
 *      source code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pte_osal.h>

#include "pthread.h"
#include "implement.h"

#define PTE_ONCE_STARTED 1
#define PTE_ONCE_INIT 0
#define PTE_ONCE_DONE 2

static void pte_once_init_routine_cleanup(void * arg)
{
   pthread_once_t * once_control = (pthread_once_t *) arg;

   (void) PTE_ATOMIC_EXCHANGE(&once_control->state,PTE_ONCE_INIT);

   /* MBR fence */
   if (PTE_ATOMIC_EXCHANGE_ADD((int*)&once_control->semaphore, 0L)) 
      pte_osSemaphorePost((pte_osSemaphoreHandle) once_control->semaphore, 1);
}

void pthread_testcancel (void)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function creates a deferred cancellation point
 *      in the calling thread. The call has no effect if the
 *      current cancelability state is
 *              PTHREAD_CANCEL_DISABLE
 *
 * PARAMETERS
 *      N/A
 *
 *
 * DESCRIPTION
 *      This function creates a deferred cancellation point
 *      in the calling thread. The call has no effect if the
 *      current cancelability state is
 *              PTHREAD_CANCEL_DISABLE
 *
 *      NOTES:
 *      1)      Cancellation is asynchronous. Use pthread_join
 *              to wait for termination of thread if necessary
 *
 * RESULTS
 *              N/A
 *
 * ------------------------------------------------------
 */
{
   pthread_t self = pthread_self ();
   pte_thread_t * sp = (pte_thread_t *) self;

   if (sp == NULL)
      return;

   /*
    * Pthread_cancel() will have set sp->state to PThreadStateCancelPending
    * and set an event, so no need to enter kernel space if
    * sp->state != PThreadStateCancelPending - that only slows us down.
    */
   if (sp->state != PThreadStateCancelPending)
      return;

   (void) pthread_mutex_lock (&sp->cancelLock);

   if (sp->cancelState != PTHREAD_CANCEL_DISABLE)
   {
      sp->state = PThreadStateCanceling;
      sp->cancelState = PTHREAD_CANCEL_DISABLE;

      (void) pthread_mutex_unlock (&sp->cancelLock);
      pte_throw (PTE_EPS_CANCEL);
   }

   (void) pthread_mutex_unlock (&sp->cancelLock);
}

void pthread_terminate(void)
{
   pte_thread_t * tp, * tpNext;

   if (!pte_processInitialized)
      return;

   if (pte_selfThreadKey != NULL)
   {
      /*
       * Release pte_selfThreadKey
       */
      pthread_key_delete (pte_selfThreadKey);

      pte_selfThreadKey = NULL;
   }

   if (pte_cleanupKey != NULL)
   {
      /*
       * Release pte_cleanupKey
       */
      pthread_key_delete (pte_cleanupKey);

      pte_cleanupKey = NULL;
   }

   pte_osMutexLock (pte_thread_reuse_lock);


   tp = pte_threadReuseTop;
   while (tp != PTE_THREAD_REUSE_EMPTY)
   {
      tpNext = tp->prevReuse;
      free (tp);
      tp = tpNext;
   }

   pte_osMutexUnlock(pte_thread_reuse_lock);

   pte_processInitialized = PTE_FALSE;
}

int pthread_init(void)
{
   /*
    * Ignore if already initialized. this is useful for
    * programs that uses a non-dll pthread
    * library. Such programs must call pte_processInitialize() explicitly,
    * since this initialization routine is automatically called only when
    * the dll is loaded.
    */
   if (pte_processInitialized)
      return PTE_TRUE;

   pte_processInitialized = PTE_TRUE;

   // Must happen before creating keys.
   pte_osInit();

   /*
    * Initialize Keys
    */
   if ((pthread_key_create (&pte_selfThreadKey, NULL) != 0) ||
         (pthread_key_create (&pte_cleanupKey, NULL) != 0))
      pthread_terminate();

   /*
    * Set up the global locks.
    */
   pte_osMutexCreate (&pte_thread_reuse_lock);
   pte_osMutexCreate (&pte_mutex_test_init_lock);
   pte_osMutexCreate (&pte_cond_list_lock);
   pte_osMutexCreate (&pte_cond_test_init_lock);
   pte_osMutexCreate (&pte_rwlock_test_init_lock);
   pte_osMutexCreate (&pte_spinlock_test_init_lock);

   return (pte_processInitialized);
}

int pthread_join (pthread_t thread, void **value_ptr)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function waits for 'thread' to terminate and
    *      returns the thread's exit value if 'value_ptr' is not
    *      NULL. This also detaches the thread on successful
    *      completion.
    *
    * PARAMETERS
    *      thread
    *              an instance of pthread_t
    *
    *      value_ptr
    *              pointer to an instance of pointer to void
    *
    *
    * DESCRIPTION
    *      This function waits for 'thread' to terminate and
    *      returns the thread's exit value if 'value_ptr' is not
    *      NULL. This also detaches the thread on successful
    *      completion.
    *      NOTE:   detached threads cannot be joined or canceled
    *
    * RESULTS
    *              0               'thread' has completed
    *              EINVAL          thread is not a joinable thread,
    *              ESRCH           no thread could be found with ID 'thread',
    *              ENOENT          thread couldn't find it's own valid handle,
    *              EDEADLK         attempt to join thread with self
    *
    * ------------------------------------------------------
    */
{
   int result;
   pthread_t self;
   pte_thread_t * tp = (pte_thread_t *) thread;

   pte_osMutexLock (pte_thread_reuse_lock);

   if (NULL == tp
         || ((pte_thread_t*)thread)->x != tp->x)
      result = ESRCH;
   else if (PTHREAD_CREATE_DETACHED == tp->detachState)
      result = EINVAL;
   else
      result = 0;

   pte_osMutexUnlock(pte_thread_reuse_lock);

   if (result == 0)
   {
      /*
       * The target thread is joinable and can't be reused before we join it.
       */
      self = pthread_self();

      if (NULL == self)
         result = ENOENT;
      else if (pthread_equal (self, thread))
         result = EDEADLK;
      else
      {
         /*
          * Pthread_join is a cancelation point.
          * If we are canceled then our target thread must not be
          * detached (destroyed). This is guarranteed because
          * pthreadCancelableWait will not return if we
          * are canceled.
          */

         result = pte_osThreadWaitForEnd(tp->threadId);

         if (PTE_OS_OK == result)
         {
            if (value_ptr != NULL)
               *value_ptr = tp->exitStatus;

            /*
             * The result of making multiple simultaneous calls to
             * pthread_join() or pthread_detach() specifying the same
             * target is undefined.
             */
            result = pthread_detach (thread);
         }
         /* Call was cancelled, but still return success (per spec) */
         else if (result == PTE_OS_INTERRUPTED)
            result = 0;
         else
            result = ESRCH;
      }
   }

   return (result);
}

int pthread_cancel (pthread_t thread)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function requests cancellation of 'thread'.
    *
    * PARAMETERS
    *      thread
    *              reference to an instance of pthread_t
    *
    *
    * DESCRIPTION
    *      This function requests cancellation of 'thread'.
    *      NOTE: cancellation is asynchronous; use pthread_join to
    *                wait for termination of 'thread' if necessary.
    *
    * RESULTS
    *              0               successfully requested cancellation,
    *              ESRCH           no thread found corresponding to 'thread',
    *              ENOMEM          implicit self thread create failed.
    * ------------------------------------------------------
    */
{
   int result;
   int cancel_self;
   pthread_t self;
   pte_thread_t * tp;

   result = pthread_kill (thread, 0);

   if (0 != result)
      return result;

   if ((self = pthread_self ()) == NULL)
      return ENOMEM;

   /*
    * FIXME!!
    *
    * Can a thread cancel itself?
    *
    * The standard doesn't
    * specify an error to be returned if the target
    * thread is itself.
    *
    * If it may, then we need to ensure that a thread can't
    * deadlock itself trying to cancel itself asyncronously
    * (pthread_cancel is required to be an async-cancel
    * safe function).
    */
   cancel_self = pthread_equal (thread, self);

   tp = (pte_thread_t *) thread;

   /*
    * Lock for async-cancel safety.
    */
   (void) pthread_mutex_lock (&tp->cancelLock);

   if (tp->cancelType == PTHREAD_CANCEL_ASYNCHRONOUS
         && tp->cancelState == PTHREAD_CANCEL_ENABLE
         && tp->state < PThreadStateCanceling)
   {
      if (cancel_self)
      {
         tp->state = PThreadStateCanceling;
         tp->cancelState = PTHREAD_CANCEL_DISABLE;

         (void) pthread_mutex_unlock (&tp->cancelLock);
         pte_throw (PTE_EPS_CANCEL);

         /* Never reached */
      }
      else
      {
         /*
          * We don't support asynchronous cancellation for thread other than ourselves.
          * as it requires significant platform and OS specific functionality (see below).
          *
          * We should never get here, as we don't allow the cancellability type to be
          * sent to async.
          *
          * If you really wanted to implement async cancellation, you would probably need to
          * do something like the Win32 implement did, which is:
          *   1. Suspend the target thread.
          *   2. Replace the PC for the target thread to a routine that throws an exception
          *      or does a longjmp, depending on cleanup method.
          *   3. Resume the target thread.
          *
          * Note that most of the async cancellation code is still in here if anyone
          * wanted to add the OS/platform specific stuff.
          */
         (void) pthread_mutex_unlock (&tp->cancelLock);

         result = EPERM;

      }
   }
   else
   {
      /*
       * Set for deferred cancellation.
       */
      if (tp->state < PThreadStateCancelPending)
      {
         tp->state = PThreadStateCancelPending;

         if (pte_osThreadCancel(tp->threadId) != PTE_OS_OK)
            result = ESRCH;
      }
      else if (tp->state >= PThreadStateCanceling)
         result = ESRCH;

      (void) pthread_mutex_unlock (&tp->cancelLock);
   }

   return (result);
}

/*
 * pthread_delay_np
 *
 * DESCRIPTION
 *
 *       This routine causes a thread to delay execution for a specific period of time.
 *       This period ends at the current time plus the specified interval. The routine
 *       will not return before the end of the period is reached, but may return an
 *       arbitrary amount of time after the period has gone by. This can be due to
 *       system load, thread priorities, and system timer granularity.
 *
 *       Specifying an interval of zero (0) seconds and zero (0) nanoseconds is
 *       allowed and can be used to force the thread to give up the processor or to
 *       deliver a pending cancelation request.
 *
 *       The timespec structure contains the following two fields:
 *
 *            tv_sec is an integer number of seconds.
 *            tv_nsec is an integer number of nanoseconds.
 *
 *  Return Values
 *
 *  If an error condition occurs, this routine returns an integer value indicating
 *  the type of error. Possible return values are as follows:
 *
 *  0
 *           Successful completion.
 *  [EINVAL]
 *           The value specified by interval is invalid.
 *
 * Example
 *
 * The following code segment would wait for 5 and 1/2 seconds
 *
 *  struct timespec tsWait;
 *  int      intRC;
 *
 *  tsWait.tv_sec  = 5;
 *  tsWait.tv_nsec = 500000000L;
 *  intRC = pthread_delay_np(&tsWait);
 */
int pthread_delay_np (struct timespec *interval)
{
   unsigned int wait_time;
   unsigned int secs_in_millisecs;
   unsigned int millisecs;
   pthread_t self;
   pte_thread_t * sp;

   if (interval == NULL)
      return EINVAL;

   if (interval->tv_sec == 0L && interval->tv_nsec == 0L)
   {
      pthread_testcancel ();
      pte_osThreadSleep (1);
      pthread_testcancel ();
      return (0);
   }

   /* convert secs to millisecs */
   secs_in_millisecs = interval->tv_sec * 1000L;

   /* convert nanosecs to millisecs (rounding up) */
   millisecs = (interval->tv_nsec + 999999L) / 1000000L;

   wait_time = secs_in_millisecs + millisecs;

   if (NULL == (self = pthread_self ()))
      return ENOMEM;

   sp = (pte_thread_t *) self;

   if (sp->cancelState == PTHREAD_CANCEL_ENABLE)
   {
      pte_osResult cancelStatus;
      /*
       * Async cancelation won't catch us until wait_time is up.
       * Deferred cancelation will cancel us immediately.
       */
      cancelStatus = pte_osThreadCheckCancel(sp->threadId);

      if (cancelStatus == PTE_OS_INTERRUPTED)
      {
         /*
          * Canceling!
          */
         (void) pthread_mutex_lock (&sp->cancelLock);
         if (sp->state < PThreadStateCanceling)
         {
            sp->state = PThreadStateCanceling;
            sp->cancelState = PTHREAD_CANCEL_DISABLE;
            (void) pthread_mutex_unlock (&sp->cancelLock);

            pte_throw (PTE_EPS_CANCEL);
         }

         (void) pthread_mutex_unlock (&sp->cancelLock);
         return ESRCH;
      }
      else if (cancelStatus != PTE_OS_OK)
         return EINVAL;
   }
   else

      pte_osThreadSleep (wait_time);

   return (0);
}

int pthread_detach (pthread_t thread)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function detaches the given thread.
    *
    * PARAMETERS
    *      thread
    *              an instance of a pthread_t
    *
    *
    * DESCRIPTION
    *      This function detaches the given thread. You may use it to
    *      detach the main thread or to detach a joinable thread.
    *      NOTE:   detached threads cannot be joined;
    *              storage is freed immediately on termination.
    *
    * RESULTS
    *              0               successfully detached the thread,
    *              EINVAL          thread is not a joinable thread,
    *              ENOSPC          a required resource has been exhausted,
    *              ESRCH           no thread could be found for 'thread',
    *
    * ------------------------------------------------------
    */
{
   int result;
   unsigned char destroyIt = PTE_FALSE;
   pte_thread_t * tp = (pte_thread_t *) thread;

   pte_osMutexLock (pte_thread_reuse_lock);

   if (NULL == tp
         || ((pte_thread_t*)thread)->x != tp->x)
      result = ESRCH;
   else if (PTHREAD_CREATE_DETACHED == tp->detachState)
      result = EINVAL;
   else
   {
      /*
       * Joinable pte_thread_t structs are not scavenged until
       * a join or detach is done. The thread may have exited already,
       * but all of the state and locks etc are still there.
       */
      result = 0;

      if (pthread_mutex_lock (&tp->cancelLock) == 0)
      {
         if (tp->state != PThreadStateLast)
            tp->detachState = PTHREAD_CREATE_DETACHED;
         else if (tp->detachState != PTHREAD_CREATE_DETACHED)
         {
            /*
             * Thread is joinable and has exited or is exiting.
             */
            destroyIt = PTE_TRUE;
         }
         (void) pthread_mutex_unlock (&tp->cancelLock);
      }
      else
      {
         /* cancelLock shouldn't fail, but if it does ... */
         result = ESRCH;
      }
   }

   pte_osMutexUnlock(pte_thread_reuse_lock);

   if (result == 0)
   {
      /* Thread is joinable */

      if (destroyIt)
      {
         /* The thread has exited or is exiting but has not been joined or
          * detached. Need to wait in case it's still exiting.
          */
         pte_osThreadWaitForEnd(tp->threadId);

         pte_threadDestroy (thread);
      }
   }

   return (result);
}

int pthread_equal (pthread_t t1, pthread_t t2)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function returns nonzero if t1 and t2 are equal, else
    *      returns nonzero
    *
    * PARAMETERS
    *      t1,
    *      t2
    *              thread IDs
    *
    *
    * DESCRIPTION
    *      This function returns nonzero if t1 and t2 are equal, else
    *      returns zero.
    *
    * RESULTS
    *              non-zero        if t1 and t2 refer to the same thread,
    *              0               t1 and t2 do not refer to the same thread
    *
    * ------------------------------------------------------
    */
{
   /*
    * We also accept NULL == NULL - treating NULL as a thread
    * for this special case, because there is no error that we can return.
    */
   int result = ( t1 == t2 && ((pte_thread_t*)t1)->x == ((pte_thread_t*)t2)->x );

   return (result);
}

void pthread_exit (void *value_ptr)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function terminates the calling thread, returning
    *      the value 'value_ptr' to any joining thread.
    *
    * PARAMETERS
    *      value_ptr
    *              a generic data value (i.e. not the address of a value)
    *
    *
    * DESCRIPTION
    *      This function terminates the calling thread, returning
    *      the value 'value_ptr' to any joining thread.
    *      NOTE: thread should be joinable.
    *
    * RESULTS
    *              N/A
    *
    * ------------------------------------------------------
    */
{
   pte_thread_t * sp;

   /*
    * Don't use pthread_self() to avoid creating an implicit POSIX thread handle
    * unnecessarily.
    */
   sp = (pte_thread_t *) pthread_getspecific (pte_selfThreadKey);

   if (NULL == sp)
   {
      /*
       * A POSIX thread handle was never created. I.e. this is a
       * Win32 thread that has never called a pthreads-win32 routine that
       * required a POSIX handle.
       *
       * Implicit POSIX handles are cleaned up in pte_throw() now.
       */

      /* Terminate thread */
      pte_osThreadExit();

      /* Never reached */
   }

   sp->exitStatus = value_ptr;

   pte_throw (PTE_EPS_EXIT);

   /* Never reached. */
}

int pthread_once (pthread_once_t * once_control, void (*init_routine) (void))
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      If any thread in a process  with  a  once_control  parameter
 *      makes  a  call to pthread_once(), the first call will summon
 *      the init_routine(), but  subsequent  calls  will  not. The
 *      once_control  parameter  determines  whether  the associated
 *      initialization routine has been called.  The  init_routine()
 *      is complete upon return of pthread_once().
 *      This function guarantees that one and only one thread
 *      executes the initialization routine, init_routine when
 *      access is controlled by the pthread_once_t control
 *      key.
 *
 *      pthread_once() is not a cancelation point, but the init_routine
 *      can be. If it's cancelled then the effect on the once_control is
 *      as if pthread_once had never been entered.
 *
 *
 * PARAMETERS
 *      once_control
 *              pointer to an instance of pthread_once_t
 *
 *      init_routine
 *              pointer to an initialization routine
 *
 *
 * DESCRIPTION
 *      See above.
 *
 * RESULTS
 *              0               success,
 *              EINVAL          once_control or init_routine is NULL
 *
 * ------------------------------------------------------
 */
{
   int result;
   int state;
   pte_osSemaphoreHandle sema;

   if (once_control == NULL || init_routine == NULL)
   {
      result = EINVAL;
      goto FAIL0;
   }
   else
   {
      result = 0;
   }

   while ((state =
            PTE_ATOMIC_COMPARE_EXCHANGE(&once_control->state,
               PTE_ONCE_STARTED,
               PTE_ONCE_INIT))
         != PTE_ONCE_DONE)
   {
      if (PTE_ONCE_INIT == state)
      {


         pthread_cleanup_push(pte_once_init_routine_cleanup, (void *) once_control);
         (*init_routine)();
         pthread_cleanup_pop(0);

         (void) PTE_ATOMIC_EXCHANGE(&once_control->state,PTE_ONCE_DONE);

         /*
          * we didn't create the semaphore.
          * it is only there if there is someone waiting.
          */
         if (PTE_ATOMIC_EXCHANGE_ADD((int*)&once_control->semaphore, 0L)) /* MBR fence */
            pte_osSemaphorePost((pte_osSemaphoreHandle) once_control->semaphore,once_control->numSemaphoreUsers);
      }
      else
      {
         PTE_ATOMIC_INCREMENT(&once_control->numSemaphoreUsers);

         if (!PTE_ATOMIC_EXCHANGE_ADD((int*)&once_control->semaphore, 0L)) /* MBR fence */
         {
            pte_osSemaphoreCreate(0, (pte_osSemaphoreHandle*) &sema);

            if (PTE_ATOMIC_COMPARE_EXCHANGE((int *) &once_control->semaphore,
                     (int) sema,
                     0))
               pte_osSemaphoreDelete((pte_osSemaphoreHandle)sema);
         }

         /*
          * Check 'state' again in case the initting thread has finished or
          * cancelled and left before seeing that there was a semaphore.
          */
         if (PTE_ATOMIC_EXCHANGE_ADD(&once_control->state, 0L) == PTE_ONCE_STARTED)
            pte_osSemaphorePend((pte_osSemaphoreHandle) once_control->semaphore,NULL);

         if (0 == PTE_ATOMIC_DECREMENT(&once_control->numSemaphoreUsers))
         {
            /* we were last */
            if ((sema =
                     (pte_osSemaphoreHandle) PTE_ATOMIC_EXCHANGE((int *) &once_control->semaphore,0)))
               pte_osSemaphoreDelete(sema);
         }
      }
   }

   /*
    * ------------
    * Failure Code
    * ------------
    */
FAIL0:
   return (result);
}

pthread_t pthread_self (void)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function returns a reference to the current running
    *      thread.
    *
    * PARAMETERS
    *      N/A
    *
    *
    * DESCRIPTION
    *      This function returns a reference to the current running
    *      thread.
    *
    * RESULTS
    *              pthread_t       reference to the current thread
    *
    * ------------------------------------------------------
    */
{
   pthread_t self;
   pte_thread_t * sp;

   sp = (pte_thread_t *) pthread_getspecific (pte_selfThreadKey);

   if (sp != NULL)
      self = sp;
   else
   {
      /*
       * Need to create an implicit 'self' for the currently
       * executing thread.
       *
       * Note that this is a potential memory leak as there is
       * no way to free the memory and any resources allocated
       * by pte_new!
       */
      self = pte_new ();
      sp = (pte_thread_t *) self;

      if (sp != NULL)
      {
         /*
          * This is a non-POSIX thread which has chosen to call
          * a POSIX threads function for some reason. We assume that
          * it isn't joinable, but we do assume that it's
          * (deferred) cancelable.
          */
         sp->implicit = 1;
         sp->detachState = PTHREAD_CREATE_DETACHED;

         sp->threadId = pte_osThreadGetHandle();
         /*
          * No need to explicitly serialise access to sched_priority
          * because the new handle is not yet public.
          */
         sp->sched_priority = 0;

         pthread_setspecific (pte_selfThreadKey, (void *) sp);
      }
   }

   return (self);
}

/*
 * Notes on handling system time adjustments (especially negative ones).
 * ---------------------------------------------------------------------
 *
 * This solution was suggested by Alexander Terekhov, but any errors
 * in the implementation are mine - [Ross Johnson]
 *
 * 1) The problem: threads doing a timedwait on a CV may expect to timeout
 *    at a specific absolute time according to a system timer. If the
 *    system clock is adjusted backwards then those threads sleep longer than
 *    expected. Also, pthreads-embedded converts absolute times to intervals in
 *    order to make use of the underlying OS, and so waiting threads may
 *    awake before their proper abstimes.
 *
 * 2) We aren't able to distinquish between threads on timed or untimed waits,
 *    so we wake them all at the time of the adjustment so that they can
 *    re-evaluate their conditions and re-compute their timeouts.
 *
 * 3) We rely on correctly written applications for this to work. Specifically,
 *    they must be able to deal properly with spurious wakeups. That is,
 *    they must re-test their condition upon wakeup and wait again if
 *    the condition is not satisfied.
 */

void *
pthread_timechange_handler_np (void *arg)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      Broadcasts all CVs to force re-evaluation and
 *      new timeouts if required.
 *
 * PARAMETERS
 *      NONE
 *
 *
 * DESCRIPTION
 *      Broadcasts all CVs to force re-evaluation and
 *      new timeouts if required.
 *
 *      This routine may be passed directly to pthread_create()
 *      as a new thread in order to run asynchronously.
 *
 *
 * RESULTS
 *              0               successfully broadcast all CVs
 *              EAGAIN          Not all CVs were broadcast
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   pthread_cond_t cv;

   pte_osMutexLock (pte_cond_list_lock);

   cv = pte_cond_list_head;

   while (cv != NULL && 0 == result)
   {
      result = pthread_cond_broadcast (&cv);
      cv = cv->next;
   }

   pte_osMutexUnlock(pte_cond_list_lock);

   return (void *) (result != 0 ? EAGAIN : 0);
}

int pthread_kill (pthread_t thread, int sig)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function requests that a signal be delivered to the
    *      specified thread. If sig is zero, error checking is
    *      performed but no signal is actually sent such that this
    *      function can be used to check for a valid thread ID.
    *
    * PARAMETERS
    *      thread  reference to an instances of pthread_t
    *      sig     signal. Currently only a value of 0 is supported.
    *
    *
    * DESCRIPTION
    *      This function requests that a signal be delivered to the
    *      specified thread. If sig is zero, error checking is
    *      performed but no signal is actually sent such that this
    *      function can be used to check for a valid thread ID.
    *
    * RESULTS
    *              ESRCH           the thread is not a valid thread ID,
    *              EINVAL          the value of the signal is invalid
    *                              or unsupported.
    *              0               the signal was successfully sent.
    *
    * ------------------------------------------------------
    */
{
   int result = 0;
   pte_thread_t * tp;

   pte_osMutexLock (pte_thread_reuse_lock);

   tp = (pte_thread_t *) thread;

   if (NULL == tp
         || ((pte_thread_t*)thread)->x != tp->x
         || 0 == tp->threadId)
      result = ESRCH;

   pte_osMutexUnlock(pte_thread_reuse_lock);

   /*
    * Currently does not support any signals.
    */
   if (0 == result && 0 != sig)
      result = EINVAL;

   return result;
}

/*
 * pthread_num_processors_np()
 *
 * Get the number of CPUs available to the process.
 */
int pthread_num_processors_np (void)
{
   int count;

   if (pte_getprocessors (&count) != 0)
      count = 1;

   return (count);
}
