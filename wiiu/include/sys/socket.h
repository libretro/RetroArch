#pragma once
#include <wiiu/types.h>
#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOL_SOCKET      -1

#define INADDR_ANY      0

#define PF_UNSPEC 0
#define PF_INET   2
#define AF_UNSPEC 0
#define AF_INET   2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define MSG_DONTWAIT    0x0020
/* #define MSG_DONTWAIT    0x0004 */

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#define SO_REUSEADDR    0x0004
#define SO_KEEPALIVE    0x0008
#define SO_BROADCAST    0x0020
#define SO_TCPSACK      0x0200
#define SO_WINSCALE     0x0400
#define SO_SNDBUF       0x1001
#define SO_RCVBUF       0x1002
#define SO_NBIO         0x1014
#define SO_BIO          0x1015
#define SO_NONBLOCK     0x1016
#define SO_RUSRBUF      0x10000

#define SO_SUCCESS         0
#define SO_ENOBUFS         1
#define SO_ETIMEDOUT       2
#define SO_EISCONN         3
#define SO_EOPNOTSUPP      4
#define SO_ECONNABORTED    5
#define SO_EWOULDBLOCK     6
#define SO_ECONNREFUSED    7
#define SO_ECONNRESET      8
#define SO_ENOTCONN        9
#define SO_EALREADY        10
#define SO_EINVAL          11
#define SO_EMSGSIZE        12
#define SO_EPIPE           13
#define SO_EDESTADDRREQ    14
#define SO_ESHUTDOWN       15
#define SO_ENOPROTOOPT     16
#define SO_EHAVEOOB        17
#define SO_ENOMEM          18
#define SO_EADDRNOTAVAIL   19
#define SO_EADDRINUSE      20
#define SO_EAFNOSUPPORT    21
#define SO_EINPROGRESS     22
#define SO_ELOWER          23
#define SO_ENOTSOCK        24
#define SO_EIEIO           27
#define SO_ETOOMANYREFS    28
#define SO_EFAULT          29
#define SO_ENETUNREACH     30
#define SO_EPROTONOSUPPORT 31
#define SO_EPROTOTYPE      32
#define SO_ERROR           41
#define SO_ENOLIBRM        42
#define SO_ELIBNOTREADY    43
#define SO_EBUSY           44
#define SO_EUNKNOWN        45
#define SO_EAPIERROR       46
#define SO_ERANGEINVALID   47
#define SO_ENORESOURCES    48
#define SO_EBADFD          49
#define SO_EABORTED        50
#define SO_EMFILE          51

#ifdef EWOULDBLOCK
#undef EWOULDBLOCK
#endif

#ifdef EAGAIN
#undef EAGAIN
#endif

#ifdef EINPROGRESS
#undef EINPROGRESS
#endif

#define EWOULDBLOCK SO_EWOULDBLOCK
#define EAGAIN SO_EWOULDBLOCK
#define EINPROGRESS SO_EINPROGRESS

typedef uint32_t socklen_t;
typedef uint16_t sa_family_t;

struct sockaddr
{
   sa_family_t sa_family;
   char        sa_data[];
};

/* Wii U only supports IPv4 so we make sockaddr_storage
   be sockaddr_in for compatibility.
 */
#define sockaddr_storage sockaddr_in

struct linger
{
   int l_onoff;
   int l_linger;
};

void socket_lib_init();
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int socketclose(int sockfd);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
int listen(int sockfd, int backlog);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int shutdown(int sockfd, int how);
int socket(int domain, int type, int protocol);
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int somemopt (int req_type, char* mem, unsigned int memlen, int flags);

int socketlasterr(void);

#ifdef __cplusplus
}
#endif
