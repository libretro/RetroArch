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

#include "vcos.h"
#include "vcos_msgqueue.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define MAGIC VCOS_MSGQ_MAGIC

/* Probably a good idea for MSG_T to be multiple of 8 so that doubles
 * are naturally aligned without problem.
 */
vcos_static_assert((sizeof(VCOS_MSG_T) & 7) == 0);

static void vcos_msgq_pool_on_reply(VCOS_MSG_WAITER_T *waiter,
                                    VCOS_MSG_T *msg);
static void vcos_msgq_queue_waiter_on_reply(VCOS_MSG_WAITER_T *waiter,
                                    VCOS_MSG_T *msg);

/** Simple reply protocol. The client creates a semaphore and waits
 * for it. No queuing of multiple replies is possible but nothing needs
 * to be setup in advance. Because creating semaphores is very fast on
 * VideoCore there's no need to do anything elaborate to optimize create
 * time - this might need revisiting on other platforms.
 */

typedef struct
{
   VCOS_MSG_WAITER_T waiter;
   VCOS_SEMAPHORE_T waitsem;
} VCOS_MSG_SIMPLE_WAITER_T;

static void vcos_msgq_simple_waiter_on_reply(VCOS_MSG_WAITER_T *waiter,
                                             VCOS_MSG_T *msg)
{
   VCOS_MSG_SIMPLE_WAITER_T *self;
   (void)msg;
   self = (VCOS_MSG_SIMPLE_WAITER_T*)waiter;
   vcos_semaphore_post(&self->waitsem);
}

static VCOS_STATUS_T vcos_msgq_simple_waiter_init(VCOS_MSG_SIMPLE_WAITER_T *waiter)
{
   VCOS_STATUS_T status;
   status = vcos_semaphore_create(&waiter->waitsem, "waiter", 0);
   waiter->waiter.on_reply = vcos_msgq_simple_waiter_on_reply;
   return status;
}

static void vcos_msgq_simple_waiter_deinit(VCOS_MSG_SIMPLE_WAITER_T *waiter)
{
   vcos_semaphore_delete(&waiter->waitsem);
}

/*
 * Message queues
 */

static VCOS_STATUS_T vcos_msgq_create_internal(VCOS_MSGQUEUE_T *q, const char *name)
{
   VCOS_STATUS_T st;

   memset(q, 0, sizeof(*q));

   q->waiter.on_reply = vcos_msgq_queue_waiter_on_reply;
   st = vcos_semaphore_create(&q->sem, name, 0);
   if (st != VCOS_SUCCESS)
      goto fail_sem;

   st = vcos_mutex_create(&q->lock, name);
   if (st != VCOS_SUCCESS)
      goto fail_mtx;

   return st;

fail_mtx:
   vcos_semaphore_delete(&q->sem);
fail_sem:
   return st;
}

static void vcos_msgq_delete_internal(VCOS_MSGQUEUE_T *q)
{
   vcos_semaphore_delete(&q->sem);
   vcos_mutex_delete(&q->lock);
}

VCOS_STATUS_T vcos_msgq_create(VCOS_MSGQUEUE_T *q, const char *name)
{
   VCOS_STATUS_T st;

   st = vcos_msgq_create_internal(q, name);

   return st;
}

void vcos_msgq_delete(VCOS_MSGQUEUE_T *q)
{
   vcos_msgq_delete_internal(q);
}

/* append a message to a message queue */
static _VCOS_INLINE void msgq_append(VCOS_MSGQUEUE_T *q, VCOS_MSG_T *msg)
{
   vcos_mutex_lock(&q->lock);
   if (q->head == NULL)
   {
      q->head = q->tail = msg;
   }
   else
   {
      q->tail->next = msg;
      q->tail = msg;
   }
   vcos_mutex_unlock(&q->lock);
}

/*
 * A waiter for a message queue. Just appends the message to the
 * queue, waking up the waiting thread.
 */
static void vcos_msgq_queue_waiter_on_reply(VCOS_MSG_WAITER_T *waiter,
                                            VCOS_MSG_T *msg)
{
   VCOS_MSGQUEUE_T *queue = (VCOS_MSGQUEUE_T*)waiter;
   msgq_append(queue, msg);
   vcos_semaphore_post(&queue->sem);
}

/* initialise this library */

VCOS_STATUS_T vcos_msgq_init(void)
{
   return VCOS_SUCCESS;
}

void vcos_msgq_deinit(void)
{
}

static _VCOS_INLINE
void vcos_msg_send_helper(VCOS_MSG_WAITER_T *waiter,
                          VCOS_MSGQUEUE_T *dest,
                          uint32_t code,
                          VCOS_MSG_T *msg)
{
   vcos_assert(msg);
   vcos_assert(dest);

   msg->code = code;
   if (waiter)
      msg->waiter = waiter;
   msg->next = NULL;
   msg->src_thread = vcos_thread_current();

   msgq_append(dest, msg);
   vcos_semaphore_post(&dest->sem);
}

/* wait on a queue for a message */
VCOS_MSG_T *vcos_msg_wait(VCOS_MSGQUEUE_T *queue)
{
   VCOS_MSG_T *msg;
   vcos_semaphore_wait(&queue->sem);
   vcos_mutex_lock(&queue->lock);

   msg = queue->head;
   vcos_assert(msg);    /* should always be a message here! */

   queue->head = msg->next;
   if (queue->head == NULL)
      queue->tail = NULL;

   vcos_mutex_unlock(&queue->lock);
   return msg;
}

