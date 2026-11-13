/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CORE_SIGNATURE_H__
#define CORE_SIGNATURE_H__

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <lrc_hash.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* <core>.sig, big-endian: magic (8), key id (8), build time (8, never
 * 0), core SHA-256 (32), name length (1), name, then r || s over the
 * SHA-256 of everything before it. */
#define CORE_SIG_MAGIC       "RACSIG01"
#define CORE_SIG_FIXED_LEN   57
#define CORE_SIG_NAME_MAX    255
#define CORE_SIG_RS_LEN      64
#define CORE_SIG_MAX_LEN     (CORE_SIG_FIXED_LEN + CORE_SIG_NAME_MAX + CORE_SIG_RS_LEN)
#define CORE_SIG_KEY_LEN     65
#define CORE_SIG_KEY_ID_LEN  8
#define CORE_SHA256_LEN      32

enum core_check_result
{
   CORE_CHECK_OK = 0,
   CORE_CHECK_UNSIGNED,
   CORE_CHECK_MODIFIED,
   CORE_CHECK_OLDER,
   CORE_CHECK_UNKNOWN_KEY,
   /* Could not be checked, as an unreadable core: never trusted, and
    * nothing is recorded */
   CORE_CHECK_ERROR
};

/* digest is our own SHA-256 of the signed bytes, never the file's */
struct core_sig
{
   uint64_t build_time;
   uint8_t  key_id[CORE_SIG_KEY_ID_LEN];
   uint8_t  core_sha256[CORE_SHA256_LEN];
   uint8_t  digest[CORE_SHA256_LEN];
   uint8_t  r[32];
   uint8_t  s[32];
   char     name[CORE_SIG_NAME_MAX + 1];
};

/* Keys are num_keys 65-byte P-256 points, 0x04 || X || Y, back to back.
 * libretro's, from core_signing_keys.h; NULL when there are none. */
const uint8_t *core_sig_builtin_keys(size_t *num_keys);

/* The first 8 bytes of the SHA-256 of a 65-byte key */
void core_sig_key_id(const uint8_t *key, uint8_t *id);

/* False unless buf is exactly one well-formed .sig */
bool core_sig_parse(struct core_sig *sig, const uint8_t *buf, size_t len);

/* OK, MODIFIED or UNKNOWN_KEY; the build time is the caller's */
enum core_check_result core_sig_verify(const struct core_sig *sig,
      const char *file_name, const uint8_t *core_sha256,
      const uint8_t *keys, size_t num_keys);

/* What identifies a file without reading it. Times are in the
 * platform's own epoch and units: only equality is ever asked. */
#define CORE_ID_VERITY_MAX   64

struct core_identity
{
   uint64_t dev;
   uint64_t size;
   int64_t  mtime;
   int64_t  ctime;
   uint32_t mtime_nsec;
   uint32_t ctime_nsec;
   uint8_t  ino[16];
   /* fs-verity digest (Android), when enabled */
   uint8_t  verity[CORE_ID_VERITY_MAX];
   uint8_t  verity_len;
   /* Its filesystem keeps ctime itself, so no writer can forge a record
    * of it: only then is one made or honoured. Never stored. */
   bool     ctime_kept;
};

bool core_identity_equal(const struct core_identity *a,
      const struct core_identity *b);

/* Filesystems whose kernel keeps ctime and that aren't stacked on
 * another: Linux's statfs f_type, the BSDs' and macOS's f_fstypename */
bool core_fs_magic_keeps_ctime(unsigned long f_type);
bool core_fs_name_keeps_ctime(const char *name);

/* A core opened for checking. On Windows nothing can write or delete
 * it until it is closed. */
typedef struct core_file core_file_t;

core_file_t *core_file_open(const char *path);
void core_file_close(core_file_t *f);
/* Bytes read, 0 at the end, -1 on error */
int64_t core_file_read(core_file_t *f, void *buf, size_t len);
bool core_file_identity(core_file_t *f, struct core_identity *id);

/* Hashes an open core a chunk per step, so a task can pace it */
struct core_hash
{
   struct sha256_state sha;
   uint8_t *buf;
   uint64_t done;
};

bool core_hash_init(struct core_hash *h);
/* 1 while there is more, 0 at the end, -1 on a read error */
int core_hash_step(struct core_hash *h, core_file_t *f);
/* Writes the digest and frees what init took. It is the file's only
 * when h.done equals the size in the file's identity. */
void core_hash_final(struct core_hash *h, uint8_t *sha256);
void core_hash_free(struct core_hash *h);

/* The trust store: identity records, trusted hashes, the newest build
 * time seen per core name in each folder and a few flags.
 *
 * Only the main thread holds a core_trust_t, changes it and starts its
 * writes. Every change publishes a new core_trust_set_t and never touches
 * one already handed out, so a set is immutable once published. A task
 * gets its own reference from core_trust_get() at push, on the main
 * thread, and drops it with core_trust_set_put() from any thread. */
