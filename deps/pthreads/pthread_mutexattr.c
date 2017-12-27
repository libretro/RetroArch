/*
 * pthread_mutexattr.c
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

#include <stdlib.h>

#include "pthread.h"
#include "implement.h"

int pthread_mutexattr_destroy (pthread_mutexattr_t * attr)
/*
 * ------------------------------------------------------
 * DOCPUBLIC
 *      Destroys a mutex attributes object. The object can
 *      no longer be used.
 *
 * PARAMETERS
 *      attr
 *              pointer to an instance of pthread_mutexattr_t
 *
 *
 * DESCRIPTION
 *      Destroys a mutex attributes object. The object can
 *      no longer be used.
 *
 *      NOTES:
 *              1)      Does not affect mutexes created using 'attr'
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
      pthread_mutexattr_t ma = *attr;

      *attr = NULL;
      free (ma);
   }

   return (result);
}

int pthread_mutexattr_getkind_np (pthread_mutexattr_t * attr, int *kind)
{
   return pthread_mutexattr_gettype (attr, kind);
}

int pthread_mutexattr_gettype (pthread_mutexattr_t * attr, int *kind)
{
   int result = 0;

   if (attr != NULL && *attr != NULL && kind != NULL)
      *kind = (*attr)->kind;
   else
      result = EINVAL;

   return (result);
}
int pthread_mutexattr_getpshared (const pthread_mutexattr_t * attr, int *pshared)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      Determine whether mutexes created with 'attr' can be
    *      shared between processes.
    *
    * PARAMETERS
    *      attr
    *              pointer to an instance of pthread_mutexattr_t
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
    *      processes if pthread_mutex_t variable is allocated
    *      in memory shared by these processes.
    *      NOTES:
    *              1)      pshared mutexes MUST be allocated in shared
    *                      memory.
    *              2)      The following macro is defined if shared mutexes
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

int pthread_mutexattr_init (pthread_mutexattr_t * attr)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      Initializes a mutex attributes object with default
    *      attributes.
    *
    * PARAMETERS
    *      attr
    *              pointer to an instance of pthread_mutexattr_t
    *
    *
    * DESCRIPTION
    *      Initializes a mutex attributes object with default
    *      attributes.
    *
    *      NOTES:
    *              1)      Used to define mutex types
    *
    * RESULTS
    *              0               successfully initialized attr,
    *              ENOMEM          insufficient memory for attr.
    *
    * ------------------------------------------------------
    */
{
   int result = 0;
   pthread_mutexattr_t ma = (pthread_mutexattr_t) calloc (1, sizeof (*ma));

   if (ma == NULL)
      result = ENOMEM;
   else
   {
      ma->pshared = PTHREAD_PROCESS_PRIVATE;
      ma->kind = PTHREAD_MUTEX_DEFAULT;
   }

   *attr = ma;

   return (result);
}

int pthread_mutexattr_setkind_np (pthread_mutexattr_t * attr, int kind)
{
   return pthread_mutexattr_settype (attr, kind);
}

int pthread_mutexattr_setpshared (pthread_mutexattr_t * attr, int pshared)
   /*
    * ------------------------------------------------------
    * DOCPUBLIC
    *      Mutexes created with 'attr' can be shared between
    *      processes if pthread_mutex_t variable is allocated
    *      in memory shared by these processes.
    *
    * PARAMETERS
    *      attr
    *              pointer to an instance of pthread_mutexattr_t
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
    *      processes if pthread_mutex_t variable is allocated
    *      in memory shared by these processes.
    *
    *      NOTES:
    *              1)      pshared mutexes MUST be allocated in shared
    *                      memory.
    *
    *              2)      The following macro is defined if shared mutexes
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
}

int pthread_mutexattr_settype (pthread_mutexattr_t * attr, int kind)
   /*
    * ------------------------------------------------------
    *
    * DOCPUBLIC
    * The pthread_mutexattr_settype() and
    * pthread_mutexattr_gettype() functions  respectively set and
    * get the mutex type  attribute. This attribute is set in  the
    * type parameter to these functions.
    *
    * PARAMETERS
    *      attr
    *              pointer to an instance of pthread_mutexattr_t
    *
    *      type
    *              must be one of:
    *
    *                      PTHREAD_MUTEX_DEFAULT
    *
    *                      PTHREAD_MUTEX_NORMAL
    *
    *                      PTHREAD_MUTEX_ERRORCHECK
    *
    *                      PTHREAD_MUTEX_RECURSIVE
    *
    * DESCRIPTION
    * The pthread_mutexattr_settype() and
    * pthread_mutexattr_gettype() functions  respectively set and
    * get the mutex type  attribute. This attribute is set in  the
    * type  parameter to these functions. The default value of the
    * type  attribute is  PTHREAD_MUTEX_DEFAULT.
    *
    * The type of mutex is contained in the type  attribute of the
    * mutex attributes. Valid mutex types include:
    *
    * PTHREAD_MUTEX_NORMAL
    *          This type of mutex does  not  detect  deadlock.  A
    *          thread  attempting  to  relock  this mutex without
    *          first unlocking it will  deadlock.  Attempting  to
    *          unlock  a  mutex  locked  by  a  different  thread
    *          results  in  undefined  behavior.  Attempting   to
    *          unlock  an  unlocked  mutex  results  in undefined
    *          behavior.
    *
    * PTHREAD_MUTEX_ERRORCHECK
    *          This type of  mutex  provides  error  checking.  A
    *          thread  attempting  to  relock  this mutex without
    *          first unlocking it will return with  an  error.  A
    *          thread  attempting to unlock a mutex which another
    *          thread has locked will return  with  an  error.  A
    *          thread attempting to unlock an unlocked mutex will
    *          return with an error.
    *
    * PTHREAD_MUTEX_DEFAULT
    *          Same as PTHREAD_MUTEX_NORMAL.
    *
    * PTHREAD_MUTEX_RECURSIVE
    *          A thread attempting to relock this  mutex  without
    *          first  unlocking  it  will  succeed in locking the
    *          mutex. The relocking deadlock which can occur with
    *          mutexes of type  PTHREAD_MUTEX_NORMAL cannot occur
    *          with this type of mutex. Multiple  locks  of  this
    *          mutex  require  the  same  number  of  unlocks  to
    *          release  the  mutex  before  another  thread   can
    *          acquire the mutex. A thread attempting to unlock a
    *          mutex which another thread has locked will  return
    *          with  an  error. A thread attempting to  unlock an
    *          unlocked mutex will return  with  an  error.  This
    *          type  of mutex is only supported for mutexes whose
    *          process        shared         attribute         is
    *          PTHREAD_PROCESS_PRIVATE.
    *
   * RESULTS
   *              0               successfully set attribute,
   *              EINVAL          'attr' or 'type' is invalid,
   *
   * ------------------------------------------------------
   */
{
   int result = 0;

   if ((attr != NULL && *attr != NULL))
   {
      switch (kind)
      {
         case PTHREAD_MUTEX_FAST_NP:
         case PTHREAD_MUTEX_RECURSIVE_NP:
         case PTHREAD_MUTEX_ERRORCHECK_NP:
            (*attr)->kind = kind;
            break;
         default:
            result = EINVAL;
            break;
      }
   }
   else
      result = EINVAL;

   return (result);
}
