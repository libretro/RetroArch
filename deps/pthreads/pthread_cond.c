/*
 * pthread_cond.c
 *
 * Description:
 * This translation unit implements condition variables and their primitives.
 *
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

#include <stdlib.h>

#include "pthread.h"
#include "implement.h"

/*
 * Arguments for cond_wait_cleanup, since we can only pass a
 * single void * to it.
 */
typedef struct
{
   pthread_mutex_t *mutexPtr;
   pthread_cond_t cv;
   int *resultPtr;
} pte_cond_wait_cleanup_args_t;

static void pte_cond_wait_cleanup (void *args)
{
   pte_cond_wait_cleanup_args_t *cleanup_args =
      (pte_cond_wait_cleanup_args_t *) args;
   pthread_cond_t cv = cleanup_args->cv;
   int *resultPtr = cleanup_args->resultPtr;
   int nSignalsWasLeft;
   int result;

   /*
    * Whether we got here as a result of signal/broadcast or because of
    * timeout on wait or thread cancellation we indicate that we are no
    * longer waiting. The waiter is responsible for adjusting waiters
    * (to)unblock(ed) counts (protected by unblock lock).
    */
   if ((result = pthread_mutex_lock (&(cv->mtxUnblockLock))) != 0)
   {
      *resultPtr = result;
      return;
   }

   if (0 != (nSignalsWasLeft = cv->nWaitersToUnblock))
   {
      --(cv->nWaitersToUnblock);
   }
   else if (INT_MAX / 2 == ++(cv->nWaitersGone))
   {
      /* Use the non-cancellable version of sem_wait() */
      //      if (sem_wait_nocancel (&(cv->semBlockLock)) != 0)
      if (sem_wait (&(cv->semBlockLock)) != 0)
      {
         *resultPtr = errno;
         /*
          * This is a fatal error for this CV,
          * so we deliberately don't unlock
          * cv->mtxUnblockLock before returning.
          */
         return;
      }
      cv->nWaitersBlocked -= cv->nWaitersGone;
      if (sem_post (&(cv->semBlockLock)) != 0)
      {
         *resultPtr = errno;
         /*
          * This is a fatal error for this CV,
          * so we deliberately don't unlock
          * cv->mtxUnblockLock before returning.
          */
         return;
      }
      cv->nWaitersGone = 0;
   }

   if ((result = pthread_mutex_unlock (&(cv->mtxUnblockLock))) != 0)
   {
      *resultPtr = result;
      return;
   }

   if (1 == nSignalsWasLeft)
   {
      if (sem_post (&(cv->semBlockLock)) != 0)
      {
         *resultPtr = errno;
         return;
      }
   }

   /*
    * XSH: Upon successful return, the mutex has been locked and is owned
    * by the calling thread.
    */
   if ((result = pthread_mutex_lock (cleanup_args->mutexPtr)) != 0)
      *resultPtr = result;
}

