/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <retro_endianness.h>

#include "netplay_private.h"

enum
{
   CMD_OPT_ALLOWED_IN_SPECTATE_MODE = 0x1,
   CMD_OPT_REQUIRE_ACK              = 0x2,
   CMD_OPT_HOST_ONLY                = 0x4,
   CMD_OPT_CLIENT_ONLY              = 0x8,
   CMD_OPT_REQUIRE_SYNC             = 0x10
};

void *netplay_data;

/**
 * warn_hangup:
 *
 * Warns that netplay has disconnected.
 **/
static void warn_hangup(void)
{
   RARCH_WARN("Netplay has disconnected. Will continue without connection ...\n");
   runloop_msg_queue_push("Netplay has disconnected. Will continue without connection.", 0, 480, false);
}

/**
 * check_netplay_synched:
 * @netplay: pointer to the netplay object.
 * Checks to see if the host and client have synchronized states. Returns true
 * on success and false on failure.
 */
bool check_netplay_synched(netplay_t* netplay)
{
   assert(netplay);
   return netplay->frame_count < (netplay->flip_frame + 2 * UDP_FRAME_PACKETS);
}

static bool netplay_info_cb(netplay_t* netplay, unsigned frames) {
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

static bool send_chunk(netplay_t *netplay)
{
   const struct sockaddr *addr = NULL;

   if (netplay->addr)
      addr = netplay->addr->ai_addr;
   else if (netplay->has_client_addr)
      addr = (const struct sockaddr*)&netplay->their_addr;

   if (addr)
   {
      ssize_t bytes_sent;

#ifdef HAVE_IPV6
      bytes_sent = (sendto(netplay->udp_fd, (const char*)netplay->packet_buffer,
               sizeof(netplay->packet_buffer), 0, addr,
               sizeof(struct sockaddr_in6)));
#else
      bytes_sent = (sendto(netplay->udp_fd, (const char*)netplay->packet_buffer,
               sizeof(netplay->packet_buffer), 0, addr,
               sizeof(struct sockaddr_in)));
#endif

      if (bytes_sent != sizeof(netplay->packet_buffer))
      {
         warn_hangup();
         netplay->has_connection = false;
         return false;
      }
   }
   return true;
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
   uint32_t state[UDP_WORDS_PER_FRAME - 1] = {0};
   struct delta_frame *ptr                 = &netplay->buffer[netplay->self_ptr];

   if (!input_driver_is_libretro_input_blocked() && netplay->frame_count > 0)
   {
      unsigned i;
      settings_t *settings = config_get_ptr();

      /* First frame we always give zero input since relying on 
       * input from first frame screws up when we use -F 0. */
      retro_input_state_t cb = netplay->cbs.state_cb;
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         int16_t tmp = cb(settings->input.netplay_client_swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_JOYPAD, 0, i);
         state[0] |= tmp ? 1 << i : 0;
      }

      for (i = 0; i < 2; i++)
      {
         int16_t tmp_x = cb(settings->input.netplay_client_swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_ANALOG, i, 0);
         int16_t tmp_y = cb(settings->input.netplay_client_swap_input ?
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
    *    ; To compat packet losses, send input in a sliding window
    *    frame redundancy_frames[UDP_FRAME_PACKETS];
    * }
    */
   memmove(netplay->packet_buffer, netplay->packet_buffer + UDP_WORDS_PER_FRAME,
         sizeof (netplay->packet_buffer) - UDP_WORDS_PER_FRAME * sizeof(uint32_t));
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * UDP_WORDS_PER_FRAME] = htonl(netplay->frame_count); 
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * UDP_WORDS_PER_FRAME + 1] = htonl(state[0]);
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * UDP_WORDS_PER_FRAME + 2] = htonl(state[1]);
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * UDP_WORDS_PER_FRAME + 3] = htonl(state[2]);

   if (!send_chunk(netplay))
   {
      warn_hangup();
      netplay->has_connection = false;
      return false;
   }

   memcpy(ptr->self_state, state, sizeof(state));
   netplay->self_ptr = NEXT_PTR(netplay->self_ptr);
   return true;
}

