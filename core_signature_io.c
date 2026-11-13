/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_XOPEN_SOURCE)
/* fstatvfs, realpath, st_mtim and O_CLOEXEC; strict C89 hides them */
#define _XOPEN_SOURCE 700
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#if !defined(__linux__) || defined(ANDROID)
#include <dlfcn.h>
#endif
#if defined(__linux__)
#include <sys/vfs.h>
#define CORE_FS_MAGIC
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
   || defined(__DragonFly__)
#include <sys/param.h>
#include <sys/mount.h>
#define CORE_FS_NAME
#endif
#endif

#include <compat/strl.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "core_signature.h"
#include "file_path_special.h"
#ifdef ANDROID
#include "frontend/drivers/platform_unix.h"
#endif

#define CORE_HASH_CHUNK (64 * 1024)
/* .<pid>.<stamp>.tmp, 64-bit numbers */
#define CORE_TRUST_TEMP_TAIL 48

struct core_file
{
#ifdef _WIN32
   HANDLE h;
#else
   int fd;
#endif
};

core_file_t *core_file_open(const char *path)
{
   core_file_t *f = (core_file_t*)malloc(sizeof(*f));
#ifdef _WIN32
#ifndef LEGACY_WIN32
   wchar_t *wpath;
#endif
#else
   struct stat st;
   int flags = O_RDONLY;
#endif
   if (!f)
      return NULL;
#ifdef _WIN32
   /* No write or delete sharing: nothing can change it while open */
#ifdef LEGACY_WIN32
   f->h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
#else
   if (!(wpath = utf8_to_utf16_string_alloc(path)))
   {
      free(f);
      return NULL;
   }
   f->h = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL,
         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
   free(wpath);
#endif
   if (f->h == INVALID_HANDLE_VALUE)
   {
      free(f);
      return NULL;
   }
#else
#ifdef O_CLOEXEC
   flags |= O_CLOEXEC;
#endif
   /* A FIFO would block until a writer opens it */
#ifdef O_NONBLOCK
   flags |= O_NONBLOCK;
#endif
#ifdef O_NOCTTY
   flags |= O_NOCTTY;
#endif
   if ((f->fd = open(path, flags)) < 0)
   {
      free(f);
      return NULL;
   }
   if (fstat(f->fd, &st) != 0 || !S_ISREG(st.st_mode))
   {
      close(f->fd);
      free(f);
      return NULL;
   }
#endif
   return f;
}

void core_file_close(core_file_t *f)
{
   if (!f)
      return;
#ifdef _WIN32
   CloseHandle(f->h);
#else
   close(f->fd);
#endif
   free(f);
}

int64_t core_file_read(core_file_t *f, void *buf, size_t len)
{
#ifdef _WIN32
   DWORD got = 0;
   if (len > 0x40000000)
      len = 0x40000000;
   if (!ReadFile(f->h, buf, (DWORD)len, &got, NULL))
      return -1;
   return (int64_t)got;
#else
   ssize_t got;
   do
      got = read(f->fd, buf, len);
   while (got < 0 && errno == EINTR);
   return (int64_t)got;
#endif
}

#ifdef _WIN32
/* FILE_BASIC_INFO and FILE_ID_INFO, which older SDKs lack */
struct core_file_basic_info
{
   LARGE_INTEGER creation;
   LARGE_INTEGER access;
   LARGE_INTEGER write;
   LARGE_INTEGER change;
   DWORD attributes;
};

struct core_file_id_info
{
   ULONGLONG volume;
   BYTE id[16];
};

/* GetFileInformationByHandleEx, Vista and later */
typedef BOOL (WINAPI *core_file_info_ex_t)(HANDLE, int, LPVOID, DWORD);
#define CORE_FILE_BASIC_INFO 0
#define CORE_FILE_ID_INFO    18

static void core_file_time(uint64_t ticks, int64_t *sec, uint32_t *nsec)
{
   *sec  = (int64_t)(ticks / 10000000);
   *nsec = (uint32_t)(ticks % 10000000) * 100;
}
#endif

