/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (strlcpy_append_test.c).
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

/* Regression test for strlcpy_append in
 * libretro-common/string/stdstring.c.
 *
 * strlcpy_append exists because the unsafe idiom
 *     _len += strlcpy(s + _len, src, len - _len);
 * silently corrupts @s on truncation: strlcpy returns
 * strlen(@src), so once @src is too long for the remaining
 * space, _len overshoots @len and the next subtraction
 * (len - _len) underflows size_t.
 *
 * Contract: on success the function advances *pos by
 * strlen(@src) and returns 0.  On truncation it leaves @s
 * NUL-terminated and clamps *pos to len - 1, so subsequent
 * calls in a chain short-circuit (also returning -1).  This
 * lets callers chain three or more appends and check just
 * the final return value.
 *
 * Heap-allocated destinations sized to exactly the legal
 * capacity so AddressSanitizer flags any reintroduction of
 * the unbounded chain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>

static int failures = 0;

#define EXPECT_EQ_INT(got, want, msg) do { \
   long _g = (long)(got), _w = (long)(want); \
   if (_g != _w) { \
      printf("[ERROR] %s:%d  %s  got=%ld want=%ld\n", \
            __func__, __LINE__, (msg), _g, _w); \
      failures++; \
   } \
} while (0)

#define EXPECT_EQ_STR(got, want, msg) do { \
   if (strcmp((got), (want)) != 0) { \
      printf("[ERROR] %s:%d  %s  got=\"%s\" want=\"%s\"\n", \
            __func__, __LINE__, (msg), (got), (want)); \
      failures++; \
   } \
} while (0)

#define EXPECT_TRUE(cond, msg) do { \
   if (!(cond)) { \
      printf("[ERROR] %s:%d  %s\n", __func__, __LINE__, (msg)); \
      failures++; \
   } \
} while (0)

/* ---- tests ------------------------------------------------ */

static void test_basic_append(void)
{
   char  *s   = (char*)malloc(16);
   size_t pos = 0;
   int    rv, saved = failures;
   if (!s) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(s, 0, 16);

   rv = strlcpy_append(s, 16, &pos, "hello");
   EXPECT_EQ_INT(rv, 0, "rv");
   EXPECT_EQ_INT(pos, 5, "pos");
   EXPECT_EQ_STR(s, "hello", "buffer");

   free(s);
   if (failures == saved) printf("[SUCCESS] basic append\n");
}

static void test_chained_append(void)
{
   char  *s   = (char*)malloc(32);
   size_t pos = 0;
   int    rv1, rv2, rv3, saved = failures;
   if (!s) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(s, 0, 32);

   rv1 = strlcpy_append(s, 32, &pos, "hello, ");
   rv2 = strlcpy_append(s, 32, &pos, "world");
   rv3 = strlcpy_append(s, 32, &pos, "!");
   EXPECT_EQ_INT(rv1, 0, "rv1");
   EXPECT_EQ_INT(rv2, 0, "rv2");
   EXPECT_EQ_INT(rv3, 0, "rv3");
   EXPECT_EQ_INT(pos, 13, "pos");
   EXPECT_EQ_STR(s, "hello, world!", "buffer");

   free(s);
   if (failures == saved) printf("[SUCCESS] chained append\n");
}

static void test_truncation_at_boundary(void)
{
   /* Buffer sized exactly for "hello" + NUL = 6.  First append
    * fits exactly.  Second append truncates: rv = -1, pos
    * clamps to len - 1 = 5, buffer remains "hello". */
   char  *s   = (char*)malloc(6);
   size_t pos = 0;
   int    rv, saved = failures;
   if (!s) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(s, 0, 6);

   rv = strlcpy_append(s, 6, &pos, "hello");
   EXPECT_EQ_INT(rv, 0, "first rv");
   EXPECT_EQ_INT(pos, 5, "first pos");
   EXPECT_EQ_STR(s, "hello", "buffer after first");

   rv = strlcpy_append(s, 6, &pos, "x");
   EXPECT_EQ_INT(rv, -1, "second rv (truncation)");
   EXPECT_EQ_INT(pos, 5, "pos clamped to len - 1");
   EXPECT_EQ_STR(s, "hello", "buffer NUL-terminated at len - 1");

   free(s);
   if (failures == saved) printf("[SUCCESS] truncation at boundary\n");
}

