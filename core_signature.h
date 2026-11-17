/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CORE_SIGNATURE_H__
#define CORE_SIGNATURE_H__

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <lrc_hash.h>
#include <retro_common_api.h>
#include <vfs/vfs.h>

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
   /* As opened, never stored: its owner, group and st_mode (POSIX, with
    * has_owner), and whether its mount is read-only (with vol_known),
    * so the exemption rules ask nothing again */
   bool     has_owner;
   bool     vol_known;
   bool     read_only;
   uint16_t mode;
   uint32_t uid;
   uint32_t gid;
};

bool core_identity_equal(const struct core_identity *a,
      const struct core_identity *b);


/* A core opened for checking: the VFS's stable open of it
 * (RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE), a regular file read on its
 * descriptor. On Windows nothing can write, rename or delete it until it
 * is closed. */
typedef libretro_vfs_implementation_file core_file_t;

core_file_t *core_file_open(const char *path);
void core_file_close(core_file_t *f);
/* Bytes read, 0 at the end, -1 on error */
int64_t core_file_read(core_file_t *f, void *buf, size_t len);
bool core_file_identity(core_file_t *f, struct core_identity *id);
/* core_file_open() and core_file_identity() in one: the identity the
 * open vouched for the file with, without a query of its own. id is
 * zeroed when the open fails. */
core_file_t *core_file_open_id(const char *path, struct core_identity *id);
#ifndef _WIN32
/* The open file itself, for loading the core from it. It stays f's:
 * close f, never it. */
int core_file_fd(core_file_t *f);
#endif
/* fs-verity, Linux and Android, where the filesystem has it. The digest
 * the kernel keeps for f, into id->verity: false when it has none. */
bool core_file_verity_measure(core_file_t *f, struct core_identity *id);
/* Makes f read-only for good, every block checked as it is read. Reads
 * the whole file, so only in a task. 1 when on (or on already), 0 when
 * the filesystem or kernel can't, the file isn't ours or a writer has it
 * open, -1 on any other failure. */
int core_file_verity_enable(core_file_t *f);

/* <path>.sig, for free(); NULL when out of memory */
char *core_sig_path(const char *path);
/* <path>.sig into sig, which holds CORE_SIG_MAX_LEN + 1 bytes: read no
 * further than the longest valid one and a byte, so a longer one stays
 * invalid. Its length, or -1 when there is none or it can't be read. */
int64_t core_sig_read(const char *path, uint8_t *sig);

