/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2020 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _COMPAT_H_
#define _COMPAT_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_XBOX) || defined(_WINDOWS) || defined(__MINGW32__)
#if defined(_WINDOWS) || defined(__MINGW32__)
#include <windows.h>
#ifdef __USE_WINSOCK__
#include <winsock.h>
#else
#include <ws2tcpip.h>
#include <winsock2.h>
#endif
#elif defined(_XBOX)
#include <xtl.h>
#include <winsockx.h>
#endif
typedef SOCKET t_socket;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (t_socket)(~0)
#endif
#define SMB2_INVALID_SOCKET INVALID_SOCKET
#define SMB2_VALID_SOCKET(sock)	((sock) != SMB2_INVALID_SOCKET)
#else
typedef int t_socket;
#define SMB2_VALID_SOCKET(sock)	((sock) >= 0)
#define SMB2_INVALID_SOCKET		-1
#endif

#if defined(_XBOX) || defined(_WINDOWS) || defined(__MINGW32__)

#include <stddef.h>
#include <errno.h>

#ifdef __USE_WINSOCK__
#include <io.h>
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* !WIN32_LEAN_AND_MEAN */

#ifdef XBOX_PLATFORM /* Xbox XDK Doesn´t have stdint.h header */
typedef char int8_t;
typedef short int16_t;
typedef short int_least16_t;
typedef int int32_t;
typedef long long int64_t;
typedef int intptr_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned int uint_t;
typedef unsigned int uintptr_t;
#else
#include <stdint.h>	
#endif

#ifndef ENETRESET
#define ENETRESET WSAENETRESET
#endif

#ifndef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif

#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif

#ifndef ENODATA
#define ENODATA WSANO_DATA
#endif

#ifndef ETXTBSY 
#define ETXTBSY         139
#endif

#ifndef ENOLINK
#define ENOLINK         121
#endif

#ifndef EWOULDBLOCK 
#define EWOULDBLOCK     WSAEWOULDBLOCK
#endif

#ifndef EBADF
#define EBADF WSAENOTSOCK
#endif

#if defined(_XBOX)
#define snprintf _snprintf
int gethostname(char* name, size_t len);
#endif

#ifndef EAI_AGAIN
#define EAI_AGAIN EAGAIN
#endif

#ifndef EAI_FAIL
#define EAI_FAIL        4
#endif

#ifndef EAI_MEMORY
#define EAI_MEMORY      6
#endif

#ifndef EAI_NONAME
#define EAI_NONAME      8
#endif

#ifndef EAI_SERVICE
#define EAI_SERVICE     9
#endif

#if defined(_XBOX) || defined(__USE_WINSOCK__)
typedef int socklen_t;
#endif

#ifndef POLLIN
#define POLLIN      0x0001    /* There is data to read */
#endif
#ifndef POLLPRI
#define POLLPRI     0x0002    /* There is urgent data to read */
#endif
#ifndef POLLOUT
#define POLLOUT     0x0004    /* Writing now will not block */
#endif
#ifndef POLLERR
#define POLLERR     0x0008    /* Error condition */
#endif
#ifndef POLLHUP
#define POLLHUP     0x0010    /* Hung up */
#endif

#ifndef SO_ERROR
#define SO_ERROR 0x1007
#endif

#if defined(_XBOX) || defined(__USE_WINSOCK__)
struct sockaddr_storage {
#ifdef HAVE_SOCKADDR_SA_LEN
	unsigned char ss_len;
#endif /* HAVE_SOCKADDR_SA_LEN */
	unsigned char ss_family;
	unsigned char fill[127];
};

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};

/* XBOX Defs end */
struct pollfd {
        t_socket fd;
        short events;
        short revents;
};

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#ifdef _XBOX
#define inline __inline 
#endif

#endif

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#endif

struct iovec
{
  unsigned long iov_len; /* from WSABUF */
  void *iov_base;        
};	

#if defined(_XBOX) || defined(__USE_WINSOCK__)
int poll(struct pollfd *fds, unsigned int nfds, int timo);

#ifdef __USE_WINSOCK__
#define write(fd, buf, maxcount) _write(fd, buf, (unsigned int)maxcount)
#define read(fd, buf, maxcount) _read(fd, buf, (unsigned int)maxcount)
#endif

int smb2_getaddrinfo(const char *node, const char*service,
                const struct addrinfo *hints,
                struct addrinfo **res);
void smb2_freeaddrinfo(struct addrinfo *res);

#define getaddrinfo smb2_getaddrinfo
#define freeaddrinfo smb2_freeaddrinfo

#else

#undef poll
#define poll WSAPoll

#endif

#ifdef __USE_WINSOCK__

ssize_t writev(t_socket fd, const struct iovec* vector, int count);
ssize_t readv(t_socket fd, const struct iovec* vector, int count);

