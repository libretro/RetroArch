/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_mediacodec_file.h"
#include <errno.h>
#include <stdlib.h>
#if defined(__ANDROID__) || defined(RECORD_MEDIACODEC_FILE_TEST)
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#ifndef RECORD_MCF_CLOSE
#define RECORD_MCF_CLOSE _close
#endif
#ifndef RECORD_MCF_SYNC
#define RECORD_MCF_SYNC _commit
#endif
static int record_mcf_open(const char *path)
{
   wchar_t *wide;
   int result, saved, count;
   count = MultiByteToWideChar(
         CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
   if (count <= 0 || (size_t)count > (size_t)-1 / sizeof(*wide))
   {
      errno = EINVAL;
      return -1;
   }
   wide = (wchar_t *)malloc((size_t)count * sizeof(*wide));
   if (!wide)
   {
      errno = ENOMEM;
      return -1;
   }
   if (!MultiByteToWideChar(
             CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count))
   {
      free(wide);
      errno = EINVAL;
      return -1;
   }
   result
         = _wopen(wide, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_NOINHERIT,
               _S_IREAD | _S_IWRITE);
   saved = errno;
   free(wide);
   errno = saved;
   return result;
}
static bool record_mcf_valid(int fd)
{
   struct _stat64 info;
   HANDLE handle = (HANDLE)_get_osfhandle(fd);
   char unused = 0;
   DWORD transferred = 0;
   if (handle == INVALID_HANDLE_VALUE || _fstat64(fd, &info))
      return false;
   if ((info.st_mode & _S_IFMT) != _S_IFREG || info.st_size != 0
         || _lseeki64(fd, 0, SEEK_CUR) != 0
         || GetFileType(handle) != FILE_TYPE_DISK)
   {
      errno = EINVAL;
      return false;
   }
   if (!ReadFile(handle, &unused, 0, &transferred, NULL)
         || !WriteFile(handle, &unused, 0, &transferred, NULL))
   {
      errno = EBADF;
      return false;
   }
   if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0))
   {
      errno = EIO;
      return false;
   }
   return true;
}
#else
#include <unistd.h>
#ifndef RECORD_MCF_CLOSE
#define RECORD_MCF_CLOSE close
#endif
#ifndef RECORD_MCF_SYNC
#define RECORD_MCF_SYNC fsync
#endif
static int record_mcf_open(const char *path)
{
   int flags = O_CREAT | O_EXCL | O_RDWR;
#ifdef O_CLOEXEC
   flags |= O_CLOEXEC;
#endif
   return open(path, flags, 0600);
}
static bool record_mcf_valid(int fd)
{
   struct stat info;
   off_t position;
   int flags = fcntl(fd, F_GETFL), descriptor;
   if (flags < 0 || fstat(fd, &info))
      return false;
   if ((flags & O_ACCMODE) != O_RDWR || (flags & O_APPEND)
         || !S_ISREG(info.st_mode) || info.st_size != 0)
   {
      errno = EINVAL;
      return false;
   }
   position = lseek(fd, 0, SEEK_CUR);
   if (position < 0)
      return false;
   if (position != 0)
   {
      errno = EINVAL;
      return false;
   }
   descriptor = fcntl(fd, F_GETFD);
   return descriptor >= 0 && fcntl(fd, F_SETFD, descriptor | FD_CLOEXEC) == 0;
}
#endif
struct record_mediacodec_file
{
   int fd;
};
static void record_mcf_error(int *error, int value)
{
   if (error)
      *error = value;
}
record_mediacodec_file_t *record_mediacodec_file_new(
      const char *path, int *error)
{
   record_mediacodec_file_t *f;
   int saved;
   record_mcf_error(error, 0);
   if (!path || !*path)
   {
      record_mcf_error(error, EINVAL);
      return NULL;
   }
   f = (record_mediacodec_file_t *)calloc(1, sizeof(*f));
   if (!f)
   {
      record_mcf_error(error, ENOMEM);
      return NULL;
   }
   f->fd = record_mcf_open(path);
   if (f->fd < 0 || !record_mcf_valid(f->fd))
   {
      saved = errno;
      if (f->fd >= 0)
         RECORD_MCF_CLOSE(f->fd);
      free(f);
      record_mcf_error(error, saved ? saved : EIO);
      return NULL;
   }
   return f;
}
record_mediacodec_file_t *record_mediacodec_file_adopt_fresh(
      int *owned_fd, int *error)
{
   record_mediacodec_file_t *f;
   record_mcf_error(error, 0);
   if (!owned_fd || *owned_fd < 0)
   {
      record_mcf_error(error, EINVAL);
      return NULL;
   }
   f = (record_mediacodec_file_t *)calloc(1, sizeof(*f));
   if (!f)
   {
      record_mcf_error(error, ENOMEM);
      return NULL;
   }
   if (!record_mcf_valid(*owned_fd))
   {
      int saved = errno;
      free(f);
      record_mcf_error(error, saved ? saved : EIO);
      return NULL;
   }
   f->fd = *owned_fd;
   *owned_fd = -1;
   return f;
}
int record_mediacodec_file_descriptor(const record_mediacodec_file_t *f)
{
   return f ? f->fd : -1;
}
bool record_mediacodec_file_close(
      record_mediacodec_file_t *f, bool complete, int *error)
{
   int saved = 0;
   if (f)
   {
      if (complete && RECORD_MCF_SYNC(f->fd))
         saved = errno ? errno : EIO;
      if (RECORD_MCF_CLOSE(f->fd) && !saved)
         saved = errno ? errno : EIO;
      free(f);
   }
   record_mcf_error(error, saved);
   return saved == 0;
}
#else
record_mediacodec_file_t *record_mediacodec_file_new(
      const char *path, int *error)
{
   (void)path;
   if (error)
      *error = ENOSYS;
   return NULL;
}
record_mediacodec_file_t *record_mediacodec_file_adopt_fresh(
      int *fd, int *error)
{
   (void)fd;
   if (error)
      *error = ENOSYS;
   return NULL;
}
int record_mediacodec_file_descriptor(const record_mediacodec_file_t *f)
{
   (void)f;
   return -1;
}
bool record_mediacodec_file_close(
      record_mediacodec_file_t *f, bool complete, int *error)
{
   (void)complete;
   if (error)
      *error = f ? ENOSYS : 0;
   return f == NULL;
}
#endif
