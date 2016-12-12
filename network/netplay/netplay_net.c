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
#include <net/net_natt.h>

#include "netplay_private.h"

#include "retro_assert.h"

#include "../../autosave.h"

#if 0
#define DEBUG_NONDETERMINISTIC_CORES
#endif

static void netplay_handle_frame_hash(netplay_t *netplay, struct delta_frame *delta)
{
   static bool crcs_valid = true;
   if (netplay_is_server(netplay))
   {
      if (netplay->check_frames &&
          (delta->frame % netplay->check_frames == 0 || delta->frame == 1))
      {
         delta->crc = netplay_delta_frame_crc(netplay, delta);
         netplay_cmd_crc(netplay, delta);
      }
   }
   else if (delta->crc && crcs_valid)
   {
      /* We have a remote CRC, so check it */
      uint32_t local_crc = netplay_delta_frame_crc(netplay, delta);
      if (local_crc != delta->crc)
      {
         if (delta->frame == 1)
         {
            /* We check frame 1 just to make sure the CRCs make sense at all.
             * If we've diverged at frame 1, we assume CRCs are not useful. */
            crcs_valid = false;
         }
         else if (crcs_valid)
         {
            /* Fix this! */
            netplay_cmd_request_savestate(netplay);
         }
      }
   }
}

/**
 * netplay_sync_pre_frame:
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay synchronization.
 **/
