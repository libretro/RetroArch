/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_linked_list.c).
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

#include <lists/linked_list.h>

#define SUITE_NAME "Linked List"

static char *_value_1 = "value1";
static char *_value_2 = "value2";
static char *_value_3 = "value3";

START_TEST (test_linked_list_create)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_nonnull(list);
   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_free)
{
   linked_list_t *queue = linked_list_new();
   linked_list_free(queue, NULL);
   linked_list_free(NULL, NULL);
}
END_TEST

static int _free_alloced_value_count;
static void _free_alloced_value(void *value)
{
   _free_alloced_value_count++;
   free(value);
}

START_TEST (test_linked_list_free_with_fn)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, malloc(1));
   linked_list_add(list, malloc(1));
   linked_list_add(list, malloc(1));

   _free_alloced_value_count = 0;
   linked_list_free(list, &_free_alloced_value);

   ck_assert_int_eq(3, _free_alloced_value_count);
}
END_TEST

static void _verify_list(linked_list_t *list, int size, ...)
{
   va_list values_list;
   void **values;
   int i;
   linked_list_iterator_t *iterator;

   values = (void **)malloc(size * sizeof(void *));

   ck_assert_int_eq(linked_list_size(list), size);

   va_start(values_list, size);
   for (i = 0; i < size; i++)
   {
      values[i] = va_arg(values_list, void *);
      ck_assert_ptr_eq(values[i], linked_list_get(list, i));
   }
   va_end(values_list);

   iterator = linked_list_iterator(list, true);
   for (i = 0; i < size; i++)
   {
      ck_assert_ptr_nonnull(iterator);
      ck_assert_ptr_eq(values[i], linked_list_iterator_value(iterator));
      iterator = linked_list_iterator_next(iterator);
   }
   ck_assert_ptr_null(iterator);

   iterator = linked_list_iterator(list, false);
   for (i = size - 1; i >= 0; i--)
   {
      ck_assert_ptr_nonnull(iterator);
      ck_assert_ptr_eq(values[i], linked_list_iterator_value(iterator));
      iterator = linked_list_iterator_next(iterator);
   }
   ck_assert_ptr_null(iterator);

   free(values);
}

START_TEST (test_linked_list_add)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_empty)
{
   linked_list_t *list = linked_list_new();
   linked_list_insert(list, 0, _value_1);

   ck_assert_int_eq(linked_list_size(list), 1);
   ck_assert_ptr_eq(linked_list_get(list, 0), _value_1);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);
   linked_list_insert(list, 0, _value_1);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_3);
   linked_list_insert(list, 1, _value_2);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_insert(list, 2, _value_3);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_invalid)
{
   linked_list_t *list = linked_list_new();
   linked_list_insert(list, 2, _value_1);

   ck_assert_int_eq(linked_list_size(list), 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_insert_null)
{
   linked_list_insert(NULL, 0, _value_1);
}
END_TEST

START_TEST (test_linked_list_get_invalid)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get(list, 2));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_get_null)
{
   ck_assert_ptr_null(linked_list_get(NULL, 0));
}
END_TEST

START_TEST (test_linked_list_get_first_matching_null)
{
   ck_assert_ptr_null(linked_list_get_first_matching(NULL, NULL, NULL));
}
END_TEST

START_TEST (test_linked_list_get_first_matching_function_null)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_first_matching(list, NULL, NULL));

   linked_list_free(list, NULL);
}

bool _matches_function(void *value, void *state)
{
   ck_assert_ptr_eq(_value_1, state);
   return value == _value_2;
}

START_TEST (test_linked_list_get_first_matching_no_match)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_first_matching(list, &_matches_function, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_get_first_matching_with_match)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(_value_2, linked_list_get_first_matching(list, &_matches_function, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_get_last_matching_null)
{
   ck_assert_ptr_null(linked_list_get_last_matching(NULL, NULL, NULL));
}
END_TEST

START_TEST (test_linked_list_get_last_matching_function_null)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_last_matching(list, NULL, NULL));

   linked_list_free(list, NULL);
}

