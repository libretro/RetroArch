/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_XOPEN_SOURCE)
/* symlink and lstat; strict C89 hides them */
#define _XOPEN_SOURCE 700
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/vfs.h>
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../core_signature.h"
#include "../../../core_signing_keys.h"
#include "../../../file_path_special.h"
#include "core_signature_fixtures.h"

/* Matches the .gitignore's samples/ patterns */
#define TMP_DIR "core_signature_test_tmp"

static unsigned checks;
static unsigned failures;
static const char *self_path = "";

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint8_t *fx_core;
static uint8_t fx_keys[2 * CORE_SIG_KEY_LEN];
/* The folder key of a check_of() */
static const uint8_t nowhere[CORE_SHA256_LEN];
/* and its device */
static const uint64_t fx_dev = 1;

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

#ifndef _WIN32
/* The open file itself, for loading from it */
static void test_file_fd(void)
{
   uint8_t buf[4];
   struct core_identity id;
   struct stat st;
   uint64_t ino = 0;
   core_file_t *f;
   unsigned i;
   int fd;
   CHECK(filestream_write_file(tmp("native.so"), fx_core, FX_CORE_SIZE));
   CHECK((f = core_file_open(tmp("native.so"))) != NULL);
   if (!f)
      return;
   CHECK(core_file_identity(f, &id));
   fd = core_file_fd(f);
   CHECK(fd >= 0 && fstat(fd, &st) == 0);
   for (i = 0; i < 8; i++)
      ino = (ino << 8) | id.ino[8 + i];
   CHECK((uint64_t)st.st_dev == id.dev && (uint64_t)st.st_ino == ino
         && (uint64_t)st.st_size == FX_CORE_SIZE);
   CHECK(read(fd, buf, sizeof(buf)) == (ssize_t)sizeof(buf));
   CHECK(!memcmp(buf, fx_core, sizeof(buf)));
   /* Closed with f */
   core_file_close(f);
   CHECK(fcntl(fd, F_GETFD) == -1);
   filestream_delete(tmp("native.so"));
}
#endif

static void check_of(struct core_check *c, const char *name,
      uint64_t file, uint8_t hash_byte, uint64_t build_time)
{
   unsigned i;
   memset(c, 0, sizeof(*c));
   strlcpy(c->rec.name, name, sizeof(c->rec.name));
   c->rec.id.dev        = fx_dev;
   c->rec.id.size       = file * 10;
   for (i = 0; i < 8; i++)
      c->rec.id.ino[8 + i] = (uint8_t)(file >> (56 - 8 * i));
   c->rec.id.ctime_kept = true;
   c->rec.build_time    = build_time;
   memset(c->rec.sha256, hash_byte, CORE_SHA256_LEN);
   c->record            = true;
   c->raise_newest      = build_time != 0;
}

/* A store for check_of()'s records */
static core_trust_t *fx_store(void)
{
   core_trust_t *t = core_trust_new();
   CHECK(core_trust_set_device(t, &fx_dev, NULL));
   return t;
}

/* One as RetroArch loads it, for records of the test's own files */
static core_trust_t *real_store(void)
{
   core_trust_t *t = core_trust_new();
   CHECK(core_trust_load_file(t, tmp("store_trust"))
         == CORE_TRUST_LOAD_MISSING);
   return t;
}

/* A store file whose cut bytes from buf + at are replaced by ins, and
 * the trailer made good again, so the parse reaches its own checks */
static enum core_trust_load parse_spliced(core_trust_t *u,
      const uint8_t *buf, size_t len, size_t at, size_t cut,
      const uint8_t *ins, size_t n)
{
   size_t   out_len = len - cut + n;
   uint8_t *out     = (uint8_t*)malloc(out_len);
   enum core_trust_load ret;
   memcpy(out, buf, at);
   memcpy(out + at, ins, n);
   memcpy(out + at + n, buf + at + cut, len - at - cut);
   sha(out, out_len - CORE_SHA256_LEN, out + out_len - CORE_SHA256_LEN);
   ret = core_trust_parse(u, out, out_len);
   free(out);
   return ret;
}

static void test_trust(void)
{
   core_trust_t *t = fx_store();
   core_trust_t *u = core_trust_new();
   core_trust_set_t *s0, *s1;
   struct core_check c;
   uint8_t more[6 + CORE_TRUST_RECORD_FIXED + CORE_SIG_NAME_MAX];
   static const uint8_t zero[1] = { 0 };
   uint8_t *buf;
   size_t len, name_at, rec_len, i;

   check_of(&c, "a_libretro.so", 1, 0xaa, 100);
   s0 = core_trust_get(t);
   CHECK(core_trust_apply(t, &c));
   /* A set handed out never changes */
   CHECK(!core_trust_has_identity(s0, &c.rec.id, "a_libretro.so"));
   CHECK(core_trust_newest(s0, nowhere, "a_libretro.so") == 0);
   core_trust_set_put(s0);
   s1 = core_trust_get(t);
   CHECK(core_trust_has_identity(s1, &c.rec.id, "a_libretro.so"));
   CHECK(!core_trust_has_identity(s1, &c.rec.id, "b_libretro.so"));
   CHECK(core_trust_newest(s1, nowhere, "a_libretro.so") == 100);
   CHECK(!core_trust_has_hash(s1, c.rec.sha256));
   core_trust_set_put(s1);

   /* Newest only rises */
   check_of(&c, "a_libretro.so", 2, 0xab, 50);
   c.record = false;
   CHECK(core_trust_apply(t, &c));
   s1 = core_trust_get(t);
   CHECK(core_trust_newest(s1, nowhere, "a_libretro.so") == 100);
   core_trust_set_put(s1);

   /* Always Trust keeps the hash and the identity */
   check_of(&c, "u_libretro.so", 3, 0x55, 0);
   s0 = core_trust_get(t);
   CHECK(core_trust_always(t, &c));
   CHECK(!core_trust_has_hash(s0, c.rec.sha256));
   CHECK(!core_trust_has_identity(s0, &c.rec.id, "u_libretro.so"));
   core_trust_set_put(s0);
   s0 = core_trust_get(t);
   CHECK(core_trust_set_flag(t, CORE_TRUST_SEEDED, true));
   CHECK(core_trust_flags(s0) == 0);
   core_trust_set_put(s0);
   s1 = core_trust_get(t);
   CHECK(core_trust_has_hash(s1, c.rec.sha256));
   CHECK(core_trust_has_identity(s1, &c.rec.id, "u_libretro.so"));
   CHECK(core_trust_flags(s1) == CORE_TRUST_SEEDED);
   core_trust_set_put(s1);

   /* Round trip */
   CHECK((buf = core_trust_serialize(t, &len)) != NULL);
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_OK);
   s0 = core_trust_get(u);
   CHECK(core_trust_has_hash(s0, c.rec.sha256));
   CHECK(core_trust_has_identity(s0, &c.rec.id, "u_libretro.so"));
   CHECK(core_trust_newest(s0, nowhere, "a_libretro.so") == 100);
   CHECK(core_trust_flags(s0) == CORE_TRUST_SEEDED);
   core_trust_set_put(s0);

   /* Any damage is found */
   for (i = 0; i < len; i += 7)
   {
      buf[i] ^= 0x10;
      CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_DAMAGED);
      buf[i] ^= 0x10;
   }
   CHECK(core_trust_parse(u, buf, len - 1) == CORE_TRUST_LOAD_DAMAGED);
   CHECK(core_trust_parse(u, buf, CORE_TRUST_FILE_MAX + 1)
         == CORE_TRUST_LOAD_DAMAGED);
   free(buf);

   /* A record the parser would refuse is never stored */
   check_of(&c, "v_libretro.so", 4, 0x66, 7);
   c.rec.id.verity_len = CORE_ID_VERITY_MAX + 1;
   s0 = core_trust_get(t);
   CHECK(!core_trust_apply(t, &c));
   CHECK(!core_trust_always(t, &c));
   s1 = core_trust_get(t);
   CHECK(s1 == s0);
   core_trust_set_put(s1);
   core_trust_set_put(s0);

   /* Records are a cache: the oldest goes */
   for (i = 0; i < CORE_TRUST_MAX_RECORDS + 1; i++)
   {
      check_of(&c, "r_libretro.so", 1000 + i, 0x01, 0);
      CHECK(core_trust_apply(t, &c));
   }
   s0 = core_trust_get(t);
   check_of(&c, "r_libretro.so", 1000, 0x01, 0);
   CHECK(!core_trust_has_identity(s0, &c.rec.id, "r_libretro.so"));
   check_of(&c, "r_libretro.so", 1000 + CORE_TRUST_MAX_RECORDS, 0x01, 0);
   CHECK(core_trust_has_identity(s0, &c.rec.id, "r_libretro.so"));
   core_trust_set_put(s0);
   /* Trusted hashes are not: a full list refuses */
   for (i = 0; i < CORE_TRUST_MAX_HASHES - 1; i++)
   {
      check_of(&c, "h_libretro.so", 5000, 0, 0);
      c.rec.sha256[0] = (uint8_t)i;
      c.rec.sha256[1] = (uint8_t)(i >> 8);
      CHECK(core_trust_always(t, &c));
   }
   c.rec.sha256[2] = 1;
   CHECK(!core_trust_always(t, &c));
   /* A full store still fits the cap */
   CHECK((buf = core_trust_serialize(t, &len)) != NULL);
   CHECK(len <= CORE_TRUST_FILE_MAX);
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_OK);

   /* The parser's own checks, behind a good trailer */
   CHECK(parse_spliced(u, buf, len, 0, 0, zero, 0) == CORE_TRUST_LOAD_OK);
   name_at = CORE_TRUST_HEAD_LEN + CORE_TRUST_RECORD_FIXED - 1;
   rec_len = CORE_TRUST_RECORD_FIXED + buf[name_at];
   /* One record past the cap */
   more[0] = (CORE_TRUST_MAX_RECORDS + 1) >> 8;
   more[1] = (CORE_TRUST_MAX_RECORDS + 1) & 0xff;
   memcpy(more + 2, buf + 14, 4);
   memcpy(more + 6, buf + CORE_TRUST_HEAD_LEN, rec_len);
   CHECK(parse_spliced(u, buf, len, 12, 6, more, 6 + rec_len)
         == CORE_TRUST_LOAD_DAMAGED);
   more[0] = CORE_ID_VERITY_MAX + 1;
   CHECK(parse_spliced(u, buf, len, CORE_TRUST_HEAD_LEN + 56, 1, more, 1)
         == CORE_TRUST_LOAD_DAMAGED);
   /* An empty name, and one with a NUL */
   CHECK(parse_spliced(u, buf, len, name_at, 1 + buf[name_at], zero, 1)
         == CORE_TRUST_LOAD_DAMAGED);
   CHECK(parse_spliced(u, buf, len, name_at + 1, 1, zero, 1)
         == CORE_TRUST_LOAD_DAMAGED);
   /* A byte too many, and one too few */
   CHECK(parse_spliced(u, buf, len, len - CORE_SHA256_LEN, 0, zero, 1)
         == CORE_TRUST_LOAD_DAMAGED);
   CHECK(parse_spliced(u, buf, len, len - CORE_SHA256_LEN - 1, 1, zero, 0)
         == CORE_TRUST_LOAD_DAMAGED);
   free(buf);

   core_trust_free(t);
   core_trust_free(u);
}

