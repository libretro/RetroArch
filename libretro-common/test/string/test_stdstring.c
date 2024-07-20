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

#include <string/stdstring.h>
#include <encodings/utf.h>

#define SUITE_NAME "stdstring"

START_TEST (test_string_filter)
{
   char test1[] = "foo bar some string";
   char test2[] = "";
   string_remove_all_chars(test1, 's');
   string_remove_all_chars(test2, '0');
   string_remove_all_chars(NULL, 'a');
   ck_assert(!strcmp(test1, "foo bar ome tring"));
   ck_assert(!strcmp(test2, ""));
}
END_TEST

START_TEST (test_string_replace)
{
   char test1[] = "foo bar some string";
   string_replace_all_chars(test1, 's', 'S');
   string_replace_all_chars(NULL, 'a', 'A');
   ck_assert(!strcmp(test1, "foo bar Some String"));
}
END_TEST

START_TEST (test_string_case)
{
   char test1[] = "foo";
   char test2[] = "01foOo[]_";
   ck_assert(!strcmp(string_to_upper(test1), "FOO"));
   ck_assert(!strcmp(string_to_upper(test2), "01FOOO[]_"));
   ck_assert(!strcmp(string_to_lower(test2), "01fooo[]_"));
}
END_TEST

START_TEST (test_string_char_classify)
{
   ck_assert(ISSPACE(' '));
   ck_assert(ISSPACE('\n'));
   ck_assert(ISSPACE('\r'));
   ck_assert(ISSPACE('\t'));
   ck_assert(!ISSPACE('a'));

   ck_assert(ISALPHA('a'));
   ck_assert(ISALPHA('Z'));
   ck_assert(!ISALPHA('5'));

   ck_assert(ISALNUM('a'));
   ck_assert(ISALNUM('Z'));
   ck_assert(ISALNUM('5'));
}
END_TEST

START_TEST (test_string_num_conv)
{
   ck_assert_uint_eq(3, string_to_unsigned("3"));
   ck_assert_uint_eq(2147483647, string_to_unsigned("2147483647"));
   ck_assert_uint_eq(0, string_to_unsigned("foo"));
   ck_assert_uint_eq(0, string_to_unsigned("-1"));
   ck_assert_uint_eq(0, string_to_unsigned(NULL));

   ck_assert_uint_eq(10, string_hex_to_unsigned("0xa"));
   ck_assert_uint_eq(10, string_hex_to_unsigned("a"));
   ck_assert_uint_eq(255, string_hex_to_unsigned("FF"));
   ck_assert_uint_eq(255, string_hex_to_unsigned("0xff"));
   ck_assert_uint_eq(0, string_hex_to_unsigned("0xfzzf"));
   ck_assert_uint_eq(0, string_hex_to_unsigned("0x"));
   ck_assert_uint_eq(0, string_hex_to_unsigned("0xx"));
   ck_assert_uint_eq(0, string_hex_to_unsigned(NULL));
}
END_TEST

START_TEST (test_string_tokenizer)
{
   char *testinput = "@@1@@2@@3@@@@9@@@";
   char **ptr = &testinput;
   char *token = NULL;
   token = string_tokenize(ptr, "@@");
   ck_assert(token != NULL);
   ck_assert(!strcmp(token, ""));
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert(token != NULL);
   ck_assert(!strcmp(token, "1"));
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert(token != NULL);
   ck_assert(!strcmp(token, "2"));
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert(token != NULL);
   ck_assert(!strcmp(token, "3"));
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert(token != NULL);
   ck_assert(!strcmp(token, ""));
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert(token != NULL);
   ck_assert(!strcmp(token, "9"));
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert(token != NULL);
   ck_assert(!strcmp(token, "@"));
   free(token);
   token = string_tokenize(ptr, "@@");
   ck_assert(token == NULL);
}
END_TEST

START_TEST (test_string_replacesubstr)
{
   char *res = string_replace_substring("foobaarhellowooorldtest", "oo", "ooo");
   ck_assert(res != NULL);
   ck_assert(!strcmp(res, "fooobaarhellowoooorldtest"));
   free(res);
}
END_TEST

START_TEST (test_string_trim)
{
   char test1[] = "\t \t\nhey there \n \n";
   char test2[] = "\t \t\nhey there \n \n";
   char test3[] = "\t \t\nhey there \n \n";
   ck_assert(string_trim_whitespace_left(test1) ==  (char*)test1);
   ck_assert(!strcmp(test1, "hey there \n \n"));
   ck_assert(string_trim_whitespace_right(test2) ==  (char*)test2);
   ck_assert(!strcmp(test2, "\t \t\nhey there"));
   ck_assert(string_trim_whitespace(test3) ==  (char*)test3);
   ck_assert(!strcmp(test3, "hey there"));
}
END_TEST

START_TEST (test_string_comparison)
{
   ck_assert(string_is_not_equal_fast("foo", "bar", 3));
   ck_assert(string_is_equal_fast("foo2", "foo2", 4));
   ck_assert(!string_is_equal_fast("foo1", "foo2", 4));
   ck_assert(string_is_equal_fast("foo1", "foo2", 3));
}
END_TEST

