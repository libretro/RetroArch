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
#include <sys/types.h>

#include <boolean.h>
#include <compat/strl.h>
#include <retro_assert.h>
#include <string/stdstring.h>

#include "netplay_private.h"

#include "../../configuration.h"
#include "../../input/input_driver.h"
#include "../../runloop.h"

#include "../../tasks/tasks_internal.h"
#include <file/file_path.h>
#include "../../file_path_special.h"
#include "paths.h"

/* Only used before init_netplay */
static bool netplay_enabled = false;
static bool netplay_is_client = false;

/* Used while Netplay is running */
static netplay_t *netplay_data = NULL;

/* Used to avoid recursive netplay calls */
static bool in_netplay = false;

/* Used for deferred netplay initialization */
static bool      netplay_client_deferred = false;
static char      server_address_deferred[512] = "";
static unsigned  server_port_deferred = 0;

/**
 * netplay_is_alive:
 * @netplay              : pointer to netplay object
 *
 * Checks if input port/index is controlled by netplay or not.
 *
 * Returns: true (1) if alive, otherwise false (0).
 **/
static bool netplay_is_alive(void)
{
   if (!netplay_data)
      return false;
   return (netplay_data->is_server && !!netplay_data->connected_players) ||
          (!netplay_data->is_server && netplay_data->self_mode >= NETPLAY_CONNECTION_CONNECTED);
}

/**
 * netplay_should_skip:
 * @netplay              : pointer to netplay object
 *
 * If we're fast-forward replaying to resync, check if we 
 * should actually show frame.
 *
 * Returns: bool (1) if we should skip this frame, otherwise
 * false (0).
 **/
static bool netplay_should_skip(netplay_t *netplay)
{
   if (!netplay)
      return false;
   return netplay->is_replay && (netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED);
}

/**
 * netplay_can_poll
 *
 * Just a frontend for netplay->can_poll that handles netplay==NULL
 */
static bool netplay_can_poll(netplay_t *netplay)
{
   if (!netplay)
      return false;
   return netplay->can_poll;
}

/**
 * get_self_input_state:
 * @netplay              : pointer to netplay object
 *
 * Grab our own input state and send this frame's input state (self and remote)
 * over the network
 *
 * Returns: true (1) if successful, otherwise false (0).
 */
static bool get_self_input_state(netplay_t *netplay)
{
   uint32_t state[WORDS_PER_INPUT] = {0, 0, 0};
   struct delta_frame *ptr = &netplay->buffer[netplay->self_ptr];
   size_t i;

   if (!netplay_delta_frame_ready(netplay, ptr, netplay->self_frame_count))
      return false;

   if (ptr->have_local)
   {
      /* We've already read this frame! */
      return true;
   }

   if (!input_driver_is_libretro_input_blocked() && netplay->self_frame_count > 0)
   {
      /* First frame we always give zero input since relying on 
       * input from first frame screws up when we use -F 0. */
      retro_input_state_t cb = netplay->cbs.state_cb;
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         int16_t tmp = cb(0,
               RETRO_DEVICE_JOYPAD, 0, i);
         state[0] |= tmp ? 1 << i : 0;
      }

      for (i = 0; i < 2; i++)
      {
         int16_t tmp_x = cb(0,
               RETRO_DEVICE_ANALOG, i, 0);
         int16_t tmp_y = cb(0,
               RETRO_DEVICE_ANALOG, i, 1);
         state[1 + i] = (uint16_t)tmp_x | (((uint16_t)tmp_y) << 16);
      }
   }

   memcpy(ptr->self_state, state, sizeof(state));
   ptr->have_local = true;

   /* If we're playing, copy it in as real input */
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
   {
      memcpy(ptr->real_input_state[netplay->self_player], state,
         sizeof(state));
      ptr->have_real[netplay->self_player] = true;
   }

   /* And send this input to our peers */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
         netplay_send_cur_input(netplay, &netplay->connections[i]);
   }

   return true;
}

bool init_netplay_deferred(const char* server, unsigned port)
{
   if (!string_is_empty(server) && port != 0)
   {
      strlcpy(server_address_deferred, server, sizeof(server_address_deferred));
      server_port_deferred = port;
      netplay_client_deferred = true;
   }
   else
      netplay_client_deferred = false;
   return netplay_client_deferred;
}


