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

#define VCOS_LOG_CATEGORY (&vcos_blockpool_log)

#include <stddef.h>
#include <string.h>
#include "interface/vcos/vcos.h"
#include "interface/vcos/generic/vcos_generic_blockpool.h"

#define VCOS_BLOCKPOOL_FOURCC(a,b,c,d) ((a) | (b << 8) | (c << 16) | (d << 24))
#define VCOS_BLOCKPOOL_MAGIC           VCOS_BLOCKPOOL_FOURCC('v', 'b', 'p', 'l')
#define VCOS_BLOCKPOOL_SUBPOOL_MAGIC   VCOS_BLOCKPOOL_FOURCC('v', 's', 'p', 'l')

#define VCOS_BLOCKPOOL_SUBPOOL_FLAG_NONE        (0)
#define VCOS_BLOCKPOOL_SUBPOOL_FLAG_OWNS_MEM    (1 << 0)
#define VCOS_BLOCKPOOL_SUBPOOL_FLAG_EXTENSION   (1 << 1)

/* Uncomment to enable really verbose debug messages */
/* #define VCOS_BLOCKPOOL_DEBUGGING */
/* Whether to overwrite freed blocks with 0xBD */
#ifdef VCOS_BLOCKPOOL_DEBUGGING
#define VCOS_BLOCKPOOL_OVERWRITE_ON_FREE        1
#define VCOS_BLOCKPOOL_DEBUG_MEMSET_MAX_SIZE    (UINT32_MAX)
#else
#define VCOS_BLOCKPOOL_OVERWRITE_ON_FREE        0
#define VCOS_BLOCKPOOL_DEBUG_MEMSET_MAX_SIZE    (2 * 1024 * 1024)
#endif

#ifdef VCOS_BLOCKPOOL_DEBUGGING
#define VCOS_BLOCKPOOL_ASSERT vcos_demand
#define VCOS_BLOCKPOOL_TRACE_LEVEL VCOS_LOG_TRACE
#define VCOS_BLOCKPOOL_DEBUG_LOG(s, ...) vcos_log_trace("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#undef  VCOS_BLOCKPOOL_OVERWRITE_ON_FREE
#define VCOS_BLOCKPOOL_OVERWRITE_ON_FREE 1
#else
#define VCOS_BLOCKPOOL_ASSERT vcos_demand
#define VCOS_BLOCKPOOL_TRACE_LEVEL VCOS_LOG_ERROR
#define VCOS_BLOCKPOOL_DEBUG_LOG(s, ...)
#endif

#define ASSERT_POOL(p) \
   VCOS_BLOCKPOOL_ASSERT((p) && (p)->magic == VCOS_BLOCKPOOL_MAGIC);

#define ASSERT_SUBPOOL(p) \
   VCOS_BLOCKPOOL_ASSERT((p) && (p)->magic == VCOS_BLOCKPOOL_SUBPOOL_MAGIC && \
         p->start >= p->mem);

#if defined(VCOS_LOGGING_ENABLED)
static VCOS_LOG_CAT_T vcos_blockpool_log =
VCOS_LOG_INIT("vcos_blockpool", VCOS_BLOCKPOOL_TRACE_LEVEL);
#endif