/* A check that only raises the newest of core n in folder key, keys
 * ordered as their numbers */
static void newest_of(struct core_check *c, unsigned long key, unsigned n,
      uint64_t build_time)
{
   char name[] = "c00000_libretro.so";
   unsigned i;
   for (i = 5; i > 0; i--, n /= 10)
      name[i] = (char)('0' + n % 10);
   check_of(c, name, 1, 0x02, build_time);
   c->folder[0] = (uint8_t)(key >> 16);
   c->folder[1] = (uint8_t)(key >> 8);
   c->folder[2] = (uint8_t)key;
   c->record    = false;
}

static uint64_t newest_is(const core_trust_set_t *s, unsigned long key,
      unsigned n)
{
   struct core_check c;
   newest_of(&c, key, n, 0);
   return core_trust_newest(s, c.folder, c.rec.name);
}

/* A full newest list drops its oldest build; a tie goes to the lower
 * folder key, then the lower name */
static void test_trust_newest_full(void)
{
   static const uint64_t times[] = { 30, 10, 10 };
   core_trust_t *t = fx_store();
   core_trust_set_t *s;
   struct core_check c;
   unsigned i, kept;
   /* Three in one folder, the rest one each, keys falling */
   for (i = 0; i < CORE_TRUST_MAX_NEWEST; i++)
   {
      if (i < 3)
         newest_of(&c, 0xfe0000UL, i, times[i]);
      else
         newest_of(&c, 0x10000UL + CORE_TRUST_MAX_NEWEST - i, 0, 20);
      CHECK(core_trust_apply(t, &c));
   }
   newest_of(&c, 0, 3, 40);
   c.record = true;
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c.rec.id, c.rec.name));
   CHECK(newest_is(s, 0, 3) == 40);
   CHECK(newest_is(s, 0xfe0000UL, 1) == 0);
   CHECK(newest_is(s, 0xfe0000UL, 2) == 10);
   core_trust_set_put(s);
   /* Then the other 10, then the lowest key of the 20s, listed last */
   for (i = 4; i < 6; i++)
   {
      newest_of(&c, 0, i, 40);
      CHECK(core_trust_apply(t, &c));
   }
   s = core_trust_get(t);
   CHECK(newest_is(s, 0xfe0000UL, 0) == 30);
   CHECK(newest_is(s, 0xfe0000UL, 2) == 0);
   CHECK(newest_is(s, 0x10001UL, 0) == 0);
   CHECK(newest_is(s, 0x10002UL, 0) == 20);
   for (i = 3; i < 6; i++)
      CHECK(newest_is(s, 0, i) == 40);
   for (i = 3, kept = 0; i < CORE_TRUST_MAX_NEWEST; i++)
      kept += newest_is(s, 0x10000UL + CORE_TRUST_MAX_NEWEST - i, 0) == 20;
   CHECK(kept == CORE_TRUST_MAX_NEWEST - 4);
   core_trust_set_put(s);
   core_trust_free(t);
}

/* Which folders lose at the cap is not decided by their keys or sizes */
static void test_trust_newest_cap(void)
{
   core_trust_t *t = core_trust_new();
   core_trust_set_t *s;
   struct core_check c;
   unsigned i, n;
   /* New cores in folders keyed below the rest each stay */
   for (i = 0; i < CORE_TRUST_MAX_NEWEST; i++)
   {
      newest_of(&c, 0x800000UL + i, 0, 1000 + i);
      CHECK(core_trust_apply(t, &c));
   }
   for (i = 0; i < 10; i++)
   {
      newest_of(&c, 0x100000UL + i, 0, 5000);
      CHECK(core_trust_apply(t, &c));
   }
   s = core_trust_get(t);
   for (i = n = 0; i < 10; i++)
      n += newest_is(s, 0x100000UL + i, 0) == 5000;
   CHECK(n == 10);
   core_trust_set_put(s);
   core_trust_free(t);

   /* A folder holding most of them keeps its own as it grows */
   t = core_trust_new();
   for (i = 0; i < CORE_TRUST_MAX_NEWEST; i++)
   {
      if (i < 200)
         newest_of(&c, 0x800000UL, i, 2000 + i);
      else
         newest_of(&c, 0xa00000UL + i, 0, 1);
      CHECK(core_trust_apply(t, &c));
   }
   for (i = 0; i < 5; i++)
   {
      newest_of(&c, 0x800000UL, 900 + i, 9000);
      CHECK(core_trust_apply(t, &c));
   }
   s = core_trust_get(t);
   for (i = n = 0; i < 200; i++)
      n += newest_is(s, 0x800000UL, i) == 2000 + i;
   CHECK(n == 200);
   for (i = n = 0; i < 5; i++)
      n += newest_is(s, 0x800000UL, 900 + i) == 9000;
   CHECK(n == 5);
   core_trust_set_put(s);
   core_trust_free(t);
}

struct trust_reader
{
   core_trust_set_t *s;
   retro_atomic_int_t done;
   bool saw_change;
};

/* A worker reading a set while the main thread changes the store */
static void reader(void *data)
{
   struct trust_reader *r = (struct trust_reader*)data;
   uint8_t hash[CORE_SHA256_LEN];
   int done;
   memset(hash, 0x77, sizeof(hash));
   do
   {
      /* So the last pass comes after every change */
      done = retro_atomic_load_acquire_int(&r->done);
      if (     core_trust_has_hash(r->s, hash)
            || core_trust_newest(r->s, nowhere, "x") != 0
            || core_trust_flags(r->s) != 0)
         r->saw_change = true;
   } while (!done);
   core_trust_set_put(r->s);
}

static void test_trust_threads(void)
{
   core_trust_t *t = core_trust_new();
   struct trust_reader r;
   struct core_check c;
   sthread_t *th;
   unsigned i;
   r.s          = core_trust_get(t);
   r.saw_change = false;
   retro_atomic_int_init(&r.done, 0);
   th = sthread_create(reader, &r);
   CHECK(th != NULL);
   /* The first apply edits the reader's set unless it copies */
   for (i = 0; i < 200; i++)
   {
      check_of(&c, "x", 1, 0x77, 1 + i);
      core_trust_apply(t, &c);
      core_trust_set_flag(t, CORE_TRUST_SEEDED, !(i & 1));
      core_trust_always(t, &c);
   }
   retro_atomic_store_release_int(&r.done, 1);
   if (th)
      sthread_join(th);
   else
      core_trust_set_put(r.s);
   CHECK(!r.saw_change);
   core_trust_free(t);
}

static bool file_is(const char *path, const void *data, size_t len)
{
   void *buf   = NULL;
   int64_t got = 0;
   bool same   = filestream_read_file(path, &buf, &got)
      && got == (int64_t)len && !memcmp(buf, data, len);
   free(buf);
   return same;
}

/* core_trust_default_path's folder, here the test's own */
bool fill_pathname_application_data(char *s, size_t len)
{
   strlcpy(s, TMP_DIR, len);
   return true;
}

static const char junk[] = "not a trust store";

