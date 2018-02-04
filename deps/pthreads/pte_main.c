/*
 * pte_main.c
 *
 * Description:
 * This translation unit instantiates data associated with the implementation
 * as a whole.
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

#include "pte_osal.h"

#include "pthread.h"
#include "implement.h"


int pte_processInitialized = PTE_FALSE;
pte_thread_t * pte_threadReuseTop = PTE_THREAD_REUSE_EMPTY;
pte_thread_t * pte_threadReuseBottom = PTE_THREAD_REUSE_EMPTY;
pthread_key_t pte_selfThreadKey = NULL;
pthread_key_t pte_cleanupKey = NULL;
pthread_cond_t pte_cond_list_head = NULL;
pthread_cond_t pte_cond_list_tail = NULL;

int pte_concurrency = 0;

/* What features have been auto-detaected */
int pte_features = 0;

unsigned char pte_smp_system = PTE_TRUE;  /* Safer if assumed true initially. */

/*
 * Global lock for managing pthread_t struct reuse.
 */
pte_osMutexHandle pte_thread_reuse_lock;

/*
 * Global lock for testing internal state of statically declared mutexes.
 */
pte_osMutexHandle pte_mutex_test_init_lock;

/*
 * Global lock for testing internal state of PTHREAD_COND_INITIALIZER
 * created condition variables.
 */
pte_osMutexHandle pte_cond_test_init_lock;

/*
 * Global lock for testing internal state of PTHREAD_RWLOCK_INITIALIZER
 * created read/write locks.
 */
pte_osMutexHandle pte_rwlock_test_init_lock;

/*
 * Global lock for testing internal state of PTHREAD_SPINLOCK_INITIALIZER
 * created spin locks.
 */
pte_osMutexHandle pte_spinlock_test_init_lock;

/*
 * Global lock for condition variable linked list. The list exists
 * to wake up CVs when a WM_TIMECHANGE message arrives. See
 * w32_TimeChangeHandler.c.
 */
pte_osMutexHandle pte_cond_list_lock;

int pthread_create (pthread_t * tid,
                const pthread_attr_t * attr,
                void *(*start) (void *), void *arg)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function creates a thread running the start function,
 *      passing it the parameter value, 'arg'. The 'attr'
 *      argument specifies optional creation attributes.
 *      The identity of the new thread is returned
 *      via 'tid', which should not be NULL.
 *
 * PARAMETERS
 *      tid
 *              pointer to an instance of pthread_t
 *
 *      attr
 *              optional pointer to an instance of pthread_attr_t
 *
 *      start
 *              pointer to the starting routine for the new thread
 *
 *      arg
 *              optional parameter passed to 'start'
 *
 *
 * DESCRIPTION
 *      This function creates a thread running the start function,
 *      passing it the parameter value, 'arg'. The 'attr'
 *      argument specifies optional creation attributes.
 *      The identity of the new thread is returned
 *      via 'tid', which should not be the NULL pointer.
 *
 * RESULTS
 *              0               successfully created thread,
 *              EINVAL          attr invalid,
 *              EAGAIN          insufficient resources.
 *
 * ------------------------------------------------------
 */
{
   pthread_t thread;
   pte_thread_t * tp;
   int result = EAGAIN;
   int run = PTE_TRUE;
   ThreadParms *parms = NULL;
   long stackSize;
   int priority = 0;
   pthread_t self;
   pte_osResult osResult;
   register pthread_attr_t a = NULL;

   /*
    * Before doing anything, check that tid can be stored through
    * without invoking a memory protection error (segfault).
    * Make sure that the assignment below can't be optimised out by the compiler.
    * This is assured by conditionally assigning *tid again at the end.
    */

   if (attr != NULL)
      a = *attr;

   if ((thread = pte_new ()) == NULL)
      goto FAIL0;

   tp = (pte_thread_t *) thread;

   priority = tp->sched_priority;

   if ((parms = (ThreadParms *) malloc (sizeof (*parms))) == NULL)
      goto FAIL0;

   parms->tid   = thread;
   parms->start = start;
   parms->arg   = arg;

   if (a != NULL)
   {
      stackSize = a->stacksize;
      tp->detachState = a->detachstate;
      priority = a->param.sched_priority;

      if ( (priority > pte_osThreadGetMaxPriority()) ||
            (priority < pte_osThreadGetMinPriority()) )
      {
         result = EINVAL;
         goto FAIL0;
      }

      /* Everything else */

      /*
       * Thread priority must be set to a valid system level
       * without altering the value set by pthread_attr_setschedparam().
       */

      if (PTHREAD_INHERIT_SCHED == a->inheritsched)
      {
         /*
          * If the thread that called pthread_create() is an OS thread
          * then the inherited priority could be the result of a temporary
          * system adjustment. This is not the case for POSIX threads.
          */
         self = pthread_self ();
         priority = ((pte_thread_t *) self)->sched_priority;
      }


   }
   else
   {
      /*
       * Default stackSize
       */
      stackSize = PTHREAD_STACK_MIN;

   }

   tp->state = run ? PThreadStateInitial : PThreadStateSuspended;

   tp->keys  = NULL;

   /*
    * Threads must be started in suspended mode and resumed if necessary
    * after _beginthreadex returns us the handle. Otherwise we set up a
    * race condition between the creating and the created threads.
    * Note that we also retain a local copy of the handle for use
    * by us in case thread.p->threadH gets NULLed later but before we've
    * finished with it here.
    */
   result = pthread_mutex_lock (&tp->threadLock);

   if (result == 0)
   {
      /*
       * Must record the thread's sched_priority as given,
       * not as finally adjusted.
       */
      tp->sched_priority = priority;

      (void) pthread_mutex_unlock (&tp->threadLock);
   }

   osResult = pte_osThreadCreate(pte_threadStart,
         stackSize,
         priority,
         parms,
         &(tp->threadId));

   if (osResult == PTE_OS_OK)
   {
      pte_osThreadStart(tp->threadId);
      result = 0;
   }
   else
   {
      tp->threadId = 0;
      result = EAGAIN;
      goto FAIL0;
   }

   /*
    * Fall Through Intentionally
    */

   /*
    * ------------
    * Failure Code
    * ------------
    */

FAIL0:
   if (result != 0)
   {

      pte_threadDestroy (thread);
      tp = NULL;

      if (parms != NULL)
         free (parms);
   }
   else
   {
      if (tid != NULL)
         *tid = thread;
   }

   return (result);
}

