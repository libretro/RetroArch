/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Recording file acceptance requires assertions
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define NATIVE_CLOSE _close
#define NATIVE_SYNC _commit
#define NATIVE_READ _read
#define NATIVE_WRITE _write
#define NATIVE_SEEK _lseeki64
#else
#include <unistd.h>
#define NATIVE_CLOSE close
#define NATIVE_SYNC fsync
#define NATIVE_READ read
#define NATIVE_WRITE write
#define NATIVE_SEEK lseek
#endif
static unsigned closes, syncs;
static int sync_fault, close_fault, allocation_fault;
static int file_close(int fd)
{
   int result = NATIVE_CLOSE(fd);
   closes++;
   if (close_fault)
   {
      errno = EINTR;
      return -1;
   }
   return result;
}
static int file_sync(int fd)
{
   syncs++;
   if (sync_fault)
   {
      errno = EIO;
      return -1;
   }
   return NATIVE_SYNC(fd);
}
static void *file_allocate(size_t count, size_t size)
{
   return allocation_fault ? NULL : calloc(count, size);
}
#define RECORD_MCF_CLOSE file_close
#define RECORD_MCF_SYNC file_sync
#define calloc file_allocate
#include "../../../record/record_mediacodec_file.c"
#undef calloc
static void path(char *out, const char *directory, const char *leaf)
{
   assert(strlen(directory) + strlen(leaf) + 2 < 1024);
   sprintf(out, "%s/%s", directory, leaf);
}
int main(int argc, char **argv)
{
   record_mediacodec_file_t *f;
   char name[1024], bytes[8];
   int fd, saved, error, pipes[2];
   unsigned before;
   assert(argc == 2);
   assert(!record_mediacodec_file_new(NULL, &error) && error == EINVAL);
   assert(!record_mediacodec_file_new("", &error) && error == EINVAL);
   fd = -1;
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == -1
         && error == EINVAL);
   assert(!record_mediacodec_file_adopt_fresh(NULL, &error) && error == EINVAL);
   assert(record_mediacodec_file_descriptor(NULL) == -1);
   assert(record_mediacodec_file_close(NULL, true, &error) && !error);
   path(name, argv[1], "record-\xc3\xa9.mp4");
   allocation_fault = 1;
   assert(!record_mediacodec_file_new(name, &error) && error == ENOMEM);
   allocation_fault = 0;
   f = record_mediacodec_file_new(name, &error);
   assert(f && !error);
   fd = record_mediacodec_file_descriptor(f);
   assert(fd >= 0 && NATIVE_WRITE(fd, "sentinel", 8) == 8);
   assert(!record_mediacodec_file_new(name, &error) && error == EEXIST);
   assert(NATIVE_SEEK(fd, 0, SEEK_SET) == 0);
   assert(NATIVE_READ(fd, bytes, 8) == 8 && !memcmp(bytes, "sentinel", 8));
   assert(record_mediacodec_file_close(f, true, &error) && !error);
   assert(syncs == 1 && closes == 1);
   path(name, argv[1], "adopt.mp4");
   fd = record_mcf_open(name);
   assert(fd >= 0);
   saved = fd;
   allocation_fault = 1;
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && error == ENOMEM
         && fd == saved);
   allocation_fault = 0;
   f = record_mediacodec_file_adopt_fresh(&fd, &error);
   assert(f && !error && fd == -1
         && record_mediacodec_file_descriptor(f) == saved);
#ifndef _WIN32
   assert(fcntl(saved, F_GETFD) & FD_CLOEXEC);
#else
   {
      DWORD flags = 0;
      assert(GetHandleInformation((HANDLE)_get_osfhandle(saved), &flags));
      assert(!(flags & HANDLE_FLAG_INHERIT));
   }
#endif
   assert(record_mediacodec_file_close(f, false, &error) && !error);
   assert(!record_mediacodec_file_new(name, &error) && error == EEXIST);
   path(name, argv[1], "nonempty.mp4");
   fd = record_mcf_open(name);
   assert(fd >= 0);
   assert(
         NATIVE_WRITE(fd, "keep", 4) == 4 && NATIVE_SEEK(fd, 0, SEEK_SET) == 0);
   saved = fd;
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == saved);
   assert(NATIVE_READ(fd, bytes, 4) == 4 && !memcmp(bytes, "keep", 4));
   assert(!NATIVE_CLOSE(fd));
   path(name, argv[1], "position.mp4");
   fd = record_mcf_open(name);
   assert(fd >= 0);
   assert(NATIVE_SEEK(fd, 5, SEEK_SET) == 5);
   saved = fd;
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == saved);
   assert(NATIVE_SEEK(fd, 0, SEEK_CUR) == 5 && !NATIVE_CLOSE(fd));
   path(name, argv[1], "readonly.mp4");
   fd = record_mcf_open(name);
   assert(fd >= 0 && !NATIVE_CLOSE(fd));
#ifdef _WIN32
   fd = _open(name, _O_RDONLY | _O_BINARY);
#else
   fd = open(name, O_RDONLY);
#endif
   saved = fd;
   assert(fd >= 0);
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == saved);
   assert(!NATIVE_CLOSE(fd));
#ifdef _WIN32
   fd = _open(name, _O_WRONLY | _O_BINARY);
#else
   fd = open(name, O_WRONLY);
#endif
   saved = fd;
   assert(fd >= 0);
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == saved);
   assert(!NATIVE_CLOSE(fd));
#ifdef _WIN32
   assert(!_pipe(pipes, 256, _O_BINARY));
#else
   assert(!pipe(pipes));
#endif
   fd = pipes[1];
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == pipes[1]);
   assert(!NATIVE_CLOSE(pipes[0]) && !NATIVE_CLOSE(pipes[1]));
#ifdef _WIN32
   fd = _open("NUL", _O_RDWR | _O_BINARY);
#else
   fd = open("/dev/null", O_RDWR);
#endif
   saved = fd;
   assert(fd >= 0);
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == saved);
   assert(!NATIVE_CLOSE(fd));
#ifndef _WIN32
   path(name, argv[1], "append.mp4");
   fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_APPEND, 0600);
   saved = fd;
   assert(fd >= 0);
   assert(!record_mediacodec_file_adopt_fresh(&fd, &error) && fd == saved);
   assert(fcntl(fd, F_GETFL) & O_APPEND);
   assert(!NATIVE_CLOSE(fd));
   path(name, argv[1], "link.mp4");
   assert(!symlink("nonexistent-recording", name));
   assert(!record_mediacodec_file_new(name, &error) && error == EEXIST);
#endif
   path(name, argv[1], "sync-fault.mp4");
   f = record_mediacodec_file_new(name, &error);
   assert(f);
   before = closes;
   sync_fault = 1;
   assert(!record_mediacodec_file_close(f, true, &error) && error == EIO);
   assert(closes == before + 1);
   sync_fault = 0;
   path(name, argv[1], "close-fault.mp4");
   f = record_mediacodec_file_new(name, &error);
   assert(f);
   before = closes;
   close_fault = 1;
   assert(!record_mediacodec_file_close(f, false, &error) && error == EINTR);
   assert(closes == before + 1);
   close_fault = 0;
   NATIVE_CLOSE(0);
   path(name, argv[1], "fd-zero.mp4");
   f = record_mediacodec_file_new(name, &error);
   assert(f);
   assert(record_mediacodec_file_descriptor(f) == 0);
   assert(record_mediacodec_file_close(f, false, &error) && !error);
   puts("PASS: exclusive UTF-8 recording files, fresh-fd adoption and one-shot "
        "finalization faults");
   return 0;
}
