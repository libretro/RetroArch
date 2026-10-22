/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Windows CRT pipes for host-only FD ownership models. */
#ifdef _WIN32
#define ANDROID_WIN32_FD_TEST_IMPLEMENTATION
#include "win32_fd_test.h"
#include <errno.h>
#include <windows.h>
#define TEST_FD_LIMIT 8192
static unsigned char live[TEST_FD_LIMIT];
static int valid(int fd)
{
   return fd >= 0 && fd < TEST_FD_LIMIT && live[fd];
}
int android_test_pipe(int pair[2])
{
   if (_pipe(pair, 4096, _O_BINARY | _O_NOINHERIT))
      return -1;
   if (pair[0] >= TEST_FD_LIMIT || pair[1] >= TEST_FD_LIMIT)
   {
      _close(pair[0]);
      _close(pair[1]);
      errno = EMFILE;
      return -1;
   }
   live[pair[0]] = live[pair[1]] = 1;
   return 0;
}
int android_test_close(int fd)
{
   int result;
   if (!valid(fd))
   {
      errno = EBADF;
      return -1;
   }
   result = _close(fd);
   if (!result)
      live[fd] = 0;
   return result;
}
int android_test_fcntl(int fd, int command, ...)
{
   int copy;
   DWORD flags;
   if (!valid(fd))
   {
      errno = EBADF;
      return -1;
   }
   if (command == F_GETFD)
   {
      if (!GetHandleInformation((HANDLE)_get_osfhandle(fd), &flags))
         return -1;
      return flags & HANDLE_FLAG_INHERIT ? 0 : FD_CLOEXEC;
   }
   if (command != F_DUPFD_CLOEXEC)
   {
      errno = EINVAL;
      return -1;
   }
   copy = _dup(fd);
   if (copy < 0)
      return -1;
   if (copy >= TEST_FD_LIMIT
         || !SetHandleInformation((HANDLE)_get_osfhandle(copy), HANDLE_FLAG_INHERIT, 0))
   {
      _close(copy);
      return -1;
   }
   live[copy] = 1;
   return copy;
}
int android_test_poll(struct pollfd *fds, nfds_t count, int timeout)
{
   nfds_t i;
   int ready = 0;
   if (timeout || count > TEST_FD_LIMIT || (count && !fds))
   {
      errno = EINVAL;
      return -1;
   }
   for (i = 0; i < count; i++)
   {
      DWORD available = 0;
      fds[i].revents = 0;
      if (fds[i].fd < 0)
         continue;
      if (!valid(fds[i].fd))
         fds[i].revents = POLLNVAL;
      else if (PeekNamedPipe((HANDLE)_get_osfhandle(fds[i].fd), NULL, 0, NULL,
                  &available, NULL))
      {
         if (available && (fds[i].events & POLLIN))
            fds[i].revents = POLLIN;
      }
      else
         fds[i].revents = GetLastError() == ERROR_BROKEN_PIPE ? POLLHUP : POLLERR;
      if (fds[i].revents)
         ready++;
   }
   return ready;
}

#else
typedef int android_win32_fd_test_unused;
#endif
