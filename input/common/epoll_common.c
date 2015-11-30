#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "epoll_common.h"

#include "../../verbosity.h"

static int g_epoll;
static bool epoll_inited;
static bool epoll_first_inited_is_joypad;

bool epoll_new(bool is_joypad)
{
   if (epoll_inited)
      return true;

   g_epoll              = epoll_create(32);
   if (g_epoll < 0)
      return false;

   epoll_first_inited_is_joypad = is_joypad;
   epoll_inited = true;

   return true;
}

void epoll_free(bool is_joypad)
{
   if (!epoll_inited || (is_joypad && !epoll_first_inited_is_joypad))
      return;

   if (g_epoll >= 0)
      close(g_epoll);
   g_epoll = -1;

   epoll_inited                 = false;
   epoll_first_inited_is_joypad = false;
}

int epoll_waiting(struct epoll_event *events, int maxevents, int timeout)
{
   return epoll_wait(g_epoll, events, maxevents, timeout);
}

bool epoll_add(int fd, void *device)
{
   struct epoll_event event    = {0};

   event.events             = EPOLLIN;
   event.data.ptr           = device;

   /* Shouldn't happen, but just check it. */
   if (epoll_ctl(g_epoll, EPOLL_CTL_ADD, fd, &event) < 0)
   {
      RARCH_ERR("Failed to add FD (%d) to epoll list (%s).\n",
            fd, strerror(errno));
      return false;
   }

   return true;
}
