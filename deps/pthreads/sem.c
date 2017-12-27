/*
 * -------------------------------------------------------------
 *
 * Module: sem.c
 *
 * Purpose:
 *	Semaphores aren't actually part of the PThreads standard.
 *	They are defined by the POSIX Standard:
 *
 *		POSIX 1003.1b-1993	(POSIX.1b)
 *
 * -------------------------------------------------------------
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
#include "semaphore.h"
#include "implement.h"

typedef struct
{
   sem_t sem;
   int * resultPtr;
} sem_timedwait_cleanup_args_t;

static void pte_sem_wait_cleanup(void * sem)
{
   sem_t s = (sem_t) sem;
   unsigned int timeout;

   if (pthread_mutex_lock (&s->lock) == 0)
   {
      /*
       * If sema is destroyed do nothing, otherwise:-
       * If the sema is posted between us being cancelled and us locking
       * the sema again above then we need to consume that post but cancel
       * anyway. If we don't get the semaphore we indicate that we're no
       * longer waiting.
       */
      timeout = 0;
      if (pte_osSemaphorePend(s->sem, &timeout) != PTE_OS_OK)
      {
         ++s->value;

         /*
          * Don't release the W32 sema, it doesn't need adjustment
          * because it doesn't record the number of waiters.
          */

      }
      (void) pthread_mutex_unlock (&s->lock);
   }
}

static void pte_sem_timedwait_cleanup (void * args)
{
   sem_timedwait_cleanup_args_t * a = (sem_timedwait_cleanup_args_t *)args;
   sem_t s = a->sem;

   if (pthread_mutex_lock (&s->lock) == 0)
   {
      /*
       * We either timed out or were cancelled.
       * If someone has posted between then and now we try to take the semaphore.
       * Otherwise the semaphore count may be wrong after we
       * return. In the case of a cancellation, it is as if we
       * were cancelled just before we return (after taking the semaphore)
       * which is ok.
       */
      unsigned int timeout = 0;
      if (pte_osSemaphorePend(s->sem, &timeout) == PTE_OS_OK)
      {
         /* We got the semaphore on the second attempt */
         *(a->resultPtr) = 0;
      }
      else
      {
         /* Indicate we're no longer waiting */
         s->value++;

         /*
          * Don't release the OS sema, it doesn't need adjustment
          * because it doesn't record the number of waiters.
          */

      }
      (void) pthread_mutex_unlock (&s->lock);
   }
}

int sem_close (sem_t * sem)
{
   errno = ENOSYS;
   return -1;
}

int sem_destroy (sem_t * sem)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function destroys an unnamed semaphore.
 *
 * PARAMETERS
 *      sem
 *              pointer to an instance of sem_t
 *
 * DESCRIPTION
 *      This function destroys an unnamed semaphore.
 *
 * RESULTS
 *              0               successfully destroyed semaphore,
 *              -1              failed, error in errno
 * ERRNO
 *              EINVAL          'sem' is not a valid semaphore,
 *              ENOSYS          semaphores are not supported,
 *              EBUSY           threads (or processes) are currently
 *                                      blocked on 'sem'
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   sem_t s = NULL;

   if (sem == NULL || *sem == NULL)
      result = EINVAL;
   else
   {
      s = *sem;

      if ((result = pthread_mutex_lock (&s->lock)) == 0)
      {
         if (s->value < 0)
         {
            (void) pthread_mutex_unlock (&s->lock);
            result = EBUSY;
         }
         else
         {
            /* There are no threads currently blocked on this semaphore. */
            pte_osResult osResult = pte_osSemaphoreDelete(s->sem);

            if (osResult != PTE_OS_OK)
            {
               (void) pthread_mutex_unlock (&s->lock);
               result = EINVAL;
            }
            else
            {
               /*
                * Invalidate the semaphore handle when we have the lock.
                * Other sema operations should test this after acquiring the lock
                * to check that the sema is still valid, i.e. before performing any
                * operations. This may only be necessary before the sema op routine
                * returns so that the routine can return EINVAL - e.g. if setting
                * s->value to SEM_VALUE_MAX below does force a fall-through.
                */
               *sem = NULL;

               /* Prevent anyone else actually waiting on or posting this sema.
               */
               s->value = SEM_VALUE_MAX;

               (void) pthread_mutex_unlock (&s->lock);

               do
               {
                  /* Give other threads a chance to run and exit any sema op
                   * routines. Due to the SEM_VALUE_MAX value, if sem_post or
                   * sem_wait were blocked by us they should fall through.
                   */
                  pte_osThreadSleep(1);
               }
               while (pthread_mutex_destroy (&s->lock) == EBUSY);
            }
         }
      }
   }

   if (result != 0)
   {
      errno = result;
      return -1;
   }

   free (s);

   return 0;
}

