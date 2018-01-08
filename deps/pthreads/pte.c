/*
 * pte.c
 *
 * Description:
 * This translation unit implements routines which are private to
 * the implementation and may be used throughout it.
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

#include "pthread.h"
#include "semaphore.h"
#include "implement.h"

#include <pte_osal.h>

typedef long long int64_t;

static struct pthread_mutexattr_t_ pte_recursive_mutexattr_s =
{
   PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_RECURSIVE
};
static struct pthread_mutexattr_t_ pte_errorcheck_mutexattr_s =
{
   PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_ERRORCHECK
};
static pthread_mutexattr_t pte_recursive_mutexattr = &pte_recursive_mutexattr_s;
static pthread_mutexattr_t pte_errorcheck_mutexattr = &pte_errorcheck_mutexattr_s;

static int pte_thread_detach_common (unsigned char threadShouldExit)
{
   if (pte_processInitialized)
   {
      /*
       * Don't use pthread_self() - to avoid creating an implicit POSIX thread handle
       * unnecessarily.
       */
      pte_thread_t * sp = (pte_thread_t *) pthread_getspecific (pte_selfThreadKey);

      if (sp) // otherwise OS thread with no implicit POSIX handle.
      {
         pte_callUserDestroyRoutines (sp);

         (void) pthread_mutex_lock (&sp->cancelLock);
         sp->state = PThreadStateLast;

         /*
          * If the thread is joinable at this point then it MUST be joined
          * or detached explicitly by the application.
          */
         (void) pthread_mutex_unlock (&sp->cancelLock);

         if (sp->detachState == PTHREAD_CREATE_DETACHED)
         {
            if (threadShouldExit)
               pte_threadExitAndDestroy (sp);
            else
               pte_threadDestroy (sp);

#if 0
            pte_osTlsSetValue (pte_selfThreadKey->key, NULL);
#endif
         }
         else
         {
            if (threadShouldExit)
               pte_osThreadExit();
         }
      }
   }

   return 1;
}

static void pte_threadDestroyCommon (pthread_t thread, unsigned char shouldThreadExit)
{
   pte_thread_t threadCopy;
   pte_thread_t * tp = (pte_thread_t *) thread;

   if (!tp)
      return;

   /*
    * Copy thread state so that the thread can be atomically NULLed.
    */
   memcpy (&threadCopy, tp, sizeof (threadCopy));

   /*
    * Thread ID structs are never freed. They're NULLed and reused.
    * This also sets the thread to PThreadStateInitial (invalid).
    */
   pte_threadReusePush (thread);

   (void) pthread_mutex_destroy(&threadCopy.cancelLock);
   (void) pthread_mutex_destroy(&threadCopy.threadLock);

   if (threadCopy.threadId != 0)
   {
      if (shouldThreadExit)
         pte_osThreadExitAndDelete(threadCopy.threadId);
      else
         pte_osThreadDelete(threadCopy.threadId);
   }
}

