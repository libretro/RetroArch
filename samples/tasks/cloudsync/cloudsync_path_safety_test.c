/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cloudsync_path_safety_test.c).
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

/* Regression test for the manifest-key check in
 * tasks/task_cloudsync.c::task_cloud_sync_fetch_server_file.
 *
 * A malicious sync server can return a manifest key whose path portion
 * contains ".." or an absolute component, letting the fetch write outside
 * the cloud-sync base directory via fill_pathname_join_special. A key with
 * no '/' made the old strchr(key, '/') + 1 read from address 1.
 *
 * This test #includes the real predicate source (tasks/task_cloudsync_path.c),
 * which has no dependencies for that reason, so it checks the shipped code
 * rather than a copy.
 *
 * Build standalone:
 *   cc -Wall -pedantic -std=gnu99 -g -O0 -o cloudsync_path_safety_test \
 *      cloudsync_path_safety_test.c
 *   ./cloudsync_path_safety_test
 */

#include <stdio.h>
#include <stddef.h>

/* The real shipped predicate -- not a copy. Dependency-free, so it compiles
 * here with no libretro-common include path. */
#include "../../../tasks/task_cloudsync_path.c"

static int failures = 0;

/* want_safe: the key is expected to yield a non-NULL relative path.
 * !want_safe: the key must be rejected (NULL return). */
static void expect(const char *key, int want_safe)
{
   const char *got = cloud_sync_manifest_key_path(key);
   int         ok  = want_safe ? (got != NULL) : (got == NULL);

   if (!ok)
   {
      printf("[FAILED] %-40s  expected %s, got %s\n",
            key ? key : "(null)",
            want_safe ? "safe"       : "UNSAFE",
            got        ? "safe"       : "UNSAFE");
      failures++;
      return;
   }
   printf("[SUCCESS] %-40s  %s\n",
         key ? key : "(null)",
         want_safe ? "safe" : "correctly rejected");
}

int main(void)
{
   /* --- legitimate manifest keys -- must be accepted ---
    * The first path segment is the portable directory prefix that
    * task_cloud_sync_directory_map() maps to a local base dir; the
    * predicate returns everything after that first '/'. */
   expect("saves/game.srm",             1);
   expect("states/game.state",          1);
   expect("config/retroarch.cfg",       1);
   expect("saves/subdir/game.srm",      1);
   expect("saves/a.b/c",                1);
   expect("saves/foo.bar",              1);

   /* --- traversal payloads -- must be rejected ---
    * NOTE: the shipped guard uses strstr(path, "..") -- a SUBSTRING match,
    * stricter than a path-segment match. Any ".." anywhere in the path
    * portion is rejected, including non-traversal filenames that merely
    * contain "..". This documents that shipped behaviour; the test locks
    * it in rather than asserting an idealised segment-only contract. */
   expect("saves/../../etc/passwd",     0);
   expect("saves/../secret",            0);
   expect("saves/..",                   0);
   expect("saves/foo/../../etc/passwd", 0);
   expect("saves/x/..",                 0);
   expect("saves/..bar/../baz",         0);
   expect("saves/...",                  0); /* contains ".." substring */
   expect("saves/..foo",                0); /* contains ".." substring */
   expect("saves/foo..bar",             0); /* contains ".." substring */

   /* --- absolute path portion -- must be rejected --- */
   expect("saves//etc/passwd",          0); /* leading '/' after prefix slash */

   /* --- malformed keys -- must be rejected --- */
   expect("noslash",                    0); /* no '/': would be NULL+1 UB */
   expect("saves/",                     0); /* empty path portion */
   expect("",                           0);
   expect(NULL,                         0);

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll cloud_sync_manifest_key_path regression tests passed.\n");
   return 0;
}
