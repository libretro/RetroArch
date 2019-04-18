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

#ifndef MMAL_QUEUE_H
#define MMAL_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalQueue Queues of buffer headers
 * This provides a thread-safe implementation of a queue of buffer headers
 * (\ref MMAL_BUFFER_HEADER_T). The queue works in a first-in, first-out basis
 * so the buffer headers will be dequeued in the order they have been queued. */
/* @{ */

#include "mmal_buffer.h"

typedef struct MMAL_QUEUE_T MMAL_QUEUE_T;

/** Create a queue of MMAL_BUFFER_HEADER_T
 *
 * @return Pointer to the newly created queue or NULL on failure.
 */
MMAL_QUEUE_T *mmal_queue_create(void);

/** Put a MMAL_BUFFER_HEADER_T into a queue
 *
 * @param queue  Pointer to a queue
 * @param buffer Pointer to the MMAL_BUFFER_HEADER_T to add to the queue
 */
void mmal_queue_put(MMAL_QUEUE_T *queue, MMAL_BUFFER_HEADER_T *buffer);

/** Put a MMAL_BUFFER_HEADER_T back at the start of a queue.
 * This is used when a buffer header was removed from the queue but not
 * fully processed and needs to be put back where it was originally taken.
 *
 * @param queue  Pointer to a queue
 * @param buffer Pointer to the MMAL_BUFFER_HEADER_T to add to the queue
 */
void mmal_queue_put_back(MMAL_QUEUE_T *queue, MMAL_BUFFER_HEADER_T *buffer);

/** Get a MMAL_BUFFER_HEADER_T from a queue
 *
 * @param queue  Pointer to a queue
 *
 * @return pointer to the next MMAL_BUFFER_HEADER_T or NULL if the queue is empty.
 */
MMAL_BUFFER_HEADER_T *mmal_queue_get(MMAL_QUEUE_T *queue);

/** Wait for a MMAL_BUFFER_HEADER_T from a queue.
 * This is the same as a get except that this will block until a buffer header is
 * available.
 *
 * @param queue  Pointer to a queue
 *
 * @return pointer to the next MMAL_BUFFER_HEADER_T.
 */
MMAL_BUFFER_HEADER_T *mmal_queue_wait(MMAL_QUEUE_T *queue);

/** Wait for a MMAL_BUFFER_HEADER_T from a queue, up to a given timeout.
 * This is the same as a wait, except that it will abort in case of timeout.
 *
 * @param queue  Pointer to a queue
 * @param timeout Number of milliseconds to wait before
 *                returning if the semaphore can't be acquired.
 *
 * @return pointer to the next MMAL_BUFFER_HEADER_T.
 */
MMAL_BUFFER_HEADER_T *mmal_queue_timedwait(MMAL_QUEUE_T *queue, VCOS_UNSIGNED timeout);

/** Get the number of MMAL_BUFFER_HEADER_T currently in a queue.
 *
 * @param queue  Pointer to a queue
 *
 * @return length (in elements) of the queue.
 */
unsigned int mmal_queue_length(MMAL_QUEUE_T *queue);

/** Destroy a queue of MMAL_BUFFER_HEADER_T.
 *
 * @param queue  Pointer to a queue
 */
void mmal_queue_destroy(MMAL_QUEUE_T *queue);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_QUEUE_H */