static void test_trust_file_write(core_trust_t *t, core_trust_t *u)
{
   static const char junk2[] = "nor is this";
   core_trust_set_t *s;
   struct core_check c;
   char path[256];
   char bad[256];
   core_trust_write_t *w;
   uint8_t *buf;
   size_t len;

   CHECK(core_trust_default_path(path, sizeof(path)));
   CHECK(string_ends_with(path, CORE_TRUST_FILE_NAME));
   CHECK(strstr(path, TMP_DIR) == path);
#ifdef _WIN32
   CHECK(strstr(path, "RetroArch") != NULL);
#endif
   strlcpy(path, tmp("core_trust"), sizeof(path));
   strlcpy(bad, tmp("core_trust.bad"), sizeof(bad));
   CHECK(core_trust_load_file(t, path) == CORE_TRUST_LOAD_MISSING);
   CHECK(core_trust_load_file(u, tmp("nowhere/core_trust"))
         == CORE_TRUST_LOAD_MISSING);
   CHECK(core_trust_set_device(t, &fx_dev, NULL));

   /* Written whole, through a temporary, and read back */
   check_of(&c, "a_libretro.so", 1, 0xaa, 100);
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_set_flag(t, CORE_TRUST_SEEDED, true));
   CHECK((w = core_trust_write_begin(t)) != NULL);
   CHECK(core_trust_write_run(w, path));
   CHECK((buf = core_trust_serialize(t, &len)) != NULL);
   CHECK(file_is(path, buf, len));
   free(buf);
   CHECK(!core_trust_write_end(t, w, true));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(u);
   CHECK(core_trust_flags(s) == CORE_TRUST_SEEDED);
   CHECK(core_trust_has_identity(s, &c.rec.id, "a_libretro.so"));
   core_trust_set_put(s);

   /* One write at a time; a change meanwhile writes again after */
   CHECK((w = core_trust_write_begin(t)) != NULL);
   CHECK(core_trust_set_flag(t, CORE_TRUST_NOTICE_SHOWN, true));
   CHECK(!core_trust_write_begin(t));
   CHECK(core_trust_write_end(t, w, false));
   CHECK((w = core_trust_write_begin(t)) != NULL);
   CHECK(!core_trust_write_end(t, w, false));

   /* A damaged file is set aside and the store begins again, seed
    * and all */
   CHECK(filestream_write_file(path, junk, sizeof(junk)));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_RESET);
   CHECK(file_is(bad, junk, sizeof(junk)));
   CHECK(!path_is_valid(path));
   s = core_trust_get(u);
   CHECK(core_trust_flags(s) == 0);
   CHECK(!core_trust_has_identity(s, &c.rec.id, "a_libretro.so"));
   CHECK(core_trust_newest(s, nowhere, "a_libretro.so") == 0);
   core_trust_set_put(s);
   CHECK(core_trust_set_device(u, &fx_dev, NULL));
   check_of(&c, "b_libretro.so", 2, 0xbb, 200);
   CHECK(core_trust_apply(u, &c));
   CHECK((w = core_trust_write_begin(u)) != NULL);
   CHECK(core_trust_write_run(w, path));
   CHECK(!core_trust_write_end(u, w, true));
   CHECK(core_trust_load_file(t, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c.rec.id, "b_libretro.so"));
   core_trust_set_put(s);

   /* Damage alone leaves the store read-only, until set aside */
   CHECK(core_trust_parse(u, (const uint8_t*)junk, sizeof(junk))
         == CORE_TRUST_LOAD_DAMAGED);
   if ((w = core_trust_write_begin(u)))
   {
      core_trust_write_end(u, w, false);
      CHECK(!"a damaged store is writable");
   }

   /* An older .bad is replaced */
   CHECK(filestream_write_file(path, junk2, sizeof(junk2)));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_RESET);
   CHECK(file_is(bad, junk2, sizeof(junk2)));
}

static void test_trust_file_kept(core_trust_t *t, core_trust_t *u)
{
   char path[256];
   char bad[256];
   core_trust_write_t *w;
   uint8_t *big;
   uint8_t *buf;
   size_t len;

   strlcpy(path, tmp("core_trust"), sizeof(path));
   strlcpy(bad, tmp("core_trust.bad"), sizeof(bad));

   /* Too big to be a store is damage too */
   big = (uint8_t*)calloc(1, CORE_TRUST_FILE_MAX + 1);
   CHECK(filestream_write_file(path, big, CORE_TRUST_FILE_MAX + 1));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_RESET);
   CHECK(file_is(bad, big, CORE_TRUST_FILE_MAX + 1));

   /* A newer version's store, however big, is left for it */
   CHECK((buf = core_trust_serialize(t, &len)) != NULL);
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '2';
   CHECK(filestream_write_file(path, buf, (int64_t)len));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_UNREADABLE);
   if ((w = core_trust_write_begin(u)))
   {
      core_trust_write_end(u, w, false);
      CHECK(!"a newer store is writable");
   }
   CHECK(file_is(path, buf, len));
   CHECK(file_is(bad, big, CORE_TRUST_FILE_MAX + 1));
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '9';
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_UNREADABLE);
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = ':';
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_DAMAGED);
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '0';
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_DAMAGED);
   free(buf);
   memcpy(big, "RACTRST2", 8);
   CHECK(filestream_write_file(path, big, CORE_TRUST_FILE_MAX + 1));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_UNREADABLE);
   CHECK(file_is(path, big, CORE_TRUST_FILE_MAX + 1));
   free(big);

   /* One that cannot be set aside is never written over */
   CHECK(!filestream_delete(bad));
   CHECK(path_mkdir(bad));
   CHECK(filestream_write_file(path, junk, sizeof(junk)));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_UNREADABLE);
   if ((w = core_trust_write_begin(u)))
   {
      core_trust_write_end(u, w, false);
      CHECK(!"wrote over a store not set aside");
   }
   CHECK(file_is(path, junk, sizeof(junk)));
   CHECK(path_is_directory(bad));
}

static void test_trust_file_unreadable(void)
{
   core_trust_t *u;
#ifndef _WIN32
   char path[256];
#endif
   char dir[256];
   core_trust_write_t *w;

   /* Anything but a file is unreadable, and stays where it is */
   strlcpy(dir, tmp("core_trust_dir"), sizeof(dir));
   CHECK(path_mkdir(dir));
   u = core_trust_new();
   CHECK(core_trust_load_file(u, dir) == CORE_TRUST_LOAD_UNREADABLE);
   if ((w = core_trust_write_begin(u)))
   {
      core_trust_write_end(u, w, false);
      CHECK(!"an unreadable store is writable");
   }
   CHECK(path_is_directory(dir));
   CHECK(!path_is_valid(tmp("core_trust_dir.bad")));
#ifndef _WIN32
   /* Refused without waiting for a writer */
   CHECK(!mkfifo(tmp("fifo"), 0600));
   CHECK(core_trust_load_file(u, tmp("fifo")) == CORE_TRUST_LOAD_UNREADABLE);
   CHECK(!path_is_valid(tmp("fifo.bad")));
   filestream_delete(tmp("fifo"));

   /* Can't be looked at is not missing */
   strlcpy(dir, tmp("locked"), sizeof(dir));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(path, dir, "core_trust", sizeof(path));
   CHECK(filestream_write_file(path, junk, sizeof(junk)));
   if (geteuid() == 0)
      printf("note: running as root, unsearchable folder not checked\n");
   else
   {
      core_trust_free(u);
      u = core_trust_new();
      CHECK(!chmod(dir, 0600));
      CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_UNREADABLE);
      CHECK(!chmod(dir, 0700));
      if ((w = core_trust_write_begin(u)))
      {
         core_trust_write_end(u, w, false);
         CHECK(!"an unsearchable store is writable");
      }
      CHECK(file_is(path, junk, sizeof(junk)));
   }
   {
      struct stat st;
      core_trust_free(u);
      u = core_trust_new();
      CHECK(!symlink("loop_b", tmp("loop_a")));
      CHECK(!symlink("loop_a", tmp("loop_b")));
      CHECK(core_trust_load_file(u, tmp("loop_a"))
            == CORE_TRUST_LOAD_UNREADABLE);
      if ((w = core_trust_write_begin(u)))
      {
         core_trust_write_end(u, w, false);
         CHECK(!"a store behind a symlink loop is writable");
      }
      CHECK(!lstat(tmp("loop_a"), &st) && S_ISLNK(st.st_mode));
      CHECK(!path_is_valid(tmp("loop_a.bad")));
   }
#endif

   core_trust_free(u);
}

static void test_trust_file(void)
{
   core_trust_t *t = core_trust_new();
   core_trust_t *u = core_trust_new();

   test_trust_file_write(t, u);
   test_trust_file_kept(t, u);
   core_trust_free(u);
   test_trust_file_unreadable();
   core_trust_free(t);
}

static void put_core(const char *core, const uint8_t *sig, size_t sig_len)
{
   char sig_path[256];
   strlcpy(sig_path, core, sizeof(sig_path));
   strlcat(sig_path, ".sig", sizeof(sig_path));
   CHECK(filestream_write_file_atomic(core, fx_core, FX_CORE_SIZE));
   CHECK(filestream_write_file(sig_path, sig, (int64_t)sig_len));
}

static enum core_check_result check_path(core_trust_t *t, const char *path,
      unsigned flags, struct core_check *c)
{
   core_trust_set_t *s = core_trust_get(t);
   enum core_check_result r = core_check_file(s, fx_keys, 2, path, flags, c);
   core_trust_set_put(s);
   return r;
}

