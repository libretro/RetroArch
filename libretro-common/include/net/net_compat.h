/* Copyright  (C) 2010-2022 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_compat.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBRETRO_SDK_NET_COMPAT_H
#define _LIBRETRO_SDK_NET_COMPAT_H

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

#include <errno.h>

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#include <retro_common_api.h>

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#if _MSC_VER && _MSC_VER <= 1600
/* If we are using MSVC2010 or lower, disable WSAPoll support 
 * to ensure Windows XP and earlier backwards compatibility */
#else
#ifndef WIN32_SUPPORTS_POLL
#define WIN32_SUPPORTS_POLL 1
#endif
#endif

#if defined(WIN32_SUPPORTS_POLL) && defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0600
#define NETWORK_HAVE_POLL 1
#endif

#elif defined(_XBOX)
#define NOD3D

#include <xtl.h>
#include <io.h>

#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE 0 /* verify if correct */
#endif

#define socklen_t unsigned int

#elif defined(VITA)
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>

#define NETWORK_HAVE_POLL 1

#define AF_UNSPEC 0
#define AF_INET SCE_NET_AF_INET

#define SOCK_STREAM SCE_NET_SOCK_STREAM
#define SOCK_DGRAM SCE_NET_SOCK_DGRAM

#define INADDR_ANY SCE_NET_INADDR_ANY
#define INADDR_NONE 0xFFFFFFFF

#define SOL_SOCKET SCE_NET_SOL_SOCKET
#define SO_REUSEADDR SCE_NET_SO_REUSEADDR
#define SO_KEEPALIVE SCE_NET_SO_KEEPALIVE
#define SO_BROADCAST SCE_NET_SO_BROADCAST
#define SO_SNDBUF SCE_NET_SO_SNDBUF
#define SO_RCVBUF SCE_NET_SO_RCVBUF
#define SO_SNDTIMEO SCE_NET_SO_SNDTIMEO
#define SO_RCVTIMEO SCE_NET_SO_RCVTIMEO
#define SO_ERROR SCE_NET_SO_ERROR
#define SO_NBIO SCE_NET_SO_NBIO

#define IPPROTO_IP SCE_NET_IPPROTO_IP
#define IP_MULTICAST_TTL SCE_NET_IP_MULTICAST_TTL

#define IPPROTO_TCP SCE_NET_IPPROTO_TCP
#define TCP_NODELAY SCE_NET_TCP_NODELAY

#define IPPROTO_UDP SCE_NET_IPPROTO_UDP

#define MSG_DONTWAIT SCE_NET_MSG_DONTWAIT

#define POLLIN   SCE_NET_EPOLLIN
#define POLLOUT  SCE_NET_EPOLLOUT
#define POLLERR  SCE_NET_EPOLLERR
#define POLLHUP  SCE_NET_EPOLLHUP
#define POLLNVAL 0

#define sockaddr SceNetSockaddr
#define sockaddr_in SceNetSockaddrIn
#define in_addr SceNetInAddr
#define socklen_t unsigned int

#define socket(a,b,c) sceNetSocket("unknown",a,b,c)
#define getsockname sceNetGetsockname
#define getsockopt sceNetGetsockopt
#define setsockopt sceNetSetsockopt
#define bind sceNetBind
#define listen sceNetListen
#define accept sceNetAccept
#define connect sceNetConnect
#define send sceNetSend
#define sendto sceNetSendto
#define recv sceNetRecv
#define recvfrom sceNetRecvfrom
#define htonl sceNetHtonl
#define ntohl sceNetNtohl
#define htons sceNetHtons
#define ntohs sceNetNtohs
#define inet_ntop sceNetInetNtop
#define inet_pton sceNetInetPton

#elif defined(GEKKO)
#include <network.h>

#define NETWORK_HAVE_POLL 1

#define pollfd pollsd

#define socket(a,b,c) net_socket(a,b,c)
#define getsockopt(a,b,c,d,e) net_getsockopt(a,b,c,d,e)
#define setsockopt(a,b,c,d,e) net_setsockopt(a,b,c,d,e)
#define bind(a,b,c) net_bind(a,b,c)
#define listen(a,b) net_listen(a,b)
#define accept(a,b,c) net_accept(a,b,c)
#define connect(a,b,c) net_connect(a,b,c)
#define send(a,b,c,d) net_send(a,b,c,d)
#define sendto(a,b,c,d,e,f) net_sendto(a,b,c,d,e,f)
#define recv(a,b,c,d) net_recv(a,b,c,d)
#define recvfrom(a,b,c,d,e,f) net_recvfrom(a,b,c,d,e,f)
#define select(a,b,c,d,e) net_select(a,b,c,d,e)
#define gethostbyname(a) net_gethostbyname(a)

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <netinet/in.h>
#ifndef __PSL1GHT__
#include <netinet/tcp.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#if !defined(__PSL1GHT__) && defined(__PS3__)
#include <netex/libnetctl.h>
#include <netex/errno.h>
#else
#include <signal.h>
#endif

#if defined(__PSL1GHT__)
#include <net/poll.h>

#define NETWORK_HAVE_POLL 1

#elif defined(WIIU)
#define WIIU_RCVBUF 0x40000
#define WIIU_SNDBUF 0x40000

#elif !defined(__PS3__)
#include <poll.h>

#define NETWORK_HAVE_POLL 1

#endif
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#if defined(AF_INET6) && !defined(HAVE_SOCKET_LEGACY) && !defined(_3DS)
#define HAVE_INET6 1
#endif