static void test_truncation_wide(void)
{
   /* Source much larger than destination.  Pre-fix the naive
    * idiom would have memcpy'd off the end of s; with
    * strlcpy_append the call clamps without writing past the
    * buffer.  Heap-allocated to exactly 4 bytes so ASan flags
    * any OOB. */
   char  *s   = (char*)malloc(4);
   size_t pos = 0;
   int    rv, saved = failures;
   if (!s) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(s, 0, 4);

   rv = strlcpy_append(s, 4, &pos,
         "this string is much longer than the destination");
   EXPECT_EQ_INT(rv, -1, "rv");
   EXPECT_EQ_INT(pos, 3, "pos clamped to len - 1");
   /* strlcpy fills 3 chars + NUL.  Buffer is "thi". */
   EXPECT_EQ_STR(s, "thi", "buffer truncated and NUL-terminated");

   free(s);
   if (failures == saved) printf("[SUCCESS] truncation by wide margin\n");
}

static void test_chain_short_circuits_after_truncation(void)
{
   /* Once a previous call truncates and clamps *pos to len - 1,
    * every subsequent call short-circuits with -1 without
    * writing past the buffer.  This is the property that makes
    * "check only the last return value" safe. */
   char  *s   = (char*)malloc(8);
   size_t pos = 0;
   int    rv1, rv2, rv3, saved = failures;
   if (!s) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(s, 0, 8);

   rv1 = strlcpy_append(s, 8, &pos, "ab");
   rv2 = strlcpy_append(s, 8, &pos,
         "this exceeds the remaining space");
   rv3 = strlcpy_append(s, 8, &pos, "cd");

   EXPECT_EQ_INT(rv1, 0,  "rv1 fits");
   EXPECT_EQ_INT(rv2, -1, "rv2 truncates");
   EXPECT_EQ_INT(rv3, -1, "rv3 short-circuits");
   EXPECT_EQ_INT(pos, 7,  "pos clamped at len - 1");
   EXPECT_TRUE(strlen(s) <= 7, "buffer fits");

   free(s);
   if (failures == saved) printf("[SUCCESS] chain short-circuits after truncation\n");
}

static void test_empty_source(void)
{
   char  *s   = (char*)malloc(8);
   size_t pos = 0;
   int    rv, saved = failures;
   if (!s) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(s, 0, 8);

   rv = strlcpy_append(s, 8, &pos, "");
   EXPECT_EQ_INT(rv, 0, "rv");
   EXPECT_EQ_INT(pos, 0, "pos unchanged");
   EXPECT_EQ_STR(s, "", "buffer empty");

   rv = strlcpy_append(s, 8, &pos, "ok");
   EXPECT_EQ_INT(rv, 0, "second rv");
   EXPECT_EQ_INT(pos, 2, "second pos");
   EXPECT_EQ_STR(s, "ok", "second buffer");

   free(s);
   if (failures == saved) printf("[SUCCESS] empty source\n");
}

static void test_null_args(void)
{
   char   buf[16];
   size_t pos = 0;
   int    saved = failures;

   EXPECT_EQ_INT(strlcpy_append(NULL, 16, &pos, "x"), -1, "NULL s");
   EXPECT_EQ_INT(strlcpy_append(buf,  16, NULL, "x"), -1, "NULL pos");
   EXPECT_EQ_INT(strlcpy_append(buf,  16, &pos, NULL), -1, "NULL src");
   EXPECT_EQ_INT(strlcpy_append(buf,   0, &pos, "x"),  -1, "zero len");

   if (failures == saved) printf("[SUCCESS] NULL/zero args rejected\n");
}

static void test_pos_at_or_past_len(void)
{
   char  *s   = (char*)malloc(8);
   size_t pos = 8;
   int    rv, saved = failures;
   if (!s) { printf("[ERROR] alloc\n"); failures++; return; }
   memset(s, 0, 8);

   rv = strlcpy_append(s, 8, &pos, "x");
   EXPECT_EQ_INT(rv, -1, "pos == len rejects");
   EXPECT_EQ_INT(pos, 7, "pos clamped to len - 1");

   pos = 99;
   rv  = strlcpy_append(s, 8, &pos, "x");
   EXPECT_EQ_INT(rv, -1, "pos > len rejects");
   EXPECT_EQ_INT(pos, 7, "pos clamped to len - 1");

   free(s);
   if (failures == saved) printf("[SUCCESS] pos at/past len rejected and clamped\n");
}

int main(void)
{
   test_basic_append();
   test_chained_append();
   test_truncation_at_boundary();
   test_truncation_wide();
   test_chain_short_circuits_after_truncation();
   test_empty_source();
   test_null_args();
   test_pos_at_or_past_len();

   if (failures)
   {
      printf("\n%d strlcpy_append test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll strlcpy_append regression tests passed.\n");
   return 0;
}