static bool netplay_cmd_ack(netplay_t *netplay)
{
   uint32_t cmd = htonl(NETPLAY_CMD_ACK);
   return socket_send_all_blocking(netplay->fd, &cmd, sizeof(cmd), false);
}

static bool netplay_cmd_nak(netplay_t *netplay)
{
   uint32_t cmd = htonl(NETPLAY_CMD_NAK);
   return socket_send_all_blocking(netplay->fd, &cmd, sizeof(cmd), false);
}

static bool netplay_get_response(netplay_t *netplay)
{
   uint32_t response;
   if (!socket_receive_all_blocking(netplay->fd, &response, sizeof(response)))
      return false;

   return ntohl(response) == NETPLAY_CMD_ACK;
}

static bool netplay_get_cmd(netplay_t *netplay)
{
   uint32_t cmd;
   uint32_t flip_frame;
   size_t cmd_size;

   if (!socket_receive_all_blocking(netplay->fd, &cmd, sizeof(cmd)))
      return false;

   cmd      = ntohl(cmd);

   cmd_size = cmd & 0xffff;
   cmd      = cmd >> 16;

   switch (cmd)
   {
      case NETPLAY_CMD_FLIP_PLAYERS:
         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("CMD_FLIP_PLAYERS recieved an unexpected command size.\n");
            return netplay_cmd_nak(netplay);
         }

         if (!socket_receive_all_blocking(
                  netplay->fd, &flip_frame, sizeof(flip_frame)))
         {
            RARCH_ERR("Failed to receive CMD_FLIP_PLAYERS argument.\n");
            return netplay_cmd_nak(netplay);
         }

         flip_frame = ntohl(flip_frame);

         if (flip_frame < netplay->flip_frame)
         {
            RARCH_ERR("Host asked us to flip users in the past. Not possible ...\n");
            return netplay_cmd_nak(netplay);
         }

         netplay->flip ^= true;
         netplay->flip_frame = flip_frame;

         RARCH_LOG("Netplay users are flipped.\n");
         runloop_msg_queue_push("Netplay users are flipped.", 1, 180, false);

         return netplay_cmd_ack(netplay);

      case NETPLAY_CMD_SPECTATE:
         RARCH_ERR("NETPLAY_CMD_SPECTATE unimplemented.\n");
         return netplay_cmd_nak(netplay);

      case NETPLAY_CMD_DISCONNECT:
         warn_hangup();
         return netplay_cmd_ack(netplay);

      case NETPLAY_CMD_LOAD_SAVESTATE:
         RARCH_ERR("NETPLAY_CMD_LOAD_SAVESTATE unimplemented.\n");
         return netplay_cmd_nak(netplay);

      case NETPLAY_CMD_PAUSE:
         command_event(EVENT_CMD_PAUSE, NULL);
         return netplay_cmd_ack(netplay);

      case NETPLAY_CMD_RESUME:
         command_event(EVENT_CMD_UNPAUSE, NULL);
         return netplay_cmd_ack(netplay);

      default: break;
   }

   RARCH_ERR("Unknown netplay command received.\n");
   return netplay_cmd_nak(netplay);
}

#define MAX_RETRIES 16
#define RETRY_MS 500

static int poll_input(netplay_t *netplay, bool block)
{
   int max_fd        = (netplay->fd > netplay->udp_fd ? 
         netplay->fd : netplay->udp_fd) + 1;
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

      netplay->timeout_cnt++;

      FD_ZERO(&fds);
      FD_SET(netplay->udp_fd, &fds);
      FD_SET(netplay->fd, &fds);

      if (socket_select(max_fd, &fds, NULL, NULL, &tmp_tv) < 0)
         return -1;

      /* Somewhat hacky,
       * but we aren't using the TCP connection for anything useful atm. */
      if (FD_ISSET(netplay->fd, &fds) && !netplay_get_cmd(netplay))
         return -1; 

      if (FD_ISSET(netplay->udp_fd, &fds))
         return 1;

      if (!block)
         continue;

      if (!send_chunk(netplay))
      {
         warn_hangup();
         netplay->has_connection = false;
         return -1;
      }

      RARCH_LOG("Network is stalling, resending packet... Count %u of %d ...\n",
            netplay->timeout_cnt, MAX_RETRIES);
   } while ((netplay->timeout_cnt < MAX_RETRIES) && block);

   if (block)
      return -1;
   return 0;
}

