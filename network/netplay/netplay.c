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

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "ws2_32")
#endif

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <retro_assert.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <features/features_cpu.h>
#include <retro_endianness.h>

#include "netplay_private.h"

#include "../../autosave.h"
#include "../../configuration.h"
#include "../../command.h"
#include "../../movie.h"
#include "../../runloop.h"

#define MAX_STALL_TIME_USEC         (10*1000*1000)
#define MAX_RETRIES                 16
#define RETRY_MS                    500

enum
{
   CMD_OPT_ALLOWED_IN_SPECTATE_MODE = 0x1,
   CMD_OPT_REQUIRE_ACK              = 0x2,
   CMD_OPT_HOST_ONLY                = 0x4,
   CMD_OPT_CLIENT_ONLY              = 0x8,
   CMD_OPT_REQUIRE_SYNC             = 0x10
};

/* Only used before init_netplay */
static bool netplay_enabled = false;
static bool netplay_is_client = false;

/* Used to advertise or request advertisement of Netplay */
static int netplay_ad_fd = -1;

/* Used while Netplay is running */
static netplay_t *netplay_data = NULL;

/* Used to avoid recursive netplay calls */
static bool in_netplay = false;

static void announce_nat_traversal(netplay_t *netplay);

static int init_tcp_connection(const struct addrinfo *res,
      bool server, bool spectate,
      struct sockaddr *other_addr, socklen_t addr_size)
{
   bool ret = true;
   int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   if (fd < 0)
   {
      ret = false;
      goto end;
   }

#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
   {
      int flag = 1;
      if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&flag, sizeof(int)) < 0)
         RARCH_WARN("Could not set netplay TCP socket to nodelay. Expect jitter.\n");
   }
#endif

#if defined(F_SETFD) && defined(FD_CLOEXEC)
   /* Don't let any inherited processes keep open our port */
   if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
      RARCH_WARN("Cannot set Netplay port to close-on-exec. It may fail to reopen if the client disconnects.\n");
#endif

   if (server)
   {
      if (socket_connect(fd, (void*)res, false) < 0)
      {
         ret = false;
         goto end;
      }
   }
   else
   {
      if (  !socket_bind(fd, (void*)res) || 
            listen(fd, spectate ? MAX_SPECTATORS : 1) < 0)
      {
         ret = false;
         goto end;
      }
   }

end:
   if (!ret && fd >= 0)
   {
      socket_close(fd);
      fd = -1;
   }

   return fd;
}

static bool init_tcp_socket(netplay_t *netplay, const char *server,
      uint16_t port, bool spectate)
{
   char port_buf[16];
   bool ret                        = false;
   const struct addrinfo *tmp_info = NULL;
   struct addrinfo *res            = NULL;
   struct addrinfo hints           = {0};

   port_buf[0] = '\0';

#ifdef AF_INET6
   if (!server)
      hints.ai_family = AF_INET6;
#endif
   hints.ai_socktype = SOCK_STREAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo_retro(server, port_buf, &hints, &res) < 0)
      return false;

   if (!res)
      return false;

   /* If we're serving on IPv6, make sure we accept all connections, including
    * IPv4 */
#ifdef AF_INET6
   if (!server && res->ai_family == AF_INET6)
   {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) res->ai_addr;
      sin6->sin6_addr = in6addr_any;
   }
#endif

   /* If "localhost" is used, it is important to check every possible 
    * address for IPv4/IPv6. */
   tmp_info = res;

   while (tmp_info)
   {
      int fd = init_tcp_connection(
            tmp_info,
            server,
            netplay->spectate.enabled,
            (struct sockaddr*)&netplay->other_addr,
            sizeof(netplay->other_addr));

      if (fd >= 0)
      {
         ret = true;
         netplay->fd = fd;
         break;
      }

      tmp_info = tmp_info->ai_next;
   }

   if (res)
      freeaddrinfo_retro(res);

   if (!ret)
      RARCH_ERR("Failed to set up netplay sockets.\n");

   return ret;
}

static void init_nat_traversal(netplay_t *netplay)
{
   natt_init();

   if (!natt_new(&netplay->nat_traversal_state))
   {
      netplay->nat_traversal = false;
      return;
   }

   natt_open_port_any(&netplay->nat_traversal_state, netplay->tcp_port, SOCKET_PROTOCOL_TCP);

   if (!netplay->nat_traversal_state.request_outstanding)
      announce_nat_traversal(netplay);
}

