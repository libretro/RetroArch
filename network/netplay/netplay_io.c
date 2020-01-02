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
#include "../../command.h"
#include "../../tasks/tasks_internal.h"

#include "../../discord/discord.h"

extern bool discord_is_inited;

static void handle_play_spectate(netplay_t *netplay, uint32_t client_num,
      struct netplay_connection *connection, uint32_t cmd, uint32_t cmd_size,
      uint32_t *payload);

#if 0
#define DEBUG_NETPLAY_STEPS 1

static void print_state(netplay_t *netplay)
{
   char msg[512];
   size_t cur = 0;
   uint32_t client;

#define APPEND(out) cur += snprintf out
#define M msg + cur, sizeof(msg) - cur

   APPEND((M, "NETPLAY: S:%u U:%u O:%u", netplay->self_frame_count, netplay->unread_frame_count, netplay->other_frame_count));
   if (!netplay->is_server)
      APPEND((M, " H:%u", netplay->server_frame_count));
   for (client = 0; client < MAX_USERS; client++)
   {
      if ((netplay->connected_players & (1<<client)))
         APPEND((M, " %u:%u", client, netplay->read_frame_count[client]));
   }
   msg[sizeof(msg)-1] = '\0';

   RARCH_LOG("[netplay] %s\n", msg);

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
   size_t i;

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
#ifdef HAVE_DISCORD
      if (discord_is_inited)
      {
         discord_userdata_t userdata;
         userdata.status = DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED;
         command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
      }
#endif
      netplay->is_connected = false;
   }
   RARCH_LOG("[netplay] %s\n", dmsg);
   runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   socket_close(connection->fd);
   connection->active = false;
   netplay_deinit_socket_buffer(&connection->send_packet_buffer);
   netplay_deinit_socket_buffer(&connection->recv_packet_buffer);

   if (!netplay->is_server)
   {
      netplay->self_mode = NETPLAY_CONNECTION_NONE;
      netplay->connected_players &= (1L<<netplay->self_client_num);
      for (i = 0; i < MAX_CLIENTS; i++)
      {
         if (i == netplay->self_client_num)
            continue;
         netplay->client_devices[i] = 0;
      }
      for (i = 0; i < MAX_INPUT_DEVICES; i++)
         netplay->device_clients[i] &= (1L<<netplay->self_client_num);
      netplay->stall = NETPLAY_STALL_NONE;

   }
   else
   {
      uint32_t client_num = (uint32_t)(connection - netplay->connections + 1);

      /* Mark the player for removal */
      if (connection->mode == NETPLAY_CONNECTION_PLAYING ||
          connection->mode == NETPLAY_CONNECTION_SLAVE)
      {
         /* This special mode keeps the connection object alive long enough to
          * send the disconnection message at the correct time */
         connection->mode = NETPLAY_CONNECTION_DELAYED_DISCONNECT;
         connection->delay_frame = netplay->read_frame_count[client_num];

         /* Mark them as not playing anymore */
         netplay->connected_players &= ~(1L<<client_num);
         netplay->connected_slaves  &= ~(1L<<client_num);
         netplay->client_devices[client_num] = 0;
         for (i = 0; i < MAX_INPUT_DEVICES; i++)
            netplay->device_clients[i] &= ~(1L<<client_num);

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
   unsigned i;

   for (i = 0; i < netplay->connections_size; i++)
   {
      uint32_t client_num                   = (uint32_t)(i + 1);
      struct netplay_connection *connection = &netplay->connections[i];

      if ((connection->active || connection->mode == NETPLAY_CONNECTION_DELAYED_DISCONNECT) &&
          connection->delay_frame &&
          connection->delay_frame <= netplay->self_frame_count)
      {
         /* Something was delayed! Prepare the MODE command */
         uint32_t payload[15] = {0};
         payload[0]           = htonl(connection->delay_frame);
         payload[1]           = htonl(client_num);
         payload[2]           = htonl(0);

         memcpy(payload + 3, netplay->device_share_modes, sizeof(netplay->device_share_modes));
         strncpy((char *) (payload + 7), connection->nick, NETPLAY_NICK_LEN);

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
static bool send_input_frame(netplay_t *netplay, struct delta_frame *dframe,
      struct netplay_connection *only, struct netplay_connection *except,
      uint32_t client_num, bool slave)
{
#define BUFSZ 16 /* FIXME: Arbitrary restriction */
   uint32_t buffer[BUFSZ], devices, device;
   size_t bufused, i;

   /* Set up the basic buffer */
   bufused = 4;
   buffer[0] = htonl(NETPLAY_CMD_INPUT);
   buffer[2] = htonl(dframe->frame);
   buffer[3] = htonl(client_num);

   /* Add the device data */
   devices = netplay->client_devices[client_num];
   for (device = 0; device < MAX_INPUT_DEVICES; device++)
   {
      netplay_input_state_t istate;
      if (!(devices & (1<<device)))
         continue;
      istate = dframe->real_input[device];
      while (istate && (!istate->used || istate->client_num != (slave?MAX_CLIENTS:client_num)))
         istate = istate->next;
      if (!istate)
         continue;
      if (bufused + istate->size >= BUFSZ)
         continue; /* FIXME: More severe? */
      for (i = 0; i < istate->size; i++)
         buffer[bufused+i] = htonl(istate->data[i]);
      bufused += istate->size;
   }
   buffer[1] = htonl((bufused-2) * sizeof(uint32_t));

#ifdef DEBUG_NETPLAY_STEPS
   RARCH_LOG("[netplay] Sending input for client %u\n", (unsigned) client_num);
   print_state(netplay);
#endif

   if (only)
   {
      if (!netplay_send(&only->send_packet_buffer, only->fd, buffer, bufused*sizeof(uint32_t)))
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
         if (connection == except)
            continue;
         if (connection->active &&
             connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
             (connection->mode != NETPLAY_CONNECTION_PLAYING ||
              i+1 != client_num))
         {
            if (!netplay_send(&connection->send_packet_buffer, connection->fd,
                  buffer, bufused*sizeof(uint32_t)))
               netplay_hangup(netplay, connection);
         }
      }
   }

   return true;
#undef BUFSZ
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
   uint32_t from_client, to_client;
   struct delta_frame *dframe = &netplay->buffer[netplay->self_ptr];

   if (netplay->is_server)
   {
      to_client = (uint32_t)(connection - netplay->connections + 1);

      /* Send the other players' input data (FIXME: This involves an
       * unacceptable amount of recalculating) */
      for (from_client = 1; from_client < MAX_CLIENTS; from_client++)
      {
         if (from_client == to_client)
            continue;

         if ((netplay->connected_players & (1<<from_client)))
         {
            if (dframe->have_real[from_client])
            {
               if (!send_input_frame(netplay, dframe, connection, NULL, from_client, false))
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
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING
         || netplay->self_mode == NETPLAY_CONNECTION_SLAVE)
   {
      if (!send_input_frame(netplay, dframe, connection, NULL,
            netplay->self_client_num,
            netplay->self_mode == NETPLAY_CONNECTION_SLAVE))
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
 * Send a mode change request. As a server, the request is to ourself, and so
 * honored instantly.
 */
bool netplay_cmd_mode(netplay_t *netplay,
   enum rarch_netplay_connection_mode mode)
{
   uint32_t cmd, device;
   uint32_t payload_buf = 0, *payload    = NULL;
   uint8_t share_mode                    = 0;
   struct netplay_connection *connection = NULL;

   if (!netplay->is_server)
      connection = &netplay->one_connection;

   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         cmd = NETPLAY_CMD_SPECTATE;
         break;

      case NETPLAY_CONNECTION_SLAVE:
         payload_buf = NETPLAY_CMD_PLAY_BIT_SLAVE;
         /* no break */

      case NETPLAY_CONNECTION_PLAYING:
         {
            settings_t *settings = config_get_ptr();
            payload = &payload_buf;

            /* Add a share mode if requested */
            share_mode = netplay_settings_share_mode(
                  settings->uints.netplay_share_digital,
                  settings->uints.netplay_share_analog
                  );
            payload_buf |= ((uint32_t) share_mode) << 16;

            /* Request devices */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (settings->bools.netplay_request_devices[device])
                  payload_buf |= 1<<device;
            }

            payload_buf = htonl(payload_buf);
            cmd         = NETPLAY_CMD_PLAY;
         }
         break;

      default:
         return false;
   }

   if (netplay->is_server)
   {
      handle_play_spectate(netplay, 0, NULL, cmd, payload ? sizeof(uint32_t) : 0, payload);
      return true;
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

/**
 * announce_play_spectate
 *
 * Announce a play or spectate mode change
 */
static void announce_play_spectate(netplay_t *netplay,
      const char *nick,
      enum rarch_netplay_connection_mode mode, uint32_t devices)
{
   char msg[512];
   msg[0] = msg[sizeof(msg) - 1] = '\0';

   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         if (nick)
            snprintf(msg, sizeof(msg) - 1,
                  msg_hash_to_str(MSG_NETPLAY_PLAYER_S_LEFT), NETPLAY_NICK_LEN,
                  nick);
         else
            strlcpy(msg, msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME), sizeof(msg));
         break;

      case NETPLAY_CONNECTION_PLAYING:
      case NETPLAY_CONNECTION_SLAVE:
      {
         uint32_t device;
         uint32_t one_device = (uint32_t) -1;
         char device_str[512];
         size_t device_str_len;

         for (device = 0; device < MAX_INPUT_DEVICES; device++)
         {
            if (!(devices & (1<<device)))
               continue;
            if (one_device == (uint32_t) -1)
               one_device = device;
            else
            {
               one_device = (uint32_t) -1;
               break;
            }
         }

         if (one_device != (uint32_t) -1)
         {
            /* Only have one device, simpler message */
            if (nick)
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N),
                     NETPLAY_NICK_LEN, nick, one_device + 1);
            else
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N),
                     one_device + 1);
         }
         else
         {
            /* Multiple devices, so step one is to make the device string listing them all */
            device_str[0] = 0;
            device_str_len = 0;
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (!(devices & (1<<device)))
                  continue;
               if (device_str_len)
                  device_str_len += snprintf(device_str + device_str_len,
                        sizeof(device_str) - 1 - device_str_len, ", ");
               device_str_len += snprintf(device_str + device_str_len,
                     sizeof(device_str) - 1 - device_str_len, "%u",
                     (unsigned) (device+1));
            }

            /* Then we make the final string */
            if (nick)
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(
                           MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S),
                     NETPLAY_NICK_LEN, nick, sizeof(device_str),
                     device_str);
            else
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(
                           MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S),
                     sizeof(device_str), device_str);
         }

         break;
      }

      default: /* wrong usage */
         break;
   }

   if (msg[0])
   {
      RARCH_LOG("[netplay] %s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

/**
 * handle_play_spectate
 *
 * Handle a play or spectate request
 */
static void handle_play_spectate(netplay_t *netplay, uint32_t client_num,
      struct netplay_connection *connection, uint32_t cmd, uint32_t cmd_size,
      uint32_t *in_payload)
{
   /*
    * MODE payload:
    * word 0: frame number
    * word 1: mode info (playing, slave, client number)
    * word 2: device bitmap
    * words 3-6: share modes for all devices
    * words 7-14: client nick
    */
   uint32_t payload[15] = {0};

   switch (cmd)
   {
      case NETPLAY_CMD_SPECTATE:
      {
         size_t i;

         /* The frame we haven't received is their end frame */
         if (connection)
            connection->delay_frame = netplay->read_frame_count[client_num];

         /* Mark them as not playing anymore */
         if (connection)
            connection->mode = NETPLAY_CONNECTION_SPECTATING;
         else
         {
            netplay->self_devices = 0;
            netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
         }
         netplay->connected_players &= ~(1 << client_num);
         netplay->connected_slaves &= ~(1 << client_num);
         netplay->client_devices[client_num] = 0;
         for (i = 0; i < MAX_INPUT_DEVICES; i++)
            netplay->device_clients[i] &= ~(1 << client_num);

         /* Tell someone */
         payload[0] = htonl(netplay->read_frame_count[client_num]);
         payload[2] = htonl(0);
         memcpy(payload + 3, netplay->device_share_modes, sizeof(netplay->device_share_modes));
         if (connection)
         {
            /* Only tell the player. The others will be told at delay_frame */
            payload[1] = htonl(NETPLAY_CMD_MODE_BIT_YOU | client_num);
            strncpy((char *) (payload + 7), connection->nick, NETPLAY_NICK_LEN);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

         }
         else
         {
            /* It was the server, so tell everyone else */
            payload[1] = htonl(0);
            strncpy((char *) (payload + 7), netplay->nick, NETPLAY_NICK_LEN);
            netplay_send_raw_cmd_all(netplay, NULL, NETPLAY_CMD_MODE, payload, sizeof(payload));

         }

         /* Announce it */
         announce_play_spectate(netplay, connection ? connection->nick : NULL,
               NETPLAY_CONNECTION_SPECTATING, 0);
         break;
      }

      case NETPLAY_CMD_PLAY:
      {
         uint32_t mode, devices = 0, device;
         uint8_t share_mode;
         bool slave = false;
         settings_t *settings = config_get_ptr();

         if (cmd_size != sizeof(uint32_t) || !in_payload)
            return;
         mode = ntohl(in_payload[0]);

         /* Check the requested mode */
         slave = (mode&NETPLAY_CMD_PLAY_BIT_SLAVE)?true:false;
         share_mode = (mode>>16)&0xFF;

         /* And the requested devices */
         devices = mode&0xFFFF;

         /* Check if their slave mode request corresponds with what we allow */
         if (connection)
         {
            if (settings->bools.netplay_require_slaves)
               slave = true;
            else if (!settings->bools.netplay_allow_slaves)
               slave = false;
         }
         else
            slave = false;

         /* Fix our share mode */
         if (share_mode)
         {
            if ((share_mode & NETPLAY_SHARE_DIGITAL_BITS) == 0)
               share_mode |= NETPLAY_SHARE_DIGITAL_OR;
            if ((share_mode & NETPLAY_SHARE_ANALOG_BITS) == 0)
               share_mode |= NETPLAY_SHARE_ANALOG_MAX;
            share_mode &= ~NETPLAY_SHARE_NO_PREFERENCE;
         }

         /* They start at the next frame, but we start immediately */
         if (connection)
         {
            netplay->read_ptr[client_num] = NEXT_PTR(netplay->self_ptr);
            netplay->read_frame_count[client_num] = netplay->self_frame_count + 1;
         }
         else
         {
            netplay->read_ptr[client_num] = netplay->self_ptr;
            netplay->read_frame_count[client_num] = netplay->self_frame_count;
         }
         payload[0] = htonl(netplay->read_frame_count[client_num]);

         if (devices)
         {
            /* Make sure the devices are available and/or shareable */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (!(devices & (1<<device)))
                  continue;
               if (!netplay->device_clients[device])
                  continue;
               if (netplay->device_share_modes[device] && share_mode)
                  continue;

               /* Device already taken and unshareable */
               payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_NOT_AVAILABLE);
               /* FIXME: Refusal message for the server */
               if (connection)
                  netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
               devices = 0;
               break;
            }
            if (devices == 0)
               break;

            /* Set the share mode on any new devices */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (!(devices & (1<<device)))
                  continue;
               if (!netplay->device_clients[device])
                  netplay->device_share_modes[device] = share_mode;
            }

         }
         else
         {
            /* Find an available device */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (netplay->config_devices[device] == RETRO_DEVICE_NONE)
               {
                  device = MAX_INPUT_DEVICES;
                  break;
               }
               if (!netplay->device_clients[device])
                  break;
            }
            if (device >= MAX_INPUT_DEVICES &&
                netplay->config_devices[1] == RETRO_DEVICE_NONE && share_mode)
            {
               /* No device free and no device specifically asked for, but only
                * one device, so share it */
               if (netplay->device_share_modes[0])
               {
                  device     = 0;
                  share_mode = netplay->device_share_modes[0];
                  break;
               }
            }
            if (device >= MAX_INPUT_DEVICES)
            {
               /* No slots free! */
               payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS);
               /* FIXME: Message for the server */
               if (connection)
                  netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
               break;
            }
            devices = 1<<device;
            netplay->device_share_modes[device] = share_mode;

         }

         payload[2] = htonl(devices);

         /* Mark them as playing */
         if (connection)
            connection->mode =
                  slave ? NETPLAY_CONNECTION_SLAVE : NETPLAY_CONNECTION_PLAYING;
         else
         {
            netplay->self_devices = devices;
            netplay->self_mode = NETPLAY_CONNECTION_PLAYING;
         }
         netplay->connected_players |= 1 << client_num;
         if (slave)
            netplay->connected_slaves |= 1 << client_num;
         netplay->client_devices[client_num] = devices;
         for (device = 0; device < MAX_INPUT_DEVICES; device++)
         {
            if (!(devices & (1<<device)))
               continue;
            netplay->device_clients[device] |= 1 << client_num;
         }

         /* Tell everyone */
         payload[1] = htonl(
               NETPLAY_CMD_MODE_BIT_PLAYING
                     | (slave ? NETPLAY_CMD_MODE_BIT_SLAVE : 0) | client_num);
         memcpy(payload + 3, netplay->device_share_modes, sizeof(netplay->device_share_modes));
         if (connection)
            strncpy((char *) (payload + 7), connection->nick, NETPLAY_NICK_LEN);
         else
            strncpy((char *) (payload + 7), netplay->nick, NETPLAY_NICK_LEN);
         netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE,
               payload, sizeof(payload));

         /* Tell the player */
         if (connection)
         {
            payload[1] = htonl(NETPLAY_CMD_MODE_BIT_PLAYING |
                               ((connection->mode == NETPLAY_CONNECTION_SLAVE)?
                                NETPLAY_CMD_MODE_BIT_SLAVE:0) |
                               NETPLAY_CMD_MODE_BIT_YOU |
                               client_num);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));
         }

         /* Announce it */
         announce_play_spectate(netplay, connection ? connection->nick : NULL,
               NETPLAY_CONNECTION_PLAYING, devices);
         break;
      }
   }
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
   uint32_t cmd_size;
   ssize_t recvd;

   /* We don't handle the initial handshake here */
   if (connection->mode < NETPLAY_CONNECTION_CONNECTED)
      return netplay_handshake(netplay, connection, had_input);

   RECV(&cmd, sizeof(cmd))
      return false;

   cmd      = ntohl(cmd);

   RECV(&cmd_size, sizeof(cmd_size))
      return false;

   cmd_size = ntohl(cmd_size);