/* peek on a queue for a message */
VCOS_MSG_T *vcos_msg_peek(VCOS_MSGQUEUE_T *queue)
{
   VCOS_MSG_T *msg;
   vcos_mutex_lock(&queue->lock);

   msg = queue->head;

   /* if there's a message, remove it from the queue */
   if (msg)
   {
      queue->head = msg->next;
      if (queue->head == NULL)
         queue->tail = NULL;

      /* keep the semaphore count consistent */

      /* coverity[lock_order]
       * the semaphore must have a non-zero count so cannot block here.
       */
      vcos_semaphore_wait(&queue->sem);
   }

   vcos_mutex_unlock(&queue->lock);
   return msg;
}

void vcos_msg_send(VCOS_MSGQUEUE_T *dest, uint32_t code, VCOS_MSG_T *msg)
{
   vcos_assert(msg->magic == MAGIC);
   vcos_msg_send_helper(NULL, dest, code, msg);
}

/** Send on to the target queue, then wait on a simple waiter for the reply
 */
VCOS_STATUS_T vcos_msg_sendwait(VCOS_MSGQUEUE_T *dest, uint32_t code, VCOS_MSG_T *msg)
{
   VCOS_STATUS_T st;
   VCOS_MSG_SIMPLE_WAITER_T waiter;

   vcos_assert(msg->magic == MAGIC);

   /* if this fires, you've set a waiter up but are now about to obliterate it
    * with the 'wait for a reply' waiter.
    */
   vcos_assert(msg->waiter == NULL);

   if ((st=vcos_msgq_simple_waiter_init(&waiter)) != VCOS_SUCCESS)
      return st;

   vcos_msg_send_helper(&waiter.waiter, dest, code, msg);
   vcos_semaphore_wait(&waiter.waitsem);
   vcos_msgq_simple_waiter_deinit(&waiter);

   return VCOS_SUCCESS;
}

/** Send a reply to a message
  */
void vcos_msg_reply(VCOS_MSG_T *msg)
{
   vcos_assert(msg->magic == MAGIC);
   msg->code |= MSG_REPLY_BIT;
   if (msg->waiter)
   {
      msg->waiter->on_reply(msg->waiter, msg);
   }
   else
   {
      VCOS_ALERT("%s: reply to non-reply message id %d",
                 VCOS_FUNCTION,
                 msg->code);
      vcos_assert(0);
   }
}

void vcos_msg_set_source(VCOS_MSG_T *msg, VCOS_MSGQUEUE_T *queue)
{
   vcos_assert(msg);
   vcos_assert(msg->magic == MAGIC);
   vcos_assert(queue);
   msg->waiter = &queue->waiter;
}

/*
 * Message pools
 */

VCOS_STATUS_T vcos_msgq_pool_create(VCOS_MSGQ_POOL_T *pool,
                                    size_t count,
                                    size_t payload_size,
                                    const char *name)
{
   VCOS_STATUS_T status;
   int bp_size = payload_size + sizeof(VCOS_MSG_T);
   status = vcos_blockpool_create_on_heap(&pool->blockpool,
                                          count, bp_size,
                                          VCOS_BLOCKPOOL_ALIGN_DEFAULT,
                                          0,
                                          name);
   if (status != VCOS_SUCCESS)
      goto fail_pool;

   status = vcos_semaphore_create(&pool->sem, name, count);
   if (status != VCOS_SUCCESS)
      goto fail_sem;

   pool->waiter.on_reply = vcos_msgq_pool_on_reply;
   pool->magic = MAGIC;
   return status;

fail_sem:
   vcos_blockpool_delete(&pool->blockpool);
fail_pool:
   return status;
}

void vcos_msgq_pool_delete(VCOS_MSGQ_POOL_T *pool)
{
   vcos_blockpool_delete(&pool->blockpool);
   vcos_semaphore_delete(&pool->sem);
}

/** Called when a message from a pool is replied-to. Just returns
 * the message back to the blockpool.
 */
static void vcos_msgq_pool_on_reply(VCOS_MSG_WAITER_T *waiter,
                                    VCOS_MSG_T *msg)
{
   vcos_unused(waiter);
   vcos_assert(msg->magic == MAGIC);
   vcos_msgq_pool_free(msg);
}

VCOS_MSG_T *vcos_msgq_pool_alloc(VCOS_MSGQ_POOL_T *pool)
{
   VCOS_MSG_T *msg;
   if (vcos_semaphore_trywait(&pool->sem) == VCOS_SUCCESS)
   {
      msg = vcos_blockpool_calloc(&pool->blockpool);
      vcos_assert(msg);
      msg->magic = MAGIC;
      msg->waiter = &pool->waiter;
      msg->pool = pool;
   }
   else
   {
      msg = NULL;
   }
   return msg;
}

void vcos_msgq_pool_free(VCOS_MSG_T *msg)
{
   if (msg)
   {
      VCOS_MSGQ_POOL_T *pool;
      vcos_assert(msg->pool);

      pool = msg->pool;
      vcos_assert(msg->pool->magic == MAGIC);

      vcos_blockpool_free(msg);
      vcos_semaphore_post(&pool->sem);
   }
}

VCOS_MSG_T *vcos_msgq_pool_wait(VCOS_MSGQ_POOL_T *pool)
{
   VCOS_MSG_T *msg;
   vcos_semaphore_wait(&pool->sem);
   msg = vcos_blockpool_calloc(&pool->blockpool);
   vcos_assert(msg);
   msg->magic = MAGIC;
   msg->waiter = &pool->waiter;
   msg->pool = pool;
   return msg;
}

void vcos_msg_init(VCOS_MSG_T *msg)
{
   msg->magic = MAGIC;
   msg->next = NULL;
   msg->waiter = NULL;
   msg->pool = NULL;
}
