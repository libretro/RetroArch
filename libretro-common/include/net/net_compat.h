/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NETPLAY_COMPAT_H__
#define NETPLAY_COMPAT_H__

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
#define MSG_DONTWAIT PSP2_NET_MSG_DONTWAIT

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

static INLINE int inet_pton(int af, const char *src, void *dst)
{
   if (af != AF_INET)
      return -1;

   return inet_aton (src, dst);
}
#endif

static INLINE bool isagain(int bytes)
{
#if defined(_WIN32)
   if (bytes != SOCKET_ERROR)
      return false;
   if (WSAGetLastError() != WSAEWOULDBLOCK)
      return false;
   return true;
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
#define addrinfo addrinfo_rarch__

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

int getaddrinfo_rarch(const char *node, const char *service,
      const struct addrinfo *hints,
      struct addrinfo **res);

void freeaddrinfo_rarch(struct addrinfo *res);

bool socket_nonblock(int fd);

int socket_close(int fd);

int socket_select(int nfds, fd_set *readfs, fd_set *writefds,
      fd_set *errorfds, struct timeval *timeout);

int socket_send_all_blocking(int fd, const void *data_, size_t size);

int socket_receive_all_blocking(int fd, void *data_, size_t size);

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

#endif

