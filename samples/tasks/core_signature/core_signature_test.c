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
#ifdef ANDROID
#include <dlfcn.h>
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../core_signature.h"
#include "../../../core_load_file.h"
#include "../../../core_signing_keys.h"
#include "../../../file_path_special.h"
#include "core_signature_fixtures.h"

/* Matches the .gitignore's samples/ patterns */
#define TMP_DIR "core_signature_test_tmp"

#ifdef _WIN32
#define LIB_EXT ".dll"
#else
#define LIB_EXT ".so"
#endif

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
#ifdef __linux__
   /* ext2/3/4, xfs, btrfs, f2fs, tmpfs, zfs, bcachefs: the VFS's list */
   static const unsigned long kept_magic[] = { 0xEF53UL, 0x58465342UL,
      0x9123683EUL, 0xF2F52010UL, 0x01021994UL, 0x2FC12FC1UL,
      0xCA451A4EUL };
#endif
   struct core_identity a, b;
   struct core_hash h;
   uint8_t want[CORE_SHA256_LEN];
   uint8_t got[CORE_SHA256_LEN];
   core_file_t *f;
   int step;
#ifdef __linux__
   size_t i;
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
#ifdef _WIN32
   CHECK(!records_for(tmp("a.so")));
#else
   if (!records_for(tmp("a.so")))
      printf("note: no identity records on %s's filesystem\n", TMP_DIR);
#endif
#ifdef __linux__
   CHECK(statfs(TMP_DIR, &fs) == 0);
   {
      bool kept = false;
      for (i = 0; i < sizeof(kept_magic) / sizeof(kept_magic[0]); i++)
         kept |= (unsigned long)fs.f_type == kept_magic[i];
      CHECK(records_for(tmp("a.so")) == kept);
   }
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

#ifdef _WIN32
/* Hashes path to its end, reporting the mode core_hash_step took */
static bool hash_whole(const char *path, uint8_t *got, unsigned *mode,
      uint64_t *done)
{
   struct core_hash h;
   core_file_t *f = core_file_open(path);
   int step;
   if (!f || !core_hash_init(&h))
   {
      core_file_close(f);
      return false;
   }
   do
      step = core_hash_step(&h, f);
   while (step > 0);
   *mode = h.mode;
   *done = h.done;
   core_hash_final(&h, got);
   core_file_close(f);
   return step == 0;
}

/* On a local fixed drive a core is hashed in place from the mapping the
 * VFS lends; by any other path it is read. Both give the file's digest. */
static void test_hash_mapped(void)
{
   /* Not a whole number of chunks */
   size_t len  = 40 * 1024 * 1024 + 4099;
   uint8_t *buf = (uint8_t*)malloc(len);
   uint8_t want[CORE_SHA256_LEN], got[CORE_SHA256_LEN];
   char full[MAX_PATH];
   uint32_t x  = 2463534242u;
   uint64_t done;
   unsigned mode;
   size_t i;
   CHECK(buf != NULL);
   if (!buf)
      return;
   for (i = 0; i < len; i++)
   {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      buf[i] = (uint8_t)x;
   }
   sha(buf, len, want);
   CHECK(filestream_write_file(tmp("big.dll"), buf, (int64_t)len));
   free(buf);
   CHECK(GetFullPathNameA(tmp("big.dll"), sizeof(full), full, NULL) > 0);
   if (full[1] == ':')
   {
      char root[4];
      root[0] = full[0];
      root[1] = ':';
      root[2] = '\\';
      root[3] = '\0';
      if (GetDriveTypeA(root) == DRIVE_FIXED)
      {
         CHECK(hash_whole(full, got, &mode, &done));
         CHECK(mode == 1);   /* CORE_HASH_MAPPED */
         CHECK(done == len);
         CHECK(!memcmp(want, got, CORE_SHA256_LEN));
      }
      else
         printf("note: %s is not on a fixed drive, mapping not checked\n",
               full);
   }
   /* A relative path names no drive: read */
   CHECK(hash_whole(tmp("big.dll"), got, &mode, &done));
   CHECK(mode == 2);         /* CORE_HASH_READ */
   CHECK(done == len);
   CHECK(!memcmp(want, got, CORE_SHA256_LEN));
   filestream_delete(tmp("big.dll"));
}
#endif

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

/* core_trust_state_of(), which core_check_record() agrees with */
static enum core_trust_state state_is(core_trust_t *t,
      const struct core_check *c, uint8_t *sha256)
{
   core_trust_set_t *s         = core_trust_get(t);
   enum core_trust_state state = core_trust_state_of(s, c, sha256);
   CHECK(core_check_record(s, c) == (state != CORE_TRUST_STATE_UNCHECKED));
   core_trust_set_put(s);
   return state;
}

