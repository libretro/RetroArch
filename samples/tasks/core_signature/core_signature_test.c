/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#endif
#ifdef __linux__
#include <sys/vfs.h>
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../core_signature.h"
#include "../../../core_signing_keys.h"
#include "core_signature_fixtures.h"

/* Matches the .gitignore's samples/ patterns */
#define TMP_DIR "core_signature_test_tmp"

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint8_t *fx_core;
static uint8_t fx_keys[2 * CORE_SIG_KEY_LEN];

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
   memcpy(fx_keys, fx_key1, CORE_SIG_KEY_LEN);
   memcpy(fx_keys + CORE_SIG_KEY_LEN, fx_key2, CORE_SIG_KEY_LEN);
}

static void sha(const uint8_t *buf, size_t len, uint8_t *out)
{
   struct sha256_state st;
   sha256_stream_init(&st, 0);
   sha256_stream_update(&st, buf, len);
   sha256_stream_final(&st, out);
}

/* A copy of a fixture with one byte flipped; idx past the end appends */
static uint8_t edited[CORE_SIG_MAX_LEN + 2];
static const uint8_t *flip(const uint8_t *sig, size_t len, size_t idx)
{
   memcpy(edited, sig, len);
   if (idx < len)
      edited[idx] ^= 0x01;
   else
      edited[len] = 0;
   return edited;
}

static enum core_check_result verify(const uint8_t *buf, size_t len,
      const char *name, const uint8_t *core_sha, size_t num_keys)
{
   struct core_sig sig;
   if (!core_sig_parse(&sig, buf, len))
      return CORE_CHECK_MODIFIED;
   return core_sig_verify(&sig, name, core_sha, fx_keys, num_keys);
}

static void test_sig(void)
{
   struct core_sig sig;
   uint8_t core_sha[CORE_SHA256_LEN];
   uint8_t digest[CORE_SHA256_LEN];
   uint8_t id[CORE_SIG_KEY_ID_LEN];
   size_t n = sizeof(fx_sig_good);
   size_t count;
   const uint8_t *keys;
   uint8_t *short_sig;
   sha(fx_core, FX_CORE_SIZE, core_sha);

   /* Parse: exact length, magic, name */
   CHECK(core_sig_parse(&sig, fx_sig_good, n));
   CHECK(string_is_equal(sig.name, FX_NAME));
   CHECK(sig.build_time == FX_TIME);
   CHECK(!memcmp(sig.core_sha256, core_sha, CORE_SHA256_LEN));
   sha(fx_sig_good, n - CORE_SIG_RS_LEN, digest);
   CHECK(!memcmp(sig.digest, digest, CORE_SHA256_LEN));
   CHECK(!core_sig_parse(&sig, fx_sig_good, n - 1));
   CHECK(!core_sig_parse(&sig, flip(fx_sig_good, n, n), n + 1));
   CHECK(!core_sig_parse(&sig, fx_sig_good, CORE_SIG_FIXED_LEN));
   CHECK(!core_sig_parse(&sig, flip(fx_sig_good, n, 0), n));
   CHECK(!core_sig_parse(&sig, flip(fx_sig_good, n, CORE_SIG_FIXED_LEN - 1),
            n));
   memcpy(edited, fx_sig_good, n);
   edited[CORE_SIG_FIXED_LEN] = '\0';
   CHECK(!core_sig_parse(&sig, edited, n));
   /* Build time 0 would never age out under the rollback rule */
   memcpy(edited, fx_sig_good, n);
   memset(edited + 16, 0, 8);
   CHECK(!core_sig_parse(&sig, edited, n));
   edited[23] = 1;
   CHECK(core_sig_parse(&sig, edited, n) && sig.build_time == 1);
   memcpy(edited, fx_sig_good, CORE_SIG_FIXED_LEN);
   edited[CORE_SIG_FIXED_LEN - 1] = 0;
   memcpy(edited + CORE_SIG_FIXED_LEN, fx_sig_good + n - CORE_SIG_RS_LEN,
         CORE_SIG_RS_LEN);
   CHECK(!core_sig_parse(&sig, edited, CORE_SIG_FIXED_LEN + CORE_SIG_RS_LEN));
   /* Shorter than the fixed part, alone in its allocation so a sanitizer
    * sees any read past it */
   short_sig = (uint8_t*)malloc(CORE_SIG_FIXED_LEN - 1);
   memcpy(short_sig, fx_sig_good, CORE_SIG_FIXED_LEN - 1);
   CHECK(!core_sig_parse(&sig, short_sig, CORE_SIG_FIXED_LEN - 1));
   CHECK(!core_sig_parse(&sig, short_sig, sizeof(CORE_SIG_MAGIC) - 1));
   free(short_sig);

   /* Key ids are the SHA-256 of the key, not the key */
   sha(fx_key1, CORE_SIG_KEY_LEN, digest);
   core_sig_key_id(fx_key1, id);
   CHECK(!memcmp(id, digest, CORE_SIG_KEY_ID_LEN));
   CHECK(!memcmp(id, fx_sig_good + 8, CORE_SIG_KEY_ID_LEN));

   /* Verify */
   CHECK(verify(fx_sig_good, n, FX_NAME, core_sha, 2) == CORE_CHECK_OK);
   CHECK(verify(fx_sig_good, n, FX_NAME, core_sha, 1) == CORE_CHECK_OK);
   CHECK(verify(fx_sig_key2, sizeof(fx_sig_key2), FX_NAME, core_sha, 2)
         == CORE_CHECK_OK);
   CHECK(verify(fx_sig_key2, sizeof(fx_sig_key2), FX_NAME, core_sha, 1)
         == CORE_CHECK_UNKNOWN_KEY);
   CHECK(verify(fx_sig_good, n, FX_NAME, core_sha, 0)
         == CORE_CHECK_UNKNOWN_KEY);
   CHECK(verify(fx_sig_older, sizeof(fx_sig_older), FX_NAME, core_sha, 2)
         == CORE_CHECK_OK);
   /* Validly signed for another name, so only the name check refuses */
   CHECK(verify(fx_sig_other_name, sizeof(fx_sig_other_name),
            "other_libretro.so", core_sha, 2) == CORE_CHECK_OK);
   CHECK(verify(fx_sig_other_name, sizeof(fx_sig_other_name),
            FX_NAME, core_sha, 2) == CORE_CHECK_MODIFIED);
   CHECK(verify(fx_sig_good, n, "test_libretro.dll", core_sha, 2)
         == CORE_CHECK_MODIFIED);
   /* One flipped byte: key id, build time, core hash, name, r, s */
   CHECK(verify(flip(fx_sig_good, n, 9), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_UNKNOWN_KEY);
   CHECK(verify(flip(fx_sig_good, n, 23), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, 30), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, CORE_SIG_FIXED_LEN), n, FX_NAME,
            core_sha, 2) == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, n - 40), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, n - 64), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, n - 1), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   /* The core itself */
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   sha(fx_core, FX_CORE_SIZE, digest);
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   CHECK(verify(fx_sig_good, n, FX_NAME, digest, 2) == CORE_CHECK_MODIFIED);

   /* What core_signing_keys.h lists */
   keys = core_sig_builtin_keys(&count);
   CHECK(count == CORE_SIGNING_KEYS_COUNT && (keys == NULL) == (count == 0));
}

