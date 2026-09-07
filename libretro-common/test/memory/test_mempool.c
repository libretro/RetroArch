/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_mempool.c).
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

/* Tests for mempool_t, the fixed-size block allocator behind the
 * menu's per-entry structures.
 *
 * The properties that matter to its callers are: a block stays put and
 * stays distinct for as long as it is held, a released block comes back
 * without a further call into libc, and the whole thing unwinds without
 * leaking whatever the growth path allocated.  Under ASan the last one
 * is checked by the sanitizer rather than by an assertion here.
 *
 * The write-through-every-block cases exist because a stride that is
 * too small or a chunk header of the wrong size produces overlapping
 * blocks that still pass a pointer-distinctness check -- the overlap
 * only shows up once something writes a whole block and reads it back.
 */

#include <check.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory/mempool.h>

#define SUITE_NAME "mempool"

/* Larger than the first chunk (16 blocks below), so every test that
 * uses it crosses at least two chunk boundaries. */
#define MANY 500

struct payload
{
   void  *ptr;
   double d;
   char   tail[13];
};

START_TEST (test_mempool_init_allocates_nothing)
{
   mempool_t pool;
   mempool_init(&pool, sizeof(struct payload), 16);
   ck_assert_ptr_null(pool.chunks);
   ck_assert_ptr_null(pool.free_list);
   ck_assert_uint_eq(pool.live, 0);
   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_blocks_are_distinct_and_writable)
{
   mempool_t pool;
   void *blocks[MANY];
   size_t i, j;

   mempool_init(&pool, sizeof(struct payload), 16);

   for (i = 0; i < MANY; i++)
   {
      struct payload *p = (struct payload*)mempool_alloc(&pool);
      ck_assert_ptr_nonnull(p);
      blocks[i] = p;
      /* Stamp the whole block, including the tail, so that an
       * overlapping stride corrupts an earlier block. */
      p->ptr    = (void*)(uintptr_t)i;
      p->d      = (double)i;
      memset(p->tail, (int)(i & 0x7f), sizeof(p->tail));
   }

   ck_assert_uint_eq(pool.live, MANY);

   /* Nothing aliases anything else. */
   for (i = 0; i < MANY; i++)
      for (j = i + 1; j < MANY; j++)
         ck_assert_ptr_ne(blocks[i], blocks[j]);

   /* Every stamp survived every later allocation. */
   for (i = 0; i < MANY; i++)
   {
      struct payload *p = (struct payload*)blocks[i];
      char expect[sizeof(p->tail)];
      memset(expect, (int)(i & 0x7f), sizeof(expect));
      ck_assert_ptr_eq(p->ptr, (void*)(uintptr_t)i);
      ck_assert_double_eq(p->d, (double)i);
      ck_assert_mem_eq(p->tail, expect, sizeof(expect));
   }

   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_blocks_are_aligned)
{
   mempool_t pool;
   size_t i;
   /* A block size that is not a multiple of the alignment: the pool
    * has to round the stride up rather than pack blocks tightly. */
   mempool_init(&pool, 13, 8);

   for (i = 0; i < MANY; i++)
   {
      void *p = mempool_alloc(&pool);
      ck_assert_ptr_nonnull(p);
      ck_assert_uint_eq((uintptr_t)p % sizeof(double), 0);
   }

   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_free_makes_block_reusable)
{
   mempool_t pool;
   void *first, *again;
   mempool_init(&pool, sizeof(struct payload), 16);

   first = mempool_alloc(&pool);
   ck_assert_uint_eq(pool.live, 1);
   mempool_free(&pool, first);
   ck_assert_uint_eq(pool.live, 0);

   /* The free list is LIFO, so the block just returned is the next
    * one out.  This is the property the menu relies on to keep a
    * rebuild of the same list off libc entirely. */
   again = mempool_alloc(&pool);
   ck_assert_ptr_eq(again, first);
   ck_assert_uint_eq(pool.live, 1);

   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_rebuild_takes_no_new_chunk)
{
   mempool_t pool;
   void *blocks[MANY];
   size_t i;
   struct mempool_chunk *after_first_build;

   mempool_init(&pool, sizeof(struct payload), 16);

   for (i = 0; i < MANY; i++)
      blocks[i] = mempool_alloc(&pool);
   after_first_build = pool.chunks;

   for (i = 0; i < MANY; i++)
      mempool_free(&pool, blocks[i]);
   ck_assert_uint_eq(pool.live, 0);

   /* Second build of the same size: every block comes off the free
    * list and the chunk list is untouched. */
   for (i = 0; i < MANY; i++)
      ck_assert_ptr_nonnull(mempool_alloc(&pool));
   ck_assert_ptr_eq(pool.chunks, after_first_build);

   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_free_null_is_ignored)
{
   mempool_t pool;
   mempool_init(&pool, sizeof(struct payload), 16);
   mempool_free(&pool, NULL);
   ck_assert_uint_eq(pool.live, 0);
   ck_assert_ptr_null(pool.free_list);
   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_deinit_leaves_pool_reusable)
{
   mempool_t pool;
   size_t i;
   mempool_init(&pool, sizeof(struct payload), 16);

   for (i = 0; i < MANY; i++)
      ck_assert_ptr_nonnull(mempool_alloc(&pool));

   /* Deinit with blocks still outstanding: the menu tears a driver
    * down this way rather than releasing entry by entry. */
   mempool_deinit(&pool);
   ck_assert_ptr_null(pool.chunks);
   ck_assert_ptr_null(pool.free_list);
   ck_assert_uint_eq(pool.live, 0);

   /* No second mempool_init(): the block size survives. */
   for (i = 0; i < MANY; i++)
      ck_assert_ptr_nonnull(mempool_alloc(&pool));

   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_tiny_block_rounds_up_to_pointer)
{
   mempool_t pool;
   void *a, *b;
   /* One byte cannot hold the free-list link the pool stores in an
    * unused block. */
   mempool_init(&pool, 1, 4);
   ck_assert_uint_ge(pool.block_size, sizeof(void*));

   a = mempool_alloc(&pool);
   b = mempool_alloc(&pool);
   ck_assert_ptr_nonnull(a);
   ck_assert_ptr_nonnull(b);
   ck_assert_ptr_ne(a, b);

   mempool_free(&pool, a);
   mempool_free(&pool, b);
   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_default_chunk_size)
{
   mempool_t pool;
   size_t i;
   /* 0 asks for the default rather than a chunk of no blocks, which
    * would make mempool_grow() loop without ever producing one. */
   mempool_init(&pool, sizeof(struct payload), 0);
   ck_assert_uint_gt(pool.blocks, 0);
   for (i = 0; i < MANY; i++)
      ck_assert_ptr_nonnull(mempool_alloc(&pool));
   mempool_deinit(&pool);
}
END_TEST