static void test_newest_folders(void)
{
   static const char *const same[] = {
      TMP_DIR "/one/./" FX_NAME, "./" TMP_DIR "/one/" FX_NAME,
      TMP_DIR "//one//" FX_NAME, TMP_DIR "/two/../one/" FX_NAME };
#ifdef _WIN32
   static const char *const spelt[] = {
      TMP_DIR "\\ONE\\" FX_NAME, TMP_DIR "\\one/" FX_NAME,
      "./CORE_signature_test_tmp/One\\\\" FX_NAME };
#endif
   core_trust_t *t = core_trust_new();
   struct core_check c;
   uint8_t key[CORE_SHA256_LEN], other[CORE_SHA256_LEN];
   char one[256], two[256];
   size_t i;
   CHECK(path_mkdir(tmp("one")) && path_mkdir(tmp("two")));
   /* One key per folder, however the path spells it */
   CHECK(core_folder_key(tmp("one/" FX_NAME), key));
   for (i = 0; i < sizeof(same) / sizeof(same[0]); i++)
      CHECK(     core_folder_key(same[i], other)
            && !memcmp(key, other, sizeof(key)));
#ifdef _WIN32
   for (i = 0; i < sizeof(spelt) / sizeof(spelt[0]); i++)
      CHECK(     core_folder_key(spelt[i], other)
            && !memcmp(key, other, sizeof(key)));
#else
   CHECK(!core_folder_key(tmp("nowhere/" FX_NAME), other));
#endif
   CHECK(core_folder_key(tmp("two/" FX_NAME), other));
   CHECK(memcmp(key, other, sizeof(key)));
   CHECK(core_folder_key(tmp(FX_NAME), other));
   CHECK(memcmp(key, other, sizeof(key)));
   /* Android: one key for every folder */
   CHECK(core_folder_key_with(tmp("two/" FX_NAME), true, other));
   CHECK(memcmp(key, other, sizeof(key)));
   CHECK(core_folder_key_with(tmp("one/" FX_NAME), false, key));
   CHECK(core_folder_key_with(tmp("two/" FX_NAME), false, other));
   CHECK(!memcmp(key, other, sizeof(key)));
   /* which needs no folder found */
   CHECK(core_folder_key_with(tmp("nowhere/" FX_NAME), false, other));
   CHECK(!memcmp(key, other, sizeof(key)));
   strlcpy(one, tmp("one/" FX_NAME), sizeof(one));
   strlcpy(two, tmp("two/" FX_NAME), sizeof(two));
   put_core(one, fx_sig_good, sizeof(fx_sig_good));
   CHECK(check_path(t, one, 0, &c) == CORE_CHECK_OK);
   CHECK(core_trust_apply(t, &c));
   /* Another install's folder keeps its own */
   put_core(two, fx_sig_older, sizeof(fx_sig_older));
   CHECK(check_path(t, two, 0, &c) == CORE_CHECK_OK);
   /* The same folder, however spelled, is still older */
   put_core(one, fx_sig_older, sizeof(fx_sig_older));
   for (i = 0; i < sizeof(same) / sizeof(same[0]); i++)
      CHECK(check_path(t, same[i], 0, &c) == CORE_CHECK_OLDER);
   core_trust_free(t);
}

static bool save(core_trust_t *t, const char *path)
{
   core_trust_write_t *w = core_trust_write_begin(t);
   bool ok;
   if (!w)
      return false;
   ok = core_trust_write_run(w, path);
   core_trust_write_end(t, w, ok);
   return ok;
}

