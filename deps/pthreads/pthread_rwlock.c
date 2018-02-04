/*
 * pthread_rwlock.c
 *
 * Description:
 * This translation unit implements read/write lock primitives.
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
#include <errno.h>
#include <limits.h>

#include "pthread.h"
#include "implement.h"

int pthread_rwlock_destroy (pthread_rwlock_t * rwlock)
{
   pthread_rwlock_t rwl;
   int result = 0, result1 = 0, result2 = 0;

   if (rwlock == NULL || *rwlock == NULL)
      return EINVAL;

   if (*rwlock != PTHREAD_RWLOCK_INITIALIZER)
   {
      rwl = *rwlock;

      if (rwl->nMagic != PTE_RWLOCK_MAGIC)
         return EINVAL;

      if ((result = pthread_mutex_lock (&(rwl->mtxExclusiveAccess))) != 0)
         return result;

      if ((result =
               pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
      {
         (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         return result;
      }

      /*
       * Check whether any threads own/wait for the lock (wait for ex.access);
       * report "BUSY" if so.
       */
      if (rwl->nExclusiveAccessCount > 0
            || rwl->nSharedAccessCount > rwl->nCompletedSharedAccessCount)
      {
         result = pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted));
         result1 = pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         result2 = EBUSY;
      }
      else
      {
         rwl->nMagic = 0;

         if ((result =
                  pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted))) != 0)
         {
            pthread_mutex_unlock (&rwl->mtxExclusiveAccess);
            return result;
         }

         if ((result =
                  pthread_mutex_unlock (&(rwl->mtxExclusiveAccess))) != 0)
            return result;

         *rwlock = NULL;	/* Invalidate rwlock before anything else */
         result = pthread_cond_destroy (&(rwl->cndSharedAccessCompleted));
         result1 = pthread_mutex_destroy (&(rwl->mtxSharedAccessCompleted));
         result2 = pthread_mutex_destroy (&(rwl->mtxExclusiveAccess));
         (void) free (rwl);
      }
   }
   else
   {
      /*
       * See notes in pte_rwlock_check_need_init() above also.
       */

      pte_osMutexLock (pte_rwlock_test_init_lock);

      /*
       * Check again.
       */
      if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
      {
         /*
          * This is all we need to do to destroy a statically
          * initialised rwlock that has not yet been used (initialised).
          * If we get to here, another thread
          * waiting to initialise this rwlock will get an EINVAL.
          */
         *rwlock = NULL;
      }
      /*
       * The rwlock has been initialised while we were waiting
       * so assume it's in use.
       */
      else
         result = EBUSY;

      pte_osMutexUnlock(pte_rwlock_test_init_lock);

   }

   return ((result != 0) ? result : ((result1 != 0) ? result1 : result2));
}

int pthread_rwlock_init (pthread_rwlock_t * rwlock,
      const pthread_rwlockattr_t * attr)
{
   int result;
   pthread_rwlock_t rwl = 0;

   if (rwlock == NULL)
      return EINVAL;

   if (attr != NULL && *attr != NULL)
   {
      result = EINVAL;		/* Not supported */
      goto DONE;
   }

   rwl = (pthread_rwlock_t) calloc (1, sizeof (*rwl));

   if (rwl == NULL)
   {
      result = ENOMEM;
      goto DONE;
   }

   rwl->nSharedAccessCount = 0;
   rwl->nExclusiveAccessCount = 0;
   rwl->nCompletedSharedAccessCount = 0;

   result = pthread_mutex_init (&rwl->mtxExclusiveAccess, NULL);
   if (result != 0)
   {
      goto FAIL0;
   }

   result = pthread_mutex_init (&rwl->mtxSharedAccessCompleted, NULL);
   if (result != 0)
   {
      goto FAIL1;
   }

   result = pthread_cond_init (&rwl->cndSharedAccessCompleted, NULL);
   if (result != 0)
   {
      goto FAIL2;
   }

   rwl->nMagic = PTE_RWLOCK_MAGIC;

   result = 0;
   goto DONE;

FAIL2:
   (void) pthread_mutex_destroy (&(rwl->mtxSharedAccessCompleted));

FAIL1:
   (void) pthread_mutex_destroy (&(rwl->mtxExclusiveAccess));

FAIL0:
   (void) free (rwl);
   rwl = NULL;

DONE:
   *rwlock = rwl;

   return result;
}

