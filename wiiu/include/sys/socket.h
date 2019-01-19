#pragma once
#include <wiiu/types.h>
#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOL_SOCKET      -1

#define INADDR_ANY      0

#define AF_UNSPEC 0
#define AF_INET   2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define MSG_DONTWAIT    0x0020
/* #define MSG_DONTWAIT    0x0004 */

#define SO_REUSEADDR    0x0004
#define SO_NBIO         0x1014
#define SO_NONBLOCK     0x1016

/* return codes */
#define SO_SUCCESS      0
#define SO_EWOULDBLOCK  6
#define SO_EINVAL      11

#define EWOULDBLOCK SO_EWOULDBLOCK
#define EAGAIN SO_EWOULDBLOCK
#define ENOBUFS 105 /* No buffer space available */

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

int socketlasterr(void);

#ifdef __cplusplus
}
#endif