static void test_trust_merge(void)
{
   core_trust_t *t = core_trust_new();
   core_trust_t *a = core_trust_new();
   core_trust_t *b = core_trust_new();
   core_trust_t *v = core_trust_new();
   core_trust_set_t *s;
   struct core_check c;
   uint8_t h[CORE_SHA256_LEN];
   uint8_t *buf;
   size_t len;
   char path[256];
   strlcpy(path, tmp("merge_trust"), sizeof(path));
   /* B turns the check on while A, started with it off, runs on */
   CHECK(core_trust_set_flag(t, CORE_TRUST_CHECK_OFF, true));
   CHECK(core_trust_set_flag(t, CORE_TRUST_SEEDED, true));
   CHECK(save(t, path));
   CHECK(core_trust_load_file(a, path) == CORE_TRUST_LOAD_OK);
   CHECK(core_trust_load_file(b, path) == CORE_TRUST_LOAD_OK);
   CHECK(core_trust_set_device(a, &fx_dev, NULL));
   CHECK(core_trust_set_device(b, &fx_dev, NULL));
   CHECK(core_trust_set_flag(b, CORE_TRUST_CHECK_OFF, false));
   CHECK(save(b, path));
   check_of(&c, "a_libretro.so", 11, 0x11, 0);
   CHECK(core_trust_apply(a, &c));
   CHECK(save(a, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(v);
   CHECK(core_trust_flags(s) == CORE_TRUST_SEEDED);
   CHECK(core_trust_has_identity(s, &c.rec.id, "a_libretro.so"));
   core_trust_set_put(s);
   core_trust_free(a);
   core_trust_free(b);

   /* The newest date is the larger, and trusted hashes are the union */
   a = core_trust_new();
   b = core_trust_new();
   CHECK(core_trust_load_file(a, path) == CORE_TRUST_LOAD_OK);
   CHECK(core_trust_load_file(b, path) == CORE_TRUST_LOAD_OK);
   CHECK(core_trust_set_device(a, &fx_dev, NULL));
   CHECK(core_trust_set_device(b, &fx_dev, NULL));
   check_of(&c, "x_libretro.so", 21, 0x21, 200);
   CHECK(core_trust_apply(a, &c));
   CHECK(core_trust_always(a, &c));
   check_of(&c, "x_libretro.so", 22, 0x22, 100);
   CHECK(core_trust_apply(b, &c));
   CHECK(core_trust_always(b, &c));
   check_of(&c, "y_libretro.so", 23, 0x23, 50);
   CHECK(core_trust_apply(b, &c));
   CHECK(save(a, path));
   CHECK(save(b, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(v);
   CHECK(core_trust_newest(s, nowhere, "x_libretro.so") == 200);
   CHECK(core_trust_newest(s, nowhere, "y_libretro.so") == 50);
   memset(h, 0x21, sizeof(h));
   CHECK(core_trust_has_hash(s, h));
   memset(h, 0x22, sizeof(h));
   CHECK(core_trust_has_hash(s, h));
   check_of(&c, "x_libretro.so", 21, 0x21, 200);
   CHECK(core_trust_has_identity(s, &c.rec.id, "x_libretro.so"));
   check_of(&c, "x_libretro.so", 22, 0x22, 100);
   CHECK(core_trust_has_identity(s, &c.rec.id, "x_libretro.so"));
   core_trust_set_put(s);
   check_of(&c, "x_libretro.so", 24, 0x24, 300);
   CHECK(core_trust_apply(a, &c));
   CHECK(save(a, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(v);
   CHECK(core_trust_newest(s, nowhere, "x_libretro.so") == 300);
   CHECK(core_trust_newest(s, nowhere, "y_libretro.so") == 50);
   memset(h, 0x22, sizeof(h));
   CHECK(core_trust_has_hash(s, h));
   core_trust_set_put(s);

   /* A newer version's file found at save time is left as it is */
   CHECK((buf = core_trust_serialize(v, &len)) != NULL);
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '2';
   CHECK(filestream_write_file(path, buf, (int64_t)len));
   CHECK(core_trust_set_flag(a, CORE_TRUST_NOTICE_SHOWN, true));
   CHECK(!save(a, path));
   CHECK(file_is(path, buf, len));
   /* and what was not written goes with the next write */
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '1';
   CHECK(filestream_write_file(path, buf, (int64_t)len));
   free(buf);
   CHECK(save(a, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(v);
   CHECK(core_trust_flags(s)
         == (CORE_TRUST_SEEDED | CORE_TRUST_NOTICE_SHOWN));
   CHECK(core_trust_newest(s, nowhere, "y_libretro.so") == 50);
   core_trust_set_put(s);

   core_trust_free(t);
   core_trust_free(a);
   core_trust_free(b);
   core_trust_free(v);
}

/* A save that finds no room on disk for a hash keeps it for the next */
static void test_trust_merge_full(void)
{
   core_trust_t *a = core_trust_new();
   core_trust_t *b = core_trust_new();
   core_trust_t *v = core_trust_new();
   core_trust_set_t *s;
   struct core_check c;
   char path[256];
   unsigned i;
   strlcpy(path, tmp("full_trust"), sizeof(path));
   CHECK(save(a, path));
   CHECK(core_trust_load_file(b, path) == CORE_TRUST_LOAD_OK);
   /* A fills the trusted hashes and the newest list */
   for (i = 0; i < CORE_TRUST_MAX_HASHES; i++)
   {
      check_of(&c, "a_libretro.so", 100 + i, 0xa0, 0);
      c.rec.sha256[1] = (uint8_t)i;
      CHECK(core_trust_always(a, &c));
   }
   for (i = 0; i < CORE_TRUST_MAX_NEWEST; i++)
   {
      check_of(&c, "a_libretro.so", 1, 0xa0, 1);
      c.folder[0] = (uint8_t)(i + 1);
      c.folder[1] = (uint8_t)((i + 1) >> 8);
      c.record    = false;
      CHECK(core_trust_apply(a, &c));
   }
   CHECK(save(a, path));
   /* B's Always Trust finds no room; the rest of B is saved */
   check_of(&c, "b_libretro.so", 2, 0xb0, 7);
   CHECK(core_trust_apply(b, &c));
   CHECK(core_trust_always(b, &c));
   CHECK(core_trust_set_flag(b, CORE_TRUST_NOTICE_SHOWN, true));
   CHECK(save(b, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(v);
   CHECK(core_trust_flags(s) == CORE_TRUST_NOTICE_SHOWN);
   CHECK(!core_trust_has_hash(s, c.rec.sha256));
   CHECK(core_trust_newest(s, nowhere, "b_libretro.so") == 7);
   /* All as old: the lowest key made the room */
   memset(c.folder, 0, sizeof(c.folder));
   c.folder[1] = 1;
   CHECK(core_trust_newest(s, c.folder, "a_libretro.so") == 0);
   c.folder[0] = 1;
   c.folder[1] = 0;
   CHECK(core_trust_newest(s, c.folder, "a_libretro.so") == 1);
   core_trust_set_put(s);
   /* Another instance sets a damaged file aside: B's next save has room
    * for the hash, and raises nothing it saved before */
   CHECK(filestream_write_file(path, "junk", 4));
   core_trust_free(a);
   a = core_trust_new();
   CHECK(core_trust_load_file(a, path) == CORE_TRUST_LOAD_RESET);
   CHECK(save(a, path));
   CHECK(core_trust_set_flag(b, CORE_TRUST_SEEDED, true));
   CHECK(save(b, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   s = core_trust_get(v);
   CHECK(core_trust_has_hash(s, c.rec.sha256));
   CHECK(core_trust_newest(s, nowhere, "b_libretro.so") == 0);
   core_trust_set_put(s);
   core_trust_free(a);
   core_trust_free(b);
   core_trust_free(v);
}

/* Two instances at the cap: each save raises only its own, so neither
 * brings back what the other's evicted */
static void test_trust_merge_cap(void)
{
   core_trust_t *a = core_trust_new();
   core_trust_t *b = core_trust_new();
   core_trust_t *v = core_trust_new();
   core_trust_set_t *s;
   struct core_check c;
   char path[256];
   void *settled  = NULL;
   int64_t len    = 0;
   unsigned i, round, lost_a, lost_b;
   strlcpy(path, tmp("cap_trust"), sizeof(path));
   for (i = 0; i < CORE_TRUST_MAX_NEWEST; i++)
   {
      newest_of(&c, 0x800000UL + i, 0, 100 + i);
      CHECK(core_trust_apply(a, &c));
   }
   CHECK(save(a, path));
   CHECK(core_trust_load_file(b, path) == CORE_TRUST_LOAD_OK);
   for (i = 0; i < 50; i++)
   {
      newest_of(&c, 0x200000UL + i, 0, 500);
      CHECK(core_trust_apply(a, &c));
      newest_of(&c, 0x300000UL + i, 0, 600);
      CHECK(core_trust_apply(b, &c));
   }
   for (round = 0; round < 4; round++)
   {
      CHECK(save((round & 1) ? b : a, path));
      CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
      s = core_trust_get(v);
      for (i = lost_a = lost_b = 0; i < 50; i++)
      {
         lost_a += newest_is(s, 0x200000UL + i, 0) != 500;
         lost_b += newest_is(s, 0x300000UL + i, 0) != 600;
      }
      core_trust_set_put(s);
      CHECK(lost_a == 0);
      CHECK(lost_b == (round ? 0 : 50));
      /* Saves with nothing new of their own change nothing */
      if (round == 1)
         CHECK(filestream_read_file(path, &settled, &len));
      else if (round > 1)
         CHECK(settled && file_is(path, settled, (size_t)len));
   }
   free(settled);
   core_trust_free(a);
   core_trust_free(b);
   core_trust_free(v);
}

static int RETRO_CALLCONV refuse_rename(const char *old_path,
      const char *new_path)
{
   (void)old_path;
   (void)new_path;
   return -1;
}

static void test_trust_temp(void)
{
   static struct retro_vfs_interface iface;
   struct retro_vfs_interface_info info;
   core_trust_t *a = core_trust_new();
   core_trust_t *b = core_trust_new();
   core_trust_t *v = core_trust_new();
   struct core_check c;
   char dir[256];
   char path[256];
   char other[256];
   char name[256];
   void *old       = NULL;
   int64_t old_len = 0;
   uint8_t *buf;
   size_t len;
   RFILE *file;

   CHECK(core_trust_temp_name(other, sizeof(other), "x", 1, 5));
   CHECK(core_trust_temp_name(name, sizeof(name), "x", 2, 5));
   CHECK(strcmp(other, name) != 0);
   CHECK(core_trust_temp_name(name, sizeof(name), "x", 1, 6));
   CHECK(strcmp(other, name) != 0);
   CHECK(strcmp(other, "x.tmp") != 0 && string_starts_with(other, "x."));
   CHECK(!core_trust_temp_name(name, 4, "x", 1, 5));

   strlcpy(dir, tmp("race"), sizeof(dir));
   CHECK(path_mkdir(dir));
   fill_pathname_join_special(path, dir, "core_trust", sizeof(path));
   CHECK(core_trust_set_flag(a, CORE_TRUST_SEEDED, true));
   CHECK(save(a, path));
   CHECK(core_trust_load_file(b, path) == CORE_TRUST_LOAD_OK);
   check_of(&c, "a_libretro.so", 31, 0x31, 100);
   CHECK(core_trust_apply(a, &c));
   check_of(&c, "b_libretro.so", 32, 0x32, 200);
   CHECK(core_trust_apply(b, &c));

   /* B, another process, has all but its last byte in its temporary
    * when A saves; then B's rename lands */
   CHECK(core_trust_temp_name(other, sizeof(other), path, 0, 0));
   CHECK((buf = core_trust_serialize(b, &len)) != NULL);
   CHECK((file = filestream_open(other, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)) != NULL);
   if (file)
   {
      CHECK(filestream_write(file, buf, (int64_t)len - 1)
            == (int64_t)len - 1);
      CHECK(!filestream_flush(file));
      CHECK(save(a, path));
      CHECK(filestream_write(file, buf + len - 1, 1) == 1);
      CHECK(!filestream_close(file));
   }
   CHECK(!filestream_rename(other, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   CHECK(file_is(path, buf, len));
   free(buf);

   /* Not the shared name: a folder there doesn't stop a save */
   fill_pathname_join_special(name, dir, "core_trust.tmp", sizeof(name));
   CHECK(path_mkdir(name));
   CHECK(save(a, path));
   CHECK(path_is_directory(name));
   CHECK(path_rmdir(name));

   /* A failed write takes its temporary with it, and only that */
   CHECK(filestream_read_file(path, &old, &old_len));
   CHECK(core_trust_set_flag(a, CORE_TRUST_NOTICE_SHOWN, true));
   iface.rename                    = refuse_rename;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = &iface;
   filestream_vfs_init(&info);
   CHECK(!save(a, path));
   info.iface = NULL;
   filestream_vfs_init(&info);
   CHECK(file_is(path, old, (size_t)old_len));
   free(old);
   /* Nothing is left beside the store */
   CHECK(!filestream_delete(path));
   CHECK(path_rmdir(dir));

   core_trust_free(a);
   core_trust_free(b);
   core_trust_free(v);
}

static void test_record_older(void)
{
   core_trust_t *t = real_store();
   core_trust_t *u = real_store();
   core_trust_set_t *s;
   core_file_t *f;
   struct core_check c, c2;
   uint8_t core_sha[CORE_SHA256_LEN];
   uint8_t *buf;
   size_t len;
   char core[256];
   bool kept;
   sha(fx_core, FX_CORE_SIZE, core_sha);
   CHECK(path_mkdir(tmp("three")));
   strlcpy(core, tmp("three/" FX_NAME), sizeof(core));
   put_core(core, fx_sig_older, sizeof(fx_sig_older));
   kept = records_for(core);
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_OK);
   CHECK(core_trust_apply(t, &c));
   /* A newer build seen in the same folder since */
   s = core_trust_get(t);
   memset(&c2, 0, sizeof(c2));
   strlcpy(c2.rec.name, FX_NAME, sizeof(c2.rec.name));
   CHECK(core_folder_key(core, c2.folder));
   c2.rec.id.dev = 99;
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_good,
         sizeof(fx_sig_good), 0, &c2);
   core_trust_set_put(s);
   CHECK(c2.result == CORE_CHECK_OK);
   CHECK(core_trust_apply(t, &c2));
   /* so the record no longer answers alone */
   s = core_trust_get(t);
   CHECK((f = core_file_open(core)) != NULL);
   CHECK(!core_check_quick(s, f, core, &c));
   core_file_close(f);
   core_trust_set_put(s);
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_OLDER);

   /* A restored backup's record still answers, where there are records */
   put_core(core, fx_sig_older, sizeof(fx_sig_older));
   CHECK(check_path(t, core, CORE_CHECK_ALLOW_OLDER, &c) == CORE_CHECK_OK);
   CHECK(core_trust_apply(t, &c));
   CHECK(check_path(t, core, 0, &c)
         == (kept ? CORE_CHECK_OK : CORE_CHECK_OLDER) && !c.record);
   CHECK((buf = core_trust_serialize(t, &len)) != NULL);
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_OK);
   free(buf);
   CHECK(check_path(u, core, 0, &c)
         == (kept ? CORE_CHECK_OK : CORE_CHECK_OLDER) && !c.record);
   /* So does Always Trust's; without records its hash does */
   put_core(core, fx_sig_older, sizeof(fx_sig_older));
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_OLDER);
   CHECK(core_trust_always(t, &c));
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_OK && c.record == !kept);

   /* Two checks in flight: the older one is not recorded */
   core_trust_free(t);
   t = fx_store();
   s = core_trust_get(t);
   memset(&c, 0, sizeof(c));
   strlcpy(c.rec.name, FX_NAME, sizeof(c.rec.name));
   c.rec.id.dev        = fx_dev;
   c.rec.id.ctime_kept = true;
   c2 = c;
   c.rec.id.ino[0]  = 1;
   c2.rec.id.ino[0] = 2;
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_good,
         sizeof(fx_sig_good), 0, &c);
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_older,
         sizeof(fx_sig_older), 0, &c2);
   core_trust_set_put(s);
   CHECK(c.result == CORE_CHECK_OK && c2.result == CORE_CHECK_OK);
   CHECK(core_trust_apply(t, &c));
   CHECK(!core_trust_apply(t, &c2));
   s = core_trust_get(t);
   CHECK(!core_trust_has_identity(s, &c2.rec.id, FX_NAME));
   /* Unless it is a restore */
   c2.rec.id.ino[0] = 3;
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_older,
         sizeof(fx_sig_older), CORE_CHECK_ALLOW_OLDER, &c2);
   core_trust_set_put(s);
   CHECK(core_trust_apply(t, &c2));
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c2.rec.id, FX_NAME));
   core_trust_set_put(s);
   core_trust_free(t);
   core_trust_free(u);
}

static void test_ctime_kept(void)
{
   core_trust_t *t = core_trust_new();
   core_trust_set_t *s;
   core_file_t *f;
   struct core_check c, c2;
   uint8_t core_sha[CORE_SHA256_LEN];
   char core[256];
   bool kept;
   sha(fx_core, FX_CORE_SIZE, core_sha);
   CHECK(path_mkdir(tmp("four")));
   strlcpy(core, tmp("four/" FX_NAME), sizeof(core));
   put_core(core, fx_sig_good, sizeof(fx_sig_good));
   s = core_trust_get(t);
   CHECK((f = core_file_open(core)) != NULL);
   CHECK(!core_check_quick(s, f, core, &c));
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_good,
         sizeof(fx_sig_good), 0, &c);
   core_trust_set_put(s);
   CHECK(c.result == CORE_CHECK_OK && c.record);
   kept = c.rec.id.ctime_kept;
   /* The store on its device, so only ctime decides */
   CHECK(core_trust_set_device(t, &c.rec.id.dev, NULL));
   /* Marked as on a filesystem that doesn't keep ctime: no record, but
    * the newest build and Always Trust's hash still count */
   c.rec.id.ctime_kept = false;
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_always(t, &c));
   s = core_trust_get(t);
   CHECK(!core_trust_has_identity(s, &c.rec.id, FX_NAME));
   CHECK(core_trust_newest(s, c.folder, FX_NAME) == FX_TIME);
   CHECK(core_trust_has_hash(s, core_sha));
   core_trust_set_put(s);
   /* nor is a record honoured for one */
   core_trust_free(t);
   t = core_trust_new();
   CHECK(core_trust_set_device(t, &c.rec.id.dev, NULL));
   c.rec.id.ctime_kept = true;
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(core_check_record(s, &c));
   c.rec.id.ctime_kept = false;
   CHECK(!core_check_record(s, &c));
   /* This file, as its own filesystem has it */
   CHECK(core_check_quick(s, f, core, &c2) == kept);
   core_file_close(f);
   core_trust_set_put(s);
   core_trust_free(t);
}

/* Records only of files on the store's own filesystem */
static void test_store_device(void)
{
   core_trust_t *t = core_trust_new();
   core_trust_set_t *s;
   core_file_t *f;
   struct core_check c, c2;
   uint64_t other;
   char core[256];
   bool kept;
   /* None with no device known */
   check_of(&c, "d_libretro.so", 1, 0xdd, 10);
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_always(t, &c));
   s = core_trust_get(t);
   CHECK(!core_trust_has_identity(s, &c.rec.id, c.rec.name));
   CHECK(core_trust_has_hash(s, c.rec.sha256));
   CHECK(core_trust_newest(s, nowhere, c.rec.name) == 10);
   core_trust_set_put(s);
   core_trust_free(t);
   /* as when the store's folder can't be found */
   t = core_trust_new();
   CHECK(core_trust_load_file(t, tmp("nowhere/core_trust"))
         == CORE_TRUST_LOAD_MISSING);
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(!core_trust_has_identity(s, &c.rec.id, c.rec.name));
   core_trust_set_put(s);
   core_trust_free(t);

   /* A core beside the store keeps its record */
   CHECK(path_mkdir(tmp("five")));
   strlcpy(core, tmp("five/" FX_NAME), sizeof(core));
   put_core(core, fx_sig_good, sizeof(fx_sig_good));
   kept = records_for(core);
   t    = real_store();
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_OK && c.record);
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c.rec.id, FX_NAME) == kept);
   CHECK(core_check_record(s, &c) == kept);
   core_trust_set_put(s);
   /* The store on another device: not honoured */
   other = c.rec.id.dev + 1;
   CHECK(core_trust_set_device(t, &other, NULL));
   s = core_trust_get(t);
   CHECK(!core_check_record(s, &c));
   CHECK((f = core_file_open(core)) != NULL);
   CHECK(!core_check_quick(s, f, core, &c2));
   core_file_close(f);
   core_trust_set_put(s);
   core_trust_free(t);
   /* nor stored, but the newest build and the hash count */
   t = core_trust_new();
   CHECK(core_trust_set_device(t, &other, NULL));
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_always(t, &c));
   s = core_trust_get(t);
   CHECK(!core_trust_has_identity(s, &c.rec.id, FX_NAME));
   CHECK(core_trust_newest(s, c.folder, FX_NAME) == FX_TIME);
   CHECK(core_trust_has_hash(s, c.rec.sha256));
   core_trust_set_put(s);
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_OK && c.record);
   core_trust_free(t);
}

