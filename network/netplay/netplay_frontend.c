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

#include "../../version.h"

#include <boolean.h>
#include <compat/strl.h>
#include <retro_assert.h>
#include <string/stdstring.h>
#include <net/net_http.h>

#include <file/file_path.h>

#include "netplay_private.h"

#include "../../configuration.h"
#include "../../input/input_driver.h"
#include "../../tasks/tasks_internal.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../command.h"
#include "../../retroarch.h"

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

/* Used */
static int reannounce = 0;
static bool is_mitm = false;

bool netplay_disconnect(netplay_t *netplay);

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
               RETRO_DEVICE_JOYPAD, 0, (unsigned)i);
         state[0] |= tmp ? 1 << i : 0;
      }

      for (i = 0; i < 2; i++)
      {
         int16_t tmp_x = cb(0,
               RETRO_DEVICE_ANALOG, (unsigned)i, 0);
         int16_t tmp_y = cb(0,
               RETRO_DEVICE_ANALOG, (unsigned)i, 1);
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

   /* Handle any delayed state changes */
   if (netplay->is_server)
      netplay_delayed_state_change(netplay);

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

   if (!get_self_input_state(netplay_data))
      goto catastrophe;

   /* If we're not connected, we're done */
   if (netplay_data->self_mode == NETPLAY_CONNECTION_NONE)
      return true;

   /* Read Netplay input, block if we're configured to stall for input every
    * frame */
   netplay_update_unread_ptr(netplay_data);
   if (netplay_data->stateless_mode &&
       netplay_data->connected_players &&
       netplay_data->unread_frame_count <= netplay_data->run_frame_count)
      res = netplay_poll_net_input(netplay_data, true);
   else
      res = netplay_poll_net_input(netplay_data, false);
   if (res == -1)
      goto catastrophe;

   /* Simulate the input if we don't have real input */
   netplay_simulate_input(netplay_data, netplay_data->run_ptr, false);

   /* Handle any slaves */
   if (netplay_data->is_server && netplay_data->connected_slaves)
      netplay_handle_slaves(netplay_data);

   netplay_update_unread_ptr(netplay_data);

   /* Figure out how many frames of input latency we should be using to hide
    * network latency */
   if (netplay_data->frame_run_time_avg || netplay_data->stateless_mode)
   {
      /* FIXME: Using fixed 60fps for this calculation */
      unsigned frames_per_frame = netplay_data->frame_run_time_avg ?
                                  (16666/netplay_data->frame_run_time_avg) :
                                   0;
      unsigned frames_ahead = (netplay_data->run_frame_count > netplay_data->unread_frame_count) ?
                              (netplay_data->run_frame_count - netplay_data->unread_frame_count) :
                              0;
      settings_t *settings  = config_get_ptr();
      unsigned input_latency_frames_min = settings->uints.netplay_input_latency_frames_min;
      unsigned input_latency_frames_max = input_latency_frames_min + settings->uints.netplay_input_latency_frames_range;

      /* Assume we need a couple frames worth of time to actually run the
       * current frame */
      if (frames_per_frame > 2)
         frames_per_frame -= 2;
      else
         frames_per_frame = 0;

      /* Shall we adjust our latency? */
      if (netplay_data->stateless_mode)
      {
         /* In stateless mode, we adjust up if we're "close" and down if we
          * have a lot of slack */
         if (netplay_data->input_latency_frames < input_latency_frames_min ||
             (netplay_data->unread_frame_count == netplay_data->run_frame_count + 1 &&
              netplay_data->input_latency_frames < input_latency_frames_max))
         {
            netplay_data->input_latency_frames++;
         }
         else if (netplay_data->input_latency_frames > input_latency_frames_max ||
                  (netplay_data->unread_frame_count > netplay_data->run_frame_count + 2 &&
                   netplay_data->input_latency_frames > input_latency_frames_min))
         {
            netplay_data->input_latency_frames--;
         }

      }
      else if (netplay_data->input_latency_frames < input_latency_frames_min ||
               (frames_per_frame < frames_ahead &&
                netplay_data->input_latency_frames < input_latency_frames_max))
      {
         /* We can't hide this much network latency with replay, so hide some
          * with input latency */
         netplay_data->input_latency_frames++;
      }
      else if (netplay_data->input_latency_frames > input_latency_frames_max ||
               (frames_per_frame > frames_ahead + 2 &&
                netplay_data->input_latency_frames > input_latency_frames_min))
      {
         /* We don't need this much latency (any more) */
         netplay_data->input_latency_frames--;
      }
   }

   /* If we're stalled, consider unstalling */
   switch (netplay_data->stall)
   {
      case NETPLAY_STALL_RUNNING_FAST:
      {
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

      case NETPLAY_STALL_SPECTATOR_WAIT:
         if (netplay_data->self_mode == NETPLAY_CONNECTION_PLAYING || netplay_data->unread_frame_count > netplay_data->self_frame_count)
            netplay_data->stall = NETPLAY_STALL_NONE;
         break;

      case NETPLAY_STALL_INPUT_LATENCY:
         /* Just let it recalculate momentarily */
         netplay_data->stall = NETPLAY_STALL_NONE;
         break;

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
         break;
   }

   /* If we're not stalled, consider stalling */
   if (!netplay_data->stall)
   {
      /* Have we not read enough latency frames? */
      if (netplay_data->self_mode == NETPLAY_CONNECTION_PLAYING &&
          netplay_data->connected_players &&
          netplay_data->run_frame_count + netplay_data->input_latency_frames > netplay_data->self_frame_count)
      {
         netplay_data->stall = NETPLAY_STALL_INPUT_LATENCY;
         netplay_data->stall_time = 0;
      }

      /* Are we too far ahead? */
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

      /* If we're a spectator, are we ahead at all? */
      if (!netplay_data->is_server &&
          (netplay_data->self_mode == NETPLAY_CONNECTION_SPECTATING ||
           netplay_data->self_mode == NETPLAY_CONNECTION_SLAVE) &&
          netplay_data->unread_frame_count <= netplay_data->self_frame_count)
      {
         netplay_data->stall = NETPLAY_STALL_SPECTATOR_WAIT;
         netplay_data->stall_time = cpu_features_get_time_usec();
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
            goto catastrophe;
         return false;
      }
   }

   return true;

catastrophe:
   for (i = 0; i < netplay_data->connections_size; i++)
      netplay_hangup(netplay_data, &netplay_data->connections[i]);
   return false;
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
      netplay->replay_ptr : netplay->run_ptr;

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

static void netplay_announce_cb(void *task_data, void *user_data, const char *error)
{
   RARCH_LOG("[netplay] announcing netplay game... \n");

   if (task_data)
   {
      http_transfer_data_t *data = (http_transfer_data_t*)task_data;
      struct string_list *lines;
      unsigned i, ip_len, port_len;
      const char *mitm_ip = NULL;
      const char *mitm_port = NULL;
      char *buf;
      char *host_string;

      if (data->len == 0)
      {
         free(task_data);
         return;
      }

      buf = (char*)calloc(1, data->len + 1);

      memcpy(buf, data->data, data->len);

      lines = string_split(buf, "\n");

      if (lines->size == 0)
      {
         string_list_free(lines);
         free(buf);
         free(task_data);
         return;
      }

      for (i = 0; i < lines->size; i++)
      {
         const char *line = lines->elems[i].data;

         if (!strncmp(line, "mitm_ip=", 8))
            mitm_ip = line + 8;

         if (!strncmp(line, "mitm_port=", 10))
            mitm_port = line + 10;
      }

      if (mitm_ip && mitm_port)
      {
         RARCH_LOG("[netplay] joining relay server: %s:%s\n", mitm_ip, mitm_port);

         ip_len   = (unsigned)strlen(mitm_ip);
         port_len = (unsigned)strlen(mitm_port);

         /* Enable Netplay client mode */
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
         {
            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
            is_mitm = true;
         }

         netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

         host_string = (char*)calloc(1, ip_len + port_len + 2);

         memcpy(host_string, mitm_ip, ip_len);
         memcpy(host_string + ip_len, "|", 1);
         memcpy(host_string + ip_len + 1, mitm_port, port_len);

         /* Enable Netplay */
         command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, (void*)host_string);
         command_event(CMD_EVENT_NETPLAY_INIT, (void*)host_string);

         free(host_string);
      }

      string_list_free(lines);
      free(buf);
      free(task_data);
   }

   return;
}

static void netplay_announce(void)
{
   char buf [2048];
   char url [2048]               = "http://newlobby.libretro.com/add/";
   char *username                = NULL;
   char *corename                = NULL;
   char *gamename                = NULL;
   char *coreversion             = NULL;
   settings_t *settings          = config_get_ptr();
   rarch_system_info_t *system   = runloop_get_system_info();
   uint32_t content_crc          = content_get_crc();

   net_http_urlencode_full(&username, settings->paths.username);
   net_http_urlencode_full(&corename, system->info.library_name);
   net_http_urlencode_full(&gamename, 
      !string_is_empty(path_basename(path_get(RARCH_PATH_BASENAME))) ? 
      path_basename(path_get(RARCH_PATH_BASENAME)) : "N/A");
   net_http_urlencode_full(&coreversion, system->info.library_version);

   buf[0] = '\0';

   snprintf(buf, sizeof(buf), "username=%s&core_name=%s&core_version=%s&"
      "game_name=%s&game_crc=%08X&port=%d"
      "&has_password=%d&has_spectate_password=%d&force_mitm=%d&retroarch_version=%s",
      username, corename, coreversion, gamename, content_crc,
      settings->uints.netplay_port,
      *settings->paths.netplay_password ? 1 : 0,
      *settings->paths.netplay_spectate_password ? 1 : 0,
      settings->bools.netplay_use_mitm_server,
      PACKAGE_VERSION);
#if 0
   RARCH_LOG("[netplay] announcement URL: %s\n", buf);
#endif
   task_push_http_post_transfer(url, buf, true, NULL, netplay_announce_cb, NULL);

   free(username);
   free(corename);
   free(gamename);
   free(coreversion);
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
   settings_t *settings  = config_get_ptr();

   retro_assert(netplay);

   if (settings->bools.netplay_public_announce)
   {
      reannounce++;
      if ((netplay->is_server || is_mitm) && (reannounce % 600 == 0))
         netplay_announce();
   }
   else
   {
      /* Make sure that if announcement is turned on mid-game, it gets announced */
      reannounce = -1;
   }

   /* FIXME: This is an ugly way to learn we're not paused anymore */
   if (netplay->local_paused)
      netplay_frontend_paused(netplay, false);

   if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
   {
      /* Are we ready now? */
      netplay_try_init_serialization(netplay);
   }

   if (netplay->is_server && !settings->bools.netplay_use_mitm_server)
   {
      /* Advertise our server */
      netplay_lan_ad_server(netplay);

      /* NAT traversal if applicable */
      if (netplay->nat_traversal &&
          !netplay->nat_traversal_task_oustanding &&
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

   /* If we're disconnected, deinitialize */
   if (!netplay->is_server && !netplay->connections[0].active)
   {
      netplay_disconnect(netplay);
      return true;
   }

   if (sync_stalled ||
       ((!netplay->is_server || netplay->connected_players) &&
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
         netplay_hangup(netplay, connection);
   }

   /* If we're disconnected, deinitialize */
   if (!netplay->is_server && !netplay->connections[0].active)
      netplay_disconnect(netplay);
}

/**
 * netplay_force_future
 * @netplay              : pointer to netplay object
 *
 * Force netplay to ignore all past input, typically because we've just loaded
 * a state or reset.
 */
static void netplay_force_future(netplay_t *netplay)
{
   /* Wherever we're inputting, that's where we consider our state to be loaded */
   netplay->run_ptr = netplay->self_ptr;
   netplay->run_frame_count = netplay->self_frame_count;

   /* We need to ignore any intervening data from the other side,
    * and never rewind past this */
   netplay_update_unread_ptr(netplay);
   if (netplay->unread_frame_count < netplay->run_frame_count)
   {
      uint32_t player;
      for (player = 0; player < MAX_USERS; player++)
      {
         if (!(netplay->connected_players & (1<<player))) continue;
         if (netplay->read_frame_count[player] < netplay->run_frame_count)
         {
            netplay->read_ptr[player] = netplay->run_ptr;
            netplay->read_frame_count[player] = netplay->run_frame_count;
         }
      }
      if (netplay->server_frame_count < netplay->run_frame_count)
      {
         netplay->server_ptr = netplay->run_ptr;
         netplay->server_frame_count = netplay->run_frame_count;
      }
      netplay_update_unread_ptr(netplay);
   }
   if (netplay->other_frame_count < netplay->run_frame_count)
   {
      netplay->other_ptr = netplay->run_ptr;
      netplay->other_frame_count = netplay->run_frame_count;
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
      (const uint8_t*)serial_info->data_const, (uint32_t)serial_info->size);
   z->compression_backend->set_out(z->compression_stream,
      netplay->zbuffer, (uint32_t)netplay->zbuffer_size);
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
   header[2] = htonl(netplay->run_frame_count);
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

   netplay_force_future(netplay);

   /* Record it in our own buffer */
   if (save || !serial_info)
   {
      if (netplay_delta_frame_ready(netplay,
               &netplay->buffer[netplay->run_ptr], netplay->run_frame_count))
      {
         if (!serial_info)
         {
            tmp_serial_info.size = netplay->state_size;
            tmp_serial_info.data = netplay->buffer[netplay->run_ptr].state;
            if (!core_serialize(&tmp_serial_info))
               return;
            tmp_serial_info.data_const = tmp_serial_info.data;
            serial_info = &tmp_serial_info;
         }
         else
         {
            if (serial_info->size <= netplay->state_size)
            {
               memcpy(netplay->buffer[netplay->run_ptr].state,
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

   /* Don't send it if we're expected to be desynced */
   if (netplay->desync)
      return;

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
 * netplay_core_reset
 * @netplay              : pointer to netplay object
 *
 * Indicate that the core has been reset to netplay peers
 **/
static void netplay_core_reset(netplay_t *netplay)
{
   uint32_t cmd[3];
   size_t i;

   /* Ignore past input */
   netplay_force_future(netplay);

   /* Request that our peers reset */
   cmd[0] = htonl(NETPLAY_CMD_RESET);
   cmd[1] = htonl(sizeof(uint32_t));
   cmd[2] = htonl(netplay->self_frame_count);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (!connection->active ||
            connection->mode < NETPLAY_CONNECTION_CONNECTED) continue;

      if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmd,
               sizeof(cmd)))
         netplay_hangup(netplay, connection);
   }
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
      if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING ||
          netplay->self_mode == NETPLAY_CONNECTION_SLAVE)
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

      RARCH_LOG("[netplay] %s\n", dmsg);
      runloop_msg_queue_push(dmsg, 1, 180, false);

      netplay_send_raw_cmd_all(netplay, NULL, NETPLAY_CMD_MODE, payload, sizeof(payload));

   }
   else
   {
      uint32_t cmd;

      if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING ||
          netplay->self_mode == NETPLAY_CONNECTION_SLAVE)
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

   deinit_netplay();
   return true;
}

void deinit_netplay(void)
{
   if (netplay_data)
   {
      netplay_free(netplay_data);
      netplay_enabled = false;
      netplay_is_client = false;
      is_mitm = false;
   }
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
      RARCH_LOG("[netplay] %s\n", msg_hash_to_str(MSG_CONNECTING_TO_NETPLAY_HOST));
   }
   else
   {
      RARCH_LOG("[netplay] %s\n", msg_hash_to_str(MSG_WAITING_FOR_CLIENT));
      runloop_msg_queue_push(
         msg_hash_to_str(MSG_WAITING_FOR_CLIENT),
         0, 180, false);

      if (settings->bools.netplay_public_announce)
         netplay_announce();
   }

   netplay_data = (netplay_t*)netplay_new(
         netplay_is_client ? direct_host : NULL,
         netplay_is_client ? (!netplay_client_deferred ? server
            : server_address_deferred) : NULL,
         netplay_is_client ? (!netplay_client_deferred ? port
            : server_port_deferred   ) : (port != 0 ? port : RARCH_DEFAULT_PORT),
         settings->bools.netplay_stateless_mode,
         settings->ints.netplay_check_frames,
         &cbs,
         settings->bools.netplay_nat_traversal,
         settings->paths.username,
         quirks);

   if (netplay_data)
   {
      if (netplay_data->is_server && !settings->bools.netplay_start_as_spectator)
         netplay_data->self_mode = NETPLAY_CONNECTION_PLAYING;
      return true;
   }

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

         case RARCH_NETPLAY_CTL_IS_SERVER:
            ret = netplay_enabled && !netplay_is_client;
            goto done;

         case RARCH_NETPLAY_CTL_IS_CONNECTED:
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
      case RARCH_NETPLAY_CTL_IS_SERVER:
         ret = netplay_enabled && !netplay_is_client;
         goto done;
      case RARCH_NETPLAY_CTL_IS_CONNECTED:
         ret = netplay_data->is_connected;
         goto done;
      case RARCH_NETPLAY_CTL_POST_FRAME:
         netplay_post_frame(netplay_data);
         break;
      case RARCH_NETPLAY_CTL_PRE_FRAME:
         ret = netplay_pre_frame(netplay_data);
         goto done;
      case RARCH_NETPLAY_CTL_FLIP_PLAYERS:
         if (netplay_data->is_server)
            netplay_flip_users(netplay_data);
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
      case RARCH_NETPLAY_CTL_RESET:
         netplay_core_reset(netplay_data);
         break;
      case RARCH_NETPLAY_CTL_DISCONNECT:
         ret = netplay_disconnect(netplay_data);
         goto done;
      case RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL:
         netplay_data->nat_traversal_task_oustanding = false;
#ifndef HAVE_SOCKET_LEGACY
         netplay_announce_nat_traversal(netplay_data);
#endif
         goto done;
      case RARCH_NETPLAY_CTL_DESYNC_PUSH:
         netplay_data->desync++;
         break;
      case RARCH_NETPLAY_CTL_DESYNC_POP:
         if (netplay_data->desync)
         {
            netplay_data->desync--;
            if (!netplay_data->desync)
               netplay_load_savestate(netplay_data, NULL, true);
         }
         break;
      default:
      case RARCH_NETPLAY_CTL_NONE:
         ret = false;
   }

done:
   in_netplay = false;
   return ret;
}
