/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (async_job.c).
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
#include <rthreads/async_job.h>

typedef struct async_job_node async_job_node_t;

struct async_job_node
{
   async_task_t task;
   void *payload;
   async_job_node_t *next;
};

struct async_job
{
   async_job_node_t *first;
   async_job_node_t *last;
   volatile int finish;
   slock_t *lock;
   ssem_t *sem;
   sthread_t* thread;
};

static void async_job_processor(void *userdata)
{
   async_job_node_t *node;
   async_job_t      *ajob = (async_job_t*)userdata;
   
   for (;;)
   {
      ssem_wait(ajob->sem);
      
      if (ajob->finish)
         return;
      
      slock_lock(ajob->lock);
      
      node = ajob->first;
      ajob->first = node->next;
      
      slock_unlock(ajob->lock);
      
      node->task(node->payload);
      free((void*)node);
   }
}

async_job_t *async_job_new(void)
{
   async_job_t *ajob = (async_job_t*)calloc(1, sizeof(*ajob));
   
   if (ajob)
   {
      ajob->lock   = slock_new();
      
      if (ajob->lock)
      {
         ajob->sem = ssem_new(0);
         
         if (ajob->sem)
         {
            ajob->thread = sthread_create(async_job_processor, (void*)ajob);
            
            if (ajob->thread)
               return ajob;
            
            ssem_free(ajob->sem);
         }
         
         slock_free(ajob->lock);
      }
      
      free((void*)ajob);
   }
   
   return NULL;
}

void async_job_free(async_job_t *ajob)
{
   ajob->finish = 1;
   ssem_signal(ajob->sem);
   sthread_join(ajob->thread);
   ssem_free(ajob->sem);
   free((void*)ajob);
}

int async_job_add(async_job_t *ajob, async_task_t task, void *payload)
{
   async_job_node_t *node;
   
   if (!ajob)
      return -1;
   
   node = (async_job_node_t*)calloc(1, sizeof(*node));
   
   if (!node)
      return -1;

   node->task    = task;
   node->payload = payload;

   slock_lock(ajob->lock);

   if (ajob->first)
   {
      ajob->last->next = node;
      ajob->last       = node;
   }
   else
      ajob->first      = ajob->last = node;

   slock_unlock(ajob->lock);
   ssem_signal(ajob->sem);

   return 0;
}
