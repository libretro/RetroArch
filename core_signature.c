/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <crypto/pk.h>
#include <lrc_hash.h>
#include <retro_atomic.h>
#include <string/stdstring.h>

#include "core_signature.h"
#include "core_signing_keys.h"

#ifdef HAVE_CORE_SIGNING_TEST_KEY
#define CORE_SIGNING_KEYS_ALL (CORE_SIGNING_KEYS_COUNT + 1)
#else
#define CORE_SIGNING_KEYS_ALL CORE_SIGNING_KEYS_COUNT
#endif

#if CORE_SIGNING_KEYS_ALL > 0
static const uint8_t core_signing_keys[CORE_SIGNING_KEYS_ALL][CORE_SIG_KEY_LEN] =
{
#if CORE_SIGNING_KEYS_COUNT > 0
   CORE_SIGNING_KEYS_LIST,
#endif
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CORE_SIGNING_TEST_KEY
#endif
};
#endif

const uint8_t *core_sig_builtin_keys(size_t *num_keys)
{
#if CORE_SIGNING_KEYS_ALL > 0
   *num_keys = CORE_SIGNING_KEYS_ALL;
   return core_signing_keys[0];
#else
   *num_keys = 0;
   return NULL;
#endif
}

static uint64_t core_get_be(const uint8_t *p, unsigned bytes)
{
   uint64_t v = 0;
   unsigned i;
   for (i = 0; i < bytes; i++)
      v = (v << 8) | p[i];
   return v;
}

static void core_sig_sha256(const uint8_t *buf, size_t len, uint8_t *out)
{
   struct sha256_state sha;
   sha256_stream_init(&sha, 0);
   sha256_stream_update(&sha, buf, len);
   sha256_stream_final(&sha, out);
}

void core_sig_key_id(const uint8_t *key, uint8_t *id)
{
   uint8_t digest[CORE_SHA256_LEN];
   core_sig_sha256(key, CORE_SIG_KEY_LEN, digest);
   memcpy(id, digest, CORE_SIG_KEY_ID_LEN);
}

bool core_sig_parse(struct core_sig *sig, const uint8_t *buf, size_t len)
{
   size_t n;
   if (     len < CORE_SIG_FIXED_LEN
         || memcmp(buf, CORE_SIG_MAGIC, sizeof(CORE_SIG_MAGIC) - 1))
      return false;
   n = buf[CORE_SIG_FIXED_LEN - 1];
   /* p256_ecdsa_verify takes no lengths: exactly one r || s, no more */
   if (     n == 0
         || len != CORE_SIG_FIXED_LEN + n + CORE_SIG_RS_LEN
         || memchr(buf + CORE_SIG_FIXED_LEN, '\0', n))
      return false;
   /* The rollback rule reads 0 as unsigned, never too old */
   if (!core_get_be(buf + 16, 8))
      return false;
   memcpy(sig->key_id, buf + 8, CORE_SIG_KEY_ID_LEN);
   sig->build_time = core_get_be(buf + 16, 8);
   memcpy(sig->core_sha256, buf + 24, CORE_SHA256_LEN);
   memcpy(sig->name, buf + CORE_SIG_FIXED_LEN, n);
   sig->name[n] = '\0';
   memcpy(sig->r, buf + CORE_SIG_FIXED_LEN + n, 32);
   memcpy(sig->s, buf + CORE_SIG_FIXED_LEN + n + 32, 32);
   core_sig_sha256(buf, CORE_SIG_FIXED_LEN + n, sig->digest);
   return true;
}

enum core_check_result core_sig_verify(const struct core_sig *sig,
      const char *file_name, const uint8_t *core_sha256,
      const uint8_t *keys, size_t num_keys)
{
   uint8_t id[CORE_SIG_KEY_ID_LEN];
   size_t i;
   if (!string_is_equal(sig->name, file_name))
      return CORE_CHECK_MODIFIED;
   for (i = 0; i < num_keys; i++)
   {
      core_sig_key_id(keys + i * CORE_SIG_KEY_LEN, id);
      if (!memcmp(id, sig->key_id, CORE_SIG_KEY_ID_LEN))
         break;
   }
   if (i == num_keys)
      return CORE_CHECK_UNKNOWN_KEY;
   if (     !memcmp(sig->core_sha256, core_sha256, CORE_SHA256_LEN)
         && p256_ecdsa_verify(keys + i * CORE_SIG_KEY_LEN, sig->digest, CORE_SHA256_LEN,
               sig->r, sig->s) == 0)
      return CORE_CHECK_OK;
   return CORE_CHECK_MODIFIED;
}

