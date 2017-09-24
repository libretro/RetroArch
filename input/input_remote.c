/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <libretro.h>
#include <net/net_compat.h>
#include <net/net_socket.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "input_remote.h"

#include "../configuration.h"
#include "../msg_hash.h"
#include "../verbosity.h"

#define DEFAULT_NETWORK_GAMEPAD_PORT 55400
#define UDP_FRAME_PACKETS 16

struct remote_message
{
   int port;
   int device;
   int index;
   int id;
   uint16_t state;
};

struct input_remote
{

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
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

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
static bool input_remote_init_network(input_remote_t *handle,
      uint16_t port, unsigned user)
{
   int fd;
   struct addrinfo *res  = NULL;

   port = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

   fd = socket_init((void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   handle->net_fd[user] = fd;

   if (!socket_nonblock(handle->net_fd[user]))
      goto error;

   if (!socket_bind(handle->net_fd[user], res))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
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

input_remote_t *input_remote_new(uint16_t port, unsigned max_users)
{
   unsigned user;
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   settings_t   *settings = config_get_ptr();
#endif
   input_remote_t *handle = (input_remote_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   (void)port;

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   for(user = 0; user < max_users; user ++)
   {
      handle->net_fd[user] = -1;
      if(settings->bools.network_remote_enable_user[user])
         if (!input_remote_init_network(handle, port, user))
            goto error;
   }
#endif

   return handle;

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
error:
   input_remote_free(handle, max_users);
   return NULL;
#endif
}

void input_remote_free(input_remote_t *handle, unsigned max_users)
{
   unsigned user;
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)

   for(user = 0; user < max_users; user ++)
      socket_close(handle->net_fd[user]);
#endif

   free(handle);
}

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
static void input_remote_parse_packet(struct remote_message *msg, unsigned user)
{
   input_remote_state_t *input_state  = input_remote_get_state_ptr();

   /* Parse message */
   switch (msg->device)
   {
      case RETRO_DEVICE_JOYPAD:
         input_state->buttons[user] &= ~(1 << msg->id);
         if (msg->state)
            input_state->buttons[user] |= 1 << msg->id;
         break;
      case RETRO_DEVICE_ANALOG:
         input_state->analog[msg->index * 2 + msg->id][user] = msg->state;
         break;
   }
}
#endif

void input_remote_state(
      int16_t *ret,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (input_remote_key_pressed(id, port))
            *ret |= 1;
         break;
      case RETRO_DEVICE_ANALOG:
         {
            unsigned base = 0;
            input_remote_state_t *input_state  = input_remote_get_state_ptr();

            if (!input_state)
               return;

            if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
               base = 2;
            if (id == RETRO_DEVICE_ID_ANALOG_Y)
               base += 1;
            if (input_state && input_state->analog[base][port])
               *ret = input_state->analog[base][port];
         }
         break;
   }
}

bool input_remote_key_pressed(int key, unsigned port)
{
   input_remote_state_t *input_state  = input_remote_get_state_ptr();

   if (!input_state)
      return false;

   return (input_state->buttons[port] & (UINT64_C(1) << key));
}

void input_remote_poll(input_remote_t *handle, unsigned max_users)
{
   unsigned user;
   settings_t *settings            = config_get_ptr();
   input_remote_state_t *input_state  = input_remote_get_state_ptr();
   
   for(user = 0; user < max_users; user++)
   {
      if (settings->bools.network_remote_enable_user[user])
      {
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         struct remote_message msg;
         ssize_t ret;
         fd_set fds;

         if (handle->net_fd[user] < 0)
            return;

         FD_ZERO(&fds);
         FD_SET(handle->net_fd[user], &fds);

         ret = recvfrom(handle->net_fd[user], (char*)&msg,
               sizeof(msg), 0, NULL, NULL);

         if (ret == sizeof(msg))
            input_remote_parse_packet(&msg, user);
         else if ((ret != -1) || ((errno != EAGAIN) && (errno != ENOENT)))
#endif
         {
            input_state->buttons[user] = 0;
            input_state->analog[0][user] = 0;
            input_state->analog[1][user] = 0;
            input_state->analog[2][user] = 0;
            input_state->analog[3][user] = 0;
         }
      }
   }
}