static int pte_cond_timedwait (pthread_cond_t * cond,
      pthread_mutex_t * mutex, const struct timespec *abstime)
{
   int result = 0;
   pthread_cond_t cv;
   pte_cond_wait_cleanup_args_t cleanup_args;

   if (cond == NULL || *cond == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static condition variable. We check
    * again inside the guarded section of pte_cond_check_need_init()
    * to avoid race conditions.
    */
   if (*cond == PTHREAD_COND_INITIALIZER)
      result = pte_cond_check_need_init (cond);

   if (result != 0 && result != EBUSY)
      return result;

   cv = *cond;

   /* Thread can be cancelled in sem_wait() but this is OK */
   if (sem_wait (&(cv->semBlockLock)) != 0)
      return errno;

   ++(cv->nWaitersBlocked);

   if (sem_post (&(cv->semBlockLock)) != 0)
      return errno;

   /*
    * Setup this waiter cleanup handler
    */
   cleanup_args.mutexPtr = mutex;
   cleanup_args.cv = cv;
   cleanup_args.resultPtr = &result;

   pthread_cleanup_push (pte_cond_wait_cleanup, (void *) &cleanup_args);

   /*
    * Now we can release 'mutex' and...
    */
   if ((result = pthread_mutex_unlock (mutex)) == 0)
   {
      /*
       * ...wait to be awakened by
       *              pthread_cond_signal, or
       *              pthread_cond_broadcast, or
       *              timeout, or
       *              thread cancellation
       *
       * Note:
       *
       *      sem_timedwait is a cancellation point,
       *      hence providing the mechanism for making
       *      pthread_cond_wait a cancellation point.
       *      We use the cleanup mechanism to ensure we
       *      re-lock the mutex and adjust (to)unblock(ed) waiters
       *      counts if we are cancelled, timed out or signalled.
       */
      if (sem_timedwait (&(cv->semBlockQueue), abstime) != 0)
         result = errno;
   }


   /*
    * Always cleanup
    */
   pthread_cleanup_pop (1);

   /*
    * "result" can be modified by the cleanup handler.
    */
   return result;
}

int
pthread_cond_destroy (pthread_cond_t * cond)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function destroys a condition variable
 *
 *
 * PARAMETERS
 *      cond
 *              pointer to an instance of pthread_cond_t
 *
 *
 * DESCRIPTION
 *      This function destroys a condition variable.
 *
 *      NOTES:
 *              1)      A condition variable can be destroyed
 *                      immediately after all the threads that
 *                      are blocked on it are awakened. e.g.
 *
 *                      struct list {
 *                        pthread_mutex_t lm;
 *                        ...
 *                      }
 *
 *                      struct elt {
 *                        key k;
 *                        int busy;
 *                        pthread_cond_t notbusy;
 *                        ...
 *                      }
 *
 *
 *                      struct elt *
 *                      list_find(struct list *lp, key k)
 *                      {
 *                        struct elt *ep;
 *
 *                        pthread_mutex_lock(&lp->lm);
 *                        while ((ep = find_elt(l,k) != NULL) && ep->busy)
 *                          pthread_cond_wait(&ep->notbusy, &lp->lm);
 *                        if (ep != NULL)
 *                          ep->busy = 1;
 *                        pthread_mutex_unlock(&lp->lm);
 *                        return(ep);
 *                      }
 *
 *                      delete_elt(struct list *lp, struct elt *ep)
 *                      {
 *                        pthread_mutex_lock(&lp->lm);
 *                        assert(ep->busy);
 *                        ... remove ep from list ...
 *                        ep->busy = 0;
 *                    (A) pthread_cond_broadcast(&ep->notbusy);
 *                        pthread_mutex_unlock(&lp->lm);
 *                    (B) pthread_cond_destroy(&rp->notbusy);
 *                        free(ep);
 *                      }
 *
 *                      In this example, the condition variable
 *                      and its list element may be freed (line B)
 *                      immediately after all threads waiting for
 *                      it are awakened (line A), since the mutex
 *                      and the code ensure that no other thread
 *                      can touch the element to be deleted.
 *
 * RESULTS
 *              0               successfully released condition variable,
 *              EINVAL          'cond' is invalid,
 *              EBUSY           'cond' is in use,
 *
 * ------------------------------------------------------
 */
{
   pthread_cond_t cv;
   int result = 0, result1 = 0, result2 = 0;

   /*
    * Assuming any race condition here is harmless.
    */
   if (cond == NULL || *cond == NULL)
      return EINVAL;

   if (*cond != PTHREAD_COND_INITIALIZER)
   {

      pte_osMutexLock (pte_cond_list_lock);

      cv = *cond;

      /*
       * Close the gate; this will synchronize this thread with
       * all already signaled waiters to let them retract their
       * waiter status - SEE NOTE 1 ABOVE!!!
       */
      if (sem_wait (&(cv->semBlockLock)) != 0)
         return errno;

      /*
       * !TRY! lock mtxUnblockLock; try will detect busy condition
       * and will not cause a deadlock with respect to concurrent
       * signal/broadcast.
       */
      if ((result = pthread_mutex_trylock (&(cv->mtxUnblockLock))) != 0)
      {
         (void) sem_post (&(cv->semBlockLock));
         return result;
      }

      /*
       * Check whether cv is still busy (still has waiters)
       */
      if (cv->nWaitersBlocked > cv->nWaitersGone)
      {
         if (sem_post (&(cv->semBlockLock)) != 0)
            result = errno;
         result1 = pthread_mutex_unlock (&(cv->mtxUnblockLock));
         result2 = EBUSY;
      }
      else
      {
         /*
          * Now it is safe to destroy
          */
         *cond = NULL;

         if (sem_destroy (&(cv->semBlockLock)) != 0)
            result = errno;
         if (sem_destroy (&(cv->semBlockQueue)) != 0)
            result1 = errno;
         if ((result2 = pthread_mutex_unlock (&(cv->mtxUnblockLock))) == 0)
            result2 = pthread_mutex_destroy (&(cv->mtxUnblockLock));

         /* Unlink the CV from the list */

         if (pte_cond_list_head == cv)
            pte_cond_list_head = cv->next;
         else
            cv->prev->next = cv->next;

         if (pte_cond_list_tail == cv)
            pte_cond_list_tail = cv->prev;
         else
            cv->next->prev = cv->prev;

         (void) free (cv);
      }

      pte_osMutexUnlock(pte_cond_list_lock);

   }
   else
   {
      /*
       * See notes in pte_cond_check_need_init() above also.
       */

      pte_osMutexLock (pte_cond_test_init_lock);

      /*
       * Check again.
       */
      if (*cond == PTHREAD_COND_INITIALIZER)
      {
         /*
          * This is all we need to do to destroy a statically
          * initialised cond that has not yet been used (initialised).
          * If we get to here, another thread waiting to initialise
          * this cond will get an EINVAL. That's OK.
          */
         *cond = NULL;
      }
      else
      {
         /*
          * The cv has been initialised while we were waiting
          * so assume it's in use.
          */
         result = EBUSY;
      }

      pte_osMutexUnlock(pte_cond_test_init_lock);
   }

   return ((result != 0) ? result : ((result1 != 0) ? result1 : result2));
}

