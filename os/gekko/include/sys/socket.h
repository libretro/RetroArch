/* Sockets, Wii: IPv4 through IOS (rvl/net.c).  Socket descriptors are
 * C library file descriptors, so close(), read() and write() work. */

#ifndef GEKKO_SYS_SOCKET_H
#define GEKKO_SYS_SOCKET_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __socklen_t   socklen_t;
typedef __sa_family_t sa_family_t;

struct sockaddr
{
   uint8_t     sa_len;
   sa_family_t sa_family;
   char        sa_data[14];
};

struct sockaddr_storage
{
   uint8_t     ss_len;
   sa_family_t ss_family;
   char        ss_data[26];
};

struct linger
{
   int l_onoff;
   int l_linger;
};

#define AF_UNSPEC     0
#define AF_INET       2
#define PF_UNSPEC     AF_UNSPEC
#define PF_INET       AF_INET

#define SOCK_STREAM   1
#define SOCK_DGRAM    2
#define SOCK_RAW      3

#define SOL_SOCKET    0xffff
#define SO_REUSEADDR  0x0004
#define SO_KEEPALIVE  0x0008
#define SO_BROADCAST  0x0020
#define SO_LINGER     0x0080
#define SO_OOBINLINE  0x0100
#define SO_SNDBUF     0x1001
#define SO_RCVBUF     0x1002
#define SO_SNDLOWAT   0x1003
#define SO_RCVLOWAT   0x1004
#define SO_TYPE       0x1008
#define SO_ERROR      0x1009

#define MSG_OOB       0x01
#define MSG_PEEK      0x02
#define MSG_DONTWAIT  0x04

#define SHUT_RD       0
#define SHUT_WR       1
#define SHUT_RDWR     2

#define SOMAXCONN     5

int     socket(int domain, int type, int protocol);
int     bind(int fd, const struct sockaddr *addr, socklen_t len);
int     listen(int fd, int backlog);
int     accept(int fd, struct sockaddr *addr, socklen_t *len);
int     connect(int fd, const struct sockaddr *addr, socklen_t len);
ssize_t send(int fd, const void *buf, size_t len, int flags);
ssize_t sendto(int fd, const void *buf, size_t len, int flags,
      const struct sockaddr *to, socklen_t tolen);
ssize_t recv(int fd, void *buf, size_t len, int flags);
ssize_t recvfrom(int fd, void *buf, size_t len, int flags,
      struct sockaddr *from, socklen_t *fromlen);
int     setsockopt(int fd, int level, int name, const void *val,
      socklen_t len);
int     getsockopt(int fd, int level, int name, void *val, socklen_t *len);
int     getsockname(int fd, struct sockaddr *addr, socklen_t *len);
int     getpeername(int fd, struct sockaddr *addr, socklen_t *len);
int     shutdown(int fd, int how);

#ifdef __cplusplus
}
#endif

#endif