#define CORE_TRUST_CHECK_OFF     (1 << 0)
#define CORE_TRUST_SEEDED        (1 << 1)
#define CORE_TRUST_NOTICE_SHOWN  (1 << 2)

/* Bump the digit on any format change, or older builds set the store aside */
#define CORE_TRUST_MAGIC         "RACTRST1"
#define CORE_TRUST_MAX_RECORDS   512
#define CORE_TRUST_MAX_HASHES    256
#define CORE_TRUST_MAX_NEWEST    4096
#define CORE_TRUST_HEAD_LEN      18
#define CORE_TRUST_RECORD_FIXED  (8 + 16 + 8 + 8 + 4 + 8 + 4 + 1 + CORE_ID_VERITY_MAX + 8 + CORE_SHA256_LEN + 1 + 1)
#define CORE_TRUST_NEWEST_FIXED  (8 + CORE_SHA256_LEN + 1)
#define CORE_TRUST_FILE_MAX      (CORE_TRUST_HEAD_LEN + CORE_SHA256_LEN \
      + CORE_TRUST_MAX_RECORDS * (CORE_TRUST_RECORD_FIXED + CORE_SIG_NAME_MAX) \
      + CORE_TRUST_MAX_HASHES  * CORE_SHA256_LEN \
      + CORE_TRUST_MAX_NEWEST  * (CORE_TRUST_NEWEST_FIXED + CORE_SIG_NAME_MAX))

enum core_trust_load
{
   CORE_TRUST_LOAD_OK = 0,
   /* No file yet: an empty store, written on the first change */
   CORE_TRUST_LOAD_MISSING,
   /* Never written over, so nothing it holds is lost. The store's
    * flags mean nothing then: don't seed from it. */
   CORE_TRUST_LOAD_UNREADABLE,
   /* Read, but not a store: from core_trust_parse only */
   CORE_TRUST_LOAD_DAMAGED,
   /* Was damaged, now <path>.bad; an empty store, every flag clear */
   CORE_TRUST_LOAD_RESET
};

/* Always Trust, or a restore: still matches once a newer build is seen */
#define CORE_TRUST_RECORD_ANY_AGE (1 << 0)

struct core_trust_record
{
   struct core_identity id;
   uint64_t build_time;
   uint8_t  sha256[CORE_SHA256_LEN];
   uint8_t  flags;
   char     name[CORE_SIG_NAME_MAX + 1];
};

/* A check's outcome, for the main thread to record */
struct core_check
{
   struct core_trust_record rec;
   /* Key of the folder the core is installed in */
   uint8_t folder[CORE_SHA256_LEN];
   enum core_check_result result;
   bool record;
   bool raise_newest;
};

typedef struct core_trust core_trust_t;
typedef struct core_trust_set core_trust_set_t;

core_trust_t *core_trust_new(void);
void core_trust_free(core_trust_t *t);
/* Anything but OK leaves the store as it was and read-only. A NULL buf
 * is a file that could not be read. */
enum core_trust_load core_trust_parse(core_trust_t *t,
      const uint8_t *buf, size_t len);
/* The file's bytes, malloc'd */
uint8_t *core_trust_serialize(const core_trust_t *t, size_t *len);

core_trust_set_t *core_trust_get(core_trust_t *t);
void core_trust_set_put(core_trust_set_t *s);

/* Apply and always take a check core_check_finish ended, as
 * core_check_file's: before it c->rec.sha256 is unset.
 * Each returns false, changing nothing, when out of memory; always
 * also when the trusted hashes are full. Apply and always also refuse
 * an empty name or a verity_len over CORE_ID_VERITY_MAX, and always a
 * check that ended in ERROR. Apply also refuses to record a build older
 * than the newest now known for its folder, unless the check allowed
 * older. Neither records a file whose identity isn't ctime_kept or that
 * is on neither the store's device nor RetroArch's. A full newest list
 * drops its oldest build. */
bool core_trust_apply(core_trust_t *t, const struct core_check *c);
bool core_trust_always(core_trust_t *t, const struct core_check *c);
bool core_trust_set_flag(core_trust_t *t, unsigned flag, bool on);
/* Records are made and honoured only for files on dev, the store's own
 * filesystem, or on self_dev, RetroArch's own: whoever can write either
 * raw can rewrite the store or RetroArch itself. NULL when unknown; a
 * new store has neither, so no records. core_trust_load_file takes dev
 * from the store's folder and self_dev from core_self_find. */
bool core_trust_set_device(core_trust_t *t, const uint64_t *dev,
      const uint64_t *self_dev);

unsigned core_trust_flags(const core_trust_set_t *s);
bool core_trust_has_identity(const core_trust_set_t *s,
      const struct core_identity *id, const char *name);
bool core_trust_has_hash(const core_trust_set_t *s, const uint8_t *sha256);
/* 0 when the name was never seen in that folder */
uint64_t core_trust_newest(const core_trust_set_t *s, const uint8_t *folder,
      const char *name);