int
sem_getvalue (sem_t * sem, int *sval)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function stores the current count value of the
 *      semaphore.
 * RESULTS
 *
 * Return value
 *
 *       0                  sval has been set.
 *      -1                  failed, error in errno
 *
 *  in global errno
 *
 *      EINVAL              'sem' is not a valid semaphore,
 *      ENOSYS              this function is not supported,
 *
 *
 * PARAMETERS
 *
 *      sem                 pointer to an instance of sem_t
 *
 *      sval                pointer to int.
 *
 * DESCRIPTION
 *      This function stores the current count value of the semaphore
 *      pointed to by sem in the int pointed to by sval.
 */
{
   if (sem == NULL || *sem == NULL || sval == NULL)
   {
      errno = EINVAL;
      return -1;
   }
   else
   {
      long value;
      register sem_t s = *sem;
      int result       = 0;

      if ((result = pthread_mutex_lock(&s->lock)) == 0)
      {
         if (*sem == NULL)
         {
            (void) pthread_mutex_unlock (&s->lock);
            errno = EINVAL;
            return -1;
         }

         value = s->value;
         (void) pthread_mutex_unlock(&s->lock);
         *sval = value;
      }

      return result;
   }
}

int
sem_init (sem_t * sem, int pshared, unsigned int value)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function initializes a semaphore. The
 *      initial value of the semaphore is 'value'
 *
 * PARAMETERS
 *      sem
 *              pointer to an instance of sem_t
 *
 *      pshared
 *              if zero, this semaphore may only be shared between
 *              threads in the same process.
 *              if nonzero, the semaphore can be shared between
 *              processes
 *
 *      value
 *              initial value of the semaphore counter
 *
 * DESCRIPTION
 *      This function initializes a semaphore. The
 *      initial value of the semaphore is set to 'value'.
 *
 * RESULTS
 *              0               successfully created semaphore,
 *              -1              failed, error in errno
 * ERRNO
 *              EINVAL          'sem' is not a valid semaphore, or
 *                              'value' >= SEM_VALUE_MAX
 *              ENOMEM          out of memory,
 *              ENOSPC          a required resource has been exhausted,
 *              ENOSYS          semaphores are not supported,
 *              EPERM           the process lacks appropriate privilege
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   sem_t s = NULL;

   if (pshared != 0)
   {
      /*
       * Creating a semaphore that can be shared between
       * processes
       */
      result = EPERM;
   }
   else if (value > (unsigned int)SEM_VALUE_MAX)
   {
      result = EINVAL;
   }
   else
   {
      s = (sem_t) calloc (1, sizeof (*s));

      if (NULL == s)
      {
         result = ENOMEM;
      }
      else
      {

         s->value = value;
         if (pthread_mutex_init(&s->lock, NULL) == 0)
         {

            pte_osResult osResult = pte_osSemaphoreCreate(0, &s->sem);



            if (osResult != PTE_OS_OK)
            {
               (void) pthread_mutex_destroy(&s->lock);
               result = ENOSPC;
            }

         }
         else
         {
            result = ENOSPC;
         }

         if (result != 0)
         {
            free(s);
         }
      }
   }

   if (result != 0)
   {
      errno = result;
      return -1;
   }

   *sem = s;

   return 0;
}

int sem_open (const char *name, int oflag, mode_t mode, unsigned int value)
{
   errno = ENOSYS;
   return -1;
}

int
sem_post (sem_t * sem)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function posts a wakeup to a semaphore.
 *
 * PARAMETERS
 *      sem
 *              pointer to an instance of sem_t
 *
 * DESCRIPTION
 *      This function posts a wakeup to a semaphore. If there
 *      are waiting threads (or processes), one is awakened;
 *      otherwise, the semaphore value is incremented by one.
 *
 * RESULTS
 *              0               successfully posted semaphore,
 *              -1              failed, error in errno
 * ERRNO
 *              EINVAL          'sem' is not a valid semaphore,
 *              ENOSYS          semaphores are not supported,
 *              ERANGE          semaphore count is too big
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   sem_t s = *sem;

   if (s == NULL)
      result = EINVAL;
   else if ((result = pthread_mutex_lock (&s->lock)) == 0)
   {
      /* See sem_destroy.c
      */
      if (*sem == NULL)
      {
         (void) pthread_mutex_unlock (&s->lock);
         result = EINVAL;
         return -1;
      }

      if (s->value < SEM_VALUE_MAX)
      {
         pte_osResult osResult = pte_osSemaphorePost(s->sem, 1);

         if (++s->value <= 0
               && (osResult != PTE_OS_OK))
         {
            s->value--;
            result = EINVAL;
         }

      }
      else
         result = ERANGE;

      (void) pthread_mutex_unlock (&s->lock);
   }

   if (result != 0)
   {
      errno = result;
      return -1;
   }

   return 0;
}