#ifdef DEBUG_NETPLAY_STEPS
   RARCH_LOG("[netplay] Received netplay command %X (%u) from %u\n", cmd, cmd_size,
         (unsigned) (connection - netplay->connections));
#endif

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
            uint32_t frame_num, client_num, input_size, devices, device;
            struct delta_frame *dframe;

            if (cmd_size < 2*sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_INPUT too short, no frame/client number.");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frame_num, sizeof(frame_num))
               return false;
            RECV(&client_num, sizeof(client_num))
               return false;
            frame_num = ntohl(frame_num);
            client_num = ntohl(client_num);
            client_num &= 0xFFFF;

            if (netplay->is_server)
            {
               /* Ignore the claimed client #, must be this client */
               if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
                   connection->mode != NETPLAY_CONNECTION_SLAVE)
               {
                  RARCH_ERR("Netplay input from non-participating player.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
               client_num = (uint32_t)(connection - netplay->connections + 1);
            }

            if (client_num > MAX_CLIENTS)
            {
               RARCH_ERR("NETPLAY_CMD_INPUT received data for an unsupported client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Figure out how much input is expected */
            devices = netplay->client_devices[client_num];
            input_size = netplay_expected_input_size(netplay, devices);

            if (cmd_size != (2+input_size) * sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_INPUT received an unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (client_num >= MAX_CLIENTS || !(netplay->connected_players & (1<<client_num)))
            {
               RARCH_ERR("Invalid NETPLAY_CMD_INPUT player number.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Check the frame number only if they're not in slave mode */
            if (connection->mode == NETPLAY_CONNECTION_PLAYING)
            {
               if (frame_num < netplay->read_frame_count[client_num])
               {
                  uint32_t buf;
                  /* We already had this, so ignore the new transmission */
                  for (; input_size; input_size--)
                  {
                     RECV(&buf, sizeof(uint32_t))
                        return netplay_cmd_nak(netplay, connection);
                  }
                  break;
               }
               else if (frame_num > netplay->read_frame_count[client_num])
               {
                  /* Out of order = out of luck */
                  RARCH_ERR("Netplay input out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
            }

            /* The data's good! */
            dframe = &netplay->buffer[netplay->read_ptr[client_num]];
            if (!netplay_delta_frame_ready(netplay, dframe, netplay->read_frame_count[client_num]))
            {
               /* Hopefully we'll be ready after another round of input */
               goto shrt;
            }

            /* Copy in the input */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               netplay_input_state_t istate;
               uint32_t dsize, di;
               if (!(devices & (1<<device)))
                  continue;

               dsize = netplay_expected_input_size(netplay, 1 << device);
               istate = netplay_input_state_for(&dframe->real_input[device],
                     client_num, dsize,
                     false /* Must be false because of slave-mode clients */,
                     false);
               if (!istate)
               {
                  /* Catastrophe! */
                  return netplay_cmd_nak(netplay, connection);
               }
               RECV(istate->data, dsize*sizeof(uint32_t))
                  return false;
               for (di = 0; di < dsize; di++)
                  istate->data[di] = ntohl(istate->data[di]);
            }
            dframe->have_real[client_num] = true;

            /* Slaves may go through several packets of data in the same frame
             * if latency is choppy, so we advance and send their data after
             * handling all network data this frame */
            if (connection->mode == NETPLAY_CONNECTION_PLAYING)
            {
               netplay->read_ptr[client_num] = NEXT_PTR(netplay->read_ptr[client_num]);
               netplay->read_frame_count[client_num]++;

               if (netplay->is_server)
               {
                  /* Forward it on if it's past data */
                  if (dframe->frame <= netplay->self_frame_count)
                     send_input_frame(netplay, dframe, NULL, connection, client_num, false);
               }
            }

            /* If this was server data, advance our server pointer too */
            if (!netplay->is_server && client_num == 0)
            {
               netplay->server_ptr = netplay->read_ptr[0];
               netplay->server_frame_count = netplay->read_frame_count[0];
            }

#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[netplay] Received input from %u\n", client_num);
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

            /* We already had this, so ignore the new transmission */
            if (frame < netplay->server_frame_count)
               break;

            if (frame != netplay->server_frame_count)
            {
               RARCH_ERR("NETPLAY_CMD_NOINPUT for invalid frame.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            netplay->server_ptr = NEXT_PTR(netplay->server_ptr);
            netplay->server_frame_count++;
#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[netplay] Received server noinput\n");
            print_state(netplay);
#endif
            break;
         }

      case NETPLAY_CMD_SPECTATE:
      {
         uint32_t client_num;

         if (!netplay->is_server)
         {
            RARCH_ERR("NETPLAY_CMD_SPECTATE from a server.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (cmd_size != 0)
         {
            RARCH_ERR("Unexpected payload in NETPLAY_CMD_SPECTATE.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
             connection->mode != NETPLAY_CONNECTION_SLAVE)
         {
            /* They were confused */
            return netplay_cmd_nak(netplay, connection);
         }

         client_num = (uint32_t)(connection - netplay->connections + 1);

         handle_play_spectate(netplay, client_num, connection, cmd, 0, NULL);
         break;
      }

      case NETPLAY_CMD_PLAY:
      {
         uint32_t client_num;
         uint32_t payload[1];

         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("Incorrect NETPLAY_CMD_PLAY payload size.\n");
            return netplay_cmd_nak(netplay, connection);
         }
         RECV(payload, sizeof(uint32_t))
         {
            RARCH_ERR("Failed to receive NETPLAY_CMD_PLAY payload.\n");
            return netplay_cmd_nak(netplay, connection);
         }

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

         /* They were obviously confused */
         if (
                  connection->mode == NETPLAY_CONNECTION_PLAYING
               || connection->mode == NETPLAY_CONNECTION_SLAVE)
            return netplay_cmd_nak(netplay, connection);

         client_num = (unsigned)(connection - netplay->connections + 1);

         handle_play_spectate(netplay, client_num, connection, cmd, cmd_size, payload);
         break;
      }

      case NETPLAY_CMD_MODE:
      {
         uint32_t payload[15];
         uint32_t frame, mode, client_num, devices, device;
         size_t ptr;
         struct delta_frame *dframe;
         const char *nick;

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

         if (netplay->is_server)
         {
            RARCH_ERR("NETPLAY_CMD_MODE from client.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (cmd_size != sizeof(payload))
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
         client_num = mode & 0xFFFF;
         if (client_num >= MAX_CLIENTS)
         {
            RARCH_ERR("Received NETPLAY_CMD_MODE for a higher player number than we support.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         devices = ntohl(payload[2]);
         memcpy(netplay->device_share_modes, payload + 3, sizeof(netplay->device_share_modes));
         nick = (const char *) (payload + 7);

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
                  netplay->self_mode = NETPLAY_CONNECTION_SLAVE;
               else
                  netplay->self_mode = NETPLAY_CONNECTION_PLAYING;

               netplay->connected_players |= (1<<client_num);
               netplay->client_devices[client_num] = devices;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  if (devices & (1<<device))
                     netplay->device_clients[device] |= (1<<client_num);
               netplay->self_devices = devices;

               netplay->read_ptr[client_num] = netplay->server_ptr;
               netplay->read_frame_count[client_num] = netplay->server_frame_count;

               /* Fix up current frame info */
               if (!(mode & NETPLAY_CMD_MODE_BIT_SLAVE) && frame <= netplay->self_frame_count)
               {
                  /* It wanted past frames, better send 'em! */
                  START(netplay->server_ptr);
                  while (dframe->used && dframe->frame <= netplay->self_frame_count)
                  {
                     for (device = 0; device < MAX_INPUT_DEVICES; device++)
                     {
                        uint32_t dsize;
                        netplay_input_state_t istate;
                        if (!(devices & (1<<device)))
                           continue;
                        dsize = netplay_expected_input_size(netplay, 1 << device);
                        istate = netplay_input_state_for(
                              &dframe->real_input[device], client_num, dsize,
                              false, false);
                        if (!istate)
                           continue;
                        memset(istate->data, 0, dsize*sizeof(uint32_t));
                     }
                     dframe->have_local = true;
                     dframe->have_real[client_num] = true;
                     send_input_frame(netplay, dframe, connection, NULL, client_num, false);
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

                     dframe->have_local = true;

                     /* Go on to the next delta frame */
                     NEXT();
                     frame_count++;

                     if (frame_count >= frame)
                        break;
                  }

               }

               /* Announce it */
               announce_play_spectate(netplay, NULL, NETPLAY_CONNECTION_PLAYING, devices);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change self->%X\n", devices);
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
               netplay->connected_players &= ~(1<<client_num);
               netplay->client_devices[client_num] = 0;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  netplay->device_clients[device] &= ~(1<<client_num);

               /* Announce it */
               announce_play_spectate(netplay, NULL, NETPLAY_CONNECTION_SPECTATING, 0);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change self->spectating\n");
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

               netplay->connected_players |= (1<<client_num);
               netplay->client_devices[client_num] = devices;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  if (devices & (1<<device))
                     netplay->device_clients[device] |= (1<<client_num);

               netplay->read_ptr[client_num] = netplay->server_ptr;
               netplay->read_frame_count[client_num] = netplay->server_frame_count;

               /* Announce it */
               announce_play_spectate(netplay, nick, NETPLAY_CONNECTION_PLAYING, devices);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change %u->%u\n", client_num, devices);
               print_state(netplay);
#endif

            }
            else
            {
               netplay->connected_players &= ~(1<<client_num);
               netplay->client_devices[client_num] = 0;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  netplay->device_clients[device] &= ~(1<<client_num);

               /* Announce it */
               announce_play_spectate(netplay, nick, NETPLAY_CONNECTION_SPECTATING, 0);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change %u->spectator\n", client_num);
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

               case NETPLAY_CMD_MODE_REFUSED_REASON_NOT_AVAILABLE:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE);
                  break;

               default:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY);
            }

            if (dmsg)
            {
               RARCH_LOG("[netplay] %s\n", dmsg);
               runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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

            /* Oh well, we got rid of it! */
            if (!found)
               break;

            if (buffer[0] <= netplay->other_frame_count)
            {
               /* We've already replayed up to this frame, so we can check it
                * directly */
               uint32_t local_crc = netplay_delta_frame_crc(
                     netplay, &netplay->buffer[tmp_ptr]);

               /* Problem! */
               if (buffer[1] != local_crc)
                  netplay_cmd_request_savestate(netplay);
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
            uint32_t client;
            uint32_t load_frame_count;
            size_t load_ptr;
            struct compression_transcoder *ctrans = NULL;
            uint32_t                   client_num = (uint32_t)
             (connection - netplay->connections + 1);

            /* Make sure we're ready for it */
            if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            {
               if (!netplay->is_replay)
               {
                  netplay->is_replay          = true;
                  netplay->replay_ptr         = netplay->run_ptr;
                  netplay->replay_frame_count = netplay->run_frame_count;
                  netplay_wait_and_init_serialization(netplay);
                  netplay->is_replay         = false;
               }
               else
                  netplay_wait_and_init_serialization(netplay);
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

            if (netplay->is_server)
            {
               load_ptr = netplay->read_ptr[client_num];
               load_frame_count = netplay->read_frame_count[client_num];
            }
            else
            {
               load_ptr = netplay->server_ptr;
               load_frame_count = netplay->server_frame_count;
            }

            if (frame != load_frame_count)
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE loading a state out of order!\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (!netplay_delta_frame_ready(netplay, &netplay->buffer[load_ptr], load_frame_count))
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
                  (uint8_t*)netplay->buffer[load_ptr].state,
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
            if (load_frame_count > netplay->run_frame_count ||
                cmd == NETPLAY_CMD_RESET)
            {
               /* This is squirrely: We need to assure that when we advance the
                * frame in post_frame, THEN we're referring to the frame to
                * load into. If we refer directly to read_ptr, then we'll end
                * up never reading the input for read_frame_count itself, which
                * will make the other side unhappy. */
               netplay->run_ptr           = PREV_PTR(load_ptr);
               netplay->run_frame_count   = load_frame_count - 1;
               if (frame > netplay->self_frame_count)
               {
                  netplay->self_ptr         = netplay->run_ptr;
                  netplay->self_frame_count = netplay->run_frame_count;
               }
            }

            /* Don't expect earlier data from other clients */
            for (client = 0; client < MAX_CLIENTS; client++)
            {
               if (!(netplay->connected_players & (1<<client)))
                  continue;

               if (frame > netplay->read_frame_count[client])
               {
                  netplay->read_ptr[client] = load_ptr;
                  netplay->read_frame_count[client] = load_frame_count;
               }
            }

            /* Make sure our states are correct */
            netplay->savestate_request_outstanding = false;
            netplay->other_ptr                     = load_ptr;
            netplay->other_frame_count             = load_frame_count;

#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[netplay] Loading state at %u\n", load_frame_count);
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
            RARCH_LOG("[netplay] %s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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

            RARCH_LOG("[netplay] Network is stalling at frame %u, count %u of %d ...\n",
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
   struct delta_frame *oframe, *frame = &netplay->buffer[netplay->self_ptr];
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active &&
          connection->mode == NETPLAY_CONNECTION_SLAVE)
      {
         uint32_t devices, device;
         uint32_t client_num = (uint32_t)(i + 1);

         /* This is a slave connection. First, should we do anything at all? If
          * we've already "read" this data, then we can just ignore it */
         if (netplay->read_frame_count[client_num] > netplay->self_frame_count)
            continue;

         /* Alright, we have to send something. Do we need to generate it first? */
         if (!frame->have_real[client_num])
         {
            devices = netplay->client_devices[client_num];

            /* Copy the previous frame's data */
            oframe = &netplay->buffer[PREV_PTR(netplay->self_ptr)];
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               netplay_input_state_t istate_out, istate_in;
               if (!(devices & (1<<device)))
                  continue;
               istate_in = oframe->real_input[device];
               while (istate_in && istate_in->client_num != client_num)
                  istate_in = istate_in->next;
               if (!istate_in)
               {
                  /* Start with blank input */
                  netplay_input_state_for(&frame->real_input[device],
                        client_num,
                        netplay_expected_input_size(netplay, 1 << device), true,
                        false);

               }
               else
               {
                  /* Copy the previous input */
                  istate_out = netplay_input_state_for(&frame->real_input[device],
                        client_num, istate_in->size, true, false);
                  memcpy(istate_out->data, istate_in->data,
                        istate_in->size * sizeof(uint32_t));
               }
            }
            frame->have_real[client_num] = true;
         }

         /* Send it along */
         send_input_frame(netplay, frame, NULL, NULL, client_num, false);

         /* And mark it as "read" */
         netplay->read_ptr[client_num] = NEXT_PTR(netplay->self_ptr);
         netplay->read_frame_count[client_num] = netplay->self_frame_count + 1;
      }
   }
}

/**
 * netplay_announce_nat_traversal
 *
 * Announce successful NAT traversal.
 */
void netplay_announce_nat_traversal(netplay_t *netplay)
{
#ifndef HAVE_SOCKET_LEGACY
   char msg[4200], host[PATH_MAX_LENGTH], port[6];

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
   {
      snprintf(msg, sizeof(msg), "%s\n",
            msg_hash_to_str(MSG_UPNP_FAILED));
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("[netplay] %s\n", msg);
      return;
   }

   snprintf(msg, sizeof(msg), "%s: %s:%s\n",
         msg_hash_to_str(MSG_PUBLIC_ADDRESS),
         host, port);
   runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[netplay] %s\n", msg);
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