#else

inline int writev(t_socket sock, struct iovec *iov, int nvecs)
{
  DWORD ret;

  int res = WSASend(sock, (LPWSABUF)iov, nvecs, &ret, 0, NULL, NULL);

  if (res == 0) {
    return (int)ret;
  }
  return -1;
}

inline int readv(t_socket sock, struct iovec *iov, int nvecs)
{
  DWORD ret;
  DWORD flags = 0;

  int res = WSARecv(sock, (LPWSABUF)iov, nvecs, &ret, &flags, NULL, NULL);

  if (res == 0) {
    return (int)ret;
  }
  return -1;
}
#endif

#ifdef __USE_WINSOCK__
#define close(x) _close((int)x)
#else
#define close closesocket
#endif

void srandom(unsigned int seed);
int random(void);

int getlogin_r(char *buf, size_t size);

int getpid();

#pragma warning( disable : 4090 ) 

#define strdup _strdup

#ifdef _XBOX
/* just pretend they are the same so we compile */
#define sockaddr_in6 sockaddr_in
#endif

#endif /* _XBOX */

#ifdef PICO_PLATFORM

#include "lwip/netdb.h"
#include "lwip/sockets.h"

#ifndef SOL_TCP
#define SOL_TCP 6
#endif

#define EAI_AGAIN EAGAIN
long long int be64toh(long long int x);
int getlogin_r(char *buf, size_t size);

#endif /* PICO_PLATFORM */

#ifdef __DREAMCAST__

#include <netdb.h>
#include <unistd.h>

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

ssize_t writev(t_socket fd, const struct iovec *iov, int iovcnt);
ssize_t readv(t_socket fd, const struct iovec *iov, int iovcnt);

int getlogin_r(char *buf, size_t size);

#endif /* __DREAMCAST__ */

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)

#include <errno.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#if defined(__amigaos4__) || defined(__AROS__)
#include <sys/uio.h>
#endif
int getlogin_r(char *buf, size_t size);
#if !defined(__amigaos4__) && (defined(__AMIGA__) || defined(__AROS__))
#include <proto/bsdsocket.h>
#undef HAVE_UNISTD_H
#define close CloseSocket
#undef getaddrinfo
#undef freeaddrinfo
#endif
#define strncpy(a,b,c) strcpy(a,b)

#define POLLIN      0x0001    /* There is data to read */
#define POLLPRI     0x0002    /* There is urgent data to read */
#define POLLOUT     0x0004    /* Writing now will not block */
#define POLLERR     0x0008    /* Error condition */
#define POLLHUP     0x0010    /* Hung up */

struct pollfd {
        int fd;
        short events;
        short revents;
};

#ifndef HAVE_ADDRINFO

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};

#endif

int poll(struct pollfd *fds, unsigned int nfds, int timo);

int smb2_getaddrinfo(const char *node, const char*service,
                const struct addrinfo *hints,
                struct addrinfo **res);
void smb2_freeaddrinfo(struct addrinfo *res);

#define getaddrinfo smb2_getaddrinfo
#define freeaddrinfo smb2_freeaddrinfo

#ifndef __amigaos4__
ssize_t writev(t_socket fd, const struct iovec *iov, int iovcnt);
ssize_t readv(t_socket fd, const struct iovec *iov, int iovcnt);
#endif

#if !defined(HAVE_SOCKADDR_STORAGE)
/*
 * RFC 2553: protocol-independent placeholder for socket addresses
 */
#define _SS_MAXSIZE	128
#define _SS_ALIGNSIZE	(sizeof(double))
#define _SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(unsigned char) * 2)
#define _SS_PAD2SIZE	(_SS_MAXSIZE - sizeof(unsigned char) * 2 - \
				_SS_PAD1SIZE - _SS_ALIGNSIZE)

struct sockaddr_storage {
#ifdef HAVE_SOCKADDR_LEN
	unsigned char ss_len;		/* address length */
	unsigned char ss_family;	/* address family */
#else
	unsigned short ss_family;
#endif
	char	__ss_pad1[_SS_PAD1SIZE];
	double	__ss_align;	/* force desired structure storage alignment */
	char	__ss_pad2[_SS_PAD2SIZE];
};
#endif

#ifndef EAI_AGAIN
#define EAI_AGAIN EAGAIN
#endif

#ifndef EAI_FAIL
#define EAI_FAIL        4
#endif

#ifndef EAI_MEMORY
#define EAI_MEMORY      6
#endif

#ifndef EAI_NONAME
#define EAI_NONAME      8
#endif

#ifndef EAI_SERVICE
#define EAI_SERVICE     9
#endif

#endif

#ifdef __PS2__

#ifdef _EE
#include <unistd.h>
#else
#ifndef __ps2sdk_iop__
#include <alloc.h>
#endif
#include <stdint.h>
#include <ps2ip.h>
#include <loadcore.h>
#endif