int pthread_rwlock_rdlock (pthread_rwlock_t * rwlock)
{
   int result;
   pthread_rwlock_t rwl;

   if (rwlock == NULL || *rwlock == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static rwlock. We check
    * again inside the guarded section of pte_rwlock_check_need_init()
    * to avoid race conditions.
    */
   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
   {
      result = pte_rwlock_check_need_init (rwlock);

      if (result != 0 && result != EBUSY)
         return result;
   }

   rwl = *rwlock;

   if (rwl->nMagic != PTE_RWLOCK_MAGIC)
      return EINVAL;

   if ((result = pthread_mutex_lock (&(rwl->mtxExclusiveAccess))) != 0)
      return result;

   if (++rwl->nSharedAccessCount == INT_MAX)
   {
      if ((result =
               pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
      {
         (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         return result;
      }

      rwl->nSharedAccessCount -= rwl->nCompletedSharedAccessCount;
      rwl->nCompletedSharedAccessCount = 0;

      if ((result =
               pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted))) != 0)
      {
         (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         return result;
      }
   }

   return (pthread_mutex_unlock (&(rwl->mtxExclusiveAccess)));
}

int pthread_rwlock_timedrdlock (pthread_rwlock_t * rwlock,
                            const struct timespec *abstime)
{
   int result;
   pthread_rwlock_t rwl;

   if (rwlock == NULL || *rwlock == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static rwlock. We check
    * again inside the guarded section of pte_rwlock_check_need_init()
    * to avoid race conditions.
    */
   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
   {
      result = pte_rwlock_check_need_init (rwlock);

      if (result != 0 && result != EBUSY)
      {
         return result;
      }
   }

   rwl = *rwlock;

   if (rwl->nMagic != PTE_RWLOCK_MAGIC)
   {
      return EINVAL;
   }

   if ((result =
            pthread_mutex_timedlock (&(rwl->mtxExclusiveAccess), abstime)) != 0)
   {
      return result;
   }

   if (++rwl->nSharedAccessCount == INT_MAX)
   {
      if ((result =
               pthread_mutex_timedlock (&(rwl->mtxSharedAccessCompleted),
                  abstime)) != 0)
      {
         if (result == ETIMEDOUT)
         {
            ++rwl->nCompletedSharedAccessCount;
         }
         (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         return result;
      }

      rwl->nSharedAccessCount -= rwl->nCompletedSharedAccessCount;
      rwl->nCompletedSharedAccessCount = 0;

      if ((result =
               pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted))) != 0)
      {
         (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         return result;
      }
   }

   return (pthread_mutex_unlock (&(rwl->mtxExclusiveAccess)));
}

int pthread_rwlock_timedwrlock (pthread_rwlock_t * rwlock,
                            const struct timespec *abstime)
{
   int result;
   pthread_rwlock_t rwl;

   if (rwlock == NULL || *rwlock == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static rwlock. We check
    * again inside the guarded section of pte_rwlock_check_need_init()
    * to avoid race conditions.
    */
   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
   {
      result = pte_rwlock_check_need_init (rwlock);

      if (result != 0 && result != EBUSY)
      {
         return result;
      }
   }

   rwl = *rwlock;

   if (rwl->nMagic != PTE_RWLOCK_MAGIC)
   {
      return EINVAL;
   }

   if ((result =
            pthread_mutex_timedlock (&(rwl->mtxExclusiveAccess), abstime)) != 0)
   {
      return result;
   }

   if ((result =
            pthread_mutex_timedlock (&(rwl->mtxSharedAccessCompleted),
               abstime)) != 0)
   {
      (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
      return result;
   }

   if (rwl->nExclusiveAccessCount == 0)
   {
      if (rwl->nCompletedSharedAccessCount > 0)
      {
         rwl->nSharedAccessCount -= rwl->nCompletedSharedAccessCount;
         rwl->nCompletedSharedAccessCount = 0;
      }

      if (rwl->nSharedAccessCount > 0)
      {
         rwl->nCompletedSharedAccessCount = -rwl->nSharedAccessCount;

         /*
          * This routine may be a cancelation point
          * according to POSIX 1003.1j section 18.1.2.
          */
         pthread_cleanup_push (pte_rwlock_cancelwrwait, (void *) rwl);

         do
         {
            result =
               pthread_cond_timedwait (&(rwl->cndSharedAccessCompleted),
                     &(rwl->mtxSharedAccessCompleted),
                     abstime);
         }
         while (result == 0 && rwl->nCompletedSharedAccessCount < 0);

         pthread_cleanup_pop ((result != 0) ? 1 : 0);

         if (result == 0)
         {
            rwl->nSharedAccessCount = 0;
         }
      }
   }

   if (result == 0)
      rwl->nExclusiveAccessCount++;

   return result;
}

int pthread_rwlock_tryrdlock (pthread_rwlock_t * rwlock)
{
   int result;
   pthread_rwlock_t rwl;

   if (rwlock == NULL || *rwlock == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static rwlock. We check
    * again inside the guarded section of pte_rwlock_check_need_init()
    * to avoid race conditions.
    */
   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
   {
      result = pte_rwlock_check_need_init (rwlock);

      if (result != 0 && result != EBUSY)
      {
         return result;
      }
   }

   rwl = *rwlock;

   if (rwl->nMagic != PTE_RWLOCK_MAGIC)
   {
      return EINVAL;
   }

   if ((result = pthread_mutex_trylock (&(rwl->mtxExclusiveAccess))) != 0)
   {
      return result;
   }

   if (++rwl->nSharedAccessCount == INT_MAX)
   {
      if ((result =
               pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
      {
         (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         return result;
      }

      rwl->nSharedAccessCount -= rwl->nCompletedSharedAccessCount;
      rwl->nCompletedSharedAccessCount = 0;

      if ((result =
               pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted))) != 0)
      {
         (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
         return result;
      }
   }

   return (pthread_mutex_unlock (&rwl->mtxExclusiveAccess));
}

int pthread_rwlock_trywrlock (pthread_rwlock_t * rwlock)
{
   int result, result1;
   pthread_rwlock_t rwl;

   if (rwlock == NULL || *rwlock == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static rwlock. We check
    * again inside the guarded section of pte_rwlock_check_need_init()
    * to avoid race conditions.
    */
   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
   {
      result = pte_rwlock_check_need_init (rwlock);

      if (result != 0 && result != EBUSY)
      {
         return result;
      }
   }

   rwl = *rwlock;

   if (rwl->nMagic != PTE_RWLOCK_MAGIC)
   {
      return EINVAL;
   }

   if ((result = pthread_mutex_trylock (&(rwl->mtxExclusiveAccess))) != 0)
   {
      return result;
   }

   if ((result =
            pthread_mutex_trylock (&(rwl->mtxSharedAccessCompleted))) != 0)
   {
      result1 = pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
      return ((result1 != 0) ? result1 : result);
   }

   if (rwl->nExclusiveAccessCount == 0)
   {
      if (rwl->nCompletedSharedAccessCount > 0)
      {
         rwl->nSharedAccessCount -= rwl->nCompletedSharedAccessCount;
         rwl->nCompletedSharedAccessCount = 0;
      }

      if (rwl->nSharedAccessCount > 0)
      {
         if ((result =
                  pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted))) != 0)
         {
            (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
            return result;
         }

         if ((result =
                  pthread_mutex_unlock (&(rwl->mtxExclusiveAccess))) == 0)
         {
            result = EBUSY;
         }
      }
      else
      {
         rwl->nExclusiveAccessCount = 1;
      }
   }
   else
   {
      result = EBUSY;
   }

   return result;
}

int pthread_rwlock_unlock (pthread_rwlock_t * rwlock)
{
   int result, result1;
   pthread_rwlock_t rwl;

   if (rwlock == NULL || *rwlock == NULL)
      return (EINVAL);

   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
   {
      /*
       * Assume any race condition here is harmless.
       */
      return 0;
   }

   rwl = *rwlock;

   if (rwl->nMagic != PTE_RWLOCK_MAGIC)
   {
      return EINVAL;
   }

   if (rwl->nExclusiveAccessCount == 0)
   {
      if ((result =
               pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
      {
         return result;
      }

      if (++rwl->nCompletedSharedAccessCount == 0)
      {
         result = pthread_cond_signal (&(rwl->cndSharedAccessCompleted));
      }

      result1 = pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted));
   }
   else
   {
      rwl->nExclusiveAccessCount--;

      result = pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted));
      result1 = pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));

   }

   return ((result != 0) ? result : result1);
}

int pthread_rwlock_wrlock (pthread_rwlock_t * rwlock)
{
   int result;
   pthread_rwlock_t rwl;

   if (rwlock == NULL || *rwlock == NULL)
      return EINVAL;

   /*
    * We do a quick check to see if we need to do more work
    * to initialise a static rwlock. We check
    * again inside the guarded section of pte_rwlock_check_need_init()
    * to avoid race conditions.
    */
   if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
   {
      result = pte_rwlock_check_need_init (rwlock);

      if (result != 0 && result != EBUSY)
         return result;
   }

   rwl = *rwlock;

   if (rwl->nMagic != PTE_RWLOCK_MAGIC)
      return EINVAL;

   if ((result = pthread_mutex_lock (&(rwl->mtxExclusiveAccess))) != 0)
      return result;

   if ((result = pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
   {
      (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
      return result;
   }

   if (rwl->nExclusiveAccessCount == 0)
   {
      if (rwl->nCompletedSharedAccessCount > 0)
      {
         rwl->nSharedAccessCount -= rwl->nCompletedSharedAccessCount;
         rwl->nCompletedSharedAccessCount = 0;
      }

      if (rwl->nSharedAccessCount > 0)
      {
         rwl->nCompletedSharedAccessCount = -rwl->nSharedAccessCount;

         /*
          * This routine may be a cancelation point
          * according to POSIX 1003.1j section 18.1.2.
          */
         pthread_cleanup_push (pte_rwlock_cancelwrwait, (void *) rwl);

         do
         {
            result = pthread_cond_wait (&(rwl->cndSharedAccessCompleted),
                  &(rwl->mtxSharedAccessCompleted));
         }
         while (result == 0 && rwl->nCompletedSharedAccessCount < 0);

         pthread_cleanup_pop ((result != 0) ? 1 : 0);

         if (result == 0)
            rwl->nSharedAccessCount = 0;
      }
   }

   if (result == 0)
      rwl->nExclusiveAccessCount++;

   return result;
}
