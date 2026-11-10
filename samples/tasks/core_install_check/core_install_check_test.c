/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* A core staged for install (core_install_check.c): the check, its
 * .sig and the move into place. Linux and mingw-w64. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <streams/file_stream.h>

#include "../../../core_install_check.h"
#include "../core_signature/core_signature_fixtures.h"

#define TMP_DIR "core_install_check_test_tmp"

static int checks   = 0;
static int failures = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint8_t *fx_core;
static char cores[256];
static char staging[256];
static char installed[256];
static char staged[256];

/* core_trust_default_path's folder, here the test's own */
bool fill_pathname_application_data(char *s, size_t len)
{
   strlcpy(s, TMP_DIR, len);
   return true;
}

/* The bytes the fixtures' .sig files sign (core_signature_test.c) */
static void fx_make_core(void)
{
   unsigned long x = FX_CORE_SEED;
   size_t i;
   fx_core = (uint8_t*)malloc(FX_CORE_SIZE);
   for (i = 0; i < FX_CORE_SIZE; i++)
   {
      x          = (x * 1103515245UL + 12345UL) & 0xffffffffUL;
      fx_core[i] = (uint8_t)(x >> 24);
   }
}

static void sha(const uint8_t *data, size_t len, uint8_t *out)
{
   struct sha256_state st;
   sha256_stream_init(&st, 0);
   sha256_stream_update(&st, data, len);
   sha256_stream_final(&st, out);
}

static bool put(const char *path, const void *data, size_t len)
{
   return filestream_write_file(path, data, (int64_t)len);
}

static bool same(const char *path, const void *data, size_t len)
{
   void *buf = NULL;
   int64_t n = 0;
   bool ok;
   if (!filestream_read_file(path, &buf, &n))
      return false;
   ok = n == (int64_t)len && !memcmp(buf, data, len);
   free(buf);
   return ok;
}

static void sig_of(const char *path, char *s, size_t len)
{
   strlcpy(s, path, len);
   strlcat(s, ".sig", len);
}

/* The staged core and its .sig (none when sig is NULL) */
static void stage(const uint8_t *core, const uint8_t *sig, size_t sig_len)
{
   char p[300];
   put(staged, core, FX_CORE_SIZE);
   sig_of(staged, p, sizeof(p));
   if (path_is_valid(p))
      filestream_delete(p);
   if (sig)
      put(p, sig, sig_len);
}

/* As RetroArch loads it: records for the test's own files */
static core_trust_t *new_store(const char *name)
{
   char p[300];
   core_trust_t *t = core_trust_new();
   fill_pathname_join_special(p, TMP_DIR, name, sizeof(p));
   core_trust_load_file(t, p);
   return t;
}

/* The whole paced check, as a task runs it */
static enum core_check_result run(core_trust_t *t, unsigned flags,
      struct core_install_check *ic)
{
   core_trust_set_t *s = core_trust_get(t);
   if (core_install_check_begin(ic, s, staged, installed))
   {
      while (!core_install_check_step(ic))
         ;
      core_install_check_end(ic, s, staged, flags);
   }
   core_trust_set_put(s);
   return ic->c.result;
}

static void test_signed(void)
{
   struct core_install_check ic;
   uint8_t key[CORE_SHA256_LEN];
   core_trust_t *t = new_store("store_signed");
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   CHECK(ic.c.raise_newest);
   CHECK(ic.c.rec.build_time == FX_TIME);
   CHECK(!strcmp(ic.c.rec.name, FX_NAME));
   /* Newest builds are the install folder's, not staging's */
   CHECK(core_folder_key(installed, key)
         && !memcmp(key, ic.c.folder, sizeof(key)));
   core_trust_free(t);
}

static void test_refused(void)
{
   struct core_install_check ic;
   core_trust_t *t = new_store("store_refused");
   stage(fx_core, NULL, 0);
   CHECK(run(t, 0, &ic) == CORE_CHECK_UNSIGNED);
   stage(fx_core, fx_sig_key2, sizeof(fx_sig_key2));
   CHECK(run(t, 0, &ic) == CORE_CHECK_UNKNOWN_KEY);
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   CHECK(run(t, 0, &ic) == CORE_CHECK_MODIFIED);
   core_trust_free(t);
}