bool netplay_sync_pre_frame(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   if (netplay_delta_frame_ready(netplay, &netplay->buffer[netplay->self_ptr], netplay->self_frame_count))
   {
      serial_info.data_const = NULL;
      serial_info.data = netplay->buffer[netplay->self_ptr].state;
      serial_info.size = netplay->state_size;

      memset(serial_info.data, 0, serial_info.size);
      if ((netplay->quirks & NETPLAY_QUIRK_INITIALIZATION) || netplay->self_frame_count == 0)
      {
         /* Don't serialize until it's safe */
      }
      else if (!(netplay->quirks & NETPLAY_QUIRK_NO_SAVESTATES) && core_serialize(&serial_info))
      {
         if ((netplay->force_send_savestate_all || netplay->force_send_savestate_one) && !netplay->stall)
         {
            /* Send this along to the other side */
            serial_info.data_const = netplay->buffer[netplay->self_ptr].state;
            netplay_load_savestate(netplay, &serial_info, false);
            netplay->force_send_savestate_all =
                netplay->force_send_savestate_one = false;
            /* FIXME: Shouldn't send to everyone! */
         }
      }
      else
      {
         /* If the core can't serialize properly, we must stall for the
          * remote input on EVERY frame, because we can't recover */
         netplay->quirks |= NETPLAY_QUIRK_NO_SAVESTATES;
         netplay->delay_frames = 0;
      }

      /* If we can't transmit savestates, we must stall until the client is ready */
      if (netplay->self_frame_count > 0 &&
          (netplay->quirks & (NETPLAY_QUIRK_NO_SAVESTATES|NETPLAY_QUIRK_NO_TRANSMISSION)) &&
          (netplay->connections_size == 0 || !netplay->connections[0].active ||
           netplay->connections[0].mode < NETPLAY_CONNECTION_CONNECTED))
         netplay->stall = NETPLAY_STALL_NO_CONNECTION;
   }

   if (netplay->is_server)
   {
      fd_set fds;
      struct timeval tmp_tv = {0};
      int new_fd;
      struct sockaddr_storage their_addr;
      socklen_t addr_size;
      struct netplay_connection *connection;
      size_t connection_num;

      /* Check for a connection */
      FD_ZERO(&fds);
      FD_SET(netplay->listen_fd, &fds);
      if (socket_select(netplay->listen_fd + 1, &fds, NULL, NULL, &tmp_tv) > 0 &&
          FD_ISSET(netplay->listen_fd, &fds))
      {
         addr_size = sizeof(their_addr);
         new_fd = accept(netplay->listen_fd, (struct sockaddr*)&their_addr, &addr_size);
         if (new_fd < 0)
         {
            RARCH_ERR("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));
            goto process;
         }

         /* Set the socket nonblocking */
         if (!socket_nonblock(new_fd))
         {
            /* Catastrophe! */
            socket_close(new_fd);
            goto process;
         }

#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
         {
            int flag = 1;
            if (setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
               (const char*)
#else
               (const void*)
#endif
               &flag,
               sizeof(int)) < 0)
               RARCH_WARN("Could not set netplay TCP socket to nodelay. Expect jitter.\n");
         }
#endif

#if defined(F_SETFD) && defined(FD_CLOEXEC)
         /* Don't let any inherited processes keep open our port */
         if (fcntl(new_fd, F_SETFD, FD_CLOEXEC) < 0)
            RARCH_WARN("Cannot set Netplay port to close-on-exec. It may fail to reopen if the client disconnects.\n");
#endif

         /* Allocate a connection */
         for (connection_num = 0; connection_num < netplay->connections_size; connection_num++)
            if (!netplay->connections[connection_num].active) break;
         if (connection_num == netplay->connections_size)
         {
            if (connection_num == 0)
            {
               netplay->connections = malloc(sizeof(struct netplay_connection));
               if (netplay->connections == NULL)
               {
                  socket_close(new_fd);
                  goto process;
               }
               netplay->connections_size = 1;

            }
            else
            {
               size_t new_connections_size = netplay->connections_size * 2;
               struct netplay_connection *new_connections =
                  realloc(netplay->connections,
                     new_connections_size*sizeof(struct netplay_connection));
               if (new_connections == NULL)
               {
                  socket_close(new_fd);
                  goto process;
               }

               memset(new_connections + netplay->connections_size, 0,
                  netplay->connections_size * sizeof(struct netplay_connection));
               netplay->connections = new_connections;
               netplay->connections_size = new_connections_size;

            }
         }
         connection = &netplay->connections[connection_num];

         /* Set it up */
         memset(connection, 0, sizeof(*connection));
         connection->active = true;
         connection->fd = new_fd;
         connection->mode = NETPLAY_CONNECTION_INIT;

         if (!netplay_init_socket_buffer(&connection->send_packet_buffer,
               netplay->packet_buffer_size) ||
             !netplay_init_socket_buffer(&connection->recv_packet_buffer,
               netplay->packet_buffer_size))
         {
            if (connection->send_packet_buffer.data)
               netplay_deinit_socket_buffer(&connection->send_packet_buffer);
            connection->active = false;
            socket_close(new_fd);
            goto process;
         }

         netplay_handshake_init_send(netplay, connection);

      }
   }

process:
   netplay->can_poll = true;
   input_poll_net();

   return (netplay->stall != NETPLAY_STALL_NO_CONNECTION);
}

/**
 * netplay_sync_post_frame:
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay synchronization.
 * We check if we have new input and replay from recorded input.
 **/
