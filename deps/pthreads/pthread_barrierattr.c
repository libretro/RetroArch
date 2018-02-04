/*
 * pthread_barrier_attr.c
 *
 * Description:
 * This translation unit implements barrier primitives.
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
pthread_barrierattr_destroy (pthread_barrierattr_t * attr)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      Destroys a barrier attributes object. The object can
 *      no longer be used.
 *
 * PARAMETERS
 *      attr
 *              pointer to an instance of pthread_barrierattr_t
 *
 *
 * DESCRIPTION
 *      Destroys a barrier attributes object. The object can
 *      no longer be used.
 *
 *      NOTES:
 *              1)      Does not affect barrieres created using 'attr'
 *
 * RESULTS
 *              0               successfully released attr,
 *              EINVAL          'attr' is invalid.
 *
 * ------------------------------------------------------
 */
{
   int result = 0;

   if (attr == NULL || *attr == NULL)
      result = EINVAL;
   else
   {
      pthread_barrierattr_t ba = *attr;

      *attr = NULL;
      free (ba);
   }

   return (result);
}

int
pthread_barrierattr_getpshared (const pthread_barrierattr_t * attr,
      int *pshared)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      Determine whether barriers created with 'attr' can be
    *      shared between processes.
    *
    * PARAMETERS
    *      attr
    *              pointer to an instance of pthread_barrierattr_t
    *
    *      pshared
    *              will be set to one of:
    *
    *                      PTHREAD_PROCESS_SHARED
    *                              May be shared if in shared memory
    *
    *                      PTHREAD_PROCESS_PRIVATE
    *                              Cannot be shared.
    *
    *
    * DESCRIPTION
    *      Mutexes creatd with 'attr' can be shared between
    *      processes if pthread_barrier_t variable is allocated
    *      in memory shared by these processes.
    *      NOTES:
    *              1)      pshared barriers MUST be allocated in shared
    *                      memory.
    *              2)      The following macro is defined if shared barriers
    *                      are supported:
    *                              _POSIX_THREAD_PROCESS_SHARED
    *
    * RESULTS
    *              0               successfully retrieved attribute,
    *              EINVAL          'attr' is invalid,
    *
    * ------------------------------------------------------
    */
{
   int result;

   if ((attr != NULL && *attr != NULL) && (pshared != NULL))
   {
      *pshared = (*attr)->pshared;
      result = 0;
   }
   else
      result = EINVAL;

   return (result);
}

int pthread_barrierattr_init (pthread_barrierattr_t * attr)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      Initializes a barrier attributes object with default
    *      attributes.
    *
    * PARAMETERS
    *      attr
    *              pointer to an instance of pthread_barrierattr_t
    *
    *
    * DESCRIPTION
    *      Initializes a barrier attributes object with default
    *      attributes.
    *
    *      NOTES:
    *              1)      Used to define barrier types
    *
    * RESULTS
    *              0               successfully initialized attr,
    *              ENOMEM          insufficient memory for attr.
    *
    * ------------------------------------------------------
    */
{
   int result               = 0;
   pthread_barrierattr_t ba = (pthread_barrierattr_t) calloc (1, sizeof (*ba));

   if (ba == NULL)
      result = ENOMEM;
   else
      ba->pshared = PTHREAD_PROCESS_PRIVATE;

   *attr = ba;

   return (result);
}

int pthread_barrierattr_setpshared (pthread_barrierattr_t * attr, int pshared)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      Barriers created with 'attr' can be shared between
    *      processes if pthread_barrier_t variable is allocated
    *      in memory shared by these processes.
    *
    * PARAMETERS
    *      attr
    *              pointer to an instance of pthread_barrierattr_t
    *
    *      pshared
    *              must be one of:
    *
    *                      PTHREAD_PROCESS_SHARED
    *                              May be shared if in shared memory
    *
    *                      PTHREAD_PROCESS_PRIVATE
    *                              Cannot be shared.
    *
    * DESCRIPTION
    *      Mutexes creatd with 'attr' can be shared between
    *      processes if pthread_barrier_t variable is allocated
    *      in memory shared by these processes.
    *
    *      NOTES:
    *              1)      pshared barriers MUST be allocated in shared
    *                      memory.
    *
    *              2)      The following macro is defined if shared barriers
    *                      are supported:
    *                              _POSIX_THREAD_PROCESS_SHARED
    *
    * RESULTS
    *              0               successfully set attribute,
    *              EINVAL          'attr' or pshared is invalid,
    *              ENOSYS          PTHREAD_PROCESS_SHARED not supported,
    *
    * ------------------------------------------------------
    */
{
   int result;

   if ((attr != NULL && *attr != NULL) &&
         ((pshared == PTHREAD_PROCESS_SHARED) ||
          (pshared == PTHREAD_PROCESS_PRIVATE)))
   {
      if (pshared == PTHREAD_PROCESS_SHARED)
      {

#if !defined( _POSIX_THREAD_PROCESS_SHARED )

         result = ENOSYS;
         pshared = PTHREAD_PROCESS_PRIVATE;

#else

         result = 0;

#endif /* _POSIX_THREAD_PROCESS_SHARED */

      }
      else
         result = 0;

      (*attr)->pshared = pshared;
   }
   else
      result = EINVAL;

   return (result);

}				/* pthread_barrierattr_setpshared */
