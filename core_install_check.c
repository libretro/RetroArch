/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "core_install_check.h"

bool core_install_check_begin(struct core_install_check *ic,
      const core_trust_set_t *s, const char *staged,
      const char *install_path)
{
   memset(ic, 0, sizeof(*ic));
   ic->step     = -1;
   ic->c.result = CORE_CHECK_ERROR;
   strlcpy(ic->c.rec.name, path_basename_nocompression(staged),
         sizeof(ic->c.rec.name));
   if (!(ic->f = core_file_open(staged)))
      return false;
   /* A record or the folder rule vouches for a file in place, not for
    * a download: only ERROR, or checks off, ends it before hashing */
   if (     core_check_quick(s, ic->f, staged, &ic->c)
         && (     ic->c.result == CORE_CHECK_ERROR
               || (core_trust_flags(s) & CORE_TRUST_CHECK_OFF)))
   {
      core_install_check_free(ic);
      return false;
   }
   if (     !core_folder_key(install_path, ic->c.folder)
         || !core_hash_init(&ic->h))
   {
      ic->c.result = CORE_CHECK_ERROR;
      core_install_check_free(ic);
      return false;
   }
   ic->c.result = CORE_CHECK_ERROR;
   ic->hashing  = true;
   ic->step     = 1;
   return true;
}

bool core_install_check_step(struct core_install_check *ic)
{
   if (ic->step > 0)
      ic->step = core_hash_step(&ic->h, ic->f);
   return ic->step <= 0;
}

void core_install_check_end(struct core_install_check *ic,
      const core_trust_set_t *s, const char *staged, unsigned flags)
{
   size_t num_keys     = 0;
   const uint8_t *keys = core_sig_builtin_keys(&num_keys);
   core_file_close(ic->f);
   ic->f       = NULL;
   ic->hashing = false;
   core_check_hashed(s, keys, num_keys, &ic->h, ic->step, staged,
         flags, &ic->c);
}

void core_install_check_free(struct core_install_check *ic)
{
   if (ic->hashing)
      core_hash_free(&ic->h);
   ic->hashing = false;
   core_file_close(ic->f);
   ic->f       = NULL;
}

void core_install_check_end_sig(struct core_install_check *ic,
      const core_trust_set_t *s, const char *staged)
{
   /* Empty, so no trusted hash ends the check before the .sig; private
    * to this call (core_trust_new() is pure), not RetroArch's store */
   core_trust_t *none     = core_trust_new();
   core_trust_set_t *bare = none ? core_trust_get(none) : NULL;
   if (!bare)
   {
      core_install_check_free(ic);
      ic->c.result = CORE_CHECK_ERROR;
   }
   else
   {
      core_install_check_end(ic, bare, staged, 0);
      if (     ic->c.result == CORE_CHECK_OK
            && ic->c.rec.build_time
               < core_trust_newest(s, ic->c.folder, ic->c.rec.name))
      {
         ic->c.result       = CORE_CHECK_OLDER;
         ic->c.record       = false;
         ic->c.raise_newest = false;
      }
      core_trust_set_put(bare);
   }
   if (none)
      core_trust_free(none);
}

bool core_install_copy_sig(const char *src, const char *dst)
{
   uint8_t buf[CORE_SIG_MAX_LEN + 1];
   bool ok       = false;
   char *dst_sig = NULL;
   int64_t _len  = core_sig_read(src, buf);
   if (_len >= 0 && (dst_sig = core_sig_path(dst)))
      ok = filestream_write_file(dst_sig, buf, _len);
   free(dst_sig);
   return ok;
}

void core_install_drop_sig(const char *path)
{
   char *sig = core_sig_path(path);
   if (sig && path_is_valid(sig))
      filestream_delete(sig);
   free(sig);
}