#ifdef PS2RPC
#include <ps2ips.h>
#else
#include <ps2ip.h>
#endif

#ifdef _IOP
typedef uint32_t UWORD32;
typedef size_t ssize_t;
#include <tcpip.h>
#endif

long long int be64toh(long long int x);
#ifdef _IOP
char *strdup(const char *s);

int gethostname(char *name, size_t len);
int random(void);
void srandom(unsigned int seed);
time_t time(time_t *tloc);
int asprintf(char **strp, const char *fmt, ...);
#endif
int getlogin_r(char *buf, size_t size);

#ifdef _IOP
int getpid();
#define close(x) lwip_close(x)
#define snprintf(format, n, ...) sprintf(format, __VA_ARGS__)
#define fcntl(a,b,c) lwip_fcntl(a,b,c)
#endif

#ifndef POLLIN
#define POLLIN      0x0001    /* There is data to read */
#endif

#ifndef POLLPRI
#define POLLPRI     0x0002    /* There is urgent data to read */
#endif

#ifndef POLLOUT
#define POLLOUT     0x0004    /* Writing now will not block */
#endif

#ifndef POLLERR
#define POLLERR     0x0008    /* Error condition */
#endif

#ifndef POLLHUP
#define POLLHUP     0x0010    /* Hung up */
#endif

struct pollfd {
        int fd;
        short events;
        short revents;
};

int poll(struct pollfd *fds, unsigned int nfds, int timo);

struct iovec {
  void  *iov_base;
  size_t iov_len;
};

#ifdef _IOP
#undef connect
#define connect(a,b,c) iop_connect(a,b,c)
int iop_connect(int sockfd, struct sockaddr *addr, socklen_t addrlen);

#define write(a,b,c) lwip_send(a,b,c,MSG_DONTWAIT)
#define read(a,b,c) lwip_recv(a,b,c,MSG_DONTWAIT)
#endif

#ifdef __ps2sdk_iop__
void *malloc(int size);

void free(void *ptr);

void *calloc(size_t nmemb, size_t size);
#endif

ssize_t writev(t_socket fd, const struct iovec *iov, int iovcnt);
ssize_t readv(t_socket fd, const struct iovec *iov, int iovcnt);

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#ifndef EAI_AGAIN
#define EAI_AGAIN EAGAIN
#endif

#ifdef _IOP
#define strerror(x) "Unknown"
#endif

/* just pretend they are the same so we compile */
#define sockaddr_in6 sockaddr_in

#endif /* __PS2__ */

#ifdef PS3_PPU_PLATFORM

#include <sys/time.h>
#include <netdb.h>
#include <net/poll.h>

int getlogin_r(char *buf, size_t size);
void srandom(unsigned int seed);
int random(void);
#define getaddrinfo smb2_getaddrinfo
#define freeaddrinfo smb2_freeaddrinfo

#define TCP_NODELAY     1  /* Don't delay send to coalesce packets  */

#define EAI_FAIL        4
#define EAI_MEMORY      6
#define EAI_NONAME      8
#define EAI_SERVICE     9

int smb2_getaddrinfo(const char *node, const char*service,
                const struct addrinfo *hints,
                struct addrinfo **res);
void smb2_freeaddrinfo(struct addrinfo *res);

ssize_t writev(t_socket fd, const struct iovec *iov, int iovcnt);
ssize_t readv(t_socket fd, const struct iovec *iov, int iovcnt);

#define SOL_TCP IPPROTO_TCP
#define EAI_AGAIN EAGAIN

#if !defined(HAVE_SOCKADDR_STORAGE)
/*
 * RFC 2553: protocol-independent placeholder for socket addresses
 */
#define _SS_MAXSIZE	128
#define _SS_ALIGNSIZE	(sizeof(double))
#define _SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(unsigned char) * 2)
#define _SS_PAD2SIZE	(_SS_MAXSIZE - sizeof(unsigned char) * 2 - \
				_SS_PAD1SIZE - _SS_ALIGNSIZE)

struct sockaddr_storage {
#ifdef HAVE_SOCKADDR_LEN
	unsigned char ss_len;		/* address length */
	unsigned char ss_family;	/* address family */
#else
	unsigned short ss_family;
#endif
	char	__ss_pad1[_SS_PAD1SIZE];
	double	__ss_align;	/* force desired structure storage alignment */
	char	__ss_pad2[_SS_PAD2SIZE];
};
#endif


/* just pretend they are the same so we compile */
#define sockaddr_in6 sockaddr_in

#endif

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)

#ifndef ENODATA
#define ENODATA ENOATTR
#endif

#ifndef ENOLINK
#define ENOLINK ENOATTR
#endif

#include <stdint.h>

