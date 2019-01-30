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
VideoCore OS Abstraction Layer - Queue public header file
=============================================================================*/

#ifndef VCOS_QUEUE_H
#define VCOS_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/** \file vcos_queue.h
  *
  * API for accessing a fixed length queue.
  *
  * Nucleus offers variable length items, but this feature is not used
  * in the current code base, so is withdrawn to simplify the API.
  */

/** Create a fixed length queue.
  *
  * @param queue        Pointer to queue control block
  * @param name         Name of queue
  * @param message_size Size of each queue message item in words (words are sizeof VCOS_UNSIGNED).
  * @param queue_start  Start address of queue area
  * @param queue_size   Size in words (words are sizeof VCOS_UNSIGNED) of queue
  *
  */

VCOS_INLINE_DECL
VCOS_STATUS_T vcos_queue_create(VCOS_QUEUE_T *queue,
                                const char *name,
                                VCOS_UNSIGNED message_size,
                                void *queue_start,
                                VCOS_UNSIGNED queue_size);

/** Delete a queue.
  * @param queue The queue to delete
  */
VCOS_INLINE_DECL
void vcos_queue_delete(VCOS_QUEUE_T *queue);

/** Send an item to a queue. If there is no space, the call with
  * either block waiting for space, or return an error, depending
  * on the value of the wait parameter.
  *
  * @param queue The queue to send to
  * @param src   The data to send (length set when queue was created)
  * @param wait  Whether to wait for space (VCOS_SUSPEND) or fail if
  *              no space (VCOS_NO_SUSPEND).
  *
  * @return If space available, returns VCOS_SUCCESS. Otherwise returns
  * VCOS_EAGAIN if no space available before timeout expires.
  *
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_queue_send(VCOS_QUEUE_T *queue, const void *src, VCOS_UNSIGNED wait);

/** Receive an item from a queue.
  * @param queue The queue to receive from
  * @param dst   Where to write the data to
  * @param wait  Whether to wait (VCOS_SUSPEND) or fail if
  *              empty (VCOS_NO_SUSPEND).
  *
  * @return If an item is available, returns VCOS_SUCCESS. Otherwise returns
  * VCOS_EAGAIN if no item available before timeout expires.
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_queue_receive(VCOS_QUEUE_T *queue, void *dst, VCOS_UNSIGNED wait);

#ifdef __cplusplus
}
#endif
#endif

