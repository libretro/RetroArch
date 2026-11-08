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
#include <retro_atomic.h>
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

   /* Any damage makes it unreadable */
   for (i = 0; i < len; i += 7)
   {
      buf[i] ^= 0x10;
      CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_UNREADABLE);
      buf[i] ^= 0x10;
   }
   CHECK(core_trust_parse(u, buf, len - 1) == CORE_TRUST_LOAD_UNREADABLE);
   CHECK(core_trust_parse(u, buf, CORE_TRUST_FILE_MAX + 1)
         == CORE_TRUST_LOAD_UNREADABLE);
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
         == CORE_TRUST_LOAD_UNREADABLE);
   more[0] = CORE_ID_VERITY_MAX + 1;
   CHECK(parse_spliced(u, buf, len, CORE_TRUST_HEAD_LEN + 56, 1, more, 1)
         == CORE_TRUST_LOAD_UNREADABLE);
   /* An empty name, and one with a NUL */
   CHECK(parse_spliced(u, buf, len, name_at, 1 + buf[name_at], zero, 1)
         == CORE_TRUST_LOAD_UNREADABLE);
   CHECK(parse_spliced(u, buf, len, name_at + 1, 1, zero, 1)
         == CORE_TRUST_LOAD_UNREADABLE);
   /* A byte too many, and one too few */
   CHECK(parse_spliced(u, buf, len, len - CORE_SHA256_LEN, 0, zero, 1)
         == CORE_TRUST_LOAD_UNREADABLE);
   CHECK(parse_spliced(u, buf, len, len - CORE_SHA256_LEN - 1, 1, zero, 0)
         == CORE_TRUST_LOAD_UNREADABLE);
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
   test_trust();
   test_trust_newest_full();
   test_trust_newest_cap();
   test_trust_threads();
   clean();
   free(fx_core);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
