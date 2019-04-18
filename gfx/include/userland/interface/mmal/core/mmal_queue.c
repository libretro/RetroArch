/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mmal.h"
#include "mmal_queue.h"

/** Definition of the QUEUE */
struct MMAL_QUEUE_T
{
   VCOS_MUTEX_T lock;
   unsigned int length;
   MMAL_BUFFER_HEADER_T *first;
   MMAL_BUFFER_HEADER_T **last;
   VCOS_SEMAPHORE_T semaphore;
};

// Only sanity check if asserts are enabled
#if VCOS_ASSERT_ENABLED
static void mmal_queue_sanity_check(MMAL_QUEUE_T *queue, MMAL_BUFFER_HEADER_T *buffer)
{
  MMAL_BUFFER_HEADER_T *q;
  int len = 0;
  for (q = queue->first; q && len<queue->length; q = q->next)
  {
    vcos_assert(buffer != q);
    len++;
  }
  vcos_assert(len == queue->length && !q);
}
#else
#define mmal_queue_sanity_check(q,b)
#endif

/** Create a QUEUE of MMAL_BUFFER_HEADER_T */
MMAL_QUEUE_T *mmal_queue_create(void)
{
   MMAL_QUEUE_T *queue;

   queue = vcos_malloc(sizeof(*queue), "MMAL queue");
   if(!queue) return 0;

   if(vcos_mutex_create(&queue->lock, "MMAL queue lock") != VCOS_SUCCESS )
   {
      vcos_free(queue);
      return 0;
   }

   if(vcos_semaphore_create(&queue->semaphore, "MMAL queue sema", 0) != VCOS_SUCCESS )
   {
      vcos_mutex_delete(&queue->lock);
      vcos_free(queue);
      return 0;
   }

   /* gratuitous lock for coverity */ vcos_mutex_lock(&queue->lock);
   queue->length = 0;
   queue->first = 0;
   queue->last = &queue->first;
   mmal_queue_sanity_check(queue, NULL);
   /* gratuitous unlock for coverity */ vcos_mutex_unlock(&queue->lock);

   return queue;
}

/** Put a MMAL_BUFFER_HEADER_T into a QUEUE */
void mmal_queue_put(MMAL_QUEUE_T *queue, MMAL_BUFFER_HEADER_T *buffer)
{
   vcos_assert(queue && buffer);
   if(!queue || !buffer) return;

   vcos_mutex_lock(&queue->lock);
   mmal_queue_sanity_check(queue, buffer);
   queue->length++;
   *queue->last = buffer;
   buffer->next = 0;
   queue->last = &buffer->next;
   // There is a possible advantage to putting the semaphore_post outside
   // the lock as that would avoid the case where we set the semaphore, causing
   // a task switch to a waiting get thread which then blocks because we still
   // have the lock.  However this allows a race condition if we have (legit) code
   // of the (simplified) form:
   // if (mmal_queue_length(q) > 0) b = mmal_queue_get(q);
   // where the _get should always succeed
   // This has an easy fix if we have a fn that returns the count in a semaphore
   // but by not all OSs support that (e.g. Win32)
   vcos_semaphore_post(&queue->semaphore);
   vcos_mutex_unlock(&queue->lock);
}

/** Put a MMAL_BUFFER_HEADER_T back at the start of a QUEUE. */
void mmal_queue_put_back(MMAL_QUEUE_T *queue, MMAL_BUFFER_HEADER_T *buffer)
{
   if(!queue || !buffer) return;

   vcos_mutex_lock(&queue->lock);
   mmal_queue_sanity_check(queue, buffer);
   queue->length++;
   buffer->next = queue->first;
   queue->first = buffer;
   if(queue->last == &queue->first) queue->last = &buffer->next;
   vcos_semaphore_post(&queue->semaphore);
   vcos_mutex_unlock(&queue->lock);
}

/** Get a MMAL_BUFFER_HEADER_T from a QUEUE. Semaphore already claimed */
static MMAL_BUFFER_HEADER_T *mmal_queue_get_core(MMAL_QUEUE_T *queue)
{
   MMAL_BUFFER_HEADER_T * buffer;

   vcos_mutex_lock(&queue->lock);
   mmal_queue_sanity_check(queue, NULL);
   buffer = queue->first;
   vcos_assert(buffer != NULL);

   queue->first = buffer->next;
   if(!queue->first) queue->last = &queue->first;

   queue->length--;
   vcos_mutex_unlock(&queue->lock);

   return buffer;
}

/** Get a MMAL_BUFFER_HEADER_T from a QUEUE. */
MMAL_BUFFER_HEADER_T *mmal_queue_get(MMAL_QUEUE_T *queue)
{
   vcos_assert(queue);
   if(!queue) return 0;

   if(vcos_semaphore_trywait(&queue->semaphore) != VCOS_SUCCESS)
       return NULL;

   return mmal_queue_get_core(queue);
}

/** Wait for a MMAL_BUFFER_HEADER_T from a QUEUE. */
MMAL_BUFFER_HEADER_T *mmal_queue_wait(MMAL_QUEUE_T *queue)
{
	if(!queue) return 0;

   if (vcos_semaphore_wait(&queue->semaphore) != VCOS_SUCCESS)
       return NULL;

   return mmal_queue_get_core(queue);
}

MMAL_BUFFER_HEADER_T *mmal_queue_timedwait(MMAL_QUEUE_T *queue, VCOS_UNSIGNED timeout)
{
    if (!queue)
        return NULL;

    if (vcos_semaphore_wait_timeout(&queue->semaphore, timeout) != VCOS_SUCCESS)
        return NULL;

    return mmal_queue_get_core(queue);
}

/** Get the number of MMAL_BUFFER_HEADER_T currently in a QUEUE */
unsigned int mmal_queue_length(MMAL_QUEUE_T *queue)
{
	if(!queue) return 0;

	return queue->length;
}

/** Destroy a queue of MMAL_BUFFER_HEADER_T */
void mmal_queue_destroy(MMAL_QUEUE_T *queue)
{
   if(!queue) return;
   vcos_mutex_delete(&queue->lock);
   vcos_semaphore_delete(&queue->semaphore);
   vcos_free(queue);
}
