/* Copyright  (C) 2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_stdstring.c).
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

#include <check.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <utils/md5.h>

#define SUITE_NAME "hash"

START_TEST (test_md5)
{
   uint8_t output[16];
   MD5_CTX ctx;
   MD5_Init(&ctx);
   MD5_Final(output, &ctx);
   ck_assert(!memcmp(
      "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e",
      output, 16));
   MD5_Init(&ctx);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Final(output, &ctx);
   ck_assert(!memcmp(
      "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6",
      output, 16));
   MD5_Init(&ctx);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Final(output, &ctx);
   ck_assert(!memcmp(
      "\x4e\x67\xdb\x4a\x7a\x40\x6b\x0c\xfd\xad\xd8\x87\xcd\xe7\x88\x8e",
      output, 16));
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_md5);
   suite_add_tcase(s, tc_core);

   return s;
}

int main(void)
{
   int num_fail;
   Suite *s = create_suite();
   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
