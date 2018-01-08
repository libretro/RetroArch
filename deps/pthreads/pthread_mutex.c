/*
 * pthread_mutex.c
 *
 * Description:
 * This translation unit implements mutual exclusion (mutex) primitives.
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

#define TEST_IE InterlockedExchange

int pthread_mutex_destroy (pthread_mutex_t * mutex)
{
   int result = 0;
   pthread_mutex_t mx;

   /*
    * Let the system deal with invalid pointers.
    */

   /*
    * Check to see if we have something to delete.
    */
   if (*mutex < PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
   {
      mx = *mutex;

      result = pthread_mutex_trylock (&mx);

      /*
       * If trylock succeeded and the mutex is not recursively locked it
       * can be destroyed.
       */
      if (result == 0)
      {
         if (mx->kind != PTHREAD_MUTEX_RECURSIVE || 1 == mx->recursive_count)
         {
            /*
             * FIXME!!!
             * The mutex isn't held by another thread but we could still
             * be too late invalidating the mutex below since another thread
             * may already have entered mutex_lock and the check for a valid
             * *mutex != NULL.
             *
             * Note that this would be an unusual situation because it is not
             * common that mutexes are destroyed while they are still in
             * use by other threads.
             */
            *mutex = NULL;

            result = pthread_mutex_unlock (&mx);

            if (result == 0)
            {
               pte_osSemaphoreDelete(mx->handle);

               free(mx);

            }
            else
            {
               /*
                * Restore the mutex before we return the error.
                */
               *mutex = mx;
            }
         }
         else			/* mx->recursive_count > 1 */
         {
            /*
             * The mutex must be recursive and already locked by us (this thread).
             */
            mx->recursive_count--;	/* Undo effect of pthread_mutex_trylock() above */
            result = EBUSY;
         }
      }
   }
   else
   {
      /*
       * See notes in pte_mutex_check_need_init() above also.
       */

      pte_osMutexLock (pte_mutex_test_init_lock);


      /*
       * Check again.
       */
      if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
      {
         /*
          * This is all we need to do to destroy a statically
          * initialised mutex that has not yet been used (initialised).
          * If we get to here, another thread
          * waiting to initialise this mutex will get an EINVAL.
          */
         *mutex = NULL;
      }
      else
      {
         /*
          * The mutex has been initialised while we were waiting
          * so assume it's in use.
          */
         result = EBUSY;
      }

      pte_osMutexUnlock(pte_mutex_test_init_lock);

   }

   return (result);
}

int pthread_mutex_init (pthread_mutex_t * mutex, const pthread_mutexattr_t * attr)
{
   int result = 0;
   pthread_mutex_t mx;

   if (mutex == NULL)
      return EINVAL;

   mx = (pthread_mutex_t) calloc (1, sizeof (*mx));

   if (mx == NULL)
      result = ENOMEM;
   else
   {
      mx->lock_idx = 0;
      mx->recursive_count = 0;
      mx->kind = (attr == NULL || *attr == NULL
            ? PTHREAD_MUTEX_DEFAULT : (*attr)->kind);
      mx->ownerThread = NULL;

      pte_osSemaphoreCreate(0,&mx->handle);

   }

   *mutex = mx;

   return (result);
}