/**
 * netplay_poll:
 * @netplay              : pointer to netplay object
 *
 * Polls network to see if we have anything new. If our 
 * network buffer is full, we simply have to block 
 * for new input data.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool netplay_poll(void)
{
   int res;
   uint32_t player;
   size_t i;

   netplay_data->can_poll = false;

   get_self_input_state(netplay_data);

   /* If we're not connected, we're done */
   if (netplay_data->self_mode == NETPLAY_CONNECTION_NONE)
      return true;

   /* Read Netplay input, block if we're configured to stall for input every
    * frame */
   netplay_update_unread_ptr(netplay_data);
   if (netplay_data->stateless_mode &&
       netplay_data->connected_players &&
       netplay_data->unread_frame_count <= netplay_data->self_frame_count)
      res = netplay_poll_net_input(netplay_data, true);
   else
      res = netplay_poll_net_input(netplay_data, false);
   if (res == -1)
   {
      /* Catastrophe! */
      for (i = 0; i < netplay_data->connections_size; i++)
         netplay_hangup(netplay_data, &netplay_data->connections[i]);
      return false;
   }

   /* Simulate the input if we don't have real input */
   netplay_simulate_input(netplay_data, netplay_data->self_ptr, false);

   /* Consider stalling */
   switch (netplay_data->stall)
   {
      case NETPLAY_STALL_RUNNING_FAST:
      {
         netplay_update_unread_ptr(netplay_data);
         if (netplay_data->unread_frame_count + NETPLAY_MAX_STALL_FRAMES - 2
               > netplay_data->self_frame_count)
         {
            netplay_data->stall = NETPLAY_STALL_NONE;
            for (i = 0; i < netplay_data->connections_size; i++)
            {
               struct netplay_connection *connection = &netplay_data->connections[i];
               if (connection->active && connection->stall)
                  connection->stall = NETPLAY_STALL_NONE;
            }
         }
         break;
      }

      case NETPLAY_STALL_SERVER_REQUESTED:
      {
         /* See if the stall is done */
         if (netplay_data->connections[0].stall_frame == 0)
         {
            /* Stop stalling! */
            netplay_data->connections[0].stall = NETPLAY_STALL_NONE;
            netplay_data->stall = NETPLAY_STALL_NONE;
         }
         else
         {
            netplay_data->connections[0].stall_frame--;
         }
         break;
      }

      case NETPLAY_STALL_NO_CONNECTION:
         /* We certainly haven't fixed this */
         break;

      default: /* not stalling */
      {
         /* Are we too far ahead? */
         netplay_update_unread_ptr(netplay_data);
         if (netplay_data->unread_frame_count + NETPLAY_MAX_STALL_FRAMES
               <= netplay_data->self_frame_count)
         {
            netplay_data->stall      = NETPLAY_STALL_RUNNING_FAST;
            netplay_data->stall_time = cpu_features_get_time_usec();

            /* Figure out who to blame */
            if (netplay_data->is_server)
            {
               for (player = 0; player < MAX_USERS; player++)
               {
                  if (!(netplay_data->connected_players & (1<<player))) continue;
                  if (netplay_data->read_frame_count[player] > netplay_data->unread_frame_count) continue;
                  for (i = 0; i < netplay_data->connections_size; i++)
                  {
                     struct netplay_connection *connection = &netplay_data->connections[i];
                     if (connection->active &&
                         connection->mode == NETPLAY_CONNECTION_PLAYING &&
                         connection->player == player)
                     {
                        connection->stall = NETPLAY_STALL_RUNNING_FAST;
                        connection->stall_time = netplay_data->stall_time;
                        break;
                     }
                  }
               }
            }

         }
      }
   }

   /* If we're stalling, consider disconnection */
   if (netplay_data->stall && netplay_data->stall_time)
   {
      retro_time_t now = cpu_features_get_time_usec();

      /* Don't stall out while they're paused */
      if (netplay_data->remote_paused)
         netplay_data->stall_time = now;
      else if (now - netplay_data->stall_time >=
               (netplay_data->is_server ? MAX_SERVER_STALL_TIME_USEC :
                                          MAX_CLIENT_STALL_TIME_USEC))
      {
         /* Stalled out! */
         if (netplay_data->is_server)
         {
            for (i = 0; i < netplay_data->connections_size; i++)
            {
               struct netplay_connection *connection = &netplay_data->connections[i];
               if (connection->active &&
                   connection->mode == NETPLAY_CONNECTION_PLAYING &&
                   connection->stall &&
                   now - connection->stall_time >= MAX_SERVER_STALL_TIME_USEC)
               {
                  netplay_hangup(netplay_data, connection);
               }
            }
         }
         else
         {
            netplay_hangup(netplay_data, &netplay_data->connections[0]);
         }
         return false;
      }
   }

   return true;
}

