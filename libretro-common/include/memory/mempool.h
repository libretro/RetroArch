/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mempool.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_MEMPOOL_H
#define __LIBRETRO_SDK_MEMPOOL_H

/**
 * @file mempool.h
 *
 * Allocator for one fixed block size.
 *
 * Serves blocks out of chunks it takes from malloc() in bulk, so a
 * caller that allocates and releases N same-sized objects pays a
 * logarithmic number of calls into libc rather than N of them.  A
 * released block goes on an intrusive free list and is handed back out
 * on the next request.
 *
 * A block keeps its address for as long as it is held, so anything
 * that relocates the object holding the pointer -- qsort() in
 * file_list_sort_on_alt(), memmove() in file_list_insert() -- moves
 * the pointer and never the block.
 *
 * There is no locking.  A pool belongs to one thread, which for the
 * users in tree is the thread that drives the menu.
 */

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct mempool_chunk;
union  mempool_node;

typedef struct mempool
{
   struct mempool_chunk *chunks;
   union  mempool_node  *free_list;
   size_t block_size;
   /* Blocks in the next chunk to be taken; doubles per chunk. */
   size_t blocks;
   /* Blocks currently handed out, for leak checks in tests. */
   size_t live;
} mempool_t;

/**
 * Prepare @pool to serve blocks of @block_size bytes.
 *
 * @block_size is rounded up to pointer size and alignment, so a block
 * is suitable for any object of that size or smaller.  @blocks is the
 * number of blocks in the first chunk; pass 0 for a default.  No
 * allocation happens here -- a pool that is never used costs nothing
 * beyond the struct.
 */
void mempool_init(mempool_t *pool, size_t block_size, size_t blocks);

/**
 * Take one block.  Contents are undefined; the caller initialises it,
 * exactly as with malloc(). Returns NULL only when a new chunk was
 * needed and could not be allocated.
 */
void *mempool_alloc(mempool_t *pool);

/**
 * Give one block back.  @block must have come from @pool.  NULL is
 * accepted and ignored, matching free().
 *
 * The block is reusable immediately but the chunk holding it stays
 * with the pool: memory returns to libc only at mempool_deinit().
 */
void mempool_free(mempool_t *pool, void *block);

/**
 * Release every chunk.  All outstanding blocks become invalid, so the
 * caller has to have finished with them.  The pool is left as
 * mempool_init() leaves it and can be used again without
 * re-initialising.
 */
void mempool_deinit(mempool_t *pool);

RETRO_END_DECLS

#endif