START_TEST (test_linked_list_get_last_matching_no_match)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_get_last_matching(list, &_matches_function, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_get_last_matching_with_match)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(_value_2, linked_list_get_last_matching(list, &_matches_function, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_at_null)
{
   ck_assert_ptr_null(linked_list_remove_at(NULL, 0));
}
END_TEST

START_TEST (test_linked_list_remove_at_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_remove_at(list, 0));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_at_invalid)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   linked_list_remove_at(list, 3);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_at_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   linked_list_remove_at(list, 0);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_at_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   linked_list_remove_at(list, 1);

   _verify_list(list, 2, _value_1, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_at_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   linked_list_remove_at(list, 2);

   _verify_list(list, 2, _value_1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_at_only)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);

   linked_list_remove_at(list, 0);

   _verify_list(list, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_null)
{
   ck_assert_ptr_null(linked_list_remove_first(NULL, _value_1));
}
END_TEST

START_TEST (test_linked_list_remove_first_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_remove_first(list, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_not_found)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_null(linked_list_remove_first(list, "foo"));

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_first(list, _value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_first(list, _value_2), _value_2);

   _verify_list(list, 2, _value_1, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_first(list, _value_3), _value_3);

   _verify_list(list, 2, _value_1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_only)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_first(list, _value_1), _value_1);

   _verify_list(list, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_multiple)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_first(list, _value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_1);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_null)
{
   ck_assert_ptr_null(linked_list_remove_last(NULL, _value_1));
}
END_TEST

START_TEST (test_linked_list_remove_last_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_remove_last(list, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_not_found)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_null(linked_list_remove_last(list, "foo"));

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_last(list, _value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_last(list, _value_2), _value_2);

   _verify_list(list, 2, _value_1, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_last(list, _value_3), _value_3);

   _verify_list(list, 2, _value_1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_only)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_last(list, _value_1), _value_1);

   _verify_list(list, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_multiple)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_last(list, _value_1), _value_1);

   _verify_list(list, 2, _value_1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_null)
{
   ck_assert_ptr_null(linked_list_remove_all(NULL, _value_1));
}
END_TEST

START_TEST (test_linked_list_remove_all_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_remove_all(list, _value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_not_found)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_null(linked_list_remove_all(list, "foo"));

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_all(list, _value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_all(list, _value_2), _value_2);

   _verify_list(list, 2, _value_1, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_all(list, _value_3), _value_3);

   _verify_list(list, 2, _value_1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_only)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_all(list, _value_1), _value_1);

   _verify_list(list, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_multiple)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_all(list, _value_1), _value_1);

   _verify_list(list, 1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

bool _match_value_1(void *value)
{
   return _value_1 == value;
}

bool _no_match(void *value)
{
   return false;
}

START_TEST (test_linked_list_remove_first_matching_null)
{
   ck_assert_ptr_null(linked_list_remove_first_matching(NULL, &_match_value_1));
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_remove_first_matching(list, &_match_value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_not_found)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_null(linked_list_remove_first_matching(list, &_no_match));

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_first_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_first_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_first_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_only)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_first_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_first_matching_multiple)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_first_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_1);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_null)
{
   ck_assert_ptr_null(linked_list_remove_last_matching(NULL, &_match_value_1));
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_ptr_null(linked_list_remove_last_matching(list, &_match_value_1));

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_not_found)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_null(linked_list_remove_last_matching(list, &_no_match));

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_last_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_3);

   ck_assert_ptr_eq(linked_list_remove_last_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_last_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_only)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_last_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_last_matching_multiple)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);

   ck_assert_ptr_eq(linked_list_remove_last_matching(list, &_match_value_1), _value_1);

   _verify_list(list, 2, _value_1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_null)
{
   linked_list_remove_all_matching(NULL, &_match_value_1);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_empty)
{
   linked_list_t *list = linked_list_new();
   linked_list_remove_all_matching(list, &_match_value_1);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_not_found)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   linked_list_remove_all_matching(list, &_no_match);

   _verify_list(list, 3, _value_1, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   linked_list_remove_all_matching(list, &_match_value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_3);

   linked_list_remove_all_matching(list, &_match_value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);
   linked_list_add(list, _value_1);

   linked_list_remove_all_matching(list, &_match_value_1);

   _verify_list(list, 2, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_only)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);

   linked_list_remove_all_matching(list, &_match_value_1);

   _verify_list(list, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_remove_all_matching_multiple)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_1);

   linked_list_remove_all_matching(list, &_match_value_1);

   _verify_list(list, 1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_set_at_null)
{
   ck_assert_int_eq(linked_list_set_at(NULL, 0, _value_1) == true, 0);
}
END_TEST

