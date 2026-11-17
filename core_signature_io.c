/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_GNU_SOURCE)
/* realpath, O_CLOEXEC and O_PATH; strict C89 hides them */
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#if !defined(__linux__) || defined(ANDROID)
#include <dlfcn.h>
#endif
/* Where one statfs() tells both a file's filesystem and its mount flags:
 * the open asks it once for ctime_kept and read_only together */
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
   || defined(__OpenBSD__) || defined(__DragonFly__)
#define CORE_VOLUME_AT_OPEN
#endif
#if defined(__APPLE__)
#include <limits.h>
#include <CoreFoundation/CoreFoundation.h>
#endif
#endif

#include <compat/strl.h>
#include <retro_atomic.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>

#include "core_signature.h"
#include "file_path_special.h"
#ifdef ANDROID
#include "frontend/drivers/platform_unix.h"
#endif

#define CORE_HASH_CHUNK (64 * 1024)
/* .<pid>.<stamp>.tmp, 64-bit numbers */
#define CORE_TRUST_TEMP_TAIL 48

core_file_t *core_file_open(const char *path)
{
   core_file_t *f = retro_vfs_file_open_impl(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   /* A scheme the VFS opens another way (smb://, cdrom://) drops the
    * hint: such a file is not one that can be checked */
   if (f && !(f->hints & RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE))
   {
      retro_vfs_file_close_impl(f);
      return NULL;
   }
   return f;
}

void core_file_close(core_file_t *f)
{
   if (f)
      retro_vfs_file_close_impl(f);
}

int64_t core_file_read(core_file_t *f, void *buf, size_t len)
{
   int64_t got;
   /* -1 only when nothing was read, so a retry starts where it was */
   do
      got = retro_vfs_file_read_impl(f, buf, len);
#ifdef EINTR
   while (got < 0 && errno == EINTR);
#else
   while (0);
#endif
   return got;
}

#ifndef _WIN32
int core_file_fd(core_file_t *f)
{
   return f->fd;
}
#endif

/* id from the VFS's identity of f, as the trust store keeps it */
static void core_identity_from_vfs(core_file_t *f,
      const struct retro_vfs_file_identity *v, struct core_identity *id)
{
#if defined(CORE_VOLUME_AT_OPEN)
   struct retro_vfs_volume vol;
#endif
   memset(id, 0, sizeof(*id));
   id->dev        = v->dev;
   id->size       = v->size;
   id->mtime      = v->mtime;
   id->ctime      = v->ctime;
   id->mtime_nsec = v->mtime_nsec;
   id->ctime_nsec = v->ctime_nsec;
   memcpy(id->ino, v->id, sizeof(id->ino));
   if (v->flags & RETRO_VFS_IDENTITY_HAS_OWNER)
   {
      id->has_owner = true;
      id->uid       = v->uid;
      id->gid       = v->gid;
      id->mode      = (uint16_t)(v->perm
            | (v->type == RETRO_VFS_IDENTITY_DIRECTORY ? 0040000
            :  v->type == RETRO_VFS_IDENTITY_REGULAR   ? 0100000 : 0));
   }
#ifdef _WIN32
   /* Kept in the store as Windows has them, in seconds from 1601, so
    * the records already made still match. ctime_kept stays false: any
    * writer can set ChangeTime. */
   (void)f;
   id->mtime += (int64_t)11644473600LL;
   if (v->ctime || v->ctime_nsec)
      id->ctime += (int64_t)11644473600LL;
#elif defined(CORE_VOLUME_AT_OPEN)
   if (retro_vfs_file_volume_impl(f, RETRO_VFS_VOLUME_WANT_FLAGS, &vol))
   {
      id->ctime_kept = (vol.flags & RETRO_VFS_VOLUME_CTIME_KEPT) != 0;
      id->read_only  = (vol.flags & RETRO_VFS_VOLUME_READ_ONLY) != 0;
      id->vol_known  = true;
   }
#else
   (void)f;
#endif
}

bool core_file_identity(core_file_t *f, struct core_identity *id)
{
   struct retro_vfs_file_identity v;
   if (!retro_vfs_file_identity_impl(f, &v))
   {
      memset(id, 0, sizeof(*id));
      return false;
   }
   core_identity_from_vfs(f, &v, id);
   return true;
}

core_file_t *core_file_open_id(const char *path, struct core_identity *id)
{
   struct retro_vfs_file_identity v;
   core_file_t *f = retro_vfs_file_open_stable_impl(path, &v);
   if (f)
      core_identity_from_vfs(f, &v, id);
   else
      memset(id, 0, sizeof(*id));
   return f;
}

#ifdef _WIN32
#define CORE_HASH_UNSET  0
#define CORE_HASH_MAPPED 1
#define CORE_HASH_READ   2

/* The file's mapping, from the VFS, to hash in place. Only on a local
 * fixed drive: a read through a mapping fails as an exception, not an
 * error, and a network share or a removable drive can fail mid-file,
 * where a read reports it and the check refuses cleanly. Nothing can
 * shorten the file: its handle denies writes. */
static bool core_hash_map(struct core_hash *h, core_file_t *f)
{
   struct retro_vfs_volume vol;
   int64_t pos, len = 0;
   if (     !retro_vfs_file_volume_impl(f, RETRO_VFS_VOLUME_WANT_KIND, &vol)
         || vol.kind != RETRO_VFS_VOLUME_FIXED)
      return false;
#if !defined(_WIN64)
   /* A 32-bit process keeps its address space for the core it loads:
    * the mapping lives as long as the file is open */
   if (retro_vfs_file_size_impl(f) > 256 * 1024 * 1024)
      return false;
#endif
   if (     (pos = retro_vfs_file_tell_impl(f)) < 0
         || !(h->map = retro_vfs_file_map_impl(f, &len))
         || len <= pos)
   {
      h->map = NULL;
      return false;
   }
   h->start = (uint64_t)pos;
   h->size  = (uint64_t)len;
   return true;
}
#endif

bool core_hash_init(struct core_hash *h)
{
   sha256_stream_init(&h->sha, 0);
   h->done = 0;
#ifdef _WIN32
   h->buf   = NULL;
   h->map   = NULL;
   h->start = 0;
   h->size  = 0;
   h->mode  = CORE_HASH_UNSET;
   return true;
#else
   return (h->buf = (uint8_t*)malloc(CORE_HASH_CHUNK)) != NULL;
#endif
}

int core_hash_step(struct core_hash *h, core_file_t *f)
{
   int64_t got;
#ifdef _WIN32
   if (h->mode == CORE_HASH_UNSET)
   {
      if (core_hash_map(h, f))
         h->mode = CORE_HASH_MAPPED;
      else if (!(h->buf = (uint8_t*)malloc(CORE_HASH_CHUNK)))
         return -1;
      else
         h->mode = CORE_HASH_READ;
   }
   if (h->mode == CORE_HASH_MAPPED)
   {
      uint64_t pos = h->start + h->done;
      uint64_t len = h->size - pos;
      if (pos >= h->size)
         return 0;
      if (len > CORE_HASH_CHUNK)
         len = CORE_HASH_CHUNK;
      sha256_stream_update(&h->sha, h->map + pos, (size_t)len);
      h->done += len;
      return 1;
   }
#endif
   if ((got = core_file_read(f, h->buf, CORE_HASH_CHUNK)) < 0)
      return -1;
   if (got == 0)
      return 0;
   sha256_stream_update(&h->sha, h->buf, (size_t)got);
   h->done += (uint64_t)got;
   return 1;
}

void core_hash_final(struct core_hash *h, uint8_t *sha256)
{
   sha256_stream_final(&h->sha, sha256);
   core_hash_free(h);
}

void core_hash_free(struct core_hash *h)
{
   /* A mapping is the file's, dropped when it closes */
   free(h->buf);
   h->buf = NULL;
#ifdef _WIN32
   h->map = NULL;
#endif
}


bool core_file_verity_measure(core_file_t *f, struct core_identity *id)
{
   size_t n = retro_vfs_file_verity_measure_impl(f, id->verity,
         sizeof(id->verity));
   if (!n)
      return false;
   id->verity_len = (uint8_t)n;
   return true;
}

int core_file_verity_enable(core_file_t *f)
{
   return retro_vfs_file_verity_enable_impl(f);
}

/* 1 a file, 0 missing, -1 anything else: what can't be looked at, a
 * directory, or a FIFO, whose open would wait for a writer */
static int core_trust_file_kind(const char *path)
{
   struct retro_vfs_file_identity v;
   int found = retro_vfs_stat_identity_impl(path, &v);
   if (found <= 0)
      return found;
   return v.type == RETRO_VFS_IDENTITY_REGULAR ? 1 : -1;
}

/* The device of the folder holding path; Windows has no records */
static bool core_trust_folder_dev(const char *path, uint64_t *dev)
{
#ifdef _WIN32
   return false;
#else
   struct retro_vfs_file_identity v;
   size_t n  = (size_t)(path_basename_nocompression(path) - path);
   char *dir = (char*)malloc(n + 2);
   bool ok;
   if (!dir)
      return false;
   memcpy(dir, path, n);
   if (!n)
      dir[n++] = '.';
   dir[n] = '\0';
   if ((ok = retro_vfs_stat_identity_impl(dir, &v) == 1))
      *dev = v.dev;
   free(dir);
   return ok;
#endif
}

/* len bytes of f into buf, or false */
static bool core_file_read_all(core_file_t *f, uint8_t *buf, size_t len)
{
   size_t done = 0;
   while (done < len)
   {
      int64_t got = core_file_read(f, buf + done, len - done);
      if (got <= 0)
         return false;
      done += (size_t)got;
   }
   return true;
}

enum core_trust_load core_trust_load_file(core_trust_t *t,
      const char *path)
{
   uint8_t *buf = NULL;
   bool renamed = false;
   struct core_self self;
   core_file_t *f;
   uint64_t dev;
   int64_t size;
   size_t n;
   char *bad;
   enum core_trust_load ret;
   core_trust_set_device(t, core_trust_folder_dev(path, &dev) ? &dev : NULL,
         core_self_find(&self) ? &self.dev : NULL);
   /* Opened as a core is: a regular file or nothing, never waited on,
    * and its size from the same answer, so no stat of the path first.
    * Only a failed open asks whether there is a file at all. */
   if (!(f = core_file_open(path)))
      return core_trust_file_kind(path)
         ? core_trust_parse(t, NULL, 0) : CORE_TRUST_LOAD_MISSING;
   /* core_trust_parse(t, NULL, 0) is UNREADABLE and read-only */
   if ((size = retro_vfs_file_size_impl(f)) < 0)
   {
      core_file_close(f);
      return core_trust_parse(t, NULL, 0);
   }
   if (size > CORE_TRUST_FILE_MAX)
   {
      /* Too big for ours: only its magic, which may say a newer one's */
      uint8_t head[sizeof(CORE_TRUST_MAGIC) - 1];
      bool got = core_file_read_all(f, head, sizeof(head));
      core_file_close(f);
      if (!got)
         return core_trust_parse(t, NULL, 0);
      ret = core_trust_parse(t, head, sizeof(head));
   }
   else
   {
      /* Never NULL, even for an empty file: that one is damaged, not
       * unreadable */
      bool got = (buf = (uint8_t*)malloc(size ? (size_t)size : 1)) != NULL
         && core_file_read_all(f, buf, (size_t)size);
      core_file_close(f);
      if (!got)
      {
         free(buf);
         return core_trust_parse(t, NULL, 0);
      }
      ret = core_trust_parse(t, buf, (size_t)size);
      free(buf);
   }
   if (ret != CORE_TRUST_LOAD_DAMAGED)
      return ret;
   /* Set aside over any older .bad; what can't be is never written over */
   n = strlen(path) + sizeof(".bad");
   if ((bad = (char*)malloc(n)))
   {
      strlcpy(bad, path, n);
      strlcat(bad, ".bad", n);
      renamed = filestream_rename(path, bad) == 0;
      free(bad);
   }
   if (!renamed || !core_trust_reset(t))
      return core_trust_parse(t, NULL, 0);
   return CORE_TRUST_LOAD_RESET;
}

bool core_trust_temp_name(char *s, size_t len, const char *path,
      unsigned long pid, unsigned long stamp)
{
   char tail[CORE_TRUST_TEMP_TAIL];
   snprintf(tail, sizeof(tail), ".%lu.%lx.tmp", pid, stamp);
   strlcpy(s, path, len);
   return strlcat(s, tail, len) < len;
}

/* As filestream_write_file_atomic, through a temporary of our own */
static bool core_trust_save(const char *path, const uint8_t *buf,
      size_t len)
{
   RFILE *file;
   bool ok;
   size_t n   = strlen(path) + CORE_TRUST_TEMP_TAIL;
   char *temp = (char*)malloc(n);
#ifdef _WIN32
   unsigned long pid = (unsigned long)GetCurrentProcessId();
#else
   unsigned long pid = (unsigned long)getpid();
#endif
   if (     !temp
         || !core_trust_temp_name(temp, n, path, pid,
            (unsigned long)cpu_features_get_time_usec())
         || !(file = filestream_open(temp, RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK)))
   {
      free(temp);
      return false;
   }
   ok = filestream_write(file, buf, (int64_t)len) == (int64_t)len;
   /* A full disk can show only at close */
   if (filestream_close(file) != 0)
      ok = false;
   if (!ok || filestream_rename(temp, path) != 0)
   {
      filestream_delete(temp);
      ok = false;
   }
   free(temp);
   return ok;
}

bool core_trust_write_run(core_trust_write_t *w, const char *path)
{
   core_trust_t *disk = core_trust_new();
   uint8_t *buf       = NULL;
   bool ok            = false;
   size_t len         = 0;
   if (!disk)
      return false;
   switch (core_trust_load_file(disk, path))
   {
      case CORE_TRUST_LOAD_OK:
         buf = core_trust_write_bytes(w, disk, &len);
         break;
      /* Gone, or damaged and now set aside: this store whole */
      case CORE_TRUST_LOAD_MISSING:
      case CORE_TRUST_LOAD_RESET:
         buf = core_trust_write_bytes(w, NULL, &len);
         break;
      default:
         break;
   }
   if (buf)
      ok = core_trust_save(path, buf, len);
   free(buf);
   core_trust_free(disk);
   return ok;
}

bool core_trust_default_path(char *s, size_t len)
{
#ifdef ANDROID
   if (     !g_android
         || !g_android->activity
         || string_is_empty(g_android->activity->internalDataPath))
      return false;
   fill_pathname_join_special(s, g_android->activity->internalDataPath,
         CORE_TRUST_FILE_NAME, len);
   return true;
#else
   if (!fill_pathname_application_data(s, len))
      return false;
#ifdef _WIN32
   /* That is %APPDATA% itself, shared with every program */
   fill_pathname_slash(s, len);
   strlcat(s, "RetroArch", len);
#endif
   path_mkdir(s);
   fill_pathname_slash(s, len);
   strlcat(s, CORE_TRUST_FILE_NAME, len);
   return true;
#endif
}

#ifndef _WIN32
static bool core_self_lookup(struct core_self *self)
{
   struct retro_vfs_file_identity v;
#if defined(__linux__) && !defined(ANDROID)
   if (retro_vfs_stat_identity_impl("/proc/self/exe", &v) != 1)
      return false;
#elif defined(ANDROID)
   /* /proc/self/exe is app_process: our own library, or the APK it was
    * loaded from inside */
   Dl_info info;
   char *own;
   size_t n;
   bool ok;
   if (     !dladdr((const void*)(uintptr_t)core_self_lookup, &info)
         || !(n = core_self_path_len(info.dli_fname))
         || !(own = (char*)malloc(n + 1)))
      return false;
   memcpy(own, info.dli_fname, n);
   own[n] = '\0';
   ok     = retro_vfs_stat_identity_impl(own, &v) == 1;
   free(own);
   if (!ok)
      return false;
#else
   Dl_info info;
   if (     !dladdr((const void*)(uintptr_t)core_self_lookup, &info)
         || !info.dli_fname
         || info.dli_fname[0] != '/'
         || retro_vfs_stat_identity_impl(info.dli_fname, &v) != 1)
      return false;
#endif
   self->dev     = v.dev;
   self->uid     = (unsigned long)v.uid;
   self->mnt     = 0;
   self->has_mnt = false;
#if defined(__linux__) && !defined(ANDROID)
   {
      /* Its mount, for the in-package rule: the binary we run, even one
       * we may only execute */
      struct retro_vfs_volume vol;
      if (retro_vfs_stat_volume_impl("/proc/self/exe",
               RETRO_VFS_VOLUME_WANT_MOUNT, &vol) == 1)
      {
         self->mnt     = vol.mount;
         self->has_mnt = true;
      }
   }
#endif
   return true;
}

/* RetroArch's own file can't change under a running RetroArch, so it is
 * looked up once. Lock-free: every caller that finds nothing published
 * looks it up itself and uses its own answer, and the first to claim the
 * slot publishes it; none waits on another. */
#define CORE_SELF_CLAIMED (1 << 0)
#define CORE_SELF_READY   (1 << 1)
#define CORE_SELF_FOUND   (1 << 2)
static retro_atomic_int_t core_self_state;
static struct core_self core_self_cached;
#endif

bool core_self_find(struct core_self *self)
{
#ifdef _WIN32
   (void)self;
   return false;
#else
   bool found;
   int state = retro_atomic_load_acquire_int(&core_self_state);
   if (state & CORE_SELF_READY)
   {
      if (!(state & CORE_SELF_FOUND))
         return false;
      *self = core_self_cached;
      return true;
   }
   found = core_self_lookup(self);
   if (!(retro_atomic_fetch_or_int(&core_self_state, CORE_SELF_CLAIMED)
            & CORE_SELF_CLAIMED))
   {
      if (found)
         core_self_cached = *self;
      retro_atomic_store_release_int(&core_self_state, CORE_SELF_CLAIMED
            | CORE_SELF_READY | (found ? CORE_SELF_FOUND : 0));
   }
   return found;
#endif
}

#ifndef _WIN32
/* Off Linux, with no user namespaces to read, and on Android, whose
 * SELinux denies the read, 65534 is only a fallback */
static unsigned long core_overflow_uid(void)
{
#if defined(__linux__) && !defined(ANDROID)
   unsigned long uid = CORE_UID_UNKNOWN;
   FILE *fp          = fopen("/proc/sys/kernel/overflowuid", "r");
   if (fp)
   {
      if (fscanf(fp, "%lu", &uid) != 1)
         uid = CORE_UID_UNKNOWN;
      fclose(fp);
   }
   return uid;
#else
   return 65534;
#endif
}

/* The POSIX mode the rules read: permission bits and the type */
static unsigned core_identity_mode(const struct retro_vfs_file_identity *v)
{
   unsigned mode = v->perm;
   if (v->type == RETRO_VFS_IDENTITY_DIRECTORY)
      mode |= 0040000;
   else if (v->type == RETRO_VFS_IDENTITY_REGULAR)
      mode |= 0100000;
   return mode;
}

static bool core_path_exempt(const struct core_owner *who,
      unsigned long self_uid, const struct retro_vfs_file_identity *v,
      const char *path)
{
   struct retro_vfs_volume vol;
   /* access() also sees ACLs and read-only mounts */
   return (v->flags & RETRO_VFS_IDENTITY_HAS_OWNER)
      && core_mode_exempt(who, self_uid, (unsigned long)v->uid,
            (unsigned long)v->gid, core_identity_mode(v),
            retro_vfs_stat_volume_impl(path, RETRO_VFS_VOLUME_WANT_FLAGS,
               &vol) == 1 && (vol.flags & RETRO_VFS_VOLUME_READ_ONLY))
      && access(path, W_OK) != 0;
}

/* A file's mount is read-only: as the open found it, else asked now */
static bool core_file_read_only(core_file_t *f,
      const struct core_identity *id)
{
   struct retro_vfs_volume vol;
   if (id->vol_known)
      return id->read_only;
   return retro_vfs_file_volume_impl(f, RETRO_VFS_VOLUME_WANT_FLAGS, &vol)
      && (vol.flags & RETRO_VFS_VOLUME_READ_ONLY);
}

static bool core_identity_is(const struct retro_vfs_file_identity *v,
      const struct core_identity *id)
{
   return v->dev == id->dev && !memcmp(v->id, id->ino, sizeof(id->ino));
}
#endif

bool core_file_unwritable(core_file_t *f, const char *path,
      const struct core_identity *id, const struct core_self *self)
{
#ifdef _WIN32
   (void)f;
   (void)path;
   (void)id;
   (void)self;
   return false;
#else
   struct retro_vfs_file_identity pst;
   struct core_owner who;
   unsigned long *gids = NULL;
   gid_t *list         = NULL;
   char *dir           = NULL;
   char *slash;
   bool ok             = false;
   int n, i;
   /* Only beside RetroArch: a disk or image the user mounted is not */
   if (     !self
         || !id
         || !id->has_owner
         || id->dev != self->dev
         || (n = getgroups(0, NULL)) < 0)
      return false;
   list = (gid_t*)malloc(((size_t)n + 1) * sizeof(*list));
   gids = (unsigned long*)malloc(((size_t)n + 1) * sizeof(*gids));
   if (!list || !gids || (n = getgroups(n, list)) < 0)
      goto end;
   for (i = 0; i < n; i++)
      gids[i] = (unsigned long)list[i];
   gids[n]      = (unsigned long)getegid();
   who.gids     = gids;
   who.num_gids = (size_t)n + 1;
   who.uid      = (unsigned long)geteuid();
   who.overflow_uid = core_overflow_uid();
   if (!core_mode_exempt(&who, self->uid, (unsigned long)id->uid,
            (unsigned long)id->gid, id->mode, core_file_read_only(f, id)))
      goto end;
   /* The folders of the real path, which must name this same file */
   if (     !(dir = realpath(path, NULL))
         || retro_vfs_stat_identity_impl(dir, &pst) != 1
         || !core_identity_is(&pst, id)
         || access(dir, W_OK) == 0)
      goto end;
   for (;;)
   {
      if (!(slash = strrchr(dir, '/')))
         goto end;
      slash[slash == dir ? 1 : 0] = '\0';
      if (     retro_vfs_stat_identity_impl(dir, &pst) != 1
            || !core_path_exempt(&who, self->uid, &pst, dir))
         goto end;
      if (string_is_equal(dir, "/"))
         break;
   }
   ok = true;
end:
   free(list);
   free(gids);
   free(dir);
   return ok;
#endif
}

bool core_file_in_package(core_file_t *f, const char *path,
      const struct core_identity *id, const struct core_self *self)
{
#if defined(__APPLE__)
   char bundle[PATH_MAX];
   struct retro_vfs_file_identity rst;
   CFBundleRef app = CFBundleGetMainBundle();
   CFURLRef url    = app ? CFBundleCopyBundleURL(app) : NULL;
   char *core_real = NULL;
   char *app_real  = NULL;
   bool ok         = false;
   (void)f;
   (void)self;
   if (!url || !id)
   {
      if (url)
         CFRelease(url);
      return false;
   }
   /* The real path must name the file that was checked */
   if (     CFURLGetFileSystemRepresentation(url, true, (UInt8*)bundle,
               sizeof(bundle))
         && (app_real = realpath(bundle, NULL))
         && (core_real = realpath(path, NULL))
         && retro_vfs_stat_identity_impl(core_real, &rst) == 1
         && core_identity_is(&rst, id))
      ok = core_path_in_app(core_real, app_real);
   CFRelease(url);
   free(core_real);
   free(app_real);
   return ok;
#elif defined(__linux__) && !defined(ANDROID)
   /* The mount it is on against RetroArch's own, found once */
   struct retro_vfs_volume vol;
   bool has_mnt = retro_vfs_file_volume_impl(f, RETRO_VFS_VOLUME_WANT_MOUNT,
         &vol);
   (void)path;
   return core_mount_in_package(has_mnt ? &vol.mount : NULL,
         self && self->has_mnt ? &self->mnt : NULL,
         id && core_file_read_only(f, id));
#else
   (void)f;
   (void)path;
   (void)id;
   (void)self;
   return false;
#endif
}

/* The folder's full path in one spelling, malloc'd, and its size */
static void *core_folder_full(const char *dir, size_t *size)
{
#ifdef _WIN32
   DWORD n = 0, len = 0;
#ifdef LEGACY_WIN32
   char *full = NULL;
   if (     (n = GetFullPathNameA(dir, 0, NULL, NULL))
         && (full = (char*)malloc(n)))
      len = GetFullPathNameA(dir, n, full, NULL);
#else
   wchar_t *full = NULL;
   wchar_t *wdir = utf8_to_utf16_string_alloc(dir);
   if (     wdir
         && (n = GetFullPathNameW(wdir, 0, NULL, NULL))
         && (full = (wchar_t*)malloc(n * sizeof(*full))))
      len = GetFullPathNameW(wdir, n, full, NULL);
   free(wdir);
#endif
   if (!full || !len || len >= n)
   {
      free(full);
      return NULL;
   }
   /* Absolute and without . or ..; now one case, no trailing separator */
   while (len > 3 && (full[len - 1] == '\\' || full[len - 1] == '/'))
      len--;
#ifdef LEGACY_WIN32
   CharUpperBuffA(full, len);
#else
   CharUpperBuffW(full, len);
#endif
   *size = len * sizeof(*full);
   return full;
#else
   char *full = realpath(dir, NULL);
   if (full)
      *size = strlen(full);
   return full;
#endif
}

bool core_folder_key_with(const char *path, bool per_folder, uint8_t *key)
{
   struct sha256_state sha;
   size_t n   = (size_t)(path_basename_nocompression(path) - path);
   bool ok    = false;
   char *dir;
   void *full;
   size_t size;
   sha256_stream_init(&sha, 0);
   /* One key for every folder, so none is looked up */
   if (!per_folder)
   {
      sha256_stream_final(&sha, key);
      return true;
   }
   if (!(dir = (char*)malloc(n + 2)))
      return false;
   memcpy(dir, path, n);
   if (!n)
      dir[n++] = '.';
   dir[n] = '\0';
   if ((full = core_folder_full(dir, &size)))
   {
      sha256_stream_update(&sha, (const uint8_t*)full, size);
      sha256_stream_final(&sha, key);
      free(full);
      ok = true;
   }
   free(dir);
   return ok;
}

bool core_folder_key(const char *path, uint8_t *key)
{
#ifdef ANDROID
   return core_folder_key_with(path, false, key);
#else
   return core_folder_key_with(path, true, key);
#endif
}

bool core_check_quick_id(const core_trust_set_t *s, core_file_t *f,
      const char *path, const struct core_identity *id, struct core_check *c)
{
   struct core_self self;
   bool found;
   memset(c, 0, sizeof(*c));
   c->result = CORE_CHECK_OK;
   strlcpy(c->rec.name, path_basename_nocompression(path),
         sizeof(c->rec.name));
   if (core_trust_flags(s) & CORE_TRUST_CHECK_OFF)
      return true;
   if (id)
      c->rec.id = *id;
   else if (!core_file_identity(f, &c->rec.id))
   {
      c->result = CORE_CHECK_ERROR;
      return true;
   }
   /* A record with an fs-verity digest holds only with the kernel's */
   if (core_check_wants_verity(s, c))
      core_file_verity_measure(f, &c->rec.id);
   /* A record that holds in any folder answers without the folder's
    * key: a pass is never applied to the store, so nothing reads it */
   if (core_check_record_folderless(s, c))
      return true;
   if (!core_folder_key(path, c->folder))
   {
      c->result = CORE_CHECK_ERROR;
      return true;
   }
   if (core_check_record(s, c))
      return true;
   found = core_self_find(&self);
   return core_file_unwritable(f, path, &c->rec.id, found ? &self : NULL)
      || core_file_in_package(f, path, &c->rec.id, found ? &self : NULL);
}

bool core_check_quick(const core_trust_set_t *s, core_file_t *f,
      const char *path, struct core_check *c)
{
   return core_check_quick_id(s, f, path, NULL, c);
}

char *core_sig_path(const char *path)
{
   size_t path_len = strlen(path);
   char *sig_path  = (char*)malloc(path_len + sizeof(".sig"));
   if (sig_path)
   {
      memcpy(sig_path, path, path_len);
      memcpy(sig_path + path_len, ".sig", sizeof(".sig"));
   }
   return sig_path;
}

int64_t core_sig_read(const char *path, uint8_t *sig)
{
   int64_t got     = 0;
   int64_t sig_len = -1;
   core_file_t *f  = NULL;
   char *sig_path  = core_sig_path(path);
   if (sig_path && (f = core_file_open(sig_path)))
   {
      /* The size as opened: read that much (or one past the most a .sig
       * may hold) and stop, with no read of its own to find the end */
      int64_t want = retro_vfs_file_size_impl(f);
      if (want < 0 || want > CORE_SIG_MAX_LEN + 1)
         want = CORE_SIG_MAX_LEN + 1;
      for (sig_len = 0; sig_len < want; sig_len += got)
         if ((got = core_file_read(f, sig + (size_t)sig_len,
                     (size_t)(want - sig_len))) <= 0)
            break;
      core_file_close(f);
      if (got < 0)
         sig_len = -1;
   }
   free(sig_path);
   return sig_len;
}

void core_check_hashed(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys, struct core_hash *h, int step,
      const char *path, unsigned flags, struct core_check *c)
{
   uint8_t sig[CORE_SIG_MAX_LEN + 1];
   uint8_t sha256[CORE_SHA256_LEN];
   int64_t sig_len;
   if (step < 0 || h->done != c->rec.id.size)
   {
      core_hash_free(h);
      c->result = CORE_CHECK_ERROR;
      return;
   }
   core_hash_final(h, sha256);
   sig_len = core_sig_read(path, sig);
   core_check_finish(s, keys, num_keys, sha256,
         sig_len < 0 ? NULL : sig, sig_len < 0 ? 0 : (size_t)sig_len,
         flags, c);
}

enum core_check_result core_check_file(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys,
      const char *path, unsigned flags, struct core_check *c)
{
   struct core_hash h;
   int step       = -1;
   struct core_identity id;
   core_file_t *f = core_file_open_id(path, &id);
   if (!f || core_check_quick_id(s, f, path, &id, c))
   {
      if (!f)
      {
         memset(c, 0, sizeof(*c));
         c->result = CORE_CHECK_ERROR;
      }
      core_file_close(f);
      return c->result;
   }
   if (core_hash_init(&h))
      do
         step = core_hash_step(&h, f);
      while (step > 0);
   core_file_close(f);
   core_check_hashed(s, keys, num_keys, &h, step, path, flags, c);
   return c->result;
}