int
sem_post_multiple (sem_t * sem, int count)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function posts multiple wakeups to a semaphore.
 *
 * PARAMETERS
 *      sem
 *              pointer to an instance of sem_t
 *
 *      count
 *              counter, must be greater than zero.
 *
 * DESCRIPTION
 *      This function posts multiple wakeups to a semaphore. If there
 *      are waiting threads (or processes), n <= count are awakened;
 *      the semaphore value is incremented by count - n.
 *
 * RESULTS
 *              0               successfully posted semaphore,
 *              -1              failed, error in errno
 * ERRNO
 *              EINVAL          'sem' is not a valid semaphore
 *                              or count is less than or equal to zero.
 *              ERANGE          semaphore count is too big
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   long waiters;
   sem_t s = *sem;

   if (s == NULL || count <= 0)
      result = EINVAL;
   else if ((result = pthread_mutex_lock (&s->lock)) == 0)
   {
      /* See sem_destroy.c
      */
      if (*sem == NULL)
      {
         (void) pthread_mutex_unlock (&s->lock);
         result = EINVAL;
         return -1;
      }

      if (s->value <= (SEM_VALUE_MAX - count))
      {
         waiters = -s->value;
         s->value += count;
         if (waiters > 0)
         {

            pte_osSemaphorePost(s->sem, (waiters<=count)?waiters:count);
            result = 0;
         }
         /*
            else
            {
            s->value -= count;
            result = EINVAL;
            }
            */
      }
      else
      {
         result = ERANGE;
      }

      (void) pthread_mutex_unlock (&s->lock);
   }

   if (result != 0)
   {
      errno = result;
      return -1;
   }

   return 0;
}

int
sem_timedwait (sem_t * sem, const struct timespec *abstime)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function waits on a semaphore possibly until
 *      'abstime' time.
 *
 * PARAMETERS
 *      sem
 *              pointer to an instance of sem_t
 *
 *      abstime
 *              pointer to an instance of struct timespec
 *
 * DESCRIPTION
 *      This function waits on a semaphore. If the
 *      semaphore value is greater than zero, it decreases
 *      its value by one. If the semaphore value is zero, then
 *      the calling thread (or process) is blocked until it can
 *      successfully decrease the value or until interrupted by
 *      a signal.
 *
 *      If 'abstime' is a NULL pointer then this function will
 *      block until it can successfully decrease the value or
 *      until interrupted by a signal.
 *
 * RESULTS
 *              0               successfully decreased semaphore,
 *              -1              failed, error in errno
 * ERRNO
 *              EINVAL          'sem' is not a valid semaphore,
 *              ENOSYS          semaphores are not supported,
 *              EINTR           the function was interrupted by a signal,
 *              EDEADLK         a deadlock condition was detected.
 *              ETIMEDOUT       abstime elapsed before success.
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   sem_t s = *sem;

   pthread_testcancel();

   if (sem == NULL)
      result = EINVAL;
   else
   {
      unsigned int milliseconds;
      unsigned int *pTimeout;

      if (abstime == NULL)
      {
         pTimeout = NULL;
      }
      else
      {
         /*
          * Calculate timeout as milliseconds from current system time.
          */
         milliseconds = pte_relmillisecs (abstime);
         pTimeout = &milliseconds;
      }

      if ((result = pthread_mutex_lock (&s->lock)) == 0)
      {
         int v;

         /* See sem_destroy.c
         */
         if (*sem == NULL)
         {
            (void) pthread_mutex_unlock (&s->lock);
            errno = EINVAL;
            return -1;
         }

         v = --s->value;
         (void) pthread_mutex_unlock (&s->lock);

         if (v < 0)
         {

            {
               sem_timedwait_cleanup_args_t cleanup_args;

               cleanup_args.sem = s;
               cleanup_args.resultPtr = &result;

               /* Must wait */
               pthread_cleanup_push(pte_sem_timedwait_cleanup, (void *) &cleanup_args);

               result = pte_cancellable_wait(s->sem,pTimeout);

               pthread_cleanup_pop(result);
            }
         }
      }

   }

   if (result != 0)
   {

      errno = result;
      return -1;

   }

   return 0;
}

