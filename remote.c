/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <net/net_socket.h>

#include "remote.h"

#include "msg_hash.h"
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

typedef struct input_remote_state
{
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons[MAX_USERS];
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4][MAX_USERS]; 

} input_remote_state_t;

static input_remote_state_t remote_st_ptr;
static input_remote_state_t *input_remote_get_state_ptr(void)
{
   return &remote_st_ptr;
}

#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
static bool remote_init_network(rarch_remote_t *handle,
      uint16_t port, unsigned user)
{
   int fd;
   struct addrinfo *res  = NULL;

   port = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

   fd = socket_init((void*)&res, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   handle->net_fd[user] = fd;

   if (!socket_nonblock(handle->net_fd[user]))
      goto error;

   if (!socket_bind(handle->net_fd[user], res))
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
   unsigned user;
#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
   settings_t   *settings = config_get_ptr();
#endif
   rarch_remote_t *handle = (rarch_remote_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   (void)port;

#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
   for(user = 0; user < settings->input.max_users; user ++)
   {
      handle->net_fd[user] = -1;
      if(settings->network_remote_enable_user[user])
         if (!remote_init_network(handle, port, user))
            goto error;
   }
#endif

   return handle;

#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
error:
   rarch_remote_free(handle);
   return NULL;
#endif
}

void rarch_remote_free(rarch_remote_t *handle)
{
   unsigned user;
#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
   settings_t *settings = config_get_ptr();

   for(user = 0; user < settings->input.max_users; user ++)
      socket_close(handle->net_fd[user]);
#endif

   free(handle);
}

#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
static void parse_packet(char *buffer, unsigned size, unsigned user)
{
   input_remote_state_t *ol_state  = input_remote_get_state_ptr();
   /* todo implement parsing of input_state from the packet */
   ol_state->buttons[user] = atoi(buffer);
}
#endif

void input_state_remote(int16_t *ret,
      unsigned port, unsigned device, unsigned idx,
      unsigned id)
{
   input_remote_state_t *ol_state  = input_remote_get_state_ptr();

   if (!ol_state)
      return;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (input_remote_key_pressed(id, port))
            *ret |= 1;
         break;
      case RETRO_DEVICE_ANALOG:
         {
            unsigned base = 0;

            if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
               base = 2;
            if (id == RETRO_DEVICE_ID_ANALOG_Y)
               base += 1;
            if (ol_state && ol_state->analog[base][port])
               *ret = ol_state->analog[base][port];
         }
         break;
   }
}

bool input_remote_key_pressed(int key, unsigned port)
{
   input_remote_state_t *ol_state  = input_remote_get_state_ptr();

   if (!ol_state)
      return false;

   return (ol_state->buttons[port] & (UINT64_C(1) << key));
}

void rarch_remote_poll(rarch_remote_t *handle)
{
   unsigned user;
   settings_t *settings            = config_get_ptr();
   input_remote_state_t *ol_state  = input_remote_get_state_ptr();
   
   for(user = 0; user < settings->input.max_users; user++)
   {
      if (settings->network_remote_enable_user[user])
      {
#if defined(HAVE_NETWORK_GAMEPAD) && defined(HAVE_NETPLAY)
         char buf[8];
         ssize_t ret;
         fd_set fds;
         struct timeval tmp_tv = {0};

         if (handle->net_fd[user] < 0)
            return;

         FD_ZERO(&fds);
         FD_SET(handle->net_fd[user], &fds);

         ret = recvfrom(handle->net_fd[user], buf,
               sizeof(buf) - 1, 0, NULL, NULL);

         if (ret > 0)
            parse_packet(buf, sizeof(buf), user);
         else
#endif
            ol_state->buttons[user] = 0;
      }
   }
}
