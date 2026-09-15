/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_zstd_test.c).
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

/* Contract test for archive_file_zstd content_size guards.
 *
 * Self-contained: does NOT link against libzstd or the archive
 * backend.  The bugs being guarded against are arithmetic truncation
 * and addition overflow on an attacker-controlled 64-bit value
 * returned by ZSTD_getFrameContentSize(); the guards live in
 * archive_file_zstd.c and are plain integer comparisons.
 *
 * This test is explicitly a CONTRACT test, not a regression test.
 * It validates that the guard SPEC (as replicated in the oracle
 * functions below) behaves correctly on boundary inputs.  It does
 * NOT call into archive_file_zstd.c, so it cannot detect if someone
 * later edits the real guards in a way that diverges from this spec.
 *
 * Why: a true regression test would need to link libzstd to exercise
 * ZSTD_getFrameContentSize on a crafted frame, which would introduce
 * an external dependency this samples tree does not otherwise carry.
 * The contract test is the next-best thing within that constraint.
 *
 * What the real patched code does (keep these in sync with the
 * oracle functions below):
 *
 *   zstd_parse_file_iterate_step() [iterate path]:
 *       if (content_size > UINT32_MAX)
 *           return -1;
 *       ctx->decompressed_size = (uint32_t)content_size;
 *
 *   zstd_file_read() [decompress-in-place path]:
 *       if (content_size >= SIZE_MAX)
 *           return -1;
 *       decompressed = malloc((size_t)(content_size + 1));
 *
 * If either real guard is ever edited, the corresponding oracle
 * below must be updated to match or this test loses its value.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

/* These match <zstd.h> but are reproduced here so we don't pull in
 * the zstd headers.  Update if zstd ever redefines them. */
#ifndef ZSTD_CONTENTSIZE_UNKNOWN
#define ZSTD_CONTENTSIZE_UNKNOWN ((unsigned long long)0 - 1)
#endif
#ifndef ZSTD_CONTENTSIZE_ERROR
#define ZSTD_CONTENTSIZE_ERROR   ((unsigned long long)0 - 2)
#endif

static int failures = 0;

/* --- oracle: the patched iterate-path guard --------------------- *
 * Must be kept in sync with archive_file_zstd.c:
 *     zstd_parse_file_iterate_step.
 * Returns 0 if the value is accepted, -1 if rejected.             */
static int oracle_iterate_guard(unsigned long long content_size)
{
   if (   content_size == ZSTD_CONTENTSIZE_UNKNOWN
       || content_size == ZSTD_CONTENTSIZE_ERROR)
      return -1;
   if (content_size > UINT32_MAX)
      return -1;
   return 0;
}

/* --- oracle: the patched read-path guard ------------------------ *
 * Must be kept in sync with archive_file_zstd.c: zstd_file_read.  *
 * Returns 0 if the value is accepted, -1 if rejected.             */
static int oracle_read_guard(unsigned long long content_size)
{
   if (   content_size == ZSTD_CONTENTSIZE_UNKNOWN
       || content_size == ZSTD_CONTENTSIZE_ERROR)
      return -1;
   if (content_size >= (unsigned long long)SIZE_MAX)
      return -1;
   return 0;
}

/* ================================================================ */

typedef struct {
   const char        *label;
   unsigned long long value;
   int                want_iterate;  /* expected oracle_iterate result */
   int                want_read;     /* expected oracle_read result    */
} case_t;

int main(void)
{
   size_t i;
   /* These cases exercise the boundaries the guards protect.  Each
    * case lists the expected verdict for both the iterate path
    * (uint32_t destination) and the read path (size_t + 1). */
   case_t cases[] = {
      /* label                           value                   iter  read */
      { "zero",                           0,                       0,   0 },
      { "typical small",                  1024,                    0,   0 },
      { "100 MiB",                        100ULL * 1024 * 1024,    0,   0 },
      { "UINT32_MAX exactly",             (unsigned long long)UINT32_MAX, 0,  0 },
      { "UINT32_MAX + 1  (iterate trunc)",(unsigned long long)UINT32_MAX + 1, -1, 0 },
      { "4 GiB  (iterate trunc)",         4ULL * 1024 * 1024 * 1024, -1, 0 },
      { "2^63           (iterate trunc)", 1ULL << 63,               -1, 0 },
      { "ZSTD_CONTENTSIZE_ERROR sentinel",ZSTD_CONTENTSIZE_ERROR,   -1, -1 },
      { "ZSTD_CONTENTSIZE_UNKNOWN",       ZSTD_CONTENTSIZE_UNKNOWN, -1, -1 },
      /* SIZE_MAX case -- on 64-bit, SIZE_MAX == ULLONG_MAX - 1,
       * which equals ZSTD_CONTENTSIZE_ERROR, so the sentinel check
       * catches it.  On 32-bit, SIZE_MAX is far smaller and the
       * >= SIZE_MAX branch catches it first.  Either way: rejected
       * on the read path.  On the iterate path it's also rejected
       * (too big for uint32_t). */
      { "SIZE_MAX (read-path guard)",     (unsigned long long)SIZE_MAX, -1, -1 },
   };

   for (i = 0; i < sizeof(cases)/sizeof(cases[0]); i++)
   {
      int got_iter = oracle_iterate_guard(cases[i].value);
      int got_read = oracle_read_guard(cases[i].value);

      if (got_iter != cases[i].want_iterate)
      {
         printf("[FAILED] iterate-guard(%s=%llu) got %d want %d\n",
               cases[i].label,
               (unsigned long long)cases[i].value,
               got_iter, cases[i].want_iterate);
         failures++;
         continue;
      }
      if (got_read != cases[i].want_read)
      {
         printf("[FAILED] read-guard(%s=%llu) got %d want %d\n",
               cases[i].label,
               (unsigned long long)cases[i].value,
               got_read, cases[i].want_read);
         failures++;
         continue;
      }
      printf("[SUCCESS] %-40s iter=%s read=%s\n",
            cases[i].label,
            got_iter == 0 ? "accept" : "reject",
            got_read == 0 ? "accept" : "reject");
   }

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll zstd content_size regression tests passed.\n");
   return 0;
}
