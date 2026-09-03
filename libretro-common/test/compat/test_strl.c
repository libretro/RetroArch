/* Tests for strlcpy_lit(), the literal-source form of strlcpy().
 *
 * strlcpy() opens with strlen() of its source, and because it lives in
 * another translation unit that scan happens at runtime even when the
 * source is a literal the compiler has already measured. strlcpy_lit()
 * hands the length over as sizeof(), so the copy folds to stores.
 *
 * The property that matters is that it is a drop-in: for every literal
 * length against every destination size, the bytes written, the
 * terminator and the return value all have to match what strlcpy()
 * would have produced -- including when the literal does not fit, where
 * dropping the bound would turn a truncation into an overrun. The
 * exhaustive case below pins that across the whole small-size matrix
 * rather than at a few hand-picked points.
 */

#include <check.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>

#define SUITE_NAME "strl"

/* Compares strlcpy_lit() against strlcpy() for one literal, over every
 * destination size from 0 up past the literal's length. The buffer is
 * poisoned first so an unwritten tail is visible, and the full buffer
 * is compared so an overrun past the terminator is caught. */
#define CHECK_LIT(lit)                                                 \
   do {                                                               \
      size_t sz;                                                      \
      for (sz = 0; sz <= sizeof("" lit) + 4; sz++)                    \
      {                                                               \
         char a[64], b[64];                                           \
         size_t ra, rb;                                               \
         memset(a, '\xA5', sizeof(a));                                \
         memset(b, '\xA5', sizeof(b));                                \
         ra = strlcpy(a, "" lit, sz);                                 \
         rb = strlcpy_lit(b, lit, sz);                                \
         ck_assert_uint_eq(ra, rb);                                   \
         ck_assert_mem_eq(a, b, sizeof(a));                           \
      }                                                               \
   } while (0)

START_TEST (test_lit_matches_strlcpy)
{
   CHECK_LIT("");
   CHECK_LIT("a");
   CHECK_LIT("ab");
   CHECK_LIT("core_options");
   CHECK_LIT("input_player1_analog_dpad_mode");
   CHECK_LIT("/");
   CHECK_LIT(".lpl");
   /* Adjacent literals concatenate, so sizeof still measures the
    * characters and not a pointer. */
   CHECK_LIT("smb:" "//");
   /* Embedded escapes must not confuse sizeof. */
   CHECK_LIT("a\tb\\c");
}
END_TEST

START_TEST (test_lit_return_is_source_length)
{
   char s[8];
   /* Fits. */
   ck_assert_uint_eq(strlcpy_lit(s, "abc", sizeof(s)), 3);
   ck_assert_str_eq(s, "abc");
   /* Exactly fits: 7 characters plus the terminator. */
   ck_assert_uint_eq(strlcpy_lit(s, "abcdefg", sizeof(s)), 7);
   ck_assert_str_eq(s, "abcdefg");
   /* Does not fit: truncates and reports the source length, as
    * strlcpy() does. */
   ck_assert_uint_eq(strlcpy_lit(s, "abcdefgh", sizeof(s)), 8);
   ck_assert_str_eq(s, "abcdefg");
}
END_TEST

START_TEST (test_lit_runtime_size)
{
   char s[16];
   /* A size the compiler cannot fold, which is what a 'len' parameter
    * looks like from inside a function. */
   size_t n = 4;
   memset(s, '\xA5', sizeof(s));
   ck_assert_uint_eq(strlcpy_lit(s, "abcdefgh", n), 8);
   ck_assert_str_eq(s, "abc");
   ck_assert_int_eq(s[4], '\xA5');
}
END_TEST

START_TEST (test_lit_zero_size_writes_nothing)
{
   char s[4];
   memset(s, '\xA5', sizeof(s));
   ck_assert_uint_eq(strlcpy_lit(s, "abc", 0), strlcpy(s, "abc", 0));
   ck_assert_int_eq(s[0], '\xA5');
}
END_TEST

Suite *create_suite(void)
{
   Suite *s       = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_lit_matches_strlcpy);
   tcase_add_test(tc_core, test_lit_return_is_source_length);
   tcase_add_test(tc_core, test_lit_runtime_size);
   tcase_add_test(tc_core, test_lit_zero_size_writes_nothing);
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
