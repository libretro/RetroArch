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
VideoCore OS Abstraction Layer - event flags implemented via a semaphore
=============================================================================*/

#ifndef VCOS_GENERIC_BLOCKPOOL_H
#define VCOS_GENERIC_BLOCKPOOL_H

/**
  * \file
  *
  * This provides a generic, thread safe implementation of a VCOS block pool
  * fixed size memory allocator.
  */

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"

/** Bits 0 to (VCOS_BLOCKPOOL_SUBPOOL_BITS - 1) are used to store the
 * subpool id. */
#define VCOS_BLOCKPOOL_SUBPOOL_BITS 3
#define VCOS_BLOCKPOOL_MAX_SUBPOOLS (1 << VCOS_BLOCKPOOL_SUBPOOL_BITS)

/* Make zero an invalid handle at the cost of decreasing the maximum
 * number of blocks (2^28) by 1. Alternatively, a spare bit could be
 * used to indicated valid blocks but there are likely to be better
 * uses for spare bits. e.g. allowing more subpools
 */
#define INDEX_OFFSET 1

#define VCOS_BLOCKPOOL_HANDLE_GET_INDEX(h) \
   (((h) >> VCOS_BLOCKPOOL_SUBPOOL_BITS) - INDEX_OFFSET)

#define VCOS_BLOCKPOOL_HANDLE_GET_SUBPOOL(h) \
   ((h) & ((1 << VCOS_BLOCKPOOL_SUBPOOL_BITS) - 1))

#define VCOS_BLOCKPOOL_HANDLE_CREATE(i,s) \
   ((((i) + INDEX_OFFSET) << VCOS_BLOCKPOOL_SUBPOOL_BITS) | (s))

#define VCOS_BLOCKPOOL_INVALID_HANDLE 0
#define VCOS_BLOCKPOOL_ALIGN_DEFAULT sizeof(unsigned long)
#define VCOS_BLOCKPOOL_FLAG_NONE 0

typedef struct VCOS_BLOCKPOOL_HEADER_TAG
{
   /* Blocks either refer to to the pool if they are allocated
    * or the free list if they are available.
    */
   union {
   struct VCOS_BLOCKPOOL_HEADER_TAG *next;
   struct VCOS_BLOCKPOOL_SUBPOOL_TAG* subpool;
   } owner;
} VCOS_BLOCKPOOL_HEADER_T;

typedef struct VCOS_BLOCKPOOL_SUBPOOL_TAG
{
   /** VCOS_BLOCKPOOL_SUBPOOL_MAGIC */
   uint32_t magic;
   VCOS_BLOCKPOOL_HEADER_T* free_list;
   /* The start of the pool memory */
   void *mem;
   /* Address of the first block header */
   void *start;
   /* The end of the subpool */
   void *end;
   /** The number of blocks in this sub-pool */
   VCOS_UNSIGNED num_blocks;
   /** Current number of available blocks in this sub-pool */
   VCOS_UNSIGNED available_blocks;
   /** Pointers to the pool that owns this sub-pool */
   struct VCOS_BLOCKPOOL_TAG* owner;
   /** Define properties such as memory ownership */
   uint32_t flags;
} VCOS_BLOCKPOOL_SUBPOOL_T;

typedef struct VCOS_BLOCKPOOL_TAG
{
   /** VCOS_BLOCKPOOL_MAGIC */
   uint32_t magic;
   /** Thread safety for Alloc, Free, Delete, Stats */
   VCOS_MUTEX_T mutex;
   /** Alignment of block data pointers */
   VCOS_UNSIGNED align;
   /** Flags for future use e.g. cache options */
   VCOS_UNSIGNED flags;
   /** The size of the block data */
   size_t block_data_size;
   /** Block size inc overheads */
   size_t block_size;
   /** Name for debugging */
   const char *name;
   /* The number of subpools that may be used */
   VCOS_UNSIGNED num_subpools;
   /** Number of blocks in each dynamically allocated subpool */
   VCOS_UNSIGNED num_extension_blocks;
   /** Array of subpools. Subpool zero is is not deleted until the pool is
    * destroed. If the index of the pool is < num_subpools and
    * subpool[index.mem] is null then the subpool entry is valid but
    * "not currently allocated" */
   VCOS_BLOCKPOOL_SUBPOOL_T subpools[VCOS_BLOCKPOOL_MAX_SUBPOOLS];
} VCOS_BLOCKPOOL_T;

