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

#ifndef MMAL_POOL_H
#define MMAL_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalPool Pools of buffer headers
 * A pool of buffer headers is composed of a queue (\ref MMAL_QUEUE_T) and a user
 * specified number of buffer headers (\ref MMAL_BUFFER_HEADER_T). */
/* @{ */

#include "mmal_queue.h"

/** Definition of a pool */
typedef struct MMAL_POOL_T
{
   MMAL_QUEUE_T *queue;             /**< Queue used by the pool */
   uint32_t headers_num;            /**< Number of buffer headers in the pool */
   MMAL_BUFFER_HEADER_T **header;   /**< Array of buffer headers belonging to the pool */
} MMAL_POOL_T;

/** Allocator alloc prototype
 *
 * @param context The context pointer passed in on pool creation.
 * @param size    The size of the allocation required, in bytes.
 * @return The pointer to the newly allocated memory, or NULL on failure.
 */
typedef void *(*mmal_pool_allocator_alloc_t)(void *context, uint32_t size);
/** Allocator free prototype
 *
 * @param context The context pointer passed in on pool creation.
 * @param mem     The pointer to the memory to be released.
 */
typedef void (*mmal_pool_allocator_free_t)(void *context, void *mem);

/** Create a pool of MMAL_BUFFER_HEADER_T.
 * After allocation, all allocated buffer headers will have been added to the queue.
 *
 * It is valid to create a pool with no buffer headers, or with zero size payload buffers.
 * The mmal_pool_resize() function can be used to increase or decrease the number of buffer
 * headers, or the size of the payload buffers, after creation of the pool.
 *
 * The payload buffers may also be allocated independently by the client, and assigned
 * to the buffer headers, but it will be the responsibility of the client to deal with
 * resizing and releasing the memory. It is recommended that mmal_pool_create_with_allocator()
 * is used in this case, supplying allocator function pointers that will be used as
 * necessary by MMAL.
 *
 * @param headers      Number of buffer headers to be allocated with the pool.
 * @param payload_size Size of the payload buffer that will be allocated in 
 *                     each of the buffer headers.
 * @return Pointer to the newly created pool or NULL on failure.
 */
MMAL_POOL_T *mmal_pool_create(unsigned int headers, uint32_t payload_size);

/** Create a pool of MMAL_BUFFER_HEADER_T.
 * After allocation, all allocated buffer headers will have been added to the queue.
 *
 * It is valid to create a pool with no buffer headers, or with zero size payload buffers.
 * The mmal_pool_resize() function can be used to increase or decrease the number of buffer
 * headers, or the size of the payload buffers, after creation of the pool. The allocators
 * passed during creation shall be used when resizing the payload buffers.
 *
 * @param headers      Number of buffer headers to be allocated with the pool.
 * @param payload_size Size of the payload buffer that will be allocated in
 *                     each of the buffer headers.
 * @param allocator_context Pointer to the context of the allocator.
 * @param allocator_alloc   Function pointer for the alloc call of the allocator.
 * @param allocator_free    Function pointer for the free call of the allocator.
 *
 * @return Pointer to the newly created pool or NULL on failure.
 */
MMAL_POOL_T *mmal_pool_create_with_allocator(unsigned int headers, uint32_t payload_size,
                              void *allocator_context, mmal_pool_allocator_alloc_t allocator_alloc,
                              mmal_pool_allocator_free_t allocator_free);

/** Destroy a pool of MMAL_BUFFER_HEADER_T.
 * This will also deallocate all of the memory which was allocated when creating or
 * resizing the pool.
 *
 * If payload buffers have been allocated independently by the client, they should be
 * released prior to calling this function. If the client provided allocator functions,
 * the allocator_free function shall be called for each payload buffer.
 *
 * @param pool  Pointer to a pool
 */
void mmal_pool_destroy(MMAL_POOL_T *pool);

/** Resize a pool of MMAL_BUFFER_HEADER_T.
 * This allows modifying either the number of allocated buffers, the payload size or both at the
 * same time.
 *
 * @param pool         Pointer to the pool
 * @param headers      New number of buffer headers to be allocated in the pool.
 *                     It is not valid to pass zero for the number of buffers.
 * @param payload_size Size of the payload buffer that will be allocated in
 *                     each of the buffer headers.
 *                     If this is set to 0, all payload buffers shall be released.
 * @return MMAL_SUCCESS or an error on failure.
 */
MMAL_STATUS_T mmal_pool_resize(MMAL_POOL_T *pool, unsigned int headers, uint32_t payload_size);

/** Definition of the callback used by a pool to signal back to the user that a buffer header
 * has been released back to the pool.
 *
 * @param pool       Pointer to the pool
 * @param buffer     Buffer header just released
 * @param userdata   User specific data passed in when setting the callback
 * @return True to have the buffer header put back in the pool's queue, false if the buffer
 *          header has been taken within the callback.
 */
typedef MMAL_BOOL_T (*MMAL_POOL_BH_CB_T)(MMAL_POOL_T *pool, MMAL_BUFFER_HEADER_T *buffer, void *userdata);

/** Set a buffer header release callback to the pool.
 * Each time a buffer header is released to the pool, the callback will be triggered.
 *
 * @param pool     Pointer to a pool
 * @param cb       Callback function
 * @param userdata User specific data which will be passed with each callback
 */
void mmal_pool_callback_set(MMAL_POOL_T *pool, MMAL_POOL_BH_CB_T cb, void *userdata);

/** Set a pre-release callback for all buffer headers in the pool.
 * Each time a buffer header is about to be released to the pool, the callback
 * will be triggered.
 *
 * @param pool     Pointer to the pool
 * @param cb       Pre-release callback function
 * @param userdata User-specific data passed back with each callback
 */
void mmal_pool_pre_release_callback_set(MMAL_POOL_T *pool, MMAL_BH_PRE_RELEASE_CB_T cb, void *userdata);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_POOL_H */