void
pte_callUserDestroyRoutines (pthread_t thread)
/*
 * -------------------------------------------------------------------
 * DOCPRIVATE
 *
 * This the routine runs through all thread keys and calls
 * the destroy routines on the user's data for the current thread.
 * It simulates the behaviour of POSIX Threads.
 *
 * PARAMETERS
 *              thread
 *                      an instance of pthread_t
 *
 * RETURNS
 *              N/A
 * -------------------------------------------------------------------
 */
{
   int assocsRemaining;
   int iterations = 0;
   ThreadKeyAssoc *assoc = NULL;
   pte_thread_t   *sp    = (pte_thread_t *) thread;

   if (!thread)
      return;

   /*
    * Run through all Thread<-->Key associations
    * for the current thread.
    *
    * Do this process at most PTHREAD_DESTRUCTOR_ITERATIONS times.
    */
   do
   {
      assocsRemaining = 0;
      iterations++;

      (void) pthread_mutex_lock(&(sp->threadLock));
      /*
       * The pointer to the next assoc is stored in the thread struct so that
       * the assoc destructor in pthread_key_delete can adjust it
       * if it deletes this assoc. This can happen if we fail to acquire
       * both locks below, and are forced to release all of our locks,
       * leaving open the opportunity for pthread_key_delete to get in
       * before us.
       */
      sp->nextAssoc = sp->keys;
      (void) pthread_mutex_unlock(&(sp->threadLock));

      for (;;)
      {
         void * value;
         pthread_key_t k;
         void (*destructor) (void *);

         /*
          * First we need to serialise with pthread_key_delete by locking
          * both assoc guards, but in the reverse order to our convention,
          * so we must be careful to avoid deadlock.
          */
         (void) pthread_mutex_lock(&(sp->threadLock));

         if ((assoc = (ThreadKeyAssoc *)sp->nextAssoc) == NULL)
         {
            /* Finished */
            pthread_mutex_unlock(&(sp->threadLock));
            break;
         }
         else
         {
            /*
             * assoc->key must be valid because assoc can't change or be
             * removed from our chain while we hold at least one lock. If
             * the assoc was on our key chain then the key has not been
             * deleted yet.
             *
             * Now try to acquire the second lock without deadlocking.
             * If we fail, we need to relinquish the first lock and the
             * processor and then try to acquire them all again.
             */
            if (pthread_mutex_trylock(&(assoc->key->keyLock)) == EBUSY)
            {
               pthread_mutex_unlock(&(sp->threadLock));
               pte_osThreadSleep(1); // Ugly but necessary to avoid priority effects.
               /*
                * Go around again.
                * If pthread_key_delete has removed this assoc in the meantime,
                * sp->nextAssoc will point to a new assoc.
                */
               continue;
            }
         }

         /* We now hold both locks */

         sp->nextAssoc = assoc->nextKey;

         /*
          * Key still active; pthread_key_delete
          * will block on these same mutexes before
          * it can release actual key; therefore,
          * key is valid and we can call the destroy
          * routine;
          */
         k = assoc->key;
         destructor = k->destructor;
         value = pte_osTlsGetValue(k->key);
         pte_osTlsSetValue (k->key, NULL);

         // Every assoc->key exists and has a destructor
         if (value && iterations <= PTHREAD_DESTRUCTOR_ITERATIONS)
         {
            /*
             * Unlock both locks before the destructor runs.
             * POSIX says pthread_key_delete can be run from destructors,
             * and that probably includes with this key as target.
             * pthread_setspecific can also be run from destructors and
             * also needs to be able to access the assocs.
             */
            (void) pthread_mutex_unlock(&(sp->threadLock));
            (void) pthread_mutex_unlock(&(k->keyLock));

            assocsRemaining++;


            /*
             * Run the caller's cleanup routine.
             */
            destructor (value);
         }
         else
         {
            /*
             * Remove association from both the key and thread chains
             * and reclaim it's memory resources.
             */
            pte_tkAssocDestroy (assoc);
            (void) pthread_mutex_unlock(&(sp->threadLock));
            (void) pthread_mutex_unlock(&(k->keyLock));
         }
      }
   }while (assocsRemaining);
}

int pte_cancellable_wait (pte_osSemaphoreHandle semHandle, unsigned int* timeout)
{
   pte_osResult osResult;
   int result        = EINVAL;
   int cancelEnabled = 0;
   pthread_t self    = pthread_self();
   pte_thread_t *sp  = (pte_thread_t *) self;

   if (sp)
   {
      /*
       * Get cancelEvent handle
       */
      if (sp->cancelState == PTHREAD_CANCEL_ENABLE)
         cancelEnabled = 1;
   }


   if (cancelEnabled)
      osResult = pte_osSemaphoreCancellablePend(semHandle, timeout);
   else
      osResult = pte_osSemaphorePend(semHandle, timeout);

   switch (osResult)
   {
      case PTE_OS_OK:
         result = 0;
         break;

      case PTE_OS_TIMEOUT:
         result = ETIMEDOUT;
         break;

      case PTE_OS_INTERRUPTED:
         if (sp)
         {
            /*
             * Should handle POSIX and implicit POSIX threads..
             * Make sure we haven't been async-canceled in the meantime.
             */
            (void) pthread_mutex_lock (&sp->cancelLock);
            if (sp->state < PThreadStateCanceling)
            {
               sp->state = PThreadStateCanceling;
               sp->cancelState = PTHREAD_CANCEL_DISABLE;
               (void) pthread_mutex_unlock (&sp->cancelLock);
               pte_throw (PTE_EPS_CANCEL);

               /* Never reached */
            }
            (void) pthread_mutex_unlock (&sp->cancelLock);
         }
         break;
      default:
         result = EINVAL;
   }

   return (result);
}