#define CORE_TRUST_FILE_NAME "core_trust"

/* Another instance can be saving the same file, so a write reads it
 * again and puts this store's own changes since it was last loaded or
 * saved over what is there now.
 *
 * After every change, on the main thread: NULL means a write is still
 * out (it runs again after), the file could not be read at load, or out
 * of memory. */
typedef struct core_trust_write core_trust_write_t;
core_trust_write_t *core_trust_write_begin(core_trust_t *t);
/* In a task: false when nothing was written, as when the file there
 * now can't be read or is a newer version's */
bool core_trust_write_run(core_trust_write_t *w, const char *path);
/* On the main thread, with what run returned; frees w. True means
 * begin again. */
bool core_trust_write_end(core_trust_t *t, core_trust_write_t *w,
      bool written);
/* The bytes run writes over disk, a store loaded from the file, or NULL
 * when there is none; malloc'd. A hash disk has no room for stays a
 * change for the next write. */
uint8_t *core_trust_write_bytes(core_trust_write_t *w,
      const core_trust_t *disk, size_t *len);
/* The temporary run writes before its rename, one of its own so two
 * instances' saves can't mix: <path>.<pid>.<stamp>.tmp. stamp is the
 * clock, as Flatpak's PID namespaces repeat pids. */
bool core_trust_temp_name(char *s, size_t len, const char *path,
      unsigned long pid, unsigned long stamp);

/* An empty, writable store, for once a damaged file is set aside */
bool core_trust_reset(core_trust_t *t);
enum core_trust_load core_trust_load_file(core_trust_t *t,
      const char *path);
/* Android: internalDataPath. Elsewhere: the platform's default config
 * folder, never a path from a config file. */
bool core_trust_default_path(char *s, size_t len);

#define CORE_CHECK_ALLOW_OLDER   (1 << 0)

/* Whose writes a file's owner and mode bits allow */
struct core_owner
{
   const unsigned long *gids;
   size_t num_gids;
   unsigned long uid;
   /* Whom a user namespace shows for the accounts it doesn't map, or
    * CORE_UID_UNKNOWN */
   unsigned long overflow_uid;
};

#define CORE_UID_UNKNOWN ((unsigned long)-1)

bool core_mode_writable(const struct core_owner *who, unsigned long uid,
      unsigned long gid, unsigned mode, bool read_only_mount);
/* One file or folder's part of core_file_unwritable's rule, self_uid
 * being the owner of RetroArch's own binary or library and mode st_mode,
 * type bits and all */
bool core_mode_exempt(const struct core_owner *who, unsigned long self_uid,
      unsigned long uid, unsigned long gid, unsigned mode,
      bool read_only_mount);

/* RetroArch's own binary or library */
struct core_self
{
   uint64_t dev;
   unsigned long uid;
};

/* POSIX: false when unknown, and always on Windows */
bool core_self_find(struct core_self *self);
/* The exemption, POSIX only (none on Windows): the file is on self->dev
 * (self NULL when unknown), and it and every folder up to / aren't
 * writable by us, are owned by root or RetroArch's owner, never the
 * overflow uid, and have no group write unless a sticky folder or, on
 * Linux and Android, gid 0; read-only mounts skip owner and group.
 * Android: other apps' library folders are system's too, so
 * android_core_path_resolve must come first. */
bool core_file_unwritable(core_file_t *f, const char *path,
      const struct core_self *self);

/* The key of the folder holding path, for its newest build times: its
 * canonical path, so ., .., repeated separators and symlinks (POSIX) or
 * case (Windows) don't change it. On Android every folder has one key,
 * so a config file naming a fresh folder can't reset them, and the
 * folder isn't looked up. Elsewhere false when it can't be found. */
bool core_folder_key(const char *path, uint8_t *key);
/* Either rule, per_folder false being Android's, for tests */
bool core_folder_key_with(const char *path, bool per_folder, uint8_t *key);

/* A record of this file, for a build no older than the newest seen in
 * c->folder unless the record may be older */
bool core_check_record(const core_trust_set_t *s, const struct core_check *c);
/* The check up to hashing: true when c->result is final.
 * Fills c->rec.name, and c->rec.id and c->folder unless the check is
 * off, for core_check_finish(). */
bool core_check_quick(const core_trust_set_t *s, core_file_t *f,
      const char *path, struct core_check *c);
/* The rest, given the core's SHA-256 and its .sig (NULL or empty when none) */
void core_check_finish(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys,
      const uint8_t *sha256, const uint8_t *sig_buf, size_t sig_len,
      unsigned flags, struct core_check *c);
/* The whole check in one call, hashing without a pause. It opens path
 * itself, so it suits a file nothing else can replace (a staged install,
 * tests); a load check runs core_check_quick() and core_check_finish() on
 * the file it loads from. */
enum core_check_result core_check_file(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys,
      const char *path, unsigned flags, struct core_check *c);

RETRO_END_DECLS

#endif
