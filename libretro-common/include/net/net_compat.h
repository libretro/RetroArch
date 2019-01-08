/* Copyright  (C) 2010-2018 The RetroArch team
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

#ifndef LIBRETRO_SDK_NETPLAY_COMPAT_H__
#define LIBRETRO_SDK_NETPLAY_COMPAT_H__

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

#elif defined(_XBOX)

#define NOD3D
#include <xtl.h>
#include <io.h>

#elif defined(GEKKO)

#include <network.h>

#elif defined(VITA)

#include <psp2/net/net.h>
#include <psp2/net/netctl.h>

#define sockaddr_in SceNetSockaddrIn
#define sockaddr SceNetSockaddr
#define sendto sceNetSendto
#define recvfrom sceNetRecvfrom
#define socket(a,b,c) sceNetSocket("unknown",a,b,c)
#define bind sceNetBind
#define accept sceNetAccept
#define setsockopt sceNetSetsockopt
#define connect sceNetConnect
#define listen sceNetListen
#define send sceNetSend
#define recv sceNetRecv
#define MSG_DONTWAIT SCE_NET_MSG_DONTWAIT
#define AF_INET SCE_NET_AF_INET
#define AF_UNSPEC 0
#define INADDR_ANY SCE_NET_INADDR_ANY
#define INADDR_NONE 0xffffffff
#define SOCK_STREAM SCE_NET_SOCK_STREAM
#define SOCK_DGRAM SCE_NET_SOCK_DGRAM
#define SOL_SOCKET SCE_NET_SOL_SOCKET
#define SO_REUSEADDR SCE_NET_SO_REUSEADDR
#define SO_SNDBUF SCE_NET_SO_SNDBUF
#define SO_SNDTIMEO SCE_NET_SO_SNDTIMEO
#define SO_NBIO SCE_NET_SO_NBIO
#define htonl sceNetHtonl
#define ntohl sceNetNtohl
#define htons sceNetHtons
#define socklen_t unsigned int

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

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <cell/sysmodule.h>
#include <netex/net.h>
#include <netex/libnetctl.h>
#include <sys/timer.h>

#ifndef EWOULDBLOCK
#define EWOULDBLOCK SYS_NET_EWOULDBLOCK
#endif

#else
#include <signal.h>
#endif

#endif

#include <errno.h>

#ifdef GEKKO
#define sendto(s, msg, len, flags, addr, tolen) net_sendto(s, msg, len, 0, addr, 8)
#define socket(domain, type, protocol) net_socket(domain, type, protocol)
#define bind(s, name, namelen) net_bind(s, name, namelen)
#define listen(s, backlog) net_listen(s, backlog)
#define accept(s, addr, addrlen) net_accept(s, addr, addrlen)
#define connect(s, addr, addrlen) net_connect(s, addr, addrlen)
#define send(s, data, size, flags) net_send(s, data, size, flags)
#define recv(s, mem, len, flags) net_recv(s, mem, len, flags)
#define recvfrom(s, mem, len, flags, from, fromlen) net_recvfrom(s, mem, len, flags, from, fromlen)
#define select(maxfdp1, readset, writeset, exceptset, timeout) net_select(maxfdp1, readset, writeset, exceptset, timeout)
#endif

static INLINE bool isagain(int bytes)
{
#if defined(_WIN32)
   if (bytes != SOCKET_ERROR)
      return false;
   if (WSAGetLastError() != WSAEWOULDBLOCK)
      return false;
   return true;
#elif defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   return (sys_net_errno == SYS_NET_EWOULDBLOCK) || (sys_net_errno == SYS_NET_EAGAIN);//35
#elif defined(VITA)
   return (bytes<0 && (bytes == SCE_NET_ERROR_EAGAIN || bytes == SCE_NET_ERROR_EWOULDBLOCK));
#elif defined(WIIU)
   return (bytes == -1) && ((socketlasterr() == SO_SUCCESS) || (socketlasterr() == SO_EWOULDBLOCK));
#else
   return (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
#endif
}

#ifdef _XBOX
#define socklen_t int

#ifndef h_addr
#define h_addr h_addr_list[0] /* for backward compatibility */
#endif

#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE 0 /* verify if correct */
#endif
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

/* Compatibility layer for legacy or incomplete BSD socket implementations.
 * Only for IPv4. Mostly useful for the consoles which do not support
 * anything reasonably modern on the socket API side of things. */

#ifdef HAVE_SOCKET_LEGACY

#define sockaddr_storage sockaddr_in
#define addrinfo addrinfo_retro__

struct addrinfo
{
   int ai_flags;
   int ai_family;
   int ai_socktype;
   int ai_protocol;
   size_t ai_addrlen;
   struct sockaddr *ai_addr;
   char *ai_canonname;
   struct addrinfo *ai_next;
};

#ifndef AI_PASSIVE
#define AI_PASSIVE 1
#endif

/* gai_strerror() not used, so we skip that. */

#endif

uint16_t inet_htons(uint16_t hostshort);

int inet_ptrton(int af, const char *src, void *dst);

int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **res);

void freeaddrinfo_retro(struct addrinfo *res);

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
