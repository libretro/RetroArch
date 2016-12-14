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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include <boolean.h>
#include <compat/strl.h>

#include "netplay_private.h"

#include "../../runloop.h"

/**
 * netplay_hangup:
 *
 * Disconnects an active Netplay connection due to an error
 **/
void netplay_hangup(netplay_t *netplay, struct netplay_connection *connection)
{
   if (!netplay)
      return;
   if (!connection->active)
      return;

   RARCH_WARN("Netplay has disconnected. Will continue without connection ...\n");
   runloop_msg_queue_push("Netplay has disconnected. Will continue without connection.", 0, 480, false);

   socket_close(connection->fd);
   connection->active = false;
   netplay_deinit_socket_buffer(&connection->send_packet_buffer);
   netplay_deinit_socket_buffer(&connection->recv_packet_buffer);

   if (!netplay->is_server)
   {
      netplay->self_mode = NETPLAY_CONNECTION_NONE;
      netplay->connected_players = 0;

   }
   else
   {
      /* Remove this player */
      if (connection->mode == NETPLAY_CONNECTION_PLAYING)
      {
         netplay->connected_players &= ~(1<<connection->player);

         /* FIXME: Duplication */
         if (netplay->is_server)
         {
            uint32_t payload[2];
            payload[0] = htonl(netplay->read_frame_count[connection->player]);
            payload[1] = htonl(connection->player);
            netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));
         }
      }

   }
}

/* Send the specified input data */
static bool send_input_frame(netplay_t *netplay,
   struct netplay_connection *only, struct netplay_connection *except,
   uint32_t frame, uint32_t player, uint32_t *state)
{
   uint32_t buffer[2 + WORDS_PER_FRAME];
   size_t i;

   buffer[0] = htonl(NETPLAY_CMD_INPUT);
   buffer[1] = htonl(WORDS_PER_FRAME * sizeof(uint32_t));
   buffer[2] = htonl(frame);
   buffer[3] = htonl(player);
   buffer[4] = htonl(state[0]);
   buffer[5] = htonl(state[1]);
   buffer[6] = htonl(state[2]);

   if (only)
   {
      if (!netplay_send(&only->send_packet_buffer, only->fd, buffer, sizeof(buffer)))
      {
         netplay_hangup(netplay, only);
         return false;
      }
   }
   else
   {
      for (i = 0; i < netplay->connections_size; i++)
      {
         struct netplay_connection *connection = &netplay->connections[i];
         if (connection == except) continue;
         if (connection->active &&
             connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
             (connection->mode != NETPLAY_CONNECTION_PLAYING ||
              connection->player != player))
         {
            if (!netplay_send(&connection->send_packet_buffer, connection->fd,
                  buffer, sizeof(buffer)))
               netplay_hangup(netplay, connection);
         }
      }
   }

   return true;
}

/* Send the current input frame */
bool netplay_send_cur_input(netplay_t *netplay,
   struct netplay_connection *connection)
{
   struct delta_frame *dframe = &netplay->buffer[netplay->self_ptr];
   uint32_t player;

   if (netplay->is_server)
   {
      /* Send the other players' input data */
      for (player = 0; player < MAX_USERS; player++)
      {
         if (connection->mode == NETPLAY_CONNECTION_PLAYING &&
               connection->player == player)
            continue;
         if ((netplay->connected_players & (1<<player)))
         {
            if (dframe->have_real[player])
            {
               if (!send_input_frame(netplay, connection, NULL,
                        netplay->self_frame_count, player,
                        dframe->real_input_state[player]))
                  return false;
            }
         }
      }

      /* If we're not playing, send a NOINPUT */
      if (netplay->self_mode != NETPLAY_CONNECTION_PLAYING)
      {
         uint32_t payload = htonl(netplay->self_frame_count);
         if (!netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_NOINPUT,
               &payload, sizeof(payload)))
            return false;
      }

   }

   /* Send our own data */
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
   {
      if (!send_input_frame(netplay, connection, NULL,
            netplay->self_frame_count,
            (netplay->is_server ?  NETPLAY_CMD_INPUT_BIT_SERVER : 0) | netplay->self_player,
            dframe->self_state))
         return false;
   }

   if (!netplay_send_flush(&connection->send_packet_buffer, connection->fd,
         false))
      return false;

   return true;
}