static void test_trust_forget(void)
{
   core_trust_t *t = fx_store();
   core_trust_set_t *s0, *s;
   struct core_check a, b, g, o;
   uint8_t two[2 * CORE_SHA256_LEN];

   /* b: a's bytes under another name */
   check_of(&a, "a_libretro.so", 1, 0xa1, 0);
   check_of(&b, "b_libretro.so", 2, 0xa1, 0);
   check_of(&g, "g_libretro.so", 3, 0xa2, 500);
   /* Always Trust on an OLDER warning */
   check_of(&o, "o_libretro.so", 4, 0xa3, 400);
   o.result       = CORE_CHECK_OLDER;
   o.record       = false;
   o.raise_newest = false;
   CHECK(core_trust_always(t, &a));
   CHECK(core_trust_always(t, &b));
   CHECK(core_trust_apply(t, &g));
   CHECK(core_trust_always(t, &o));
   s0 = core_trust_get(t);
   CHECK(core_trust_forget(t, a.rec.sha256, 1));
   CHECK(core_trust_has_hash(s0, a.rec.sha256));
   core_trust_set_put(s0);
   s = core_trust_get(t);
   CHECK(!core_trust_has_hash(s, a.rec.sha256));
   CHECK(!core_trust_has_identity(s, &a.rec.id, "a_libretro.so"));
   CHECK(!core_trust_has_identity(s, &b.rec.id, "b_libretro.so"));
   CHECK(core_trust_state_of(s, &b, NULL) == CORE_TRUST_STATE_UNCHECKED);
   CHECK(core_trust_has_identity(s, &g.rec.id, "g_libretro.so"));
   CHECK(core_trust_newest(s, nowhere, "g_libretro.so") == 500);
   core_trust_set_put(s);
   /* Its record loses its build time */
   CHECK(core_trust_forget(t, o.rec.sha256, 1));
   s = core_trust_get(t);
   CHECK(!core_check_record(s, &o));
   CHECK(!core_trust_has_identity(s, &o.rec.id, "o_libretro.so"));
   core_trust_set_put(s);
   /* Nothing holds it: no new set */
   memset(two, 0xee, CORE_SHA256_LEN);
   s0 = core_trust_get(t);
   CHECK(core_trust_forget(t, two, 1));
   s = core_trust_get(t);
   CHECK(s == s0);
   core_trust_set_put(s);
   core_trust_set_put(s0);
   /* Two in one call */
   check_of(&a, "x_libretro.so", 5, 0xa4, 0);
   check_of(&b, "y_libretro.so", 6, 0xa5, 0);
   CHECK(core_trust_always(t, &a));
   CHECK(core_trust_always(t, &b));
   memcpy(two, a.rec.sha256, CORE_SHA256_LEN);
   memcpy(two + CORE_SHA256_LEN, b.rec.sha256, CORE_SHA256_LEN);
   CHECK(core_trust_forget(t, two, 2));
   s = core_trust_get(t);
   CHECK(!core_trust_has_hash(s, a.rec.sha256));
   CHECK(!core_trust_has_hash(s, b.rec.sha256));
   CHECK(!core_check_record(s, &a));
   CHECK(!core_check_record(s, &b));
   core_trust_set_put(s);
   core_trust_free(t);
}

static void test_trust_state(void)
{
   static const uint64_t other = 2;
   core_trust_t *t = fx_store();
   struct core_check h, c;
   uint8_t sha256[CORE_SHA256_LEN];

   check_of(&h, "h_libretro.so", 1, 0x71, 0);
   CHECK(core_trust_always(t, &h));
   memset(sha256, 0, sizeof(sha256));
   CHECK(state_is(t, &h, sha256) == CORE_TRUST_STATE_TRUSTED);
   CHECK(!memcmp(sha256, h.rec.sha256, CORE_SHA256_LEN));
   check_of(&c, "g_libretro.so", 2, 0x72, 300);
   CHECK(core_trust_apply(t, &c));
   CHECK(state_is(t, &c, NULL) == CORE_TRUST_STATE_SIGNED);
   /* No build time, and its hash isn't trusted */
   check_of(&c, "p_libretro.so", 3, 0x73, 0);
   CHECK(core_trust_apply(t, &c));
   CHECK(state_is(t, &c, NULL) == CORE_TRUST_STATE_UNCHECKED);
   /* Older than a build seen since */
   check_of(&c, "q_libretro.so", 4, 0x74, 100);
   CHECK(core_trust_apply(t, &c));
   check_of(&c, "q_libretro.so", 4, 0x74, 300);
   c.record = false;
   CHECK(core_trust_apply(t, &c));
   check_of(&c, "q_libretro.so", 4, 0x74, 100);
   CHECK(state_is(t, &c, NULL) == CORE_TRUST_STATE_UNCHECKED);
   check_of(&c, "n_libretro.so", 5, 0x75, 0);
   CHECK(state_is(t, &c, NULL) == CORE_TRUST_STATE_UNCHECKED);
   c = h;
   c.rec.id.size++;
   CHECK(state_is(t, &c, NULL) == CORE_TRUST_STATE_UNCHECKED);
   c = h;
   strlcpy(c.rec.name, "other_libretro.so", sizeof(c.rec.name));
   CHECK(state_is(t, &c, NULL) == CORE_TRUST_STATE_UNCHECKED);
   /* A signed build trusted as well */
   check_of(&c, "w_libretro.so", 6, 0x76, 200);
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_always(t, &c));
   CHECK(state_is(t, &c, NULL) == CORE_TRUST_STATE_TRUSTED);
   /* On neither device any more */
   CHECK(core_trust_set_device(t, &other, NULL));
   CHECK(state_is(t, &h, NULL) == CORE_TRUST_STATE_UNCHECKED);
   core_trust_free(t);
}