/**
 * input_poll_net
 *
 * Poll the network if necessary.
 */
void input_poll_net(void)
{
   if (!netplay_should_skip(netplay_data) && netplay_can_poll(netplay_data))
      netplay_poll();
}

/* Netplay polling callbacks */
void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   if (!netplay_should_skip(netplay_data))
      netplay_data->cbs.frame_cb(data, width, height, pitch);
}

void audio_sample_net(int16_t left, int16_t right)
{
   if (!netplay_should_skip(netplay_data) && !netplay_data->stall)
      netplay_data->cbs.sample_cb(left, right);
}

size_t audio_sample_batch_net(const int16_t *data, size_t frames)
{
   if (!netplay_should_skip(netplay_data) && !netplay_data->stall)
      return netplay_data->cbs.sample_batch_cb(data, frames);
   return frames;
}

static int16_t netplay_input_state(netplay_t *netplay,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   size_t ptr = netplay->is_replay ? 
      netplay->replay_ptr : netplay->self_ptr;

   const uint32_t *curr_input_state = NULL;

   if (port <= 1)
   {
      /* Possibly flip the port */
      if (netplay_flip_port(netplay))
         port ^= 1;
   }
   else if (port >= MAX_USERS)
   {
      return 0;
   }

   if (port > netplay->player_max)
      netplay->player_max = port;

   if (netplay->buffer[ptr].have_real[port])
   {
      netplay->buffer[ptr].used_real[port] = true;
      curr_input_state = netplay->buffer[ptr].real_input_state[port];
   }
   else
   {
      curr_input_state = netplay->buffer[ptr].simulated_input_state[port];
   }

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return ((1 << id) & curr_input_state[0]) ? 1 : 0;

      case RETRO_DEVICE_ANALOG:
      {
         uint32_t state = curr_input_state[1 + idx];
         return (int16_t)(uint16_t)(state >> (id * 16));
      }

      default:
         return 0;
   }
}

static int reannounce = 0;

static void netplay_announce_cb(void *task_data, void *user_data, const char *error)
{
   RARCH_LOG("Announcing netplay game... \n");
   return;
}

static void netplay_announce()
{
   char buf [2048];
   char url [2048]               = "http://lobby.libretro.com/raw/?";
   rarch_system_info_t *system   = NULL;
   settings_t *settings          = config_get_ptr();
   uint32_t *content_crc_ptr     = NULL;

   content_get_crc(&content_crc_ptr);

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   buf[0] = '\0';

   snprintf(buf, sizeof(buf), "%susername=%s&corename=%s&coreversion=%s&"
   "gamename=%s&gamecrc=%d&port=%d", 
      url, settings->username, system->info.library_name, 
      system->info.library_version, 
      path_basename(path_get(RARCH_PATH_BASENAME)),*content_crc_ptr,
      settings->netplay.port);

   task_push_http_transfer(buf, true, NULL, netplay_announce_cb, NULL);
}

int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   if (netplay_is_alive())
      return netplay_input_state(netplay_data, port, device, idx, id);
   return netplay_data->cbs.state_cb(port, device, idx, id);
}
/* ^^^ Netplay polling callbacks */

/**
 * netplay_command:
 * @netplay                : pointer to netplay object
 * @cmd                    : command to send
 * @data                   : data to send as argument
 * @sz                     : size of data
 * @command_str            : name of action
 * @success_msg            : message to display upon success
 * 
 * Sends a single netplay command and waits for response. Only actually used
 * for player flipping. FIXME: Should probably just be removed.
 */
bool netplay_command(netplay_t* netplay, struct netplay_connection *connection,
   enum netplay_cmd cmd, void* data, size_t sz, const char* command_str,
   const char* success_msg)
{
   retro_assert(netplay);

   if (!netplay_send_raw_cmd(netplay, connection, cmd, data, sz))
      return false;

   runloop_msg_queue_push(success_msg, 1, 180, false);

   return true;
}