int pthread_cond_init (pthread_cond_t * cond, const pthread_condattr_t * attr)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function initializes a condition variable.
    *
    * PARAMETERS
    *      cond
    *              pointer to an instance of pthread_cond_t
    *
    *      attr
    *              specifies optional creation attributes.
    *
    *
    * DESCRIPTION
    *      This function initializes a condition variable.
    *
    * RESULTS
    *              0               successfully created condition variable,
    *              EINVAL          'attr' is invalid,
    *              EAGAIN          insufficient resources (other than
    *                              memory,
    *              ENOMEM          insufficient memory,
    *              EBUSY           'cond' is already initialized,
    *
    * ------------------------------------------------------
    */
{
   int result;
   pthread_cond_t cv = NULL;

   if (cond == NULL)
      return EINVAL;

   if ((attr != NULL && *attr != NULL) &&
         ((*attr)->pshared == PTHREAD_PROCESS_SHARED))
   {
      /*
       * Creating condition variable that can be shared between
       * processes.
       */
      result = ENOSYS;
      goto DONE;
   }

   cv = (pthread_cond_t) calloc (1, sizeof (*cv));

   if (cv == NULL)
   {
      result = ENOMEM;
      goto DONE;
   }

   cv->nWaitersBlocked = 0;
   cv->nWaitersToUnblock = 0;
   cv->nWaitersGone = 0;

   if (sem_init (&(cv->semBlockLock), 0, 1) != 0)
   {
      result = errno;
      goto FAIL0;
   }

   if (sem_init (&(cv->semBlockQueue), 0, 0) != 0)
   {
      result = errno;
      goto FAIL1;
   }

   if ((result = pthread_mutex_init (&(cv->mtxUnblockLock), 0)) != 0)
   {
      goto FAIL2;
   }

   result = 0;

   goto DONE;

   /*
    * -------------
    * Failed...
    * -------------
    */
FAIL2:
   (void) sem_destroy (&(cv->semBlockQueue));

FAIL1:
   (void) sem_destroy (&(cv->semBlockLock));

FAIL0:
   (void) free (cv);
   cv = NULL;

DONE:
   if (0 == result)
   {

      pte_osMutexLock (pte_cond_list_lock);

      cv->next = NULL;
      cv->prev = pte_cond_list_tail;

      if (pte_cond_list_tail != NULL)
         pte_cond_list_tail->next = cv;

      pte_cond_list_tail = cv;

      if (pte_cond_list_head == NULL)
         pte_cond_list_head = cv;

      pte_osMutexUnlock(pte_cond_list_lock);
   }

   *cond = cv;

   return result;
}

static int pte_cond_unblock (pthread_cond_t * cond, int unblockAll)
   /*
    * Notes.
    *
    * Does not use the external mutex for synchronisation,
    * therefore semBlockLock is needed.
    * mtxUnblockLock is for LEVEL-2 synch. LEVEL-2 is the
    * state where the external mutex is not necessarily locked by
    * any thread, ie. between cond_wait unlocking and re-acquiring
    * the lock after having been signaled or a timeout or
    * cancellation.
    *
    * Uses the following CV elements:
    *   nWaitersBlocked
    *   nWaitersToUnblock
    *   nWaitersGone
    *   mtxUnblockLock
    *   semBlockLock
    *   semBlockQueue
    */
{
   int result;
   pthread_cond_t cv;
   int nSignalsToIssue;

   if (cond == NULL || *cond == NULL)
      return EINVAL;

   cv = *cond;

   /*
    * No-op if the CV is static and hasn't been initialised yet.
    * Assuming that any race condition is harmless.
    */
   if (cv == PTHREAD_COND_INITIALIZER)
      return 0;

   if ((result = pthread_mutex_lock (&(cv->mtxUnblockLock))) != 0)
      return result;

   if (0 != cv->nWaitersToUnblock)
   {
      if (0 == cv->nWaitersBlocked)
         return pthread_mutex_unlock (&(cv->mtxUnblockLock));

      if (unblockAll)
      {
         cv->nWaitersToUnblock += (nSignalsToIssue = cv->nWaitersBlocked);
         cv->nWaitersBlocked = 0;
      }
      else
      {
         nSignalsToIssue = 1;
         cv->nWaitersToUnblock++;
         cv->nWaitersBlocked--;
      }
   }
   else if (cv->nWaitersBlocked > cv->nWaitersGone)
   {
      /* Use the non-cancellable version of sem_wait() */
      //      if (sem_wait_nocancel (&(cv->semBlockLock)) != 0)
      if (sem_wait (&(cv->semBlockLock)) != 0)
      {
         result = errno;
         (void) pthread_mutex_unlock (&(cv->mtxUnblockLock));
         return result;
      }
      if (0 != cv->nWaitersGone)
      {
         cv->nWaitersBlocked -= cv->nWaitersGone;
         cv->nWaitersGone = 0;
      }
      if (unblockAll)
      {
         nSignalsToIssue = cv->nWaitersToUnblock = cv->nWaitersBlocked;
         cv->nWaitersBlocked = 0;
      }
      else
      {
         nSignalsToIssue = cv->nWaitersToUnblock = 1;
         cv->nWaitersBlocked--;
      }
   }
   else
      return pthread_mutex_unlock (&(cv->mtxUnblockLock));

   if ((result = pthread_mutex_unlock (&(cv->mtxUnblockLock))) == 0)
   {
      if (sem_post_multiple (&(cv->semBlockQueue), nSignalsToIssue) != 0)
         result = errno;
   }

   return result;
}

int pthread_cond_signal (pthread_cond_t * cond)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function signals a condition variable, waking
    *      one waiting thread.
    *      If SCHED_FIFO or SCHED_RR policy threads are waiting
    *      the highest priority waiter is awakened; otherwise,
    *      an unspecified waiter is awakened.
    *
    * PARAMETERS
    *      cond
    *              pointer to an instance of pthread_cond_t
    *
    *
    * DESCRIPTION
    *      This function signals a condition variable, waking
    *      one waiting thread.
    *      If SCHED_FIFO or SCHED_RR policy threads are waiting
    *      the highest priority waiter is awakened; otherwise,
    *      an unspecified waiter is awakened.
    *
    *      NOTES:
    *
    *      1)      Use when any waiter can respond and only one need
    *              respond (all waiters being equal).
    *
    * RESULTS
    *              0               successfully signaled condition,
    *              EINVAL          'cond' is invalid,
    *
    * ------------------------------------------------------
    */
{
   /*
    * The '0'(FALSE) unblockAll arg means unblock ONE waiter.
    */
   return (pte_cond_unblock (cond, 0));

}				/* pthread_cond_signal */

   int
