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

/*=============================================================================
VCOS - packet-like messages, based loosely on those found in TRIPOS.

In the simple case, only the server thread creates a message queue, and
clients wait for replies on a semaphore. In more complex cases, clients can
also create message queues (not yet implemented).

Although it's possible for a thread to create multiple queues and listen
on them in turn, if you find yourself doing this it's probably a bug.
=============================================================================*/

#ifndef VCOS_MSGQUEUE_H
#define VCOS_MSGQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vcos_types.h"
#include "vcos.h"
#include "vcos_blockpool.h"

/**
 * \file
 *
 * Packet-like messages, based loosely on those found in TRIPOS and
 * derivatives thereof.
 *
 * A task can send a message *pointer* to another task, where it is
 * queued on a linked list and the task woken up. The receiving task
 * consumes all of the messages on its input queue, and optionally
 * sends back replies using the original message memory.
 *
 * A caller can wait for the reply to a specific message - any other
 * messages that arrive in the meantime are queued separately.
 *
 *
 * All messages have a standard common layout, but the payload area can
 * be used freely to extend this.
 */

#define VCOS_MSGQ_MAGIC 0x5147534d

/** Map the payload portion of a message to a structure pointer.
  */
#define VCOS_MSG_DATA(_msg) (void*)((_msg)->data)

/** Standard message ids - FIXME - these need to be done properly! */
#define VCOS_MSG_N_QUIT            1
#define VCOS_MSG_N_OPEN            2
#define VCOS_MSG_N_CLOSE           3
#define VCOS_MSG_N_PRIVATE         (1<<20)

#define VCOS_MSG_REPLY_BIT         (1<<31)

/** Make gnuc compiler be happy about pointer punning */
#ifdef __GNUC__
#define __VCOS_MAY_ALIAS __attribute__((__may_alias__))
#else
#define __VCOS_MAY_ALIAS
#endif

struct VCOS_MSG_T;

/* Replies go to one of these objects.
 */
typedef struct VCOS_MSG_WAITER_T
{
   /* When the reply is sent, this function gets called with the
    * address of the waiter.
    */
   void (*on_reply)(struct VCOS_MSG_WAITER_T *waiter,
                    struct VCOS_MSG_T *msg);
} VCOS_MSG_WAITER_T;

/** A single message queue.
  */
typedef struct VCOS_MSGQUEUE_T
{
   VCOS_MSG_WAITER_T waiter;           /**< So we can wait on a queue */
   struct VCOS_MSG_T *head;            /**< head of linked list of messages waiting on this queue */
   struct VCOS_MSG_T *tail;            /**< tail of message queue */
   VCOS_SEMAPHORE_T sem;               /**< thread waits on this for new messages */
   VCOS_MUTEX_T lock;                  /**< locks the messages list */
   int attached;                       /**< Is this attached to a thread? */
} VCOS_MSGQUEUE_T;

/** A single message
  */
typedef struct VCOS_MSG_T
{
   uint32_t magic;                     /**< Sanity checking */
   uint32_t code;                      /**< message code */
   struct VCOS_MSG_T *next;            /**< next in queue */
   VCOS_THREAD_T *src_thread;          /**< for debug */
   struct VCOS_MSG_WAITER_T *waiter;   /**< client waiter structure */
   struct VCOS_MSGQ_POOL_T *pool;      /**< Pool allocated from, or NULL */
} VCOS_MSG_T;

#define MSG_REPLY_BIT (1<<31)

/** Initialize a VCOS_MSG_T. Can also use vcos_msg_init().
 */
#define VCOS_MSG_INITIALIZER {VCOS_MSGQ_MAGIC, 0, NULL, NULL, NULL, 0}

/** A pool of messages. This contains its own waiter and
 * semaphore, as well as a blockpool for the actual memory
 * management.
 *
 * When messages are returned to the waiter, it posts the
 * semaphore.
 *
 * When waiting for a message, we just wait on the semaphore.
 * When allocating without waiting, we just try-wait on the
 * semaphore.
 *
 * If we managed to claim the semaphore, then by definition
 * there must be at least that many free messages in the
 * blockpool.
 */
typedef struct VCOS_MSGQ_POOL_T
{
   VCOS_MSG_WAITER_T waiter;
   VCOS_BLOCKPOOL_T blockpool;
   VCOS_SEMAPHORE_T sem;
   uint32_t magic;
} VCOS_MSGQ_POOL_T;