void netplay_sync_post_frame(netplay_t *netplay)
{
   netplay->self_ptr = NEXT_PTR(netplay->self_ptr);
   netplay->self_frame_count++;

   /* Only relevant if we're connected */
   if (!netplay->connected_players)
   {
      netplay->other_frame_count = netplay->self_frame_count;
      netplay->other_ptr = netplay->self_ptr;
      return;
   }

#ifndef DEBUG_NONDETERMINISTIC_CORES
   if (!netplay->force_rewind)
   {
      /* Skip ahead if we predicted correctly.
       * Skip until our simulation failed. */
      while (netplay->other_frame_count < netplay->unread_frame_count &&
             netplay->other_frame_count < netplay->self_frame_count)
      {
         struct delta_frame *ptr = &netplay->buffer[netplay->other_ptr];
         size_t i;

         for (i = 0; i < MAX_USERS; i++)
         {
            if (memcmp(ptr->simulated_input_state[i], ptr->real_input_state[i],
                     sizeof(ptr->real_input_state[i])) != 0
                  && !ptr->used_real[i])
               break;
         }
         if (i != MAX_USERS) break;
         netplay_handle_frame_hash(netplay, ptr);
         netplay->other_ptr = NEXT_PTR(netplay->other_ptr);
         netplay->other_frame_count++;
      }
   }
#endif

   /* Now replay the real input if we've gotten ahead of it */
   if (netplay->force_rewind ||
       (netplay->other_frame_count < netplay->unread_frame_count &&
        netplay->other_frame_count < netplay->self_frame_count))
   {
      retro_ctx_serialize_info_t serial_info;

      /* Replay frames. */
      netplay->is_replay = true;
      netplay->replay_ptr = netplay->other_ptr;
      netplay->replay_frame_count = netplay->other_frame_count;

      if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
         /* Make sure we're initialized before we start loading things */
         netplay_wait_and_init_serialization(netplay);

      serial_info.data       = NULL;
      serial_info.data_const = netplay->buffer[netplay->replay_ptr].state;
      serial_info.size       = netplay->state_size;

      if (!core_unserialize(&serial_info))
      {
         RARCH_ERR("Netplay savestate loading failed: Prepare for desync!\n");
      }

      while (netplay->replay_frame_count < netplay->self_frame_count)
      {
         struct delta_frame *ptr = &netplay->buffer[netplay->replay_ptr];
         serial_info.data       = ptr->state;
         serial_info.size       = netplay->state_size;
         serial_info.data_const = NULL;

         /* Remember the current state */
         memset(serial_info.data, 0, serial_info.size);
         core_serialize(&serial_info);
         if (netplay->replay_frame_count < netplay->unread_frame_count)
            netplay_handle_frame_hash(netplay, ptr);

         /* Simulate this frame's input */
         if (netplay->replay_frame_count >= netplay->unread_frame_count)
            netplay_simulate_input(netplay, netplay->replay_ptr, true);

         autosave_lock();
         core_run();
         autosave_unlock();
         netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
         netplay->replay_frame_count++;

#ifdef DEBUG_NONDETERMINISTIC_CORES
         if (ptr->have_remote && netplay_delta_frame_ready(netplay, &netplay->buffer[netplay->replay_ptr], netplay->replay_frame_count))
         {
            RARCH_LOG("PRE  %u: %X\n", netplay->replay_frame_count-1, netplay_delta_frame_crc(netplay, ptr));
            if (netplay->is_server)
               RARCH_LOG("INP  %X %X\n", ptr->real_input_state[0], ptr->self_state[0]);
            else
               RARCH_LOG("INP  %X %X\n", ptr->self_state[0], ptr->real_input_state[0]);
            ptr = &netplay->buffer[netplay->replay_ptr];
            serial_info.data = ptr->state;
            memset(serial_info.data, 0, serial_info.size);
            core_serialize(&serial_info);
            RARCH_LOG("POST %u: %X\n", netplay->replay_frame_count-1, netplay_delta_frame_crc(netplay, ptr));
         }
#endif
      }

      if (netplay->unread_frame_count < netplay->self_frame_count)
      {
         netplay->other_ptr = netplay->unread_ptr;
         netplay->other_frame_count = netplay->unread_frame_count;
      }
      else
      {
         netplay->other_ptr = netplay->self_ptr;
         netplay->other_frame_count = netplay->self_frame_count;
      }
      netplay->is_replay = false;
      netplay->force_rewind = false;
   }

   /* If we're supposed to stall, rewind (we shouldn't get this far if we're
    * stalled, so this is a last resort) */
   if (netplay->stall)
   {
      retro_ctx_serialize_info_t serial_info;

      netplay->self_ptr = PREV_PTR(netplay->self_ptr);
      netplay->self_frame_count--;

      serial_info.data       = NULL;
      serial_info.data_const = netplay->buffer[netplay->self_ptr].state;
      serial_info.size       = netplay->state_size;

      core_unserialize(&serial_info);
   }
}