pthread_cond_broadcast (pthread_cond_t * cond)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function broadcasts the condition variable,
    *      waking all current waiters.
    *
    * PARAMETERS
    *      cond
    *              pointer to an instance of pthread_cond_t
    *
    *
    * DESCRIPTION
    *      This function signals a condition variable, waking
    *      all waiting threads.
    *
    *      NOTES:
    *
    *      1)      Use when more than one waiter may respond to
    *              predicate change or if any waiting thread may
    *              not be able to respond
    *
    * RESULTS
    *              0               successfully signalled condition to all
    *                              waiting threads,
    *              EINVAL          'cond' is invalid
    *              ENOSPC          a required resource has been exhausted,
    *
    * ------------------------------------------------------
    */
{
   /*
    * The TRUE unblockAll arg means unblock ALL waiters.
    */
   return (pte_cond_unblock (cond, PTE_TRUE));
}

int pthread_cond_wait (pthread_cond_t * cond, pthread_mutex_t * mutex)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function waits on a condition variable until
    *      awakened by a signal or broadcast.
    *
    *      Caller MUST be holding the mutex lock; the
    *      lock is released and the caller is blocked waiting
    *      on 'cond'. When 'cond' is signaled, the mutex
    *      is re-acquired before returning to the caller.
    *
    * PARAMETERS
    *      cond
    *              pointer to an instance of pthread_cond_t
    *
    *      mutex
    *              pointer to an instance of pthread_mutex_t
    *
    *
    * DESCRIPTION
    *      This function waits on a condition variable until
    *      awakened by a signal or broadcast.
    *
    *      NOTES:
    *
    *      1)      The function must be called with 'mutex' LOCKED
    *              by the calling thread, or undefined behaviour
    *              will result.
    *
    *      2)      This routine atomically releases 'mutex' and causes
    *              the calling thread to block on the condition variable.
    *              The blocked thread may be awakened by
    *                      pthread_cond_signal or
    *                      pthread_cond_broadcast.
    *
    * Upon successful completion, the 'mutex' has been locked and
    * is owned by the calling thread.
    *
    *
    * RESULTS
    *              0               caught condition; mutex released,
    *              EINVAL          'cond' or 'mutex' is invalid,
    *              EINVAL          different mutexes for concurrent waits,
    *              EINVAL          mutex is not held by the calling thread,
    *
    * ------------------------------------------------------
    */
{
   /*
    * The NULL abstime arg means INFINITE waiting.
    */
   return (pte_cond_timedwait (cond, mutex, NULL));

}				/* pthread_cond_wait */


   int
