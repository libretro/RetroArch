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
#include "mmal_pool.h"
#include "core/mmal_buffer_private.h"
#include "mmal_logging.h"

/** Definition of a pool */
typedef struct MMAL_POOL_PRIVATE_T
{
   MMAL_POOL_T pool; /**< Actual pool */

   MMAL_POOL_BH_CB_T cb; /**< Buffer header release callback */
   void *userdata;       /**< User provided data to pass with callback */

   mmal_pool_allocator_alloc_t allocator_alloc; /**< Allocator for the payload buffers */
   mmal_pool_allocator_free_t allocator_free;   /**< Allocator for the payload buffers */
   void *allocator_context;                     /**< Context for the allocator */

   unsigned int header_size; /**< Size of an initialised buffer header structure */
   unsigned int payload_size;

   unsigned int headers_alloc_num; /**< Number of buffer headers allocated as part of the private structure */

} MMAL_POOL_PRIVATE_T;

#define ROUND_UP(s,align) ((((unsigned long)(s)) & ~((align)-1)) + (align))
#define ALIGN  8

static void mmal_pool_buffer_header_release(MMAL_BUFFER_HEADER_T *header);

static void *mmal_pool_allocator_default_alloc(void *context, uint32_t size)
{
   MMAL_PARAM_UNUSED(context);
   return vcos_malloc(size, "mmal_pool payload");
}

static void mmal_pool_allocator_default_free(void *context, void *mem)
{
   MMAL_PARAM_UNUSED(context);
   vcos_free(mem);
}

static MMAL_STATUS_T mmal_pool_initialise_buffer_headers(MMAL_POOL_T *pool, unsigned int headers,
                                                         MMAL_BOOL_T reinitialise)
{
   MMAL_POOL_PRIVATE_T *private = (MMAL_POOL_PRIVATE_T *)pool;
   MMAL_BUFFER_HEADER_T *header;
   uint8_t *payload = NULL;
   unsigned int i;

   header = (MMAL_BUFFER_HEADER_T *)((uint8_t *)pool->header + ROUND_UP(sizeof(void *)*headers,ALIGN));

   for (i = 0; i < headers; i++)
   {
      if (reinitialise)
         header = mmal_buffer_header_initialise(header, private->header_size);

      if (private->payload_size && private->allocator_alloc)
      {
         LOG_TRACE("allocating %u bytes for payload %u/%u", private->payload_size, i, headers);
         payload = (uint8_t*)private->allocator_alloc(private->allocator_context, private->payload_size);
         if (! payload)
         {
            LOG_ERROR("failed to allocate payload %u/%u", i, headers);
            return MMAL_ENOMEM;
         }
      }
      else
      {
         if (header->priv->pf_payload_free && header->priv->payload && header->priv->payload_size)
         {
            LOG_TRACE("freeing %u bytes for payload %u/%u", header->priv->payload_size, i, headers);
            header->priv->pf_payload_free(header->priv->payload_context, header->priv->payload);
         }
      }
      header->data = payload;
      header->alloc_size = private->payload_size;
      header->priv->pf_release = mmal_pool_buffer_header_release;
      header->priv->owner = (void *)pool;
      header->priv->refcount = 1;
      header->priv->payload = payload;
      header->priv->payload_context = private->allocator_context;
      header->priv->pf_payload_free = private->allocator_free;
      header->priv->payload_size = private->payload_size;
      pool->header[i] = header;
      pool->headers_num = i+1;
      header = (MMAL_BUFFER_HEADER_T *)((uint8_t*)header + private->header_size);
   }

   return MMAL_SUCCESS;
}

/** Create a pool of MMAL_BUFFER_HEADER_T */
MMAL_POOL_T *mmal_pool_create(unsigned int headers, uint32_t payload_size)
{
   return mmal_pool_create_with_allocator(headers, payload_size, NULL,
             mmal_pool_allocator_default_alloc, mmal_pool_allocator_default_free);
}

/** Create a pool of MMAL_BUFFER_HEADER_T */
MMAL_POOL_T *mmal_pool_create_with_allocator(unsigned int headers, uint32_t payload_size,
                              void *allocator_context, mmal_pool_allocator_alloc_t allocator_alloc,
                              mmal_pool_allocator_free_t allocator_free)
{
   unsigned int i, headers_array_size, header_size, pool_size;
   MMAL_POOL_PRIVATE_T *private;
   MMAL_BUFFER_HEADER_T **array;
   MMAL_POOL_T *pool;
   MMAL_QUEUE_T *queue;

   queue = mmal_queue_create();
   if (!queue)
   {
      LOG_ERROR("failed to create queue");
      return NULL;
   }

   /* Calculate how much memory we need */
   pool_size = ROUND_UP(sizeof(MMAL_POOL_PRIVATE_T),ALIGN);
   headers_array_size = ROUND_UP(sizeof(void *)*headers,ALIGN);
   header_size = ROUND_UP(mmal_buffer_header_size(0),ALIGN);

   LOG_TRACE("allocating %u + %u + %u * %u bytes for pool",
             pool_size, headers_array_size, header_size, headers);
   private = vcos_calloc(pool_size, 1, "MMAL pool");
   array = vcos_calloc(headers_array_size + header_size * headers, 1, "MMAL buffer headers");
   if (!private || !array)
   {
      LOG_ERROR("failed to allocate pool");
      if (private) vcos_free(private);
      if (array) vcos_free(array);
      mmal_queue_destroy(queue);
      return NULL;
   }
   pool = &private->pool;
   pool->queue = queue;
   pool->header = (MMAL_BUFFER_HEADER_T **)array;
   private->header_size = header_size;
   private->payload_size = payload_size;
   private->headers_alloc_num = headers;

   /* Use default allocators if none has been specified by client */
   if (!allocator_alloc || !allocator_free)
   {
      allocator_alloc = mmal_pool_allocator_default_alloc;
      allocator_free = mmal_pool_allocator_default_free;
      allocator_context = NULL;
   }

   /* Keep reference to the allocator to allow resizing the payloads at a later point */
   private->allocator_alloc = allocator_alloc;
   private->allocator_free = allocator_free;
   private->allocator_context = allocator_context;

   if (mmal_pool_initialise_buffer_headers(pool, headers, 1) != MMAL_SUCCESS)
   {
      mmal_pool_destroy(pool);
      return NULL;
   }

   /* Add all the headers to the queue */
   for (i = 0; i < pool->headers_num; i++)
      mmal_queue_put(queue, pool->header[i]);

   return pool;
}