/* and of files on RetroArch's own */
static void test_self_device(void)
{
   static const uint64_t store = 2, self = 3, other = 4;
   core_trust_t *t = core_trust_new();
   core_trust_set_t *s;
   struct core_check c, d;
   struct core_self me;
   bool known;
   /* The store on one device and RetroArch on another: a core on
    * RetroArch's keeps its record */
   CHECK(core_trust_set_device(t, &store, &self));
   check_of(&c, "e_libretro.so", 1, 0xee, 10);
   check_of(&d, "f_libretro.so", 2, 0xff, 10);
   c.rec.id.dev = self;
   d.rec.id.dev = self;
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_always(t, &d));
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c.rec.id, c.rec.name));
   CHECK(core_check_record(s, &c));
   CHECK(core_check_record(s, &d));
   core_trust_set_put(s);
   /* not once RetroArch's is unknown */
   CHECK(core_trust_set_device(t, &store, NULL));
   s = core_trust_get(t);
   CHECK(!core_check_record(s, &c));
   CHECK(!core_check_record(s, &d));
   core_trust_set_put(s);
   core_trust_free(t);
   /* nor made then, nor for a core on neither device */
   t = core_trust_new();
   CHECK(core_trust_set_device(t, &store, NULL));
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_set_device(t, &store, &self));
   d.rec.id.dev = other;
   CHECK(core_trust_apply(t, &d));
   CHECK(core_trust_always(t, &d));
   s = core_trust_get(t);
   CHECK(!core_trust_has_identity(s, &c.rec.id, c.rec.name));
   CHECK(!core_trust_has_identity(s, &d.rec.id, d.rec.name));
   CHECK(!core_check_record(s, &d));
   CHECK(core_trust_has_hash(s, d.rec.sha256));
   core_trust_set_put(s);
   core_trust_free(t);

   /* core_trust_load_file finds RetroArch's own where it can */
   known = core_self_find(&me);
   t     = core_trust_new();
   CHECK(core_trust_load_file(t, tmp("nowhere/core_trust"))
         == CORE_TRUST_LOAD_MISSING);
   c.rec.id.dev = known ? me.dev : self;
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c.rec.id, c.rec.name) == known);
   core_trust_set_put(s);
   core_trust_free(t);
}