bool core_file_identity(core_file_t *f, struct core_identity *id)
{
#ifdef _WIN32
   BY_HANDLE_FILE_INFORMATION info;
   struct core_file_basic_info basic;
   struct core_file_id_info fid;
   core_file_info_ex_t info_ex = (core_file_info_ex_t)GetProcAddress(
         GetModuleHandleA("kernel32.dll"), "GetFileInformationByHandleEx");
   unsigned i;
   /* ctime_kept stays false: any writer can set ChangeTime */
   memset(id, 0, sizeof(*id));
   if (!GetFileInformationByHandle(f->h, &info))
      return false;
   id->dev  = info.dwVolumeSerialNumber;
   id->size = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;
   for (i = 0; i < 4; i++)
   {
      id->ino[8 + i]  = (uint8_t)(info.nFileIndexHigh >> (24 - 8 * i));
      id->ino[12 + i] = (uint8_t)(info.nFileIndexLow  >> (24 - 8 * i));
   }
   core_file_time(((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32)
         | info.ftLastWriteTime.dwLowDateTime, &id->mtime, &id->mtime_nsec);
   if (info_ex)
   {
      /* 128-bit ids: ReFS can reuse a 64-bit index */
      if (info_ex(f->h, CORE_FILE_ID_INFO, &fid, sizeof(fid)))
      {
         id->dev = fid.volume;
         memcpy(id->ino, fid.id, sizeof(id->ino));
      }
      if (info_ex(f->h, CORE_FILE_BASIC_INFO, &basic, sizeof(basic)))
         core_file_time((uint64_t)basic.change.QuadPart,
               &id->ctime, &id->ctime_nsec);
   }
   return true;
#else
   struct stat st;
#if defined(CORE_FS_MAGIC) || defined(CORE_FS_NAME)
   struct statfs fs;
#endif
   uint64_t ino;
   unsigned i;
   memset(id, 0, sizeof(*id));
   if (fstat(f->fd, &st) != 0)
      return false;
#if defined(CORE_FS_MAGIC)
   id->ctime_kept = fstatfs(f->fd, &fs) == 0
         && core_fs_magic_keeps_ctime((unsigned long)fs.f_type);
#elif defined(CORE_FS_NAME)
   id->ctime_kept = fstatfs(f->fd, &fs) == 0
         && core_fs_name_keeps_ctime(fs.f_fstypename);
#endif
   id->dev   = (uint64_t)st.st_dev;
   id->size  = (uint64_t)st.st_size;
   id->mtime = (int64_t)st.st_mtime;
   id->ctime = (int64_t)st.st_ctime;
#if defined(__APPLE__)
   id->mtime_nsec = (uint32_t)st.st_mtimespec.tv_nsec;
   id->ctime_nsec = (uint32_t)st.st_ctimespec.tv_nsec;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) \
   || defined(__OpenBSD__)
   id->mtime_nsec = (uint32_t)st.st_mtim.tv_nsec;
   id->ctime_nsec = (uint32_t)st.st_ctim.tv_nsec;
#endif
   ino = (uint64_t)st.st_ino;
   for (i = 0; i < 8; i++)
      id->ino[8 + i] = (uint8_t)(ino >> (56 - 8 * i));
   return true;
#endif
}

bool core_hash_init(struct core_hash *h)
{
   sha256_stream_init(&h->sha, 0);
   h->done = 0;
   return (h->buf = (uint8_t*)malloc(CORE_HASH_CHUNK)) != NULL;
}

int core_hash_step(struct core_hash *h, core_file_t *f)
{
   int64_t got = core_file_read(f, h->buf, CORE_HASH_CHUNK);
   if (got < 0)
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
   free(h->buf);
   h->buf = NULL;
}

/* 1 a file, 0 missing, -1 anything else: what can't be looked at, a
 * directory, or a FIFO, whose open would wait for a writer */
