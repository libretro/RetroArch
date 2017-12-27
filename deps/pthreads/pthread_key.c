/*
 * pthread_key.c
 *
 * Description:
 * POSIX thread functions which implement thread-specific data (TSD).
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


int
pthread_key_create (pthread_key_t * key, void (*destructor) (void *))
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      This function creates a thread-specific data key visible
 *      to all threads. All existing and new threads have a value
 *      NULL for key until set using pthread_setspecific. When any
 *      thread with a non-NULL value for key terminates, 'destructor'
 *      is called with key's current value for that thread.
 *
 * PARAMETERS
 *      key
 *              pointer to an instance of pthread_key_t
 *
 *
 * DESCRIPTION
 *      This function creates a thread-specific data key visible
 *      to all threads. All existing and new threads have a value
 *      NULL for key until set using pthread_setspecific. When any
 *      thread with a non-NULL value for key terminates, 'destructor'
 *      is called with key's current value for that thread.
 *
 * RESULTS
 *              0               successfully created semaphore,
 *              EAGAIN          insufficient resources or PTHREAD_KEYS_MAX
 *                              exceeded,
 *              ENOMEM          insufficient memory to create the key,
 *
 * ------------------------------------------------------
 */
{
   int result = 0;
   pthread_key_t newkey;

   if ((newkey = (pthread_key_t) calloc (1, sizeof (*newkey))) == NULL)
      result = ENOMEM;
   else
   {
      pte_osResult osResult = pte_osTlsAlloc(&(newkey->key));

      if (osResult != PTE_OS_OK)
      {
         result = EAGAIN;

         free (newkey);
         newkey = NULL;
      }
      else if (destructor != NULL)
      {
         /*
          * Have to manage associations between thread and key;
          * Therefore, need a lock that allows multiple threads
          * to gain exclusive access to the key->threads list.
          *
          * The mutex will only be created when it is first locked.
          */
         newkey->keyLock = PTHREAD_MUTEX_INITIALIZER;
         newkey->destructor = destructor;
      }

   }

   *key = newkey;

   return (result);
}

int pthread_key_delete (pthread_key_t key)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function deletes a thread-specific data key. This
    *      does not change the value of the thread specific data key
    *      for any thread and does not run the key's destructor
    *      in any thread so it should be used with caution.
    *
    * PARAMETERS
    *      key
    *              pointer to an instance of pthread_key_t
    *
    *
    * DESCRIPTION
    *      This function deletes a thread-specific data key. This
    *      does not change the value of the thread specific data key
    *      for any thread and does not run the key's destructor
    *      in any thread so it should be used with caution.
    *
    * RESULTS
    *              0               successfully deleted the key,
    *              EINVAL          key is invalid,
    *
    * ------------------------------------------------------
    */
{
   int result = 0;

   if (key != NULL)
   {
      if (key->threads != NULL &&
            key->destructor != NULL &&
            pthread_mutex_lock (&(key->keyLock)) == 0)
      {
         ThreadKeyAssoc *assoc;
         /*
          * Run through all Thread<-->Key associations
          * for this key.
          *
          * While we hold at least one of the locks guarding
          * the assoc, we know that the assoc pointed to by
          * key->threads is valid.
          */
         while ((assoc = (ThreadKeyAssoc *) key->threads) != NULL)
         {
            pte_thread_t * thread = assoc->thread;

            /* Finished */
            if (assoc == NULL)
               break;

            if (pthread_mutex_lock (&(thread->threadLock)) == 0)
            {
               /*
                * Since we are starting at the head of the key's threads
                * chain, this will also point key->threads at the next assoc.
                * While we hold key->keyLock, no other thread can insert
                * a new assoc via pthread_setspecific.
                */
               pte_tkAssocDestroy (assoc);
               (void) pthread_mutex_unlock (&(thread->threadLock));
            }
            else
            {
               /* Thread or lock is no longer valid? */
               pte_tkAssocDestroy (assoc);
            }
         }
         pthread_mutex_unlock (&(key->keyLock));
      }

      pte_osTlsFree (key->key);
      if (key->destructor != NULL)
      {
         /* A thread could be holding the keyLock */
         while (EBUSY == (result = pthread_mutex_destroy (&(key->keyLock))))
            pte_osThreadSleep(1); /* Ugly. */
      }

      free (key);
   }

   return (result);
}