/** Initialise the library. Normally called from vcos_init().
  */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_msgq_init(void);

/** De-initialise the library. Normally called from vcos_deinit().
 */
VCOSPRE_ void VCOSPOST_ vcos_msgq_deinit(void);

/** Send a message.
 *
 * @param dest    Destination message queue
 * @param code    Message code.
 * @param msg     Pointer to message to send. Must not go out of scope before
 *                message is received (do not declare on the stack).
 */
VCOSPRE_ void VCOSPOST_ vcos_msg_send(VCOS_MSGQUEUE_T *dest, uint32_t code, VCOS_MSG_T *msg);

/** Send a message and wait for a reply.
 *
 * @param dest    Destination message queue
 * @param code    Message code.
 * @param msg     Pointer to message to send. May be declared on the stack.
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_msg_sendwait(VCOS_MSGQUEUE_T *queue, uint32_t code, VCOS_MSG_T *msg);

/** Wait for a message on a queue.
  */
VCOSPRE_ VCOS_MSG_T * VCOSPOST_ vcos_msg_wait(VCOS_MSGQUEUE_T *queue);

/** Peek for a message on this thread's endpoint. If a message is not
 * available, NULL is returned. If a message is available it will be
 * removed from the endpoint and returned.
 */
VCOSPRE_ VCOS_MSG_T * VCOSPOST_ vcos_msg_peek(VCOS_MSGQUEUE_T *queue);

/** Send a reply to a message
  */
VCOSPRE_ void VCOSPOST_ vcos_msg_reply(VCOS_MSG_T *msg);

/** Set the reply queue for a message. When the message is replied-to, it
 * will return to the given queue.
 *
 * @param msg      Message
 * @param queue    Message queue the message should return to
 */
VCOSPRE_ void VCOSPOST_ vcos_msg_set_source(VCOS_MSG_T *msg, VCOS_MSGQUEUE_T *queue);

/** Initialise a newly allocated message. This only needs to be called
 * for messages allocated on the stack, heap or statically. It is not
 * needed for messages allocated from a pool.
 */
VCOSPRE_ void VCOSPOST_ vcos_msg_init(VCOS_MSG_T *msg);

/** Create a message queue to wait on.
  */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_msgq_create(VCOS_MSGQUEUE_T *queue, const char *name);

/** Destroy a queue
  */
VCOSPRE_ void  VCOSPOST_ vcos_msgq_delete(VCOS_MSGQUEUE_T *queue);

/*
 * Message pools
 */

/** Create a pool of messages. Messages can be allocated from the pool and
 * sent to a message queue. Replying to the message will automatically
 * free it back to the pool.
 *
 * The pool is threadsafe.
 *
 * @param count          number of messages in the pool
 * @param payload_size   maximum message payload size, not including MSG_T.
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_msgq_pool_create(
   VCOS_MSGQ_POOL_T *pool,
   size_t count,
   size_t payload_size,
   const char *name);

/** Destroy a message pool.
 */
VCOSPRE_ void VCOSPOST_ vcos_msgq_pool_delete(VCOS_MSGQ_POOL_T *pool);

/** Allocate a message from a message pool.
 *
 * Note:
 *
 * If the alloc fails (returns NULL) then your worker thread has stopped
 * servicing requests or your pool is too small for the latency in
 * the system. Your best bet to handle this is to fail the call that
 * needs to send the message.
 *
 * The returned message payload area is initialised to zero.
 *
 * @param  pool  Pool to allocate from.
 * @return Message or NULL if pool exhausted.
 */
VCOSPRE_ VCOS_MSG_T *VCOSPOST_ vcos_msgq_pool_alloc(VCOS_MSGQ_POOL_T *pool);

/** Wait for a message from a message pool. Waits until a
 * message is available in the pool and then allocates it. If
 * one is already available, returns immediately.
 *
 * This call can never fail.
 *
 * The returned message payload area is initialised to zero.
 *
 * @param  pool  Pool to allocate from.
 * @return Message
 */
VCOSPRE_ VCOS_MSG_T *VCOSPOST_ vcos_msgq_pool_wait(VCOS_MSGQ_POOL_T *pool);

/** Explicitly free a message and return it to its pool.
 *
 * @param  msg  Message to free. No-op if NULL.
 */
VCOSPRE_ void VCOSPOST_ vcos_msgq_pool_free(VCOS_MSG_T *msg);

#ifdef __cplusplus
}
#endif
#endif