static void vcos_generic_blockpool_subpool_init(
      VCOS_BLOCKPOOL_T *pool, VCOS_BLOCKPOOL_SUBPOOL_T *subpool,
      void *mem, size_t pool_size, VCOS_UNSIGNED num_blocks, int align,
      uint32_t flags)
{
   VCOS_BLOCKPOOL_HEADER_T *block;
   VCOS_BLOCKPOOL_HEADER_T *end;

   vcos_unused(flags);

   vcos_log_trace(
         "%s: pool %p subpool %p mem %p pool_size %d " \
         "num_blocks %d align %d flags %x",
         VCOS_FUNCTION,
         pool, subpool, mem, (uint32_t) pool_size,
         num_blocks, align, flags);

   subpool->magic = VCOS_BLOCKPOOL_SUBPOOL_MAGIC;
   subpool->mem = mem;

   /* The block data pointers must be aligned according to align and the
    * block header pre-preceeds the first block data.
    * For large alignments there may be wasted space between subpool->mem
    * and the first block header.
    */
   subpool->start = (char *) subpool->mem + sizeof(VCOS_BLOCKPOOL_HEADER_T);
   subpool->start = (void*)
      VCOS_BLOCKPOOL_ROUND_UP((unsigned long) subpool->start, align);
   subpool->start = (char *) subpool->start - sizeof(VCOS_BLOCKPOOL_HEADER_T);

   vcos_assert(subpool->start >= subpool->mem);

   vcos_log_trace("%s: mem %p subpool->start %p" \
         " pool->block_size %d pool->block_data_size %d",
         VCOS_FUNCTION, mem, subpool->start,
         (int) pool->block_size, (int) pool->block_data_size);

   subpool->num_blocks = num_blocks;
   subpool->available_blocks = num_blocks;
   subpool->free_list = NULL;
   subpool->owner = pool;

   /* Initialise to a predictable bit pattern unless the pool is so big
    * that the delay would be noticeable. */
   if (pool_size < VCOS_BLOCKPOOL_DEBUG_MEMSET_MAX_SIZE)
      memset(subpool->mem, 0xBC, pool_size); /* For debugging */

   block = (VCOS_BLOCKPOOL_HEADER_T*) subpool->start;
   end = (VCOS_BLOCKPOOL_HEADER_T*)
      ((char *) subpool->start + (pool->block_size * num_blocks));
   subpool->end = end;

   /* Initialise the free list for this subpool */
   while (block < end)
   {
      block->owner.next = subpool->free_list;
      subpool->free_list = block;
      block = (VCOS_BLOCKPOOL_HEADER_T*)((char*) block + pool->block_size);
   }

}

VCOS_STATUS_T vcos_generic_blockpool_init(VCOS_BLOCKPOOL_T *pool,
      VCOS_UNSIGNED num_blocks, VCOS_UNSIGNED block_size,
      void *start, VCOS_UNSIGNED pool_size, VCOS_UNSIGNED align,
      VCOS_UNSIGNED flags, const char *name)
{
   VCOS_STATUS_T status = VCOS_SUCCESS;

   vcos_unused(name);
   vcos_unused(flags);

   vcos_log_trace(
         "%s: pool %p num_blocks %d block_size %d start %p pool_size %d name %p",
         VCOS_FUNCTION, pool, num_blocks, block_size, start, pool_size, name);

   vcos_demand(pool);
   vcos_demand(start);
   vcos_assert((block_size > 0));
   vcos_assert(num_blocks > 0);

   if (! align)
      align = VCOS_BLOCKPOOL_ALIGN_DEFAULT;

   if (align & 0x3)
   {
      vcos_log_error("%s: invalid alignment %d", VCOS_FUNCTION, align);
      return VCOS_EINVAL;
   }

   if (VCOS_BLOCKPOOL_SIZE(num_blocks, block_size, align) > pool_size)
   {
      vcos_log_error("%s: Pool is too small" \
            " num_blocks %d block_size %d align %d"
            " pool_size %d required size %d", VCOS_FUNCTION,
            num_blocks, block_size, align,
            pool_size, (int) VCOS_BLOCKPOOL_SIZE(num_blocks, block_size, align));
      return VCOS_ENOMEM;
   }

   status = vcos_mutex_create(&pool->mutex, "vcos blockpool mutex");
   if (status != VCOS_SUCCESS)
      return status;

   pool->block_data_size = block_size;

   /* TODO - create flag that if set forces the header to be in its own cache
    * line */
   pool->block_size = VCOS_BLOCKPOOL_ROUND_UP(pool->block_data_size +
         (align >= 4096 ? 32 : 0) +
         sizeof(VCOS_BLOCKPOOL_HEADER_T), align);

   pool->magic = VCOS_BLOCKPOOL_MAGIC;
   pool->num_subpools = 1;
   pool->num_extension_blocks = 0;
   pool->align = align;
   memset(pool->subpools, 0, sizeof(pool->subpools));

   vcos_generic_blockpool_subpool_init(pool, &pool->subpools[0], start,
         pool_size, num_blocks, align, VCOS_BLOCKPOOL_SUBPOOL_FLAG_NONE);

   return status;
}