#ifdef NETWORK_HAVE_POLL
#ifdef GEKKO
#define NET_POLL_FD(sockfd, sockfds)    (sockfds)->socket  = (sockfd)
#else
#define NET_POLL_FD(sockfd, sockfds)    (sockfds)->fd      = (sockfd)
#endif
#define NET_POLL_EVENT(sockev, sockfds) (sockfds)->events |= (sockev)
#define NET_POLL_HAS_EVENT(sockev, sockfds) ((sockfds)->revents & (sockev))
#endif

RETRO_BEGIN_DECLS

/* Compatibility layer for legacy or incomplete BSD socket implementations.
 * Only for IPv4. Mostly useful for the consoles which do not support
 * anything reasonably modern on the socket API side of things. */
#ifdef HAVE_SOCKET_LEGACY
#define sockaddr_storage sockaddr_in

#ifdef AI_PASSIVE
#undef AI_PASSIVE
#endif
#ifdef AI_CANONNAME
#undef AI_CANONNAME
#endif
#ifdef AI_NUMERICHOST
#undef AI_NUMERICHOST
#endif
#ifdef AI_NUMERICSERV
#undef AI_NUMERICSERV
#endif

#ifdef NI_NUMERICHOST
#undef NI_NUMERICHOST
#endif
#ifdef NI_NUMERICSERV
#undef NI_NUMERICSERV
#endif
#ifdef NI_NOFQDN
#undef NI_NOFQDN
#endif
#ifdef NI_NAMEREQD
#undef NI_NAMEREQD
#endif
#ifdef NI_DGRAM
#undef NI_DGRAM
#endif

#define AI_PASSIVE     1
#define AI_CANONNAME   2
#define AI_NUMERICHOST 4
#define AI_NUMERICSERV 8

#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define NI_NOFQDN      4
#define NI_NAMEREQD    8
#define NI_DGRAM       16

#ifndef __PS3__
struct addrinfo
{
   int ai_flags;
   int ai_family;
   int ai_socktype;
   int ai_protocol;
   socklen_t ai_addrlen;
   struct sockaddr *ai_addr;
   char *ai_canonname;
   struct addrinfo *ai_next;
};
#endif

/* gai_strerror() not used, so we skip that. */

#else
/* Ensure that getaddrinfo and getnameinfo flags are always defined. */
#ifndef AI_PASSIVE
#define AI_PASSIVE 0
#endif
#ifndef AI_CANONNAME
#define AI_CANONNAME 0
#endif
#ifndef AI_NUMERICHOST
#define AI_NUMERICHOST 0
#endif
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST 0
#endif
#ifndef NI_NUMERICSERV
#define NI_NUMERICSERV 0
#endif
#ifndef NI_NOFQDN
#define NI_NOFQDN 0
#endif
#ifndef NI_NAMEREQD
#define NI_NAMEREQD 0
#endif
#ifndef NI_DGRAM
#define NI_DGRAM 0
#endif

#endif

#if defined(_XBOX)
struct hostent
{
   char *h_name;
   char **h_aliases;
   int  h_addrtype;
   int  h_length;
   char **h_addr_list;
   char *h_addr;
   char *h_end;
};

#elif defined(VITA)
struct pollfd
{
   int fd;
   unsigned events;
   unsigned revents;
   unsigned __pad; /* Align to 64-bits boundary */
};

struct hostent
{
   char *h_name;
   char **h_aliases;
   int  h_addrtype;
   int  h_length;
   char **h_addr_list;
   char *h_addr;
   char *h_end;
};

#endif

static INLINE bool isagain(int val)
{
#if defined(_WIN32)
   return (val == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK);
#elif !defined(__PSL1GHT__) && defined(__PS3__)
   return (sys_net_errno == SYS_NET_EAGAIN) || (sys_net_errno == SYS_NET_EWOULDBLOCK);
#elif defined(VITA)
   return (val == SCE_NET_ERROR_EAGAIN) || (val == SCE_NET_ERROR_EWOULDBLOCK);
#elif defined(WIIU)
   return (val == -1) && (socketlasterr() == SO_SUCCESS || socketlasterr() == SO_EWOULDBLOCK);
#elif defined(GEKKO)
   return (-val == EAGAIN);
#else
   return (val < 0) && (errno == EAGAIN || errno == EWOULDBLOCK);
#endif
}

static INLINE bool isinprogress(int val)
{
#if defined(_WIN32)
   return (val == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK);
#elif !defined(__PSL1GHT__) && defined(__PS3__)
   return (sys_net_errno == SYS_NET_EINPROGRESS);
#elif defined(VITA)
   return (val == SCE_NET_ERROR_EINPROGRESS);
#elif defined(WIIU)
   return (val == -1) && (socketlasterr() == SO_EINPROGRESS);
#elif defined(GEKKO)
   return (-val == EINPROGRESS);
#else
   return (val < 0) && (errno == EINPROGRESS);
#endif
}

#if defined(_WIN32) && !defined(_XBOX)
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);
#endif

#elif defined(_XBOX)
struct hostent *gethostbyname(const char *name);

#elif defined(VITA)
char *inet_ntoa(struct in_addr in);
int inet_aton(const char *cp, struct in_addr *inp);
uint32_t inet_addr(const char *cp);

struct hostent *gethostbyname(const char *name);

#elif defined(GEKKO)
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

#endif

int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **res);

void freeaddrinfo_retro(struct addrinfo *res);

int getnameinfo_retro(const struct sockaddr *addr, socklen_t addrlen,
      char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

bool addr_6to4(struct sockaddr_storage *addr);

bool ipv4_is_lan_address(const struct sockaddr_in *addr);
bool ipv4_is_cgnat_address(const struct sockaddr_in *addr);

/**
 * network_init:
 *
 * Platform specific socket library initialization.
 *
 * @return true if successful, otherwise false.
 **/
bool network_init(void);

RETRO_END_DECLS

#endif