static bool receive_data(netplay_t *netplay, uint32_t *buffer, size_t size)
{
   socklen_t addrlen = sizeof(netplay->their_addr);

   if (recvfrom(netplay->udp_fd, (char*)buffer, size, 0,
            (struct sockaddr*)&netplay->their_addr, &addrlen) != (ssize_t)size)
      return false;

   netplay->has_client_addr = true;

   return true;
}

static void parse_packet(netplay_t *netplay, uint32_t *buffer, unsigned size)
{
   unsigned i;

   for (i = 0; i < size * UDP_WORDS_PER_FRAME; i++)
      buffer[i] = ntohl(buffer[i]);

   for (i = 0; i < size && netplay->read_frame_count <= netplay->frame_count; i++)
   {
      uint32_t frame = buffer[UDP_WORDS_PER_FRAME * i + 0];
      const uint32_t *state = &buffer[UDP_WORDS_PER_FRAME * i + 1];

      if (frame != netplay->read_frame_count)
         continue;

      netplay->buffer[netplay->read_ptr].is_simulated = false;
      memcpy(netplay->buffer[netplay->read_ptr].real_input_state, state,
            sizeof(netplay->buffer[netplay->read_ptr].real_input_state));

      netplay->read_ptr = NEXT_PTR(netplay->read_ptr);
      netplay->read_frame_count++;
      netplay->timeout_cnt = 0;
   }
}

/* TODO: Somewhat better prediction. :P */
static void simulate_input(netplay_t *netplay)
{
   size_t ptr  = PREV_PTR(netplay->self_ptr);
   size_t prev = PREV_PTR(netplay->read_ptr);

   memcpy(netplay->buffer[ptr].simulated_input_state,
         netplay->buffer[prev].real_input_state,
         sizeof(netplay->buffer[prev].real_input_state));

   netplay->buffer[ptr].is_simulated = true;
   netplay->buffer[ptr].used_real = false;
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
static bool netplay_poll(netplay_t *netplay)
{
   int res;

   if (!netplay->has_connection)
      return false;

   netplay->can_poll = false;

   if (!get_self_input_state(netplay))
      return false;

   /* We skip reading the first frame so the host has a chance to grab 
    * our host info so we don't block forever :') */
   if (netplay->frame_count == 0)
   {
      netplay->buffer[0].used_real        = true;
      netplay->buffer[0].is_simulated     = false;

      memset(netplay->buffer[0].real_input_state,
            0, sizeof(netplay->buffer[0].real_input_state));

      netplay->read_ptr                   = NEXT_PTR(netplay->read_ptr);
      netplay->read_frame_count++;
      return true;
   }

   /* We might have reached the end of the buffer, where we 
    * simply have to block. */
   res = poll_input(netplay, netplay->other_ptr == netplay->self_ptr);
   if (res == -1)
   {
      netplay->has_connection = false;
      warn_hangup();
      return false;
   }

   if (res == 1)
   {
      uint32_t first_read = netplay->read_frame_count;
      do 
      {
         uint32_t buffer[UDP_FRAME_PACKETS * UDP_WORDS_PER_FRAME];
         if (!receive_data(netplay, buffer, sizeof(buffer)))
         {
            warn_hangup();
            netplay->has_connection = false;
            return false;
         }
         parse_packet(netplay, buffer, UDP_FRAME_PACKETS);

      } while ((netplay->read_frame_count <= netplay->frame_count) && 
            poll_input(netplay, (netplay->other_ptr == netplay->self_ptr) && 
               (first_read == netplay->read_frame_count)) == 1);
   }
   else
   {
      /* Cannot allow this. Should not happen though. */
      if (netplay->self_ptr == netplay->other_ptr)
      {
         warn_hangup();
         return false;
      }
   }

   if (netplay->read_ptr != netplay->self_ptr)
      simulate_input(netplay);
   else
      netplay->buffer[PREV_PTR(netplay->self_ptr)].used_real = true;

   return true;
}

void input_poll_net(void)
{
   netplay_t *netplay = (netplay_t*)netplay_data;
   if (!netplay_should_skip(netplay) && netplay_can_poll(netplay))
      netplay_poll(netplay);
}

void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   netplay_t *netplay = (netplay_t*)netplay_data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.frame_cb(data, width, height, pitch);
}

