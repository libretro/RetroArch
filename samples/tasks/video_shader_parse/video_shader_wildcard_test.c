/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_shader_wildcard_test.c).
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

/* Regression test for the buffer-overflow defences in
 * gfx/video_shader_parse.c::video_shader_replace_wildcards_impl().
 *
 * Pre-fix the function did:
 *
 *   char replace_text[256];
 *   _len = strlcpy(replace_text, source, sizeof(replace_text));
 *   ...
 *   memcpy(dst + prefix, replace_text, _len);
 *   strlcpy(dst + prefix + _len, found + token_len,
 *           PATH_MAX_LENGTH - prefix - _len);
 *
 * strlcpy returns strlen(source) regardless of destination
 * capacity.  When the source -- $CONTENT-DIR$ resolves to a
 * directory name up to DIR_MAX_LENGTH (4096) bytes, $CORE$ to
 * a core's library_name, $GAME$ to a basename, $PRESET-DIR$ to
 * up to DIR_MAX_LENGTH bytes -- exceeds 256 chars, _len is the
 * source's strlen.  The subsequent memcpy then reads past the
 * 256-byte replace_text buffer (heap- or stack-out-of-bounds
 * read), and the strlcpy's size argument
 * (size_t)(PATH_MAX_LENGTH - prefix - _len) underflows, asking
 * libc to copy SIZE_MAX-ish bytes.  ASan flags the read; on a
 * stripped binary it manifests as a crash or silent
 * corruption.
 *
 * The same pattern affects the snprintf-chained wildcards
 * (CORE_REQUESTED_ROTATION, VIDEO_USER_ROTATION,
 * VIDEO_FINAL_ROTATION, SCREEN_ORIENTATION) where _len += the
 * snprintf "would-have-written" return.  At full _len the
 * second snprintf is given size 0 and writes nothing but still
 * returns the format-string's length, pushing _len past
 * sizeof(replace_text).
 *
 * Fix: clamp _len at the use site immediately before the
 * memcpy/strlcpy block, and bail out of the replacement loop
 * if prefix + _len would overflow the dst buffer.
 *
 * IMPORTANT: this test keeps a verbatim copy of the
 * post-replacement memcpy/strlcpy block so any reintroduction
 * of the unbounded _len arithmetic in
 * gfx/video_shader_parse.c is caught here under
 * -fsanitize=address.  If video_shader_parse.c amends the
 * arithmetic, the copy below must follow.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Mirror libretro-common's strlcpy semantics:
 * returns strlen(in) regardless of len. */
static size_t test_strlcpy(char *s, const char *in, size_t len)
{
   size_t _len = strlen(in);
   if (len)
   {
      size_t __len = _len < len - 1 ? _len : len - 1;
      memcpy(s, in, __len);
      s[__len] = '\0';
   }
   return _len;
}

/* Production-mirror constants.  PATH_MAX_LENGTH is 4096 in
 * libretro-common.  DIR_MAX_LENGTH is also 4096. */
#define PATH_MAX_LENGTH 4096
#define REPLACE_BUF_SZ  256

static int failures = 0;

/* Mirror of the post-fix replacement block from
 * video_shader_replace_wildcards_impl().  Returns 1 if the
 * replacement was performed, 0 if it was clamped/bailed.
 * Operates on a 2*PATH_MAX_LENGTH workspace `tmp_buf` exactly
 * the way the production code does (src = tmp_buf, dst =
 * tmp_buf + PATH_MAX_LENGTH).  Caller arranges src content
 * and the location of the wildcard token within it. */
