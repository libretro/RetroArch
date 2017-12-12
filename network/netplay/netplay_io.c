/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Gregor Richards
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

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"

#if 0
#define DEBUG_NETPLAY_STEPS 1

static void print_state(netplay_t *netplay)
{
   char msg[512];
   size_t cur = 0;
   uint32_t player;

#define APPEND(out) cur += snprintf out
#define M msg + cur, sizeof(msg) - cur

   APPEND((M, "NETPLAY: S:%u U:%u O:%u", netplay->self_frame_count, netplay->unread_frame_count, netplay->other_frame_count));
   if (!netplay->is_server)
      APPEND((M, " H:%u", netplay->server_frame_count));
   for (player = 0; player < MAX_USERS; player++)
   {
      if ((netplay->connected_players & (1<<player)))
         APPEND((M, " %u:%u", player, netplay->read_frame_count[player]));
   }
   APPEND((M, "\n"));
   msg[sizeof(msg)-1] = '\0';

   RARCH_LOG("%s\n", msg);

#undef APPEND
#undef M
}
#endif

/**
 * remote_unpaused
 *
 * Mark a particular remote connection as unpaused and, if relevant, inform
 * every one else that they may resume.
 */
static void remote_unpaused(netplay_t *netplay, struct netplay_connection *connection)
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
}

/**
 * netplay_hangup:
 *
 * Disconnects an active Netplay connection due to an error
 */
void netplay_hangup(netplay_t *netplay, struct netplay_connection *connection)
{
   char msg[512];
   const char *dmsg;

   if (!netplay)
      return;
   if (!connection->active)
      return;

   msg[0] = msg[sizeof(msg)-1] = '\0';
   dmsg = msg;

   /* Report this disconnection */
   if (netplay->is_server)
   {
      if (connection->nick[0])
         snprintf(msg, sizeof(msg)-1, msg_hash_to_str(MSG_NETPLAY_SERVER_NAMED_HANGUP), connection->nick);
      else
         dmsg = msg_hash_to_str(MSG_NETPLAY_SERVER_HANGUP);
   }
   else
   {
      dmsg = msg_hash_to_str(MSG_NETPLAY_CLIENT_HANGUP);
      netplay->is_connected = false;
   }
   RARCH_LOG("%s\n", dmsg);
   runloop_msg_queue_push(dmsg, 1, 180, false);

   socket_close(connection->fd);
   connection->active = false;
   netplay_deinit_socket_buffer(&connection->send_packet_buffer);
   netplay_deinit_socket_buffer(&connection->recv_packet_buffer);

   if (!netplay->is_server)
   {
      netplay->self_mode = NETPLAY_CONNECTION_NONE;
      netplay->connected_players = 0;
      netplay->stall = NETPLAY_STALL_NONE;

   }
   else
   {
      /* Mark the player for removal */
      if (connection->mode == NETPLAY_CONNECTION_PLAYING ||
          connection->mode == NETPLAY_CONNECTION_SLAVE)
      {
         /* This special mode keeps the connection object alive long enough to
          * send the disconnection message at the correct time */
         connection->mode = NETPLAY_CONNECTION_DELAYED_DISCONNECT;
         connection->delay_frame = netplay->read_frame_count[connection->player];

         /* Mark them as not playing anymore */
         netplay->connected_players &= ~(1<<connection->player);
         netplay->connected_slaves  &= ~(1<<connection->player);

      }

   }

   /* Unpause them */
   if (connection->paused)
      remote_unpaused(netplay, connection);
}

/**
 * netplay_delayed_state_change:
 *
 * Handle any pending state changes which are ready as of the beginning of the
 * current frame.
 */