/**
 * netplay_flip_users:
 * @netplay              : pointer to netplay object
 *
 * Flip who controls user 1 and 2.
 */
static void netplay_flip_users(netplay_t *netplay)
{
   /* Must be in the future because we may have 
    * already sent this frame's data */
   uint32_t     flip_frame = netplay->self_frame_count + 1;
   uint32_t flip_frame_net = htonl(flip_frame);
   size_t i;

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
      {
         netplay_command(netplay, connection, NETPLAY_CMD_FLIP_PLAYERS,
            &flip_frame_net, sizeof flip_frame_net, "flip users",
            "Successfully flipped users.\n");
      }
   }

   netplay->flip       ^= true;
   netplay->flip_frame  = flip_frame;
}

/**
 * netplay_frontend_paused
 * @netplay              : pointer to netplay object
 * @paused               : true if frontend is paused
 *
 * Inform Netplay of the frontend's pause state (paused or otherwise)
 */
static void netplay_frontend_paused(netplay_t *netplay, bool paused)
{
   size_t i;
   uint32_t paused_ct;

   /* Nothing to do if we already knew this */
   if (netplay->local_paused == paused)
      return;

   netplay->local_paused = paused;

   /* Communicating this is a bit odd: If exactly one other connection is
    * paused, then we must tell them that we're unpaused, as from their
    * perspective we are. If more than one other connection is paused, then our
    * status as proxy means we are NOT unpaused to either of them. */
   paused_ct = 0;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->paused)
         paused_ct++;
   }
   if (paused_ct > 1)
      return;

   /* Send our unpaused status. Must send manually because we must immediately
    * flush the buffer: If we're paused, we won't be polled. */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
      {
         if (paused)
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_PAUSE,
               netplay->nick, NETPLAY_NICK_LEN);
         else
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_RESUME,
               NULL, 0);

         /* We're not going to be polled, so we need to flush this command now */
         netplay_send_flush(&connection->send_packet_buffer, connection->fd, true);
      }
   }
}

/**
 * netplay_pre_frame:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay.
 * Call this before running retro_run().
 *
 * Returns: true (1) if the frontend is cleared to emulate the frame, false (0)
 * if we're stalled or paused
 **/
bool netplay_pre_frame(netplay_t *netplay)
{
   bool sync_stalled;
   reannounce ++;
   if (netplay->is_server && (reannounce % 3600 == 0))
      netplay_announce();
   retro_assert(netplay);

   /* FIXME: This is an ugly way to learn we're not paused anymore */
   if (netplay->local_paused)
      netplay_frontend_paused(netplay, false);

   if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
   {
      /* Are we ready now? */
      netplay_try_init_serialization(netplay);
   }

   if (netplay->is_server)
   {
      /* Advertise our server */
      netplay_lan_ad_server(netplay);

      /* NAT traversal if applicable */
      if (netplay->nat_traversal &&
          netplay->nat_traversal_state.request_outstanding &&
          !netplay->nat_traversal_state.have_inet4)
      {
         struct timeval tmptv = {0};
         fd_set fds = netplay->nat_traversal_state.fds;
         if (socket_select(netplay->nat_traversal_state.nfds, &fds, NULL, NULL, &tmptv) > 0)
            natt_read(&netplay->nat_traversal_state);

#ifndef HAVE_SOCKET_LEGACY
         if (!netplay->nat_traversal_state.request_outstanding ||
             netplay->nat_traversal_state.have_inet4)
            netplay_announce_nat_traversal(netplay);
#endif
      }
   }

   sync_stalled = !netplay_sync_pre_frame(netplay);

   if (sync_stalled ||
       (netplay->connected_players &&
        (netplay->stall || netplay->remote_paused)))
   {
      /* We may have received data even if we're stalled, so run post-frame
       * sync */
      netplay_sync_post_frame(netplay, true);
      return false;
   }
   return true;
}

/**
 * netplay_post_frame:   
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay.
 * We check if we have new input and replay from recorded input.
 * Call this after running retro_run().
 **/
