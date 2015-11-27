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


#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <net/net_compat.h>

#include "msg_hash.h"

#include "remote.h"

#include "general.h"
#include "runloop.h"
#include "verbosity.h"

#define DEFAULT_NETWORK_GAMEPAD_PORT 55400
#define UDP_FRAME_PACKETS 16

struct rarch_remote
{

#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
   int net_fd[MAX_USERS];
#endif

   bool state[RARCH_BIND_LIST_END];
};

uint16_t state[MAX_USERS];

#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
static bool remote_init_network(rarch_remote_t *handle, uint16_t port, unsigned user)
{
   struct addrinfo hints = {0};
   char port_buf[16]     = {0};
   struct addrinfo *res  = NULL;
   int yes               = 1;

   port = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family   = AF_INET;
#else
   hints.ai_family   = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags    = AI_PASSIVE;


   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo_retro(NULL, port_buf, &hints, &res) < 0)
      goto error;

   handle->net_fd[user] = socket(res->ai_family,
         res->ai_socktype, res->ai_protocol);
   if (handle->net_fd[user] < 0)
      goto error;

   if (!socket_nonblock(handle->net_fd[user]))
      goto error;

   setsockopt(handle->net_fd[user], SOL_SOCKET,
         SO_REUSEADDR, (const char*)&yes, sizeof(int));
   if (bind(handle->net_fd[user], res->ai_addr, res->ai_addrlen) < 0)
   {
      RARCH_ERR("Failed to bind socket.\n");
      goto error;
   }

   freeaddrinfo_retro(res);
   return true;

error:
   if (res)
      freeaddrinfo_retro(res);
   return false;
}
#endif

rarch_remote_t *rarch_remote_new(uint16_t port)
{
   rarch_remote_t *handle = (rarch_remote_t*)calloc(1, sizeof(*handle));
   settings_t *settings = config_get_ptr();
   if (!handle)
      return NULL;

   (void)port;

#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
   for(int user = 0; user < settings->input.max_users; user ++)
   {
      handle->net_fd[user] = -1;
      if(settings->network_remote_enable_user[user])
         if (!remote_init_network(handle, port, user))
            goto error;
   }

#endif

   return handle;

#if (defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY))
error:
   rarch_remote_free(handle);
   return NULL;
#endif
}

void rarch_remote_free(rarch_remote_t *handle)
{
   settings_t *settings = config_get_ptr();
#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
   for(int user = 0; user < settings->input.max_users; user ++)
   {
      socket_close(handle->net_fd[user]);
   }

#endif

   free(handle);
}

void rarch_remote_set(rarch_remote_t *handle, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
      handle->state[id] = true;
}

bool rarch_remote_get(rarch_remote_t *handle, unsigned id)
{
   return id < RARCH_BIND_LIST_END && handle->state[id];
}

static void parse_packet(char *buffer, unsigned size, unsigned user)
{
   /* todo implement parsing of input_state from the packet */

}

void rarch_remote_poll(rarch_remote_t *handle)
{
   settings_t *settings = config_get_ptr();
   
   for(int user=0; user < settings->input.max_users; user++)
   {
      if (settings->network_remote_enable_user[user])
      {
         
         fd_set fds;
         struct timeval tmp_tv = {0};
         if (handle->net_fd[user] < 0)
            return;
         FD_ZERO(&fds);
         FD_SET(handle->net_fd[user], &fds);

         char buf[1024];
         ssize_t ret = recvfrom(handle->net_fd[user], buf,
            sizeof(buf) - 1, 0, NULL, NULL);
         if (ret > 0)
            parse_packet(buf, sizeof(buf), user);
      }
   }
}