static int core_trust_file_kind(const char *path)
{
#ifdef _WIN32
   DWORD attr, err;
#ifdef LEGACY_WIN32
   attr = GetFileAttributesA(path);
   err  = GetLastError();
#else
   wchar_t *wpath = utf8_to_utf16_string_alloc(path);
   if (!wpath)
      return -1;
   attr = GetFileAttributesW(wpath);
   err  = GetLastError();
   free(wpath);
#endif
   if (attr == INVALID_FILE_ATTRIBUTES)
      return (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
         ? 0 : -1;
   return (attr & FILE_ATTRIBUTE_DIRECTORY) ? -1 : 1;
#else
   struct stat st;
   if (stat(path, &st) != 0)
      return errno == ENOENT ? 0 : -1;
   return S_ISREG(st.st_mode) ? 1 : -1;
#endif
}

/* The device of the folder holding path; Windows has no records */
static bool core_trust_folder_dev(const char *path, uint64_t *dev)
{
#ifdef _WIN32
   return false;
#else
   struct stat st;
   size_t n  = (size_t)(path_basename_nocompression(path) - path);
   char *dir = (char*)malloc(n + 2);
   bool ok;
   if (!dir)
      return false;
   memcpy(dir, path, n);
   if (!n)
      dir[n++] = '.';
   dir[n] = '\0';
   if ((ok = stat(dir, &st) == 0))
      *dev = (uint64_t)st.st_dev;
   free(dir);
   return ok;
#endif
}

enum core_trust_load core_trust_load_file(core_trust_t *t,
      const char *path)
{
   void *buf    = NULL;
   int64_t len  = 0;
   bool renamed = false;
   struct core_self self;
   uint64_t dev;
   int64_t size;
   size_t n;
   char *bad;
   enum core_trust_load ret;
   int kind;
   core_trust_set_device(t, core_trust_folder_dev(path, &dev) ? &dev : NULL,
         core_self_find(&self) ? &self.dev : NULL);
   if (!(kind = core_trust_file_kind(path)))
      return CORE_TRUST_LOAD_MISSING;
   /* core_trust_parse(t, NULL, 0) is UNREADABLE and read-only */
   if (kind < 0 || (size = path_get_size(path)) < 0)
      return core_trust_parse(t, NULL, 0);
   if (size > CORE_TRUST_FILE_MAX)
   {
      /* Too big for ours: only its magic, which may say a newer one's */
      uint8_t head[sizeof(CORE_TRUST_MAGIC) - 1];
      int64_t got = -1;
      RFILE *file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (file)
      {
         got = filestream_read(file, head, sizeof(head));
         filestream_close(file);
      }
      if (got != (int64_t)sizeof(head))
         return core_trust_parse(t, NULL, 0);
      ret = core_trust_parse(t, head, sizeof(head));
   }
   else
   {
      if (!filestream_read_file(path, &buf, &len))
      {
         free(buf);
         return core_trust_parse(t, NULL, 0);
      }
      ret = core_trust_parse(t, (const uint8_t*)buf, (size_t)len);
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

bool core_self_find(struct core_self *self)
{
#ifdef _WIN32
   return false;
#else
   struct stat st;
#if defined(__linux__) && !defined(ANDROID)
   if (stat("/proc/self/exe", &st) != 0)
      return false;
#else
   /* Android's /proc/self/exe is app_process: find our own library */
   Dl_info info;
   if (     !dladdr((const void*)(uintptr_t)core_self_find, &info)
         || !info.dli_fname
         || info.dli_fname[0] != '/'
         || stat(info.dli_fname, &st) != 0)
      return false;
#endif
   self->dev = (uint64_t)st.st_dev;
   self->uid = (unsigned long)st.st_uid;
   return true;
#endif
}

#ifndef _WIN32
/* Off Linux, with no user namespaces to read, 65534 is only a fallback */
static unsigned long core_overflow_uid(void)
{
#ifdef __linux__
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

static bool core_path_exempt(const struct core_owner *who,
      unsigned long self_uid, const struct stat *st, bool read_only_mount,
      const char *path)
{
   /* access() also sees ACLs and read-only mounts */
   return core_mode_exempt(who, self_uid, (unsigned long)st->st_uid,
         (unsigned long)st->st_gid, (unsigned)st->st_mode, read_only_mount)
      && access(path, W_OK) != 0;
}
#endif

bool core_file_unwritable(core_file_t *f, const char *path,
      const struct core_self *self)
{
#ifdef _WIN32
   return false;
#else
   struct stat st, pst;
   struct statvfs vfs;
   struct core_owner who;
   unsigned long *gids = NULL;
   gid_t *list         = NULL;
   char *dir           = NULL;
   char *slash;
   bool ok             = false;
   int n, i;
   /* Only beside RetroArch: a disk or image the user mounted is not */
   if (     !self
         || fstat(f->fd, &st) != 0
         || (uint64_t)st.st_dev != self->dev
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
   if (!core_mode_exempt(&who, self->uid, (unsigned long)st.st_uid,
            (unsigned long)st.st_gid, (unsigned)st.st_mode,
            fstatvfs(f->fd, &vfs) == 0 && (vfs.f_flag & ST_RDONLY)))
      goto end;
   /* The folders of the real path, which must name this same file */
   if (     !(dir = realpath(path, NULL))
         || stat(dir, &pst) != 0
         || pst.st_dev != st.st_dev
         || pst.st_ino != st.st_ino
         || access(dir, W_OK) == 0)
      goto end;
   for (;;)
   {
      if (!(slash = strrchr(dir, '/')))
         goto end;
      slash[slash == dir ? 1 : 0] = '\0';
      if (     stat(dir, &pst) != 0
            || !core_path_exempt(&who, self->uid, &pst,
                  statvfs(dir, &vfs) == 0 && (vfs.f_flag & ST_RDONLY), dir))
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

bool core_check_quick(const core_trust_set_t *s, core_file_t *f,
      const char *path, struct core_check *c)
{
   struct core_self self;
   memset(c, 0, sizeof(*c));
   c->result = CORE_CHECK_OK;
   strlcpy(c->rec.name, path_basename_nocompression(path),
         sizeof(c->rec.name));
   if (core_trust_flags(s) & CORE_TRUST_CHECK_OFF)
      return true;
   if (     !core_file_identity(f, &c->rec.id)
         || !core_folder_key(path, c->folder))
   {
      c->result = CORE_CHECK_ERROR;
      return true;
   }
   return core_check_record(s, c)
      || core_file_unwritable(f, path,
            core_self_find(&self) ? &self : NULL);
}

enum core_check_result core_check_file(const core_trust_set_t *s,
      const uint8_t *keys, size_t num_keys,
      const char *path, unsigned flags, struct core_check *c)
{
   uint8_t sig[CORE_SIG_MAX_LEN + 1];
   uint8_t sha256[CORE_SHA256_LEN];
   struct core_hash h;
   int64_t got      = 0;
   int64_t sig_len  = -1;
   size_t path_len  = strlen(path);
   char *sig_path   = NULL;
   int step         = -1;
   core_file_t *f   = core_file_open(path);
   if (!f || core_check_quick(s, f, path, c))
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
   if (step < 0 || h.done != c->rec.id.size)
   {
      core_hash_free(&h);
      c->result = CORE_CHECK_ERROR;
      return c->result;
   }
   core_hash_final(&h, sha256);
   /* <core>.sig, read no further than the longest valid one */
   if ((sig_path = (char*)malloc(path_len + sizeof(".sig"))))
   {
      memcpy(sig_path, path, path_len);
      memcpy(sig_path + path_len, ".sig", sizeof(".sig"));
      if ((f = core_file_open(sig_path)))
      {
         for (sig_len = 0; (size_t)sig_len < sizeof(sig); sig_len += got)
            if ((got = core_file_read(f, sig + (size_t)sig_len,
                        sizeof(sig) - (size_t)sig_len)) <= 0)
               break;
         core_file_close(f);
         if (got < 0)
            sig_len = 0;
      }
      free(sig_path);
   }
   core_check_finish(s, keys, num_keys, sha256,
         sig_len < 0 ? NULL : sig, sig_len < 0 ? 0 : (size_t)sig_len,
         flags, c);
   return c->result;
}
