/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (check_shim.h).
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

#ifndef _LIBRETRO_TEST_CHECK_SHIM_H
#define _LIBRETRO_TEST_CHECK_SHIM_H

/* Just enough of libcheck's API for a suite to build and run where
 * the real library is not installed for the target - the aarch64
 * cross run under qemu, which is how the ARM crypto paths get
 * executed on an x86 runner. A failed assertion prints and exits
 * non-zero rather than continuing with the next test. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CK_NORMAL 0
typedef struct { const char *name; int fails; } Suite;
typedef struct { const char *name; void (*fn[64])(void); int n; void (*setup)(void); void (*teardown)(void); } TCase;
typedef struct { Suite *s; } SRunner;

#define START_TEST(name) static void name(void) {
#define END_TEST }

#define ck_fail_here(msg) \
   do { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, msg); exit(1); } while (0)
#define ck_assert(expr)           do { if (!(expr)) ck_fail_here("assertion failed: " #expr); } while (0)
#define ck_assert_int_eq(a, b)    do { if ((long)(a) != (long)(b)) ck_fail_here(#a " == " #b); } while (0)
#define ck_assert_uint_eq(a, b)   do { if ((unsigned long)(a) != (unsigned long)(b)) ck_fail_here(#a " == " #b); } while (0)
#define ck_assert_ptr_nonnull(p)  do { if (!(p)) ck_fail_here(#p " != NULL"); } while (0)
#define ck_assert_ptr_null(p)     do { if ((p)) ck_fail_here(#p " == NULL"); } while (0)
#define ck_assert_str_eq(a, b)    do { if (strcmp((a), (b)) != 0) ck_fail_here(#a " == " #b); } while (0)
#define ck_assert_str_ne(a, b)    do { if (strcmp((a), (b)) == 0) ck_fail_here(#a " != " #b); } while (0)

static Suite shim_suite;
static TCase shim_tcase;
static SRunner shim_runner;

static Suite *suite_create(const char *name) { shim_suite.name = name; shim_suite.fails = 0; return &shim_suite; }
static TCase *tcase_create(const char *name) { shim_tcase.name = name; shim_tcase.n = 0; shim_tcase.setup = NULL; shim_tcase.teardown = NULL; return &shim_tcase; }
#define tcase_set_timeout(tc, t) ((void)(tc), (void)(t))
static void tcase_add_test_fn(TCase *tc, void (*fn)(void)) { if (tc->n < 64) tc->fn[tc->n++] = fn; }
#define tcase_add_test(tc, fn) tcase_add_test_fn(tc, fn)
/* Checked fixtures run around every test, as in libcheck. */
#define tcase_add_checked_fixture(tc, s, t) ((tc)->setup = (s), (tc)->teardown = (t))
static void suite_add_tcase(Suite *s, TCase *tc) { (void)s; (void)tc; }
static SRunner *srunner_create(Suite *s) { shim_runner.s = s; return &shim_runner; }
static void srunner_run_all(SRunner *sr, int mode)
{
   int i;
   (void)mode;
   printf("Running suite(s): %s (check shim)\n", sr->s->name);
   for (i = 0; i < shim_tcase.n; i++)
   {
      if (shim_tcase.setup)
         shim_tcase.setup();
      shim_tcase.fn[i]();
      if (shim_tcase.teardown)
         shim_tcase.teardown();
   }
   printf("100%%: Checks: %d, Failures: 0, Errors: 0\n", shim_tcase.n);
}
static int srunner_ntests_failed(SRunner *sr) { return sr->s->fails; }
static void srunner_free(SRunner *sr) { (void)sr; }

#endif
