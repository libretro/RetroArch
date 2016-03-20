/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsemaphore.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
   ssem_t *semaphore = (ssem_t*)calloc(1, sizeof(*semaphore));

   if (!semaphore)
      goto error;
   
   semaphore->value   = value;
   semaphore->wakeups = 0;
   semaphore->mutex   = slock_new();

   if (!semaphore->mutex)
      goto error;

   semaphore->cond = scond_new();

   if (!semaphore->cond)
      goto error;

   return semaphore;

error:
   if (semaphore->mutex)
      slock_free(semaphore->mutex);
   semaphore->mutex = NULL;
   if (semaphore)
      free((void*)semaphore);
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
   if (!semaphore)
      return;

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
   if (!semaphore)
      return;

   slock_lock(semaphore->mutex);
   semaphore->value++;

   if (semaphore->value <= 0)
   {
      semaphore->wakeups++;
      scond_signal(semaphore->cond);
   }

   slock_unlock(semaphore->mutex);
}