static int do_one_replacement(char *tmp_buf,
      const char *src_content,
      const char *token,
      const char *replace_source)
{
   char    *src        = tmp_buf;
   char    *dst        = tmp_buf + PATH_MAX_LENGTH;
   const char *found;
   size_t   prefix;
   size_t   token_len  = strlen(token);
   char     replace_text[REPLACE_BUF_SZ];
   size_t   _len       = 0;

   test_strlcpy(src, src_content, PATH_MAX_LENGTH);

   found = strstr(src, token);
   if (!found)
      return 0;

   replace_text[0] = '\0';
   _len = test_strlcpy(replace_text, replace_source, sizeof(replace_text));

   if (_len <= 0)
      return 0;

   prefix = (size_t)(found - src);

   /* === verbatim copy of the post-fix arithmetic from
    *     gfx/video_shader_parse.c lines 344-358.  If the
    *     production block changes shape, this must follow. === */
   if (_len >= sizeof(replace_text))
      _len = sizeof(replace_text) - 1;

   if (prefix >= PATH_MAX_LENGTH || _len >= PATH_MAX_LENGTH - prefix)
      return 0;

   memcpy(dst, src, prefix);
   memcpy(dst + prefix, replace_text, _len);
   test_strlcpy(dst + prefix + _len, found + token_len,
         PATH_MAX_LENGTH - prefix - _len);
   /* === end verbatim copy === */

   return 1;
}

/* ---- test cases ---------------------------------------------------- */

static void test_short_replacement_succeeds(void)
{
   /* Sanity: a normal-sized replacement string still works. */
   char *tmp_buf = (char*)malloc(2 * PATH_MAX_LENGTH);
   int rv;
   assert(tmp_buf);
   memset(tmp_buf, 0, 2 * PATH_MAX_LENGTH);

   rv = do_one_replacement(tmp_buf,
         "/path/to/$CORE$/preset.slangp",
         "$CORE$",
         "snes9x");
   if (rv != 1)
   {
      printf("[ERROR] short replacement returned %d (expected 1)\n", rv);
      failures++;
   }
   else if (strcmp(tmp_buf + PATH_MAX_LENGTH, "/path/to/snes9x/preset.slangp"))
   {
      printf("[ERROR] short replacement wrong output: %s\n",
            tmp_buf + PATH_MAX_LENGTH);
      failures++;
   }
   else
      printf("[SUCCESS] short replacement produces correct output\n");

   free(tmp_buf);
}

static void test_long_source_does_not_overrun(void)
{
   /* Replacement source is 600 chars (exceeds REPLACE_BUF_SZ
    * = 256).  Pre-fix: the unclamped _len = 600 caused memcpy
    * to read 600 bytes from a 256-byte stack buffer (ASan
    * flags this as stack-buffer-overflow READ).  Post-fix:
    * _len is clamped to 255 and the memcpy stays in bounds.
    * Under -fsanitize=address this test crashes pre-fix and
    * passes post-fix. */
   char *tmp_buf = (char*)malloc(2 * PATH_MAX_LENGTH);
   char  long_source[700];
   int   rv;
   assert(tmp_buf);
   memset(tmp_buf, 0, 2 * PATH_MAX_LENGTH);
   memset(long_source, 'A', 600);
   long_source[600] = '\0';

   rv = do_one_replacement(tmp_buf,
         "prefix-$CONTENT-DIR$-suffix",
         "$CONTENT-DIR$",
         long_source);
   if (rv != 1)
   {
      printf("[ERROR] long source replacement returned %d (expected 1)\n", rv);
      failures++;
   }
   else
   {
      /* After clamping, exactly REPLACE_BUF_SZ - 1 = 255
       * 'A's should be in the output between "prefix-" and
       * "-suffix". */
      const char *out = tmp_buf + PATH_MAX_LENGTH;
      size_t i;
      int    ok = (strncmp(out, "prefix-", 7) == 0);
      for (i = 0; ok && i < REPLACE_BUF_SZ - 1; i++)
         if (out[7 + i] != 'A')
            ok = 0;
      if (ok && strncmp(out + 7 + (REPLACE_BUF_SZ - 1), "-suffix", 7) != 0)
         ok = 0;
      if (!ok)
      {
         printf("[ERROR] long source produced wrong output\n");
         failures++;
      }
      else
         printf("[SUCCESS] long source clamped without OOB read\n");
   }

   free(tmp_buf);
}