/* With the cores folder's newest build known, a staged older one is
 * OLDER, unless older is allowed (a restore of RetroArch's backup) */
static void test_older(void)
{
   struct core_install_check ic;
   struct core_check c;
   uint8_t digest[CORE_SHA256_LEN];
   size_t n            = 0;
   const uint8_t *keys = core_sig_builtin_keys(&n);
   core_trust_t *t     = new_store("store_older");
   core_trust_set_t *s = core_trust_get(t);
   memset(&c, 0, sizeof(c));
   strlcpy(c.rec.name, FX_NAME, sizeof(c.rec.name));
   CHECK(core_folder_key(installed, c.folder));
   sha(fx_core, FX_CORE_SIZE, digest);
   core_check_finish(s, keys, n, digest, fx_sig_good, sizeof(fx_sig_good),
         0, &c);
   core_trust_set_put(s);
   CHECK(c.result == CORE_CHECK_OK && c.raise_newest);
   CHECK(core_trust_apply(t, &c));
   stage(fx_core, fx_sig_older, sizeof(fx_sig_older));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OLDER);
   CHECK(run(t, CORE_CHECK_ALLOW_OLDER, &ic) == CORE_CHECK_OK);
   CHECK(ic.c.rec.flags & CORE_TRUST_RECORD_ANY_AGE);
   core_trust_free(t);
}

/* A record or the folder rule passes a file in place; a staged
 * download is hashed whatever they say */
static void test_no_quick_pass(void)
{
   struct core_install_check ic;
   struct core_check q;
   char p[300];
   core_trust_set_t *s;
   core_file_t *f;
   bool quick_ok;
   core_trust_t *t = new_store("store_quick");
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   CHECK(core_trust_apply(t, &ic.c));
   /* Same file, same identity: only its .sig goes */
   sig_of(staged, p, sizeof(p));
   filestream_delete(p);
   s        = core_trust_get(t);
   f        = core_file_open(staged);
   quick_ok = f && core_check_quick(s, f, staged, &q)
      && q.result == CORE_CHECK_OK;
   core_file_close(f);
   core_trust_set_put(s);
   if (!quick_ok)
      printf("note: no record or folder pass for %s here\n", staged);
   CHECK(run(t, 0, &ic) == CORE_CHECK_UNSIGNED);
   core_trust_free(t);
}

static void test_checks_off(void)
{
   struct core_install_check ic;
   core_trust_set_t *s;
   core_trust_t *t = new_store("store_off");
   CHECK(core_trust_set_flag(t, CORE_TRUST_CHECK_OFF, true));
   stage(fx_core, NULL, 0);
   s = core_trust_get(t);
   CHECK(!core_install_check_begin(&ic, s, staged, installed));
   core_trust_set_put(s);
   CHECK(ic.c.result == CORE_CHECK_OK);
   CHECK(!ic.c.record && !ic.c.raise_newest);
   core_trust_free(t);
}

static void test_error_and_free(void)
{
   struct core_install_check ic;
   char none[300];
   core_trust_t *t     = new_store("store_error");
   core_trust_set_t *s = core_trust_get(t);
   fill_pathname_join_special(none, staging, "none_libretro.so",
         sizeof(none));
   CHECK(!core_install_check_begin(&ic, s, none, installed));
   CHECK(ic.c.result == CORE_CHECK_ERROR);
   /* Cancelled part-way: free closes and releases (ASan sees leaks) */
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(core_install_check_begin(&ic, s, staged, installed));
   /* No result until the end: a cancel reads a failure */
   CHECK(ic.c.result == CORE_CHECK_ERROR);
   core_install_check_step(&ic);
   core_install_check_free(&ic);
   core_install_check_free(&ic);
   core_trust_set_put(s);
   core_trust_free(t);
}