VCOS_STATUS_T vcos_generic_blockpool_create_on_heap(VCOS_BLOCKPOOL_T *pool,
      VCOS_UNSIGNED num_blocks, VCOS_UNSIGNED block_size, VCOS_UNSIGNED align,
      VCOS_UNSIGNED flags, const char *name)
{
   VCOS_STATUS_T status = VCOS_SUCCESS;
   size_t size = VCOS_BLOCKPOOL_SIZE(num_blocks, block_size, align);
   void* mem = vcos_malloc(size, name);

   vcos_log_trace("%s: num_blocks %d block_size %d name %s",
         VCOS_FUNCTION, num_blocks, block_size, name);

   if (! mem)
      return VCOS_ENOMEM;

   status = vcos_generic_blockpool_init(pool, num_blocks,
         block_size, mem, size, align, flags, name);

   if (status != VCOS_SUCCESS)
      goto fail;

   pool->subpools[0].flags |= VCOS_BLOCKPOOL_SUBPOOL_FLAG_OWNS_MEM;
   return status;

fail:
   vcos_free(mem);
   return status;
}

VCOS_STATUS_T vcos_generic_blockpool_extend(VCOS_BLOCKPOOL_T *pool,
      VCOS_UNSIGNED num_extensions, VCOS_UNSIGNED num_blocks)
{
   VCOS_UNSIGNED i;
   ASSERT_POOL(pool);

   vcos_log_trace("%s: pool %p num_extensions %d num_blocks %d",
         VCOS_FUNCTION, pool, num_extensions, num_blocks);

   /* Extend may only be called once */
   if (pool->num_subpools > 1)
      return VCOS_EACCESS;

   if (num_extensions < 1 ||
         num_extensions > VCOS_BLOCKPOOL_MAX_SUBPOOLS - 1)
      return VCOS_EINVAL;

   if (num_blocks < 1)
      return VCOS_EINVAL;

   pool->num_subpools += num_extensions;
   pool->num_extension_blocks = num_blocks;

   /* Mark these subpools as valid but unallocated */
   for (i = 1; i < pool->num_subpools; ++i)
   {
      pool->subpools[i].magic = VCOS_BLOCKPOOL_SUBPOOL_MAGIC;
      pool->subpools[i].start = NULL;
      pool->subpools[i].mem = NULL;
   }

   return VCOS_SUCCESS;
}

void *vcos_generic_blockpool_alloc(VCOS_BLOCKPOOL_T *pool)
{
   VCOS_UNSIGNED i;
   void* ret = NULL;
   VCOS_BLOCKPOOL_SUBPOOL_T *subpool = NULL;

   ASSERT_POOL(pool);
   vcos_mutex_lock(&pool->mutex);

   /* Starting with the main pool try and find a free block */
   for (i = 0; i < pool->num_subpools; ++i)
   {
      if (pool->subpools[i].start && pool->subpools[i].available_blocks > 0)
      {
         subpool = &pool->subpools[i];
         break; /* Found a subpool with free blocks */
      }
   }

   if (! subpool)
   {
      /* All current subpools are full, try to allocate a new one */
      for (i = 1; i < pool->num_subpools; ++i)
      {
         if (! pool->subpools[i].start)
         {
            VCOS_BLOCKPOOL_SUBPOOL_T *s = &pool->subpools[i];
            size_t size = VCOS_BLOCKPOOL_SIZE(pool->num_extension_blocks,
                  pool->block_data_size, pool->align);
            void *mem = vcos_malloc(size, pool->name);
            if (mem)
            {
               vcos_log_trace("%s: Allocated subpool %d", VCOS_FUNCTION, i);
               vcos_generic_blockpool_subpool_init(pool, s, mem, size,
                     pool->num_extension_blocks,
                     pool->align,
                     VCOS_BLOCKPOOL_SUBPOOL_FLAG_OWNS_MEM |
                     VCOS_BLOCKPOOL_SUBPOOL_FLAG_EXTENSION);
               subpool = s;
               break; /* Created a subpool */
            }
            else
            {
               vcos_log_warn("%s: Failed to allocate subpool", VCOS_FUNCTION);
            }
         }
      }
   }

   if (subpool)
   {
      /* Remove from free list */
      VCOS_BLOCKPOOL_HEADER_T* nb = subpool->free_list;

      vcos_assert(subpool->free_list);
      subpool->free_list = nb->owner.next;

      /* Owner is pool so free can be called without passing pool
       * as a parameter */
      nb->owner.subpool = subpool;

      ret = nb + 1; /* Return pointer to block data */
      --(subpool->available_blocks);
   }
   vcos_mutex_unlock(&pool->mutex);
   VCOS_BLOCKPOOL_DEBUG_LOG("pool %p subpool %p ret %p", pool, subpool, ret);

   if (ret)
   {
      vcos_assert(ret > subpool->start);
      vcos_assert(ret < subpool->end);
   }
   return ret;
}