bool core_identity_equal(const struct core_identity *a,
      const struct core_identity *b)
{
   return   a->dev        == b->dev
         && a->size       == b->size
         && a->mtime      == b->mtime
         && a->mtime_nsec == b->mtime_nsec
         && a->ctime      == b->ctime
         && a->ctime_nsec == b->ctime_nsec
         && a->verity_len == b->verity_len
         && a->verity_len <= CORE_ID_VERITY_MAX
         && !memcmp(a->ino, b->ino, sizeof(a->ino))
         && !memcmp(a->verity, b->verity, a->verity_len);
}

bool core_fs_magic_keeps_ctime(unsigned long f_type)
{
   /* ext2/3/4, xfs, btrfs, f2fs, tmpfs, zfs, bcachefs */
   static const unsigned long kept[] = { 0xEF53UL, 0x58465342UL,
      0x9123683EUL, 0xF2F52010UL, 0x01021994UL, 0x2FC12FC1UL,
      0xCA451A4EUL };
   size_t i;
   for (i = 0; i < sizeof(kept) / sizeof(kept[0]); i++)
      if (f_type == kept[i])
         return true;
   return false;
}

bool core_fs_name_keeps_ctime(const char *name)
{
   static const char *const kept[] = { "apfs", "hfs", "ufs", "ffs", "zfs",
      "tmpfs" };
   size_t i;
   for (i = 0; i < sizeof(kept) / sizeof(kept[0]); i++)
      if (string_is_equal(name, kept[i]))
         return true;
   return false;
}

struct core_trust_newest
{
   uint64_t build_time;
   uint8_t  folder[CORE_SHA256_LEN];
   char     name[CORE_SIG_NAME_MAX + 1];
};

struct core_trust_set
{
   struct core_trust_record *records;
   uint8_t (*hashes)[CORE_SHA256_LEN];
   struct core_trust_newest *newest;
   unsigned num_records;
   unsigned num_hashes;
   unsigned num_newest;
   unsigned flags;
   /* The store's and RetroArch's devices, as core_trust_t has them */
   uint64_t dev;
   uint64_t self_dev;
   bool has_dev;
   bool has_self_dev;
   retro_atomic_int_t refs;
};

struct core_trust
{
   core_trust_set_t *cur;
   /* cur as last loaded or saved */
   core_trust_set_t *base;
   /* Given to each set published */
   uint64_t dev;
   uint64_t self_dev;
   bool has_dev;
   bool has_self_dev;
   bool read_only;
   bool writing;
   bool dirty;
};

struct core_trust_write
{
   core_trust_set_t *cur;
   core_trust_set_t *base;
   /* cur less the hashes the file had no room for, when there were any */
   core_trust_set_t *kept;
};

static void core_trust_set_free(core_trust_set_t *s)
{
   free(s->records);
   free(s->hashes);
   free(s->newest);
   free(s);
}