int pthread_mutex_lock (pthread_mutex_t * mutex)
{
   int result = 0;
   pthread_mutex_t mx;

   /*
    * Let the system deal with invalid pointers.
    */
   if (*mutex == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static mutex. We check
    * again inside the guarded section of pte_mutex_check_need_init()
    * to avoid race conditions.
    */
   if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
   {
      if ((result = pte_mutex_check_need_init (mutex)) != 0)
         return (result);
   }

   mx = *mutex;

   if (mx->kind == PTHREAD_MUTEX_NORMAL)
   {
      if (PTE_ATOMIC_EXCHANGE(
               &mx->lock_idx,
               1) != 0)
      {
         while (PTE_ATOMIC_EXCHANGE(&mx->lock_idx,-1) != 0)
         {
            if (pte_osSemaphorePend(mx->handle,NULL) != PTE_OS_OK)
            {
               result = EINVAL;
               break;
            }
         }
      }
   }
   else
   {
      pthread_t self = pthread_self();

      if (PTE_ATOMIC_COMPARE_EXCHANGE(&mx->lock_idx,1,0) == 0)
      {
         mx->recursive_count = 1;
         mx->ownerThread = self;
      }
      else
      {
         if (pthread_equal (mx->ownerThread, self))
         {
            if (mx->kind == PTHREAD_MUTEX_RECURSIVE)
               mx->recursive_count++;
            else
               result = EDEADLK;
         }
         else
         {
            while (PTE_ATOMIC_EXCHANGE(&mx->lock_idx,-1) != 0)
            {
               if (pte_osSemaphorePend(mx->handle,NULL) != PTE_OS_OK)
               {
                  result = EINVAL;
                  break;
               }
            }

            if (0 == result)
            {
               mx->recursive_count = 1;
               mx->ownerThread = self;
            }
         }
      }

   }

   return (result);
}

static int pte_timed_eventwait (pte_osSemaphoreHandle event, const struct timespec *abstime)
/*
 * ------------------------------------------------------
 * DESCRIPTION
 *      This function waits on an event until signaled or until
 *      abstime passes.
 *      If abstime has passed when this routine is called then
 *      it returns a result to indicate this.
 *
 *      If 'abstime' is a NULL pointer then this function will
 *      block until it can successfully decrease the value or
 *      until interrupted by a signal.
 *
 *      This routine is not a cancelation point.
 *
 * RESULTS
 *              0               successfully signaled,
 *              ETIMEDOUT       abstime passed
 *              EINVAL          'event' is not a valid event,
 *
 * ------------------------------------------------------
 */
{
   unsigned int milliseconds;
   pte_osResult status;
   int retval;

   if (abstime == NULL)
      status = pte_osSemaphorePend(event, NULL);
   else
   {
      /*
       * Calculate timeout as milliseconds from current system time.
       */
      milliseconds = pte_relmillisecs (abstime);

      status = pte_osSemaphorePend(event, &milliseconds);
   }


   if (status == PTE_OS_TIMEOUT)
   {
      retval = ETIMEDOUT;
   }
   else
   {
      retval = 0;
   }

   return retval;

}

int pthread_mutex_timedlock (pthread_mutex_t * mutex,
      const struct timespec *abstime)
{
   int result;
   pthread_mutex_t mx;

   /*
    * Let the system deal with invalid pointers.
    */

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static mutex. We check
    * again inside the guarded section of pte_mutex_check_need_init()
    * to avoid race conditions.
    */
   if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
   {
      if ((result = pte_mutex_check_need_init (mutex)) != 0)
         return (result);
   }

   mx = *mutex;

   if (mx->kind == PTHREAD_MUTEX_NORMAL)
   {
      if (PTE_ATOMIC_EXCHANGE(&mx->lock_idx,1) != 0)
      {
         while (PTE_ATOMIC_EXCHANGE(&mx->lock_idx,-1) != 0)
         {
            if (0 != (result = pte_timed_eventwait (mx->handle, abstime)))
               return result;
         }
      }
   }
   else
   {
      pthread_t self = pthread_self();

      if (PTE_ATOMIC_COMPARE_EXCHANGE(&mx->lock_idx,1,0) == 0)
      {
         mx->recursive_count = 1;
         mx->ownerThread = self;
      }
      else
      {
         if (pthread_equal (mx->ownerThread, self))
         {
            if (mx->kind == PTHREAD_MUTEX_RECURSIVE)
               mx->recursive_count++;
            else
               return EDEADLK;
         }
         else
         {
            while (PTE_ATOMIC_EXCHANGE(&mx->lock_idx,-1) != 0)
            {
               if (0 != (result = pte_timed_eventwait (mx->handle, abstime)))
                  return result;
            }

            mx->recursive_count = 1;
            mx->ownerThread = self;
         }
      }
   }

   return 0;
}

int pthread_mutex_trylock (pthread_mutex_t * mutex)
{
   int result = 0;
   pthread_mutex_t mx;

   /*
    * Let the system deal with invalid pointers.
    */

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static mutex. We check
    * again inside the guarded section of pte_mutex_check_need_init()
    * to avoid race conditions.
    */
   if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
   {
      if ((result = pte_mutex_check_need_init (mutex)) != 0)
         return (result);
   }

   mx = *mutex;

   if (0 == PTE_ATOMIC_COMPARE_EXCHANGE (&mx->lock_idx,1,0))
   {
      if (mx->kind != PTHREAD_MUTEX_NORMAL)
      {
         mx->recursive_count = 1;
         mx->ownerThread = pthread_self ();
      }
   }
   else
   {
      if (mx->kind == PTHREAD_MUTEX_RECURSIVE &&
            pthread_equal (mx->ownerThread, pthread_self ()))
         mx->recursive_count++;
      else
         result = EBUSY;
   }

   return (result);
}

int pthread_mutex_unlock (pthread_mutex_t * mutex)
{
   int result         = 0;
   pthread_mutex_t mx = *mutex;

   /*
    * Let the system deal with invalid pointers.
    */

   /*
    * If the thread calling us holds the mutex then there is no
    * race condition. If another thread holds the
    * lock then we shouldn't be in here.
    */
   if (mx < PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
   {
      if (mx->kind == PTHREAD_MUTEX_NORMAL)
      {
         int idx;

         idx = PTE_ATOMIC_EXCHANGE (&mx->lock_idx,0);
         if (idx != 0)
         {
            if (idx < 0)
            {
               /*
                * Someone may be waiting on that mutex.
                */
               if (pte_osSemaphorePost(mx->handle,1) != PTE_OS_OK)
                  result = EINVAL;
            }
         }
         else
         {
            /*
             * Was not locked (so can't be owned by us).
             */
            result = EPERM;
         }
      }
      else
      {
         if (pthread_equal (mx->ownerThread, pthread_self ()))
         {
            if (mx->kind != PTHREAD_MUTEX_RECURSIVE
                  || 0 == --mx->recursive_count)
            {
               mx->ownerThread = NULL;

               if (PTE_ATOMIC_EXCHANGE (&mx->lock_idx,0) < 0)
               {
                  if (pte_osSemaphorePost(mx->handle,1) != PTE_OS_OK)
                     result = EINVAL;
               }
            }
         }
         else
            result = EPERM;
      }
   }
   else
      result = EINVAL;

   return (result);
}
