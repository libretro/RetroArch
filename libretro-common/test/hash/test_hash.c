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

#include <lrc_hash.h>

#define SUITE_NAME "hash"

START_TEST (test_sha256)
{
   char output[65];
   sha256_hash(output, (uint8_t*)"abc", 3);
   ck_assert(!strcmp(output,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}
END_TEST

START_TEST (test_sha1)
{
   char output[41];
   char tmpfile[512];
   FILE *fd;
   tmpnam(tmpfile);
   fd = fopen(tmpfile, "wb");
   ck_assert(fd != NULL);
   fwrite("abc", 1, 3, fd);
   fclose(fd);
   sha1_calculate(tmpfile, output);

   ck_assert(!strcmp(output,
      "A9993E364706816ABA3E25717850C26C9CD0D89D"));
}
END_TEST

START_TEST (test_djb2)
{
   ck_assert_uint_eq(djb2_calculate("retroarch"), 0xFADF3BCF);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_sha256);
   tcase_add_test(tc_core, test_sha1);
   tcase_add_test(tc_core, test_djb2);
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