/* Room for one more of each than asked, for the change a clone is for */
static core_trust_set_t *core_trust_set_alloc(unsigned records,
      unsigned hashes, unsigned newest)
{
   core_trust_set_t *s = (core_trust_set_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->records = (struct core_trust_record*)calloc(records + 1,
         sizeof(*s->records));
   s->hashes  = (uint8_t (*)[CORE_SHA256_LEN])calloc(hashes + 1,
         CORE_SHA256_LEN);
   s->newest  = (struct core_trust_newest*)calloc(newest + 1,
         sizeof(*s->newest));
   if (!s->records || !s->hashes || !s->newest)
   {
      core_trust_set_free(s);
      return NULL;
   }
   retro_atomic_int_init(&s->refs, 1);
   return s;
}

static core_trust_set_t *core_trust_set_clone(const core_trust_set_t *src)
{
   core_trust_set_t *s = core_trust_set_alloc(src->num_records,
         src->num_hashes, src->num_newest);
   if (!s)
      return NULL;
   memcpy(s->records, src->records, src->num_records * sizeof(*s->records));
   memcpy(s->hashes, src->hashes, src->num_hashes * CORE_SHA256_LEN);
   memcpy(s->newest, src->newest, src->num_newest * sizeof(*s->newest));
   s->num_records = src->num_records;
   s->num_hashes  = src->num_hashes;
   s->num_newest  = src->num_newest;
   s->flags       = src->flags;
   return s;
}

void core_trust_set_put(core_trust_set_t *s)
{
   if (s && retro_atomic_fetch_sub_int(&s->refs, 1) == 1)
      core_trust_set_free(s);
}

static core_trust_set_t *core_trust_set_ref(core_trust_set_t *s)
{
   retro_atomic_fetch_add_int(&s->refs, 1);
   return s;
}

core_trust_set_t *core_trust_get(core_trust_t *t)
{
   return core_trust_set_ref(t->cur);
}

static void core_trust_publish(core_trust_t *t, core_trust_set_t *s)
{
   s->dev          = t->dev;
   s->self_dev     = t->self_dev;
   s->has_dev      = t->has_dev;
   s->has_self_dev = t->has_self_dev;
   core_trust_set_put(t->cur);
   t->cur = s;
}

static void core_trust_rebase(core_trust_t *t)
{
   core_trust_set_put(t->base);
   t->base = core_trust_set_ref(t->cur);
}

core_trust_t *core_trust_new(void)
{
   core_trust_t *t = (core_trust_t*)calloc(1, sizeof(*t));
   if (!t)
      return NULL;
   if (!(t->cur = core_trust_set_alloc(0, 0, 0)))
   {
      free(t);
      return NULL;
   }
   t->base = core_trust_set_ref(t->cur);
   return t;
}

void core_trust_free(core_trust_t *t)
{
   if (!t)
      return;
   core_trust_set_put(t->cur);
   core_trust_set_put(t->base);
   free(t);
}

unsigned core_trust_flags(const core_trust_set_t *s)
{
   return s->flags;
}

static const struct core_trust_record *core_trust_find(
      const core_trust_set_t *s, const struct core_identity *id,
      const char *name)
{
   unsigned i;
   for (i = 0; i < s->num_records; i++)
      if (     core_identity_equal(&s->records[i].id, id)
            && string_is_equal(s->records[i].name, name))
         return &s->records[i];
   return NULL;
}

bool core_trust_has_identity(const core_trust_set_t *s,
      const struct core_identity *id, const char *name)
{
   return core_trust_find(s, id, name) != NULL;
}

bool core_trust_has_hash(const core_trust_set_t *s, const uint8_t *sha256)
{
   unsigned i;
   for (i = 0; i < s->num_hashes; i++)
      if (!memcmp(s->hashes[i], sha256, CORE_SHA256_LEN))
         return true;
   return false;
}

uint64_t core_trust_newest(const core_trust_set_t *s, const uint8_t *folder,
      const char *name)
{
   unsigned i;
   for (i = 0; i < s->num_newest; i++)
      if (     string_is_equal(s->newest[i].name, name)
            && !memcmp(s->newest[i].folder, folder, CORE_SHA256_LEN))
         return s->newest[i].build_time;
   return 0;
}

/* A signed build older than the newest seen in its folder */
static bool core_trust_too_old(const core_trust_set_t *s,
      const struct core_trust_record *rec, const uint8_t *folder)
{
   return   rec->build_time
         && !(rec->flags & CORE_TRUST_RECORD_ANY_AGE)
         && rec->build_time < core_trust_newest(s, folder, rec->name);
}

/* Of a file whose kernel keeps its ctime, on the store's own device or
 * RetroArch's */
static bool core_trust_may_record(const core_trust_set_t *s,
      const struct core_identity *id)
{
   return id->ctime_kept
      && (     (s->has_dev      && id->dev == s->dev)
            || (s->has_self_dev && id->dev == s->self_dev));
}

bool core_check_record(const core_trust_set_t *s, const struct core_check *c)
{
   const struct core_trust_record *r = core_trust_may_record(s, &c->rec.id)
      ? core_trust_find(s, &c->rec.id, c->rec.name) : NULL;
   return r && !core_trust_too_old(s, r, c->folder);
}

/* One record per file; when full the oldest goes, and its core is
 * hashed again next time */
static struct core_trust_record *core_trust_put_record(core_trust_set_t *s,
      const struct core_trust_record *rec)
{
   unsigned i;
   for (i = 0; i < s->num_records; i++)
      if (     s->records[i].id.dev == rec->id.dev
            && !memcmp(s->records[i].id.ino, rec->id.ino,
                  sizeof(rec->id.ino)))
         break;
   if (i == s->num_records)
   {
      if (s->num_records == CORE_TRUST_MAX_RECORDS)
      {
         memmove(s->records, s->records + 1,
               (CORE_TRUST_MAX_RECORDS - 1) * sizeof(*s->records));
         i = CORE_TRUST_MAX_RECORDS - 1;
      }
      else
         s->num_records++;
   }
   s->records[i] = *rec;
   return &s->records[i];
}

/* The entry a new one replaces when the list is full: the oldest build.
 * Ties go to the lower folder key, then the lower name, so the list's
 * order doesn't matter. */
static unsigned core_trust_newest_victim(const core_trust_set_t *s)
{
   unsigned i;
   unsigned victim = 0;
   for (i = 1; i < s->num_newest; i++)
   {
      const struct core_trust_newest *e = &s->newest[i];
      const struct core_trust_newest *v = &s->newest[victim];
      int key = memcmp(e->folder, v->folder, CORE_SHA256_LEN);
      if (     e->build_time < v->build_time
            || (e->build_time == v->build_time
               && (key < 0 || (key == 0 && strcmp(e->name, v->name) < 0))))
         victim = i;
   }
   return victim;
}

static void core_trust_raise(core_trust_set_t *s, const uint8_t *folder,
      const char *name, uint64_t build_time)
{
   unsigned i;
   for (i = 0; i < s->num_newest; i++)
      if (     string_is_equal(s->newest[i].name, name)
            && !memcmp(s->newest[i].folder, folder, CORE_SHA256_LEN))
      {
         if (build_time > s->newest[i].build_time)
            s->newest[i].build_time = build_time;
         return;
      }
   if (s->num_newest == CORE_TRUST_MAX_NEWEST)
      i = core_trust_newest_victim(s);
   else
      s->num_newest++;
   s->newest[i].build_time = build_time;
   memcpy(s->newest[i].folder, folder, CORE_SHA256_LEN);
   strlcpy(s->newest[i].name, name, sizeof(s->newest[i].name));
}

bool core_trust_apply(core_trust_t *t, const struct core_check *c)
{
   core_trust_set_t *s;
   bool record = c->record && core_trust_may_record(t->cur, &c->rec.id);
   if (!record && !c->raise_newest)
      return true;
   /* Against now, not the set the check saw: another check may have
    * raised it since */
   if (     string_is_empty(c->rec.name)
         || (record && c->rec.id.verity_len > CORE_ID_VERITY_MAX)
         || (record && core_trust_too_old(t->cur, &c->rec, c->folder))
         || !(s = core_trust_set_clone(t->cur)))
      return false;
   if (record)
      core_trust_put_record(s, &c->rec);
   if (c->raise_newest)
      core_trust_raise(s, c->folder, c->rec.name, c->rec.build_time);
   core_trust_publish(t, s);
   return true;
}

bool core_trust_always(core_trust_t *t, const struct core_check *c)
{
   core_trust_set_t *s;
   bool known = core_trust_has_hash(t->cur, c->rec.sha256);
   if (     string_is_empty(c->rec.name)
         || c->result == CORE_CHECK_ERROR
         || c->rec.id.verity_len > CORE_ID_VERITY_MAX
         || (!known && t->cur->num_hashes == CORE_TRUST_MAX_HASHES)
         || !(s = core_trust_set_clone(t->cur)))
      return false;
   if (!known)
      memcpy(s->hashes[s->num_hashes++], c->rec.sha256, CORE_SHA256_LEN);
   if (core_trust_may_record(t->cur, &c->rec.id))
      core_trust_put_record(s, &c->rec)->flags |= CORE_TRUST_RECORD_ANY_AGE;
   core_trust_publish(t, s);
   return true;
}

bool core_trust_set_device(core_trust_t *t, const uint64_t *dev,
      const uint64_t *self_dev)
{
   core_trust_set_t *s;
   t->dev          = dev      ? *dev      : 0;
   t->self_dev     = self_dev ? *self_dev : 0;
   t->has_dev      = dev      != NULL;
   t->has_self_dev = self_dev != NULL;
   if (!(s = core_trust_set_clone(t->cur)))
      return false;
   core_trust_publish(t, s);
   return true;
}

bool core_trust_set_flag(core_trust_t *t, unsigned flag, bool on)
{
   core_trust_set_t *s;
   if (((t->cur->flags & flag) != 0) == on)
      return true;
   if (!(s = core_trust_set_clone(t->cur)))
      return false;
   if (on)
      s->flags |= flag;
   else
      s->flags &= ~flag;
   core_trust_publish(t, s);
   return true;
}

static uint8_t *core_trust_put_be(uint8_t *p, uint64_t v, unsigned bytes)
{
   unsigned i;
   for (i = 0; i < bytes; i++)
      p[i] = (uint8_t)(v >> (8 * (bytes - 1 - i)));
   return p + bytes;
}

static uint8_t *core_trust_put_name(uint8_t *p, const char *name)
{
   size_t n = strlen(name);
   *p++     = (uint8_t)n;
   memcpy(p, name, n);
   return p + n;
}

static bool core_trust_get_name(const uint8_t **p, const uint8_t *end,
      char *name)
{
   size_t n;
   if (*p >= end)
      return false;
   n = **p;
   if (     n == 0
         || (size_t)(end - *p - 1) < n
         || memchr(*p + 1, '\0', n))
      return false;
   memcpy(name, *p + 1, n);
   name[n] = '\0';
   *p     += 1 + n;
   return true;
}

static uint8_t *core_trust_serialize_set(const core_trust_set_t *s,
      size_t *len)
{
   size_t cap = CORE_TRUST_HEAD_LEN + CORE_SHA256_LEN
      + s->num_records * (CORE_TRUST_RECORD_FIXED + CORE_SIG_NAME_MAX)
      + s->num_hashes  * CORE_SHA256_LEN
      + s->num_newest  * (CORE_TRUST_NEWEST_FIXED + CORE_SIG_NAME_MAX);
   uint8_t *buf = (uint8_t*)malloc(cap);
   uint8_t *p;
   unsigned i;
   if (!buf)
      return NULL;
   memcpy(buf, CORE_TRUST_MAGIC, sizeof(CORE_TRUST_MAGIC) - 1);
   p = core_trust_put_be(buf + 8, s->flags, 4);
   p = core_trust_put_be(p, s->num_records, 2);
   p = core_trust_put_be(p, s->num_hashes, 2);
   p = core_trust_put_be(p, s->num_newest, 2);
   for (i = 0; i < s->num_records; i++)
   {
      const struct core_trust_record *r = &s->records[i];
      p    = core_trust_put_be(p, r->id.dev, 8);
      memcpy(p, r->id.ino, sizeof(r->id.ino));
      p   += sizeof(r->id.ino);
      p    = core_trust_put_be(p, r->id.size, 8);
      p    = core_trust_put_be(p, (uint64_t)r->id.mtime, 8);
      p    = core_trust_put_be(p, r->id.mtime_nsec, 4);
      p    = core_trust_put_be(p, (uint64_t)r->id.ctime, 8);
      p    = core_trust_put_be(p, r->id.ctime_nsec, 4);
      *p++ = r->id.verity_len;
      memcpy(p, r->id.verity, CORE_ID_VERITY_MAX);
      p   += CORE_ID_VERITY_MAX;
      p    = core_trust_put_be(p, r->build_time, 8);
      memcpy(p, r->sha256, CORE_SHA256_LEN);
      p   += CORE_SHA256_LEN;
      *p++ = r->flags;
      p    = core_trust_put_name(p, r->name);
   }
   for (i = 0; i < s->num_hashes; i++, p += CORE_SHA256_LEN)
      memcpy(p, s->hashes[i], CORE_SHA256_LEN);
   for (i = 0; i < s->num_newest; i++)
   {
      p  = core_trust_put_be(p, s->newest[i].build_time, 8);
      memcpy(p, s->newest[i].folder, CORE_SHA256_LEN);
      p += CORE_SHA256_LEN;
      p  = core_trust_put_name(p, s->newest[i].name);
   }
   core_sig_sha256(buf, (size_t)(p - buf), p);
   *len = (size_t)(p - buf) + CORE_SHA256_LEN;
   return buf;
}

uint8_t *core_trust_serialize(const core_trust_t *t, size_t *len)
{
   return core_trust_serialize_set(t->cur, len);
}

/* The version digit after RACTRST */
#define CORE_TRUST_VER_AT (sizeof(CORE_TRUST_MAGIC) - 2)

enum core_trust_load core_trust_parse(core_trust_t *t,
      const uint8_t *buf, size_t len)
{
   uint8_t digest[CORE_SHA256_LEN];
   const uint8_t *p, *end;
   core_trust_set_t *s = NULL;
   unsigned nr, nh, nn, i;
   if (!buf)
      goto unreadable;
   /* A newer version's: kept for it */
   if (     len > CORE_TRUST_VER_AT
         && !memcmp(buf, CORE_TRUST_MAGIC, CORE_TRUST_VER_AT)
         && buf[CORE_TRUST_VER_AT] >  CORE_TRUST_MAGIC[CORE_TRUST_VER_AT]
         && buf[CORE_TRUST_VER_AT] <= '9')
      goto unreadable;
   if (     len < CORE_TRUST_HEAD_LEN + CORE_SHA256_LEN
         || len > CORE_TRUST_FILE_MAX
         || memcmp(buf, CORE_TRUST_MAGIC, sizeof(CORE_TRUST_MAGIC) - 1))
      goto damaged;
   end = buf + len - CORE_SHA256_LEN;
   core_sig_sha256(buf, (size_t)(end - buf), digest);
   if (memcmp(digest, end, CORE_SHA256_LEN))
      goto damaged;
   nr = (unsigned)core_get_be(buf + 12, 2);
   nh = (unsigned)core_get_be(buf + 14, 2);
   nn = (unsigned)core_get_be(buf + 16, 2);
   if (     nr > CORE_TRUST_MAX_RECORDS
         || nh > CORE_TRUST_MAX_HASHES
         || nn > CORE_TRUST_MAX_NEWEST)
      goto damaged;
   /* Out of memory is not damage: the file stays */
   if (!(s = core_trust_set_alloc(nr, nh, nn)))
      goto unreadable;
   s->flags = (unsigned)core_get_be(buf + 8, 4);
   p        = buf + CORE_TRUST_HEAD_LEN;
   for (i = 0; i < nr; i++)
   {
      struct core_trust_record *r = &s->records[i];
      if ((size_t)(end - p) < CORE_TRUST_RECORD_FIXED)
         goto damaged;
      r->id.dev        = core_get_be(p, 8);
      memcpy(r->id.ino, p + 8, sizeof(r->id.ino));
      r->id.size       = core_get_be(p + 24, 8);
      r->id.mtime      = (int64_t)core_get_be(p + 32, 8);
      r->id.mtime_nsec = (uint32_t)core_get_be(p + 40, 4);
      r->id.ctime      = (int64_t)core_get_be(p + 44, 8);
      r->id.ctime_nsec = (uint32_t)core_get_be(p + 52, 4);
      r->id.verity_len = p[56];
      memcpy(r->id.verity, p + 57, CORE_ID_VERITY_MAX);
      r->build_time    = core_get_be(p + 57 + CORE_ID_VERITY_MAX, 8);
      memcpy(r->sha256, p + 65 + CORE_ID_VERITY_MAX, CORE_SHA256_LEN);
      r->flags         = p[65 + CORE_ID_VERITY_MAX + CORE_SHA256_LEN];
      p               += CORE_TRUST_RECORD_FIXED - 1;
      if (     r->id.verity_len > CORE_ID_VERITY_MAX
            || !core_trust_get_name(&p, end, r->name))
         goto damaged;
   }
   if ((size_t)(end - p) < (size_t)nh * CORE_SHA256_LEN)
      goto damaged;
   memcpy(s->hashes, p, (size_t)nh * CORE_SHA256_LEN);
   p += (size_t)nh * CORE_SHA256_LEN;
   for (i = 0; i < nn; i++)
   {
      if ((size_t)(end - p) < 8 + CORE_SHA256_LEN)
         goto damaged;
      s->newest[i].build_time = core_get_be(p, 8);
      memcpy(s->newest[i].folder, p + 8, CORE_SHA256_LEN);
      p += 8 + CORE_SHA256_LEN;
      if (!core_trust_get_name(&p, end, s->newest[i].name))
         goto damaged;
   }
   if (p != end)
      goto damaged;
   s->num_records = nr;
   s->num_hashes  = nh;
   s->num_newest  = nn;
   core_trust_publish(t, s);
   core_trust_rebase(t);
   t->read_only   = false;
   return CORE_TRUST_LOAD_OK;

damaged:
   if (s)
      core_trust_set_free(s);
   t->read_only = true;
   return CORE_TRUST_LOAD_DAMAGED;

unreadable:
   t->read_only = true;
   return CORE_TRUST_LOAD_UNREADABLE;
}

bool core_trust_reset(core_trust_t *t)
{
   core_trust_set_t *s = core_trust_set_alloc(0, 0, 0);
   if (!s)
      return false;
   core_trust_publish(t, s);
   core_trust_rebase(t);
   t->read_only = false;
   return true;
}

core_trust_write_t *core_trust_write_begin(core_trust_t *t)
{
   core_trust_write_t *w;
   if (t->read_only)
      return NULL;
   if (t->writing)
   {
      t->dirty = true;
      return NULL;
   }
   if (!(w = (core_trust_write_t*)malloc(sizeof(*w))))
      return NULL;
   w->cur     = core_trust_get(t);
   w->base    = core_trust_set_ref(t->base);
   w->kept    = NULL;
   t->writing = true;
   return w;
}

bool core_trust_write_end(core_trust_t *t, core_trust_write_t *w,
      bool written)
{
   bool again = t->dirty;
   /* Not written: the changes go out with the next write, as does a hash
    * that found no room */
   if (written)
   {
      core_trust_set_put(t->base);
      t->base = w->kept ? w->kept : core_trust_set_ref(w->cur);
      w->kept = NULL;
   }
   core_trust_set_put(w->kept);
   core_trust_set_put(w->cur);
   core_trust_set_put(w->base);
   free(w);
   t->writing = false;
   t->dirty   = false;
   return again;
}

static bool core_trust_record_equal(const struct core_trust_record *a,
      const struct core_trust_record *b)
{
   return   core_identity_equal(&a->id, &b->id)
         && a->build_time == b->build_time
         && a->flags      == b->flags
         && !memcmp(a->sha256, b->sha256, CORE_SHA256_LEN)
         && string_is_equal(a->name, b->name);
}

static bool core_trust_has_record(const core_trust_set_t *s,
      const struct core_trust_record *rec)
{
   unsigned i;
   for (i = 0; i < s->num_records; i++)
      if (core_trust_record_equal(&s->records[i], rec))
         return true;
   return false;
}

/* *kept, a copy of cur made on first use, without sha256 */
static bool core_trust_unhash(core_trust_set_t **kept,
      const core_trust_set_t *cur, const uint8_t *sha256)
{
   core_trust_set_t *k;
   unsigned i;
   if (!*kept && !(*kept = core_trust_set_clone(cur)))
      return false;
   k = *kept;
   for (i = 0; i < k->num_hashes; i++)
      if (!memcmp(k->hashes[i], sha256, CORE_SHA256_LEN))
      {
         memmove(k->hashes[i], k->hashes[i + 1],
               (k->num_hashes - i - 1) * CORE_SHA256_LEN);
         k->num_hashes--;
         break;
      }
   return true;
}

/* disk, with what changed from base to cur. A new hash with no room
 * leaves *kept, so it is still a change for the next write. */
static core_trust_set_t *core_trust_merge(const core_trust_set_t *disk,
      const core_trust_set_t *cur, const core_trust_set_t *base,
      core_trust_set_t **kept)
{
   unsigned changed = cur->flags ^ base->flags;
   unsigned i;
   core_trust_set_t *s = core_trust_set_alloc(CORE_TRUST_MAX_RECORDS,
         CORE_TRUST_MAX_HASHES, CORE_TRUST_MAX_NEWEST);
   if (!s)
      return NULL;
   memcpy(s->records, disk->records, disk->num_records * sizeof(*s->records));
   memcpy(s->newest, disk->newest, disk->num_newest * sizeof(*s->newest));
   s->num_records = disk->num_records;
   s->num_newest  = disk->num_newest;
   s->flags       = (disk->flags & ~changed) | (cur->flags & changed);
   for (i = 0; i < disk->num_hashes; i++)
      if (     core_trust_has_hash(cur, disk->hashes[i])
            || !core_trust_has_hash(base, disk->hashes[i]))
         memcpy(s->hashes[s->num_hashes++], disk->hashes[i],
               CORE_SHA256_LEN);
   for (i = 0; i < cur->num_hashes; i++)
   {
      if (     core_trust_has_hash(base, cur->hashes[i])
            || core_trust_has_hash(s, cur->hashes[i]))
         continue;
      if (s->num_hashes < CORE_TRUST_MAX_HASHES)
         memcpy(s->hashes[s->num_hashes++], cur->hashes[i],
               CORE_SHA256_LEN);
      else if (!core_trust_unhash(kept, cur, cur->hashes[i]))
      {
         core_trust_set_free(s);
         return NULL;
      }
   }
   /* Only what this instance raised; the rest may be what another evicted */
   for (i = 0; i < cur->num_newest; i++)
      if (cur->newest[i].build_time > core_trust_newest(base,
               cur->newest[i].folder, cur->newest[i].name))
         core_trust_raise(s, cur->newest[i].folder, cur->newest[i].name,
               cur->newest[i].build_time);
   for (i = 0; i < cur->num_records; i++)
      if (!core_trust_has_record(base, &cur->records[i]))
         core_trust_put_record(s, &cur->records[i]);
   return s;
}

uint8_t *core_trust_write_bytes(core_trust_write_t *w,
      const core_trust_t *disk, size_t *len)
{
   core_trust_set_t *s;
   uint8_t *buf;
   if (!disk)
      return core_trust_serialize_set(w->cur, len);
   if (!(s = core_trust_merge(disk->cur, w->cur, w->base, &w->kept)))
      return NULL;
   buf = core_trust_serialize_set(s, len);
   core_trust_set_free(s);
   return buf;
}

bool core_mode_writable(const struct core_owner *who, unsigned long uid,
      unsigned long gid, unsigned mode, bool read_only_mount)
{
   size_t i;
   if (read_only_mount)
      return false;
   /* An owner can chmod, whatever the bits say */
   if (who->uid == 0 || who->uid == uid || (mode & 0002))
      return true;
   if (mode & 0020)
      for (i = 0; i < who->num_gids; i++)
         if (who->gids[i] == gid)
            return true;
   return false;
}

bool core_mode_exempt(const struct core_owner *who, unsigned long self_uid,
      unsigned long uid, unsigned long gid, unsigned mode,
      bool read_only_mount)
{
   /* Its group can't rename or replace what it doesn't own */
   bool sticky_dir = (mode & 0171000) == 0041000;
   /* With no overflow uid known, RetroArch's owner may be it */
   bool self_known = who->overflow_uid != CORE_UID_UNKNOWN;
#if defined(__linux__) || defined(ANDROID)
   bool root_group = gid == 0;
#else
   /* gid 0 is wheel on macOS and the BSDs: admin users */
   bool root_group = false;
#endif
   return !core_mode_writable(who, uid, gid, mode, read_only_mount)
      && (read_only_mount || ((uid == 0 || (self_known && uid == self_uid))
            && uid != who->overflow_uid
            && (root_group || !(mode & 0020) || sticky_dir)));
}

void core_check_finish(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys,
      const uint8_t *sha256, const uint8_t *sig_buf, size_t sig_len,
      unsigned flags, struct core_check *c)
{
   struct core_sig sig;
   memcpy(c->rec.sha256, sha256, CORE_SHA256_LEN);
   c->rec.build_time = 0;
   c->rec.flags      = (flags & CORE_CHECK_ALLOW_OLDER)
      ? CORE_TRUST_RECORD_ANY_AGE : 0;
   c->record         = false;
   c->raise_newest   = false;
   if (core_trust_has_hash(s, sha256))
   {
      c->result = CORE_CHECK_OK;
      c->record = true;
   }
   else if (!sig_buf || !sig_len)
      c->result = CORE_CHECK_UNSIGNED;
   else if (!core_sig_parse(&sig, sig_buf, sig_len))
      c->result = CORE_CHECK_MODIFIED;
   else if ((c->result = core_sig_verify(&sig, c->rec.name, sha256,
               keys, num_keys)) == CORE_CHECK_OK)
   {
      c->rec.build_time = sig.build_time;
      if (     !(flags & CORE_CHECK_ALLOW_OLDER)
            && sig.build_time < core_trust_newest(s, c->folder, c->rec.name))
         c->result = CORE_CHECK_OLDER;
      else
      {
         c->record       = true;
         c->raise_newest = true;
      }
   }
}