static bool init_ad_socket(netplay_t *netplay, uint16_t port)
{
   int fd = socket_init((void**)&netplay->addr, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   if (!socket_bind(fd, (void*)netplay->addr))
   {
      socket_close(fd);
      goto error;
   }

   netplay_ad_fd = fd;

   return true;

error:
   RARCH_ERR("Failed to initialize netplay advertisement socket.\n");
   return false;
}

static bool init_socket(netplay_t *netplay, const char *server, uint16_t port)
{
   if (!network_init())
      return false;

   if (!init_tcp_socket(netplay, server, port, netplay->spectate.enabled))
      return false;

   if (netplay->is_server && netplay->nat_traversal)
      init_nat_traversal(netplay);

   return true;
}

/**
 * hangup:
 *
 * Disconnects an active Netplay connection due to an error
 **/
static void hangup(netplay_t *netplay)
{
   if (!netplay)
      return;
   if (!netplay->has_connection)
      return;

   RARCH_WARN("Netplay has disconnected. Will continue without connection ...\n");
   runloop_msg_queue_push("Netplay has disconnected. Will continue without connection.", 0, 480, false);

   socket_close(netplay->fd);
   netplay->fd = -1;

   if (netplay->is_server && !netplay->spectate.enabled)
   {
      /* In server mode, make the socket listen for a new connection */
      if (!init_socket(netplay, NULL, netplay->tcp_port))
      {
         RARCH_WARN("Failed to reinitialize Netplay.\n");
         runloop_msg_queue_push("Failed to reinitialize Netplay.", 0, 480, false);
      }
   }

   netplay->has_connection = false;

   /* Reset things that will behave oddly if we get a new connection */
   netplay->remote_paused  = false;
   netplay->flip           = false;
   netplay->flip_frame     = 0;
   netplay->stall          = 0;
}

static bool netplay_info_cb(netplay_t* netplay, unsigned frames)
{
   return netplay->net_cbs->info_cb(netplay, frames);
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
   return netplay->is_replay && netplay->has_connection;
}

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
 * Grab our own input state and send this over the network.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool get_self_input_state(netplay_t *netplay)
{
   uint32_t state[WORDS_PER_FRAME - 1] = {0, 0, 0};
   struct delta_frame *ptr             = &netplay->buffer[netplay->self_ptr];

   if (!netplay_delta_frame_ready(netplay, ptr, netplay->self_frame_count))
      return false;

   if (ptr->have_local)
   {
      /* We've already read this frame! */
      return true;
   }

   if (!input_driver_is_libretro_input_blocked() && netplay->self_frame_count > 0)
   {
      unsigned i;
      settings_t *settings = config_get_ptr();

      /* First frame we always give zero input since relying on 
       * input from first frame screws up when we use -F 0. */
      retro_input_state_t cb = netplay->cbs.state_cb;
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         int16_t tmp = cb(settings->netplay.swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_JOYPAD, 0, i);
         state[0] |= tmp ? 1 << i : 0;
      }

      for (i = 0; i < 2; i++)
      {
         int16_t tmp_x = cb(settings->netplay.swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_ANALOG, i, 0);
         int16_t tmp_y = cb(settings->netplay.swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_ANALOG, i, 1);
         state[1 + i] = (uint16_t)tmp_x | (((uint16_t)tmp_y) << 16);
      }
   }

   /* Here we construct the payload format:
    * frame {
    *    uint32_t frame_number
    *    uint32_t RETRO_DEVICE_JOYPAD state (top 16 bits zero)
    *    uint32_t ANALOG state[0]
    *    uint32_t ANALOG state[1]
    * }
    *
    * payload {
    *    cmd (CMD_INPUT)
    *    cmd_size (4 words)
    *    frame
    * }
    */
   netplay->packet_buffer[0] = htonl(NETPLAY_CMD_INPUT);
   netplay->packet_buffer[1] = htonl(WORDS_PER_FRAME * sizeof(uint32_t));
   netplay->packet_buffer[2] = htonl(netplay->self_frame_count);
   netplay->packet_buffer[3] = htonl(state[0]);
   netplay->packet_buffer[4] = htonl(state[1]);
   netplay->packet_buffer[5] = htonl(state[2]);

   if (!netplay->spectate.enabled) /* Spectate sends in its own way */
   {
      if (!socket_send_all_blocking(netplay->fd,
               netplay->packet_buffer, sizeof(netplay->packet_buffer), false))
      {
         hangup(netplay);
         return false;
      }
   }

   memcpy(ptr->self_state, state, sizeof(state));
   ptr->have_local = true;
   return true;
}

static bool netplay_send_raw_cmd(netplay_t *netplay, uint32_t cmd,
      const void *data, size_t size)
{
   uint32_t cmdbuf[2];

   cmdbuf[0] = htonl(cmd);
   cmdbuf[1] = htonl(size);

   if (!socket_send_all_blocking(netplay->fd, cmdbuf, sizeof(cmdbuf), false))
      return false;

   if (size > 0)
      if (!socket_send_all_blocking(netplay->fd, data, size, false))
         return false;

   return true;
}

static bool netplay_cmd_nak(netplay_t *netplay)
{
   return netplay_send_raw_cmd(netplay, NETPLAY_CMD_NAK, NULL, 0);
}

