/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - pinumbernumber
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/epoll.h>

#include "epoll_common.h"

#include "../../verbosity.h"

bool epoll_new(int *epoll_fd)
{
   *epoll_fd = epoll_create(32);
   if (*epoll_fd < 0)
      return false;

   return true;
}

void epoll_free(int *epoll_fd)
{
   if (*epoll_fd >= 0)
      close(*epoll_fd);

   *epoll_fd = -1;
}

int epoll_waiting(int *epoll_fd, void *events, int maxevents, int timeout)
{
   return epoll_wait(*epoll_fd, (struct epoll_event*)events, maxevents, timeout);
}

bool epoll_add(int *epoll_fd, int fd, void *device)
{
   struct epoll_event event    = {0};

   event.events             = EPOLLIN;
   event.data.ptr           = device;

   /* Shouldn't happen, but just check it. */
   if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
   {
      RARCH_ERR("Failed to add FD (%d) to epoll list (%s).\n",
            fd, strerror(errno));
      return false;
   }

   return true;
}
