#pragma once
#include <wiiu/types.h>
#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOL_SOCKET      0xFFFF

#define INADDR_ANY      0

#define PF_UNSPEC       0
#define PF_INET         2
#define PF_INET6        23

#define AF_UNSPEC       PF_UNSPEC
#define AF_INET         PF_INET
#define AF_INET6        PF_INET6

#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define MSG_OOB         0x0001
#define MSG_PEEK        0x0002
#define MSG_DONTWAIT    0x0004
#define MSG_DONTROUTE   0x0000  // ???
#define MSG_WAITALL     0x0000  // ???
#define MSG_MORE        0x0000  // ???
#define MSG_NOSIGNAL    0x0000  // there are no signals

#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

/*
 * SOL_SOCKET options
 */
#define SO_REUSEADDR    0x0004      // reuse address
#define SO_LINGER       0x0080      // linger (no effect?)
#define SO_OOBINLINE    0x0100      // out-of-band data inline (no effect?)
#define SO_SNDBUF       0x1001      // send buffer size
#define SO_RCVBUF       0x1002      // receive buffer size
#define SO_SNDLOWAT     0x1003      // send low-water mark (no effect?)
#define SO_RCVLOWAT     0x1004      // receive low-water mark
#define SO_TYPE         0x1008      // get socket type
#define SO_ERROR        0x1009      // get socket error

typedef uint32_t socklen_t;
typedef uint16_t sa_family_t;

struct sockaddr
{
   sa_family_t sa_family;
   char        sa_data[];
};

struct sockaddr_storage
{
   sa_family_t ss_family;
   char        __ss_padding[26];
};

struct linger
{
   int l_onoff;
   int l_linger;
};

struct in_addr
{
   unsigned int s_addr;
};

struct sockaddr_in
{
   short sin_family;
   unsigned short sin_port;
   struct in_addr sin_addr;
   char sin_zero[8];
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
char *inet_ntoa(struct in_addr in);
int inet_aton(const char *cp, struct in_addr *inp);

#ifdef __cplusplus
}
#endif