#endif /* PS4_PLATFORM */

#ifdef __linux__
#include <stdint.h>
#endif

#ifdef __APPLE__
#include <stdint.h>
#endif

#ifdef __vita__

#include <netinet/in.h>

int getlogin_r(char *buf, size_t size);

ssize_t writev(t_socket fd, const struct iovec *iov, int iovcnt);
ssize_t readv(t_socket fd, const struct iovec *iov, int iovcnt);

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#endif

#if defined(__SWITCH__) || defined(__3DS__) || defined(__wii__) || defined(__gamecube__) || defined(__WIIU__) || defined(__NDS__)

#include <sys/types.h>

#if defined(__3DS__) || defined(__wii__) || defined(__gamecube__) || defined(__WIIU__) || defined(__NDS__)
struct iovec {
  void  *iov_base;
  size_t iov_len;
};	
#if defined(__wii__) || defined(__gamecube__) || defined(__NDS__)
#ifndef __NDS__
#include <network.h>
#endif
#if !defined(HAVE_SOCKADDR_STORAGE)
struct sockaddr_storage {
#ifdef HAVE_SOCKADDR_SA_LEN
	unsigned char ss_len;
#endif /* HAVE_SOCKADDR_SA_LEN */
	unsigned char ss_family;
	unsigned char fill[127];
};
#endif

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};

#endif
#define sockaddr_in6 sockaddr_in
#else
#include <sys/_iovec.h>
#endif

#ifndef EAI_AGAIN
#define EAI_AGAIN EAGAIN
#endif

#ifndef EAI_FAIL
#define EAI_FAIL        4
#endif

#ifndef EAI_MEMORY
#define EAI_MEMORY      6
#endif

#ifndef EAI_NONAME
#define EAI_NONAME      8
#endif

#ifndef EAI_SERVICE
#define EAI_SERVICE     9
#endif

#ifndef POLLIN
#define POLLIN      0x0001    /* There is data to read */
#endif
#ifndef POLLPRI
#define POLLPRI     0x0002    /* There is urgent data to read */
#endif
#ifndef POLLOUT
#define POLLOUT     0x0004    /* Writing now will not block */
#endif
#ifndef POLLERR
#define POLLERR     0x0008    /* Error condition */
#endif
#ifndef POLLHUP
#define POLLHUP     0x0010    /* Hung up */
#endif

#ifndef TCP_NODELAY
#define TCP_NODELAY     1  /* Don't delay send to coalesce packets  */
#endif

#ifndef __NDS__
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
#endif

ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
int getlogin_r(char *buf, size_t size);

#if defined(__wii__) || defined(__gamecube__) || defined(__NDS__)
int smb2_getaddrinfo(const char *node, const char*service,
                const struct addrinfo *hints,
                struct addrinfo **res);
void smb2_freeaddrinfo(struct addrinfo *res);

#define getaddrinfo smb2_getaddrinfo
#define freeaddrinfo smb2_freeaddrinfo

#ifndef __NDS__
#define connect net_connect
#define socket net_socket 
#define setsockopt net_setsockopt
s32 getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
#define select net_select
#define accept net_accept
#define listen net_listen
#define bind net_bind
#endif

struct pollfd {
        int fd;
        short events;
        short revents;
};

int poll(struct pollfd *fds, unsigned int nfds, int timo);

#endif

#endif

#ifdef ESP_PLATFORM
#include <stddef.h>
#include <esp_system.h>
#include <sys/types.h>

#ifndef SOL_TCP
#define SOL_TCP 6
#endif

void srandom(unsigned int seed);
long random(void);
int getlogin_r(char *buf, size_t size);
#endif

#ifdef __ANDROID__
#include <stddef.h>
/* getlogin_r() was added in API 28 */
#if __ANDROID_API__ < 28
int getlogin_r(char *buf, size_t size);
#endif
#endif /* __ANDROID__ */

#ifndef O_RDONLY
#define O_RDONLY	00000000
#endif

#ifndef O_WRONLY
#define O_WRONLY	00000001
#endif

#ifndef O_RDWR
#define O_RDWR		00000002
#endif

#ifndef O_DSYNC
#define O_DSYNC		040000
#endif /* !O_DSYNC */

#ifndef __O_SYNC 
#define __O_SYNC	020000000
#endif

#ifndef O_SYNC
#define O_SYNC		(__O_SYNC|O_DSYNC)
#endif /* !O_SYNC */

#ifndef O_ACCMODE
#define O_ACCMODE (O_RDWR|O_WRONLY|O_RDONLY)
#endif /* !O_ACCMODE */

#ifndef ENOMEM
#define ENOMEM 12
#endif

#ifndef EINVAL
#define EINVAL 22
#endif

#ifdef __cplusplus
}
#endif

#endif /* _COMPAT_H_ */
