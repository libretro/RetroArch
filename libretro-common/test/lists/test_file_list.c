/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_file_list.c).
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

/* Tests for file_list_t::actiondata_free, the optional destructor for
 * item_file::actiondata.
 *
 * It exists because the menu's actiondata (menu_file_list_cbs_t) owns
 * a further allocation, so it cannot be torn down with a plain free().
 * Seven call sites across menu_driver.c, xmb.c and ozone.c can destroy
 * a menu list; routing them all through one hook is what keeps them
 * from each needing to know how to take a cbs apart.
 *
 * Two properties are worth pinning down.  A list that has not set the
 * hook must keep the old behaviour exactly -- every file_list_t outside
 * the menu relies on it, and they get NULL by being calloc()ed or
 * memset() to zero rather than by saying so.  And the hook must never
 * be handed NULL, because a destructor written for a real object is
 * under no obligation to tolerate it.
 */

#include <check.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <lists/file_list.h>

#define SUITE_NAME "file_list"

static int   dtor_calls;
static void *dtor_seen[8];

static void test_dtor(void *actiondata)
{
   ck_assert_ptr_nonnull(actiondata);
   if (dtor_calls < (int)(sizeof(dtor_seen) / sizeof(dtor_seen[0])))
      dtor_seen[dtor_calls] = actiondata;
   dtor_calls++;
   free(actiondata);
}

static void reset_dtor(void)
{
   dtor_calls = 0;
   memset(dtor_seen, 0, sizeof(dtor_seen));
}

/* A file_list_t as every caller in tree obtains one: zeroed. */
static void list_init(file_list_t *list)
{
   memset(list, 0, sizeof(*list));
}

static void *set_actiondata(file_list_t *list, size_t idx, int tag)
{
   int *p = (int*)malloc(sizeof(int));
   *p     = tag;
   list->list[idx].actiondata = p;
   return p;
}

START_TEST (test_actiondata_free_uses_hook)
{
   file_list_t list;
   void *p;

   reset_dtor();
   list_init(&list);
   list.actiondata_free = test_dtor;

   ck_assert(file_list_append(&list, "path", "label", 0, 0, 0));
   p = set_actiondata(&list, 0, 0x1234);

   file_list_free_actiondata(&list, 0);

   ck_assert_int_eq(dtor_calls, 1);
   ck_assert_ptr_eq(dtor_seen[0], p);
   ck_assert_ptr_null(list.list[0].actiondata);

   file_list_deinitialize(&list);
}
END_TEST

/* No hook is the default and must stay a plain free().  The sanitizer
 * catches the leak if it stops being one. */
START_TEST (test_actiondata_free_without_hook)
{
   file_list_t list;

   reset_dtor();
   list_init(&list);

   ck_assert_ptr_null(list.actiondata_free);

   ck_assert(file_list_append(&list, "path", "label", 0, 0, 0));
   set_actiondata(&list, 0, 0x1234);

   file_list_free_actiondata(&list, 0);

   ck_assert_int_eq(dtor_calls, 0);
   ck_assert_ptr_null(list.list[0].actiondata);

   file_list_deinitialize(&list);
}
END_TEST

/* An entry with no actiondata must not reach the destructor at all. */
START_TEST (test_actiondata_free_skips_null)
{
   file_list_t list;

   reset_dtor();
   list_init(&list);
   list.actiondata_free = test_dtor;

   ck_assert(file_list_append(&list, "path", "label", 0, 0, 0));
   ck_assert_ptr_null(list.list[0].actiondata);

   file_list_free_actiondata(&list, 0);
   ck_assert_int_eq(dtor_calls, 0);

   /* Twice over: the second call sees the slot already NULL. */
   set_actiondata(&list, 0, 0x1234);
   file_list_free_actiondata(&list, 0);
   file_list_free_actiondata(&list, 0);
   ck_assert_int_eq(dtor_calls, 1);

   file_list_deinitialize(&list);
}
END_TEST

START_TEST (test_actiondata_free_null_list)
{
   reset_dtor();
   file_list_free_actiondata(NULL, 0);
   ck_assert_int_eq(dtor_calls, 0);
}
END_TEST

/* Tearing the whole list down has to route through the hook too --
 * that is the path that made it necessary, since file_list_free() and
 * file_list_deinitialize() share file_list_deinitialize_internal(),
 * and menu_list_free_list() reaches it.  The tests use the
 * deinitialize form because file_list_free() also free()s the
 * file_list_t itself, which here lives on the stack. */
