/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_XOPEN_SOURCE)
/* st_mtim and O_CLOEXEC; strict C89 hides them */
#define _XOPEN_SOURCE 700
#endif

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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

#include <encodings/utf.h>
#include <file/file_path.h>
#include <lrc_hash.h>

#include "core_signature.h"

#define CORE_HASH_CHUNK (64 * 1024)

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
