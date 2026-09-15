/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_name_safety_test.c).
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

/* Regression for commit 87f2d0b (Zip Slip in tasks/task_decompress).
 *
 * The predicate archive_name_is_safe() is static inside
 * tasks/task_decompress.c and isn't in a public header, so this test
 * keeps its own verbatim copy of the predicate as the oracle.  The
 * test's value is in locking in the predicate's contract -- any
 * future edit to the real predicate that changes semantics will
 * drift from this oracle and the test's maintainer must decide
 * whether the drift is intended.
 *
 * If you update archive_name_is_safe() in task_decompress.c, update
 * the copy in this file to match.
 *
 * Build standalone:
 *   cc -Wall -std=gnu99 -g -O0 -o archive_name_safety_test \
 *      archive_name_safety_test.c
 *   ./archive_name_safety_test
 */

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

/* =============== verbatim from tasks/task_decompress.c =============== */
static bool archive_name_is_safe(const char *name)
{
   const char *p;
   const char *seg_start;

   if (!name || !*name)
      return false;

   /* Reject absolute paths in either Unix or Windows form. */
   if (name[0] == '/' || name[0] == '\\')
      return false;
   /* Reject drive-letter prefix e.g. "C:..." */
   if (name[1] == ':')
      return false;

   /* Reject any ".." path segment.  Treat both '/' and '\\' as
    * separators so Windows-authored archives can't traverse when
    * extracted on a POSIX host. */
   p = seg_start = name;
   for (;;)
   {
      char c = *p;
      if (c == '/' || c == '\\' || c == '\0')
      {
         if ((size_t)(p - seg_start) == 2
               && seg_start[0] == '.' && seg_start[1] == '.')
            return false;
         if (c == '\0')
            break;
         seg_start = p + 1;
      }
      p++;
   }
   return true;
}
/* =============== end verbatim copy =============== */

static int failures = 0;

static void expect(const char *name, bool want)
{
   bool got = archive_name_is_safe(name);
   if (got != want)
   {
      printf("[FAILED] %-32s  expected %s, got %s\n",
            name ? name : "(null)",
            want ? "safe" : "UNSAFE",
            got  ? "safe" : "UNSAFE");
      failures++;
      return;
   }
   printf("[SUCCESS] %-32s  %s\n",
         name ? name : "(null)",
         want ? "safe" : "correctly rejected");
}

int main(void)
{
   /* --- legitimate archive contents -- must be accepted --- */
   expect("file.txt",            true);
   expect("dir/file.txt",        true);
   expect("dir/subdir/file.txt", true);
   expect("a.b/c",               true);
   expect("...",                 true);   /* three dots, not a segment of '..' */
   expect("..foo",               true);   /* prefix only */
   expect("foo..",               true);   /* suffix only */
   expect("x/y.ext",             true);

   /* --- zip-slip payloads -- must be rejected --- */
   expect("..",                   false);
   expect("../etc/passwd",        false);
   expect("../../etc/passwd",     false);
   expect("foo/../../etc/passwd", false);
   expect("foo/..",               false);
   expect("foo/..//bar",          false);  /* double slash after traversal */
   expect("foo/../bar",           false);

   /* --- absolute paths -- must be rejected --- */
   expect("/etc/passwd",          false);
   expect("\\windows\\system32",  false);

   /* --- drive-letter prefixes -- must be rejected --- */
   expect("C:\\evil",             false);
   expect("c:/evil",              false);

   /* --- Windows-style traversal -- must be rejected --- */
   expect("..\\..\\windows",      false);
   expect("foo\\..\\bar",         false);

   /* --- empty / NULL -- must be rejected --- */
   expect("",                     false);
   expect(NULL,                   false);

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll archive_name_is_safe regression tests passed.\n");
   return 0;
}