/* A record with no build time answers only while its hash is trusted */
static void test_record_needs_hash(void)
{
   core_trust_t *t = fx_store();
   core_trust_set_t *s;
   struct core_check c;

   check_of(&c, "c_libretro.so", 1, 0x61, 0);
   CHECK(core_trust_always(t, &c));
   s = core_trust_get(t);
   CHECK(core_check_record(s, &c));
   core_trust_set_put(s);
   CHECK(core_trust_forget(t, c.rec.sha256, 1));
   /* A check that saw the hash trusted lands after the forget */
   c.result    = CORE_CHECK_OK;
   c.record    = true;
   c.rec.flags = 0;
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c.rec.id, "c_libretro.so"));
   CHECK(!core_check_record(s, &c));
   CHECK(!core_check_record_folderless(s, &c));
   core_trust_set_put(s);
   /* A restored older backup's record keeps its build time */
   check_of(&c, "r_libretro.so", 2, 0x62, 100);
   c.rec.flags = CORE_TRUST_RECORD_ANY_AGE;
   CHECK(core_trust_apply(t, &c));
   check_of(&c, "r_libretro.so", 2, 0x62, 300);
   c.record = false;
   CHECK(core_trust_apply(t, &c));
   check_of(&c, "r_libretro.so", 2, 0x62, 100);
   s = core_trust_get(t);
   CHECK(core_trust_newest(s, nowhere, "r_libretro.so") == 300);
   CHECK(core_check_record(s, &c));
   core_trust_set_put(s);
   core_trust_free(t);
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
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_NEWER);
   if ((w = core_trust_write_begin(u)))
   {
      core_trust_write_end(u, w, false);
      CHECK(!"a newer store is writable");
   }
   CHECK(file_is(path, buf, len));
   CHECK(file_is(bad, big, CORE_TRUST_FILE_MAX + 1));
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '9';
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_NEWER);
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = ':';
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_DAMAGED);
   buf[sizeof(CORE_TRUST_MAGIC) - 2] = '0';
   CHECK(core_trust_parse(u, buf, len) == CORE_TRUST_LOAD_DAMAGED);
   free(buf);
   memcpy(big, "RACTRST2", 8);
   CHECK(filestream_write_file(path, big, CORE_TRUST_FILE_MAX + 1));
   CHECK(core_trust_load_file(u, path) == CORE_TRUST_LOAD_NEWER);
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

/* Another instance's save, or its check of a core trusted before, keeps
 * nothing the forget took back */
static void test_forget_survives_merge(void)
{
   core_trust_t *a = core_trust_new();
   core_trust_t *b = core_trust_new();
   core_trust_t *v = core_trust_new();
   core_trust_set_t *s;
   struct core_check u, o, x;
   char path[256];
   strlcpy(path, tmp("forget_trust"), sizeof(path));
   CHECK(core_trust_load_file(a, path) == CORE_TRUST_LOAD_MISSING);
   CHECK(core_trust_set_device(a, &fx_dev, NULL));
   check_of(&u, "u_libretro.so", 1, 0x31, 0);
   u.result = CORE_CHECK_UNSIGNED;
   check_of(&o, "o_libretro.so", 2, 0x32, 400);
   o.result       = CORE_CHECK_OLDER;
   o.record       = false;
   o.raise_newest = false;
   CHECK(core_trust_always(a, &u));
   CHECK(core_trust_always(a, &o));
   CHECK(save(a, path));
   CHECK(core_trust_load_file(b, path) == CORE_TRUST_LOAD_OK);
   CHECK(core_trust_set_device(b, &fx_dev, NULL));
   CHECK(core_trust_forget(a, u.rec.sha256, 1));
   CHECK(core_trust_forget(a, o.rec.sha256, 1));
   CHECK(save(a, path));
   u.result = CORE_CHECK_OK;
   u.record = true;
   CHECK(core_trust_apply(b, &u));
   check_of(&x, "x_libretro.so", 3, 0x33, 0);
   CHECK(core_trust_always(b, &x));
   CHECK(save(b, path));
   CHECK(core_trust_load_file(v, path) == CORE_TRUST_LOAD_OK);
   CHECK(core_trust_set_device(v, &fx_dev, NULL));
   s = core_trust_get(v);
   CHECK(!core_trust_has_hash(s, u.rec.sha256));
   CHECK(!core_trust_has_hash(s, o.rec.sha256));
   /* B's record of u is back, inert */
   CHECK(core_trust_has_identity(s, &u.rec.id, "u_libretro.so"));
   CHECK(!core_check_record(s, &u));
   CHECK(!core_check_record_folderless(s, &u));
   CHECK(!core_trust_has_identity(s, &o.rec.id, "o_libretro.so"));
   CHECK(!core_check_record(s, &o));
   CHECK(core_trust_has_hash(s, x.rec.sha256));
   core_trust_set_put(s);
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
   CHECK(!memcmp(c.folder, c2.folder, sizeof(c.folder)));
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
   /* Kept at any age, its record answers the quick check without the
    * folder's key, which stays unfilled; while a newer build in the
    * folder still decides, the key is taken */
   if (kept)
   {
      static const uint8_t zero[CORE_SHA256_LEN];
      s = core_trust_get(t);
      CHECK((f = core_file_open(core)) != NULL);
      CHECK(core_check_quick(s, f, core, &c2) && c2.result == CORE_CHECK_OK);
      CHECK(!memcmp(c2.folder, zero, sizeof(zero)));
      core_file_close(f);
      core_trust_set_put(s);
   }

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

/* Which records answer without the folder's key: those no folder's
 * newest build can make too old */
static void test_record_folderless(void)
{
   static const uint8_t zero[CORE_SHA256_LEN];
   core_trust_t *t = fx_store();
   core_trust_set_t *s;
   struct core_check c, unsigned_c, other, q;
   check_of(&c, "a_libretro.so", 1, 0xa1, 10);
   check_of(&unsigned_c, "b_libretro.so", 2, 0xb2, 0);
   check_of(&other, "c_libretro.so", 3, 0xc3, 0);
   CHECK(core_trust_apply(t, &c));
   CHECK(core_trust_apply(t, &unsigned_c));
   s = core_trust_get(t);
   /* A signed build that is the newest of its name anywhere: no folder
    * can make it too old */
   CHECK(core_check_record(s, &c));
   CHECK(core_check_record_folderless(s, &c));
   core_trust_set_put(s);
   /* A newer build of its name seen in some folder: the folder decides,
    * so its key is needed */
   {
      struct core_check newer;
      check_of(&newer, "a_libretro.so", 9, 0xa9, 20);
      newer.record = false;
      CHECK(core_trust_apply(t, &newer));
   }
   s = core_trust_get(t);
   CHECK(!core_check_record_folderless(s, &c));
   /* No build time: never too old, but its hash isn't trusted */
   CHECK(!core_check_record_folderless(s, &unsigned_c));
   /* No record */
   CHECK(!core_check_record_folderless(s, &other));
   core_trust_set_put(s);
   /* Kept at any age by Always Trust */
   CHECK(core_trust_always(t, &c));
   CHECK(core_trust_always(t, &unsigned_c));
   s = core_trust_get(t);
   CHECK(core_check_record_folderless(s, &c));
   CHECK(core_check_record_folderless(s, &unsigned_c));
   /* A signed record of a name no folder has a newest build for: as of
    * a store that kept the record and lost the newest builds */
   q = c;
   q.rec.id.ino[0] = 7;
   q.raise_newest  = false;
   strlcpy(q.rec.name, "d_libretro.so", sizeof(q.rec.name));
   core_trust_set_put(s);
   CHECK(core_trust_apply(t, &q));
   s = core_trust_get(t);
   CHECK(core_check_record_folderless(s, &q));
   CHECK(!memcmp(q.folder, zero, sizeof(zero)));
   core_trust_set_put(s);
   core_trust_free(t);
}

/* A record back after its hash was forgotten passes no quick check */
static void test_quick_inert_record(void)
{
   core_trust_t *t = real_store();
   core_trust_set_t *s;
   core_file_t *f;
   struct core_check c, q;
   char core[256];
   CHECK(path_mkdir(tmp("eight")));
   strlcpy(core, tmp("eight/" FX_NAME), sizeof(core));
   CHECK(filestream_write_file(core, fx_core, FX_CORE_SIZE));
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_UNSIGNED);
   CHECK(core_trust_always(t, &c));
   CHECK(core_trust_forget(t, c.rec.sha256, 1));
   /* A check that saw the hash trusted lands after the forget */
   c.result    = CORE_CHECK_OK;
   c.record    = true;
   c.rec.flags = 0;
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(core_trust_has_identity(s, &c.rec.id, FX_NAME) == records_for(core));
   CHECK((f = core_file_open(core)) != NULL);
   CHECK(!core_check_quick(s, f, core, &q));
   core_file_close(f);
   core_trust_set_put(s);
   core_trust_free(t);
}