START_TEST (test_word_wrap)
{
   const char *testtxt = (
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam nec "
      "enim quis orci euismod efficitur at nec arcu. Vivamus imperdiet est "
      "feugiat massa rhoncus porttitor at vitae ante. Nunc a orci vel ipsum "
      "tempor posuere sed a lacus. Ut erat odio, ultrices vitae iaculis "
      "fringilla, iaculis ut eros.\nSed facilisis viverra lectus et "
      "ullamcorper. Aenean risus ex, ornare eget scelerisque ac, imperdiet eu "
      "ipsum. Morbi pellentesque erat metus, sit amet aliquet libero rutrum "
      "et. Integer non ullamcorper tellus.");
   const char *expected = (
      "Lorem ipsum dolor sit amet, consectetur\n"
      "adipiscing elit. Nam nec enim quis orci\n"
      "euismod efficitur at nec arcu. Vivamus\n"
      "imperdiet est feugiat massa rhoncus\n"
      "porttitor at vitae ante. Nunc a orci\n"
      "vel ipsum tempor posuere sed a lacus.\n"
      "Ut erat odio, ultrices vitae iaculis\n"
      "fringilla, iaculis ut eros.\n"
      "Sed facilisis viverra lectus et\n"
      "ullamcorper. "
      "Aenean risus ex, ornare eget scelerisque ac, imperdiet eu ipsum. Morbi "
      "pellentesque erat metus, sit amet aliquet libero rutrum et. Integer "
      "non ullamcorper tellus.");

   char output[1024];

   word_wrap(output, sizeof(output), testtxt, strlen(testtxt), 40, 100, 10);
   ck_assert(!strcmp(output, expected));
}
END_TEST

START_TEST (test_strlcpy)
{
   char buf1[8];
   ck_assert_uint_eq(3, strlcpy(buf1, "foo", sizeof(buf1)));
   ck_assert(!memcmp(buf1, "foo", 4));
   ck_assert_uint_eq(11, strlcpy(buf1, "foo12345678", sizeof(buf1)));
   ck_assert(!memcmp(buf1, "foo1234", 8));
}
END_TEST

START_TEST (test_strlcat)
{
   char buf1[8];
   buf1[0] = 'f';
   buf1[1] = '\0';
   ck_assert_uint_eq(10, strlcat(buf1, "ooooooooo", sizeof(buf1)));
   ck_assert(!memcmp(buf1, "foooooo\0", 8));
   ck_assert_uint_eq(13, strlcat(buf1, "123456", sizeof(buf1)));
   ck_assert(!memcmp(buf1, "foooooo\0", 8));
}
END_TEST

START_TEST (test_strldup)
{
   char buf1[8] = "foo";
   char *tv1 = strldup(buf1, 16);
   char *tv2 = strldup(buf1, 2);
   ck_assert(tv1 != (char*)buf1);
   ck_assert(tv2 != (char*)buf1);
   ck_assert_uint_eq(strlen(tv2), 1);
   ck_assert(tv2[0] == 'f' && tv2[1] == 0);
   free(tv1);
   free(tv2);
}
END_TEST

START_TEST (test_utf8_conv_utf32)
{
   uint32_t output[12];
   const char test1[] = "aæ⠻จйγチℝ\xff";
   ck_assert_uint_eq(8, utf8_conv_utf32(output, 12, test1, strlen(test1)));
   ck_assert_uint_eq(97, output[0]);
   ck_assert_uint_eq(230, output[1]);
   ck_assert_uint_eq(10299, output[2]);
   ck_assert_uint_eq(3592, output[3]);
   ck_assert_uint_eq(1081, output[4]);
   ck_assert_uint_eq(947, output[5]);
   ck_assert_uint_eq(12481, output[6]);
   ck_assert_uint_eq(8477, output[7]);
}
END_TEST

START_TEST (test_utf8_util)
{
   const char *test1 = "aæ⠻จ𠀤";
   const char **tptr = &test1;
   char out[64];
   ck_assert_uint_eq(utf8len(test1), 5);
   ck_assert_uint_eq(utf8len(NULL), 0);
   ck_assert(&test1[1 + 2 + 3] == utf8skip(test1, 3));

   ck_assert_uint_eq(97, utf8_walk(tptr));
   ck_assert_uint_eq(230, utf8_walk(tptr));
   ck_assert_uint_eq(10299, utf8_walk(tptr));
   ck_assert_uint_eq(3592, utf8_walk(tptr));
   ck_assert_uint_eq(131108, utf8_walk(tptr));

#if 0
   ck_assert_uint_eq(1, utf8cpy(out, 64, test1, 1));
#endif
}
END_TEST

START_TEST (test_utf16_conv)
{
   const uint16_t test1[] = {0x0061, 0x00e6, 0x283b, 0x0e08, 0xd840, 0xdc24};
   char out[64];
   size_t outlen = sizeof(out);
   ck_assert(utf16_conv_utf8((uint8_t*)out, &outlen, test1, sizeof(test1) / 2));
   ck_assert_uint_eq(outlen, 13);
   ck_assert(!memcmp(out, "aæ⠻จ𠀤", 13));
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_string_comparison);
   tcase_add_test(tc_core, test_string_num_conv);
   tcase_add_test(tc_core, test_string_char_classify);
   tcase_add_test(tc_core, test_string_case);
   tcase_add_test(tc_core, test_string_filter);
   tcase_add_test(tc_core, test_string_replace);
   tcase_add_test(tc_core, test_string_tokenizer);
   tcase_add_test(tc_core, test_string_trim);
   tcase_add_test(tc_core, test_string_replacesubstr);
   tcase_add_test(tc_core, test_word_wrap);
   tcase_add_test(tc_core, test_strlcpy);
   tcase_add_test(tc_core, test_strlcat);
   tcase_add_test(tc_core, test_strldup);
   tcase_add_test(tc_core, test_utf8_conv_utf32);
   tcase_add_test(tc_core, test_utf16_conv);
   tcase_add_test(tc_core, test_utf8_util);
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
