/*
 * pthread_set.c
 *
 * Description:
 * POSIX thread functions related to state.
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

#include "pthread.h"
#include "implement.h"
#include "sched.h"

int pthread_setcancelstate (int state, int *oldstate)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function atomically sets the calling thread's
 *      cancelability state to 'state' and returns the previous
 *      cancelability state at the location referenced by
 *      'oldstate'
 *
 * PARAMETERS
 *      state,
 *      oldstate
 *              PTHREAD_CANCEL_ENABLE
 *                      cancellation is enabled,
 *
 *              PTHREAD_CANCEL_DISABLE
 *                      cancellation is disabled
 *
 *
 * DESCRIPTION
 *      This function atomically sets the calling thread's
 *      cancelability state to 'state' and returns the previous
 *      cancelability state at the location referenced by
 *      'oldstate'.
 *
 *      NOTES:
 *      1)      Use to disable cancellation around 'atomic' code that
 *              includes cancellation points
 *
 * COMPATIBILITY ADDITIONS
 *      If 'oldstate' is NULL then the previous state is not returned
 *      but the function still succeeds. (Solaris)
 *
 * RESULTS
 *              0               successfully set cancelability type,
 *              EINVAL          'state' is invalid
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   pthread_t self = pthread_self ();
   pte_thread_t * sp = (pte_thread_t *) self;

   if (sp == NULL
         || (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE))
      return EINVAL;

   /*
    * Lock for async-cancel safety.
    */
   (void) pthread_mutex_lock (&sp->cancelLock);

   if (oldstate != NULL)
      *oldstate = sp->cancelState;

   sp->cancelState = state;

   /*
    * Check if there is a pending asynchronous cancel
    */
   if (state == PTHREAD_CANCEL_ENABLE
         && (sp->cancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
         && (pte_osThreadCheckCancel(sp->threadId) == PTE_OS_INTERRUPTED) )
   {
      sp->state = PThreadStateCanceling;
      sp->cancelState = PTHREAD_CANCEL_DISABLE;
      (void) pthread_mutex_unlock (&sp->cancelLock);
      pte_throw (PTE_EPS_CANCEL);

      /* Never reached */
   }

   (void) pthread_mutex_unlock (&sp->cancelLock);

   return (result);
}

int pthread_setcanceltype (int type, int *oldtype)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function atomically sets the calling thread's
    *      cancelability type to 'type' and returns the previous
    *      cancelability type at the location referenced by
    *      'oldtype'
    *
    * PARAMETERS
    *      type,
    *      oldtype
    *              PTHREAD_CANCEL_DEFERRED
    *                      only deferred cancelation is allowed,
    *
    *              PTHREAD_CANCEL_ASYNCHRONOUS
    *                      Asynchronous cancellation is allowed
    *
    *
    * DESCRIPTION
    *      This function atomically sets the calling thread's
    *      cancelability type to 'type' and returns the previous
    *      cancelability type at the location referenced by
    *      'oldtype'
    *
    *      NOTES:
    *      1)      Use with caution; most code is not safe for use
    *              with asynchronous cancelability.
    *
    * COMPATIBILITY ADDITIONS
    *      If 'oldtype' is NULL then the previous type is not returned
    *      but the function still succeeds. (Solaris)
    *
    * RESULTS
    *              0               successfully set cancelability type,
    *              EINVAL          'type' is invalid
    *              EPERM           Async cancellation is not supported.
    *
    * ------------------------------------------------------
    */
{
   int result = 0;
   pthread_t self = pthread_self ();
   pte_thread_t * sp = (pte_thread_t *) self;

#ifndef PTE_SUPPORT_ASYNC_CANCEL
   if (type == PTHREAD_CANCEL_ASYNCHRONOUS)
   {
      /* Async cancellation is not supported at this time.  See notes in
       * pthread_cancel.
       */
      return EPERM;
   }
#endif /* PTE_SUPPORT_ASYNC_CANCEL */

   if (sp == NULL
         || (type != PTHREAD_CANCEL_DEFERRED
            && type != PTHREAD_CANCEL_ASYNCHRONOUS))
      return EINVAL;

   /*
    * Lock for async-cancel safety.
    */
   (void) pthread_mutex_lock (&sp->cancelLock);

   if (oldtype != NULL)
      *oldtype = sp->cancelType;

   sp->cancelType = type;

   /*
    * Check if there is a pending asynchronous cancel
    */

   if (sp->cancelState == PTHREAD_CANCEL_ENABLE
         && (type == PTHREAD_CANCEL_ASYNCHRONOUS)
         && (pte_osThreadCheckCancel(sp->threadId) == PTE_OS_INTERRUPTED) )
   {
      sp->state = PThreadStateCanceling;
      sp->cancelState = PTHREAD_CANCEL_DISABLE;
      (void) pthread_mutex_unlock (&sp->cancelLock);
      pte_throw (PTE_EPS_CANCEL);

      /* Never reached */
   }

   (void) pthread_mutex_unlock (&sp->cancelLock);

   return (result);
}