bool netplay_cmd_crc(netplay_t *netplay, struct delta_frame *delta)
{
   uint32_t payload[2];
   payload[0] = htonl(delta->frame);
   payload[1] = htonl(delta->crc);
   return netplay_send_raw_cmd(netplay, NETPLAY_CMD_CRC, payload, sizeof(payload));
}

bool netplay_cmd_request_savestate(netplay_t *netplay)
{
   if (netplay->savestate_request_outstanding)
      return true;
   netplay->savestate_request_outstanding = true;
   return netplay_send_raw_cmd(netplay, NETPLAY_CMD_REQUEST_SAVESTATE, NULL, 0);
}

static bool netplay_get_cmd(netplay_t *netplay)
{
   uint32_t cmd;
   uint32_t flip_frame;
   uint32_t cmd_size;

   /* FIXME: This depends on delta_frame_ready */

   netplay->timeout_cnt = 0;

   if (!socket_receive_all_blocking(netplay->fd, &cmd, sizeof(cmd)))
      return false;

   cmd      = ntohl(cmd);

   if (!socket_receive_all_blocking(netplay->fd, &cmd_size, sizeof(cmd)))
      return false;

   cmd_size = ntohl(cmd_size);

   switch (cmd)
   {
      case NETPLAY_CMD_ACK:
         /* Why are we even bothering? */
         return true;

      case NETPLAY_CMD_NAK:
         /* Disconnect now! */
         return false;

      case NETPLAY_CMD_INPUT:
         {
            uint32_t buffer[WORDS_PER_FRAME];
            unsigned i;

            if (cmd_size != WORDS_PER_FRAME * sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_INPUT received an unexpected payload size.\n");
               return netplay_cmd_nak(netplay);
            }

            if (!socket_receive_all_blocking(netplay->fd, buffer, sizeof(buffer)))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_INPUT input.\n");
               return netplay_cmd_nak(netplay);
            }

            for (i = 0; i < WORDS_PER_FRAME; i++)
               buffer[i] = ntohl(buffer[i]);

            if (buffer[0] < netplay->read_frame_count)
            {
               /* We already had this, so ignore the new transmission */
               return true;
            }
            else if (buffer[0] > netplay->read_frame_count)
            {
               /* Out of order = out of luck */
               return netplay_cmd_nak(netplay);
            }

            /* The data's good! */
            netplay->buffer[netplay->read_ptr].have_remote = true;
            memcpy(netplay->buffer[netplay->read_ptr].real_input_state,
                  buffer + 1, sizeof(buffer) - sizeof(uint32_t));
            netplay->read_ptr = NEXT_PTR(netplay->read_ptr);
            netplay->read_frame_count++;
            return true;
         }

      case NETPLAY_CMD_FLIP_PLAYERS:
         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("CMD_FLIP_PLAYERS received an unexpected command size.\n");
            return netplay_cmd_nak(netplay);
         }

         if (!socket_receive_all_blocking(
                  netplay->fd, &flip_frame, sizeof(flip_frame)))
         {
            RARCH_ERR("Failed to receive CMD_FLIP_PLAYERS argument.\n");
            return netplay_cmd_nak(netplay);
         }

         flip_frame = ntohl(flip_frame);

         if (flip_frame < netplay->read_frame_count)
         {
            RARCH_ERR("Host asked us to flip users in the past. Not possible ...\n");
            return netplay_cmd_nak(netplay);
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

         return true;

      case NETPLAY_CMD_SPECTATE:
         RARCH_ERR("NETPLAY_CMD_SPECTATE unimplemented.\n");
         return netplay_cmd_nak(netplay);

      case NETPLAY_CMD_DISCONNECT:
         hangup(netplay);
         return true;

      case NETPLAY_CMD_CRC:
         {
            uint32_t buffer[2];
            size_t tmp_ptr = netplay->self_ptr;
            bool found = false;

            if (cmd_size != sizeof(buffer))
            {
               RARCH_ERR("NETPLAY_CMD_CRC received unexpected payload size.\n");
               return netplay_cmd_nak(netplay);
            }

            if (!socket_receive_all_blocking(netplay->fd, buffer, sizeof(buffer)))
            {
               RARCH_ERR("NETPLAY_CMD_CRC failed to receive payload.\n");
               return netplay_cmd_nak(netplay);
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
               return true;
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

            return true;
         }

      case NETPLAY_CMD_REQUEST_SAVESTATE:
         /* Delay until next frame so we don't send the savestate after the
          * input */
         netplay->force_send_savestate = true;
         return true;

      case NETPLAY_CMD_LOAD_SAVESTATE:
         {
            uint32_t frame;
            uint32_t isize;
            uint32_t rd, wn;

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
               return netplay_cmd_nak(netplay);
            }

            if (!socket_receive_all_blocking(netplay->fd, &frame, sizeof(frame)))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive savestate frame.\n");
               return netplay_cmd_nak(netplay);
            }
            frame = ntohl(frame);

            if (frame != netplay->read_frame_count)
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE loading a state out of order!\n");
               return netplay_cmd_nak(netplay);
            }

            if (!socket_receive_all_blocking(netplay->fd, &isize, sizeof(isize)))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive inflated size.\n");
               return netplay_cmd_nak(netplay);
            }
            isize = ntohl(isize);

            if (isize != netplay->state_size)
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE received an unexpected save state size.\n");
               return netplay_cmd_nak(netplay);
            }

            if (!socket_receive_all_blocking(netplay->fd,
                     netplay->zbuffer, cmd_size - 2*sizeof(uint32_t)))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive savestate.\n");
               return netplay_cmd_nak(netplay);
            }

            /* And decompress it */
            netplay->decompression_backend->set_in(netplay->decompression_stream,
               netplay->zbuffer, cmd_size - 2*sizeof(uint32_t));
            netplay->decompression_backend->set_out(netplay->decompression_stream,
               (uint8_t*)netplay->buffer[netplay->read_ptr].state, netplay->state_size);
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
               netplay->self_ptr         = PREV_PTR(netplay->read_ptr);
               netplay->self_frame_count = frame - 1;
            }

            /* And force rewind to it */
            netplay->force_rewind                  = true;
            netplay->savestate_request_outstanding = false;
            netplay->other_ptr                     = netplay->read_ptr;
            netplay->other_frame_count             = frame;
            return true;
         }

      case NETPLAY_CMD_PAUSE:
         netplay->remote_paused = true;
         return true;

      case NETPLAY_CMD_RESUME:
         netplay->remote_paused = false;
         return true;

      default:
         break;
   }

   RARCH_ERR("%s.\n", msg_hash_to_str(MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED));
   return netplay_cmd_nak(netplay);
}