void *vcos_generic_blockpool_calloc(VCOS_BLOCKPOOL_T *pool)
{
   void* ret = vcos_generic_blockpool_alloc(pool);
   if (ret)
      memset(ret, 0, pool->block_data_size);
   return ret;
}

void vcos_generic_blockpool_free(void *block)
{
   VCOS_BLOCKPOOL_DEBUG_LOG("block %p", block);
   if (block)
   {
      VCOS_BLOCKPOOL_HEADER_T* hdr = (VCOS_BLOCKPOOL_HEADER_T*) block - 1;
      VCOS_BLOCKPOOL_SUBPOOL_T *subpool = hdr->owner.subpool;
      VCOS_BLOCKPOOL_T *pool = NULL;

      ASSERT_SUBPOOL(subpool);
      pool = subpool->owner;
      ASSERT_POOL(pool);

      vcos_mutex_lock(&pool->mutex);
      vcos_assert((unsigned) subpool->available_blocks < subpool->num_blocks);

      /* Change ownership of block to be the free list */
      hdr->owner.next = subpool->free_list;
      subpool->free_list = hdr;
      ++(subpool->available_blocks);

      if (VCOS_BLOCKPOOL_OVERWRITE_ON_FREE)
         memset(block, 0xBD, pool->block_data_size); /* For debugging */

      if ( (subpool->flags & VCOS_BLOCKPOOL_SUBPOOL_FLAG_EXTENSION) &&
            subpool->available_blocks == subpool->num_blocks)
      {
         VCOS_BLOCKPOOL_DEBUG_LOG("%s: freeing subpool %p mem %p", VCOS_FUNCTION,
               subpool, subpool->mem);
         /* Free the sub-pool if it was dynamically allocated */
         vcos_free(subpool->mem);
         subpool->mem = NULL;
         subpool->start = NULL;
      }
      vcos_mutex_unlock(&pool->mutex);
   }
}

VCOS_UNSIGNED vcos_generic_blockpool_available_count(VCOS_BLOCKPOOL_T *pool)
{
   VCOS_UNSIGNED ret = 0;
   VCOS_UNSIGNED i;

   ASSERT_POOL(pool);
   vcos_mutex_lock(&pool->mutex);
   for (i = 0; i < pool->num_subpools; ++i)
   {
      VCOS_BLOCKPOOL_SUBPOOL_T *subpool = &pool->subpools[i];
      ASSERT_SUBPOOL(subpool);

      /* Assume the malloc of sub pool would succeed */
      if (subpool->start)
         ret += subpool->available_blocks;
      else
         ret += pool->num_extension_blocks;
   }
   vcos_mutex_unlock(&pool->mutex);
   return ret;
}

VCOS_UNSIGNED vcos_generic_blockpool_used_count(VCOS_BLOCKPOOL_T *pool)
{
   VCOS_UNSIGNED ret = 0;
   VCOS_UNSIGNED i;

   ASSERT_POOL(pool);
   vcos_mutex_lock(&pool->mutex);

   for (i = 0; i < pool->num_subpools; ++i)
   {
      VCOS_BLOCKPOOL_SUBPOOL_T *subpool = &pool->subpools[i];
      ASSERT_SUBPOOL(subpool);
      if (subpool->start)
         ret += (subpool->num_blocks - subpool->available_blocks);
   }
   vcos_mutex_unlock(&pool->mutex);
   return ret;
}

void vcos_generic_blockpool_delete(VCOS_BLOCKPOOL_T *pool)
{
   vcos_log_trace("%s: pool %p", VCOS_FUNCTION, pool);

   if (pool)
   {
      VCOS_UNSIGNED i;

      ASSERT_POOL(pool);
      for (i = 0; i < pool->num_subpools; ++i)
      {
         VCOS_BLOCKPOOL_SUBPOOL_T *subpool = &pool->subpools[i];
         ASSERT_SUBPOOL(subpool);
         if (subpool->mem)
         {
            /* For debugging */
            memset(subpool->mem,
                  0xBE,
                  VCOS_BLOCKPOOL_SIZE(subpool->num_blocks,
                     pool->block_data_size, pool->align));

            if (subpool->flags & VCOS_BLOCKPOOL_SUBPOOL_FLAG_OWNS_MEM)
               vcos_free(subpool->mem);
            subpool->mem = NULL;
            subpool->start = NULL;
         }
      }
      vcos_mutex_delete(&pool->mutex);
      memset(pool, 0xBE, sizeof(VCOS_BLOCKPOOL_T)); /* For debugging */
   }
}