void netplay_post_frame(netplay_t *netplay)
{
   size_t i;
   retro_assert(netplay);
   netplay_update_unread_ptr(netplay);
   netplay_sync_post_frame(netplay, false);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active &&
          !netplay_send_flush(&connection->send_packet_buffer, connection->fd,
            false))
         netplay_hangup(netplay, &netplay->connections[0]);
   }
}

/**
 * netplay_send_savestate
 * @netplay              : pointer to netplay object
 * @serial_info          : the savestate being loaded
 * @cx                   : compression type
 * @z                    : compression backend to use
 *
 * Send a loaded savestate to those connected peers using the given compression
 * scheme.
 */
void netplay_send_savestate(netplay_t *netplay,
   retro_ctx_serialize_info_t *serial_info, uint32_t cx,
   struct compression_transcoder *z)
{
   uint32_t header[4];
   uint32_t rd, wn;
   size_t i;

   /* Compress it */
   z->compression_backend->set_in(z->compression_stream,
      (const uint8_t*)serial_info->data_const, serial_info->size);
   z->compression_backend->set_out(z->compression_stream,
      netplay->zbuffer, netplay->zbuffer_size);
   if (!z->compression_backend->trans(z->compression_stream, true, &rd,
         &wn, NULL))
   {
      /* Catastrophe! */
      for (i = 0; i < netplay->connections_size; i++)
         netplay_hangup(netplay, &netplay->connections[i]);
      return;
   }

   /* Send it to relevant peers */
   header[0] = htonl(NETPLAY_CMD_LOAD_SAVESTATE);
   header[1] = htonl(wn + 2*sizeof(uint32_t));
   header[2] = htonl(netplay->self_frame_count);
   header[3] = htonl(serial_info->size);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (!connection->active ||
          connection->mode < NETPLAY_CONNECTION_CONNECTED ||
          connection->compression_supported != cx) continue;

      if (!netplay_send(&connection->send_packet_buffer, connection->fd, header,
            sizeof(header)) ||
          !netplay_send(&connection->send_packet_buffer, connection->fd,
            netplay->zbuffer, wn))
         netplay_hangup(netplay, connection);
   }
}

/**
 * netplay_load_savestate
 * @netplay              : pointer to netplay object
 * @serial_info          : the savestate being loaded, NULL means 
 *                         "load it yourself"
 * @save                 : Whether to save the provided serial_info 
 *                         into the frame buffer
 *
 * Inform Netplay of a savestate load and send it to the other side
 **/
void netplay_load_savestate(netplay_t *netplay,
      retro_ctx_serialize_info_t *serial_info, bool save)
{
   retro_ctx_serialize_info_t tmp_serial_info;

   /* Record it in our own buffer */
   if (save || !serial_info)
   {
      if (netplay_delta_frame_ready(netplay,
               &netplay->buffer[netplay->self_ptr], netplay->self_frame_count))
      {
         if (!serial_info)
         {
            tmp_serial_info.size = netplay->state_size;
            tmp_serial_info.data = netplay->buffer[netplay->self_ptr].state;
            if (!core_serialize(&tmp_serial_info))
               return;
            tmp_serial_info.data_const = tmp_serial_info.data;
            serial_info = &tmp_serial_info;
         }
         else
         {
            if (serial_info->size <= netplay->state_size)
            {
               memcpy(netplay->buffer[netplay->self_ptr].state,
                     serial_info->data_const, serial_info->size);
            }
         }
      }
      else
      {
         /* FIXME: This is a critical failure! */
         return;
      }
   }

   /* We need to ignore any intervening data from the other side, 
    * and never rewind past this */
   netplay_update_unread_ptr(netplay);
   if (netplay->unread_frame_count < netplay->self_frame_count)
   {
      uint32_t player;
      for (player = 0; player < MAX_USERS; player++)
      {
         if (!(netplay->connected_players & (1<<player))) continue;
         if (netplay->read_frame_count[player] < netplay->self_frame_count)
         {
            netplay->read_ptr[player] = netplay->self_ptr;
            netplay->read_frame_count[player] = netplay->self_frame_count;
         }
      }
      if (netplay->server_frame_count < netplay->self_frame_count)
      {
         netplay->server_ptr = netplay->self_ptr;
         netplay->server_frame_count = netplay->self_frame_count;
      }
      netplay_update_unread_ptr(netplay);
   }
   if (netplay->other_frame_count < netplay->self_frame_count)
   {
      netplay->other_ptr = netplay->self_ptr;
      netplay->other_frame_count = netplay->self_frame_count;
   }

   /* If we can't send it to the peer, loading a state was a bad idea */
   if (netplay->quirks & (
              NETPLAY_QUIRK_NO_SAVESTATES
            | NETPLAY_QUIRK_NO_TRANSMISSION))
      return;

   /* Send this to every peer */
   if (netplay->compress_nil.compression_backend)
      netplay_send_savestate(netplay, serial_info, 0, &netplay->compress_nil);
   if (netplay->compress_zlib.compression_backend)
      netplay_send_savestate(netplay, serial_info, NETPLAY_COMPRESSION_ZLIB,
         &netplay->compress_zlib);
}

