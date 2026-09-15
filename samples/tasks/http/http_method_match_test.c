/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (http_method_match_test.c).
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

/* Regression test for the heap-buffer-overflow fix in
 * tasks/task_http.c::task_push_http_transfer_generic().
 *
 * Original bug: the dispatch decision was
 *
 *     if (memcmp(method, "GET", 3) == 0 && method[3] == '\0')
 *
 * `method` is a strdup'd buffer whose allocation is exactly
 * strlen(method) + 1 bytes.  When `method` is NULL or shorter than
 * 3 bytes (e.g. "" or "X") the memcmp reads past the buffer.  The
 * trailing method[3] guard runs *after* the read, so it cannot
 * prevent the overflow.
 *
 * Fix: dispatch via string_is_equal(method, "GET") with an explicit
 * NULL guard upstream.
 *
 * The match predicate is local to task_push_http_transfer_generic()
 * and isn't in a public header, so this test keeps a verbatim copy
 * of the relevant fragment as the oracle, exactly as
 * archive_name_safety_test.c does for archive_name_is_safe().  If
 * task_http.c amends the predicate, the copy here must follow.
 *
 * The test is most useful when built under AddressSanitizer:
 *
 *   make clean all SANITIZER=address
 *   ./http_method_match_test
 *
 * Without ASan the post-fix expression is also bounds-safe and the
 * test simply asserts the truth-table.  With ASan, *both* the
 * truth-table and the bounds-safety are checked at once: building
 * the pre-fix expression instead would trip ASan on the short/NULL
 * inputs below.
 *
 * Build standalone:
 *   cc -Wall -std=gnu99 -g -O0 -o http_method_match_test \
 *      http_method_match_test.c
 *   ./http_method_match_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Drop-in equivalent of libretro-common/string/stdstring.h's
 * string_is_equal().  Kept local so the test compiles standalone
 * without pulling libretro-common into the build, matching the
 * archive_name_safety_test.c convention. */
static inline bool string_is_equal(const char *a, const char *b)
{
   if (a == b)
      return true;
   if (!a || !b)
      return false;
   return strcmp(a, b) == 0;
}

/* =============== verbatim from tasks/task_http.c =============== *
 * The fragment under test is the dispatch decision made on `method`
 * inside task_push_http_transfer_generic(), post-fix:
 *
 *     if (!method)
 *        goto error;
 *     if (string_is_equal(method, "GET"))
 *        ... GET fast-path ...
 *
 * Wrapped here as a single bool predicate that mirrors the post-fix
 * control flow: NULL is rejected (returns false, like the goto error
 * branch which never reaches the GET fast-path), otherwise the
 * GET-equality test runs against the buffer's NUL terminator.
 */
static bool http_is_plain_get(const char *method)
{
   if (!method)
      return false;
   return string_is_equal(method, "GET");
}
/* =============== end verbatim copy =============== */

static int failures = 0;

/* expect() takes a strdup'd input so that the buffer's allocation
 * size is exactly strlen+1 bytes, which is what
 * net_http_connection_new() does and what makes the pre-fix code
 * trip ASan.  A literal would sit in .rodata where the OOB read may
 * not be caught. */
static void expect(const char *literal_or_null, bool want)
{
   char *buf = literal_or_null ? strdup(literal_or_null) : NULL;
   bool  got = http_is_plain_get(buf);
   const char *display = literal_or_null
      ? (literal_or_null[0] ? literal_or_null : "(empty)")
      : "(null)";

   if (got != want)
   {
      printf("[FAILED]  %-12s  expected %s, got %s\n",
            display,
            want ? "GET"     : "non-GET",
            got  ? "GET"     : "non-GET");
      failures++;
   }
   else
   {
      printf("[SUCCESS] %-12s  %s\n",
            display,
            want ? "matched GET fast-path"
                 : "correctly not GET");
   }

   free(buf);
}

int main(void)
{
   /* --- the only true case --- */
   expect("GET",      true);

   /* --- non-GET methods that the function legitimately receives --- */
   expect("POST",     false);
   expect("PUT",      false);
   expect("DELETE",   false);
   expect("OPTIONS",  false);
   expect("MKCOL",    false);
   expect("MOVE",     false);

   /* --- the bounds-safety regression cases ---
    * Pre-fix, every one of these tripped a heap-buffer-overflow
    * read of size 3 in memcmp().  Post-fix, string_is_equal walks
    * to the buffer's NUL terminator and is safe on short/empty
    * input; the NULL case is rejected by the upstream guard. */
   expect("",         false);  /* 1-byte alloc, pre-fix read 3 */
   expect("X",        false);  /* 2-byte alloc, pre-fix read 3 */
   expect("GE",       false);  /* 3-byte alloc; GET prefix of "GET" but not equal */
   expect(NULL,       false);  /* pre-fix dereferenced NULL */

   /* --- near-misses that must NOT collapse to GET --- */
   expect("GETx",     false);  /* GET prefix + extra chars */
   expect("get",      false);  /* case sensitivity */
   expect("GE\0T",    false);  /* embedded NUL: only "GE" is visible to strcmp */

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll http_is_plain_get regression tests passed.\n");
   return 0;
}
