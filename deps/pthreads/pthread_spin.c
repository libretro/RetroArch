/*
 * pthread_spin.c
 *
 * Description:
 * This translation unit implements spin lock primitives.
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


int
pthread_spin_destroy (pthread_spinlock_t * lock)
{
  register pthread_spinlock_t s;
  int result = 0;

  if (lock == NULL || *lock == NULL)
    {
      return EINVAL;
    }

  if ((s = *lock) != PTHREAD_SPINLOCK_INITIALIZER)
    {
      if (s->interlock == PTE_SPIN_USE_MUTEX)
        {
          result = pthread_mutex_destroy (&(s->u.mutex));
        }
      else if (PTE_SPIN_UNLOCKED !=
               PTE_ATOMIC_COMPARE_EXCHANGE (
                 & (s->interlock),
                 (int) PTE_OBJECT_INVALID,
                 PTE_SPIN_UNLOCKED))
        {
          result = EINVAL;
        }

      if (0 == result)
        {
          /*
           * We are relying on the application to ensure that all other threads
           * have finished with the spinlock before destroying it.
           */
          *lock = NULL;
          (void) free (s);
        }
    }
  else
    {
      /*
       * See notes in pte_spinlock_check_need_init() above also.
       */

      pte_osMutexLock (pte_spinlock_test_init_lock);

      /*
       * Check again.
       */
      if (*lock == PTHREAD_SPINLOCK_INITIALIZER)
        {
          /*
           * This is all we need to do to destroy a statically
           * initialised spinlock that has not yet been used (initialised).
           * If we get to here, another thread
           * waiting to initialise this mutex will get an EINVAL.
           */
          *lock = NULL;
        }
      else
        {
          /*
           * The spinlock has been initialised while we were waiting
           * so assume it's in use.
           */
          result = EBUSY;
        }

      pte_osMutexUnlock(pte_spinlock_test_init_lock);

    }

  return (result);
}

int pthread_spin_init (pthread_spinlock_t * lock, int pshared)
{
   pthread_spinlock_t s;
   int cpus = 0;
   int result = 0;

   if (lock == NULL)
      return EINVAL;

   if (0 != pte_getprocessors (&cpus))
   {
      cpus = 1;
   }

   if (cpus > 1)
   {
      if (pshared == PTHREAD_PROCESS_SHARED)
      {
         /*
          * Creating spinlock that can be shared between
          * processes.
          */
#if _POSIX_THREAD_PROCESS_SHARED >= 0

         /*
          * Not implemented yet.
          */

#error ERROR [__FILE__, line __LINE__]: Process shared spin locks are not supported yet.

#else

         return ENOSYS;

#endif /* _POSIX_THREAD_PROCESS_SHARED */

      }
   }

   s = (pthread_spinlock_t) calloc (1, sizeof (*s));

   if (s == NULL)
   {
      return ENOMEM;
   }

   if (cpus > 1)
   {
      s->u.cpus = cpus;
      s->interlock = PTE_SPIN_UNLOCKED;
   }
   else
   {
      pthread_mutexattr_t ma;
      result = pthread_mutexattr_init (&ma);

      if (0 == result)
      {
         ma->pshared = pshared;
         result = pthread_mutex_init (&(s->u.mutex), &ma);
         if (0 == result)
         {
            s->interlock = PTE_SPIN_USE_MUTEX;
         }
      }
      (void) pthread_mutexattr_destroy (&ma);
   }

   if (0 == result)
   {
      *lock = s;
   }
   else
   {
      (void) free (s);
      *lock = NULL;
   }

   return (result);
}

int pthread_spin_lock (pthread_spinlock_t * lock)
{
   register pthread_spinlock_t s;

   if (NULL == lock || NULL == *lock)
      return (EINVAL);

   if (*lock == PTHREAD_SPINLOCK_INITIALIZER)
   {
      int result;

      if ((result = pte_spinlock_check_need_init (lock)) != 0)
         return (result);
   }

   s = *lock;

   while ( PTE_SPIN_LOCKED ==
         PTE_ATOMIC_COMPARE_EXCHANGE (&(s->interlock),
            PTE_SPIN_LOCKED,
            PTE_SPIN_UNLOCKED))
   {
   }

   if (s->interlock == PTE_SPIN_LOCKED)
      return 0;

   if (s->interlock == PTE_SPIN_USE_MUTEX)
      return pthread_mutex_lock (&(s->u.mutex));

   return EINVAL;
}

int pthread_spin_trylock (pthread_spinlock_t * lock)
{
   register pthread_spinlock_t s;

   if (NULL == lock || NULL == *lock)
      return (EINVAL);

   if (*lock == PTHREAD_SPINLOCK_INITIALIZER)
   {
      int result;

      if ((result = pte_spinlock_check_need_init (lock)) != 0)
         return (result);
   }

   s = *lock;

   switch ((long)
         PTE_ATOMIC_COMPARE_EXCHANGE (&(s->interlock),
            PTE_SPIN_LOCKED,
            PTE_SPIN_UNLOCKED))
   {
      case PTE_SPIN_UNLOCKED:
         return 0;
      case PTE_SPIN_LOCKED:
         return EBUSY;
      case PTE_SPIN_USE_MUTEX:
         return pthread_mutex_trylock (&(s->u.mutex));
   }

   return EINVAL;
}

int pthread_spin_unlock (pthread_spinlock_t * lock)
{
   register pthread_spinlock_t s;

   if (NULL == lock || NULL == *lock)
      return (EINVAL);

   s = *lock;

   if (s == PTHREAD_SPINLOCK_INITIALIZER)
      return EPERM;

   switch ((long)
         PTE_ATOMIC_COMPARE_EXCHANGE (&(s->interlock),
            PTE_SPIN_UNLOCKED,
            PTE_SPIN_LOCKED))
   {
      case PTE_SPIN_LOCKED:
         return 0;
      case PTE_SPIN_UNLOCKED:
         return EPERM;
      case PTE_SPIN_USE_MUTEX:
         return pthread_mutex_unlock (&(s->u.mutex));
   }

   return EINVAL;
}