int sem_trywait (sem_t * sem)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function tries to wait on a semaphore.
    *
    * PARAMETERS
    *      sem
    *              pointer to an instance of sem_t
    *
    * DESCRIPTION
    *      This function tries to wait on a semaphore. If the
    *      semaphore value is greater than zero, it decreases
    *      its value by one. If the semaphore value is zero, then
    *      this function returns immediately with the error EAGAIN
    *
    * RESULTS
    *              0               successfully decreased semaphore,
    *              -1              failed, error in errno
    * ERRNO
    *              EAGAIN          the semaphore was already locked,
    *              EINVAL          'sem' is not a valid semaphore,
    *              ENOTSUP         sem_trywait is not supported,
    *              EINTR           the function was interrupted by a signal,
    *              EDEADLK         a deadlock condition was detected.
    *
    * ------------------------------------------------------
    */
{
   int result = 0;
   sem_t s = *sem;

   if (s == NULL)
      result = EINVAL;
   else if ((result = pthread_mutex_lock (&s->lock)) == 0)
   {
      /* See sem_destroy.c
      */
      if (*sem == NULL)
      {
         (void) pthread_mutex_unlock (&s->lock);
         errno = EINVAL;
         return -1;
      }

      if (s->value > 0)
         s->value--;
      else
         result = EAGAIN;

      (void) pthread_mutex_unlock (&s->lock);
   }

   if (result != 0)
   {
      errno = result;
      return -1;
   }

   return 0;
}

int sem_unlink (const char *name)
{
   errno = ENOSYS;
   return -1;
}

int sem_wait (sem_t * sem)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function  waits on a semaphore.
    *
    * PARAMETERS
    *      sem
    *              pointer to an instance of sem_t
    *
    * DESCRIPTION
    *      This function waits on a semaphore. If the
    *      semaphore value is greater than zero, it decreases
    *      its value by one. If the semaphore value is zero, then
    *      the calling thread (or process) is blocked until it can
    *      successfully decrease the value or until interrupted by
    *      a signal.
    *
    * RESULTS
    *              0               successfully decreased semaphore,
    *              -1              failed, error in errno
    * ERRNO
    *              EINVAL          'sem' is not a valid semaphore,
    *              ENOSYS          semaphores are not supported,
    *              EINTR           the function was interrupted by a signal,
    *              EDEADLK         a deadlock condition was detected.
    *
    * ------------------------------------------------------
    */
{
   int result = 0;
   sem_t s = *sem;

   pthread_testcancel();

   if (s == NULL)
   {
      result = EINVAL;
   }
   else
   {
      if ((result = pthread_mutex_lock (&s->lock)) == 0)
      {
         int v;

         /* See sem_destroy.c
         */
         if (*sem == NULL)
         {
            (void) pthread_mutex_unlock (&s->lock);
            errno = EINVAL;
            return -1;
         }

         v = --s->value;
         (void) pthread_mutex_unlock (&s->lock);

         if (v < 0)
         {
            /* Must wait */
            pthread_cleanup_push(pte_sem_wait_cleanup, (void *) s);
            result = pte_cancellable_wait(s->sem,NULL);
            /* Cleanup if we're canceled or on any other error */
            pthread_cleanup_pop(result);

            // Wait was cancelled, indicate that we're no longer waiting on this semaphore.
            /*
               if (result == PTE_OS_INTERRUPTED)
               {
               result = EINTR;
               ++s->value;
               }
               */
         }
      }

   }

   if (result != 0)
   {
      errno = result;
      return -1;
   }

   return 0;

}				/* sem_wait */


   int
sem_wait_nocancel (sem_t * sem)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function  waits on a semaphore, and doesn't
    *      allow cancellation.
    *
    * PARAMETERS
    *      sem
    *              pointer to an instance of sem_t
    *
    * DESCRIPTION
    *      This function waits on a semaphore. If the
    *      semaphore value is greater than zero, it decreases
    *      its value by one. If the semaphore value is zero, then
    *      the calling thread (or process) is blocked until it can
    *      successfully decrease the value or until interrupted by
    *      a signal.
    *
    * RESULTS
    *              0               successfully decreased semaphore,
    *              -1              failed, error in errno
    * ERRNO
    *              EINVAL          'sem' is not a valid semaphore,
    *              ENOSYS          semaphores are not supported,
    *              EINTR           the function was interrupted by a signal,
    *              EDEADLK         a deadlock condition was detected.
    *
    * ------------------------------------------------------
    */
{
   int result = 0;
   sem_t s = *sem;

   pthread_testcancel();

   if (s == NULL)
   {
      result = EINVAL;
   }
   else
   {
      if ((result = pthread_mutex_lock (&s->lock)) == 0)
      {
         int v;

         /* See sem_destroy.c
         */
         if (*sem == NULL)
         {
            (void) pthread_mutex_unlock (&s->lock);
            errno = EINVAL;
            return -1;
         }

         v = --s->value;
         (void) pthread_mutex_unlock (&s->lock);

         if (v < 0)
         {
            pte_osSemaphorePend(s->sem, NULL);
         }
      }

   }

   if (result != 0)
   {
      errno = result;
      return -1;
   }

   return 0;
}