void audio_sample_net(int16_t left, int16_t right)
{
   netplay_t *netplay = (netplay_t*)netplay_data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.sample_cb(left, right);
}

size_t audio_sample_batch_net(const int16_t *data, size_t frames)
{
   netplay_t *netplay = (netplay_t*)netplay_data;
   if (!netplay_should_skip(netplay))
      return netplay->cbs.sample_batch_cb(data, frames);
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
static bool netplay_is_alive(netplay_t *netplay)
{
   if (!netplay)
      return false;
   return netplay->has_connection;
}

static bool netplay_flip_port(netplay_t *netplay, bool port)
{
   size_t frame = netplay->frame_count;

   if (netplay->flip_frame == 0)
      return port;

   if (netplay->is_replay)
      frame = netplay->tmp_frame_count;

   return port ^ netplay->flip ^ (frame < netplay->flip_frame);
}

static int16_t netplay_input_state(netplay_t *netplay,
      bool port, unsigned device,
      unsigned idx, unsigned id)
{
   size_t ptr = netplay->is_replay ? 
      netplay->tmp_ptr : PREV_PTR(netplay->self_ptr);

   const uint32_t *curr_input_state = netplay->buffer[ptr].self_state;

   if (netplay->port == (netplay_flip_port(netplay, port) ? 1 : 0))
   {
      if (netplay->buffer[ptr].is_simulated)
         curr_input_state = netplay->buffer[ptr].simulated_input_state;
      else
         curr_input_state = netplay->buffer[ptr].real_input_state;
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
   netplay_t *netplay = (netplay_t*)netplay_data;

   if (netplay_is_alive(netplay))
   {
      /* Only two players for now. */
      if (port > 1)
         return 0;

      return netplay_input_state(netplay, port, device, idx, id);
   }
   return netplay->cbs.state_cb(port, device, idx, id);
}

#ifndef HAVE_SOCKET_LEGACY
/* Custom inet_ntop. Win32 doesn't seem to support this ... */
void np_log_connection(const struct sockaddr_storage *their_addr,
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
      char msg[512] = {0};

      snprintf(msg, sizeof(msg), "Got connection from: \"%s (%s)\" (#%u)",
            nick, str, slot);
      runloop_msg_queue_push(msg, 1, 180, false);
      RARCH_LOG("%s\n", msg);
   }
}
#endif

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

      if (!spectate)
      {
         int new_fd = accept(fd, other_addr, &addr_size);
         if (new_fd < 0)
         {
            ret = false;
            goto end;
         }

         socket_close(fd);
         fd = new_fd;
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
   char port_buf[16]               = {0};
   bool ret                        = false;
   const struct addrinfo *tmp_info = NULL;
   struct addrinfo *res            = NULL;
   struct addrinfo hints           = {0};

   hints.ai_socktype = SOCK_STREAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo_retro(server, port_buf, &hints, &res) < 0)
      return false;

   if (!res)
      return false;

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

static bool init_udp_socket(netplay_t *netplay, const char *server,
      uint16_t port)
{
   int fd = socket_init((void**)&netplay->addr, port, server, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   netplay->udp_fd = fd;

   if (!server)
   {
      /* Not sure if we have to do this for UDP, but hey :) */
      if (!socket_bind(netplay->udp_fd, (void*)netplay->addr))
      {
         RARCH_ERR("Failed to bind socket.\n");
         socket_close(netplay->udp_fd);
         netplay->udp_fd = -1;
      }

      freeaddrinfo_retro(netplay->addr);
      netplay->addr = NULL;
   }

   return true;

error:
   RARCH_ERR("Failed to initialize socket.\n");
   return false;
}

static bool init_socket(netplay_t *netplay, const char *server, uint16_t port)
{
   if (!network_init())
      return false;

   if (!init_tcp_socket(netplay, server, port, netplay->spectate.enabled))
      return false;
   if (!netplay->spectate.enabled && !init_udp_socket(netplay, server, port))
      return false;

   return true;
}

/**
 * netplay_new:
 * @server               : IP address of server.
 * @port                 : Port of server.
 * @frames               : Amount of lag frames.
 * @cb                   : Libretro callbacks.
 * @spectate             : If true, enable spectator mode.
 * @nick                 : Nickname of user.
 *
 * Creates a new netplay handle. A NULL host means we're 
 * hosting (user 1).
 *
 * Returns: new netplay handle.
 **/
netplay_t *netplay_new(const char *server, uint16_t port,
      unsigned frames, const struct retro_callbacks *cb,
      bool spectate,
      const char *nick)
{
   netplay_t *netplay = NULL;

   if (frames > UDP_FRAME_PACKETS)
      frames = UDP_FRAME_PACKETS;

   netplay = (netplay_t*)calloc(1, sizeof(*netplay));
   if (!netplay)
      return NULL;

   netplay->fd                = -1;
   netplay->udp_fd            = -1;
   netplay->cbs               = *cb;
   netplay->port              = server ? 0 : 1;
   netplay->spectate.enabled  = spectate;
   netplay->is_server         = server == NULL;
   strlcpy(netplay->nick, nick, sizeof(netplay->nick));

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
   if (netplay->udp_fd >= 0)
      socket_close(netplay->udp_fd);

   free(netplay);
   return NULL;
}

static bool netplay_send_raw_cmd(netplay_t *netplay, uint32_t cmd,
      const void *data, size_t size)
{
   cmd = (cmd << 16) | (size & 0xffff);
   cmd = htonl(cmd);

   if (!socket_send_all_blocking(netplay->fd, &cmd, sizeof(cmd), false))
      return false;

   if (!socket_send_all_blocking(netplay->fd, data, size, false))
      return false;

   return true;
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
   bool require_sync       = !!(flags & CMD_OPT_REQUIRE_SYNC);

   assert(netplay);

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

   if(require_sync && check_netplay_synched(netplay))
   {
      msg = "Cannot %s while host and client are not in sync.";
      goto error;
   }

   if(netplay_send_raw_cmd(netplay, cmd, data, sz)) {
      if(netplay_get_response(netplay))
         runloop_msg_queue_push(success_msg, 1, 180, false);
      else
      {
         msg = "Failed to send command \"%s\"";
         goto error;
      }
   }
   return true;

error:
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
   uint32_t flip_frame = netplay->frame_count + 2 * UDP_FRAME_PACKETS;
   uint32_t flip_frame_net = htonl(flip_frame);
   bool command = netplay_command(
      netplay, NETPLAY_CMD_FLIP_PLAYERS,
      &flip_frame_net, sizeof flip_frame_net,
      CMD_OPT_HOST_ONLY | CMD_OPT_REQUIRE_SYNC,
      "flip users", "Succesfully flipped users.\n");
   
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

   socket_close(netplay->fd);

   if (netplay->spectate.enabled)
   {
      for (i = 0; i < MAX_SPECTATORS; i++)
         if (netplay->spectate.fds[i] >= 0)
            socket_close(netplay->spectate.fds[i]);

      free(netplay->spectate.input);
   }
   else
   {
      socket_close(netplay->udp_fd);

      for (i = 0; i < netplay->buffer_size; i++)
         free(netplay->buffer[i].state);

      free(netplay->buffer);
   }

   if (netplay->addr)
      freeaddrinfo_retro(netplay->addr);

   free(netplay);
}


static void netplay_set_spectate_input(netplay_t *netplay, int16_t input)
{
   if (netplay->spectate.input_ptr >= netplay->spectate.input_sz)
   {
      netplay->spectate.input_sz++;
      netplay->spectate.input_sz *= 2;
      netplay->spectate.input = (uint16_t*)realloc(netplay->spectate.input,
            netplay->spectate.input_sz * sizeof(uint16_t));
   }

   netplay->spectate.input[netplay->spectate.input_ptr++] = swap_if_big16(input);
}

int16_t input_state_spectate(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   netplay_t *netplay = (netplay_t*)netplay_data;
   int16_t res        = netplay->cbs.state_cb(port, device, idx, id);

   netplay_set_spectate_input(netplay, res);
   return res;
}

static int16_t netplay_get_spectate_input(netplay_t *netplay, bool port,
      unsigned device, unsigned idx, unsigned id)
{
   int16_t inp;
   retro_ctx_input_state_info_t input_info;

   if (socket_receive_all_blocking(netplay->fd, (char*)&inp, sizeof(inp)))
      return swap_if_big16(inp);

   RARCH_ERR("Connection with host was cut.\n");
   runloop_msg_queue_push("Connection with host was cut.", 1, 180, true);

   input_info.cb = netplay->cbs.state_cb;

   core_set_input_state(&input_info);

   return netplay->cbs.state_cb(port, device, idx, id);
}

int16_t input_state_spectate_client(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   return netplay_get_spectate_input((netplay_t*)netplay_data, port,
         device, idx, id);
}

/**
 * netplay_pre_frame:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay.
 * Call this before running retro_run().
 **/
void netplay_pre_frame(netplay_t *netplay)
{
   assert(netplay && netplay->net_cbs->pre_frame);
   netplay->net_cbs->pre_frame(netplay);
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
   assert(netplay && netplay->net_cbs->post_frame);
   netplay->net_cbs->post_frame(netplay);
}

void deinit_netplay(void)
{
   netplay_t *netplay = (netplay_t*)netplay_data;
   if (netplay)
      netplay_free(netplay);
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

bool init_netplay(void)
{
   struct retro_callbacks cbs = {0};
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();

   if (!global->netplay.enable)
      return false;

   if (bsv_movie_ctl(BSV_MOVIE_CTL_START_PLAYBACK, NULL))
   {
      RARCH_WARN("%s\n",
            msg_hash_to_str(MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED));
      return false;
   }

   core_set_default_callbacks(&cbs);

   if (*global->netplay.server)
   {
      RARCH_LOG("Connecting to netplay host...\n");
      global->netplay.is_client = true;
   }
   else
      RARCH_LOG("Waiting for client...\n");

   netplay_data = (netplay_t*)netplay_new(
         global->netplay.is_client ? global->netplay.server : NULL,
         global->netplay.port ? global->netplay.port : RARCH_DEFAULT_PORT,
         global->netplay.sync_frames, &cbs, global->netplay.is_spectate,
         settings->username);

   if (netplay_data)
      return true;

   global->netplay.is_client = false;
   RARCH_WARN("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_NETPLAY_FAILED),
         0, 180, false);
   return false;
}

bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   if (!netplay_data)
      return false;

   switch (state)
   {
      case RARCH_NETPLAY_CTL_IS_DATA_INITED:
         return true;
      case RARCH_NETPLAY_CTL_POST_FRAME:
         netplay_post_frame((netplay_t*)netplay_data);
         break;
      case RARCH_NETPLAY_CTL_PRE_FRAME:
         netplay_pre_frame((netplay_t*)netplay_data);
         break;
      case RARCH_NETPLAY_CTL_FLIP_PLAYERS:
         {
            bool *state = (bool*)data;
            if (*state)
               netplay_flip_users((netplay_t*)netplay_data);
         }
         break;
      case RARCH_NETPLAY_CTL_FULLSCREEN_TOGGLE:
         {
            bool *state = (bool*)data;
            if (*state)
               command_event(EVENT_CMD_FULLSCREEN_TOGGLE, NULL);
         }
         break;
      default:
      case RARCH_NETPLAY_CTL_NONE:
         break;
   }

   return false;
}
