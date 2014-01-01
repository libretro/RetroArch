/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#if defined(_WIN32) && !defined(_XBOX)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#elif defined(_XBOX)
#define NOD3D
#include <xtl.h>
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
#else
#include <signal.h>
#endif
#endif

#ifdef _XBOX
#define socklen_t int
#endif

#if defined(_WIN32)
// Woohoo, Winsock has headers from the STONE AGE. :D
#define close(x) closesocket(x)
#define CONST_CAST (const char*)
#define NONCONST_CAST (char*)
#else
#define CONST_CAST
#define NONCONST_CAST
#include <sys/time.h>
#include <unistd.h>

#if defined(__CELLOS_LV2__)
#define close(x) socketclose(x)
#define select(nfds, readfds, writefds, errorfds, timeout) socketselect(nfds, readfds, writefds, errorfds, timeout)
#endif
#endif

// Compatibility layer for legacy or incomplete BSD socket implementations.
// Only for IPv4. Mostly useful for the consoles which do not support
// anything reasonably modern on the socket API side of things.

#ifdef HAVE_SOCKET_LEGACY

#define sockaddr_storage sockaddr_in
#define addrinfo addrinfo_rarch__
#define getaddrinfo(serv, port, hints, res) getaddrinfo_rarch__(serv, port, hints, res)
#define freeaddrinfo(res) freeaddrinfo_rarch__(res)

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

int getaddrinfo(const char *node, const char *service,
      const struct addrinfo *hints,
      struct addrinfo **res);

void freeaddrinfo(struct addrinfo *res);

#ifndef AI_PASSIVE
#define AI_PASSIVE 1
#endif

// gai_strerror() not used, so we skip that.

#endif
#endif

