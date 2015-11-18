/*
  Copyright 2005 Allen B. Downey

    This file contains an example program from The Little Book of
    Semaphores, available from Green Tea Press, greenteapress.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see http://www.gnu.org/licenses/gpl.html
    or write to the Free Software Foundation, Inc., 51 Franklin St, 
    Fifth Floor, Boston, MA  02110-1301  USA
*/

/*  Code taken from http://greenteapress.com/semaphores/semaphore.c
 *  and changed to use libretro-common's mutexes and conditions.
 */

#include <stdlib.h>

#include <rthreads/rthreads.h>
#include <rthreads/rsemaphore.h>

struct ssem
{
   int value;
   int wakeups;
   slock_t *mutex;
   scond_t *cond;
};

ssem_t *ssem_new(int value)
{
   ssem_t *semaphore = (ssem_t*)malloc(sizeof(*semaphore));

   if (semaphore)
   {
      semaphore->value = value;
      semaphore->wakeups = 0;
      semaphore->mutex = slock_new();

      if (semaphore->mutex)
      {
         semaphore->cond = scond_new();

         if (semaphore->cond)
            return semaphore;

         slock_free(semaphore->mutex);
      }

      free((void*)semaphore);
   }

   return NULL;
}

void ssem_free(ssem_t *semaphore)
{
   if (!semaphore)
      return;

   scond_free(semaphore->cond);
   slock_free(semaphore->mutex);
   free((void*)semaphore);
}

void ssem_wait(ssem_t *semaphore)
{
   slock_lock(semaphore->mutex);
   semaphore->value--;

   if (semaphore->value < 0)
   {
      do
      {
         scond_wait(semaphore->cond, semaphore->mutex);
      }while (semaphore->wakeups < 1);

      semaphore->wakeups--;
   }

   slock_unlock(semaphore->mutex);
}

void ssem_signal(ssem_t *semaphore)
{
   slock_lock(semaphore->mutex);
   semaphore->value++;

   if (semaphore->value <= 0)
   {
      semaphore->wakeups++;
      scond_signal(semaphore->cond);
   }

   slock_unlock(semaphore->mutex);
}