/* <sig>.old */
static char *core_install_old_sig_path(const char *sig)
{
   size_t _len = strlen(sig);
   char *old   = (char*)malloc(_len + sizeof(".old"));
   if (old)
   {
      memcpy(old, sig, _len);
      memcpy(old + _len, ".old", sizeof(".old"));
   }
   return old;
}

void core_install_set_aside_sig(const char *path)
{
   char *sig = core_sig_path(path);
   char *old = sig ? core_install_old_sig_path(sig) : NULL;
   /* One a crash left is never put back */
   if (old && path_is_valid(old))
      filestream_delete(old);
   if (     sig
         && path_is_valid(sig)
         && (!old || filestream_rename(sig, old) != 0))
      filestream_delete(sig);
   free(sig);
   free(old);
}

void core_install_end_aside_sig(const char *path, bool replaced)
{
   char *sig = core_sig_path(path);
   char *old = sig ? core_install_old_sig_path(sig) : NULL;
   if (     old
         && path_is_valid(old)
         && (replaced || filestream_rename(old, sig) != 0))
      filestream_delete(old);
   free(sig);
   free(old);
}

bool core_install_has_sig(const char *path)
{
   char *sig = core_sig_path(path);
   bool has  = sig && path_is_valid(sig);
   free(sig);
   return has;
}

void core_install_place_sig(const char *staged, const char *install_path)
{
   char *sig  = core_sig_path(staged);
   char *isig = core_sig_path(install_path);
   if (sig && isig)
   {
      /* None staged, or it won't move: a stale one would say changed */
      if (     !path_is_valid(sig)
            || filestream_rename(sig, isig) != 0)
      {
         if (path_is_valid(isig))
            filestream_delete(isig);
         if (path_is_valid(sig))
            filestream_delete(sig);
      }
   }
   free(sig);
   free(isig);
}

void core_install_identify(const char *install_path, struct core_check *c)
{
   struct core_identity id;
   core_file_t *f = core_file_open(install_path);
   if (     f
         && core_file_identity(f, &id)
         && id.dev        == c->rec.id.dev
         && id.size       == c->rec.id.size
         && !memcmp(id.ino, c->rec.id.ino, sizeof(id.ino))
         /* A rename keeps mtime; a write to the same inode after the
          * hash moves it, and only the owner can set it back */
         && id.mtime      == c->rec.id.mtime
         && id.mtime_nsec == c->rec.id.mtime_nsec)
      c->rec.id = id;
   else
      c->record = false;
   core_file_close(f);
}

void core_install_vet_sig(const struct core_check *c, const char *staged)
{
   uint8_t buf[CORE_SIG_MAX_LEN + 1];
   struct core_check alone;
   size_t num_keys        = 0;
   const uint8_t *keys    = core_sig_builtin_keys(&num_keys);
   core_trust_t *none     = NULL;
   core_trust_set_t *bare = NULL;
   int64_t _len;
   if (c->result != CORE_CHECK_OK || !c->record || c->raise_newest)
      return;
   alone        = *c;
   alone.result = CORE_CHECK_ERROR;
   if (     (_len = core_sig_read(staged, buf)) >= 0
         && (none = core_trust_new())
         && (bare = core_trust_get(none)))
   {
      core_check_finish(bare, keys, num_keys, c->rec.sha256, buf,
            (size_t)_len, 0, &alone);
      core_trust_set_put(bare);
   }
   if (none)
      core_trust_free(none);
   if (alone.result != CORE_CHECK_OK)
      core_install_drop_sig(staged);
}

bool core_install_commit(const char *staged, const char *install_path,
      struct core_check *c)
{
   bool moved;
   core_install_vet_sig(c, staged);
   /* A crash before the new .sig is placed leaves the core without
    * one, which Update Installed Cores fetches */
   core_install_set_aside_sig(install_path);
   moved = filestream_rename(staged, install_path) == 0;
   core_install_end_aside_sig(install_path, moved);
   if (!moved)
      return false;
   core_install_place_sig(staged, install_path);
   core_install_identify(install_path, c);
   return true;
}
