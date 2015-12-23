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

#include "netplay_private.h"

/**
 * netplay_pre_frame_spectate:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay (spectate mode version).
 **/
static void pre_frame(netplay_t *netplay)
{
   unsigned i;
   uint32_t *header;
   int new_fd, idx, bufsize;
   size_t header_size;
   struct sockaddr_storage their_addr;
   socklen_t addr_size;
   fd_set fds;
   struct timeval tmp_tv = {0};

   if (!np_is_server(netplay))
      return;

   FD_ZERO(&fds);
   FD_SET(netplay->fd, &fds);

   if (socket_select(netplay->fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   if (!FD_ISSET(netplay->fd, &fds))
      return;

   addr_size = sizeof(their_addr);
   new_fd = accept(netplay->fd, (struct sockaddr*)&their_addr, &addr_size);
   if (new_fd < 0)
   {
      RARCH_ERR("Failed to accept incoming spectator.\n");
      return;
   }

   idx = -1;
   for (i = 0; i < MAX_SPECTATORS; i++)
   {
      if (netplay->spectate.fds[i] == -1)
      {
         idx = i;
         break;
      }
   }

   /* No vacant client streams :( */
   if (idx == -1)
   {
      socket_close(new_fd);
      return;
   }

   if (!np_get_nickname(netplay, new_fd))
   {
      RARCH_ERR("Failed to get nickname from client.\n");
      socket_close(new_fd);
      return;
   }

   if (!np_send_nickname(netplay, new_fd))
   {
      RARCH_ERR("Failed to send nickname to client.\n");
      socket_close(new_fd);
      return;
   }

   header = np_bsv_header_generate(&header_size,
         np_impl_magic());

   if (!header)
   {
      RARCH_ERR("Failed to generate BSV header.\n");
      socket_close(new_fd);
      return;
   }

   bufsize = header_size;
   setsockopt(new_fd, SOL_SOCKET, SO_SNDBUF, (const char*)&bufsize,
         sizeof(int));

   if (!socket_send_all_blocking(new_fd, header, header_size))
   {
      RARCH_ERR("Failed to send header to client.\n");
      socket_close(new_fd);
      free(header);
      return;
   }

   free(header);
   netplay->spectate.fds[idx] = new_fd;

#ifndef HAVE_SOCKET_LEGACY
   np_log_connection(&their_addr, idx, netplay->other_nick);
#endif
}

/**
 * netplay_post_frame_spectate:   
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay (spectate mode version).
 * We check if we have new input and replay from recorded input.
 **/
static void post_frame(netplay_t *netplay)
{
   unsigned i;

   if (!np_is_server(netplay))
      return;

   for (i = 0; i < MAX_SPECTATORS; i++)
   {
      char msg[128];

      if (netplay->spectate.fds[i] == -1)
         continue;

      if (socket_send_all_blocking(netplay->spectate.fds[i],
               netplay->spectate.input,
               netplay->spectate.input_ptr * sizeof(int16_t)))
         continue;

      RARCH_LOG("Client (#%u) disconnected ...\n", i);

      snprintf(msg, sizeof(msg), "Client (#%u) disconnected.", i);
      runloop_msg_queue_push(msg, 1, 180, false);

      socket_close(netplay->spectate.fds[i]);
      netplay->spectate.fds[i] = -1;
      break;
   }

   netplay->spectate.input_ptr = 0;
}

static bool get_info(netplay_t *netplay)
{
   size_t save_state_size, size;
   void *buf          = NULL;
   uint32_t header[4] = {0};
   char msg[512]      = {0};
   bool ret           = true;

   if (!np_send_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to send nickname to host.\n");
      return false;
   }

   if (!np_get_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to receive nickname from host.\n");
      return false;
   }

   snprintf(msg, sizeof(msg), "Connected to \"%s\"", netplay->other_nick);
   runloop_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);


   if (!socket_receive_all_blocking(netplay->fd, header, sizeof(header)))
   {
      RARCH_ERR("Cannot get header from host.\n");
      return false;
   }

   save_state_size = core.retro_serialize_size();
   if (!np_bsv_parse_header(header, np_impl_magic()))
   {
      RARCH_ERR("Received invalid BSV header from host.\n");
      return false;
   }

   buf = malloc(save_state_size);
   if (!buf)
      return false;

   size = save_state_size;

   if (!socket_receive_all_blocking(netplay->fd, buf, size))
   {
      RARCH_ERR("Failed to receive save state from host.\n");
      free(buf);
      return false;
   }

   if (save_state_size)
      ret = core.retro_unserialize(buf, save_state_size);

   free(buf);
   return ret;
}

static bool info_cb(netplay_t *netplay, unsigned frames)
{
   unsigned i;
   if(np_is_server(netplay))
   {
      if(!np_get_info(netplay))
         return false;
   }

   for (i = 0; i < MAX_SPECTATORS; i++)
      netplay->spectate.fds[i] = -1;
   return true;
}

struct netplay_callbacks* netplay_get_cbs_spectate(void)
{
   static struct netplay_callbacks cbs = {
      &pre_frame,
      &post_frame,
      &info_cb
   };

   return &cbs;
}