int pte_cond_check_need_init (pthread_cond_t * cond)
{
   int result = 0;

   /*
    * The following guarded test is specifically for statically
    * initialised condition variables (via PTHREAD_OBJECT_INITIALIZER).
    *
    * Note that by not providing this synchronisation we risk
    * introducing race conditions into applications which are
    * correctly written.
    *
    * Approach
    * --------
    * We know that static condition variables will not be PROCESS_SHARED
    * so we can serialise access to internal state using
    * Win32 Critical Sections rather than Win32 Mutexes.
    *
    * If using a single global lock slows applications down too much,
    * multiple global locks could be created and hashed on some random
    * value associated with each mutex, the pointer perhaps. At a guess,
    * a good value for the optimal number of global locks might be
    * the number of processors + 1.
    *
    */


   pte_osMutexLock (pte_cond_test_init_lock);

   /*
    * We got here possibly under race
    * conditions. Check again inside the critical section.
    * If a static cv has been destroyed, the application can
    * re-initialise it only by calling pthread_cond_init()
    * explicitly.
    */
   if (*cond == PTHREAD_COND_INITIALIZER)
      result = pthread_cond_init (cond, NULL);
   /*
    * The cv has been destroyed while we were waiting to
    * initialise it, so the operation that caused the
    * auto-initialisation should fail.
    */
   else if (*cond == NULL)
      result = EINVAL;


   pte_osMutexUnlock(pte_cond_test_init_lock);

   return result;
}

int pte_thread_detach_and_exit_np(void)
{
   return pte_thread_detach_common(1);
}

int pte_thread_detach_np(void)
{
   return pte_thread_detach_common(0);
}

/*
 * pte_getprocessors()
 *
 * Get the number of CPUs available to the process.
 *
 * If the available number of CPUs is 1 then pthread_spin_lock()
 * will block rather than spin if the lock is already owned.
 *
 * pthread_spin_init() calls this routine when initialising
 * a spinlock. If the number of available processors changes
 * (after a call to SetProcessAffinityMask()) then only
 * newly initialised spinlocks will notice.
 */
int pte_getprocessors (int *count)
{
   int result = 0;

   *count = 1;

   return (result);
}

int pte_is_attr (const pthread_attr_t * attr)
{
  /* Return 0 if the attr object is valid, non-zero otherwise. */

  return (attr == NULL ||
          *attr == NULL || (*attr)->valid != PTE_ATTR_VALID);
}