static char tmp_path[256];
static const char *tmp(const char *name)
{
   fill_pathname_join_special(tmp_path, TMP_DIR, name, sizeof(tmp_path));
   return tmp_path;
}

static bool identity_of(const char *path, struct core_identity *id)
{
   core_file_t *f = core_file_open(path);
   bool ok        = f && core_file_identity(f, id);
   core_file_close(f);
   return ok;
}

/* Whether this file's filesystem gets identity records */
static bool records_for(const char *path)
{
   struct core_identity id;
   return identity_of(path, &id) && id.ctime_kept;
}

static void test_identity(void)
{
   /* ext2/3/4, xfs, btrfs, f2fs, tmpfs, zfs, bcachefs */
   static const unsigned long kept_magic[] = { 0xEF53UL, 0x58465342UL,
      0x9123683EUL, 0xF2F52010UL, 0x01021994UL, 0x2FC12FC1UL,
      0xCA451A4EUL };
   /* overlay, FUSE, NFS, CIFS, SMB2, FAT, exFAT, NTFS, squashfs, erofs,
    * iso9660, proc */
   static const unsigned long other_magic[] = { 0x794C7630UL, 0x65735546UL,
      0x6969UL, 0xFF534D42UL, 0xFE534D42UL, 0x4D44UL, 0x2011BAB0UL,
      0x5346544EUL, 0x73717368UL, 0xE0F5E1E2UL, 0x9660UL, 0x9FA0UL };
   static const char *const kept_name[] = { "apfs", "hfs", "ufs", "ffs",
      "zfs", "tmpfs" };
   static const char *const other_name[] = { "msdos", "exfat", "ntfs",
      "smbfs", "nfs", "fusefs", "nullfs", "unionfs", "cd9660", "hammer2",
      "APFS", "" };
   struct core_identity a, b;
   struct core_hash h;
   uint8_t want[CORE_SHA256_LEN];
   uint8_t got[CORE_SHA256_LEN];
   core_file_t *f;
   int step;
   size_t i;
#ifdef __linux__
   struct statfs fs;
#endif

   /* Each field on its own */
   memset(&a, 0, sizeof(a));
   a.dev = 1; a.size = 2; a.mtime = 3; a.mtime_nsec = 4;
   a.ctime = 5; a.ctime_nsec = 6; a.ino[15] = 7;
   a.verity_len = 32; a.verity[31] = 8;
   b = a;
   CHECK(core_identity_equal(&a, &b));
   b = a; b.dev++;        CHECK(!core_identity_equal(&a, &b));
   b = a; b.size++;       CHECK(!core_identity_equal(&a, &b));
   b = a; b.mtime++;      CHECK(!core_identity_equal(&a, &b));
   b = a; b.mtime_nsec++; CHECK(!core_identity_equal(&a, &b));
   b = a; b.ino[0]++;     CHECK(!core_identity_equal(&a, &b));
   b = a; b.verity[0]++;  CHECK(!core_identity_equal(&a, &b));
   b = a; b.verity_len--; CHECK(!core_identity_equal(&a, &b));
   /* A reused inode, same size, mtime set back: only ctime tells */
   b = a; b.ctime++;      CHECK(!core_identity_equal(&a, &b));
   b = a; b.ctime_nsec++; CHECK(!core_identity_equal(&a, &b));
   /* A damaged record never matches */
   a.verity_len = CORE_ID_VERITY_MAX + 1;
   b = a;                 CHECK(!core_identity_equal(&a, &b));

   /* From real files */
   CHECK(filestream_write_file(tmp("a.so"), fx_core, FX_CORE_SIZE));
   CHECK(filestream_write_file(tmp("b.so"), fx_core, FX_CORE_SIZE));
   CHECK(identity_of(tmp("a.so"), &a));
   CHECK(identity_of(tmp("a.so"), &b));
   CHECK(core_identity_equal(&a, &b));
   CHECK(a.size == FX_CORE_SIZE);
   CHECK(identity_of(tmp("b.so"), &b));
   CHECK(!core_identity_equal(&a, &b));
   CHECK(filestream_write_file(tmp("a.so"), fx_core, FX_CORE_SIZE - 1));
   CHECK(identity_of(tmp("a.so"), &b));
   CHECK(!core_identity_equal(&a, &b));
   CHECK(!core_file_open(tmp("missing.so")));
   CHECK(!core_file_open(TMP_DIR));

   /* Records only where the filesystem keeps ctime itself */
   for (i = 0; i < sizeof(kept_magic) / sizeof(kept_magic[0]); i++)
      CHECK(core_fs_magic_keeps_ctime(kept_magic[i]));
   for (i = 0; i < sizeof(other_magic) / sizeof(other_magic[0]); i++)
      CHECK(!core_fs_magic_keeps_ctime(other_magic[i]));
   for (i = 0; i < sizeof(kept_name) / sizeof(kept_name[0]); i++)
      CHECK(core_fs_name_keeps_ctime(kept_name[i]));
   for (i = 0; i < sizeof(other_name) / sizeof(other_name[0]); i++)
      CHECK(!core_fs_name_keeps_ctime(other_name[i]));
   CHECK(!core_fs_name_keeps_ctime(NULL));
#ifdef _WIN32
   CHECK(!records_for(tmp("a.so")));
#else
   if (!records_for(tmp("a.so")))
      printf("note: no identity records on %s's filesystem\n", TMP_DIR);
#endif
#ifdef __linux__
   CHECK(statfs(TMP_DIR, &fs) == 0 && records_for(tmp("a.so"))
         == core_fs_magic_keeps_ctime((unsigned long)fs.f_type));
   if ((f = core_file_open("/proc/self/stat")))
   {
      CHECK(core_file_identity(f, &b) && !b.ctime_kept);
      core_file_close(f);
   }
#endif
#ifndef _WIN32
   /* Refused without waiting for a writer */
   CHECK(!mkfifo(tmp("fifo"), 0600));
   CHECK(!core_file_open(tmp("fifo")));
   filestream_delete(tmp("fifo"));
#endif

   /* Hashed in chunks, the same as in one go */
   sha(fx_core, FX_CORE_SIZE, want);
   CHECK((f = core_file_open(tmp("b.so"))) != NULL);
   CHECK(core_hash_init(&h));
   do
      step = core_hash_step(&h, f);
   while (step > 0);
   CHECK(step == 0);
   CHECK(h.done == FX_CORE_SIZE);
   core_hash_final(&h, got);
   CHECK(!memcmp(want, got, CORE_SHA256_LEN));
#ifdef _WIN32
   /* Nothing writes it while it is open */
   CHECK(!filestream_write_file(tmp("b.so"), fx_core, 1));
#endif
   core_file_close(f);
}

static void clean(void)
{
   static const char *const names[] = { "a.so", "b.so", "fifo", FX_NAME,
      FX_NAME ".sig", "copy_libretro.so", "core_trust", "core_trust.tmp" };
   size_t i;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      filestream_delete(tmp(names[i]));
   path_rmdir(tmp("RetroArch"));
   path_rmdir(TMP_DIR);
}

int main(void)
{
   /* FAIL lines survive a crash or a sanitizer exit */
   setvbuf(stdout, NULL, _IONBF, 0);
   fx_make_core();
   clean();
   if (!path_mkdir(TMP_DIR))
   {
      printf("FAIL: cannot make %s\n", TMP_DIR);
      return 1;
   }
   test_sig();
   test_identity();
   clean();
   free(fx_core);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
