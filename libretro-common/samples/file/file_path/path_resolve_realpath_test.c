/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (path_resolve_realpath_test.c).
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

/* Regression for commit 87f2d0b (leading-slash stack smash in
 * path_resolve_realpath()).
 *
 * The POSIX non-symlink branch used to copy a run of leading '/'
 * characters from the input into a 4 KiB stack buffer with no bound.
 * An input of PATH_MAX_LENGTH or more leading slashes therefore walked
 * off the end of the buffer.  This input can be supplied via a
 * malicious playlist (playlist.c:1192), a core-updater metadata
 * response, or the runloop (runloop.c:586).
 *
 * Under -fstack-protector-strong the unpatched code aborts with
 *   *** stack smashing detected ***
 * The patched code detects the overflow before the write and returns
 * NULL cleanly.
 *
 * Build with RARCH_INTERNAL and -fstack-protector-strong:
 *   make CFLAGS='-DRARCH_INTERNAL -fstack-protector-strong -g -O0' \
 *        LDFLAGS='-fstack-protector-strong'
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <retro_miscellaneous.h>  /* PATH_MAX_LENGTH */

static int failures = 0;

/* With resolve_symlinks=false the function walks the input without
 * any filesystem call, so we can feed it synthetic adversarial
 * paths without needing real files on disk. */
static void test_leading_slash_bounded(void)
{
   /* Buffer must be larger than PATH_MAX_LENGTH so s_len itself fits,
    * otherwise strlcpy truncates before the function sees the payload. */
   size_t bufsz = PATH_MAX_LENGTH * 2;
   char *buf    = (char*)malloc(bufsz);
   char *r;
   size_t i;

   if (!buf)
      abort();

   /* Fill with many more leading '/' than PATH_MAX_LENGTH, followed
    * by 'foo'.  The old code would write PATH_MAX_LENGTH * 2 bytes
    * into a PATH_MAX_LENGTH stack buffer. */
   for (i = 0; i < PATH_MAX_LENGTH + 1000; i++)
      buf[i] = '/';
   strcpy(buf + PATH_MAX_LENGTH + 1000, "foo");

   r = path_resolve_realpath(buf, bufsz, false);
   /* Patched code returns NULL for input with too many leading slashes.
    * Unpatched code stack-smashes long before reaching this line. */
   if (r != NULL)
   {
      printf("[FAILED] expected NULL return for pathological input, got %p\n", (void*)r);
      failures++;
   }
   else
      printf("[SUCCESS] pathological leading slashes rejected cleanly\n");

   free(buf);
}

/* A single leading slash on an otherwise-well-formed path must still
 * resolve (this is the normal case -- every absolute path). */
static void test_leading_slash_normal(void)
{
   char buf[PATH_MAX_LENGTH];
   char *r;

   strcpy(buf, "/usr/bin/foo");
   r = path_resolve_realpath(buf, sizeof(buf), false);
   if (!r || strcmp(buf, "/usr/bin/foo") != 0)
   {
      printf("[FAILED] normal absolute path not preserved: \"%s\"\n", buf);
      failures++;
      return;
   }
   printf("[SUCCESS] normal absolute path preserved\n");
}

/* A few leading slashes (triple-slash is valid per POSIX) must still
 * work.  PATH_MAX_LENGTH - N where N is small should be OK. */
static void test_few_leading_slashes(void)
{
   char buf[PATH_MAX_LENGTH];
   char *r;

   strcpy(buf, "///usr/bin/foo");
   r = path_resolve_realpath(buf, sizeof(buf), false);
   /* POSIX folds leading "///" to "/"; libretro keeps the prefix.
    * Either way, the function must not crash. */
   if (!r)
   {
      printf("[FAILED] triple-slash path rejected: \"///usr/bin/foo\"\n");
      failures++;
      return;
   }
   printf("[SUCCESS] triple-slash path accepted: \"%s\"\n", buf);
}

int main(void)
{
   test_leading_slash_normal();
   test_few_leading_slashes();
   test_leading_slash_bounded();

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll path_resolve_realpath regression tests passed.\n");
   return 0;
}