START_TEST (test_mempool_interleaved_alloc_free)
{
   mempool_t pool;
   void *held[64];
   size_t i;
   mempool_init(&pool, sizeof(struct payload), 8);
   memset(held, 0, sizeof(held));

   /* Churn: every third allocation releases an older block, so the
    * free list and the growth path are both exercised and neither
    * ends up handing out a block twice. */
   for (i = 0; i < 4096; i++)
   {
      size_t slot = i % 64;
      if (held[slot])
         mempool_free(&pool, held[slot]);
      held[slot] = mempool_alloc(&pool);
      ck_assert_ptr_nonnull(held[slot]);
      memset(held[slot], (int)(i & 0xff), sizeof(struct payload));
   }

   for (i = 0; i < 64; i++)
      mempool_free(&pool, held[i]);
   ck_assert_uint_eq(pool.live, 0);

   mempool_deinit(&pool);
}
END_TEST

static Suite *mempool_suite(void)
{
   Suite *s        = suite_create(SUITE_NAME);
   TCase *tc_alloc = tcase_create("alloc");

   tcase_add_test(tc_alloc, test_mempool_init_allocates_nothing);
   tcase_add_test(tc_alloc, test_mempool_blocks_are_distinct_and_writable);
   tcase_add_test(tc_alloc, test_mempool_blocks_are_aligned);
   tcase_add_test(tc_alloc, test_mempool_free_makes_block_reusable);
   tcase_add_test(tc_alloc, test_mempool_rebuild_takes_no_new_chunk);
   tcase_add_test(tc_alloc, test_mempool_free_null_is_ignored);
   tcase_add_test(tc_alloc, test_mempool_deinit_leaves_pool_reusable);
   tcase_add_test(tc_alloc, test_mempool_tiny_block_rounds_up_to_pointer);
   tcase_add_test(tc_alloc, test_mempool_default_chunk_size);
   tcase_add_test(tc_alloc, test_mempool_interleaved_alloc_free);

   suite_add_tcase(s, tc_alloc);

   return s;
}

int main(void)
{
   int num_fail;
   Suite *s     = mempool_suite();
   SRunner *sr  = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail     = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
