#include <stdio.h>
#include <unistd.h>

#include "epoll_common.h"

int g_epoll;
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
