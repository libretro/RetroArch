/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Andre Leiradella
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <rthreads/rthreads.h>
#include <rthreads/rsemaphore.h>
#include <async_job.h>

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
   async_job_t *ajob = (async_job_t*)userdata;
   async_job_node_t *node;
   
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
   async_job_t *ajob = (async_job_t*)malloc(sizeof(*ajob));
   
   if (ajob)
   {
      ajob->first = NULL;
      ajob->last = NULL;
      ajob->finish = 0;
      ajob->lock = slock_new();
      
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
   async_job_node_t *node = (async_job_node_t*)malloc(sizeof(*node));

   if (!node)
      return -1;

   node->task    = task;
   node->payload = payload;
   node->next    = NULL;

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