int pte_mutex_check_need_init (pthread_mutex_t * mutex)
{
   register int result = 0;
   register pthread_mutex_t mtx;

   /*
    * The following guarded test is specifically for statically
    * initialised mutexes (via PTHREAD_MUTEX_INITIALIZER).
    *
    * Note that by not providing this synchronisation we risk
    * introducing race conditions into applications which are
    * correctly written.
    *
    * Approach
    * --------
    * We know that static mutexes will not be PROCESS_SHARED
    * so we can serialise access to internal state using
    * critical sections rather than mutexes.
    *
    * If using a single global lock slows applications down too much,
    * multiple global locks could be created and hashed on some random
    * value associated with each mutex, the pointer perhaps. At a guess,
    * a good value for the optimal number of global locks might be
    * the number of processors + 1.
    *
    */


   pte_osMutexLock (pte_mutex_test_init_lock);

   /*
    * We got here possibly under race
    * conditions. Check again inside the critical section
    * and only initialise if the mutex is valid (not been destroyed).
    * If a static mutex has been destroyed, the application can
    * re-initialise it only by calling pthread_mutex_init()
    * explicitly.
    */
   mtx = *mutex;

   if (mtx == PTHREAD_MUTEX_INITIALIZER)
      result = pthread_mutex_init (mutex, NULL);
   else if (mtx == PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
      result = pthread_mutex_init (mutex, &pte_recursive_mutexattr);
   else if (mtx == PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
      result = pthread_mutex_init (mutex, &pte_errorcheck_mutexattr);
   /*
    * The mutex has been destroyed while we were waiting to
    * initialise it, so the operation that caused the
    * auto-initialisation should fail.
    */
   else if (mtx == NULL)
      result = EINVAL;

   pte_osMutexUnlock(pte_mutex_test_init_lock);

   return (result);
}

pthread_t pte_new (void)
{
   pthread_t nil     = NULL;
   pte_thread_t * tp = NULL;

   /*
    * If there's a reusable pthread_t then use it.
    */
   pthread_t t = pte_threadReusePop ();

   if (NULL != t)
      tp = (pte_thread_t *) t;
   else
   {
      /* No reuse threads available */
      tp = (pte_thread_t *) calloc (1, sizeof(pte_thread_t));

      if (tp == NULL)
         return nil;

      /* ptHandle.p needs to point to it's parent pte_thread_t. */
      t = tp;
      tp->x = 0;
   }

   /* Set default state. */
   tp->sched_priority = pte_osThreadGetMinPriority();

   tp->detachState = PTHREAD_CREATE_JOINABLE;
   tp->cancelState = PTHREAD_CANCEL_ENABLE;
   tp->cancelType = PTHREAD_CANCEL_DEFERRED;
   tp->cancelLock = PTHREAD_MUTEX_INITIALIZER;
   tp->threadLock = PTHREAD_MUTEX_INITIALIZER;

   return t;

}

unsigned int pte_relmillisecs (const struct timespec * abstime)
{
   const long long NANOSEC_PER_MILLISEC = 1000000;
   const long long MILLISEC_PER_SEC = 1000;
   unsigned int milliseconds;
   long  tmpCurrMilliseconds;
   struct timeb currSysTime;

   /*
    * Calculate timeout as milliseconds from current system time.
    */

   /*
    * subtract current system time from abstime in a way that checks
    * that abstime is never in the past, or is never equivalent to the
    * defined INFINITE value (0xFFFFFFFF).
    *
    * Assume all integers are unsigned, i.e. cannot test if less than 0.
    */
   long long tmpAbsMilliseconds =  (int64_t)abstime->tv_sec * MILLISEC_PER_SEC;
   tmpAbsMilliseconds += ((int64_t)abstime->tv_nsec + (NANOSEC_PER_MILLISEC/2)) / NANOSEC_PER_MILLISEC;

   /* get current system time */

   _ftime(&currSysTime);

   tmpCurrMilliseconds = (int64_t) currSysTime.time * MILLISEC_PER_SEC;
   tmpCurrMilliseconds += (int64_t) currSysTime.millitm;

   if (tmpAbsMilliseconds > tmpCurrMilliseconds)
   {
      milliseconds = (unsigned int) (tmpAbsMilliseconds - tmpCurrMilliseconds);
      /* Timeouts must be finite */
      if (milliseconds == 0xFFFFFFFF)
         milliseconds--;
   }
   /* The abstime given is in the past */
   else
      milliseconds = 0;

   return milliseconds;
}

/*
 * How it works:
 * A pthread_t is a struct which is normally passed/returned by
 * value to/from pthreads routines.  Applications are therefore storing
 * a copy of the struct as it is at that time.
 *
 * The original pthread_t struct plus all copies of it contain the address of
 * the thread state struct pte_thread_t_ (p), plus a reuse counter (x). Each
 * pte_thread_t contains the original copy of it's pthread_t.
 * Once malloced, a pte_thread_t_ struct is not freed until the process exits.
 *
 * The thread reuse stack is a simple LILO stack managed through a singly
 * linked list element in the pte_thread_t.
 *
 * Each time a thread is destroyed, the pte_thread_t address is pushed onto the
 * reuse stack after it's ptHandle's reuse counter has been incremented.
 *
 * The following can now be said from this:
 * - two pthread_t's are identical if their pte_thread_t reference pointers
 * are equal and their reuse counters are equal. That is,
 *
 *   equal = (a.p == b.p && a.x == b.x)
 *
 * - a pthread_t copy refers to a destroyed thread if the reuse counter in
 * the copy is not equal to the reuse counter in the original.
 *
 *   threadDestroyed = (copy.x != ((pte_thread_t *)copy.p)->ptHandle.x)
 *
 */

/*
 * Pop a clean pthread_t struct off the reuse stack.
 */
   pthread_t
pte_threadReusePop (void)
{
   pthread_t t = NULL;

   pte_osMutexLock (pte_thread_reuse_lock);

   if (PTE_THREAD_REUSE_EMPTY != pte_threadReuseTop)
   {
      pte_thread_t * tp;

      tp = pte_threadReuseTop;

      pte_threadReuseTop = tp->prevReuse;

      if (PTE_THREAD_REUSE_EMPTY == pte_threadReuseTop)
         pte_threadReuseBottom = PTE_THREAD_REUSE_EMPTY;

      tp->prevReuse = NULL;

      t = tp;
   }

   pte_osMutexUnlock(pte_thread_reuse_lock);

   return t;

}

/*
 * Push a clean pthread_t struct onto the reuse stack.
 * Must be re-initialised when reused.
 * All object elements (mutexes, events etc) must have been either
 * detroyed before this, or never initialised.
 */
void pte_threadReusePush (pthread_t thread)
{
   pte_thread_t * tp = (pte_thread_t *) thread;
   pthread_t t;

   pte_osMutexLock (pte_thread_reuse_lock);

   t = tp;
   memset(tp, 0, sizeof(pte_thread_t));

   /* Must restore the original POSIX handle that we just wiped. */
   tp = t;

   /* Bump the reuse counter now */
#ifdef PTE_THREAD_ID_REUSE_INCREMENT
   tp->x += PTE_THREAD_ID_REUSE_INCREMENT;
#else
   tp->x++;
#endif

   tp->prevReuse = PTE_THREAD_REUSE_EMPTY;

   if (PTE_THREAD_REUSE_EMPTY != pte_threadReuseBottom)
      pte_threadReuseBottom->prevReuse = tp;
   else
      pte_threadReuseTop = tp;

   pte_threadReuseBottom = tp;

   pte_osMutexUnlock(pte_thread_reuse_lock);
}

void pte_rwlock_cancelwrwait (void *arg)
{
  pthread_rwlock_t rwl = (pthread_rwlock_t) arg;

  rwl->nSharedAccessCount = -rwl->nCompletedSharedAccessCount;
  rwl->nCompletedSharedAccessCount = 0;

  (void) pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted));
  (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
}

int pte_rwlock_check_need_init (pthread_rwlock_t * rwlock)
{
   int result = 0;

   /*
    * The following guarded test is specifically for statically
    * initialised rwlocks (via PTHREAD_RWLOCK_INITIALIZER).
    *
    * Note that by not providing this synchronisation we risk
    * introducing race conditions into applications which are
    * correctly written.
    *
    * Approach
    * --------
    * We know that static rwlocks will not be PROCESS_SHARED
    * so we can serialise access to internal state using
    * critical sections rather than mutexes.
    *
    * If using a single global lock slows applications down too much,
    * multiple global locks could be created and hashed on some random
    * value associated with each mutex, the pointer perhaps. At a guess,
    * a good value for the optimal number of global locks might be
    * the number of processors + 1.
    *
    */


   pte_osMutexLock (pte_rwlock_test_init_lock);

   /*
    * We got here possibly under race
    * conditions. Check again inside the critical section
    * and only initialise if the rwlock is valid (not been destroyed).
    * If a static rwlock has been destroyed, the application can
    * re-initialise it only by calling pthread_rwlock_init()
    * explicitly.
    */
   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
      result = pthread_rwlock_init (rwlock, NULL);
   /*
    * The rwlock has been destroyed while we were waiting to
    * initialise it, so the operation that caused the
    * auto-initialisation should fail.
    */
   else if (*rwlock == NULL)
      result = EINVAL;

   pte_osMutexUnlock(pte_rwlock_test_init_lock);

   return result;
}

int pte_spinlock_check_need_init (pthread_spinlock_t * lock)
{
   int result = 0;

   /*
    * The following guarded test is specifically for statically
    * initialised spinlocks (via PTHREAD_SPINLOCK_INITIALIZER).
    *
    * Note that by not providing this synchronisation we risk
    * introducing race conditions into applications which are
    * correctly written.
    */


   pte_osMutexLock (pte_spinlock_test_init_lock);

   /*
    * We got here possibly under race
    * conditions. Check again inside the critical section
    * and only initialise if the spinlock is valid (not been destroyed).
    * If a static spinlock has been destroyed, the application can
    * re-initialise it only by calling pthread_spin_init()
    * explicitly.
    */
   if (*lock == PTHREAD_SPINLOCK_INITIALIZER)
      result = pthread_spin_init (lock, PTHREAD_PROCESS_PRIVATE);
   /*
    * The spinlock has been destroyed while we were waiting to
    * initialise it, so the operation that caused the
    * auto-initialisation should fail.
    */
   else if (*lock == NULL)
      result = EINVAL;

   pte_osMutexUnlock(pte_spinlock_test_init_lock);

   return (result);
}

void pte_threadDestroy (pthread_t thread)
{
  pte_threadDestroyCommon(thread,0);
}

void pte_threadExitAndDestroy (pthread_t thread)
{
  pte_threadDestroyCommon(thread,1);
}

#include <setjmp.h>

int pte_threadStart (void *vthreadParms)
{
   ThreadParms * threadParms = (ThreadParms *) vthreadParms;
   void *(*start) (void *);
   void * arg;


   int setjmp_rc;

   void * status    = (void *) 0;

   pthread_t self   = threadParms->tid;
   pte_thread_t *sp = (pte_thread_t *) self;
   start = threadParms->start;
   arg = threadParms->arg;

#if 0
   free (threadParms);
#endif

   pthread_setspecific (pte_selfThreadKey, sp);

   sp->state = PThreadStateRunning;

   setjmp_rc = setjmp (sp->start_mark);


   /*
    * Run the caller's routine;
    */
   if (0 == setjmp_rc)
      sp->exitStatus = status = (*start) (arg);
   else
   {
      switch (setjmp_rc)
      {
         case PTE_EPS_CANCEL:
            status = sp->exitStatus = PTHREAD_CANCELED;
            break;
         case PTE_EPS_EXIT:
            status = sp->exitStatus;
            break;
         default:
            status = sp->exitStatus = PTHREAD_CANCELED;
            break;
      }
   }

   /*
    * We need to cleanup the pthread now if we have
    * been statically linked, in which case the cleanup
    * in dllMain won't get done. Joinable threads will
    * only be partially cleaned up and must be fully cleaned
    * up by pthread_join() or pthread_detach().
    *
    * Note: if this library has been statically linked,
    * implicitly created pthreads (those created
    * for OS threads which have called pthreads routines)
    * must be cleaned up explicitly by the application
    * (by calling pte_thread_detach_np()).
    */
   (void) pte_thread_detach_and_exit_np ();

   //pte_osThreadExit(status);

   /*
    * Never reached.
    */

   return (unsigned) status;
}


/*
 * pte_throw
 *
 * All canceled and explicitly exited POSIX threads go through
 * here. This routine knows how to exit both POSIX initiated threads and
 * 'implicit' POSIX threads for each of the possible language modes (C,
 * C++).
 */
void pte_throw (unsigned int exception)
{
   /*
    * Don't use pthread_self() to avoid creating an implicit POSIX thread handle
    * unnecessarily.
    */
   pte_thread_t * sp = (pte_thread_t *) pthread_getspecific (pte_selfThreadKey);


   /* Should never enter here */
   if (exception != PTE_EPS_CANCEL && exception != PTE_EPS_EXIT)
      exit (1);

   if (NULL == sp || sp->implicit)
   {
      /*
       * We're inside a non-POSIX initialised OS thread
       * so there is no point to jump or throw back to. Just do an
       * explicit thread exit here after cleaning up POSIX
       * residue (i.e. cleanup handlers, POSIX thread handle etc).
       */
      unsigned exitCode = 0;

      switch (exception)
      {
         case PTE_EPS_CANCEL:
            exitCode = (unsigned) PTHREAD_CANCELED;
            break;
         case PTE_EPS_EXIT:
            exitCode = (unsigned) sp->exitStatus;;
            break;
      }

      pte_thread_detach_and_exit_np ();

#if 0
      pte_osThreadExit((void*)exitCode);
#endif
   }

   pte_pop_cleanup_all (1);
   longjmp (sp->start_mark, exception);

   /* Never reached */
}

void pte_pop_cleanup_all (int execute)
{
   while (NULL != pte_pop_cleanup (execute)) { }
}

unsigned int pte_get_exception_services_code (void)
{
  return (unsigned int) NULL;
}

int pte_tkAssocCreate (pte_thread_t * sp, pthread_key_t key)
   /*
    * -------------------------------------------------------------------
    * This routine creates an association that
    * is unique for the given (thread,key) combination.The association
    * is referenced by both the thread and the key.
    * This association allows us to determine what keys the
    * current thread references and what threads a given key
    * references.
    * See the detailed description
    * at the beginning of this file for further details.
    *
    * Notes:
    *      1)      New associations are pushed to the beginning of the
    *              chain so that the internal pte_selfThreadKey association
    *              is always last, thus allowing selfThreadExit to
    *              be implicitly called last by pthread_exit.
    *      2)
    *
    * Parameters:
    *              thread
    *                      current running thread.
    *              key
    *                      key on which to create an association.
    * Returns:
    *       0              - if successful,
    *       ENOMEM         - not enough memory to create assoc or other object
    *       EINVAL         - an internal error occurred
    *       ENOSYS         - an internal error occurred
    * -------------------------------------------------------------------
    */
{
   ThreadKeyAssoc *assoc;

   /*
    * Have to create an association and add it
    * to both the key and the thread.
    *
    * Both key->keyLock and thread->threadLock are locked on
    * entry to this routine.
    */
   assoc = (ThreadKeyAssoc *) calloc (1, sizeof (*assoc));

   if (assoc == NULL)
      return ENOMEM;

   assoc->thread = sp;
   assoc->key    = key;

   /*
    * Register assoc with key
    */
   assoc->prevThread = NULL;
   assoc->nextThread = (ThreadKeyAssoc *) key->threads;
   if (assoc->nextThread)
      assoc->nextThread->prevThread = assoc;
   key->threads = (void *) assoc;

   /*
    * Register assoc with thread
    */
   assoc->prevKey = NULL;
   assoc->nextKey = (ThreadKeyAssoc *) sp->keys;
   if (assoc->nextKey)
      assoc->nextKey->prevKey = assoc;
   sp->keys = (void *) assoc;

   return (0);
}

void pte_tkAssocDestroy (ThreadKeyAssoc * assoc)
   /*
    * -------------------------------------------------------------------
    * This routine releases all resources for the given ThreadKeyAssoc
    * once it is no longer being referenced
    * ie) either the key or thread has stopped referencing it.
    *
    * Parameters:
    *              assoc
    *                      an instance of ThreadKeyAssoc.
    * Returns:
    *      N/A
    * -------------------------------------------------------------------
    */
{
   ThreadKeyAssoc *prev = NULL;
   ThreadKeyAssoc *next = NULL;
   /*
    * Both key->keyLock and thread->threadLock are locked on
    * entry to this routine.
    */
   if (!assoc)
      return;

   /* Remove assoc from thread's keys chain */
   prev = assoc->prevKey;
   next = assoc->nextKey;
   if (prev)
      prev->nextKey = next;
   if (next)
      next->prevKey = prev;

   /* We're at the head of the thread's keys chain */
   if (assoc->thread->keys == assoc)
      assoc->thread->keys = next;

   if (assoc->thread->nextAssoc == assoc)
   {
      /*
       * Thread is exiting and we're deleting the assoc to be processed next.
       * Hand thread the assoc after this one.
       */
      assoc->thread->nextAssoc = next;
   }

   /* Remove assoc from key's threads chain */
   prev = assoc->prevThread;
   next = assoc->nextThread;
   if (prev)
      prev->nextThread = next;
   if (next)
      next->prevThread = prev;

   /* We're at the head of the key's threads chain */
   if (assoc->key->threads == assoc)
      assoc->key->threads = next;

   free (assoc);
}