/**
 * netplay_toggle_play_spectate
 *
 * Toggle between play mode and spectate mode
 */
static void netplay_toggle_play_spectate(netplay_t *netplay)
{
   if (netplay->is_server)
   {
      /* FIXME: Duplication */
      uint32_t payload[2];
      char msg[512];
      const char *dmsg = NULL;
      payload[0] = htonl(netplay->self_frame_count);
      if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
      {
         /* Mark us as no longer playing */
         payload[1] = htonl(netplay->self_player);
         netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;

         dmsg = msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME);

      }
      else if (netplay->self_mode == NETPLAY_CONNECTION_SPECTATING)
      {
         uint32_t player;

         /* Take a player number */
         for (player = 0; player < MAX_USERS; player++)
            if (!(netplay->connected_players & (1<<player))) break;
         if (player == MAX_USERS) return; /* Failure! */

         payload[1] = htonl(NETPLAY_CMD_MODE_BIT_PLAYING | player);
         netplay->self_mode = NETPLAY_CONNECTION_PLAYING;
         netplay->self_player = player;

         dmsg = msg;
         msg[sizeof(msg)-1] = '\0';
         snprintf(msg, sizeof(msg)-1, msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N), player+1);
      }

      RARCH_LOG("%s\n", dmsg);
      runloop_msg_queue_push(dmsg, 1, 180, false);

      netplay_send_raw_cmd_all(netplay, NULL, NETPLAY_CMD_MODE, payload, sizeof(payload));

   }
   else
   {
      uint32_t cmd;

      if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
      {
         /* Switch to spectator mode immediately */
         netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
         cmd = NETPLAY_CMD_SPECTATE;
      }
      else if (netplay->self_mode == NETPLAY_CONNECTION_SPECTATING)
      {
         /* Switch only after getting permission */
         cmd = NETPLAY_CMD_PLAY;
      }
      else return;

      netplay_send_raw_cmd_all(netplay, NULL, cmd, NULL, 0);
   }
}

/**
 * netplay_disconnect
 * @netplay              : pointer to netplay object
 *
 * Disconnect netplay.
 *
 * Returns: true (1) if successful. At present, cannot fail.
 **/
bool netplay_disconnect(netplay_t *netplay)
{
   size_t i;
   if (!netplay)
      return true;
   for (i = 0; i < netplay->connections_size; i++)
      netplay_hangup(netplay, &netplay->connections[i]);
   return true;
}

void deinit_netplay(void)
{
   if (netplay_data)
      netplay_free(netplay_data);
   netplay_data = NULL;
   core_unset_netplay_callbacks();
}