/**
 * netplay_send_raw_cmd
 *
 * Send a raw Netplay command to the given connection
 *
 * Returns true on success, false on failure
 */
bool netplay_send_raw_cmd(netplay_t *netplay,
   struct netplay_connection *connection, uint32_t cmd, const void *data,
   size_t size)
{
   uint32_t cmdbuf[2];

   cmdbuf[0] = htonl(cmd);
   cmdbuf[1] = htonl(size);

   if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmdbuf,
         sizeof(cmdbuf)))
      return false;

   if (size > 0)
      if (!netplay_send(&connection->send_packet_buffer, connection->fd, data, size))
         return false;

   return true;
}

/**
 * netplay_send_raw_cmd_all
 *
 * Send a raw Netplay command to all connections, optionally excluding one
 * (typically the client that the relevant command came from)
 */
void netplay_send_raw_cmd_all(netplay_t *netplay,
   struct netplay_connection *except, uint32_t cmd, const void *data,
   size_t size)
{
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection == except)
         continue;
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
      {
         if (!netplay_send_raw_cmd(netplay, connection, cmd, data, size))
            netplay_hangup(netplay, connection);
      }
   }
}

static bool netplay_cmd_nak(netplay_t *netplay,
   struct netplay_connection *connection)
{
   netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_NAK, NULL, 0);
   return false;
}

bool netplay_cmd_crc(netplay_t *netplay, struct delta_frame *delta)
{
   uint32_t payload[2];
   bool success = true;
   size_t i;
   payload[0] = htonl(delta->frame);
   payload[1] = htonl(delta->crc);
   for (i = 0; i < netplay->connections_size; i++)
   {
      if (netplay->connections[i].active &&
            netplay->connections[i].mode >= NETPLAY_CONNECTION_CONNECTED)
         success = netplay_send_raw_cmd(netplay, &netplay->connections[i],
            NETPLAY_CMD_CRC, payload, sizeof(payload)) && success;
   }
   return success;
}

bool netplay_cmd_request_savestate(netplay_t *netplay)
{
   if (netplay->connections_size == 0 ||
       !netplay->connections[0].active ||
       netplay->connections[0].mode < NETPLAY_CONNECTION_CONNECTED)
      return false;
   if (netplay->savestate_request_outstanding)
      return true;
   netplay->savestate_request_outstanding = true;
   return netplay_send_raw_cmd(netplay, &netplay->connections[0],
      NETPLAY_CMD_REQUEST_SAVESTATE, NULL, 0);
}

bool netplay_cmd_mode(netplay_t *netplay,
   struct netplay_connection *connection,
   enum rarch_netplay_connection_mode mode)
{
   uint32_t cmd;
   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         cmd = NETPLAY_CMD_SPECTATE;
         break;

      case NETPLAY_CONNECTION_PLAYING:
         cmd = NETPLAY_CMD_PLAY;
         break;

      default:
         return false;
   }
   return netplay_send_raw_cmd(netplay, connection, cmd, NULL, 0);
}