static int poll_input(netplay_t *netplay, bool block)
{
   bool had_input    = false;
   int max_fd        = netplay->fd + 1;
   struct timeval tv = {0};
   tv.tv_sec         = 0;
   tv.tv_usec        = block ? (RETRY_MS * 1000) : 0;

   do
   { 
      fd_set fds;
      /* select() does not take pointer to const struct timeval.
       * Technically possible for select() to modify tmp_tv, so 
       * we go paranoia mode. */
      struct timeval tmp_tv = tv;
      had_input = false;

      netplay->timeout_cnt++;

      FD_ZERO(&fds);
      FD_SET(netplay->fd, &fds);

      if (socket_select(max_fd, &fds, NULL, NULL, &tmp_tv) < 0)
         return -1;

      if (FD_ISSET(netplay->fd, &fds))
      {
         /* If we're not ready for input, wait until we are. 
          * Could fill the TCP buffer, stalling the other side. */
         if (netplay_delta_frame_ready(netplay,
                  &netplay->buffer[netplay->read_ptr],
                  netplay->read_frame_count))
         {
            had_input = true;
            if (!netplay_get_cmd(netplay))
               return -1;
         }
      }

      /* If we were blocked for input, pass if we have this frame's input */
      if (block && netplay->read_frame_count > netplay->self_frame_count)
         break;

      /* If we had input, we might have more */
      if (had_input || !block)
         continue;

      RARCH_LOG("Network is stalling at frame %u, count %u of %d ...\n",
            netplay->self_frame_count, netplay->timeout_cnt, MAX_RETRIES);

      if (netplay->timeout_cnt >= MAX_RETRIES && !netplay->remote_paused)
         return -1;
   } while (had_input || block);

   return 0;
}

/**
 * netplay_simulate_input:
 * @netplay             : pointer to netplay object
 * @sim_ptr             : frame index for which to simulate input
 *
 * "Simulate" input by assuming it hasn't changed since the last read input.
 */
void netplay_simulate_input(netplay_t *netplay, uint32_t sim_ptr)
{
   size_t prev = PREV_PTR(netplay->read_ptr);
   memcpy(netplay->buffer[sim_ptr].simulated_input_state,
         netplay->buffer[prev].real_input_state,
         sizeof(netplay->buffer[prev].real_input_state));
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

   if (!netplay_data->has_connection)
      return false;

   netplay_data->can_poll = false;

   get_self_input_state(netplay_data);

   /* No network side in spectate mode */
   if (netplay_is_server(netplay_data) && netplay_data->spectate.enabled)
      return true;

   /* Read Netplay input, block if we're configured to stall for input every
    * frame */
   if (netplay_data->stall_frames == 0 &&
       netplay_data->read_frame_count <= netplay_data->self_frame_count)
      res = poll_input(netplay_data, true);
   else
      res = poll_input(netplay_data, false);
   if (res == -1)
   {
      hangup(netplay_data);
      return false;
   }

   /* Simulate the input if we don't have real input */
   if (!netplay_data->buffer[netplay_data->self_ptr].have_remote)
      netplay_simulate_input(netplay_data, netplay_data->self_ptr);

   /* Consider stalling */
   switch (netplay_data->stall)
   {
      case RARCH_NETPLAY_STALL_RUNNING_FAST:
         if (netplay_data->read_frame_count >= netplay_data->self_frame_count)
            netplay_data->stall = RARCH_NETPLAY_STALL_NONE;
         break;

      default: /* not stalling */
         if (netplay_data->read_frame_count + netplay_data->stall_frames 
               <= netplay_data->self_frame_count)
         {
            netplay_data->stall      = RARCH_NETPLAY_STALL_RUNNING_FAST;
            netplay_data->stall_time = cpu_features_get_time_usec();
         }
   }

   /* If we're stalling, consider disconnection */
   if (netplay_data->stall)
   {
      retro_time_t now = cpu_features_get_time_usec();

      /* Don't stall out while they're paused */
      if (netplay_data->remote_paused)
         netplay_data->stall_time = now;
      else if (now - netplay_data->stall_time >= MAX_STALL_TIME_USEC)
      {
         /* Stalled out! */
         hangup(netplay_data);
         return false;
      }
   }

   return true;
}