pthread_cond_timedwait (pthread_cond_t * cond,
      pthread_mutex_t * mutex,
      const struct timespec *abstime)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function waits on a condition variable either until
    *      awakened by a signal or broadcast; or until the time
    *      specified by abstime passes.
    *
    * PARAMETERS
    *      cond
    *              pointer to an instance of pthread_cond_t
    *
    *      mutex
    *              pointer to an instance of pthread_mutex_t
    *
    *      abstime
    *              pointer to an instance of (const struct timespec)
    *
    *
    * DESCRIPTION
    *      This function waits on a condition variable either until
    *      awakened by a signal or broadcast; or until the time
    *      specified by abstime passes.
    *
    *      NOTES:
    *      1)      The function must be called with 'mutex' LOCKED
    *              by the calling thread, or undefined behaviour
    *              will result.
    *
    *      2)      This routine atomically releases 'mutex' and causes
    *              the calling thread to block on the condition variable.
    *              The blocked thread may be awakened by
    *                      pthread_cond_signal or
    *                      pthread_cond_broadcast.
    *
    *
    * RESULTS
    *              0               caught condition; mutex released,
    *              EINVAL          'cond', 'mutex', or abstime is invalid,
    *              EINVAL          different mutexes for concurrent waits,
    *              EINVAL          mutex is not held by the calling thread,
    *              ETIMEDOUT       abstime ellapsed before cond was signaled.
    *
    * ------------------------------------------------------
    */
{
   if (abstime == NULL)
   {
      return EINVAL;
   }

   return (pte_cond_timedwait (cond, mutex, abstime));
}