static bool netplay_get_cmd(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   uint32_t cmd;
   uint32_t flip_frame;
   uint32_t cmd_size;
   ssize_t recvd;
   char msg[512];

   /* We don't handle the initial handshake here */
   switch (connection->mode)
   {
      case NETPLAY_CONNECTION_NONE:
         /* Huh?! */
         return false;
      case NETPLAY_CONNECTION_INIT:
         return netplay_handshake_init(netplay, connection, had_input);
      case NETPLAY_CONNECTION_PRE_NICK:
      {
         bool ret = netplay_handshake_pre_nick(netplay, connection, had_input);
         if (connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
             !netplay_send_cur_input(netplay, connection))
            return false;
         return ret;
      }
      case NETPLAY_CONNECTION_PRE_PASSWORD:
      {
         bool ret = netplay_handshake_pre_password(netplay, connection, had_input);
         if (connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
             !netplay_send_cur_input(netplay, connection))
            return false;
         return ret;
      }
      case NETPLAY_CONNECTION_PRE_SYNC:
      {
         bool ret = netplay_handshake_pre_sync(netplay, connection, had_input);
         if (connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
             !netplay_send_cur_input(netplay, connection))
            return false;
         return ret;
      }
      default:
         break;
   }

   /* FIXME: This depends on delta_frame_ready */

#define RECV(buf, sz) \
   recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), \
      (sz), false); \
   if (recvd >= 0 && recvd < (sz)) goto shrt; \
   else if (recvd < 0)

   RECV(&cmd, sizeof(cmd))
      return false;

   cmd      = ntohl(cmd);

   RECV(&cmd_size, sizeof(cmd_size))
      return false;

   cmd_size = ntohl(cmd_size);

   netplay->timeout_cnt = 0;

   switch (cmd)
   {
      case NETPLAY_CMD_ACK:
         /* Why are we even bothering? */
         break;

      case NETPLAY_CMD_NAK:
         /* Disconnect now! */
         return false;

      case NETPLAY_CMD_INPUT:
         {
            uint32_t buffer[WORDS_PER_FRAME];
            uint32_t player;
            unsigned i;
            struct delta_frame *dframe;

            if (cmd_size != WORDS_PER_FRAME * sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_INPUT received an unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(buffer, sizeof(buffer))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_INPUT input.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            for (i = 0; i < WORDS_PER_FRAME; i++)
               buffer[i] = ntohl(buffer[i]);

            if (netplay->is_server)
            {
               /* Ignore the claimed player #, must be this client */
               if (connection->mode != NETPLAY_CONNECTION_PLAYING)
                  return netplay_cmd_nak(netplay, connection);
               player = connection->player;
            }
            else
            {
               player = buffer[1] & ~NETPLAY_CMD_INPUT_BIT_SERVER;
            }

            if (player >= MAX_USERS || !(netplay->connected_players & (1<<player)))
               return netplay_cmd_nak(netplay, connection);

            if (buffer[0] < netplay->read_frame_count[player])
            {
               /* We already had this, so ignore the new transmission */
               break;
            }
            else if (buffer[0] > netplay->read_frame_count[player])
            {
               /* Out of order = out of luck */
               return netplay_cmd_nak(netplay, connection);
            }

            /* The data's good! */
            dframe = &netplay->buffer[netplay->read_ptr[player]];
            if (!netplay_delta_frame_ready(netplay, dframe, netplay->read_frame_count[player]))
            {
               /* FIXME: Catastrophe! */
               return netplay_cmd_nak(netplay, connection);
            }
            memcpy(dframe->real_input_state[player], buffer + 2,
               WORDS_PER_INPUT*sizeof(uint32_t));
            dframe->have_real[player] = true;
            netplay->read_ptr[player] = NEXT_PTR(netplay->read_ptr[player]);
            netplay->read_frame_count[player]++;

            if (netplay->is_server)
            {
               /* Forward it on if it's past data*/
               if (dframe->frame <= netplay->self_frame_count)
                  send_input_frame(netplay, NULL, connection, buffer[0],
                     player, dframe->real_input_state[player]);
            }

            /* If this was server data, advance our server pointer too */
            if (!netplay->is_server && (buffer[1] & NETPLAY_CMD_INPUT_BIT_SERVER))
            {
               netplay->server_ptr = netplay->read_ptr[player];
               netplay->server_frame_count = netplay->read_frame_count[player];
            }
            break;
         }

      case NETPLAY_CMD_NOINPUT:
         {
            uint32_t frame;

            if (netplay->is_server)
               return netplay_cmd_nak(netplay, connection);

            RECV(&frame, sizeof(frame))
               return netplay_cmd_nak(netplay, connection);
            frame = ntohl(frame);

            if (frame != netplay->server_frame_count)
               return netplay_cmd_nak(netplay, connection);

            netplay->server_ptr = NEXT_PTR(netplay->server_ptr);
            netplay->server_frame_count++;
            break;
         }

      case NETPLAY_CMD_FLIP_PLAYERS:
         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("CMD_FLIP_PLAYERS received an unexpected command size.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         RECV(&flip_frame, sizeof(flip_frame))
         {
            RARCH_ERR("Failed to receive CMD_FLIP_PLAYERS argument.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (netplay->is_server)
            return netplay_cmd_nak(netplay, connection);

         flip_frame = ntohl(flip_frame);

         if (flip_frame < netplay->server_frame_count)
         {
            RARCH_ERR("Host asked us to flip users in the past. Not possible ...\n");
            return netplay_cmd_nak(netplay, connection);
         }

         netplay->flip ^= true;
         netplay->flip_frame = flip_frame;

         /* Force a rewind to assure the flip happens: This just prevents us
          * from skipping other past the flip because our prediction was
          * correct */
         if (flip_frame < netplay->self_frame_count)
            netplay->force_rewind = true;

         RARCH_LOG("%s.\n", msg_hash_to_str(MSG_NETPLAY_USERS_HAS_FLIPPED));
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_NETPLAY_USERS_HAS_FLIPPED), 1, 180, false);

         break;

      case NETPLAY_CMD_SPECTATE:
      {
         uint32_t payload[2];

         if (!netplay->is_server)
            return netplay_cmd_nak(netplay, connection);

         if (connection->mode == NETPLAY_CONNECTION_PLAYING)
         {
            /* The frame we haven't received is their end frame */
            payload[0] = htonl(netplay->read_frame_count[connection->player]);

            /* Mark them as not playing anymore */
            connection->mode = NETPLAY_CONNECTION_SPECTATING;
            netplay->connected_players &= ~(1<<connection->player);

            /* Tell everyone */
            payload[1] = htonl(connection->player);
            netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

            /* Announce it */
            msg[sizeof(msg)-1] = '\0';
            snprintf(msg, sizeof(msg)-1, "Player %d has left", connection->player+1);
            RARCH_LOG("%s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, false);
         }
         else
         {
            payload[0] = htonl(0);
         }

         /* Tell the player even if they were confused */
         payload[1] = htonl(NETPLAY_CMD_MODE_BIT_YOU | connection->player);
         netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));
         break;
      }

      case NETPLAY_CMD_PLAY:
      {
         uint32_t payload[2];
         uint32_t player = 0;
         payload[0] = htonl(netplay->self_frame_count + 1);

         if (!netplay->is_server)
            return netplay_cmd_nak(netplay, connection);

         /* Find an available player slot */
         for (player = 0; player < MAX_USERS; player++)
         {
            if (!(netplay->self_mode == NETPLAY_CONNECTION_PLAYING &&
                  netplay->self_player == player) &&
                !(netplay->connected_players & (1<<player)))
               break;
         }
         if (player == MAX_USERS)
         {
            /* FIXME */
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->mode != NETPLAY_CONNECTION_PLAYING)
         {
            /* Mark them as playing */
            connection->mode = NETPLAY_CONNECTION_PLAYING;
            connection->player = player;
            netplay->connected_players |= 1<<player;

            /* Tell everyone */
            payload[1] = htonl(NETPLAY_CMD_MODE_BIT_PLAYING | connection->player);
            netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

            /* Announce it */
            msg[sizeof(msg)-1] = '\0';
            snprintf(msg, sizeof(msg)-1, "Player %d has joined", player+1);
            RARCH_LOG("%s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, false);

         }

         /* Tell the player even if they were confused */
         payload[1] = htonl(NETPLAY_CMD_MODE_BIT_PLAYING |
            NETPLAY_CMD_MODE_BIT_YOU | connection->player);
         netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

         /* And expect their data */
         netplay->read_ptr[player] = NEXT_PTR(netplay->self_ptr);
         netplay->read_frame_count[player] = netplay->self_frame_count + 1;
         break;
      }

      case NETPLAY_CMD_MODE:
      {
         uint32_t payload[2];
         uint32_t frame, mode, player;
         size_t ptr;
         struct delta_frame *dframe;

#define START(which) \
         do { \
            ptr = which; \
            dframe = &netplay->buffer[ptr]; \
         } while(0)
#define NEXT() \
         do { \
            ptr = NEXT_PTR(ptr); \
            dframe = &netplay->buffer[ptr]; \
         } while(0)

         if (cmd_size != sizeof(payload) ||
             netplay->is_server)
            return netplay_cmd_nak(netplay, connection);

         RECV(payload, sizeof(payload))
         {
            RARCH_ERR("NETPLAY_CMD_MODE failed to receive payload.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (netplay->is_server)
            return netplay_cmd_nak(netplay, connection);

         frame = ntohl(payload[0]);

         /* We're changing past input, so must replay it */
         if (frame < netplay->self_frame_count)
            netplay->force_rewind = true;

         mode = ntohl(payload[1]);
         player = mode & 0xFFFF;
         if (player >= MAX_USERS)
            return netplay_cmd_nak(netplay, connection);

         if (mode & NETPLAY_CMD_MODE_BIT_YOU)
         {
            /* A change to me! */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               if (frame != netplay->server_frame_count)
                  return netplay_cmd_nak(netplay, connection);

               /* Hooray, I get to play now! */
               if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
                  return netplay_cmd_nak(netplay, connection);

               netplay->self_mode = NETPLAY_CONNECTION_PLAYING;
               netplay->self_player = player;

               /* Fix up current frame info */
               if (frame <= netplay->self_frame_count)
               {
                  /* It wanted past frames, better send 'em! */
                  START(netplay->server_ptr);
                  while (dframe->used && dframe->frame <= netplay->self_frame_count)
                  {
                     memcpy(dframe->real_input_state[player], dframe->self_state, sizeof(dframe->self_state));
                     dframe->have_real[player] = true;
                     send_input_frame(netplay, connection, NULL, dframe->frame, player, dframe->self_state);
                     if (dframe->frame == netplay->self_frame_count) break;
                     NEXT();
                  }

               }
               else
               {
                  /* It wants future frames, make sure we don't capture or send intermediate ones */
                  START(netplay->self_ptr);
                  while (dframe->used && dframe->frame < frame)
                  {
                     memset(dframe->self_state, 0, sizeof(dframe->self_state));
                     memset(dframe->real_input_state[player], 0, sizeof(dframe->self_state));
                     dframe->have_local = true;
                     NEXT();
                  }

               }

               /* Announce it */
               msg[sizeof(msg)-1] = '\0';
               snprintf(msg, sizeof(msg)-1, "You have joined as player %d", player+1);
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);

            }
            else /* YOU && !PLAYING */
            {
               /* I'm no longer playing, but I should already know this */
               if (netplay->self_mode != NETPLAY_CONNECTION_SPECTATING)
                  return netplay_cmd_nak(netplay, connection);

               /* Announce it */
               strlcpy(msg, "You have left the game", sizeof(msg));
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);

            }

         }
         else /* !YOU */
         {
            /* Somebody else is joining or parting */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               if (frame != netplay->server_frame_count)
                  return netplay_cmd_nak(netplay, connection);

               netplay->connected_players |= (1<<player);

               netplay->read_ptr[player] = netplay->server_ptr;
               netplay->read_frame_count[player] = netplay->server_frame_count;

               /* Announce it */
               msg[sizeof(msg)-1] = '\0';
               snprintf(msg, sizeof(msg)-1, "Player %d has joined", player+1);
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);

            }
            else
            {
               netplay->connected_players &= ~(1<<player);

               /* Announce it */
               msg[sizeof(msg)-1] = '\0';
               snprintf(msg, sizeof(msg)-1, "Player %d has left", player+1);
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);
            }

         }

         break;