int pthread_setconcurrency (int level)
{
   if (level < 0)
      return EINVAL;

   pte_concurrency = level;
   return 0;
}

int pthread_setschedparam (pthread_t thread, int policy,
      const struct sched_param *param)
{
   int result;

   /* Validate the thread id. */
   result = pthread_kill (thread, 0);
   if (0 != result)
      return result;

   /* Validate the scheduling policy. */
   if (policy < SCHED_MIN || policy > SCHED_MAX)
      return EINVAL;

   /* Ensure the policy is SCHED_OTHER. */
   if (policy != SCHED_OTHER)
      return ENOTSUP;

   return (pte_setthreadpriority (thread, policy, param->sched_priority));
}


   int
pte_setthreadpriority (pthread_t thread, int policy, int priority)
{
   int prio;
   int result;
   pte_thread_t * tp = (pte_thread_t *) thread;

   prio = priority;

   /* Validate priority level. */
   if (prio < sched_get_priority_min (policy) ||
         prio > sched_get_priority_max (policy))
      return EINVAL;

   result = pthread_mutex_lock (&tp->threadLock);

   if (0 == result)
   {
      /* If this fails, the current priority is unchanged. */

      if (0 != pte_osThreadSetPriority(tp->threadId, prio))
         result = EINVAL;
      else
      {
         /*
          * Must record the thread's sched_priority as given,
          * not as finally adjusted.
          */
         tp->sched_priority = priority;
      }

      (void) pthread_mutex_unlock (&tp->threadLock);
   }

   return result;
}

int pthread_setspecific (pthread_key_t key, const void *value)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function sets the value of the thread specific
    *      key in the calling thread.
    *
    * PARAMETERS
    *      key
    *              an instance of pthread_key_t
    *      value
    *              the value to set key to
    *
    *
    * DESCRIPTION
    *      This function sets the value of the thread specific
    *      key in the calling thread.
    *
    * RESULTS
    *              0               successfully set value
    *              EAGAIN          could not set value
    *              ENOENT          SERIOUS!!
    *
    * ------------------------------------------------------
    */
{
   pthread_t self;
   int result = 0;

   if (key != pte_selfThreadKey)
   {
      /*
       * Using pthread_self will implicitly create
       * an instance of pthread_t for the current
       * thread if one wasn't explicitly created
       */
      self = pthread_self ();
      if (self == NULL)
         return ENOENT;
   }
   else
   {
      /*
       * Resolve catch-22 of registering thread with selfThread
       * key
       */
      pte_thread_t * sp = (pte_thread_t *) pthread_getspecific (pte_selfThreadKey);

      if (sp == NULL)
      {
         if (value == NULL)
            return ENOENT;

         self = *((pthread_t *) value);
      }
      else
         self = sp;
   }

   result = 0;

   if (key != NULL)
   {
      if (self != NULL && key->destructor != NULL && value != NULL)
      {
         /*
          * Only require associations if we have to
          * call user destroy routine.
          * Don't need to locate an existing association
          * when setting data to NULL since the
          * data is stored with the operating system; not
          * on the association; setting assoc to NULL short
          * circuits the search.
          */
         ThreadKeyAssoc *assoc;

         if (pthread_mutex_lock(&(key->keyLock)) == 0)
         {
            pte_thread_t * sp = (pte_thread_t *) self;

            (void) pthread_mutex_lock(&(sp->threadLock));

            assoc = (ThreadKeyAssoc *) sp->keys;
            /*
             * Locate existing association
             */
            while (assoc != NULL)
            {
               /*
                * Association already exists
                */
               if (assoc->key == key)
                  break;
               assoc = assoc->nextKey;
            }

            /*
             * create an association if not found
             */
            if (assoc == NULL)
               result = pte_tkAssocCreate (sp, key);

            (void) pthread_mutex_unlock(&(sp->threadLock));
         }
         (void) pthread_mutex_unlock(&(key->keyLock));
      }

      if (result == 0)
      {
         if (pte_osTlsSetValue (key->key, (void *) value) != PTE_OS_OK)
            result = EAGAIN;
      }
   }

   return (result);
}