START_TEST (test_linked_list_set_at_empty)
{
   linked_list_t *list = linked_list_new();
   ck_assert_int_eq(linked_list_set_at(list, 0, _value_1) == true, 0);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_set_at_invalid)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   ck_assert_int_eq(linked_list_set_at(list, 1, _value_2) == true, 0);

   linked_list_free(list, NULL);
}
END_TEST

static char *_replacement_value = "foo";

START_TEST (test_linked_list_set_at_first)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_int_eq(linked_list_set_at(list, 0, _replacement_value) == false, 0);

   _verify_list(list, 3, _replacement_value, _value_2, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_set_at_middle)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_int_eq(linked_list_set_at(list, 1, _replacement_value) == false, 0);

   _verify_list(list, 3, _value_1, _replacement_value, _value_3);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_set_at_last)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   ck_assert_int_eq(linked_list_set_at(list, 2, _replacement_value) == false, 0);

   _verify_list(list, 3, _value_1, _value_2, _replacement_value);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_iterator_remove_null)
{
   ck_assert_ptr_null(linked_list_iterator_remove(NULL));
}
END_TEST

START_TEST (test_linked_list_iterator_remove_first)
{
   linked_list_t *list;
   linked_list_iterator_t *iterator;

   list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   iterator = linked_list_iterator(list, true);
   iterator = linked_list_iterator_remove(iterator);

   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(linked_list_iterator_value(iterator), _value_2);
   _verify_list(list, 2, _value_2, _value_3);

   linked_list_iterator_free(iterator);
   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_iterator_remove_middle)
{
   linked_list_t *list;
   linked_list_iterator_t *iterator;

   list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   iterator = linked_list_iterator(list, true);
   iterator = linked_list_iterator_next(iterator);
   iterator = linked_list_iterator_remove(iterator);

   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(linked_list_iterator_value(iterator), _value_3);
   _verify_list(list, 2, _value_1, _value_3);

   linked_list_iterator_free(iterator);
   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_iterator_remove_last)
{
   linked_list_t *list;
   linked_list_iterator_t *iterator;

   list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   iterator = linked_list_iterator(list, true);
   iterator = linked_list_iterator_next(iterator);
   iterator = linked_list_iterator_next(iterator);
   iterator = linked_list_iterator_remove(iterator);

   ck_assert_ptr_null(iterator);
   _verify_list(list, 2, _value_1, _value_2);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_iterator_free_null)
{
   linked_list_iterator_free(NULL);
}
END_TEST

static size_t _foreach_count;
static void _foreach_fn(size_t index, void *value)
{
   _foreach_count++;
}

START_TEST (test_linked_list_foreach_null_list)
{
   linked_list_foreach(NULL, _foreach_fn);
}
END_TEST

START_TEST (test_linked_list_foreach_null_fn)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   linked_list_foreach(list, NULL);

   linked_list_free(list, NULL);
}
END_TEST