/*
 * The functions pte_pop_cleanup and pte_push_cleanup
 * are implemented here for applications written in C with no
 * C++ destructor support.
 */

pte_cleanup_t *pte_pop_cleanup (int execute)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function pops the most recently pushed cleanup
    *      handler. If execute is nonzero, then the cleanup handler
    *      is executed if non-null.
    *
    * PARAMETERS
    *      execute
    *              if nonzero, execute the cleanup handler
    *
    *
    * DESCRIPTION
    *      This function pops the most recently pushed cleanup
    *      handler. If execute is nonzero, then the cleanup handler
    *      is executed if non-null.
    *      NOTE: specify 'execute' as nonzero to avoid duplication
    *                of common cleanup code.
    *
    * RESULTS
    *              N/A
    *
    * ------------------------------------------------------
    */
{
   pte_cleanup_t *cleanup;

   cleanup = (pte_cleanup_t *) pthread_getspecific (pte_cleanupKey);

   if (cleanup != NULL)
   {
      if (execute && (cleanup->routine != NULL))
      {

         (*cleanup->routine) (cleanup->arg);

      }

      pthread_setspecific (pte_cleanupKey, (void *) cleanup->prev);

   }

   return (cleanup);

}				/* pte_pop_cleanup */


   void
pte_push_cleanup (pte_cleanup_t * cleanup,
      pte_cleanup_callback_t routine, void *arg)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function pushes a new cleanup handler onto the thread's stack
    *      of cleanup handlers. Each cleanup handler pushed onto the stack is
    *      popped and invoked with the argument 'arg' when
    *              a) the thread exits by calling 'pthread_exit',
    *              b) when the thread acts on a cancellation request,
    *              c) or when the thread calls pthread_cleanup_pop with a nonzero
    *                 'execute' argument
    *
    * PARAMETERS
    *      cleanup
    *              a pointer to an instance of pthread_cleanup_t,
    *
    *      routine
    *              pointer to a cleanup handler,
    *
    *      arg
    *              parameter to be passed to the cleanup handler
    *
    *
    * DESCRIPTION
    *      This function pushes a new cleanup handler onto the thread's stack
    *      of cleanup handlers. Each cleanup handler pushed onto the stack is
    *      popped and invoked with the argument 'arg' when
    *              a) the thread exits by calling 'pthread_exit',
    *              b) when the thread acts on a cancellation request,
    *              c) or when the thrad calls pthread_cleanup_pop with a nonzero
    *                 'execute' argument
    *      NOTE: pthread_push_cleanup, pte_pop_cleanup must be paired
    *                in the same lexical scope.
    *
    * RESULTS
    *              pthread_cleanup_t *
    *                              pointer to the previous cleanup
    *
    * ------------------------------------------------------
    */
{
   cleanup->routine = routine;
   cleanup->arg = arg;

   cleanup->prev = (pte_cleanup_t *) pthread_getspecific (pte_cleanupKey);

   pthread_setspecific (pte_cleanupKey, (void *) cleanup);
}