void netplay_delayed_state_change(netplay_t *netplay)
{
   struct netplay_connection *connection;
   size_t i;

   for (i = 0; i < netplay->connections_size; i++)
   {
      connection = &netplay->connections[i];
      if ((connection->active || connection->mode == NETPLAY_CONNECTION_DELAYED_DISCONNECT) &&
          connection->delay_frame &&
          connection->delay_frame <= netplay->self_frame_count)
      {
         /* Something was delayed! Prepare the MODE command */
         uint32_t payload[2];
         payload[0] = htonl(connection->delay_frame);
         payload[1] = htonl(connection->player);

         /* Remove the connection entirely if relevant */
         if (connection->mode == NETPLAY_CONNECTION_DELAYED_DISCONNECT)
            connection->mode = NETPLAY_CONNECTION_NONE;

         /* Then send the mode change packet */
         netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

         /* And forget the delay frame */
         connection->delay_frame = 0;
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

/**
 * netplay_send_cur_input
 *
 * Send the current input frame to a given connection.
 *
 * Returns true if successful, false otherwise.
 */
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
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING ||
       netplay->self_mode == NETPLAY_CONNECTION_SLAVE)
   {
      if (!send_input_frame(netplay, connection, NULL,
            netplay->self_frame_count,
            (netplay->is_server ? NETPLAY_CMD_INPUT_BIT_SERVER : 0) | netplay->self_player,
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
 * Send a raw Netplay command to the given connection.
 *
 * Returns true on success, false on failure.
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

/**
 * netplay_send_flush_all
 *
 * Flush all of our output buffers
 */
static void netplay_send_flush_all(netplay_t *netplay,
   struct netplay_connection *except)
{
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection == except)
         continue;
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
      {
         if (!netplay_send_flush(&connection->send_packet_buffer,
            connection->fd, true))
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

/**
 * netplay_cmd_crc
 *
 * Send a CRC command to all active clients.
 */
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

/**
 * netplay_cmd_request_savestate
 *
 * Send a savestate request command.
 */
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

/**
 * netplay_cmd_mode
 *
 * Send a mode request command to either play or spectate.
 */
bool netplay_cmd_mode(netplay_t *netplay,
   struct netplay_connection *connection,
   enum rarch_netplay_connection_mode mode)
{
   uint32_t cmd;
   uint32_t payloadBuf, *payload = NULL;
   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         cmd = NETPLAY_CMD_SPECTATE;
         break;

      case NETPLAY_CONNECTION_SLAVE:
         payload = &payloadBuf;
         payloadBuf = htonl(NETPLAY_CMD_PLAY_BIT_SLAVE);
         /* Intentional fallthrough */

      case NETPLAY_CONNECTION_PLAYING:
         cmd = NETPLAY_CMD_PLAY;
         break;

      default:
         return false;
   }
   return netplay_send_raw_cmd(netplay, connection, cmd, payload,
      payload ? sizeof(uint32_t) : 0);
}

/**
 * netplay_cmd_stall
 *
 * Send a stall command.
 */
bool netplay_cmd_stall(netplay_t *netplay,
   struct netplay_connection *connection,
   uint32_t frames)
{
   frames = htonl(frames);
   return netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_STALL, &frames, sizeof(frames));
}

#undef RECV
#define RECV(buf, sz) \
recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), \
(sz), false); \
if (recvd >= 0 && recvd < (ssize_t) (sz)) goto shrt; \
else if (recvd < 0)

static bool netplay_get_cmd(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   uint32_t cmd;
   uint32_t flip_frame;
   uint32_t cmd_size;
   ssize_t recvd;
   char msg[512];

   /* We don't handle the initial handshake here */
   if (connection->mode < NETPLAY_CONNECTION_CONNECTED)
      return netplay_handshake(netplay, connection, had_input);

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
               if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
                   connection->mode != NETPLAY_CONNECTION_SLAVE)
               {
                  RARCH_ERR("Netplay input from non-participating player.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
               player = connection->player;
            }
            else
            {
               player = buffer[1] & ~NETPLAY_CMD_INPUT_BIT_SERVER;
            }

            if (player >= MAX_USERS || !(netplay->connected_players & (1<<player)))
            {
               RARCH_ERR("Invalid NETPLAY_CMD_INPUT player number.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Check the frame number only if they're not in slave mode */
            if (connection->mode == NETPLAY_CONNECTION_PLAYING)
            {
               if (buffer[0] < netplay->read_frame_count[player])
               {
                  /* We already had this, so ignore the new transmission */
                  break;
               }
               else if (buffer[0] > netplay->read_frame_count[player])
               {
                  /* Out of order = out of luck */
                  RARCH_ERR("Netplay input out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
            }

            /* The data's good! */
            dframe = &netplay->buffer[netplay->read_ptr[player]];
            if (!netplay_delta_frame_ready(netplay, dframe, netplay->read_frame_count[player]))
            {
               /* Hopefully we'll be ready after another round of input */
               goto shrt;
            }
            memcpy(dframe->real_input_state[player], buffer + 2,
               WORDS_PER_INPUT*sizeof(uint32_t));
            dframe->have_real[player] = true;

            /* Slaves may go through several packets of data in the same frame
             * if latency is choppy, so we advance and send their data after
             * handling all network data this frame */
            if (connection->mode == NETPLAY_CONNECTION_PLAYING)
            {
               netplay->read_ptr[player] = NEXT_PTR(netplay->read_ptr[player]);
               netplay->read_frame_count[player]++;

               if (netplay->is_server)
               {
                  /* Forward it on if it's past data*/
                  if (dframe->frame <= netplay->self_frame_count)
                     send_input_frame(netplay, NULL, connection, buffer[0],
                        player, dframe->real_input_state[player]);
               }
            }

            /* If this was server data, advance our server pointer too */
            if (!netplay->is_server && (buffer[1] & NETPLAY_CMD_INPUT_BIT_SERVER))
            {
               netplay->server_ptr = netplay->read_ptr[player];
               netplay->server_frame_count = netplay->read_frame_count[player];
            }

#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("Received input from %u\n", player);
            print_state(netplay);
#endif
            break;
         }

      case NETPLAY_CMD_NOINPUT:
         {
            uint32_t frame;

            if (netplay->is_server)
            {
               RARCH_ERR("NETPLAY_CMD_NOINPUT from a client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frame, sizeof(frame))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_NOINPUT payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            frame = ntohl(frame);

            if (frame < netplay->server_frame_count)
            {
               /* We already had this, so ignore the new transmission */
               break;
            }

            if (frame != netplay->server_frame_count)
            {
               RARCH_ERR("NETPLAY_CMD_NOINPUT for invalid frame.\n");
               return netplay_cmd_nak(netplay, connection);
            }

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
         {
            RARCH_ERR("NETPLAY_CMD_FLIP_PLAYERS from a client.\n");
            return netplay_cmd_nak(netplay, connection);
         }

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
         {
            RARCH_ERR("NETPLAY_CMD_SPECTATE from a server.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->mode == NETPLAY_CONNECTION_PLAYING ||
             connection->mode == NETPLAY_CONNECTION_SLAVE)
         {
            /* The frame we haven't received is their end frame */
            connection->delay_frame = netplay->read_frame_count[connection->player];

            /* Mark them as not playing anymore */
            connection->mode = NETPLAY_CONNECTION_SPECTATING;
            netplay->connected_players &= ~(1<<connection->player);
            netplay->connected_slaves  &= ~(1<<connection->player);

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
         bool slave = false;
         settings_t *settings = config_get_ptr();

         /* Check if they requested slave mode */
         if (cmd_size == sizeof(uint32_t))
         {
            RECV(payload, sizeof(uint32_t))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_PLAY payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            payload[0] = ntohl(payload[0]);
            if (payload[0] & NETPLAY_CMD_PLAY_BIT_SLAVE)
               slave = true;
         }
         else if (cmd_size != 0)
         {
            RARCH_ERR("Invalid payload size for NETPLAY_CMD_PLAY.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         /* Check if their slave mode request corresponds with what we allow */
         if (settings->bools.netplay_require_slaves)
            slave = true;
         else if (!settings->bools.netplay_allow_slaves)
            slave = false;

         payload[0] = htonl(netplay->self_frame_count + 1);

         if (!netplay->is_server)
         {
            RARCH_ERR("NETPLAY_CMD_PLAY from a server.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->delay_frame)
         {
            /* Can't switch modes while a mode switch is already in progress. */
            payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_TOO_FAST);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
            break;
         }

         if (!connection->can_play)
         {
            /* Not allowed to play */
            payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_UNPRIVILEGED);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
            break;
         }

         /* Find an available player slot */
         for (player = 0; player <= netplay->player_max; player++)
         {
            if (!(netplay->self_mode == NETPLAY_CONNECTION_PLAYING &&
                  netplay->self_player == player) &&
                !(netplay->connected_players & (1<<player)))
               break;
         }
         if (player > netplay->player_max)
         {
            /* No slots free! */
            payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
            break;
         }

         if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
             connection->mode != NETPLAY_CONNECTION_SLAVE)
         {
            /* Mark them as playing */
            connection->mode = slave ? NETPLAY_CONNECTION_SLAVE :
                                       NETPLAY_CONNECTION_PLAYING;
            connection->player = player;
            netplay->connected_players |= 1<<player;
            if (slave)
               netplay->connected_slaves |= 1<<player;

            /* Tell everyone */
            payload[1] = htonl(NETPLAY_CMD_MODE_BIT_PLAYING |
                               (slave?NETPLAY_CMD_MODE_BIT_SLAVE:0) |
                               connection->player);
            netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

            /* Announce it */
            msg[sizeof(msg)-1] = '\0';
            snprintf(msg, sizeof(msg)-1, "Player %d has joined", player+1);
            RARCH_LOG("%s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, false);

         }

         /* Tell the player even if they were confused */
         payload[1] = htonl(NETPLAY_CMD_MODE_BIT_PLAYING |
                            ((connection->mode == NETPLAY_CONNECTION_SLAVE)?
                             NETPLAY_CMD_MODE_BIT_SLAVE:0) |
                            NETPLAY_CMD_MODE_BIT_YOU |
                            connection->player);
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
         {
            RARCH_ERR("Invalid payload size for NETPLAY_CMD_MODE.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         RECV(payload, sizeof(payload))
         {
            RARCH_ERR("NETPLAY_CMD_MODE failed to receive payload.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         frame = ntohl(payload[0]);

         /* We're changing past input, so must replay it */
         if (frame < netplay->self_frame_count)
            netplay->force_rewind = true;

         mode = ntohl(payload[1]);
         player = mode & 0xFFFF;
         if (player >= MAX_USERS)
         {
            RARCH_ERR("Received NETPLAY_CMD_MODE for a higher player number than we support.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (mode & NETPLAY_CMD_MODE_BIT_YOU)
         {
            /* A change to me! */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               if (frame != netplay->server_frame_count)
               {
                  RARCH_ERR("Received mode change out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Hooray, I get to play now! */
               if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
               {
                  RARCH_ERR("Received player mode change even though I'm already a player.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Our mode is based on whether we have the slave bit set */
               if (mode & NETPLAY_CMD_MODE_BIT_SLAVE)
               {
                  netplay->self_mode = NETPLAY_CONNECTION_SLAVE;

                  /* In slave mode we receive the data from the remote side, so
                   * we actually consider ourself a connected player */
                  netplay->connected_players |= (1<<player);
                  netplay->read_ptr[player] = netplay->server_ptr;
                  netplay->read_frame_count[player] = netplay->server_frame_count;
               }
               else
               {
                  netplay->self_mode = NETPLAY_CONNECTION_PLAYING;
               }
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
                  uint32_t frame_count;

                  /* It wants future frames, make sure we don't capture or send intermediate ones */
                  START(netplay->self_ptr);
                  frame_count = netplay->self_frame_count;
                  while (true)
                  {
                     if (!dframe->used)
                     {
                        /* Make sure it's ready */
                        if (!netplay_delta_frame_ready(netplay, dframe, frame_count))
                        {
                           RARCH_ERR("Received mode change but delta frame isn't ready!\n");
                           return netplay_cmd_nak(netplay, connection);
                        }
                     }

                     memset(dframe->self_state, 0, sizeof(dframe->self_state));
                     memset(dframe->real_input_state[player], 0, sizeof(dframe->self_state));
                     dframe->have_local = true;

                     /* Go on to the next delta frame */
                     NEXT();
                     frame_count++;

                     if (frame_count >= frame)
                        break;
                  }

               }

               /* Announce it */
               msg[sizeof(msg)-1] = '\0';
               snprintf(msg, sizeof(msg)-1, "You have joined as player %d", player+1);
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("Received mode change self->%u\n", player);
               print_state(netplay);
#endif

            }
            else /* YOU && !PLAYING */
            {
               /* I'm no longer playing, but I should already know this */
               if (netplay->self_mode != NETPLAY_CONNECTION_SPECTATING)
               {
                  RARCH_ERR("Received mode change to spectator unprompted.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Unmark ourself, in case we were in slave mode */
               netplay->connected_players &= ~(1<<player);

               /* Announce it */
               strlcpy(msg, "You have left the game", sizeof(msg));
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("Received mode change %u self->spectating\n", netplay->self_player);
               print_state(netplay);
#endif

            }

         }
         else /* !YOU */
         {
            /* Somebody else is joining or parting */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               if (frame != netplay->server_frame_count)
               {
                  RARCH_ERR("Received mode change out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               netplay->connected_players |= (1<<player);

               netplay->read_ptr[player] = netplay->server_ptr;
               netplay->read_frame_count[player] = netplay->server_frame_count;

               /* Announce it */
               msg[sizeof(msg)-1] = '\0';
               snprintf(msg, sizeof(msg)-1, "Player %d has joined", player+1);
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("Received mode change spectator->%u\n", player);
               print_state(netplay);
#endif

            }
            else
            {
               netplay->connected_players &= ~(1<<player);

               /* Announce it */
               msg[sizeof(msg)-1] = '\0';
               snprintf(msg, sizeof(msg)-1, "Player %d has left", player+1);
               RARCH_LOG("%s\n", msg);
               runloop_msg_queue_push(msg, 1, 180, false);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("Received mode change %u->spectator\n", player);
               print_state(netplay);
#endif

            }

         }

         break;

#undef START
#undef NEXT
      }

      case NETPLAY_CMD_MODE_REFUSED:
         {
            uint32_t reason;
            const char *dmsg = NULL;

            if (netplay->is_server)
            {
               RARCH_ERR("NETPLAY_CMD_MODE_REFUSED from client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(uint32_t))
            {
               RARCH_ERR("Received invalid payload size for NETPLAY_CMD_MODE_REFUSED.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&reason, sizeof(reason))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_MODE_REFUSED payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            reason = ntohl(reason);

            switch (reason)
            {
               case NETPLAY_CMD_MODE_REFUSED_REASON_UNPRIVILEGED:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED);
                  break;

               case NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS);
                  break;

               default:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY);
            }

            if (dmsg)
            {
               RARCH_LOG("%s\n", dmsg);
               runloop_msg_queue_push(dmsg, 1, 180, false);
            }
            break;
         }

      case NETPLAY_CMD_DISCONNECT:
         netplay_hangup(netplay, connection);
         return true;

      case NETPLAY_CMD_CRC:
         {
            uint32_t buffer[2];
            size_t tmp_ptr = netplay->run_ptr;
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
            } while (tmp_ptr != netplay->run_ptr);

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
      case NETPLAY_CMD_RESET:
         {
            uint32_t frame;
            uint32_t isize;
            uint32_t rd, wn;
            uint32_t player;
            struct compression_transcoder *ctrans;

            /* Make sure we're ready for it */
            if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            {
               if (!netplay->is_replay)
               {
                  netplay->is_replay = true;
                  netplay->replay_ptr = netplay->run_ptr;
                  netplay->replay_frame_count = netplay->run_frame_count;
                  netplay_wait_and_init_serialization(netplay);
                  netplay->is_replay = false;
               }
               else
               {
                  netplay_wait_and_init_serialization(netplay);
               }
            }

            /* Only players may load states */
            if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
                connection->mode != NETPLAY_CONNECTION_SLAVE)
            {
               RARCH_ERR("Netplay state load from a spectator.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* We only allow players to load state if we're in a simple
             * two-player situation */
            if (netplay->is_server && netplay->connections_size > 1)
            {
               RARCH_ERR("Netplay state load from a client with other clients connected disallowed.\n");
               return netplay_cmd_nak(netplay, connection);
            }

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

            /* Check the payload size */
            if ((cmd == NETPLAY_CMD_LOAD_SAVESTATE &&
                 (cmd_size < 2*sizeof(uint32_t) || cmd_size > netplay->zbuffer_size + 2*sizeof(uint32_t))) ||
                (cmd == NETPLAY_CMD_RESET && cmd_size != sizeof(uint32_t)))
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

            if ((netplay->is_server && frame != netplay->read_frame_count[connection->player]) ||
                (!netplay->is_server && frame != netplay->server_frame_count))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE loading a state out of order!\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (!netplay_delta_frame_ready(netplay, &netplay->buffer[netplay->read_ptr[connection->player]], frame))
            {
               /* Hopefully it will be after another round of input */
               goto shrt;
            }

            /* Now we switch based on whether we're loading a state or resetting */
            if (cmd == NETPLAY_CMD_LOAD_SAVESTATE)
            {
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
               switch (connection->compression_supported)
               {
                  case NETPLAY_COMPRESSION_ZLIB:
                     ctrans = &netplay->compress_zlib;
                     break;
                  default:
                     ctrans = &netplay->compress_nil;
               }
               ctrans->decompression_backend->set_in(ctrans->decompression_stream,
                  netplay->zbuffer, cmd_size - 2*sizeof(uint32_t));
               ctrans->decompression_backend->set_out(ctrans->decompression_stream,
                  (uint8_t*)netplay->buffer[netplay->read_ptr[connection->player]].state,
                  (unsigned)netplay->state_size);
               ctrans->decompression_backend->trans(ctrans->decompression_stream,
                  true, &rd, &wn, NULL);

               /* Force a rewind to the relevant frame */
               netplay->force_rewind = true;
            }
            else
            {
               /* Resetting */
               netplay->force_reset = true;

            }

            /* Skip ahead if it's past where we are */
            if (frame > netplay->run_frame_count ||
                cmd == NETPLAY_CMD_RESET)
            {
               /* This is squirrely: We need to assure that when we advance the
                * frame in post_frame, THEN we're referring to the frame to
                * load into. If we refer directly to read_ptr, then we'll end
                * up never reading the input for read_frame_count itself, which
                * will make the other side unhappy. */
               netplay->run_ptr           = PREV_PTR(netplay->read_ptr[connection->player]);
               netplay->run_frame_count   = frame - 1;
               if (frame > netplay->self_frame_count)
               {
                  netplay->self_ptr         = netplay->run_ptr;
                  netplay->self_frame_count = netplay->run_frame_count;
               }
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

            /* Make sure our states are correct */
            netplay->savestate_request_outstanding = false;
            netplay->other_ptr                     = netplay->read_ptr[connection->player];
            netplay->other_frame_count             = frame;

#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("Loading state at %u\n", frame);
            print_state(netplay);
#endif

            break;
         }

      case NETPLAY_CMD_PAUSE:
         {
            char msg[512], nick[NETPLAY_NICK_LEN];
            msg[sizeof(msg)-1] = '\0';

            /* Read in the paused nick */
            if (cmd_size != sizeof(nick))
            {
               RARCH_ERR("NETPLAY_CMD_PAUSE received invalid payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            RECV(nick, sizeof(nick))
            {
               RARCH_ERR("Failed to receive paused nickname.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            nick[sizeof(nick)-1] = '\0';

            /* We outright ignore pausing from spectators and slaves */
            if (connection->mode != NETPLAY_CONNECTION_PLAYING)
               break;

            connection->paused = true;
            netplay->remote_paused = true;
            if (netplay->is_server)
            {
               /* Inform peers */
               snprintf(msg, sizeof(msg)-1, msg_hash_to_str(MSG_NETPLAY_PEER_PAUSED), connection->nick);
               netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_PAUSE,
                     connection->nick, NETPLAY_NICK_LEN);

               /* We may not reach post_frame soon, so flush the pause message
                * immediately. */
               netplay_send_flush_all(netplay, connection);
            }
            else
            {
               snprintf(msg, sizeof(msg)-1, msg_hash_to_str(MSG_NETPLAY_PEER_PAUSED), nick);
            }
            RARCH_LOG("%s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, false);
            break;
         }

      case NETPLAY_CMD_RESUME:
         remote_unpaused(netplay, connection);
         break;

      case NETPLAY_CMD_STALL:
         {
            uint32_t frames;

            if (cmd_size != sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_STALL with incorrect payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frames, sizeof(frames))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_STALL payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            frames = ntohl(frames);
            if (frames > NETPLAY_MAX_REQ_STALL_TIME)
               frames = NETPLAY_MAX_REQ_STALL_TIME;

            if (netplay->is_server)
            {
               /* Only servers can request a stall! */
               RARCH_ERR("Netplay client requested a stall?\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* We can only stall for one reason at a time */
            if (!netplay->stall)
            {
               connection->stall = netplay->stall = NETPLAY_STALL_SERVER_REQUESTED;
               netplay->stall_time = 0;
               connection->stall_frame = frames;
            }
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

   netplay->timeout_cnt = 0;

   do
   {
      had_input = false;

      netplay->timeout_cnt++;

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
         if (netplay->unread_frame_count > netplay->run_frame_count)
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
                  netplay->run_frame_count, netplay->timeout_cnt, MAX_RETRIES);

            if (netplay->timeout_cnt >= MAX_RETRIES && !netplay->remote_paused)
               return -1;
         }
      }
   } while (had_input || block);

   return 0;
}

/**
 * netplay_handle_slaves
 *
 * Handle any slave connections
 */
void netplay_handle_slaves(netplay_t *netplay)
{
   struct delta_frame *frame = &netplay->buffer[netplay->self_ptr];
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active &&
          connection->mode == NETPLAY_CONNECTION_SLAVE)
      {
         int player = connection->player;

         /* This is a slave connection. First, should we do anything at all? If
          * we've already "read" this data, then we can just ignore it */
         if (netplay->read_frame_count[player] > netplay->self_frame_count)
            continue;

         /* Alright, we have to send something. Do we need to generate it first? */
         if (!frame->have_real[player])
         {
            /* Copy the previous frame's data */
            memcpy(frame->real_input_state[player],
                   netplay->buffer[PREV_PTR(netplay->self_ptr)].real_input_state[player],
                   WORDS_PER_INPUT*sizeof(uint32_t));
            frame->have_real[player] = true;
         }

         /* Send it along */
         send_input_frame(netplay, NULL, NULL, netplay->self_frame_count,
            player, frame->real_input_state[player]);

         /* And mark it as "read" */
         netplay->read_ptr[player] = NEXT_PTR(netplay->self_ptr);
         netplay->read_frame_count[player] = netplay->self_frame_count + 1;
      }
   }
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

/**
 * netplay_announce_nat_traversal
 *
 * Announce successful NAT traversal.
 */
void netplay_announce_nat_traversal(netplay_t *netplay)
{
#ifndef HAVE_SOCKET_LEGACY
   char msg[512], host[PATH_MAX_LENGTH], port[6];

   if (netplay->nat_traversal_state.have_inet4)
   {
      if (getnameinfo((const struct sockaddr *) &netplay->nat_traversal_state.ext_inet4_addr,
               sizeof(struct sockaddr_in),
               host, PATH_MAX_LENGTH, port, 6, NI_NUMERICHOST|NI_NUMERICSERV) != 0)
         return;

   }
#ifdef HAVE_INET6
   else if (netplay->nat_traversal_state.have_inet6)
   {
      if (getnameinfo((const struct sockaddr *) &netplay->nat_traversal_state.ext_inet6_addr,
               sizeof(struct sockaddr_in6),
               host, PATH_MAX_LENGTH, port, 6, NI_NUMERICHOST|NI_NUMERICSERV) != 0)
         return;

   }
#endif
   else
      return;

   snprintf(msg, sizeof(msg), "%s: %s:%s\n",
         msg_hash_to_str(MSG_PUBLIC_ADDRESS),
         host, port);
   runloop_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);
#endif
}

/**
 * netplay_init_nat_traversal
 *
 * Initialize the NAT traversal library and try to open a port
 */
void netplay_init_nat_traversal(netplay_t *netplay)
{
   memset(&netplay->nat_traversal_state, 0, sizeof(netplay->nat_traversal_state));
   netplay->nat_traversal_task_oustanding = true;
   task_push_netplay_nat_traversal(&netplay->nat_traversal_state, netplay->tcp_port);
}
