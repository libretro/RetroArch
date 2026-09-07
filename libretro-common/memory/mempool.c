/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mempool.c).
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

#include <stdlib.h>

#include <memory/mempool.h>

/* A free block stores the free-list link in its own storage, so a
 * block can never be smaller than a pointer.  The union carries a
 * second member of the widest alignment the pool has to satisfy: a
 * block is only ever used for one object type, and every such type in
 * tree is a struct of pointers, integers and floats. */
union mempool_node
{
   union mempool_node *next;
   void  *align_ptr;
   double align_double;
};

struct mempool_chunk
{
   struct mempool_chunk *next;
   /* Blocks follow inline.  The union pins the start of that run to
    * the same alignment mempool_alloc() promises for a block, which a
    * bare 'next' pointer would not do on a 32-bit target. */
   union mempool_node    blocks;
};

/* Offset of the first block within a chunk. */
#define MEMPOOL_CHUNK_HDR offsetof(struct mempool_chunk, blocks)

/* Chunks double until they hold this many blocks.  At 64 KiB blocks a
 * chunk of the largest menu structure in tree is a few MiB, which is
 * the point past which growing further buys nothing: the calls into
 * libc are already amortised to nothing and a bigger chunk only makes
 * the tail waste worse for a list that stops just over a boundary. */
#define MEMPOOL_MAX_BLOCKS ((size_t)1 << 16)

#define MEMPOOL_DEFAULT_BLOCKS 64

void mempool_init(mempool_t *pool, size_t block_size, size_t blocks)
{
   if (block_size < sizeof(union mempool_node))
      block_size      = sizeof(union mempool_node);

   /* Round the stride up to pointer alignment so that every block
    * after the first is aligned as well as the chunk is. */
   block_size         = (block_size + sizeof(union mempool_node) - 1)
                      & ~(sizeof(union mempool_node) - 1);

   pool->chunks       = NULL;
   pool->free_list    = NULL;
   pool->block_size   = block_size;
   pool->blocks       = blocks ? blocks : MEMPOOL_DEFAULT_BLOCKS;
   pool->live         = 0;
}

static int mempool_grow(mempool_t *pool)
{
   size_t i;
   char *base;
   struct mempool_chunk *chunk;

   /* Refuse a chunk whose size would wrap rather than allocating a
    * short one and handing out blocks past its end. */
   if (pool->blocks > ((size_t)-1 - MEMPOOL_CHUNK_HDR) / pool->block_size)
      return 0;

   if (!(chunk = (struct mempool_chunk*)malloc(
               MEMPOOL_CHUNK_HDR + pool->block_size * pool->blocks)))
      return 0;

   chunk->next        = pool->chunks;
   pool->chunks       = chunk;

   /* Thread the new blocks on back to front, so the first hand-out is
    * the lowest address in the chunk and a run of allocations walks
    * forward through it.  A list build then touches the chunk in
    * address order, which is what the hardware prefetcher wants. */
   base               = (char*)chunk + MEMPOOL_CHUNK_HDR;
   for (i = pool->blocks; i > 0; i--)
   {
      union mempool_node *node = (union mempool_node*)
         (base + (i - 1) * pool->block_size);
      node->next      = pool->free_list;
      pool->free_list = node;
   }

   if (pool->blocks < MEMPOOL_MAX_BLOCKS)
      pool->blocks  <<= 1;

   return 1;
}

void *mempool_alloc(mempool_t *pool)
{
   union mempool_node *node;

   if (!pool->free_list)
      if (!mempool_grow(pool))
         return NULL;

   node               = pool->free_list;
   pool->free_list    = node->next;
   pool->live++;

   return (void*)node;
}

void mempool_free(mempool_t *pool, void *block)
{
   union mempool_node *node;

   if (!block)
      return;

   node               = (union mempool_node*)block;
   node->next         = pool->free_list;
   pool->free_list    = node;
   pool->live--;
}

void mempool_deinit(mempool_t *pool)
{
   struct mempool_chunk *chunk = pool->chunks;

   while (chunk)
   {
      struct mempool_chunk *next = chunk->next;
      free(chunk);
      chunk           = next;
   }

   pool->chunks       = NULL;
   pool->free_list    = NULL;
   pool->live         = 0;
   /* Leave block_size alone: the pool is reusable without a second
    * mempool_init(), which is what the menu wants across a driver
    * teardown and reinit.  Reset the growth so the next use starts
    * from a small chunk again. */
   pool->blocks       = MEMPOOL_DEFAULT_BLOCKS;
}
