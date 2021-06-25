/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_generic_queue.c).
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

#include <queues/generic_queue.h>

#define SUITE_NAME "Generic Queue"

static char *_value_1 = "value1";
static char *_value_2 = "value2";
static char *_value_3 = "value3";

START_TEST (test_generic_queue_create)
{
   generic_queue_t *queue = generic_queue_new();
   ck_assert_ptr_nonnull(queue);
   generic_queue_free(queue, NULL);
}
END_TEST

START_TEST (test_generic_queue_free)
{
   generic_queue_t *queue = generic_queue_new();
   generic_queue_free(queue, NULL);
   generic_queue_free(NULL, NULL);
}
END_TEST

START_TEST (test_generic_queue_push_pop)
{
   generic_queue_t *queue;
   char *value;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   ck_assert_int_eq(generic_queue_length(queue), 1);
   value = (char *) generic_queue_pop(queue);
   ck_assert_ptr_eq(value, _value_1);
   ck_assert_int_eq(generic_queue_length(queue), 0);

   generic_queue_push(queue, _value_2);
   ck_assert_int_eq(generic_queue_length(queue), 1);
   generic_queue_push(queue, _value_3);
   ck_assert_int_eq(generic_queue_length(queue), 2);
   value = (char *) generic_queue_pop(queue);
   ck_assert_ptr_eq(value, _value_3);
   ck_assert_int_eq(generic_queue_length(queue), 1);
   value = (char *) generic_queue_pop(queue);
   ck_assert_ptr_eq(value, _value_2);
   ck_assert_int_eq(generic_queue_length(queue), 0);

   generic_queue_free(queue, NULL);
}
END_TEST

START_TEST (test_generic_queue_peek)
{
   generic_queue_t *queue;

   queue = generic_queue_new();
   ck_assert_ptr_null(generic_queue_peek(queue));
   ck_assert_ptr_null(generic_queue_peek_first(queue));

   generic_queue_push(queue, _value_1);
   ck_assert_ptr_eq(_value_1, generic_queue_peek(queue));
   ck_assert_ptr_eq(_value_1, generic_queue_peek_first(queue));

   generic_queue_push(queue, _value_2);
   ck_assert_ptr_eq(_value_2, generic_queue_peek(queue));
   ck_assert_ptr_eq(_value_1, generic_queue_peek_first(queue));

   generic_queue_push(queue, _value_3);
   ck_assert_ptr_eq(_value_3, generic_queue_peek(queue));
   ck_assert_ptr_eq(_value_1, generic_queue_peek_first(queue));

   generic_queue_free(queue, NULL);
}
END_TEST

START_TEST (test_generic_queue_shift_unshift)
{
   generic_queue_t *queue;
   char *value;

   queue = generic_queue_new();
   generic_queue_shift(queue, _value_1);
   ck_assert_int_eq(generic_queue_length(queue), 1);
   value = (char *) generic_queue_unshift(queue);
   ck_assert_ptr_eq(value, _value_1);
   ck_assert_int_eq(generic_queue_length(queue), 0);

   generic_queue_shift(queue, _value_2);
   ck_assert_int_eq(generic_queue_length(queue), 1);
   generic_queue_shift(queue, _value_3);
   ck_assert_int_eq(generic_queue_length(queue), 2);
   value = (char *) generic_queue_unshift(queue);
   ck_assert_ptr_eq(value, _value_3);
   ck_assert_int_eq(generic_queue_length(queue), 1);
   value = (char *) generic_queue_unshift(queue);
   ck_assert_ptr_eq(value, _value_2);
   ck_assert_int_eq(generic_queue_length(queue), 0);

   generic_queue_free(queue, NULL);
}
END_TEST

START_TEST (test_generic_queue_empty)
{
   generic_queue_t *queue;

   queue = generic_queue_new();
   ck_assert_ptr_null(generic_queue_pop(queue));
   ck_assert_ptr_null(generic_queue_unshift(queue));
   generic_queue_free(queue, NULL);
}
END_TEST

void _free_value(void *value)
{
   return;
}

START_TEST (test_generic_queue_iterator)
{
   generic_queue_t *queue;
   generic_queue_iterator_t *iterator;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   generic_queue_push(queue, _value_2);
   generic_queue_push(queue, _value_3);

   iterator = generic_queue_iterator(queue, true);
   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(generic_queue_iterator_value(iterator), _value_1);
   iterator = generic_queue_iterator_next(iterator);
   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(generic_queue_iterator_value(iterator), _value_2);
   iterator = generic_queue_iterator_next(iterator);
   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(generic_queue_iterator_value(iterator), _value_3);
   iterator = generic_queue_iterator_next(iterator);
   ck_assert_ptr_null(iterator);

   iterator = generic_queue_iterator(queue, false);
   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(generic_queue_iterator_value(iterator), _value_3);
   iterator = generic_queue_iterator_next(iterator);
   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(generic_queue_iterator_value(iterator), _value_2);
   iterator = generic_queue_iterator_next(iterator);
   ck_assert_ptr_nonnull(iterator);
   ck_assert_ptr_eq(generic_queue_iterator_value(iterator), _value_1);
   iterator = generic_queue_iterator_next(iterator);
   ck_assert_ptr_null(iterator);

   generic_queue_free(queue, &_free_value);
}
END_TEST

START_TEST (test_generic_queue_shift_free)
{
   generic_queue_t *queue;

   queue = generic_queue_new();

   generic_queue_shift(queue, _value_1);
   generic_queue_shift(queue, _value_2);
   generic_queue_shift(queue, _value_3);

   generic_queue_free(queue, &_free_value);
}
END_TEST