/**
 * init_netplay
 * @direct_host          : Host to connect to directly, if applicable (client only)
 * @server               : server address to connect to (client only)
 * @port                 : TCP port to host on/connect to
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool init_netplay(void *direct_host, const char *server, unsigned port)
{
   struct retro_callbacks cbs    = {0};
   settings_t *settings          = config_get_ptr();
   uint64_t serialization_quirks = 0;
   uint64_t quirks               = 0;

   if (!netplay_enabled)
      return false;

   core_set_default_callbacks(&cbs);
   if (!core_set_netplay_callbacks())
      return false;

   /* Map the core's quirks to our quirks */
   serialization_quirks = core_serialization_quirks();
   if (serialization_quirks & ~((uint64_t) NETPLAY_QUIRK_MAP_UNDERSTOOD))
   {
      /* Quirks we don't support! Just disable everything. */
      quirks |= NETPLAY_QUIRK_NO_SAVESTATES;
   }
   if (serialization_quirks & NETPLAY_QUIRK_MAP_NO_SAVESTATES)
      quirks |= NETPLAY_QUIRK_NO_SAVESTATES;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_NO_TRANSMISSION)
      quirks |= NETPLAY_QUIRK_NO_TRANSMISSION;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_INITIALIZATION)
      quirks |= NETPLAY_QUIRK_INITIALIZATION;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_ENDIAN_DEPENDENT)
      quirks |= NETPLAY_QUIRK_ENDIAN_DEPENDENT;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_PLATFORM_DEPENDENT)
      quirks |= NETPLAY_QUIRK_PLATFORM_DEPENDENT;

   if (netplay_is_client)
   {
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_CONNECTING_TO_NETPLAY_HOST));
   }
   else
   {
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_WAITING_FOR_CLIENT));
      runloop_msg_queue_push(
         msg_hash_to_str(MSG_WAITING_FOR_CLIENT),
         0, 180, false);

      netplay_announce();
   }

   netplay_data = (netplay_t*)netplay_new(
         netplay_is_client ? direct_host : NULL,
         netplay_is_client ? (!netplay_client_deferred ? server : server_address_deferred) : NULL,
         port ? ( !netplay_client_deferred ? port : server_port_deferred) : RARCH_DEFAULT_PORT,
         settings->netplay.stateless_mode, settings->netplay.check_frames, &cbs,
         settings->netplay.nat_traversal, settings->username,
         quirks);

   if (netplay_data)
      return true;

   RARCH_WARN("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_NETPLAY_FAILED),
         0, 180, false);
   return false;
}



/**
 * netplay_driver_ctl
 * 
 * Frontend access to Netplay functionality
 */
bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   bool ret = true;

   if (in_netplay)
      return true;
   in_netplay = true;

   if (!netplay_data)
   {
      switch (state)
      {
         case RARCH_NETPLAY_CTL_ENABLE_SERVER:
            netplay_enabled = true;
            netplay_is_client = false;
            goto done;

         case RARCH_NETPLAY_CTL_ENABLE_CLIENT:
            netplay_enabled = true;
            netplay_is_client = true;
            break;

         case RARCH_NETPLAY_CTL_DISABLE:
            netplay_enabled = false;
            goto done;

         case RARCH_NETPLAY_CTL_IS_ENABLED:
            ret = netplay_enabled;
            goto done;

         case RARCH_NETPLAY_CTL_IS_DATA_INITED:
            ret = false;
            goto done;

         default:
            goto done;
      }
   }

   switch (state)
   {
      case RARCH_NETPLAY_CTL_ENABLE_SERVER:
      case RARCH_NETPLAY_CTL_ENABLE_CLIENT:
      case RARCH_NETPLAY_CTL_IS_DATA_INITED:
         goto done;
      case RARCH_NETPLAY_CTL_DISABLE:
         ret = false;
         goto done;
      case RARCH_NETPLAY_CTL_IS_ENABLED:
         goto done;
      case RARCH_NETPLAY_CTL_POST_FRAME:
         netplay_post_frame(netplay_data);
         break;
      case RARCH_NETPLAY_CTL_PRE_FRAME:
         ret = netplay_pre_frame(netplay_data);
         goto done;
      case RARCH_NETPLAY_CTL_FLIP_PLAYERS:
         {
            bool *state = (bool*)data;
            if (netplay_data->is_server && *state)
               netplay_flip_users(netplay_data);
         }
         break;
      case RARCH_NETPLAY_CTL_GAME_WATCH:
         netplay_toggle_play_spectate(netplay_data);
         break;
      case RARCH_NETPLAY_CTL_PAUSE:
         netplay_frontend_paused(netplay_data, true);
         break;
      case RARCH_NETPLAY_CTL_UNPAUSE:
         netplay_frontend_paused(netplay_data, false);
         break;
      case RARCH_NETPLAY_CTL_LOAD_SAVESTATE:
         netplay_load_savestate(netplay_data, (retro_ctx_serialize_info_t*)data, true);
         break;
      case RARCH_NETPLAY_CTL_DISCONNECT:
         ret = netplay_disconnect(netplay_data);
         goto done;
      default:
      case RARCH_NETPLAY_CTL_NONE:
         ret = false;
   }

done:
   in_netplay = false;
   return ret;
}
