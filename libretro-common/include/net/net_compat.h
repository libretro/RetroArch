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

#ifndef LIBRETRO_SDK_NET_COMPAT_H__
#define LIBRETRO_SDK_NET_COMPAT_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boolean.h>
#include <retro_inline.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(_XBOX)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#if _WIN32_WINNT >= 0x0600
#define NETWORK_HAVE_POLL 1
#endif

#elif defined(_XBOX)
#define NOD3D

#include <xtl.h>
#include <io.h>

#define socklen_t int

#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE 0 /* verify if correct */
#endif

struct hostent
{
   char *h_name;
   char **h_aliases;
   int  h_addrtype;
   int  h_length;
   char **h_addr_list;
   char *h_addr;
};

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

#define sockaddr_in SceNetSockaddrIn
#define sockaddr SceNetSockaddr
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
};

struct SceNetInAddr inet_aton(const char *ip_addr);

#else
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
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

#ifdef WIIU
#define WIIU_RCVBUF (128 * 2 * 1024)
#define WIIU_SNDBUF (128 * 2 * 1024)
#endif

#if defined(__PSL1GHT__)
#include <net/poll.h>

#define NETWORK_HAVE_POLL 1

#elif !defined(WIIU) && !defined(__PS3__)
#include <poll.h>

#define NETWORK_HAVE_POLL 1

#endif
#endif

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#include <errno.h>

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

/* Compatibility layer for legacy or incomplete BSD socket implementations.
 * Only for IPv4. Mostly useful for the consoles which do not support
 * anything reasonably modern on the socket API side of things. */
#ifdef HAVE_SOCKET_LEGACY

#define sockaddr_storage sockaddr_in
#define addrinfo addrinfo_retro__

#ifndef AI_PASSIVE
#define AI_PASSIVE 1
#endif

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

/* gai_strerror() not used, so we skip that. */

#endif

static INLINE bool isagain(int bytes)
{
#if defined(_WIN32)
   return (bytes == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK);
#elif !defined(__PSL1GHT__) && defined(__PS3__) 
   return (sys_net_errno == SYS_NET_EAGAIN) || (sys_net_errno == SYS_NET_EWOULDBLOCK);
#elif defined(VITA)
   return (bytes == SCE_NET_ERROR_EAGAIN) || (bytes == SCE_NET_ERROR_EWOULDBLOCK);
#elif defined(WIIU)
   return (bytes == -1) && (socketlasterr() == SO_SUCCESS || socketlasterr() == SO_EWOULDBLOCK);
#else
   return (bytes < 0) && (errno == EAGAIN || errno == EWOULDBLOCK);
#endif
}

static INLINE bool isinprogress(int bytes)
{
#if defined(_WIN32)
   return (bytes == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK);
#elif !defined(__PSL1GHT__) && defined(__PS3__) 
   return (sys_net_errno == SYS_NET_EINPROGRESS);
#elif defined(VITA)
   return (bytes == SCE_NET_ERROR_EINPROGRESS);
#elif defined(WIIU)
   return (bytes == -1) && (socketlasterr() == SO_SUCCESS || socketlasterr() == SO_EWOULDBLOCK);
#else
   return (bytes < 0) && (errno == EINPROGRESS);
#endif
}

uint16_t inet_htons(uint16_t hostshort);

int inet_ptrton(int af, const char *src, void *dst);

int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **res);

void freeaddrinfo_retro(struct addrinfo *res);

bool addr_6to4(struct sockaddr_storage *addr);

/**
 * network_init:
 *
 * Platform specific socket library initialization.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool network_init(void);

/**
 * network_deinit:
 *
 * Deinitialize platform specific socket libraries.
 **/
void network_deinit(void);

const char *inet_ntop_compat(int af, const void *src, char *dst, socklen_t cnt);

bool udp_send_packet(const char *host, uint16_t port, const char *msg);

#endif
