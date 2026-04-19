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

/* Regression for commit <round2-TBD> (config_get_int family silent zero).
 *
 * config_get_int, config_get_uint, config_get_uint64 and
 * config_get_hex used to return true with *in = 0 when handed a
 * string that has no leading digits at all.  A typo in a config
 * file (width = abc) would silently become width = 0 with no
 * indication of failure.  The patch adds the same end-pointer and
 * no-digits-consumed checks that config_get_size_t already used.
 *
 * These test cases all must return false on patched code.  On
 * unpatched code they return true with *in = 0.
 */
static void test_config_get_int_rejects(const char *raw_val)
{
   char cfgtext[256];
   char *copy;
   config_file_t *cfg;
   int              out_int   = 0x5a5a5a;
   unsigned         out_uint  = 0x5a5a5a;
   uint64_t         out_u64   = 0x5a5a5a;

   /* Quote the value so the parser preserves trailing text and
    * whitespace; without quotes the parser stops at the first space
    * and "42 extra" would be stored as just "42". */
   snprintf(cfgtext, sizeof(cfgtext),
         "ival = \"%s\"\nuval = \"%s\"\nu64 = \"%s\"\n",
         raw_val, raw_val, raw_val);

   copy = strdup(cfgtext);
   cfg  = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!cfg)
      abort();

   if (config_get_int(cfg, "ival", &out_int))
   {
      printf("[FAILED] config_get_int accepted \"%s\" -> %d\n", raw_val, out_int);
      abort();
   }
   if (out_int != 0x5a5a5a)
   {
      printf("[FAILED] config_get_int wrote *in on reject for \"%s\": got %d\n",
            raw_val, out_int);
      abort();
   }
   if (config_get_uint(cfg, "uval", &out_uint))
   {
      printf("[FAILED] config_get_uint accepted \"%s\" -> %u\n", raw_val, out_uint);
      abort();
   }
   if (config_get_uint64(cfg, "u64", &out_u64))
   {
      printf("[FAILED] config_get_uint64 accepted \"%s\" -> %llu\n",
            raw_val, (unsigned long long)out_u64);
      abort();
   }
   printf("[SUCCESS] rejected non-numeric value \"%s\"\n", raw_val);
   config_file_free(cfg);
}

/* config_get_hex accepts base-16 digits ([0-9a-fA-F]) which includes
 * strings like "abc" and "face" -- those are valid hex.  Test with
 * characters that are not valid in any base. */
static void test_config_get_hex_rejects(const char *raw_val)
{
   char cfgtext[256];
   char *copy;
   config_file_t *cfg;
   unsigned out_hex = 0x5a5a5a;

   snprintf(cfgtext, sizeof(cfgtext), "hval = \"%s\"\n", raw_val);
   copy = strdup(cfgtext);
   cfg  = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!cfg)
      abort();

   if (config_get_hex(cfg, "hval", &out_hex))
   {
      printf("[FAILED] config_get_hex accepted \"%s\" -> 0x%x\n",
            raw_val, out_hex);
      abort();
   }
   if (out_hex != 0x5a5a5a)
   {
      printf("[FAILED] config_get_hex wrote *in on reject for \"%s\"\n", raw_val);
      abort();
   }
   printf("[SUCCESS] config_get_hex rejected non-hex \"%s\"\n", raw_val);
   config_file_free(cfg);
}

static void test_config_get_int_accepts(const char *raw_val, int want_int)
{
   char cfgtext[256];
   char *copy;
   config_file_t *cfg;
   int out_int = 0;

   snprintf(cfgtext, sizeof(cfgtext), "ival = \"%s\"\n", raw_val);
   copy = strdup(cfgtext);
   cfg  = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!cfg)
      abort();

   if (!config_get_int(cfg, "ival", &out_int) || out_int != want_int)
   {
      printf("[FAILED] config_get_int(\"%s\") expected %d got %d\n",
            raw_val, want_int, out_int);
      abort();
   }
   printf("[SUCCESS] config_get_int(\"%s\") == %d\n", raw_val, want_int);
   config_file_free(cfg);
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

   /* Non-numeric input -- must all be rejected. */
   test_config_get_int_rejects("abc");
   test_config_get_int_rejects("");
   test_config_get_int_rejects(".");
   test_config_get_int_rejects("-");
   test_config_get_int_rejects("42abc");     /* trailing garbage */
   test_config_get_int_rejects("42 extra");  /* trailing text after space */

   /* config_get_hex accepts [0-9a-fA-F] -- use characters outside it. */
   test_config_get_hex_rejects("xyz");
   test_config_get_hex_rejects("");
   test_config_get_hex_rejects("g");
   test_config_get_hex_rejects("deadbeefz");  /* trailing non-hex */
   test_config_get_hex_rejects("42 extra");   /* trailing text */

   /* Positive cases -- must still accept normal integers. */
   test_config_get_int_accepts("0",      0);
   test_config_get_int_accepts("42",     42);
   test_config_get_int_accepts("-17",   -17);
   test_config_get_int_accepts("0x10",   16);  /* hex via base-0 detection */
   test_config_get_int_accepts("010",     8);  /* octal  via base-0 detection */
}