static void test_trust_state_file(void)
{
   core_trust_t *t = real_store();
   core_trust_set_t *s;
   struct core_check c;
   char core[256];
   bool kept;
   CHECK(path_mkdir(tmp("seven")));
   strlcpy(core, tmp("seven/" FX_NAME), sizeof(core));
   put_core(core, fx_sig_good, sizeof(fx_sig_good));
   kept = records_for(core);
   CHECK(check_path(t, core, 0, &c) == CORE_CHECK_OK);
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(core_trust_state_file(s, core, NULL) == (kept
         ? CORE_TRUST_STATE_SIGNED : CORE_TRUST_STATE_UNCHECKED));
   core_trust_set_put(s);
   CHECK(core_trust_always(t, &c));
   s = core_trust_get(t);
   CHECK(core_trust_state_file(s, core, NULL) == (kept
         ? CORE_TRUST_STATE_TRUSTED : CORE_TRUST_STATE_UNCHECKED));
   CHECK(core_trust_state_file(s, tmp("seven/missing_libretro.so"), NULL)
         == CORE_TRUST_STATE_UNCHECKED);
   core_trust_set_put(s);
   core_trust_free(t);
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

/* Android's system account (uid and gid 1000) counts as root: the Thor's
 * folders on the way to nativeLibraryDir */
static void test_owner_android(void)
{
   static const unsigned long groups[] = { 3003, 9997, 20234, 50234 };
   const unsigned android = CORE_MODE_ROOT_GROUP | CORE_MODE_SYSTEM_ROOT;
   const unsigned desktop = CORE_MODE_ROOT_GROUP;
   struct core_owner app;
   app.gids         = groups;
   app.num_gids     = 4;
   app.uid          = 10234;
   app.overflow_uid = 65534;
   /* / root:root 0755, read-only */
   CHECK( core_mode_exempt_with(&app, 1000, 0, 0, 040755, true, android));
   /* /data, /data/app, /data/user/0: system:system 0771 */
   CHECK( core_mode_exempt_with(&app, 1000, 1000, 1000, 040771, false, android));
   /* /data/app/~~X, <package>-Y: system:system 0775 */
   CHECK( core_mode_exempt_with(&app, 1000, 1000, 1000, 040775, false, android));
   /* lib folders 0755, base.apk 0644, a core 0755, /data/user 0711 */
   CHECK( core_mode_exempt_with(&app, 1000, 1000, 1000, 040755, false, android));
   CHECK( core_mode_exempt_with(&app, 1000, 1000, 1000, 0100644, false, android));
   CHECK( core_mode_exempt_with(&app, 1000, 1000, 1000, 0100755, false, android));
   CHECK( core_mode_exempt_with(&app, 1000, 1000, 1000, 040711, false, android));
   /* Group write by system only with its rule */
   CHECK(!core_mode_exempt_with(&app, 1000, 1000, 1000, 040775, false, desktop));
   CHECK(!core_mode_exempt_with(&app, 1000, 1000, 1000, 040771, false, 0));
   /* and root's group only with its own */
   CHECK(!core_mode_exempt_with(&app, 1000, 0, 0, 040775, false,
            CORE_MODE_SYSTEM_ROOT));
   CHECK( core_mode_exempt_with(&app, 1000, 0, 0, 040775, false, android));
   /* RetroArch's owner unknown (overflowuid unreadable): system's still
    * pass, by the rule alone */
   app.overflow_uid = CORE_UID_UNKNOWN;
   CHECK( core_mode_exempt_with(&app, 1000, 1000, 1000, 040771, false, android));
   CHECK(!core_mode_exempt_with(&app, 1000, 1000, 1000, 040755, false, desktop));
   app.overflow_uid = 65534;
   /* Not another app's, nor ours, nor another group's write, nor a
    * world-writable one */
   CHECK(!core_mode_exempt_with(&app, 1000, 10111, 10111, 0100644, false, android));
   CHECK(!core_mode_exempt_with(&app, 1000, 10234, 10234, 040700, false, android));
   CHECK(!core_mode_exempt_with(&app, 1000, 1000, 1001, 040775, false, android));
   CHECK(!core_mode_exempt_with(&app, 1000, 1001, 1000, 040755, false, android));
   CHECK(!core_mode_exempt_with(&app, 1000, 1000, 1000, 040777, false, android));
   /* nor the overflow uid, even as 1000 */
   app.overflow_uid = 1000;
   CHECK(!core_mode_exempt_with(&app, 1000, 1000, 1000, 040755, false, android));
#if !defined(ANDROID)
   /* Elsewhere uid 1000 is a user's: its folders aren't root's */
   app.uid          = 1001;
   app.overflow_uid = 65534;
   CHECK(!core_mode_exempt(&app, 1001, 1000, 1000, 040755, false));
#endif
}

/* RetroArch's own library on Android: inside its APK, the APK */
static void test_self_path(void)
{
   static const char apk[] =
      "/data/app/~~Xy==/com.retroarch.aarch64-Zw==/base.apk";
   static const char inside[] =
      "/data/app/~~Xy==/com.retroarch.aarch64-Zw==/base.apk!/lib/arm64-v8a/libretroarch.so";
   static const char extracted[] =
      "/data/app/~~Xy==/com.retroarch.aarch64-Zw==/lib/arm64/libretroarch.so";
   CHECK(core_self_path_len(inside) == sizeof(apk) - 1);
   CHECK(core_self_path_len(extracted) == sizeof(extracted) - 1);
   CHECK(core_self_path_len("/data/a!b/libretroarch.so")
         == strlen("/data/a!b/libretroarch.so"));
   CHECK(core_self_path_len("/x.apk!/a!/b.so") == strlen("/x.apk"));
   CHECK(core_self_path_len("libretroarch.so") == 0);
   CHECK(core_self_path_len("!/lib/libretroarch.so") == 0);
   CHECK(core_self_path_len("") == 0);
   CHECK(core_self_path_len(NULL) == 0);
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
   memset(&self, 0, sizeof(self));
   self.dev = id.dev;
   self.uid = 0;
   CHECK(!core_file_unwritable(f, tmp(FX_NAME), &id, &self));
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
      CHECK(!core_file_unwritable(f, "/bin/sh", &id, &self));
      CHECK(!core_file_unwritable(f, "/bin/sh", &id, NULL));
      CHECK(!core_file_unwritable(f, "/bin/sh", NULL, &self));
      /* Exempt when this user is not root */
      self.dev = id.dev;
      if (!core_file_unwritable(f, "/bin/sh", &id, &self))
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
         if (core_file_unwritable(f, others[i], &id, &self))
         {
            found++;
            self.uid = 0;
            CHECK(!core_file_unwritable(f, others[i], &id, &self));
            self.uid = (unsigned long)geteuid();
            CHECK(!core_file_unwritable(f, others[i], &id, &self));
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

#ifdef __linux__
/* The mount /proc gives for an open file, 0 when none */
static uint64_t fdinfo_mount(core_file_t *f)
{
   char name[64];
   char text[512];
   uint64_t mnt = 0;
   size_t got;
   FILE *fp;
   snprintf(name, sizeof(name), "/proc/self/fdinfo/%d", core_file_fd(f));
   if (!(fp = fopen(name, "r")))
      return 0;
   got       = fread(text, 1, sizeof(text) - 1, fp);
   text[got] = '\0';
   fclose(fp);
   {
      const char *at = strstr(text, "mnt_id:");
      mnt = at ? (uint64_t)strtoul(at + 7, NULL, 10) : 0;
   }
   return mnt;
}
#endif

#if defined(__linux__) && !defined(ANDROID)
/* The in-package rule's own mount lookup */
static void test_fd_mount(void)
{
   uint64_t a = 0, b = 0, p = 0;
   core_file_t *one  = core_file_open(self_path);
   core_file_t *two  = core_file_open(self_path);
   core_file_t *proc = core_file_open("/proc/self/status");
   CHECK(one && two);
   if (!proc)
      printf("note: no /proc, mount lookup not checked\n");
   else if (one && two)
   {
      /* One file's two opens share a mount, the one /proc gives; /proc
       * is another */
      struct retro_vfs_volume v;
      struct core_self me;
      CHECK(retro_vfs_file_volume_impl(one, RETRO_VFS_VOLUME_WANT_MOUNT, &v));
      a = v.mount;
      CHECK(retro_vfs_file_volume_impl(two, RETRO_VFS_VOLUME_WANT_MOUNT, &v));
      b = v.mount;
      CHECK(retro_vfs_stat_volume_impl("/proc/self/status",
               RETRO_VFS_VOLUME_WANT_MOUNT, &v) == 1);
      p = v.mount;
      /* RetroArch's own, found once with the rest of it */
      CHECK(core_self_find(&me) && me.has_mnt && me.mnt == a);
      CHECK(a && a == b && a == fdinfo_mount(one));
      CHECK(p && p != a);
   }
   core_file_close(one);
   core_file_close(two);
   core_file_close(proc);
}
#endif

/* Cores inside RetroArch's own package */
static void test_in_package(void)
{
   static const char app[] = "/Applications/RetroArch.app";
   const uint64_t one = 1, two = 2;
   core_trust_t *t = core_trust_new();
   core_trust_set_t *s;
   struct core_check c;
   core_file_t *g;
   char core[512];

   /* Apple: inside RetroArch's own .app, by real paths */
   CHECK( core_path_in_app("/Applications/RetroArch.app/Contents/Frameworks/snes9x_libretro.framework/snes9x_libretro", app));
   CHECK( core_path_in_app("/Applications/RetroArch.app/Contents/Frameworks/x.dylib", "/Applications/RetroArch.app/"));
   CHECK( core_path_in_app("/Applications/RetroArch.app/Contents/Frameworks/x.dylib", "/Applications/RetroArch.app//"));
   CHECK( core_path_in_app("/private/var/containers/Bundle/Application/0A1B/RetroArch.app/Frameworks/x.framework/x", "/private/var/containers/Bundle/Application/0A1B/RetroArch.app"));
   CHECK(!core_path_in_app("/Applications/RetroArch.app2/Contents/Frameworks/x.dylib", app));
   CHECK(!core_path_in_app("/Users/me/Library/Application Support/RetroArch/cores/x.dylib", app));
   CHECK(!core_path_in_app(app, app));
   CHECK(!core_path_in_app("/Applications/RetroArch.app/", app));
   /* A command-line build's main bundle is its binary's folder */
   CHECK(!core_path_in_app("/usr/local/bin/cores/x.dylib", "/usr/local/bin"));
   CHECK(!core_path_in_app("/x.dylib", "/"));
   CHECK(!core_path_in_app(NULL, app));
   CHECK(!core_path_in_app("/Applications/RetroArch.app/x", NULL));

   /* Linux: RetroArch's own mount, read-only */
   CHECK( core_mount_in_package(&one, &one, true));
   CHECK(!core_mount_in_package(&one, &one, false));
   CHECK(!core_mount_in_package(&one, &two, true));
   CHECK(!core_mount_in_package(NULL, &one, true));
   CHECK(!core_mount_in_package(&one, NULL, true));

   /* Our own files are on a writable mount: never in the package, so
    * an unsigned one needs hashing */
   strlcpy(core, tmp("pkg_libretro.so"), sizeof(core));
   CHECK(filestream_write_file(core, fx_core, 1024));
   CHECK((g = core_file_open(core)) != NULL);
   if (g)
   {
      struct core_identity gid;
      struct core_self me;
      bool found = core_self_find(&me);
      CHECK(core_file_identity(g, &gid));
      CHECK(!core_file_in_package(g, core, &gid, found ? &me : NULL));
#if defined(__linux__) && !defined(ANDROID)
      /* Same mount as RetroArch here, but writable: not in it; read-only
       * as told, it would be */
      CHECK(found && me.has_mnt);
      CHECK(!gid.read_only && gid.vol_known);
      gid.read_only = true;
      CHECK(core_file_in_package(g, core, &gid, &me));
      me.mnt++;
      CHECK(!core_file_in_package(g, core, &gid, &me));
#endif
      s = core_trust_get(t);
      CHECK(!core_check_quick(s, g, core, &c));
      core_trust_set_put(s);
#ifdef __linux__
      {
         /* /proc's own text: the harness and its files share a mount,
          * /proc is another */
         core_file_t *self = core_file_open(self_path);
         core_file_t *proc = core_file_open("/proc/self/stat");
         uint64_t a = self ? fdinfo_mount(self) : 0;
         uint64_t b = fdinfo_mount(g);
         uint64_t p = proc ? fdinfo_mount(proc) : 0;
         CHECK(a && a == b && p && p != a);
         core_file_close(self);
         core_file_close(proc);
      }
#endif
      core_file_close(g);
   }
   core_trust_free(t);
}

typedef int (*load_id_t)(void);

/* What the loaded build of core_load_lib.c returns, 0 for none */
static int loaded_id(dylib_t lib)
{
   function_t fn = lib ? dylib_proc(lib, "core_load_test_id") : NULL;
   load_id_t id  = (load_id_t)fn;
   return fn ? id() : 0;
}

/* Copies from to to, and puts to's absolute path in s
 * (path_resolve_realpath needs RARCH_INTERNAL) */
static bool place(const char *from, const char *to, char *s, size_t len)
{
   void *buf   = NULL;
   int64_t got = 0;
   char *real  = NULL;
   bool ok;
   *s = '\0';
   ok = filestream_read_file(from, &buf, &got)
      && filestream_write_file(to, buf, got);
   free(buf);
#ifdef _WIN32
   real = _fullpath(NULL, to, 0);
#else
   real = realpath(to, NULL);
#endif
   ok   = ok && real && strlcpy(s, real, len) < len;
   free(real);
   return ok;
}

static void test_load(void)
{
   char one[512];
   char two[512];
   char two_new[512];
   char bad[512];
   char name[64];
   core_file_t *f;
   dylib_t lib;
#ifdef ANDROID
   Dl_info info;
   function_t fn;
   void *addr;
#endif

   /* Loaded from the file the check opened */
   CHECK(place("core_load_test_a" LIB_EXT,
            tmp("load_one_libretro" LIB_EXT), one, sizeof(one)));
   CHECK((f = core_file_open(one)) != NULL);
   lib = f ? core_load_file(f, one) : NULL;
   CHECK(loaded_id(lib) == 1);
#ifdef ANDROID
   /* named for the core, not /proc/self/fd/N */
   fn = lib ? dylib_proc(lib, "core_load_test_id") : NULL;
   memcpy(&addr, &fn, sizeof(addr));
   CHECK(fn && dladdr(addr, &info) && info.dli_fname
         && string_is_equal(info.dli_fname, one));
#endif
   if (lib)
      dylib_close(lib);
   core_file_close(f);

   /* Replaced once checked: Android loads what was checked, Windows
    * refuses the swap while the check holds the file, and elsewhere the
    * path loads (same-user swaps there are out of scope) */
   CHECK(place("core_load_test_a" LIB_EXT,
            tmp("load_two_libretro" LIB_EXT), two, sizeof(two)));
   CHECK(place("core_load_test_b" LIB_EXT,
            tmp("load_two_new" LIB_EXT), two_new, sizeof(two_new)));
   CHECK((f = core_file_open(two)) != NULL);
#ifdef _WIN32
   CHECK(!MoveFileExA(two_new, two, MOVEFILE_REPLACE_EXISTING));
   /* nor moved away or deleted, which frees the path for another */
   strlcpy(bad, tmp("load_two_moved" LIB_EXT), sizeof(bad));
   CHECK(!MoveFileExA(two, bad, 0));
   MoveFileExA(bad, two, 0);
   CHECK(!DeleteFileA(two));
   CopyFileA(two_new, two, TRUE);
#else
   CHECK(filestream_rename(two_new, two) == 0);
#endif
   lib = f ? core_load_file(f, two) : NULL;
#if defined(_WIN32) || defined(ANDROID)
   CHECK(loaded_id(lib) == 1);
#else
   CHECK(loaded_id(lib) == 2);
#endif
   if (lib)
      dylib_close(lib);
   core_file_close(f);
#ifdef _WIN32
   /* and lets go once closed */
   CHECK(MoveFileExA(two_new, two, MOVEFILE_REPLACE_EXISTING));
#endif

   /* Not a library */
   strlcpy(bad, tmp("load_bad_libretro" LIB_EXT), sizeof(bad));
   CHECK(filestream_write_file(bad, fx_core, FX_CORE_SIZE));
   CHECK((f = core_file_open(bad)) != NULL);
   CHECK(f && !core_load_file(f, bad));
   CHECK(dylib_error() != NULL);
   /* A second call frees glibc's message */
   dylib_error();
   core_file_close(f);

   /* Apple's .framework folder: the binary named for it inside */
   CHECK(core_load_framework_binary(
            "/Applications/RetroArch.app/Contents/Frameworks/snes9x_libretro.framework",
            one, sizeof(one))
         && string_is_equal(one,
            "/Applications/RetroArch.app/Contents/Frameworks/snes9x_libretro.framework/snes9x_libretro"));
   CHECK(core_load_framework_binary("a.framework", name, sizeof(name))
         && string_is_equal(name, "a.framework/a"));
   /* named as dylib_load names it, cut at an archive's '#' */
   CHECK(core_load_framework_binary("/c/a.zip#b.framework", name,
            sizeof(name))
         && string_is_equal(name, "/c/a.zip#b.framework/b"));
   CHECK(core_load_framework_binary("/c/x.zip#d/snes9x_libretro.framework",
            one, sizeof(one))
         && string_is_equal(one,
            "/c/x.zip#d/snes9x_libretro.framework/d/snes9x_libretro"));
   CHECK(!core_load_framework_binary("/cores/snes9x_libretro.dylib", name,
            sizeof(name)));
   CHECK(!core_load_framework_binary("/cores/x.framework/", name,
            sizeof(name)));
   CHECK(!core_load_framework_binary("/cores/.framework", name,
            sizeof(name)));
   CHECK(!core_load_framework_binary(NULL, name, sizeof(name)));
   /* "/a/b.framework/b" and its end need 17 bytes */
   CHECK(!core_load_framework_binary("/a/b.framework", name, 16));
   CHECK( core_load_framework_binary("/a/b.framework", name, 17));
}

#define VERITY_LEN 4000

/* fs-verity's digest (SHA-256, 4096-byte blocks, no salt) of a file of
 * one block at most: its tree's root is that block's hash */
static void verity_digest(const uint8_t *data, size_t len, uint8_t *out)
{
   static uint8_t block[4096];
   static uint8_t desc[256];
   size_t i;
   memset(block, 0, sizeof(block));
   memcpy(block, data, len);
   memset(desc, 0, sizeof(desc));
   desc[0] = 1;  /* version */
   desc[1] = 1;  /* SHA-256 */
   desc[2] = 12; /* log2 of the block size */
   for (i = 0; i < 8; i++)
      desc[8 + i] = (uint8_t)((uint64_t)len >> (8 * i));
   sha(block, sizeof(block), desc + 16);
   sha(desc, sizeof(desc), out);
}

/* The quick check measures a file whose record has a digest */
static void test_verity_quick(const char *core)
{
   core_trust_t *t = real_store();
   core_trust_set_t *s;
   core_file_t *f  = core_file_open(core);
   struct core_check c, c2;
   bool kept       = records_for(core);
   CHECK(f != NULL);
   if (!f)
   {
      core_trust_free(t);
      return;
   }
   s = core_trust_get(t);
   CHECK(!core_check_quick(s, f, core, &c));
   core_trust_set_put(s);
   /* Trusted, recorded with its digest */
   CHECK(core_file_verity_measure(f, &c.rec.id));
   sha(fx_core, VERITY_LEN, c.rec.sha256);
   c.result = CORE_CHECK_OK;
   c.record = true;
   CHECK(core_trust_always(t, &c));
   s = core_trust_get(t);
   CHECK(core_check_quick(s, f, core, &c2) == kept);
   core_trust_set_put(s);
   /* Another digest on record doesn't hold */
   c.rec.id.verity[0] ^= 1;
   CHECK(core_trust_apply(t, &c));
   s = core_trust_get(t);
   CHECK(!core_check_quick(s, f, core, &c2));
   core_trust_set_put(s);
   core_file_close(f);
   core_trust_free(t);
}

static void test_verity(void)
{
   core_trust_t *t = fx_store();
   core_trust_set_t *s;
   struct core_check c, d;
   struct core_identity id;
   uint8_t want[CORE_SHA256_LEN];
   char core[512];
   core_file_t *f;
   int on;

   /* A record with a digest holds only with that digest measured */
   check_of(&c, "v_libretro.so", 1, 0x11, 10);
   c.rec.id.verity_len = CORE_SHA256_LEN;
   memset(c.rec.id.verity, 0xab, CORE_SHA256_LEN);
   CHECK(core_trust_apply(t, &c));
   check_of(&d, "w_libretro.so", 2, 0x22, 10);
   CHECK(core_trust_apply(t, &d));
   s = core_trust_get(t);
   d = c;
   d.rec.id.verity_len = 0;
   memset(d.rec.id.verity, 0, sizeof(d.rec.id.verity));
   CHECK(core_check_wants_verity(s, &d));
   CHECK(!core_check_record(s, &d));
   d.rec.id.verity_len = CORE_SHA256_LEN;
   memset(d.rec.id.verity, 0xab, CORE_SHA256_LEN);
   CHECK(core_check_record(s, &d));
   d.rec.id.verity[0] ^= 1;
   CHECK(!core_check_record(s, &d));
   /* nothing to measure for another file or name */
   check_of(&d, "v_libretro.so", 3, 0x11, 10);
   CHECK(!core_check_wants_verity(s, &d));
   check_of(&d, "x_libretro.so", 1, 0x11, 10);
   CHECK(!core_check_wants_verity(s, &d));
   /* nor for a record without one */
   check_of(&d, "w_libretro.so", 2, 0x22, 10);
   CHECK(!core_check_wants_verity(s, &d));
   CHECK(core_check_record(s, &d));
   /* nor where records aren't honoured */
   check_of(&d, "v_libretro.so", 1, 0x11, 10);
   d.rec.id.ctime_kept = false;
   CHECK(!core_check_wants_verity(s, &d));
   core_trust_set_put(s);
   core_trust_free(t);

   /* Real files, where the filesystem has fs-verity */
   strlcpy(core, tmp("verity_libretro.so"), sizeof(core));
   CHECK(filestream_write_file(core, fx_core, VERITY_LEN));
   CHECK((f = core_file_open(core)) != NULL);
   on = f ? core_file_verity_enable(f) : -1;
   CHECK(on >= 0);
   memset(&id, 0, sizeof(id));
   if (on == 0)
   {
      printf("note: no fs-verity on %s's filesystem, not checked\n", TMP_DIR);
      CHECK(f && !core_file_verity_measure(f, &id));
      /* A digest on record, with none to measure, fails closed */
      t = real_store();
      s = core_trust_get(t);
      CHECK(!core_check_quick(s, f, core, &c));
      core_trust_set_put(s);
      sha(fx_core, VERITY_LEN, c.rec.sha256);
      c.result = CORE_CHECK_OK;
      c.record = true;
      CHECK(core_trust_always(t, &c));
      s = core_trust_get(t);
      CHECK(core_check_quick(s, f, core, &d) == records_for(core));
      core_trust_set_put(s);
      c.rec.id.verity_len = CORE_SHA256_LEN;
      verity_digest(fx_core, VERITY_LEN, c.rec.id.verity);
      CHECK(core_trust_apply(t, &c));
      s = core_trust_get(t);
      CHECK(!core_check_quick(s, f, core, &d));
      core_trust_set_put(s);
      core_trust_free(t);
   }
   else if (on > 0)
   {
      CHECK(core_file_verity_measure(f, &id)
            && id.verity_len == CORE_SHA256_LEN);
      verity_digest(fx_core, VERITY_LEN, want);
      CHECK(!memcmp(id.verity, want, CORE_SHA256_LEN));
      CHECK(core_file_verity_enable(f) == 1);
      /* never written again */
      CHECK(!filestream_write_file(core, fx_core, 1));
      test_verity_quick(core);
   }
   core_file_close(f);
   filestream_delete(core);
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
   /* Looked up once: the same answer again */
   {
      struct core_self again;
      memset(&again, 0, sizeof(again));
      CHECK(core_self_find(&again));
      CHECK(again.dev == self.dev && again.uid == self.uid);
   }
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
      "five/" FX_NAME, "five/" FX_NAME ".sig", "five",
      "seven/" FX_NAME, "seven/" FX_NAME ".sig", "seven", "forget_trust",
      "eight/" FX_NAME, "eight",
      "pkg_libretro.so", "verity_libretro.so",
      "load_one_libretro" LIB_EXT, "load_two_libretro" LIB_EXT,
      "load_two_new" LIB_EXT, "load_bad_libretro" LIB_EXT };
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
   /* For a run inside a read-only bind mount, outside CI */
   if (argc == 3 && string_is_equal(argv[1], "--in-package"))
   {
      struct core_identity id;
      struct core_self self;
      bool found     = core_self_find(&self);
      core_file_t *f = core_file_open_id(argv[2], &id);
      bool in        = f && core_file_in_package(f, argv[2], &id,
            found ? &self : NULL);
      core_file_close(f);
      printf("%s\n", in ? "in package" : "not in package");
      return 0;
   }
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
#ifdef _WIN32
   test_hash_mapped();
#endif
#ifndef _WIN32
   test_file_fd();
#endif
   test_trust();
   test_trust_forget();
   test_trust_state();
   test_record_needs_hash();
   test_forget_survives_merge();
   test_trust_state_file();
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
   test_record_folderless();
   test_quick_inert_record();
   test_ctime_kept();
   test_store_device();
   test_self_device();
   test_owner();
   test_owner_android();
   test_self_path();
   test_check();
   test_in_package();
#if defined(__linux__) && !defined(ANDROID)
   test_fd_mount();
#endif
   test_load();
   test_verity();
   test_self_find();
   clean();
   free(fx_core);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