void input_poll_net(void)
{
   if (!netplay_should_skip(netplay_data) && netplay_can_poll(netplay_data))
      netplay_poll();
}

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
   return netplay_data->has_connection;
}

static bool netplay_flip_port(netplay_t *netplay, bool port)
{
   size_t frame = netplay->self_frame_count;

   if (netplay->flip_frame == 0)
      return port;

   if (netplay->is_replay)
      frame = netplay->replay_frame_count;

   return port ^ netplay->flip ^ (frame < netplay->flip_frame);
}

static int16_t netplay_input_state(netplay_t *netplay,
      bool port, unsigned device,
      unsigned idx, unsigned id)
{
   size_t ptr = netplay->is_replay ? 
      netplay->replay_ptr : netplay->self_ptr;

   const uint32_t *curr_input_state = netplay->buffer[ptr].self_state;

   if (netplay->port == (netplay_flip_port(netplay, port) ? 1 : 0))
   {
      if (netplay->buffer[ptr].have_remote)
      {
         netplay->buffer[ptr].used_real = true;
         curr_input_state = netplay->buffer[ptr].real_input_state;
      }
      else
      {
         curr_input_state = netplay->buffer[ptr].simulated_input_state;
      }
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

int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   if (netplay_is_alive())
   {
      /* Only two players for now. */
      if (port > 1)
         return 0;

      return netplay_input_state(netplay_data, port, device, idx, id);
   }
   return netplay_data->cbs.state_cb(port, device, idx, id);
}

#ifndef HAVE_SOCKET_LEGACY
/* Custom inet_ntop. Win32 doesn't seem to support this ... */
void netplay_log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick)
{
   union
   {
      const struct sockaddr_storage *storage;
      const struct sockaddr_in *v4;
      const struct sockaddr_in6 *v6;
   } u;
   const char *str               = NULL;
   char buf_v4[INET_ADDRSTRLEN]  = {0};
   char buf_v6[INET6_ADDRSTRLEN] = {0};
   char msg[512];

   msg[0] = '\0';

   u.storage = their_addr;

   switch (their_addr->ss_family)
   {
      case AF_INET:
         {
            struct sockaddr_in in;

            memset(&in, 0, sizeof(in));

            str           = buf_v4;
            in.sin_family = AF_INET;
            memcpy(&in.sin_addr, &u.v4->sin_addr, sizeof(struct in_addr));

            getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in),
                  buf_v4, sizeof(buf_v4),
                  NULL, 0, NI_NUMERICHOST);
         }
         break;
      case AF_INET6:
         {
            struct sockaddr_in6 in;
            memset(&in, 0, sizeof(in));

            str            = buf_v6;
            in.sin6_family = AF_INET6;
            memcpy(&in.sin6_addr, &u.v6->sin6_addr, sizeof(struct in6_addr));

            getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in6),
                  buf_v6, sizeof(buf_v6), NULL, 0, NI_NUMERICHOST);
         }
         break;
      default:
         break;
   }

   if (str)
   {
      snprintf(msg, sizeof(msg), "%s: \"%s (%s)\"",
            msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
            nick, str);
      runloop_msg_queue_push(msg, 1, 180, false);
      RARCH_LOG("%s\n", msg);
   }
   else
   {
      snprintf(msg, sizeof(msg), "%s: \"%s\"",
            msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
            nick);
      runloop_msg_queue_push(msg, 1, 180, false);
      RARCH_LOG("%s\n", msg);
   }
   RARCH_LOG("%s %u\n", msg_hash_to_str(MSG_CONNECTION_SLOT), 
         slot);
}

#else
void netplay_log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick)
{
   char msg[512];

   msg[0] = '\0';

   snprintf(msg, sizeof(msg), "%s: \"%s\"",
         msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
         nick);
   runloop_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);
   RARCH_LOG("%s %u\n",
         msg_hash_to_str(MSG_CONNECTION_SLOT), slot);
}