static void test_owner(void)
{
   static const unsigned long groups[] = { 20, 1000 };
   struct core_owner me;
   me.gids     = groups;
   me.num_gids = 2;
   me.uid      = 1000;
   me.overflow_uid = 65534;
   CHECK( core_mode_writable(&me, 1000, 0, 0444, false));
   CHECK(!core_mode_writable(&me, 1000, 0, 0644, true));
   CHECK(!core_mode_writable(&me, 0, 0, 0755, false));
   CHECK( core_mode_writable(&me, 0, 0, 0757, false));
   CHECK( core_mode_writable(&me, 0, 20, 0775, false));
   CHECK(!core_mode_writable(&me, 0, 30, 0775, false));
   CHECK(!core_mode_writable(&me, 0, 20, 0755, false));
   me.uid = 0;
   CHECK( core_mode_writable(&me, 1000, 0, 0444, false));

   /* Exempt: not ours to write, and root's or RetroArch's owner's */
   me.uid = 1000;
   CHECK( core_mode_exempt(&me, 1000, 0, 0, 0755, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 0, 0757, false));
   CHECK(!core_mode_exempt(&me, 1000, 1000, 0, 0555, false));
   /* Another account's, in its own 0755 folders */
   CHECK(!core_mode_exempt(&me, 1000, 1001, 1001, 0644, false));
   CHECK(!core_mode_exempt(&me, 1000, 1001, 1001, 0755, false));
   CHECK(!core_mode_exempt(&me, 0, 1001, 1001, 0755, false));
   /* Linuxbrew: that account owns RetroArch too */
   CHECK( core_mode_exempt(&me, 1001, 1001, 1001, 0755, false));
   /* Any owner on a read-only mount, as Flatpak's / */
   CHECK( core_mode_exempt(&me, 1000, 1000, 1000, 0755, true));
   CHECK( core_mode_exempt(&me, 0, 1001, 1001, 0644, true));
   /* No group write, but for root's group on Linux and Android, even
    * when not ours; elsewhere gid 0 is wheel, admin users */
   CHECK(!core_mode_exempt(&me, 1000, 0, 30, 0775, false));
   CHECK(!core_mode_exempt(&me, 0, 0, 30, 0775, false));
   CHECK(!core_mode_exempt(&me, 1001, 1001, 1001, 0775, false));
   CHECK(!core_mode_exempt(&me, 1001, 1001, 30, 0664, false));
#if defined(__linux__) || defined(ANDROID)
   CHECK( core_mode_exempt(&me, 1000, 0, 0, 0775, false));
   CHECK( core_mode_exempt(&me, 1001, 1001, 0, 0775, false));
   CHECK( core_mode_exempt(&me, 1000, 0, 0, 0100664, false));
#else
   CHECK(!core_mode_exempt(&me, 1000, 0, 0, 0775, false));
   CHECK(!core_mode_exempt(&me, 1001, 1001, 0, 0775, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 0, 0100664, false));
#endif
   CHECK( core_mode_exempt(&me, 1000, 0, 0, 041775, false));
   CHECK( core_mode_exempt(&me, 1000, 0, 30, 0755, false));
   CHECK( core_mode_exempt(&me, 1000, 0, 30, 0775, true));
   CHECK( core_mode_exempt(&me, 1001, 1001, 1001, 0775, true));
   /* A sticky folder (040000 folder, 01000 sticky) may have group write:
    * multi-user Nix's /nix/store, root:nixbld 1775, and what's below */
   CHECK( core_mode_exempt(&me, 1000, 0, 30000, 041775, false));
   CHECK( core_mode_exempt(&me, 0, 0, 30000, 041775, false));
   CHECK( core_mode_exempt(&me, 1001, 1001, 30, 043775, false));
   CHECK( core_mode_exempt(&me, 1000, 0, 0, 040555, false));
   CHECK( core_mode_exempt(&me, 1000, 0, 0, 0100444, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 30000, 040775, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 30000, 042775, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 30000, 0100664, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 30000, 0101775, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 30000, 01775, false));
   /* Still not ours to write, nor anyone's */
   CHECK(!core_mode_exempt(&me, 1000, 0, 20, 041775, false));
   CHECK(!core_mode_exempt(&me, 1000, 0, 30000, 041777, false));
   CHECK(!core_mode_exempt(&me, 1000, 1001, 30000, 041775, false));
   /* In a user namespace, host root and every unmapped account show as
    * the overflow uid: never root, nor RetroArch's owner */
   CHECK(!core_mode_exempt(&me, 65534, 65534, 65534, 0100644, false));
   CHECK(!core_mode_exempt(&me, 65534, 65534, 65534, 040755, false));
   CHECK(!core_mode_exempt(&me, 65534, 65534, 0, 040755, false));
   CHECK( core_mode_exempt(&me, 65534, 0, 0, 040755, false));
   CHECK( core_mode_exempt(&me, 65534, 0, 0, 0100444, false));
   CHECK(!core_mode_exempt(&me, 1000, 65534, 65534, 0100644, false));
   CHECK( core_mode_exempt(&me, 65534, 65534, 65534, 0100644, true));
   me.overflow_uid = 4242;
   CHECK(!core_mode_exempt(&me, 4242, 4242, 4242, 0100644, false));
   CHECK( core_mode_exempt(&me, 65534, 65534, 65534, 0100644, false));
   me.overflow_uid = 0;
   CHECK(!core_mode_exempt(&me, 1000, 0, 0, 040755, false));
   /* Not known: any owner but root could be it */
   me.overflow_uid = CORE_UID_UNKNOWN;
   CHECK(!core_mode_exempt(&me, 1001, 1001, 1001, 0100644, false));
   CHECK(!core_mode_exempt(&me, 1001, 1001, 1001, 040755, false));
   CHECK(!core_mode_exempt(&me, 65534, 65534, 65534, 0100644, false));
   CHECK( core_mode_exempt(&me, 1001, 0, 0, 040755, false));
   CHECK( core_mode_exempt(&me, 65534, 0, 0, 0100444, false));
   CHECK( core_mode_exempt(&me, 1001, 1001, 1001, 0100644, true));
}

static void test_check_finish(core_trust_t *t)
{
   uint8_t core_sha[CORE_SHA256_LEN];
   uint8_t other_sha[CORE_SHA256_LEN];
   core_trust_set_t *s;
   struct core_check c;

   sha(fx_core, FX_CORE_SIZE, core_sha);

   /* core_check_finish's order, from buffers */
   s = core_trust_get(t);
   memset(&c, 0, sizeof(c));
   strlcpy(c.rec.name, FX_NAME, sizeof(c.rec.name));
   CHECK(core_folder_key(tmp(FX_NAME), c.folder));
   core_check_finish(s, fx_keys, 2, core_sha, NULL, 0, 0, &c);
   CHECK(c.result == CORE_CHECK_UNSIGNED && !c.record);
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_good, 0, 0, &c);
   CHECK(c.result == CORE_CHECK_UNSIGNED && !c.record);
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_good,
         sizeof(fx_sig_good) - 1, 0, &c);
   CHECK(c.result == CORE_CHECK_MODIFIED && !c.record);
   core_check_finish(s, fx_keys, 1, core_sha, fx_sig_key2,
         sizeof(fx_sig_key2), 0, &c);
   CHECK(c.result == CORE_CHECK_UNKNOWN_KEY && !c.record);
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_good,
         sizeof(fx_sig_good), 0, &c);
   CHECK(c.result == CORE_CHECK_OK && c.record && c.raise_newest);
   CHECK(c.rec.build_time == FX_TIME);
   core_trust_set_put(s);
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_older,
         sizeof(fx_sig_older), 0, &c);
   CHECK(c.result == CORE_CHECK_OLDER && !c.record);
   /* Restoring RetroArch's own backup */
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_older,
         sizeof(fx_sig_older), CORE_CHECK_ALLOW_OLDER, &c);
   CHECK(c.result == CORE_CHECK_OK && c.record);
   /* The same build again is not older */
   core_check_finish(s, fx_keys, 2, core_sha, fx_sig_good,
         sizeof(fx_sig_good), 0, &c);
   CHECK(c.result == CORE_CHECK_OK);
   core_trust_set_put(s);
   /* A trusted hash goes before any .sig */
   sha(fx_core, FX_CORE_SIZE - 1, other_sha);
   memcpy(c.rec.sha256, other_sha, CORE_SHA256_LEN);
   CHECK(core_trust_always(t, &c));
   s = core_trust_get(t);
   core_check_finish(s, fx_keys, 2, other_sha, (const uint8_t*)"junk", 4,
         0, &c);
   CHECK(c.result == CORE_CHECK_OK && c.record);
   core_trust_set_put(s);
}

static void test_check_files(core_trust_t *t)
{
   core_trust_set_t *s;
   struct core_check c;
#ifndef _WIN32
   core_file_t *f;
#endif
   char core[256];
   char sig_path[256];

   /* Whole checks of real files */
   strlcpy(core, tmp(FX_NAME), sizeof(core));
   strlcpy(sig_path, core, sizeof(sig_path));
   strlcat(sig_path, ".sig", sizeof(sig_path));
   CHECK(filestream_write_file(core, fx_core, FX_CORE_SIZE));
   filestream_delete(sig_path);
   s = core_trust_get(t);
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_UNSIGNED);
   /* An empty .sig is no .sig */
   CHECK(filestream_write_file(sig_path, "", 0));
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_UNSIGNED);
   filestream_delete(sig_path);
#ifndef _WIN32
   /* A FIFO .sig must not wait for a writer */
   CHECK(!mkfifo(sig_path, 0600));
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_UNSIGNED);
   filestream_delete(sig_path);