#define VCOS_BLOCKPOOL_ROUND_UP(x,s)   (((x) + ((s) - 1)) & ~((s) - 1))
/**
 * Calculates the size in bytes required for a block pool containing
 * num_blocks of size block_size plus any overheads.
 *
 * The block pool header (VCOS_BLOCKPOOL_T) is allocated separately
 *
 * Overheads:
 * block_size + header must be rounded up to meet the required alignment
 * The start of the first block may need to be up to align bytes
 * into the given buffer because statically allocated buffers within structures
 * are not guaranteed to be aligned as required.
 */
#define VCOS_BLOCKPOOL_SIZE(num_blocks, block_size, align) \
   ((VCOS_BLOCKPOOL_ROUND_UP((block_size) + (align >= 4096 ? 32 : 0) + sizeof(VCOS_BLOCKPOOL_HEADER_T), \
                             (align)) * (num_blocks)) + (align))

/**
 * Sanity check to verify whether a handle is potentially a blockpool handle
 * when the pool pointer is not available.
 *
 * If the pool pointer is available use vcos_blockpool_elem_to_handle instead.
 *
 * @param handle       the handle to verify
 * @param max_blocks   the expected maximum number of block in the pool
 *                     that the handle belongs to.
 */
#define VCOS_BLOCKPOOL_IS_VALID_HANDLE_FORMAT(handle, max_blocks) \
    ((handle) != VCOS_BLOCKPOOL_INVALID_HANDLE \
     && VCOS_BLOCKPOOL_HANDLE_GET_INDEX((handle)) < (max_blocks))

VCOSPRE_
   VCOS_STATUS_T VCOSPOST_ vcos_generic_blockpool_init(VCOS_BLOCKPOOL_T *pool,
      VCOS_UNSIGNED num_blocks, VCOS_UNSIGNED block_size,
      void *start, VCOS_UNSIGNED pool_size,
      VCOS_UNSIGNED align, VCOS_UNSIGNED flags,
      const char *name);

VCOSPRE_
   VCOS_STATUS_T VCOSPOST_ vcos_generic_blockpool_create_on_heap(
         VCOS_BLOCKPOOL_T *pool, VCOS_UNSIGNED num_blocks,
         VCOS_UNSIGNED block_size,
         VCOS_UNSIGNED align, VCOS_UNSIGNED flags,
         const char *name);

VCOSPRE_
   VCOS_STATUS_T VCOSPOST_ vcos_generic_blockpool_extend(VCOS_BLOCKPOOL_T *pool,
         VCOS_UNSIGNED num_extensions, VCOS_UNSIGNED num_blocks);

VCOSPRE_ void VCOSPOST_ *vcos_generic_blockpool_alloc(VCOS_BLOCKPOOL_T *pool);

VCOSPRE_ void VCOSPOST_ *vcos_generic_blockpool_calloc(VCOS_BLOCKPOOL_T *pool);

VCOSPRE_ void VCOSPOST_ vcos_generic_blockpool_free(void *block);

VCOSPRE_
   VCOS_UNSIGNED VCOSPOST_ vcos_generic_blockpool_available_count(
         VCOS_BLOCKPOOL_T *pool);

VCOSPRE_
   VCOS_UNSIGNED VCOSPOST_ vcos_generic_blockpool_used_count(
         VCOS_BLOCKPOOL_T *pool);

