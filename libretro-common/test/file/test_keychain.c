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
#include <crypto/kdf.h>

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

/* The key file as text, or "" */
static void read_keyfile(char *out, size_t len)
{
   FILE  *f = fopen(keyfile, "rb");
   size_t n = 0;
   out[0] = '\0';
   if (!f)
      return;
   n = fread(out, 1, len - 1, f);
   out[n] = '\0';
   fclose(f);
}

static void write_keyfile(const char *text)
{
   FILE *f = fopen(keyfile, "wb");
   ck_assert(f != NULL);
   fwrite(text, 1, strlen(text), f);
   fclose(f);
}

/* Replace the machine line with one wrapped for some other machine,
 * which is what a key file carried to another machine looks like. */
static void make_foreign(void)
{
   char  text[1024];
   char  out[1200];
   char *m, *eol;
   read_keyfile(text, sizeof(text));
   m = strstr(text, "machine ");
   ck_assert(m != NULL);
   eol = strchr(m, '\n');
   ck_assert(eol != NULL);
   *m = '\0';
   /* 60 octets of base64 that no machine key opens */
   snprintf(out, sizeof(out), "%smachine %s%s", text,
         "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7",
         eol);
   write_keyfile(out);
}

START_TEST (test_no_passphrase_file_unchanged)
{
   char text[1024];
   ck_assert(keychain_init(keyfile));
   ck_assert(!keychain_is_locked());
   ck_assert(!keychain_has_passphrase());
   read_keyfile(text, sizeof(text));
   /* the salt alone, exactly as before passphrases existed */
   ck_assert_int_eq((int)strlen(text), 65);
   ck_assert(strchr(text, ' ') == NULL);
}
END_TEST

START_TEST (test_passphrase_moves_keychain)
{
   char *sealed, *plain;
   ck_assert(keychain_init(keyfile));
   sealed = keychain_seal_alloc("cheevos_password", "hunter2");
   ck_assert(sealed != NULL);
   ck_assert(keychain_set_passphrase("correct horse"));
   ck_assert(keychain_has_passphrase());

   /* the same machine still opens it by itself */
   keychain_deinit();
   ck_assert(keychain_init(keyfile));
   ck_assert(!keychain_is_locked());
   plain = keychain_open_alloc("cheevos_password", sealed);
   ck_assert_str_eq(plain, "hunter2");
   free(plain);

   /* another machine: locked, nothing opens, nothing unlocks wrongly */
   keychain_deinit();
   make_foreign();
   ck_assert(!keychain_init(keyfile));
   ck_assert(keychain_is_locked());
   ck_assert(!keychain_is_ready());
   ck_assert(keychain_open_alloc("cheevos_password", sealed) == NULL);
   ck_assert(keychain_seal_alloc("cheevos_password", "x") == NULL);
   ck_assert(!keychain_unlock("wrong horse"));
   ck_assert(keychain_is_locked());

   /* the passphrase opens it, and the values sealed elsewhere */
   ck_assert(keychain_unlock("correct horse"));
   ck_assert(keychain_is_ready());
   ck_assert(!keychain_is_locked());
   plain = keychain_open_alloc("cheevos_password", sealed);
   ck_assert_str_eq(plain, "hunter2");
   free(plain);

   /* ...and it is wrapped for this machine now: opens by itself */
   keychain_deinit();
   ck_assert(keychain_init(keyfile));
   ck_assert(!keychain_is_locked());
   ck_assert(keychain_has_passphrase());
   plain = keychain_open_alloc("cheevos_password", sealed);
   ck_assert_str_eq(plain, "hunter2");
   free(plain);
   free(sealed);
}
END_TEST

START_TEST (test_passphrase_clear)
{
   char  text[1024];
   char *sealed, *plain;
   ck_assert(keychain_init(keyfile));
   sealed = keychain_seal_alloc("netplay_password", "p4ss");
   ck_assert(keychain_set_passphrase("pw"));
   ck_assert(keychain_set_passphrase(NULL));
   ck_assert(!keychain_has_passphrase());
   read_keyfile(text, sizeof(text));
   /* back to the plain salt: the key is this machine's own again */
   ck_assert_int_eq((int)strlen(text), 65);
   keychain_deinit();
   ck_assert(keychain_init(keyfile));
   plain = keychain_open_alloc("netplay_password", sealed);
   ck_assert_str_eq(plain, "p4ss");
   free(plain);
   free(sealed);
}
END_TEST

START_TEST (test_passphrase_change)
{
   char *sealed, *plain;
   ck_assert(keychain_init(keyfile));
   sealed = keychain_seal_alloc("webdav_password", "dav");
   ck_assert(keychain_set_passphrase("first"));
   ck_assert(keychain_set_passphrase("second"));
   keychain_deinit();
   make_foreign();
   ck_assert(!keychain_init(keyfile));
   ck_assert(!keychain_unlock("first"));
   ck_assert(keychain_unlock("second"));
   plain = keychain_open_alloc("webdav_password", sealed);
   ck_assert_str_eq(plain, "dav");
   free(plain);
   free(sealed);
}
END_TEST

START_TEST (test_kdf_resumable_matches_pbkdf2)
{
   static const uint8_t psalt[KEYCHAIN_PASS_SALT_SIZE] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
   static const uint32_t slices[] = { 1, 7, 1000, 5000 };
   uint8_t  want[32], got[32];
   unsigned i;
   ck_assert(pbkdf2_hmac_sha256((const uint8_t*)"pass phrase", 11,
         psalt, sizeof(psalt), 5003, want, sizeof(want)) == 0);
   for (i = 0; i < sizeof(slices) / sizeof(slices[0]); i++)
   {
      struct keychain_kdf *k = keychain_kdf_begin("pass phrase", psalt, 5003);
      unsigned last = 0;
      ck_assert(k != NULL);
      while (!keychain_kdf_step(k, slices[i]))
      {
         ck_assert(keychain_kdf_progress(k) >= last);
         last = keychain_kdf_progress(k);
      }
      ck_assert_int_eq((int)keychain_kdf_progress(k), 100);
      keychain_kdf_end(k, got);
      ck_assert(memcmp(got, want, sizeof(want)) == 0);
   }
   ck_assert(keychain_passphrase_derive("pass phrase", psalt, 5003, got));
   ck_assert(memcmp(got, want, sizeof(want)) == 0);
   ck_assert(keychain_kdf_begin("", psalt, 5003) == NULL);
   ck_assert(keychain_kdf_begin("x", psalt, 0) == NULL);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);
   TCase *tc_core = tcase_create("Core");
   tcase_add_checked_fixture(tc_core, setup, teardown);
   /* a passphrase is 200000 PBKDF2 rounds, slow under the sanitizers */
   tcase_set_timeout(tc_core, 120);
   tcase_add_test(tc_core, test_roundtrip);
   tcase_add_test(tc_core, test_keyfile_persistence);
   tcase_add_test(tc_core, test_no_passphrase_file_unchanged);
   tcase_add_test(tc_core, test_passphrase_moves_keychain);
   tcase_add_test(tc_core, test_passphrase_clear);
   tcase_add_test(tc_core, test_passphrase_change);
   tcase_add_test(tc_core, test_kdf_resumable_matches_pbkdf2);
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
