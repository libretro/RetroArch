/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C)      2016 - Gregor Richards
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

#include <compat/strl.h>
#include <stdio.h>

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <retro_endianness.h>

#include "netplay_private.h"

#include "retro_assert.h"

#include "../../autosave.h"

/**
 * netplay_spectate_pre_frame:
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay (spectator version).
 **/
static bool netplay_spectate_pre_frame(netplay_t *netplay)
{
   if (netplay_is_server(netplay))
   {
      fd_set fds;
      struct timeval tmp_tv = {0};
      int new_fd, idx, i;
      struct sockaddr_storage their_addr;
      socklen_t addr_size;
      retro_ctx_serialize_info_t serial_info;
      uint32_t header[3];

      netplay->can_poll = true;
      input_poll_net();

      /* Send our input to any connected spectators */
      for (i = 0; i < MAX_SPECTATORS; i++)
      {
         if (netplay->spectate.fds[i] >= 0)
         {
            netplay->packet_buffer[2] = htonl(netplay->self_frame_count - netplay->spectate.frames[i]);
            if (!socket_send_all_blocking(netplay->spectate.fds[i], netplay->packet_buffer, sizeof(netplay->packet_buffer), false))
            {
               socket_close(netplay->spectate.fds[i]);
               netplay->spectate.fds[i] = -1;
            }
         }
      }

      /* Check for connections */
      FD_ZERO(&fds);
      FD_SET(netplay->fd, &fds);
      if (socket_select(netplay->fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
         return true;

      if (!FD_ISSET(netplay->fd, &fds))
         return true;

      addr_size = sizeof(their_addr);
      new_fd = accept(netplay->fd, (struct sockaddr*)&their_addr, &addr_size);
      if (new_fd < 0)
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR));
         return true;
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
         return true;
      }

      if (!netplay_get_nickname(netplay, new_fd))
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT));
         socket_close(new_fd);
         return true;
      }

      if (!netplay_send_nickname(netplay, new_fd))
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT));
         socket_close(new_fd);
         return true;
      }

      /* Wait until it's safe to serialize */
      if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
      {
         netplay->is_replay = true;
         netplay->replay_ptr = netplay->self_ptr;
         netplay->replay_frame_count = netplay->self_frame_count;
         netplay_wait_and_init_serialization(netplay);
         netplay->is_replay = false;
      }

      /* Start them at the current frame */
      netplay->spectate.frames[idx] = netplay->self_frame_count;
      serial_info.data_const = NULL;
      serial_info.data = netplay->buffer[netplay->self_ptr].state;
      serial_info.size = netplay->state_size;
      if (core_serialize(&serial_info))
      {
         /* Send them the savestate */
         header[0] = htonl(NETPLAY_CMD_LOAD_SAVESTATE);
         header[1] = htonl(serial_info.size + sizeof(uint32_t));
         header[2] = htonl(0);
         if (!socket_send_all_blocking(new_fd, header, sizeof(header), false))
         {
            socket_close(new_fd);
            return true;
         }

         if (!socket_send_all_blocking(new_fd, serial_info.data, serial_info.size, false))
         {
            socket_close(new_fd);
            return true;
         }
      }

      /* And send them this frame's input */
      netplay->packet_buffer[2] = htonl(0);
      if (!socket_send_all_blocking(new_fd, netplay->packet_buffer, sizeof(netplay->packet_buffer), false))
      {
         socket_close(new_fd);
         return true;
      }

      netplay->spectate.fds[idx] = new_fd;

   }
   else
   {
      if (netplay_delta_frame_ready(netplay, &netplay->buffer[netplay->self_ptr], netplay->self_frame_count))
      {
         /* Mark our own data as already read, so we ignore local input */
         netplay->buffer[netplay->self_ptr].have_local = true;
      }

      netplay->can_poll = true;
      input_poll_net();

      /* Only proceed if we have data */
      if (netplay->read_frame_count <= netplay->self_frame_count)
         return false;

   }

   return true;
}

/**
 * netplay_spectate_post_frame:
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay (spectator version).
 * Not much here, just fast forward if we're behind the server.
 **/
static void netplay_spectate_post_frame(netplay_t *netplay)
{
   netplay->self_ptr = NEXT_PTR(netplay->self_ptr);
   netplay->self_frame_count++;

   if (netplay_is_server(netplay))
   {
      /* Not expecting any client data */
      netplay->read_ptr = netplay->other_ptr = netplay->self_ptr;
      netplay->read_frame_count = netplay->other_frame_count = netplay->self_frame_count;

   }
   else
   {
      /* If we must rewind, it's because we got a save state */
      if (netplay->force_rewind)
      {
         retro_ctx_serialize_info_t serial_info;

         /* Replay frames. */
         netplay->is_replay = true;
         netplay->replay_ptr = netplay->other_ptr;
         netplay->replay_frame_count = netplay->other_frame_count;

         /* Wait until it's safe to serialize */
         if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            netplay_wait_and_init_serialization(netplay);

         serial_info.data       = NULL;
         serial_info.data_const = netplay->buffer[netplay->replay_ptr].state;
         serial_info.size       = netplay->state_size;

         core_unserialize(&serial_info);

         while (netplay->replay_frame_count < netplay->self_frame_count)
         {
            autosave_lock();
            core_run();
            autosave_unlock();
            netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
            netplay->replay_frame_count++;
         }

         netplay->is_replay = false;
         netplay->force_rewind = false;
      }

      /* We're in sync by definition */
      if (netplay->read_frame_count < netplay->self_frame_count)
      {
         netplay->other_ptr = netplay->read_ptr;
         netplay->other_frame_count = netplay->read_frame_count;
      }
      else
      {
         netplay->other_ptr = netplay->self_ptr;
         netplay->other_frame_count = netplay->self_frame_count;
      }

      /* If the server gets significantly ahead, skip to catch up */
      if (netplay->self_frame_count + netplay->stall_frames <= netplay->read_frame_count)
      {
         /* "Replay" into the future */
         netplay->is_replay = true;
         netplay->replay_ptr = netplay->self_ptr;
         netplay->replay_frame_count = netplay->self_frame_count;

         while (netplay->replay_frame_count < netplay->read_frame_count - 1)
         {
            autosave_lock();
            core_run();
            autosave_unlock();

            netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
            netplay->replay_frame_count++;
            netplay->self_ptr = netplay->replay_ptr;
            netplay->self_frame_count = netplay->replay_frame_count;
         }

         netplay->is_replay = false;
      }

   }
}

static bool netplay_spectate_info_cb(netplay_t* netplay, unsigned frames)
{
   if (netplay_is_server(netplay))
   {
      int i;
      for (i = 0; i < MAX_SPECTATORS; i++)
         netplay->spectate.fds[i] = -1;
   }
   else
   {
      if (!netplay_send_nickname(netplay, netplay->fd))
         return false;

      if (!netplay_get_nickname(netplay, netplay->fd))
         return false;
   }

   netplay->has_connection = true;

   return true;
}

struct netplay_callbacks* netplay_get_cbs_spectate(void)
{
   static struct netplay_callbacks cbs = {
      &netplay_spectate_pre_frame,
      &netplay_spectate_post_frame,
      &netplay_spectate_info_cb
   };
   return &cbs;
}
