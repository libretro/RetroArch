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

#include "netplay_compat.h"
#include "netplay.h"
#include <stdlib.h>
#include <string.h>

int getaddrinfo_rarch(const char *node, const char *service,
      const struct addrinfo *hints,
      struct addrinfo **res)
{
#ifdef HAVE_SOCKET_LEGACY
   struct sockaddr_in *in_addr;
   struct addrinfo *info = (struct addrinfo*)calloc(1, sizeof(*info));
   if (!info)
      return -1;

   info->ai_family = AF_INET;
   info->ai_socktype = hints->ai_socktype;

   in_addr = (struct sockaddr_in*)calloc(1, sizeof(*in_addr));

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
#else
   return getaddrinfo(node, service, hints, res);
#endif
}

void freeaddrinfo_rarch(struct addrinfo *res)
{
#ifdef HAVE_SOCKET_LEGACY
   free(res->ai_addr);
   free(res);
#else
   freeaddrinfo(res);
#endif
}

bool socket_nonblock(int fd)
{
#if defined(__CELLOS_LV2__)
   int i = 1;
   setsockopt(fd, SOL_SOCKET, SO_NBIO, &i, sizeof(int));
   setsockopt(fd, SOL_SOCKET, SO_NBIO, &i, sizeof(int));
   return true;
#elif defined(_WIN32)
   u_long mode = 1;
   return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
   return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == 0;
#endif
}

int socket_close(int fd)
{ 
#if defined(_WIN32) && !defined(_XBOX360)
   /* WinSock has headers from the stone age. */
   return closesocket(fd);
#elif defined(__CELLOS_LV2__)
   return socketclose(fd);
#else
   return close(fd);
#endif
}

int socket_select(int nfds, fd_set *readfs, fd_set *writefds,
      fd_set *errorfds, struct timeval *timeout)
{
#if defined(__CELLOS_LV2__)
   return socketselect(nfds, readfs, writefds, errorfds, timeout);
#else
   return select(nfds, readfs, writefds, errorfds, timeout);
#endif
}