#undef START
#undef NEXT
      }

      case NETPLAY_CMD_DISCONNECT:
         netplay_hangup(netplay, connection);
         return true;

      case NETPLAY_CMD_CRC:
         {
            uint32_t buffer[2];
            size_t tmp_ptr = netplay->self_ptr;
            bool found = false;

            if (cmd_size != sizeof(buffer))
            {
               RARCH_ERR("NETPLAY_CMD_CRC received unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(buffer, sizeof(buffer))
            {
               RARCH_ERR("NETPLAY_CMD_CRC failed to receive payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            buffer[0] = ntohl(buffer[0]);
            buffer[1] = ntohl(buffer[1]);

            /* Received a CRC for some frame. If we still have it, check if it
             * matched. This approach could be improved with some quick modular
             * arithmetic. */
            do
            {
               if (     netplay->buffer[tmp_ptr].used 
                     && netplay->buffer[tmp_ptr].frame == buffer[0])
               {
                  found = true;
                  break;
               }

               tmp_ptr = PREV_PTR(tmp_ptr);
            } while (tmp_ptr != netplay->self_ptr);

            if (!found)
            {
               /* Oh well, we got rid of it! */
               break;
            }

            if (buffer[0] <= netplay->other_frame_count)
            {
               /* We've already replayed up to this frame, so we can check it
                * directly */
               uint32_t local_crc = netplay_delta_frame_crc(
                     netplay, &netplay->buffer[tmp_ptr]);

               if (buffer[1] != local_crc)
               {
                  /* Problem! */
                  netplay_cmd_request_savestate(netplay);
               }
            }
            else
            {
               /* We'll have to check it when we catch up */
               netplay->buffer[tmp_ptr].crc = buffer[1];
            }

            break;
         }

      case NETPLAY_CMD_REQUEST_SAVESTATE:
         /* Delay until next frame so we don't send the savestate after the
          * input */
         netplay->force_send_savestate = true;
         break;

      case NETPLAY_CMD_LOAD_SAVESTATE:
         {
            uint32_t frame;
            uint32_t isize;
            uint32_t rd, wn;
            uint32_t player;

            /* Make sure we're ready for it */
            if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            {
               if (!netplay->is_replay)
               {
                  netplay->is_replay = true;
                  netplay->replay_ptr = netplay->self_ptr;
                  netplay->replay_frame_count = netplay->self_frame_count;
                  netplay_wait_and_init_serialization(netplay);
                  netplay->is_replay = false;
               }
               else
               {
                  netplay_wait_and_init_serialization(netplay);
               }
            }

            /* Only players may load states */
            if (connection->mode != NETPLAY_CONNECTION_PLAYING)
               return netplay_cmd_nak(netplay, connection);

            /* There is a subtlty in whether the load comes before or after the
             * current frame:
             *
             * If it comes before the current frame, then we need to force a
             * rewind to that point.
             *
             * If it comes after the current frame, we need to jump ahead, then
             * (strangely) force a rewind to the frame we're already on, so it
             * gets loaded. This is just to avoid having reloading implemented in
             * too many places. */
            if (cmd_size > netplay->zbuffer_size + 2*sizeof(uint32_t))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE received an unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frame, sizeof(frame))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive savestate frame.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            frame = ntohl(frame);

            if (frame != netplay->read_frame_count[connection->player])
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE loading a state out of order!\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&isize, sizeof(isize))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive inflated size.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            isize = ntohl(isize);

            if (isize != netplay->state_size)
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE received an unexpected save state size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(netplay->zbuffer, cmd_size - 2*sizeof(uint32_t))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive savestate.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* And decompress it */
            netplay->decompression_backend->set_in(netplay->decompression_stream,
               netplay->zbuffer, cmd_size - 2*sizeof(uint32_t));
            netplay->decompression_backend->set_out(netplay->decompression_stream,
               (uint8_t*)netplay->buffer[netplay->read_ptr[connection->player]].state,
               netplay->state_size);
            netplay->decompression_backend->trans(netplay->decompression_stream,
               true, &rd, &wn, NULL);

            /* Skip ahead if it's past where we are */
            if (frame > netplay->self_frame_count)
            {
               /* This is squirrely: We need to assure that when we advance the
                * frame in post_frame, THEN we're referring to the frame to
                * load into. If we refer directly to read_ptr, then we'll end
                * up never reading the input for read_frame_count itself, which
                * will make the other side unhappy. */
               netplay->self_ptr         = PREV_PTR(netplay->read_ptr[connection->player]);
               netplay->self_frame_count = frame - 1;
            }

            /* Don't expect earlier data from other clients */
            for (player = 0; player < MAX_USERS; player++)
            {
               if (!(netplay->connected_players & (1<<player))) continue;
               if (frame > netplay->read_frame_count[player])
               {
                  netplay->read_ptr[player] = netplay->read_ptr[connection->player];
                  netplay->read_frame_count[player] = frame;
               }
            }

            /* And force rewind to it */
            netplay->force_rewind                  = true;
            netplay->savestate_request_outstanding = false;
            netplay->other_ptr                     = netplay->read_ptr[connection->player];
            netplay->other_frame_count             = frame;
            break;
         }

      case NETPLAY_CMD_PAUSE:
         connection->paused = true;
         netplay->remote_paused = true;
         netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_PAUSE, NULL, 0);
         break;

      case NETPLAY_CMD_RESUME:
      {
         size_t i;
         connection->paused = false;
         netplay->remote_paused = false;
         for (i = 0; i < netplay->connections_size; i++)
         {
            struct netplay_connection *sc = &netplay->connections[i];
            if (sc->active && sc->paused)
            {
               netplay->remote_paused = true;
               break;
            }
         }
         if (!netplay->remote_paused && !netplay->local_paused)
            netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_RESUME, NULL, 0);
         break;
      }

      default:
         RARCH_ERR("%s.\n", msg_hash_to_str(MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED));
         return netplay_cmd_nak(netplay, connection);
   }

   netplay_recv_flush(&connection->recv_packet_buffer);
   netplay->timeout_cnt = 0;
   if (had_input)
      *had_input = true;
   return true;