VCOSPRE_ void VCOSPOST_ vcos_generic_blockpool_delete(VCOS_BLOCKPOOL_T *pool);

VCOSPRE_ uint32_t VCOSPOST_ vcos_generic_blockpool_elem_to_handle(void *block);

VCOSPRE_ void VCOSPOST_
   *vcos_generic_blockpool_elem_from_handle(
         VCOS_BLOCKPOOL_T *pool, uint32_t handle);

VCOSPRE_ uint32_t VCOSPOST_
   vcos_generic_blockpool_is_valid_elem(
         VCOS_BLOCKPOOL_T *pool, const void *block);
#if defined(VCOS_INLINE_BODIES)

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_blockpool_init(VCOS_BLOCKPOOL_T *pool,
      VCOS_UNSIGNED num_blocks, VCOS_UNSIGNED block_size,
      void *start, VCOS_UNSIGNED pool_size,
      VCOS_UNSIGNED align, VCOS_UNSIGNED flags, const char *name)
{
   return vcos_generic_blockpool_init(pool, num_blocks, block_size,
         start, pool_size, align, flags, name);
}

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_blockpool_create_on_heap(VCOS_BLOCKPOOL_T *pool,
      VCOS_UNSIGNED num_blocks, VCOS_UNSIGNED block_size,
      VCOS_UNSIGNED align, VCOS_UNSIGNED flags, const char *name)
{
   return vcos_generic_blockpool_create_on_heap(
         pool, num_blocks, block_size, align, flags, name);
}

VCOS_INLINE_IMPL
   VCOS_STATUS_T VCOSPOST_ vcos_blockpool_extend(VCOS_BLOCKPOOL_T *pool,
         VCOS_UNSIGNED num_extensions, VCOS_UNSIGNED num_blocks)
{
    return vcos_generic_blockpool_extend(pool, num_extensions, num_blocks);
}

VCOS_INLINE_IMPL
void *vcos_blockpool_alloc(VCOS_BLOCKPOOL_T *pool)
{
   return vcos_generic_blockpool_alloc(pool);
}

VCOS_INLINE_IMPL
void *vcos_blockpool_calloc(VCOS_BLOCKPOOL_T *pool)
{
   return vcos_generic_blockpool_calloc(pool);
}

VCOS_INLINE_IMPL
void vcos_blockpool_free(void *block)
{
   vcos_generic_blockpool_free(block);
}

VCOS_INLINE_IMPL
VCOS_UNSIGNED vcos_blockpool_available_count(VCOS_BLOCKPOOL_T *pool)
{
   return vcos_generic_blockpool_available_count(pool);
}

VCOS_INLINE_IMPL
VCOS_UNSIGNED vcos_blockpool_used_count(VCOS_BLOCKPOOL_T *pool)
{
   return vcos_generic_blockpool_used_count(pool);
}

VCOS_INLINE_IMPL
void vcos_blockpool_delete(VCOS_BLOCKPOOL_T *pool)
{
   vcos_generic_blockpool_delete(pool);
}

VCOS_INLINE_IMPL
uint32_t vcos_blockpool_elem_to_handle(void *block)
{
   return vcos_generic_blockpool_elem_to_handle(block);
}

VCOS_INLINE_IMPL
void *vcos_blockpool_elem_from_handle(VCOS_BLOCKPOOL_T *pool, uint32_t handle)
{
   return vcos_generic_blockpool_elem_from_handle(pool, handle);
}

VCOS_INLINE_IMPL
uint32_t vcos_blockpool_is_valid_elem(VCOS_BLOCKPOOL_T *pool, const void *block)
{
   return vcos_generic_blockpool_is_valid_elem(pool, block);
}
#endif /* VCOS_INLINE_BODIES */

#ifdef __cplusplus
}
#endif
#endif /* VCOS_GENERIC_BLOCKPOOL_H */