#endif

static void announce_nat_traversal(netplay_t *netplay)
{
   char msg[512], host[PATH_MAX_LENGTH], port[6];

#ifndef HAVE_SOCKET_LEGACY
   if (netplay->nat_traversal_state.have_inet4)
   {
      if (getnameinfo((const struct sockaddr *) &netplay->nat_traversal_state.ext_inet4_addr,
         sizeof(struct sockaddr_in),
         host, PATH_MAX_LENGTH, port, 6, NI_NUMERICHOST|NI_NUMERICSERV) != 0)
         return;

   }
#ifdef AF_INET6
   else if (netplay->nat_traversal_state.have_inet6)
   {
      if (getnameinfo((const struct sockaddr *) &netplay->nat_traversal_state.ext_inet6_addr,
         sizeof(struct sockaddr_in6),
         host, PATH_MAX_LENGTH, port, 6, NI_NUMERICHOST|NI_NUMERICSERV) != 0)
         return;

   }
#endif
   else return;

#else
   if (netplay->nat_traversal_state.have_inet4)
   {
      strncpy(host,
         inet_ntoa(netplay->nat_traversal_state.ext_inet4_addr.sin_addr),
         PATH_MAX_LENGTH);
      host[PATH_MAX_LENGTH-1] = '\0';
      snprintf(port, 6, "%hu",
         ntohs(netplay->nat_traversal_state.ext_inet4_addr.sin_port));
      port[5] = '\0';

   }
   else return;

#endif

   snprintf(msg, sizeof(msg), "%s: %s:%s\n",
         msg_hash_to_str(MSG_PUBLIC_ADDRESS),
         host, port);
   runloop_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);
}


bool netplay_try_init_serialization(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   if (netplay->state_size)
      return true;

   if (!netplay_init_serialization(netplay))
      return false;

   /* Check if we can actually save */
   serial_info.data_const = NULL;
   serial_info.data = netplay->buffer[netplay->self_ptr].state;
   serial_info.size = netplay->state_size;

   if (!core_serialize(&serial_info))
      return false;

   /* Once initialized, we no longer exhibit this quirk */
   netplay->quirks &= ~((uint64_t) NETPLAY_QUIRK_INITIALIZATION);

   return true;
}

bool netplay_wait_and_init_serialization(netplay_t *netplay)
{
   int frame;

   if (netplay->state_size)
      return true;

   /* Wait a maximum of 60 frames */
   for (frame = 0; frame < 60; frame++) {
      if (netplay_try_init_serialization(netplay))
         return true;

#if defined(HAVE_THREADS)
      autosave_lock();
#endif
      core_run();
#if defined(HAVE_THREADS)
      autosave_unlock();
#endif
   }

   return false;
}

bool netplay_init_serialization(netplay_t *netplay)
{
   unsigned i;
   retro_ctx_size_info_t info;

   if (netplay->state_size)
      return true;

   core_serialize_size(&info);

   if (!info.size)
      return false;

   netplay->state_size = info.size;

   for (i = 0; i < netplay->buffer_size; i++)
   {
      netplay->buffer[i].state = calloc(netplay->state_size, 1);

      if (!netplay->buffer[i].state)
      {
         netplay->quirks |= NETPLAY_QUIRK_NO_SAVESTATES;
         return false;
      }
   }

   netplay->zbuffer_size = netplay->state_size * 2;
   netplay->zbuffer = (uint8_t *) calloc(netplay->zbuffer_size, 1);
   if (!netplay->zbuffer)
   {
      netplay->quirks |= NETPLAY_QUIRK_NO_TRANSMISSION;
      netplay->zbuffer_size = 0;
      return false;
   }

   return true;
}

static bool netplay_init_buffers(netplay_t *netplay, unsigned frames)
{
   if (!netplay)
      return false;

   /* * 2 + 1 because:
    * Self sits in the middle,
    * Other is allowed to drift as much as 'frames' frames behind
    * Read is allowed to drift as much as 'frames' frames ahead */
   netplay->buffer_size = frames * 2 + 1;

   netplay->buffer = (struct delta_frame*)calloc(netplay->buffer_size,
         sizeof(*netplay->buffer));

   if (!netplay->buffer)
      return false;

   if (!(netplay->quirks & NETPLAY_QUIRK_INITIALIZATION))
      netplay_init_serialization(netplay);

   return true;
}

/**
 * netplay_new:
 * @server               : IP address of server.
 * @port                 : Port of server.
 * @frames               : Amount of lag frames.
 * @check_frames         : Frequency with which to check CRCs.
 * @cb                   : Libretro callbacks.
 * @spectate             : If true, enable spectator mode.
 * @nat_traversal        : If true, attempt NAT traversal.
 * @nick                 : Nickname of user.
 * @quirks               : Netplay quirks required for this session.
 *
 * Creates a new netplay handle. A NULL host means we're 
 * hosting (user 1).
 *
 * Returns: new netplay handle.
 **/