static void test_commit(void)
{
   static const uint8_t old_sig[4] = { 1, 2, 3, 4 };
   struct core_install_check ic;
   struct core_identity id;
   char isig[300];
   char ssig[300];
   core_file_t *f;
   core_trust_t *t = new_store("store_commit");
   sig_of(installed, isig, sizeof(isig));
   sig_of(staged, ssig, sizeof(ssig));
   put(installed, "old", 3);
   put(isig, old_sig, sizeof(old_sig));
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   /* A rename may move ctime, or not before the clock ticks: as if it did */
   ic.c.rec.id.ctime_nsec ^= 1;
   CHECK(core_install_commit(staged, installed, &ic.c));
   CHECK(same(installed, fx_core, FX_CORE_SIZE));
   CHECK(same(isig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_valid(staged) && !path_is_valid(ssig));
   /* The identity is the installed file's, read after the rename */
   f = core_file_open(installed);
   CHECK(f && core_file_identity(f, &id)
         && core_identity_equal(&id, &ic.c.rec.id));
   core_file_close(f);
   CHECK(ic.c.record);
   core_trust_free(t);
}

/* Nothing staged beside it: the old core's .sig would call it changed */
static void test_commit_unsigned(void)
{
   static const uint8_t old_sig[4] = { 1, 2, 3, 4 };
   struct core_check c;
   char isig[300];
   sig_of(installed, isig, sizeof(isig));
   put(installed, "old", 3);
   put(isig, old_sig, sizeof(old_sig));
   stage(fx_core, NULL, 0);
   memset(&c, 0, sizeof(c));
   CHECK(core_install_commit(staged, installed, &c));
   CHECK(same(installed, fx_core, FX_CORE_SIZE));
   CHECK(!path_is_valid(isig));
}

/* The old .sig is set aside first: when the core can't move, the old
 * core keeps it */
static void test_commit_core_first(void)
{
   static const uint8_t old_sig[4] = { 1, 2, 3, 4 };
   static const uint8_t stray[2]   = { 9, 9 };
   struct core_install_check ic;
   char isig[300];
   char iold[300];
   core_trust_t *t = new_store("store_first");
   sig_of(installed, isig, sizeof(isig));
   strlcpy(iold, isig, sizeof(iold));
   strlcat(iold, ".old", sizeof(iold));
   put(installed, "old", 3);
   put(isig, old_sig, sizeof(old_sig));
   put(iold, stray, sizeof(stray));
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   /* Gone before the rename: it fails */
   filestream_delete(staged);
   CHECK(!core_install_commit(staged, installed, &ic.c));
   CHECK(same(installed, "old", 3));
   CHECK(same(isig, old_sig, sizeof(old_sig)));
   CHECK(!path_is_valid(iold));
   core_trust_free(t);
}

/* A .sig.old a crash left goes at the next install, and never comes
 * back as the core's .sig when that install fails */
static void test_commit_stray_old(void)
{
   static const uint8_t stray[2] = { 9, 9 };
   struct core_install_check ic;
   char isig[300];
   char iold[300];
   core_trust_t *t = new_store("store_stray");
   sig_of(installed, isig, sizeof(isig));
   strlcpy(iold, isig, sizeof(iold));
   strlcat(iold, ".old", sizeof(iold));
   put(installed, "old", 3);
   if (path_is_valid(isig))
      filestream_delete(isig);
   put(iold, stray, sizeof(stray));
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   filestream_delete(staged);
   CHECK(!core_install_commit(staged, installed, &ic.c));
   CHECK(same(installed, "old", 3));
   CHECK(!path_is_valid(isig) && !path_is_valid(iold));
   put(iold, stray, sizeof(stray));
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   CHECK(core_install_commit(staged, installed, &ic.c));
   CHECK(same(installed, fx_core, FX_CORE_SIZE));
   CHECK(same(isig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(!path_is_valid(iold));
   core_trust_free(t);
}

/* Another file at the install path now: no record of it */
static void test_identify_other(void)
{
   struct core_check c;
   core_file_t *f;
   put(staged, "other", 5);
   memset(&c, 0, sizeof(c));
   f = core_file_open(staged);
   CHECK(f && core_file_identity(f, &c.rec.id));
   core_file_close(f);
   c.record = true;
   put(installed, fx_core, FX_CORE_SIZE);
   core_install_identify(installed, &c);
   CHECK(!c.record);
}

/* The same file, written in place since the check opened it (same
 * size): its mtime moved, so no record */
static void test_identify_mtime(void)
{
   struct core_check c;
   core_file_t *f;
   put(installed, fx_core, FX_CORE_SIZE);
   memset(&c, 0, sizeof(c));
   f = core_file_open(installed);
   CHECK(f && core_file_identity(f, &c.rec.id));
   core_file_close(f);
   c.record = true;
   core_install_identify(installed, &c);
   CHECK(c.record);
   /* As the check saw it before the write */
   c.rec.id.mtime--;
   core_install_identify(installed, &c);
   CHECK(!c.record);
   c.record = true;
   f = core_file_open(installed);
   CHECK(f && core_file_identity(f, &c.rec.id));
   core_file_close(f);
   c.rec.id.mtime_nsec ^= 1;
   core_install_identify(installed, &c);
   CHECK(!c.record);
}

static void test_copy_sig(void)
{
   uint8_t big[1000];
   char src[300];
   char ssig[300];
   char dst[300];
   char dsig[300];
   void *buf = NULL;
   int64_t n = 0;
   memset(big, 0x5a, sizeof(big));
   fill_pathname_join_special(src, TMP_DIR, "src_libretro.so", sizeof(src));
   fill_pathname_join_special(dst, TMP_DIR, "dst_libretro.so", sizeof(dst));
   sig_of(src, ssig, sizeof(ssig));
   sig_of(dst, dsig, sizeof(dsig));
   put(ssig, fx_sig_good, sizeof(fx_sig_good));
   CHECK(core_install_copy_sig(src, dst));
   CHECK(same(dsig, fx_sig_good, sizeof(fx_sig_good)));
   put(ssig, big, sizeof(big));
   /* No further than the longest valid .sig, plus one to fail it */
   CHECK(core_install_copy_sig(src, dst));
   CHECK(filestream_read_file(dsig, &buf, &n)
         && n == CORE_SIG_MAX_LEN + 1
         && !memcmp(buf, big, (size_t)n));
   free(buf);
   /* None to copy: dst's is left as it is */
   put(dsig, fx_sig_good, sizeof(fx_sig_good));
   filestream_delete(ssig);
   CHECK(!core_install_copy_sig(src, dst));
   CHECK(same(dsig, fx_sig_good, sizeof(fx_sig_good)));
   core_install_drop_sig(dst);
   CHECK(!path_is_valid(dsig));
   CHECK(!core_install_copy_sig(src, dst));
   CHECK(!path_is_valid(dsig));
}

/* A core in place, nothing staged but a .sig fetched for it */
static void stage_sig(const uint8_t *sig, size_t sig_len)
{
   char p[300];
   if (path_is_valid(staged))
      filestream_delete(staged);
   sig_of(staged, p, sizeof(p));
   if (path_is_valid(p))
      filestream_delete(p);
   if (sig)
      put(p, sig, sig_len);
}

/* The installed core hashed where it is, against staged's .sig */
static enum core_check_result run_sig(core_trust_t *t,
      struct core_install_check *ic)
{
   core_trust_set_t *s = core_trust_get(t);
   if (core_install_check_begin(ic, s, installed, installed))
   {
      while (!core_install_check_step(ic))
         ;
      core_install_check_end_sig(ic, s, staged);
   }
   core_trust_set_put(s);
   return ic->c.result;
}

/* Trusted by its hash: only the .sig decides, and the store's newest
 * build still makes an older one OLDER */
static void test_end_sig(void)
{
   struct core_install_check ic;
   struct core_check c;
   uint8_t key[CORE_SHA256_LEN];
   core_trust_t *t = new_store("store_end_sig");
   put(installed, fx_core, FX_CORE_SIZE);
   memset(&c, 0, sizeof(c));
   strlcpy(c.rec.name, FX_NAME, sizeof(c.rec.name));
   sha(fx_core, FX_CORE_SIZE, c.rec.sha256);
   c.result = CORE_CHECK_UNSIGNED;
   CHECK(core_trust_always(t, &c));
   stage_sig(fx_sig_key2, sizeof(fx_sig_key2));
   CHECK(run_sig(t, &ic) == CORE_CHECK_UNKNOWN_KEY);
   stage_sig(NULL, 0);
   CHECK(run_sig(t, &ic) == CORE_CHECK_UNSIGNED);
   stage_sig(fx_sig_good, sizeof(fx_sig_good));
   CHECK(run_sig(t, &ic) == CORE_CHECK_OK);
   CHECK(ic.c.record && ic.c.raise_newest);
   CHECK(ic.c.rec.build_time == FX_TIME);
   CHECK(!strcmp(ic.c.rec.name, FX_NAME));
   CHECK(core_folder_key(installed, key)
         && !memcmp(key, ic.c.folder, sizeof(key)));
   CHECK(core_trust_apply(t, &ic.c));
   stage_sig(fx_sig_older, sizeof(fx_sig_older));
   CHECK(run_sig(t, &ic) == CORE_CHECK_OLDER);
   CHECK(!ic.c.record && !ic.c.raise_newest);
   stage_sig(NULL, 0);
   core_trust_free(t);
}

/* Passed by its trusted hash: the .sig staged beside it, never read,
 * stays only if it verifies on its own */
static void test_vet_sig(void)
{
   struct core_install_check ic;
   struct core_check c;
   char ssig[300];
   core_trust_t *t = new_store("store_vet");
   sig_of(staged, ssig, sizeof(ssig));
   memset(&c, 0, sizeof(c));
   strlcpy(c.rec.name, FX_NAME, sizeof(c.rec.name));
   sha(fx_core, FX_CORE_SIZE, c.rec.sha256);
   c.result = CORE_CHECK_UNSIGNED;
   CHECK(core_trust_always(t, &c));
   stage(fx_core, fx_sig_key2, sizeof(fx_sig_key2));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   CHECK(ic.c.record && !ic.c.raise_newest);
   core_install_vet_sig(&ic.c, staged);
   CHECK(!path_is_valid(ssig));
   CHECK(same(staged, fx_core, FX_CORE_SIZE));
   /* Too long to be one */
   {
      static uint8_t big[CORE_SIG_MAX_LEN + 8];
      memcpy(big, fx_sig_good, sizeof(fx_sig_good));
      stage(fx_core, big, sizeof(big));
      CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
      core_install_vet_sig(&ic.c, staged);
      CHECK(!path_is_valid(ssig));
   }
   stage(fx_core, fx_sig_good, sizeof(fx_sig_good));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   core_install_vet_sig(&ic.c, staged);
   CHECK(same(ssig, fx_sig_good, sizeof(fx_sig_good)));
   /* An older build of this core: its own .sig, kept */
   stage(fx_core, fx_sig_older, sizeof(fx_sig_older));
   CHECK(run(t, 0, &ic) == CORE_CHECK_OK);
   core_install_vet_sig(&ic.c, staged);
   CHECK(same(ssig, fx_sig_older, sizeof(fx_sig_older)));
   /* Not a hash pass: left as it is */
   memset(&c, 0, sizeof(c));
   c.result = CORE_CHECK_OK;
   stage(fx_core, fx_sig_key2, sizeof(fx_sig_key2));
   core_install_vet_sig(&c, staged);
   CHECK(same(ssig, fx_sig_key2, sizeof(fx_sig_key2)));
   c.record       = true;
   c.raise_newest = true;
   core_install_vet_sig(&c, staged);
   CHECK(same(ssig, fx_sig_key2, sizeof(fx_sig_key2)));
   filestream_delete(ssig);
   filestream_delete(staged);
   core_trust_free(t);
}

static void test_has_sig(void)
{
   char p[300];
   put(installed, fx_core, FX_CORE_SIZE);
   sig_of(installed, p, sizeof(p));
   if (path_is_valid(p))
      filestream_delete(p);
   CHECK(!core_install_has_sig(installed));
   put(p, fx_sig_good, sizeof(fx_sig_good));
   CHECK(core_install_has_sig(installed));
   filestream_delete(p);
}

int main(void)
{
   path_mkdir(TMP_DIR);
   fill_pathname_join_special(cores, TMP_DIR, "cores", sizeof(cores));
   fill_pathname_join_special(staging, cores, "." FX_NAME ".staging",
         sizeof(staging));
   fill_pathname_join_special(installed, cores, FX_NAME, sizeof(installed));
   fill_pathname_join_special(staged, staging, FX_NAME, sizeof(staged));
   path_mkdir(cores);
   path_mkdir(staging);
   fx_make_core();

   test_signed();
   test_refused();
   test_older();
   test_no_quick_pass();
   test_checks_off();
   test_error_and_free();
   test_commit();
   test_commit_unsigned();
   test_commit_core_first();
   test_commit_stray_old();
   test_identify_other();
   test_identify_mtime();
   test_copy_sig();
   test_end_sig();
   test_vet_sig();
   test_has_sig();

   free(fx_core);
   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
