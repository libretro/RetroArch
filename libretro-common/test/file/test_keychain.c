/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_keychain.c).
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

#ifdef CHECK_SHIM
#include "../check_shim.h"
#else
#include <check.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <file/keychain.h>

#define SUITE_NAME "keychain"

static char keyfile[512];

static void setup(void)
{
   /* the platform's temp dir: Windows has no /tmp, and TMP/TEMP is
    * what a Windows build (run natively or under Wine) can write */
   const char *tmp = getenv("TMP");
   if (!tmp || !*tmp)
      tmp = getenv("TEMP");
   if (!tmp || !*tmp)
      tmp = "/tmp";
   snprintf(keyfile, sizeof(keyfile), "%s/rakc_test_%ld.key", tmp, (long)getpid());
   unlink(keyfile);
   keychain_deinit();
}

static void teardown(void)
{
   keychain_deinit();
   unlink(keyfile);
}

START_TEST (test_roundtrip)
{
   char *sealed, *sealed2, *opened;

   ck_assert(!keychain_is_ready());
   ck_assert_ptr_null(keychain_seal_alloc("cheevos_password", "hunter2"));
   ck_assert(keychain_init(keyfile));
   ck_assert(keychain_is_ready());

   sealed = keychain_seal_alloc("cheevos_password", "hunter2");
   ck_assert_ptr_nonnull(sealed);
   ck_assert(keychain_value_is_sealed(sealed));
   ck_assert(strstr(sealed, "hunter2") == NULL);
   /* config-file safe: no whitespace or quotes */
   ck_assert(strpbrk(sealed, " \t\r\n\"") == NULL);

   /* fresh nonce every time */
   sealed2 = keychain_seal_alloc("cheevos_password", "hunter2");
   ck_assert_ptr_nonnull(sealed2);
   ck_assert_str_ne(sealed, sealed2);

   opened = keychain_open_alloc("cheevos_password", sealed);
   ck_assert_ptr_nonnull(opened);
   ck_assert_str_eq(opened, "hunter2");
   free(opened);
   opened = keychain_open_alloc("cheevos_password", sealed2);
   ck_assert_ptr_nonnull(opened);
   ck_assert_str_eq(opened, "hunter2");
   free(opened);

   /* bound to the setting name */
   ck_assert_ptr_null(keychain_open_alloc("cheevos_token", sealed));

   /* tamper: swap the third-last character (all six of its bits are
    * data) for a different alphabet character, so the decoded octets
    * always change; flipping a bit could turn 'A' into '@', which the
    * decoder does not reject and reads as 'A' again */
   sealed[strlen(sealed) - 3] = sealed[strlen(sealed) - 3] == 'A' ? 'B' : 'A';
   ck_assert_ptr_null(keychain_open_alloc("cheevos_password", sealed));
   free(sealed);
   free(sealed2);

   /* empty and long values */
   sealed = keychain_seal_alloc("netplay_password", "");
   ck_assert_ptr_nonnull(sealed);
   opened = keychain_open_alloc("netplay_password", sealed);
   ck_assert_ptr_nonnull(opened);
   ck_assert_str_eq(opened, "");
   free(opened);
   free(sealed);
   {
      char big[3000];
      memset(big, 'x', sizeof(big) - 1);
      big[sizeof(big) - 1] = '\0';
      sealed = keychain_seal_alloc("twitch_stream_key", big);
      ck_assert_ptr_nonnull(sealed);
      opened = keychain_open_alloc("twitch_stream_key", sealed);
      ck_assert_ptr_nonnull(opened);
      ck_assert_str_eq(opened, big);
      free(opened);
      free(sealed);
   }

   /* unmigrated plaintext passes through */
   ck_assert(!keychain_value_is_sealed("plain"));
   opened = keychain_open_alloc("cheevos_password", "plain");
   ck_assert_ptr_nonnull(opened);
   ck_assert_str_eq(opened, "plain");
   free(opened);
   ck_assert_ptr_null(keychain_open_alloc("cheevos_password", "$kc1$"));
   ck_assert_ptr_null(keychain_open_alloc("cheevos_password", "$kc1$!!!!"));
}
END_TEST

START_TEST (test_keyfile_persistence)
{
   char *sealed, *opened;

   ck_assert(keychain_init(keyfile));
   sealed = keychain_seal_alloc("webdav_password", "s3cret");
   ck_assert_ptr_nonnull(sealed);

   /* same key file, new process: still opens */
   keychain_deinit();
   ck_assert(!keychain_is_ready());
   ck_assert_ptr_null(keychain_open_alloc("webdav_password", sealed));
   ck_assert(keychain_init(keyfile));
   opened = keychain_open_alloc("webdav_password", sealed);
   ck_assert_ptr_nonnull(opened);
   ck_assert_str_eq(opened, "s3cret");
   free(opened);

   /* key file gone: a new key is made and the old blob is opaque */
   keychain_deinit();
   unlink(keyfile);
   ck_assert(keychain_init(keyfile));
   ck_assert_ptr_null(keychain_open_alloc("webdav_password", sealed));
   free(sealed);

   /* a corrupt key file is refused rather than overwritten */
   keychain_deinit();
   {
      FILE *f = fopen(keyfile, "wb");
      ck_assert_ptr_nonnull(f);
      fputs("not hex at all, but definitely long enough to be read as a key file...", f);
      fclose(f);
   }
   ck_assert(!keychain_init(keyfile));
   ck_assert(!keychain_is_ready());
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");
   tcase_add_checked_fixture(tc_core, setup, teardown);
   tcase_add_test(tc_core, test_roundtrip);
   tcase_add_test(tc_core, test_keyfile_persistence);
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