netplay_t *netplay_new(const char *server, uint16_t port, unsigned frames,
      unsigned check_frames, const struct retro_callbacks *cb, bool spectate,
      bool nat_traversal, const char *nick, uint64_t quirks)
{
   netplay_t *netplay = (netplay_t*)calloc(1, sizeof(*netplay));
   if (!netplay)
      return NULL;

   netplay->fd                = -1;
   netplay->tcp_port          = port;
   netplay->cbs               = *cb;
   netplay->port              = server ? 0 : 1;
   netplay->spectate.enabled  = spectate;
   netplay->is_server         = server == NULL;
   netplay->nat_traversal     = netplay->is_server ? nat_traversal : false;
   netplay->stall_frames      = frames;
   netplay->check_frames      = check_frames;
   netplay->quirks            = quirks;

   strlcpy(netplay->nick, nick, sizeof(netplay->nick));

   if (!netplay_init_buffers(netplay, frames))
   {
      free(netplay);
      return NULL;
   }

   if(spectate)
      netplay->net_cbs = netplay_get_cbs_spectate();
   else
      netplay->net_cbs = netplay_get_cbs_net();

   if (!init_socket(netplay, server, port))
   {
      free(netplay);
      return NULL;
   }

   if(!netplay_info_cb(netplay, frames))
      goto error;

   return netplay;

error:
   if (netplay->fd >= 0)
      socket_close(netplay->fd);

   free(netplay);
   return NULL;
}

/**
 * netplay_command:
 * @netplay                : pointer to netplay object
 * @cmd                    : command to send
 * @data                   : data to send as argument
 * @sz                     : size of data
 * @flags                  : flags of CMD_OPT_*
 * @command_str            : name of action
 * @success_msg            : message to display upon success
 * 
 * Sends a single netplay command and waits for response.
 */
bool netplay_command(netplay_t* netplay, enum netplay_cmd cmd,
                     void* data, size_t sz,
                     uint32_t flags,
                     const char* command_str,
                     const char* success_msg)
{
   char m[256];
   const char* msg         = NULL;
   bool allowed_spectate   = !!(flags & CMD_OPT_ALLOWED_IN_SPECTATE_MODE);
   bool host_only          = !!(flags & CMD_OPT_HOST_ONLY);

   retro_assert(netplay);

   if (netplay->spectate.enabled && !allowed_spectate)
   {
      msg = "Cannot %s in spectate mode.";
      goto error; 
   }

   if (host_only && netplay->port == 0)
   {
      msg = "Cannot %s as a client.";
      goto error;
   }

   if (!netplay_send_raw_cmd(netplay, cmd, data, sz))
      goto error;

   runloop_msg_queue_push(success_msg, 1, 180, false);

   return true;

error:
   if (msg)
      snprintf(m, sizeof(m), msg, command_str);
   RARCH_WARN("%s\n", m);
   runloop_msg_queue_push(m, 1, 180, false);
   return false;
}

/**
 * netplay_flip_users:
 * @netplay              : pointer to netplay object
 *
 * On regular netplay, flip who controls user 1 and 2.
 **/
static void netplay_flip_users(netplay_t *netplay)
{
   /* Must be in the future because we may have 
    * already sent this frame's data */
   uint32_t     flip_frame = netplay->self_frame_count + 1;
   uint32_t flip_frame_net = htonl(flip_frame);
   bool            command = netplay_command(
      netplay, NETPLAY_CMD_FLIP_PLAYERS,
      &flip_frame_net, sizeof flip_frame_net,
      CMD_OPT_HOST_ONLY | CMD_OPT_REQUIRE_SYNC,
      "flip users", "Successfully flipped users.\n");

   if(command)
   {
      netplay->flip       ^= true;
      netplay->flip_frame  = flip_frame;
   }
}

/**
 * netplay_free:
 * @netplay              : pointer to netplay object
 *
 * Frees netplay handle.
 **/