uint32_t vcos_generic_blockpool_elem_to_handle(void *block)
{
   uint32_t ret = -1;
   uint32_t index = -1;
   VCOS_BLOCKPOOL_HEADER_T *hdr = NULL;
   VCOS_BLOCKPOOL_T *pool = NULL;
   VCOS_BLOCKPOOL_SUBPOOL_T *subpool = NULL;
   uint32_t subpool_id;

   vcos_assert(block);
   hdr = (VCOS_BLOCKPOOL_HEADER_T*) block - 1;
   subpool = hdr->owner.subpool;
   ASSERT_SUBPOOL(subpool);

   pool = subpool->owner;
   ASSERT_POOL(pool);
   vcos_mutex_lock(&pool->mutex);

   /* The handle is the index into the array of blocks combined
    * with the subpool id.
    */
   index = ((size_t) hdr - (size_t) subpool->start) / pool->block_size;
   vcos_assert(index < subpool->num_blocks);

   subpool_id = ((char*) subpool - (char*) &pool->subpools[0]) /
      sizeof(VCOS_BLOCKPOOL_SUBPOOL_T);

   vcos_assert(subpool_id < VCOS_BLOCKPOOL_MAX_SUBPOOLS);
   vcos_assert(subpool_id < pool->num_subpools);
   ret = VCOS_BLOCKPOOL_HANDLE_CREATE(index, subpool_id);

   vcos_log_trace("%s: index %d subpool_id %d handle 0x%08x",
         VCOS_FUNCTION, index, subpool_id, ret);

   vcos_mutex_unlock(&pool->mutex);
   return ret;
}

void *vcos_generic_blockpool_elem_from_handle(
      VCOS_BLOCKPOOL_T *pool, uint32_t handle)
{
   VCOS_BLOCKPOOL_SUBPOOL_T *subpool;
   uint32_t subpool_id;
   uint32_t index;
   void *ret = NULL;


   ASSERT_POOL(pool);
   vcos_mutex_lock(&pool->mutex);
   subpool_id = VCOS_BLOCKPOOL_HANDLE_GET_SUBPOOL(handle);

   if (subpool_id < pool->num_subpools)
   {
      index = VCOS_BLOCKPOOL_HANDLE_GET_INDEX(handle);
      subpool = &pool->subpools[subpool_id];
      if (pool->subpools[subpool_id].magic == VCOS_BLOCKPOOL_SUBPOOL_MAGIC &&
            pool->subpools[subpool_id].mem && index < subpool->num_blocks)
      {
         VCOS_BLOCKPOOL_HEADER_T *hdr = (VCOS_BLOCKPOOL_HEADER_T*)
            ((size_t) subpool->start + (index * pool->block_size));

         if (hdr->owner.subpool == subpool) /* Check block is allocated */
            ret = hdr + 1;
      }
   }
   vcos_mutex_unlock(&pool->mutex);

   vcos_log_trace("%s: pool %p handle 0x%08x elem %p", VCOS_FUNCTION, pool,
         handle, ret);
   return ret;
}

uint32_t vcos_generic_blockpool_is_valid_elem(
      VCOS_BLOCKPOOL_T *pool, const void *block)
{
   uint32_t ret = 0;
   const char *pool_end;
   VCOS_UNSIGNED i = 0;

   ASSERT_POOL(pool);
   if (((size_t) block) & 0x3)
      return 0;

   vcos_mutex_lock(&pool->mutex);

   for (i = 0; i < pool->num_subpools; ++i)
   {
      VCOS_BLOCKPOOL_SUBPOOL_T *subpool = &pool->subpools[i];
      ASSERT_SUBPOOL(subpool);

      if (subpool->mem && subpool->start)
      {
         pool_end = (const char*)subpool->start +
            (subpool->num_blocks * pool->block_size);

         if ((const char*)block > (const char*)subpool->start &&
               (const char*)block < pool_end)
         {
            const VCOS_BLOCKPOOL_HEADER_T *hdr = (
                  const VCOS_BLOCKPOOL_HEADER_T*) block - 1;

            /* If the block has a header where the owner points to the pool then
             * it's a valid block. */
            ret = (hdr->owner.subpool == subpool && subpool->owner == pool);
            break;
         }
      }
   }
   vcos_mutex_unlock(&pool->mutex);
   return ret;
}
