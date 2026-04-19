/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file_test.c).
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
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <file/config_file.h>

static void test_config_file_parse_contains(
      const char *cfgtext,
      const char *key, const char *val)
{
   char *cfgtext_copy = strdup(cfgtext);
   config_file_t *cfg = config_file_new_from_string(cfgtext_copy, NULL);
   char          *out = NULL;
   bool            ok = false;

   free(cfgtext_copy);

   if (!cfg)
      abort();

   ok = config_get_string(cfg, key, &out);
   if (ok != (bool)val)
      abort();
   if (!val)
      return;

   if (!out)
      out = strdup("");
   if (strcmp(out, val) != 0)
   {
      printf("[FAILED] Key [%s] Doesn't contain val [%s]\n", key, val);
      abort();
   }
   printf("[SUCCESS] Key [%s] contains val [%s]\n", key, val);
   free(out);
}

/* Regression for commit 87f2d0b (memcmp OOB on short '#' comment lines).
 *
 * The bug was in config_file_parse_line() reading 8 or 10 bytes past
 * the end of a shrunken line buffer produced by filestream_getline().
 * Triggering it requires going through the file path, not the
 * from-string path: config_file_new_from_string() keeps the entire
 * string live, while filestream_getline() realloc-shrinks each line
 * to exactly strlen+1 bytes for any line shorter than ~192 chars.
 *
 * Under AddressSanitizer the unpatched code aborts with a
 * heap-buffer-overflow READ on any short '#' line.  On non-ASan
 * builds the comparison's result depends on stale heap bytes
 * adjacent to the allocation -- a real attacker-observable
 * non-determinism, not a cosmetic issue.
 */
static void test_config_file_short_comments(void)
{
   const char *content =
      "#\n"
      "#h\n"
      "#hi\n"
      "#inc\n"
      "#includ\n"
      "#includez\n"
      "#referenc\n"
      "#referencez\n"
      "foo = \"bar\"\n";
   const char *tmp_path = "rarch_cfg_short_comment_test.cfg";
   FILE          *fp    = fopen(tmp_path, "wb");
   config_file_t *cfg;
   char          *out   = NULL;

   if (!fp)
      abort();
   fputs(content, fp);
   fclose(fp);

   cfg = config_file_new(tmp_path);
   remove(tmp_path);
   if (!cfg)
      abort();

   if (!config_get_string(cfg, "foo", &out) || !out || strcmp(out, "bar") != 0)
   {
      printf("[FAILED] short-comment regression: foo!=bar (got %s)\n",
            out ? out : "(null)");
      abort();
   }
   printf("[SUCCESS] short '#' comment lines parsed without OOB\n");
   free(out);
}

int main(void)
{
   test_config_file_parse_contains("foo = \"bar\"\n",   "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"",     "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"\r\n", "foo", "bar");
   test_config_file_parse_contains("foo = \"bar\"",     "foo", "bar");

   test_config_file_parse_contains("foo = \"\"\n",   "foo", "");
   test_config_file_parse_contains("foo = \"\"",     "foo", "");
   test_config_file_parse_contains("foo = \"\"\r\n", "foo", "");
   test_config_file_parse_contains("foo = \"\"",     "foo", "");

   test_config_file_parse_contains("foo = \"\"\n",   "bar", NULL);
   test_config_file_parse_contains("foo = \"\"",     "bar", NULL);
   test_config_file_parse_contains("foo = \"\"\r\n", "bar", NULL);
   test_config_file_parse_contains("foo = \"\"",     "bar", NULL);

   test_config_file_short_comments();
}