void netplay_free(netplay_t *netplay)
{
   unsigned i;

   if (netplay->fd >= 0)
      socket_close(netplay->fd);

   if (netplay->spectate.enabled)
   {
      for (i = 0; i < MAX_SPECTATORS; i++)
         if (netplay->spectate.fds[i] >= 0)
            socket_close(netplay->spectate.fds[i]);

      free(netplay->spectate.input);
   }

   if (netplay->nat_traversal)
      natt_free(&netplay->nat_traversal_state);

   if (netplay->buffer)
   {
      for (i = 0; i < netplay->buffer_size; i++)
         if (netplay->buffer[i].state)
            free(netplay->buffer[i].state);

      free(netplay->buffer);
   }

   if (netplay->zbuffer)
      free(netplay->zbuffer);

   if (netplay->compression_stream)
      netplay->compression_backend->stream_free(netplay->compression_stream);

   if (netplay->addr)
      freeaddrinfo_retro(netplay->addr);

   free(netplay);
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
   retro_assert(netplay && netplay->net_cbs->pre_frame);

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
      /* Advertise our server if applicable */
      if (netplay_ad_fd >= 0 || init_ad_socket(netplay, RARCH_DEFAULT_PORT))
         netplay_ad_server(netplay, netplay_ad_fd);

      /* NAT traversal if applicable */
      if (netplay->nat_traversal &&
          netplay->nat_traversal_state.request_outstanding &&
          !netplay->nat_traversal_state.have_inet4)
      {
         struct timeval tmptv = {0};
         fd_set fds = netplay->nat_traversal_state.fds;
         if (socket_select(netplay->nat_traversal_state.nfds, &fds, NULL, NULL, &tmptv) > 0)
            natt_read(&netplay->nat_traversal_state);

         if (!netplay->nat_traversal_state.request_outstanding ||
             netplay->nat_traversal_state.have_inet4)
            announce_nat_traversal(netplay);
      }
   }

   if (!netplay->net_cbs->pre_frame(netplay))
      return false;

   return (!netplay->has_connection || 
          (!netplay->stall && !netplay->remote_paused));
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
   retro_assert(netplay && netplay->net_cbs->post_frame);
   netplay->net_cbs->post_frame(netplay);
}

/**
 * netplay_frontend_paused
 * @netplay              : pointer to netplay object
 * @paused               : true if frontend is paused
 *
 * Inform Netplay of the frontend's pause state (paused or otherwise)
 **/
void netplay_frontend_paused(netplay_t *netplay, bool paused)
{
   /* Nothing to do if we already knew this */
   if (netplay->local_paused == paused)
      return;

   netplay->local_paused = paused;
   if (netplay->has_connection && !netplay->spectate.enabled)
      netplay_send_raw_cmd(netplay, paused 
            ? NETPLAY_CMD_PAUSE : NETPLAY_CMD_RESUME, NULL, 0);
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
   uint32_t header[4];
   retro_ctx_serialize_info_t tmp_serial_info;
   uint32_t rd, wn;

   if (!netplay->has_connection)
      return;

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
   }

   /* We need to ignore any intervening data from the other side, 
    * and never rewind past this */
   if (netplay->read_frame_count < netplay->self_frame_count)
   {
      netplay->read_ptr = netplay->self_ptr;
      netplay->read_frame_count = netplay->self_frame_count;
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

   /* Compress it */
   netplay->compression_backend->set_in(netplay->compression_stream,
      (const uint8_t*)serial_info->data_const, serial_info->size);
   netplay->compression_backend->set_out(netplay->compression_stream,
      netplay->zbuffer, netplay->zbuffer_size);
   if (!netplay->compression_backend->trans(netplay->compression_stream,
      true, &rd, &wn, NULL))
   {
      hangup(netplay);
      return;
   }

   /* And send it to the peer */
   header[0] = htonl(NETPLAY_CMD_LOAD_SAVESTATE);
   header[1] = htonl(wn + 2*sizeof(uint32_t));
   header[2] = htonl(netplay->self_frame_count);
   header[3] = htonl(serial_info->size);

   if (!socket_send_all_blocking(netplay->fd, header, sizeof(header), false))
   {
      hangup(netplay);
      return;
   }

   if (!socket_send_all_blocking(netplay->fd,
            netplay->zbuffer, wn, false))
   {
      hangup(netplay);
      return;
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
   if (!netplay || !netplay->has_connection)
      return true;
   hangup(netplay);
   return true;
}

void deinit_netplay(void)
{
   if (netplay_data)
      netplay_free(netplay_data);
   netplay_data = NULL;
}

/**
 * init_netplay:
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/

bool init_netplay(bool is_spectate, const char *server, unsigned port)
{
   struct retro_callbacks cbs    = {0};
   settings_t *settings          = config_get_ptr();
   uint64_t serialization_quirks = 0;
   uint64_t quirks               = 0;

   if (!netplay_enabled)
      return false;

   if (bsv_movie_ctl(BSV_MOVIE_CTL_START_PLAYBACK, NULL))
   {
      RARCH_WARN("%s\n",
            msg_hash_to_str(MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED));
      return false;
   }

   core_set_default_callbacks(&cbs);

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
   }

   netplay_data = (netplay_t*)netplay_new(
         netplay_is_client ? server : NULL,
         port ? port : RARCH_DEFAULT_PORT,
         settings->netplay.sync_frames, settings->netplay.check_frames, &cbs,
         is_spectate, settings->netplay.nat_traversal, settings->username,
         quirks);

   if (netplay_data)
      return true;

   RARCH_WARN("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_NETPLAY_FAILED),
         0, 180, false);
   return false;
}

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
            if (*state)
               netplay_flip_users(netplay_data);
         }
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