/** Destroy a pool of MMAL_BUFFER_HEADER_T */
void mmal_pool_destroy(MMAL_POOL_T *pool)
{
   unsigned int i;

   if (!pool)
      return;

   /* If the payload_size is non-zero then the buffer header payload
    * must be freed. Otherwise it is the caller's responsibility. */
   for (i = 0; i < pool->headers_num; ++i)
   {
      MMAL_BUFFER_HEADER_PRIVATE_T* priv = pool->header[i]->priv;

      if (priv->pf_payload_free && priv->payload && priv->payload_size)
         priv->pf_payload_free(priv->payload_context, priv->payload);
   }

   if (pool->header)
      vcos_free(pool->header);

   if(pool->queue) mmal_queue_destroy(pool->queue);
   vcos_free(pool);
}

/** Resize a pool of MMAL_BUFFER_HEADER_T */
MMAL_STATUS_T mmal_pool_resize(MMAL_POOL_T *pool, unsigned int headers, uint32_t payload_size)
{
   MMAL_POOL_PRIVATE_T *private = (MMAL_POOL_PRIVATE_T *)pool;
   unsigned int i;

   if (!private || !headers)
      return MMAL_EINVAL;

   /* Check if anything needs to be done */
   if (headers == pool->headers_num && payload_size == private->payload_size)
      return MMAL_SUCCESS;

   /* Remove all the headers from the queue */
   for (i = 0; i < pool->headers_num; i++)
      mmal_queue_get(pool->queue);

   /* Start by freeing the current payloads */
   private->payload_size = 0;
   mmal_pool_initialise_buffer_headers(pool, pool->headers_num, 0);
   pool->headers_num = 0;

   /* Check if we need to reallocate the buffer headers themselves */
   if (headers > private->headers_alloc_num)
   {
      private->headers_alloc_num = 0;
      if (pool->header)
         vcos_free(pool->header);
      pool->header =
         vcos_calloc(private->header_size * headers + ROUND_UP(sizeof(void *)*headers,ALIGN),
                     1, "MMAL buffer headers");
      if (!pool->header)
         return MMAL_ENOMEM;
      private->headers_alloc_num = headers;
   }

   /* Allocate the new payloads */
   private->payload_size = payload_size;
   mmal_pool_initialise_buffer_headers(pool, headers, 1);

   /* Add all the headers to the queue */
   for (i = 0; i < pool->headers_num; i++)
      mmal_queue_put(pool->queue, pool->header[i]);

   return MMAL_SUCCESS;
}

/** Buffer header release callback.
 * Call out to a further client callback and put the buffer back in the queue
 * so it can be reused, unless the client callback prevents it. */
static void mmal_pool_buffer_header_release(MMAL_BUFFER_HEADER_T *header)
{
   MMAL_POOL_T *pool = (MMAL_POOL_T *)header->priv->owner;
   MMAL_POOL_PRIVATE_T *private = (MMAL_POOL_PRIVATE_T *)pool;
   MMAL_BOOL_T queue_buffer = 1;

   header->priv->refcount = 1;
   if(private->cb)
      queue_buffer = private->cb(pool, header, private->userdata);
   if (queue_buffer)
      mmal_queue_put(pool->queue, header);
}

/** Set a buffer header release callback to the pool */
void mmal_pool_callback_set(MMAL_POOL_T *pool, MMAL_POOL_BH_CB_T cb, void *userdata)
{
   MMAL_POOL_PRIVATE_T *private = (MMAL_POOL_PRIVATE_T *)pool;
   private->cb = cb;
   private->userdata = userdata;
}

/* Set a pre-release callback for all buffer headers in the pool */
void mmal_pool_pre_release_callback_set(MMAL_POOL_T *pool, MMAL_BH_PRE_RELEASE_CB_T cb, void *userdata)
{
   unsigned int i;
   MMAL_POOL_PRIVATE_T *private = (MMAL_POOL_PRIVATE_T *)pool;
   MMAL_BUFFER_HEADER_T *header =
         (MMAL_BUFFER_HEADER_T*)((uint8_t*)pool->header + ROUND_UP(sizeof(void*)*pool->headers_num,ALIGN));

   for (i = 0; i < pool->headers_num; ++i)
   {
      mmal_buffer_header_pre_release_cb_set(header, cb, userdata);
      header = (MMAL_BUFFER_HEADER_T *)((uint8_t*)header + private->header_size);
   }
}