#endif
   /* A .sig longer than any valid one is read no further than that */
   memset(edited, 0x5a, sizeof(edited));
   memcpy(edited, fx_sig_good, sizeof(fx_sig_good));
   CHECK(filestream_write_file(sig_path, edited, sizeof(edited)));
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_MODIFIED);
   CHECK(filestream_write_file(sig_path, fx_sig_older, sizeof(fx_sig_older)));
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_OLDER);
   CHECK(filestream_write_file(sig_path, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_OK);
   CHECK(c.record);
   core_trust_set_put(s);
   CHECK(core_trust_apply(t, &c));
   /* The identity record now answers without the .sig, where there
    * are records */
   filestream_delete(sig_path);
   s = core_trust_get(t);
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == (records_for(core)
            ? CORE_CHECK_OK : CORE_CHECK_UNSIGNED));
   CHECK(!c.record);
   core_trust_set_put(s);
   /* Replaced: hashed again, and unsigned */
   fx_core[0] ^= 1;
   CHECK(filestream_write_file_atomic(core, fx_core, FX_CORE_SIZE));
   fx_core[0] ^= 1;
   s = core_trust_get(t);
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_UNSIGNED);
   core_trust_set_put(s);
   /* Always Trust: the hash, wherever the file is */
   CHECK(core_trust_always(t, &c));
   fx_core[0] ^= 1;
   CHECK(filestream_write_file(tmp("copy_libretro.so"), fx_core,
            FX_CORE_SIZE));
   fx_core[0] ^= 1;
   s = core_trust_get(t);
   CHECK(core_check_file(s, fx_keys, 2, tmp("copy_libretro.so"), 0, &c)
         == CORE_CHECK_OK);
   CHECK(c.record);
   core_trust_set_put(s);
   /* Turned off: anything goes, unhashed */
   CHECK(filestream_write_file(sig_path, "junk", 4));
   CHECK(filestream_write_file(core, "x", 1));
   CHECK(core_trust_set_flag(t, CORE_TRUST_CHECK_OFF, true));
   s = core_trust_get(t);
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c) == CORE_CHECK_OK);
   CHECK(!c.record);
   core_trust_set_put(s);
   CHECK(core_trust_set_flag(t, CORE_TRUST_CHECK_OFF, false));
   s = core_trust_get(t);
   CHECK(core_check_file(s, fx_keys, 2, core, 0, &c)
         == CORE_CHECK_MODIFIED);
   /* Could not be checked: an error, not a modified core */
   CHECK(core_check_file(s, fx_keys, 2, tmp("missing.so"), 0, &c)
         == CORE_CHECK_ERROR && !c.record);
#ifndef _WIN32
   /* Its folder gone since it was opened */
   CHECK(path_mkdir(tmp("gone")));
   CHECK(filestream_write_file(tmp("gone/" FX_NAME), fx_core, FX_CORE_SIZE));
   CHECK((f = core_file_open(tmp("gone/" FX_NAME))) != NULL);
   filestream_delete(tmp("gone/" FX_NAME));
   path_rmdir(tmp("gone"));
   CHECK(core_check_quick(s, f, tmp("gone/" FX_NAME), &c)
         && c.result == CORE_CHECK_ERROR && !c.record);
   core_file_close(f);
#endif
   /* and Always Trust has no hash to take */
   check_of(&c, "e_libretro.so", 7, 0xee, 0);
   c.result = CORE_CHECK_ERROR;
   CHECK(!core_trust_always(t, &c));
   core_trust_set_put(s);
}

static void test_check_exempt(core_trust_t *t)
{
   core_trust_set_t *s = core_trust_get(t);
   struct core_identity id;
   struct core_self self;
   core_file_t *f;

   /* Our own files are writable; on Windows nothing is exempt */
   CHECK((f = core_file_open(tmp(FX_NAME))) != NULL);
   CHECK(core_file_identity(f, &id));
   self.dev = id.dev;
   self.uid = 0;
   CHECK(!core_file_unwritable(f, tmp(FX_NAME), &self));
   core_file_close(f);
#ifndef _WIN32
   /* A system file */
   if ((f = core_file_open("/bin/sh")))
   {
      struct core_self mine;
      struct core_check c;

      CHECK(core_file_identity(f, &id));
      /* Only on RetroArch's own filesystem, which must be known */
      self.dev = id.dev + 1;
      CHECK(!core_file_unwritable(f, "/bin/sh", &self));
      CHECK(!core_file_unwritable(f, "/bin/sh", NULL));
      /* Exempt when this user is not root */
      self.dev = id.dev;
      if (!core_file_unwritable(f, "/bin/sh", &self))
         printf("note: /bin/sh is writable here (root?), not checked\n");
      else
      {
         CHECK(core_check_quick(s, f, "/bin/sh", &c)
                  == (core_self_find(&mine) && mine.dev == id.dev)
               && c.result == CORE_CHECK_OK);
      }
      core_file_close(f);
   }
   /* A service account's file, where there is one: exempt only when
    * that account owns RetroArch too */
   {
      static const char *const others[] = { "/var/lib/systemd/timesync/clock",
         "/var/cache/man/index.db", "/usr/bin/at" };
      unsigned found = 0;
      size_t i;
      for (i = 0; i < sizeof(others) / sizeof(others[0]); i++)
      {
         struct stat st;
         if (     stat(others[i], &st) != 0
               || st.st_uid == 0
               || st.st_uid == geteuid()
               || !(f = core_file_open(others[i])))
            continue;
         CHECK(core_file_identity(f, &id));
         self.dev = id.dev;
         self.uid = (unsigned long)st.st_uid;
         if (core_file_unwritable(f, others[i], &self))
         {
            found++;
            self.uid = 0;
            CHECK(!core_file_unwritable(f, others[i], &self));
            self.uid = (unsigned long)geteuid();
            CHECK(!core_file_unwritable(f, others[i], &self));
         }
         core_file_close(f);
      }
      if (!found)
         printf("note: no service account's file here, not checked\n");
   }
#endif
   core_trust_set_put(s);
}

static void test_check(void)
{
   core_trust_t *t = real_store();

   test_check_finish(t);
   test_check_files(t);
   test_check_exempt(t);
   core_trust_free(t);
}

static void test_self_find(void)
{
   struct core_self self;
#ifdef _WIN32
   CHECK(!core_self_find(&self));
#else
   struct stat st;
   /* The harness's own path, found apart from how the code finds it */
   CHECK(core_self_find(&self));
   CHECK(stat(self_path, &st) == 0 && self.dev == (uint64_t)st.st_dev
         && self.uid == (unsigned long)st.st_uid);
#endif
}

static void clean(void)
{
   static const char *const names[] = { "a.so", "b.so", "native.so",
      "fifo", "fifo.bad",
      FX_NAME, FX_NAME ".sig", "copy_libretro.so", "core_trust",
      "core_trust.bad", "core_trust_dir",
      "core_trust_dir.bad", "locked/core_trust", "locked", "loop_a",
      "loop_b", "one/" FX_NAME, "one/" FX_NAME ".sig", "two/" FX_NAME,
      "two/" FX_NAME ".sig", "three/" FX_NAME, "three/" FX_NAME ".sig",
      "one", "two", "three", "merge_trust",
      "race/core_trust", "race/core_trust.tmp",
      "race", "full_trust", "full_trust.bad", "cap_trust",
      "gone/" FX_NAME, "gone",
      "four/" FX_NAME, "four/" FX_NAME ".sig", "four",
      "five/" FX_NAME, "five/" FX_NAME ".sig", "five" };
   char other[256];
   size_t i;
#ifndef _WIN32
   chmod(tmp("locked"), 0700);
#endif
   if (core_trust_temp_name(other, sizeof(other), tmp("race/core_trust"),
            0, 0))
      filestream_delete(other);
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      filestream_delete(tmp(names[i]));
   path_rmdir(tmp("RetroArch"));
   path_rmdir(TMP_DIR);
}

static void test_builtin_keys(void)
{
   uint8_t core_sha[CORE_SHA256_LEN];
   struct core_sig sig;
   size_t n         = 99;
   const uint8_t *k = core_sig_builtin_keys(&n);
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(n == CORE_SIGNING_KEYS_COUNT + 1);
   CHECK(k && !memcmp(k + (n - 1) * CORE_SIG_KEY_LEN, fx_key1,
            CORE_SIG_KEY_LEN));
   /* The fixtures are signed with test key 1: a test-key build runs them */
   sha(fx_core, FX_CORE_SIZE, core_sha);
   CHECK(core_sig_parse(&sig, fx_sig_good, sizeof(fx_sig_good)));
   CHECK(k && core_sig_verify(&sig, FX_NAME, core_sha, k, n)
         == CORE_CHECK_OK);
#else
   (void)core_sha;
   (void)sig;
   CHECK(n == CORE_SIGNING_KEYS_COUNT);
   CHECK((k != NULL) == (CORE_SIGNING_KEYS_COUNT > 0));
#endif
}

int main(int argc, char *argv[])
{
   /* FAIL lines survive a crash or a sanitizer exit */
   setvbuf(stdout, NULL, _IONBF, 0);
   if (argc > 0)
      self_path = argv[0];
   fx_make_core();
   clean();
   if (!path_mkdir(TMP_DIR))
   {
      printf("FAIL: cannot make %s\n", TMP_DIR);
      return 1;
   }
   test_sig();
   test_builtin_keys();
   test_identity();
#ifndef _WIN32
   test_file_fd();
#endif
   test_trust();
   test_trust_newest_full();
   test_trust_newest_cap();
   test_trust_threads();
   test_trust_file();
   test_newest_folders();
   test_trust_merge();
   test_trust_merge_full();
   test_trust_merge_cap();
   test_trust_temp();
   test_record_older();
   test_ctime_kept();
   test_store_device();
   test_self_device();
   test_owner();
   test_check();
   test_self_find();
   clean();
   free(fx_core);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
