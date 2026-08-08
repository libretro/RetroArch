/*
Copyright (c) 2006 by Dan Kennedy.
Copyright (c) 2006 by Juliusz Chroboczek.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/*Adaptions by memphiz@xbmc.org*/

#ifndef win32_COMPAT_H_
#define win32_COMPAT_H_
#define NO_IPv6 1

#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2ipdef.h>
#include <basetsd.h>
#include <io.h>
#include <sys/stat.h>
#include <time.h>
#include <stddef.h> // For size_t type

/* We always have multithreading for windows */
#ifndef HAVE_MULTITHREADING
#define HAVE_MULTITHREADING 1
#endif

typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef int socklen_t;

#ifndef S_IRUSR
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#endif
#ifndef S_IRWXG
#define	S_IRWXG	0000070			/* RWX mask for group */
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define	S_IRWXO	0000007			/* RWX mask for other */
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define F_GETFL  3
#define F_SETFL  4

#ifndef S_IFLNK
#define S_IFLNK        0xA000  /* Link */
#endif

#ifndef F_RDLCK
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#endif

#ifndef S_IFIFO
#define S_IFIFO        0x1000  /* FIFO */
#endif

#ifndef S_IFBLK
#define S_IFBLK        0x3000  /* Block: Is this ever set under w32? */
#endif

#ifndef S_IFSOCK
#define S_IFSOCK 0x0           /* not defined in mingw either */
#endif

#ifndef major
#define major(a) 0
#endif

#ifndef minor
#define minor(a) 0
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK 0x40000000
#endif

#ifndef O_SYNC
#define O_SYNC 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW	00400000
#endif

#define MSG_DONTWAIT 0
#define ssize_t SSIZE_T

#if(_WIN32_WINNT < 0x0600)

#define POLLIN      0x0001    /* There is data to read */
#define POLLPRI     0x0002    /* There is urgent data to read */
#define POLLOUT     0x0004    /* Writing now will not block */
#define POLLERR     0x0008    /* Error condition */
#define POLLHUP     0x0010    /* Hung up */
#define POLLNVAL    0x0020    /* Invalid request: fd not open */

struct pollfd {
    SOCKET fd;        /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};
#endif

#define close closesocket
#define ioctl ioctlsocket

#ifndef ESTALE
#define ESTALE 116
#endif

/* Wrapper macros to call misc. functions win32 is missing */
#define poll(x, y, z)        win32_poll(x, y, z)
//#define snprintf             sprintf_s
#define inet_pton(x,y,z)     win32_inet_pton(x,y,z)
#define open(x, y, z)        _open(x, y, z)
#ifndef lseek
#define lseek(x, y, z)       _lseek(x, y, z)
#endif
#define read(x, y, z)        _read(x, y, z)
#define write(x, y, z)       _write(x, y, z)
int     getpid(void);
int     win32_inet_pton(int af, const char * src, void * dst);
int     win32_poll(struct pollfd *fds, unsigned int nfsd, int timeout);
#ifdef __MINGW32__
# define win32_gettimeofday mingw_gettimeofday
#else
struct timezone
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
int     win32_gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#define DllExport

#if defined(__MINGW32__) || defined(_MSC_VER)
char* libnfs_strndup(const char *s, size_t n);
#define strndup libnfs_strndup
#endif

struct iovec
{
  unsigned long iov_len; // from WSABUF
  void *iov_base;        
};

static inline int writev(SOCKET sock, struct iovec *iov, int nvecs)
{
  DWORD ret;

  int res = WSASend(sock, (LPWSABUF)iov, nvecs, &ret, 0, NULL, NULL);

  if (res == 0) {
    return (int)ret;
  }
  return -1;
}

static inline int readv(SOCKET sock, struct iovec *iov, int nvecs)
{
  DWORD ret;
  DWORD flags = 0;

  int res = WSARecv(sock, (LPWSABUF)iov, nvecs, &ret, &flags, NULL, NULL);

  if (res == 0) {
    return (int)ret;
  }
  return -1;
}

#endif//win32_COMPAT_H_