shrt:
   /* No more data, reset and try again */
   netplay_recv_reset(&connection->recv_packet_buffer);
   return true;

#undef RECV
}

/**
 * netplay_poll_net_input
 *
 * Poll input from the network
 */
int netplay_poll_net_input(netplay_t *netplay, bool block)
{
   bool had_input = false;
   int max_fd = 0;
   size_t i;

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->fd >= max_fd)
         max_fd = connection->fd + 1;
   }

   if (max_fd == 0)
      return 0;

   do
   { 
      had_input = false;

      netplay->timeout_cnt++;

      /* Make sure we're actually ready for data */
      netplay_update_unread_ptr(netplay);
      if (!netplay_delta_frame_ready(netplay,
         &netplay->buffer[netplay->unread_ptr], netplay->unread_frame_count))
         break;
      if (!netplay->is_server &&
          !netplay_delta_frame_ready(netplay,
            &netplay->buffer[netplay->server_ptr],
            netplay->server_frame_count))
         break;

      /* Read input from each connection */
      for (i = 0; i < netplay->connections_size; i++)
      {
         struct netplay_connection *connection = &netplay->connections[i];
         if (connection->active && !netplay_get_cmd(netplay, connection, &had_input))
            netplay_hangup(netplay, connection);
      }

      if (block)
      {
         netplay_update_unread_ptr(netplay);

         /* If we were blocked for input, pass if we have this frame's input */
         if (netplay->unread_frame_count > netplay->self_frame_count)
            break;

         /* If we're supposed to block but we didn't have enough input, wait for it */
         if (!had_input)
         {
            fd_set fds;
            struct timeval tv = {0};
            tv.tv_usec = RETRY_MS * 1000;

            FD_ZERO(&fds);
            for (i = 0; i < netplay->connections_size; i++)
            {
               struct netplay_connection *connection = &netplay->connections[i];
               if (connection->active)
                  FD_SET(connection->fd, &fds);
            }

            if (socket_select(max_fd, &fds, NULL, NULL, &tv) < 0)
               return -1;

            RARCH_LOG("Network is stalling at frame %u, count %u of %d ...\n",
                  netplay->self_frame_count, netplay->timeout_cnt, MAX_RETRIES);

            if (netplay->timeout_cnt >= MAX_RETRIES && !netplay->remote_paused)
               return -1;
         }
      }
   } while (had_input || block);

   return 0;
}

/**
 * netplay_flip_port
 *
 * Should we flip ports 0 and 1?
 */
bool netplay_flip_port(netplay_t *netplay)
{
   size_t frame = netplay->self_frame_count;

   if (netplay->flip_frame == 0)
      return false;

   if (netplay->is_replay)
      frame = netplay->replay_frame_count;

   return netplay->flip ^ (frame < netplay->flip_frame);
}
