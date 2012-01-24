/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "netplay_compat.h"

#undef getaddrinfo
#undef freeaddrinfo
#undef sockaddr_storage
#undef addrinfo

#if defined(_WIN32) && !defined(_XBOX)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#elif defined(_XBOX)
#include <Xtl.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define addrinfo addrinfo_ssnes__

int getaddrinfo_ssnes__(const char *node, const char *service,
      const struct addrinfo *hints,
      struct addrinfo **res)
{
   struct addrinfo *info = (struct addrinfo*)calloc(1, sizeof(*info));
   if (!info)
      return -1;

   info->ai_family = AF_INET;
   info->ai_socktype = hints->ai_socktype;

   struct sockaddr_in *in_addr = (struct sockaddr_in*)calloc(1, sizeof(*in_addr));
   if (!in_addr)
   {
      free(info);
      return -1;
   }

   info->ai_addrlen = sizeof(*in_addr);

   in_addr->sin_family = AF_INET;
   in_addr->sin_port = htons(strtoul(service, NULL, 0));

   if (!node && (hints->ai_flags & AI_PASSIVE))
      in_addr->sin_addr.s_addr = INADDR_ANY;
   else if (node && isdigit(*node))
      in_addr->sin_addr.s_addr = inet_addr(node);
   else if (node && !isdigit(*node))
   {
      struct hostent *host = gethostbyname(node);
      if (!host || !host->h_addr_list[0])
         goto error;

      in_addr->sin_addr.s_addr = inet_addr(host->h_addr_list[0]);
   }
   else
      goto error;

   info->ai_addr = (struct sockaddr*)in_addr;
   *res = info;

   return 0;

error:
   free(in_addr);
   free(info);
   return -1;
}

void freeaddrinfo_ssnes__(struct addrinfo *res)
{
   free(res->ai_addr);
   free(res);
}

