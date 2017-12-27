/*
 * pthread_get.c
 *
 * Description:
 * This translation unit implements miscellaneous thread functions.
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

int pthread_getconcurrency (void)
{
   return pte_concurrency;
}

int pthread_getschedparam (pthread_t thread, int *policy,
      struct sched_param *param)
{
   int result;

   /* Validate the thread id. */
   result = pthread_kill (thread, 0);
   if (0 != result)
      return result;

   /*
    * Validate the policy and param args.
    * Check that a policy constant wasn't passed rather than &policy.
    */
   if (policy <= (int *) SCHED_MAX || param == NULL)
      return EINVAL;

   /* Fill out the policy. */
   *policy = SCHED_OTHER;

   /*
    * This function must return the priority value set by
    * the most recent pthread_setschedparam() or pthread_create()
    * for the target thread. It must not return the actual thread
    * priority as altered by any system priority adjustments etc.
    */
   param->sched_priority = ((pte_thread_t *)thread)->sched_priority;

   return 0;
}

void *pthread_getspecific (pthread_key_t key)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      This function returns the current value of key in the
    *      calling thread. If no value has been set for 'key' in
    *      the thread, NULL is returned.
    *
    * PARAMETERS
    *      key
    *              an instance of pthread_key_t
    *
    *
    * DESCRIPTION
    *      This function returns the current value of key in the
    *      calling thread. If no value has been set for 'key' in
    *      the thread, NULL is returned.
    *
    * RESULTS
    *              key value or NULL on failure
    *
    * ------------------------------------------------------
    */
{
   if (key == NULL)
      return NULL;

   return pte_osTlsGetValue (key->key);
}