START_TEST (test_linked_list_foreach_valid)
{
   linked_list_t *list = linked_list_new();
   linked_list_add(list, _value_1);
   linked_list_add(list, _value_2);
   linked_list_add(list, _value_3);

   _foreach_count = 0;
   linked_list_foreach(list, &_foreach_fn);
   ck_assert_uint_eq(3, _foreach_count);

   linked_list_free(list, NULL);
}

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_linked_list_create);
   tcase_add_test(tc_core, test_linked_list_free);
   tcase_add_test(tc_core, test_linked_list_free_with_fn);
   tcase_add_test(tc_core, test_linked_list_add);
   tcase_add_test(tc_core, test_linked_list_insert_empty);
   tcase_add_test(tc_core, test_linked_list_insert_first);
   tcase_add_test(tc_core, test_linked_list_insert_middle);
   tcase_add_test(tc_core, test_linked_list_insert_last);
   tcase_add_test(tc_core, test_linked_list_insert_invalid);
   tcase_add_test(tc_core, test_linked_list_insert_null);
   tcase_add_test(tc_core, test_linked_list_get_invalid);
   tcase_add_test(tc_core, test_linked_list_get_null);
   tcase_add_test(tc_core, test_linked_list_get_first_matching_null);
   tcase_add_test(tc_core, test_linked_list_get_first_matching_function_null);
   tcase_add_test(tc_core, test_linked_list_get_first_matching_no_match);
   tcase_add_test(tc_core, test_linked_list_get_first_matching_with_match);
   tcase_add_test(tc_core, test_linked_list_get_last_matching_null);
   tcase_add_test(tc_core, test_linked_list_get_last_matching_function_null);
   tcase_add_test(tc_core, test_linked_list_get_last_matching_no_match);
   tcase_add_test(tc_core, test_linked_list_get_last_matching_with_match);
   tcase_add_test(tc_core, test_linked_list_remove_at_null);
   tcase_add_test(tc_core, test_linked_list_remove_at_empty);
   tcase_add_test(tc_core, test_linked_list_remove_at_invalid);
   tcase_add_test(tc_core, test_linked_list_remove_at_first);
   tcase_add_test(tc_core, test_linked_list_remove_at_middle);
   tcase_add_test(tc_core, test_linked_list_remove_at_last);
   tcase_add_test(tc_core, test_linked_list_remove_at_only);
   tcase_add_test(tc_core, test_linked_list_remove_first_null);
   tcase_add_test(tc_core, test_linked_list_remove_first_empty);
   tcase_add_test(tc_core, test_linked_list_remove_first_not_found);
   tcase_add_test(tc_core, test_linked_list_remove_first_first);
   tcase_add_test(tc_core, test_linked_list_remove_first_middle);
   tcase_add_test(tc_core, test_linked_list_remove_first_last);
   tcase_add_test(tc_core, test_linked_list_remove_first_only);
   tcase_add_test(tc_core, test_linked_list_remove_first_multiple);
   tcase_add_test(tc_core, test_linked_list_remove_last_null);
   tcase_add_test(tc_core, test_linked_list_remove_last_empty);
   tcase_add_test(tc_core, test_linked_list_remove_last_not_found);
   tcase_add_test(tc_core, test_linked_list_remove_last_first);
   tcase_add_test(tc_core, test_linked_list_remove_last_middle);
   tcase_add_test(tc_core, test_linked_list_remove_last_last);
   tcase_add_test(tc_core, test_linked_list_remove_last_only);
   tcase_add_test(tc_core, test_linked_list_remove_last_multiple);
   tcase_add_test(tc_core, test_linked_list_remove_all_null);
   tcase_add_test(tc_core, test_linked_list_remove_all_empty);
   tcase_add_test(tc_core, test_linked_list_remove_all_not_found);
   tcase_add_test(tc_core, test_linked_list_remove_all_first);
   tcase_add_test(tc_core, test_linked_list_remove_all_middle);
   tcase_add_test(tc_core, test_linked_list_remove_all_last);
   tcase_add_test(tc_core, test_linked_list_remove_all_only);
   tcase_add_test(tc_core, test_linked_list_remove_all_multiple);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_null);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_empty);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_not_found);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_first);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_middle);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_last);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_only);
   tcase_add_test(tc_core, test_linked_list_remove_first_matching_multiple);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_null);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_empty);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_not_found);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_first);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_middle);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_last);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_only);
   tcase_add_test(tc_core, test_linked_list_remove_last_matching_multiple);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_null);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_empty);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_not_found);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_first);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_middle);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_last);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_only);
   tcase_add_test(tc_core, test_linked_list_remove_all_matching_multiple);
   tcase_add_test(tc_core, test_linked_list_set_at_null);
   tcase_add_test(tc_core, test_linked_list_set_at_empty);
   tcase_add_test(tc_core, test_linked_list_set_at_invalid);
   tcase_add_test(tc_core, test_linked_list_set_at_first);
   tcase_add_test(tc_core, test_linked_list_set_at_middle);
   tcase_add_test(tc_core, test_linked_list_set_at_last);
   tcase_add_test(tc_core, test_linked_list_iterator_remove_null);
   tcase_add_test(tc_core, test_linked_list_iterator_remove_first);
   tcase_add_test(tc_core, test_linked_list_iterator_remove_middle);
   tcase_add_test(tc_core, test_linked_list_iterator_remove_last);
   tcase_add_test(tc_core, test_linked_list_iterator_free_null);
   tcase_add_test(tc_core, test_linked_list_foreach_null_list);
   tcase_add_test(tc_core, test_linked_list_foreach_null_fn);
   tcase_add_test(tc_core, test_linked_list_foreach_valid);
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
