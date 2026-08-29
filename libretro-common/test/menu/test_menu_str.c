/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_menu_str.c).
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

/* Tests for menu_str_ref()/menu_str_unref(), the shared strings behind
 * the menu drivers' node fullpaths.
 *
 * Two things have to hold for the drivers to be able to treat the
 * result as an ordinary char*.  Repeated references to equal content
 * must return one block, which is the whole point -- a list of N rows
 * carries one copy of its fullpath rather than N.  And that block must
 * survive until the last holder drops it, however the drops are
 * ordered, because nodes are released in whatever order the list is
 * torn down in.
 *
 * The content-equal-but-different-buffer case is the one that matters
 * most: the caller's string lives in a list entry that can be freed and
 * a new one allocated at the same address between builds, so a cache
 * keyed on the source pointer would hand back a stale string. Passing a
 * distinct buffer with equal content pins the intended behaviour, and
 * passing the same address with different content pins the other side
 * of it.
 *
 * Under ASan these also check that nothing is leaked or used after
 * free; menu_str_cache_flush() at the end of each test is what the menu
 * does at teardown.
 */

#include <check.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../../menu/menu_str.h"

#define SUITE_NAME "menu_str"

static const char *PATH =
   "/home/user/.config/retroarch/playlists/Nintendo - SNES.lpl";

START_TEST (test_ref_returns_content)
{
   char *a = menu_str_ref(PATH);
   ck_assert_ptr_nonnull(a);
   /* The reference is the characters, not a header the caller has to
    * step over. */
   ck_assert_str_eq(a, PATH);
   menu_str_unref(a);
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_equal_content_shares_one_block)
{
   char buf[128];
   char *a, *b;

   /* A distinct buffer with equal content: the drivers get the same
    * fullpath through a pointer that may or may not be the one from
    * last time, and must still share. */
   strcpy(buf, PATH);
   a = menu_str_ref(PATH);
   b = menu_str_ref(buf);

   ck_assert_ptr_eq(a, b);
   ck_assert_str_eq(a, PATH);

   menu_str_unref(a);
   menu_str_unref(b);
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_same_address_different_content)
{
   char buf[128];
   char *a, *b;

   /* The stale-cache case: same source address, different content.
    * Sharing here would hand the second caller the first string. */
   strcpy(buf, PATH);
   a = menu_str_ref(buf);
   strcpy(buf, "/some/other/playlist.lpl");
   b = menu_str_ref(buf);

   ck_assert_ptr_ne(a, b);
   ck_assert_str_eq(a, PATH);
   ck_assert_str_eq(b, "/some/other/playlist.lpl");

   menu_str_unref(a);
   menu_str_unref(b);
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_block_survives_until_last_holder)
{
   char *refs[64];
   size_t i;

   for (i = 0; i < 64; i++)
   {
      refs[i] = menu_str_ref(PATH);
      ck_assert_ptr_eq(refs[i], refs[0]);
   }

   /* Drop in the same order a list is torn down in: every read before
    * the last drop must still see live memory. */
   for (i = 0; i < 63; i++)
   {
      menu_str_unref(refs[i]);
      ck_assert_str_eq(refs[63], PATH);
   }

   menu_str_unref(refs[63]);
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_reverse_order_release)
{
   char *refs[16];
   size_t i;

   for (i = 0; i < 16; i++)
      refs[i] = menu_str_ref(PATH);

   for (i = 16; i > 0; i--)
   {
      menu_str_unref(refs[i - 1]);
      if (i > 1)
         ck_assert_str_eq(refs[0], PATH);
   }

   menu_str_cache_flush();
}
END_TEST

START_TEST (test_ref_null_returns_null)
{
   ck_assert_ptr_null(menu_str_ref(NULL));
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_unref_null_is_ignored)
{
   menu_str_unref(NULL);
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_empty_string)
{
   char *a = menu_str_ref("");
   ck_assert_ptr_nonnull(a);
   ck_assert_str_eq(a, "");
   menu_str_unref(a);
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_cache_flush_is_idempotent)
{
   char *a = menu_str_ref(PATH);
   menu_str_unref(a);
   /* The cache still holds its own reference here; flushing twice must
    * not drop one that is no longer there. */
   menu_str_cache_flush();
   menu_str_cache_flush();
}
END_TEST

START_TEST (test_alternating_strings)
{
   size_t i;

   /* Only one string is cached, so alternating defeats the cache
    * entirely.  Nothing may be shared across the switch, and nothing
    * may be leaked or double-freed by the cache handover. */
   for (i = 0; i < 32; i++)
   {
      char *a = menu_str_ref(PATH);
      char *b = menu_str_ref("/other.lpl");
      ck_assert_ptr_ne(a, b);
      ck_assert_str_eq(a, PATH);
      ck_assert_str_eq(b, "/other.lpl");
      menu_str_unref(a);
      menu_str_unref(b);
   }

   menu_str_cache_flush();
}
END_TEST

Suite *create_suite(void)
{
   Suite *s       = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");

   tcase_add_test(tc_core, test_ref_returns_content);
   tcase_add_test(tc_core, test_equal_content_shares_one_block);
   tcase_add_test(tc_core, test_same_address_different_content);
   tcase_add_test(tc_core, test_block_survives_until_last_holder);
   tcase_add_test(tc_core, test_reverse_order_release);
   tcase_add_test(tc_core, test_ref_null_returns_null);
   tcase_add_test(tc_core, test_unref_null_is_ignored);
   tcase_add_test(tc_core, test_empty_string);
   tcase_add_test(tc_core, test_cache_flush_is_idempotent);
   tcase_add_test(tc_core, test_alternating_strings);

   suite_add_tcase(s, tc_core);

   return s;
}

int main(void)
{
   int num_fail;
   Suite   *s  = create_suite();
   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail    = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