/* Hashes an open core a chunk per step, so a task can pace it */
struct core_hash
{
   struct sha256_state sha;
   uint8_t *buf;
   uint64_t done;
#ifdef _WIN32
   /* Hashed in place from the file's mapping, lent by the VFS, where the
    * file allows one (its handle denies writes, so nothing can cut it
    * short): no copy into buf, which is then never allocated */
   const uint8_t *map;     /* the file's bytes, NULL when read      */
   uint64_t start;         /* file offset the hash began at         */
   uint64_t size;          /* file size, for the mapped path        */
   unsigned char mode;     /* CORE_HASH_UNSET / _MAPPED / _READ     */
#endif
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
   /* A newer version's: as UNREADABLE, the file kept for it */
   CORE_TRUST_LOAD_NEWER,
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
/* Always refuses c on s only because the trusted hashes are full */
bool core_trust_full_refuses(const core_trust_set_t *s,
      const struct core_check *c);
/* Untrusts the n hashes back to back at sha256 and retires every record
 * carrying one of them. False, changing nothing, when out of memory. */
bool core_trust_forget(core_trust_t *t, const uint8_t *sha256, unsigned n);
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
/* The trusted hashes, i below core_trust_num_hashes() */
unsigned core_trust_num_hashes(const core_trust_set_t *s);
const uint8_t *core_trust_hash_at(const core_trust_set_t *s, unsigned i);
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
/* Whether name, with no folder, is a temporary temp_name made for the
 * store file named store_name */
bool core_trust_temp_of(const char *name, const char *store_name);

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
/* core_mode_exempt's platform rules: gid 0 is root's group (Linux,
 * Android), and uid and gid 1000, Android's system, count as root */
#define CORE_MODE_ROOT_GROUP  (1 << 0)
#define CORE_MODE_SYSTEM_ROOT (1 << 1)
/* core_mode_exempt with the rules given, for tests */
bool core_mode_exempt_with(const struct core_owner *who,
      unsigned long self_uid, unsigned long uid, unsigned long gid,
      unsigned mode, bool read_only_mount, unsigned rules);

/* RetroArch's own binary or library */
struct core_self
{
   uint64_t dev;
   uint64_t mnt;      /* its mount (Linux), with has_mnt */
   unsigned long uid;
   bool has_mnt;
};

/* POSIX: false when unknown, and always on Windows */
bool core_self_find(struct core_self *self);
/* The part of RetroArch's library path to stat: the APK, for a library
 * loaded from inside it (<apk>!/<path in it>, Android). 0 when not
 * absolute. */
size_t core_self_path_len(const char *path);
/* The exemption, POSIX only (none on Windows): the file is on self->dev
 * (self NULL when unknown), and it and every folder up to / aren't
 * writable by us, are owned by root or RetroArch's owner, never the
 * overflow uid, and have no group write unless a sticky folder or, on
 * Linux and Android, gid 0; on Android system (uid and gid 1000) counts
 * as root. Read-only mounts skip owner and group.
 * Android: other apps' library folders are system's too, so
 * android_core_path_resolve must come first. */
bool core_file_unwritable(core_file_t *f, const char *path,
      const struct core_identity *id, const struct core_self *self);
/* RetroArch's own package: Apple, a core whose real path lies inside
 * RetroArch's .app bundle; Linux, one on the read-only mount RetroArch's
 * binary is on (Flatpak, AppImage, Snap). Never on Windows, Android or
 * the BSDs. */
bool core_file_in_package(core_file_t *f, const char *path,
      const struct core_identity *id, const struct core_self *self);
/* Its parts, for tests: real paths; mount ids, NULL when unknown */
bool core_path_in_app(const char *core_real, const char *app_real);
bool core_mount_in_package(const uint64_t *core_mnt,
      const uint64_t *self_mnt, bool read_only);

/* The key of the folder holding path, for its newest build times: its
 * canonical path, so ., .., repeated separators and symlinks (POSIX) or
 * case (Windows) don't change it. On Android every folder has one key,
 * so a config file naming a fresh folder can't reset them, and the
 * folder isn't looked up. Elsewhere false when it can't be found. */
bool core_folder_key(const char *path, uint8_t *key);
/* Either rule, per_folder false being Android's, for tests */
bool core_folder_key_with(const char *path, bool per_folder, uint8_t *key);

enum core_trust_state
{
   CORE_TRUST_STATE_UNCHECKED = 0,
   /* Its record's hash is trusted */
   CORE_TRUST_STATE_TRUSTED,
   /* Its record holds a .sig's build time */
   CORE_TRUST_STATE_SIGNED
};

/* What c's record says, c as core_check_quick() fills it (a build older
 * than its folder's newest needs CORE_TRUST_RECORD_ANY_AGE). sha256 (or
 * NULL) gets the record's hash unless UNCHECKED. */
enum core_trust_state core_trust_state_of(const core_trust_set_t *s,
      const struct core_check *c, uint8_t *sha256);
/* core_trust_state_of() for the file at path, which it doesn't read:
 * UNCHECKED when it can't be opened */
enum core_trust_state core_trust_state_file(const core_trust_set_t *s,
      const char *path, uint8_t *sha256);
/* A record answers for this file: core_trust_state_of() isn't UNCHECKED */
bool core_check_record(const core_trust_set_t *s, const struct core_check *c);
/* A record matches this file in all but its fs-verity digest and has
 * one: the file's must be measured for the record to hold */
/* core_check_record() for a record that holds whatever folder its core
 * is in: one with no build time and a trusted hash, or kept at any
 * age, or whose build no folder has a newer one of (the usual case:
 * the newest seen). The quick check takes it without the folder's
 * key, which costs a walk of the folder's path. */
bool core_check_record_folderless(const core_trust_set_t *s,
      const struct core_check *c);
bool core_check_wants_verity(const core_trust_set_t *s,
      const struct core_check *c);
/* The check up to hashing: true when c->result is final.
 * Fills c->rec.name, and c->rec.id unless the check is off; c->folder
 * whenever it returns false, for core_check_finish(). A pass from a
 * record that holds in any folder (core_check_record_folderless())
 * leaves c->folder zero: a pass is never applied to the store. */
bool core_check_quick(const core_trust_set_t *s, core_file_t *f,
      const char *path, struct core_check *c);
/* The same with the identity core_file_open_id() handed back for f */
bool core_check_quick_id(const core_trust_set_t *s, core_file_t *f,
      const char *path, const struct core_identity *id, struct core_check *c);
/* The rest, given the core's SHA-256 and its .sig (NULL or empty when none) */
void core_check_finish(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys,
      const uint8_t *sha256, const uint8_t *sig_buf, size_t sig_len,
      unsigned flags, struct core_check *c);
/* core_check_finish() once h has read the core at path to its end
 * (step 0) or failed (step -1), with path's .sig. Frees what h holds. */
void core_check_hashed(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys, struct core_hash *h, int step,
      const char *path, unsigned flags, struct core_check *c);
/* The whole check in one call, hashing without a pause. It opens path
 * itself, so it suits a file nothing else can replace (a staged install,
 * tests); a load check runs core_check_quick() and core_check_hashed() on
 * the file it loads from. */
enum core_check_result core_check_file(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys,
      const char *path, unsigned flags, struct core_check *c);

RETRO_END_DECLS

#endif
