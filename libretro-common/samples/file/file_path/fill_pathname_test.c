/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fill_pathname_test.c).
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

/* Regression for the fill_pathname() out-of-bounds write observed in
 * production iOS crash reports as
 *   __chk_fail_overflow -> __strlcpy_chk -> fill_pathname
 *      -> database_info_list_iterate_found_match (task_database.c)
 *
 * fill_pathname() is:
 *
 *     size_t _len = strlcpy(s, in_path, len);
 *     if ((tok = strrchr(path_basename(s), '.'))) { *tok = '\0'; _len = tok - s; }
 *     _len += strlcpy(s + _len, replace, len - _len);
 *
 * strlcpy() returns strlen(in_path), not the number of bytes written.
 * If strlen(in_path) >= len AND the truncated copy contains no '.' in
 * its basename, the conditional does not fire and _len stays at the
 * (large) source length.  The second strlcpy then runs as
 *   strlcpy(s + _len, replace, len - _len)
 * with len - _len underflowed to a huge size_t.  s + _len is well past
 * the end of the destination buffer, and strlcpy happily writes
 * strlen(replace) + 1 bytes there.
 *
 * On Apple platforms strlcpy is a libc symbol covered by FORTIFY, so
 * __strlcpy_chk catches the bogus length and aborts (this is the
 * SIGTRAP visible in the crash logs).  On Linux there is no FORTIFY
 * for strlcpy, so we detect the overrun by surrounding the destination
 * with sentinel bytes and checking they survive the call.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>

static int failures = 0;

#define DST_LEN     64
#define GUARD_LEN   256
#define SENTINEL    0xCC

/* Long input with no '.' anywhere -- forces fill_pathname() to take
 * the "no extension to strip" branch with _len > len. */
static void test_overlong_input_no_dot(void)
{
   char *region;
   char *dst;
   size_t i;
   size_t in_len = DST_LEN * 4;   /* 256 chars, well past DST_LEN */
   char *in_path;

   region = (char*)malloc(DST_LEN + GUARD_LEN);
   if (!region)
      abort();
   memset(region, SENTINEL, DST_LEN + GUARD_LEN);
   dst = region;

   in_path = (char*)malloc(in_len + 1);
   if (!in_path)
      abort();
   for (i = 0; i < in_len; i++)
      in_path[i] = 'a';
   in_path[in_len] = '\0';

   fill_pathname(dst, in_path, ".lpl", DST_LEN);

   /* The destination's first DST_LEN bytes are fair game for the
    * function to write into.  Anything in [DST_LEN, DST_LEN+GUARD_LEN)
    * must be untouched. */
   for (i = DST_LEN; i < (size_t)(DST_LEN + GUARD_LEN); i++)
   {
      if ((unsigned char)region[i] != SENTINEL)
      {
         printf("[FAILED] fill_pathname overran dst: byte %zu changed from 0x%02x to 0x%02x\n",
               i, SENTINEL, (unsigned char)region[i]);
         failures++;
         break;
      }
   }
   if (i == (size_t)(DST_LEN + GUARD_LEN))
      printf("[SUCCESS] overlong no-dot input did not overrun destination\n");

   free(in_path);
   free(region);
}

/* Long input where the truncated copy DOES contain a '.' -- the
 * conditional resets _len so this path was never broken, but include
 * it as a regression so a future "fix" doesn't silently break the
 * normal extension-replacement case. */
static void test_overlong_input_with_dot(void)
{
   char dst[DST_LEN];
   /* "aaaa.aaaa..." for in_len bytes -- truncation will still leave
    * dots inside the destination. */
   size_t in_len = DST_LEN * 4;
   char *in_path = (char*)malloc(in_len + 1);
   size_t i;

   if (!in_path)
      abort();
   for (i = 0; i < in_len; i++)
      in_path[i] = (i % 5 == 4) ? '.' : 'a';
   in_path[in_len] = '\0';

   memset(dst, 0, sizeof(dst));
   fill_pathname(dst, in_path, ".lpl", sizeof(dst));

   /* Result must be NUL-terminated within the buffer. */
   if (memchr(dst, '\0', sizeof(dst)) == NULL)
   {
      printf("[FAILED] fill_pathname did not NUL-terminate within buffer\n");
      failures++;
   }
   else
      printf("[SUCCESS] overlong with-dot input stayed NUL-terminated in buffer\n");

   free(in_path);
}

/* Helper: run fill_pathname with a 64-byte buffer and assert the
 * resulting string matches @want. */
static void check_spec(const char *label, const char *in_path,
      const char *replace, const char *want)
{
   char dst[64];
   memset(dst, 0, sizeof(dst));
   fill_pathname(dst, in_path, replace, sizeof(dst));
   if (strcmp(dst, want) != 0)
   {
      printf("[FAILED] %s: fill_pathname(\"%s\", \"%s\") = \"%s\" (expected \"%s\")\n",
            label, in_path, replace, dst, want);
      failures++;
      return;
   }
   printf("[SUCCESS] %s\n", label);
}

/* Edge case: input is exactly len-1, no extension, replace appended.
 * The first strlcpy returns strlen(in_path) == len-1, no '.' in the
 * truncated copy, _len = len-1, so len - _len = 1.  The second
 * strlcpy gets len=1 and writes only the NUL.  Must not overrun. */
static void test_exact_fit_no_extension(void)
{
   char *region = (char*)malloc(DST_LEN + GUARD_LEN);
   char *dst;
   char in_path[DST_LEN];
   size_t i;

   if (!region)
      abort();
   memset(region, SENTINEL, DST_LEN + GUARD_LEN);
   dst = region;

   for (i = 0; i < DST_LEN - 1; i++)
      in_path[i] = 'b';
   in_path[DST_LEN - 1] = '\0';

   fill_pathname(dst, in_path, ".lpl", DST_LEN);

   for (i = DST_LEN; i < (size_t)(DST_LEN + GUARD_LEN); i++)
   {
      if ((unsigned char)region[i] != SENTINEL)
      {
         printf("[FAILED] fill_pathname overran dst on exact-fit input: byte %zu changed\n", i);
         failures++;
         break;
      }
   }
   if (i == (size_t)(DST_LEN + GUARD_LEN))
      printf("[SUCCESS] exact-fit no-extension input did not overrun destination\n");

   free(region);
}

int main(void)
{
   /* Documented semantics. */
   check_spec("normal extension replacement preserved",
         "/foo/bar/baz/boo.c", ".asm", "/foo/bar/baz/boo.asm");
   check_spec("no extension -> concatenate replace",
         "foo", ".lpl", "foo.lpl");
   check_spec("empty replace strips extension",
         "foo.c", "", "foo");
   check_spec("dot in dirname does not count as extension",
         "/foo.bar/baz", ".x", "/foo.bar/baz.x");
   check_spec("only the last dot in the basename is stripped",
         "foo.bar.baz", ".x", "foo.bar.x");

   /* Out-of-bounds behavior. */
   test_overlong_input_with_dot();
   test_exact_fit_no_extension();
   test_overlong_input_no_dot();

   if (failures)
   {
      printf("\n%d fill_pathname test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll fill_pathname regression tests passed.\n");
   return 0;
}