START_TEST (test_deinitialize_uses_hook)
{
   file_list_t list;
   void *p[3];
   int i;

   reset_dtor();
   list_init(&list);
   list.actiondata_free = test_dtor;

   for (i = 0; i < 3; i++)
   {
      ck_assert(file_list_append(&list, "path", "label", 0, 0, 0));
      p[i] = set_actiondata(&list, (size_t)i, 0x100 + i);
   }

   file_list_deinitialize(&list);

   ck_assert_int_eq(dtor_calls, 3);
   for (i = 0; i < 3; i++)
      ck_assert_ptr_eq(dtor_seen[i], p[i]);
}
END_TEST

/* Entries without actiondata are skipped while their neighbours are
 * not. */
START_TEST (test_deinitialize_mixed_entries)
{
   file_list_t list;
   void *p;
   int i;

   reset_dtor();
   list_init(&list);
   list.actiondata_free = test_dtor;

   for (i = 0; i < 3; i++)
      ck_assert(file_list_append(&list, "path", "label", 0, 0, 0));

   p = set_actiondata(&list, 1, 0x777);

   file_list_deinitialize(&list);

   ck_assert_int_eq(dtor_calls, 1);
   ck_assert_ptr_eq(dtor_seen[0], p);
}
END_TEST

/* file_list_clear() deliberately does not touch actiondata; it clears
 * the strings and the size and leaves ownership with the caller.  The
 * menu relies on that split, so it is worth stating rather than
 * discovering. */
START_TEST (test_file_list_clear_leaves_actiondata)
{
   file_list_t list;
   void *p;

   reset_dtor();
   list_init(&list);
   list.actiondata_free = test_dtor;

   ck_assert(file_list_append(&list, "path", "label", 0, 0, 0));
   p = set_actiondata(&list, 0, 0x1234);

   file_list_clear(&list);

   ck_assert_int_eq(dtor_calls, 0);
   ck_assert_ptr_eq(list.list[0].actiondata, p);

   /* Still the caller's to release. */
   file_list_free_actiondata(&list, 0);
   ck_assert_int_eq(dtor_calls, 1);

   file_list_deinitialize(&list);
}
END_TEST

/* The hook lives in the list, not in the entries, so growing past
 * capacity must not disturb it. */
START_TEST (test_hook_survives_growth)
{
   file_list_t list;
   int i;
   const int n = 64;

   reset_dtor();
   list_init(&list);
   list.actiondata_free = test_dtor;

   for (i = 0; i < n; i++)
   {
      ck_assert(file_list_append(&list, "path", "label", 0, 0, 0));
      set_actiondata(&list, (size_t)i, i);
   }

   ck_assert_ptr_eq((void*)(uintptr_t)list.actiondata_free,
         (void*)(uintptr_t)test_dtor);
   ck_assert_uint_ge((unsigned)list.capacity, (unsigned)n);

   file_list_deinitialize(&list);
   ck_assert_int_eq(dtor_calls, n);
}
END_TEST

/* file_list_insert() has its own initialisation path; an inserted
 * entry must start with no actiondata rather than inheriting whatever
 * was in the slot. */
START_TEST (test_insert_starts_with_null_actiondata)
{
   file_list_t list;

   reset_dtor();
   list_init(&list);
   list.actiondata_free = test_dtor;

   ck_assert(file_list_append(&list, "a", "a", 0, 0, 0));
   set_actiondata(&list, 0, 0x1234);

   ck_assert(file_list_insert(&list, "b", "b", 0, 0, 0, 0));
   ck_assert_ptr_null(list.list[0].actiondata);

   file_list_deinitialize(&list);

   /* Only the one that was set is destroyed, and it moved with its
    * entry rather than being left behind at index 0. */
   ck_assert_int_eq(dtor_calls, 1);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s       = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");

   tcase_add_test(tc_core, test_actiondata_free_uses_hook);
   tcase_add_test(tc_core, test_actiondata_free_without_hook);
   tcase_add_test(tc_core, test_actiondata_free_skips_null);
   tcase_add_test(tc_core, test_actiondata_free_null_list);
   tcase_add_test(tc_core, test_deinitialize_uses_hook);
   tcase_add_test(tc_core, test_deinitialize_mixed_entries);
   tcase_add_test(tc_core, test_file_list_clear_leaves_actiondata);
   tcase_add_test(tc_core, test_hook_survives_growth);
   tcase_add_test(tc_core, test_insert_starts_with_null_actiondata);

   suite_add_tcase(s, tc_core);
   return s;
}

int main(void)
{
   int num_fail;
   Suite   *s  = create_suite();
   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