START_TEST (test_generic_queue_remove_one)
{
   generic_queue_t *queue;
   generic_queue_iterator_t *iterator;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);

   iterator = generic_queue_iterator(queue, true);
   iterator = generic_queue_iterator_remove(iterator);
   ck_assert_ptr_null(iterator);
   ck_assert_int_eq(generic_queue_length(queue), 0);

   generic_queue_free(queue, NULL);
}
END_TEST

static void _verify_queue_values(generic_queue_t *queue, int count, ...)
{
   va_list values_list;
   void **values;
   int i;
   generic_queue_iterator_t *iterator;

   values = (void **)malloc(count * sizeof(void *));

   ck_assert_int_eq(count, generic_queue_length(queue));

   va_start(values_list, count);
   for (i = 0; i < count; i++)
      values[i] = va_arg(values_list, void *);
   va_end(values_list);

   iterator = generic_queue_iterator(queue, true);
   for (i = 0; i < count; i++)
   {
      ck_assert_ptr_nonnull(iterator);
      ck_assert_ptr_eq(values[i], generic_queue_iterator_value(iterator));
      iterator = generic_queue_iterator_next(iterator);
   }
   ck_assert_ptr_null(iterator);

   iterator = generic_queue_iterator(queue, false);
   for (i = count - 1; i >= 0; i--)
   {
      ck_assert_ptr_nonnull(iterator);
      ck_assert_ptr_eq(values[i], generic_queue_iterator_value(iterator));
      iterator = generic_queue_iterator_next(iterator);
   }
   ck_assert_ptr_null(iterator);

   free(values);
}

START_TEST (test_generic_queue_iterator_remove_first)
{
   generic_queue_t *queue;
   generic_queue_iterator_t *iterator;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   generic_queue_push(queue, _value_2);
   generic_queue_push(queue, _value_3);

   iterator = generic_queue_iterator(queue, true);
   iterator = generic_queue_iterator_remove(iterator);
   generic_queue_iterator_free(iterator);

   _verify_queue_values(queue, 2, _value_2, _value_3);

   generic_queue_free(queue, &_free_value);
}
END_TEST

START_TEST (test_generic_queue_iterator_remove_middle)
{
   generic_queue_t *queue;
   generic_queue_iterator_t *iterator;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   generic_queue_push(queue, _value_2);
   generic_queue_push(queue, _value_3);

   iterator = generic_queue_iterator(queue, true);
   iterator = generic_queue_iterator_next(iterator);
   iterator = generic_queue_iterator_remove(iterator);
   generic_queue_iterator_free(iterator);

   _verify_queue_values(queue, 2, _value_1, _value_3);

   generic_queue_free(queue, &_free_value);
}
END_TEST

START_TEST (test_generic_queue_iterator_remove_last)
{
   generic_queue_t *queue;
   generic_queue_iterator_t *iterator;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   generic_queue_push(queue, _value_2);
   generic_queue_push(queue, _value_3);

   iterator = generic_queue_iterator(queue, false);
   iterator = generic_queue_iterator_remove(iterator);
   generic_queue_iterator_free(iterator);

   _verify_queue_values(queue, 2, _value_1, _value_2);

   generic_queue_free(queue, &_free_value);
}
END_TEST

START_TEST (test_generic_queue_remove_first)
{
   generic_queue_t *queue;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   generic_queue_push(queue, _value_2);
   generic_queue_push(queue, _value_3);

   ck_assert_ptr_eq(generic_queue_remove(queue, _value_1), _value_1);

   _verify_queue_values(queue, 2, _value_2, _value_3);

   generic_queue_free(queue, &_free_value);
}

START_TEST (test_generic_queue_remove_middle)
{
   generic_queue_t *queue;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   generic_queue_push(queue, _value_2);
   generic_queue_push(queue, _value_3);

   ck_assert_ptr_eq(generic_queue_remove(queue, _value_2), _value_2);

   _verify_queue_values(queue, 2, _value_1, _value_3);

   generic_queue_free(queue, &_free_value);
}

START_TEST (test_generic_queue_remove_last)
{
   generic_queue_t *queue;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   generic_queue_push(queue, _value_2);
   generic_queue_push(queue, _value_3);

   ck_assert_ptr_eq(generic_queue_remove(queue, _value_3), _value_3);

   _verify_queue_values(queue, 2, _value_1, _value_2);

   generic_queue_free(queue, &_free_value);
}

START_TEST (test_generic_queue_iterator_free)
{
   generic_queue_t *queue;
   generic_queue_iterator_t *iterator;

   queue = generic_queue_new();
   generic_queue_push(queue, _value_1);
   iterator = generic_queue_iterator(queue, true);

   generic_queue_iterator_free(iterator);
   generic_queue_iterator_free(NULL);

   generic_queue_free(queue, _free_value);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_generic_queue_create);
   tcase_add_test(tc_core, test_generic_queue_free);
   tcase_add_test(tc_core, test_generic_queue_push_pop);
   tcase_add_test(tc_core, test_generic_queue_peek);
   tcase_add_test(tc_core, test_generic_queue_shift_unshift);
   tcase_add_test(tc_core, test_generic_queue_empty);
   tcase_add_test(tc_core, test_generic_queue_iterator);
   tcase_add_test(tc_core, test_generic_queue_shift_free);
   tcase_add_test(tc_core, test_generic_queue_remove_one);
   tcase_add_test(tc_core, test_generic_queue_iterator_remove_first);
   tcase_add_test(tc_core, test_generic_queue_iterator_remove_middle);
   tcase_add_test(tc_core, test_generic_queue_iterator_remove_last);
   tcase_add_test(tc_core, test_generic_queue_remove_first);
   tcase_add_test(tc_core, test_generic_queue_remove_middle);
   tcase_add_test(tc_core, test_generic_queue_remove_last);
   tcase_add_test(tc_core, test_generic_queue_iterator_free);
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