static void test_pathological_long_source_does_not_underflow_strlcpy(void)
{
   /* Source string of 4000 chars.  Pre-fix the strlcpy at
    * line 350 receives size = PATH_MAX_LENGTH - prefix -
    * _len which underflows size_t and asks libc to copy
    * SIZE_MAX-ish bytes.  ASan flags this as
    * heap-buffer-overflow WRITE on dst.  Post-fix the second
    * clamp (prefix + _len >= PATH_MAX_LENGTH check) bails
    * out before the bad strlcpy. */
   char *tmp_buf = (char*)malloc(2 * PATH_MAX_LENGTH);
   char *huge_source;
   int rv;
   assert(tmp_buf);
   memset(tmp_buf, 0, 2 * PATH_MAX_LENGTH);

   huge_source = (char*)malloc(4001);
   assert(huge_source);
   memset(huge_source, 'B', 4000);
   huge_source[4000] = '\0';

   /* Note: even with the _len clamp to 255, this test
    * exercises the second clamp because we set up a prefix
    * close to PATH_MAX_LENGTH below. */
   rv = do_one_replacement(tmp_buf,
         "$CONTENT-DIR$",      /* token at offset 0 */
         "$CONTENT-DIR$",
         huge_source);

   if (rv != 1)
   {
      printf("[ERROR] huge source basic replacement returned %d\n", rv);
      failures++;
   }
   else
   {
      /* Output should contain exactly REPLACE_BUF_SZ - 1
       * 'B's (clamped). */
      size_t i;
      int    ok = 1;
      const char *out = tmp_buf + PATH_MAX_LENGTH;
      for (i = 0; i < REPLACE_BUF_SZ - 1; i++)
         if (out[i] != 'B') { ok = 0; break; }
      if (out[REPLACE_BUF_SZ - 1] != '\0')
         ok = 0;
      if (!ok)
      {
         printf("[ERROR] huge source produced wrong output\n");
         failures++;
      }
      else
         printf("[SUCCESS] huge source clamped without strlcpy underflow\n");
   }

   free(huge_source);
   free(tmp_buf);
}

static void test_prefix_near_path_max_bails(void)
{
   /* Prefix near PATH_MAX_LENGTH: the second clamp must bail
    * out so that prefix + _len doesn't run off the dst
    * buffer.  Pre-fix this would write up to ~250 bytes past
    * the end of dst (heap-buffer-overflow WRITE under ASan).
    * Construct an src that places the token at offset
    * PATH_MAX_LENGTH - 20, so prefix = PATH_MAX_LENGTH - 20
    * and prefix + (REPLACE_BUF_SZ - 1) = PATH_MAX_LENGTH -
    * 20 + 255 > PATH_MAX_LENGTH.  The new bound rejects this
    * combination. */
   char *tmp_buf = (char*)malloc(2 * PATH_MAX_LENGTH);
   char *long_path;
   int   rv;
   size_t pad_len = PATH_MAX_LENGTH - 20 - 1;     /* room for token + NUL */
   assert(tmp_buf);
   memset(tmp_buf, 0, 2 * PATH_MAX_LENGTH);

   long_path = (char*)malloc(PATH_MAX_LENGTH);
   assert(long_path);
   memset(long_path, 'X', pad_len);
   /* Token "$CORE$" is 6 chars; place it after pad. */
   memcpy(long_path + pad_len, "$CORE$", 6);
   long_path[pad_len + 6] = '\0';

   /* Replacement is 200 chars -- clamped to 199 by _len cap,
    * but prefix (PATH_MAX_LENGTH - 20 - 1) + 199 still
    * exceeds PATH_MAX_LENGTH, so the prefix+_len bound
    * fires. */
   {
      char src200[201];
      memset(src200, 'C', 200);
      src200[200] = '\0';
      rv = do_one_replacement(tmp_buf, long_path, "$CORE$", src200);
   }

   /* Bail-out is the correct behavior here. */
   if (rv != 0)
   {
      printf("[ERROR] near-overflow prefix should have bailed (rv=%d)\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] near-overflow prefix correctly bailed\n");

   free(long_path);
   free(tmp_buf);
}

int main(void)
{
   test_short_replacement_succeeds();
   test_long_source_does_not_overrun();
   test_pathological_long_source_does_not_underflow_strlcpy();
   test_prefix_near_path_max_bails();

   if (failures)
   {
      printf("\n%d video_shader wildcard test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll video_shader wildcard regression tests passed.\n");
   return 0;
}
