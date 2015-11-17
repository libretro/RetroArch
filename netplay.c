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

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "ws2_32")
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <retro_endianness.h>

#include "netplay.h"
#include "general.h"
#include "autosave.h"
#include "dynamic.h"
#include "msg_hash.h"
#include "system.h"
#include "runloop.h"

struct delta_frame
{
   void *state;

   uint16_t real_input_state;
   uint16_t simulated_input_state;
   uint16_t self_state;

   bool is_simulated;
   bool used_real;
};

#define UDP_FRAME_PACKETS 16
#define MAX_SPECTATORS 16

#define PREV_PTR(x) ((x) == 0 ? netplay->buffer_size - 1 : (x) - 1)
#define NEXT_PTR(x) ((x + 1) % netplay->buffer_size)

struct netplay
{
   char nick[32];
   char other_nick[32];
   struct sockaddr_storage other_addr;

   struct retro_callbacks cbs;
   /* TCP connection for state sending, etc. Also used for commands */
   int fd;
   /* UDP connection for game state updates. */
   int udp_fd;
   /* Which port is governed by netplay (other user)? */
   unsigned port;
   bool has_connection;

   struct delta_frame *buffer;
   size_t buffer_size;

   /* Pointer where we are now. */
   size_t self_ptr; 
   /* Points to the last reliable state that self ever had. */
   size_t other_ptr;
   /* Pointer to where we are reading. 
    * Generally, other_ptr <= read_ptr <= self_ptr. */
   size_t read_ptr;
   /* A temporary pointer used on replay. */
   size_t tmp_ptr;

   size_t state_size;

   /* Are we replaying old frames? */
   bool is_replay;
   /* We don't want to poll several times on a frame. */
   bool can_poll;

   /* To compat UDP packet loss we also send 
    * old data along with the packets. */
   uint32_t packet_buffer[UDP_FRAME_PACKETS * 2];
   uint32_t frame_count;
   uint32_t read_frame_count;
   uint32_t other_frame_count;
   uint32_t tmp_frame_count;
   struct addrinfo *addr;
   struct sockaddr_storage their_addr;
   bool has_client_addr;

   unsigned timeout_cnt;

   /* Spectating. */
   bool spectate;
   bool spectate_client;
   int spectate_fds[MAX_SPECTATORS];
   uint16_t *spectate_input;
   size_t spectate_input_ptr;
   size_t spectate_input_size;

   /* User flipping
    * Flipping state. If ptr >= flip_frame, we apply the flip.
    * If not, we apply the opposite, effectively creating a trigger point.
    * To avoid collition we need to make sure our client/host is synced up 
    * well after flip_frame before allowing another flip. */
   bool flip;
   uint32_t flip_frame;

   /* Netplay pausing
    */
   bool pause;
   uint32_t pause_frame;
};

enum {
   CMD_OPT_ALLOWED_IN_SPECTATE_MODE = 0x1,
   CMD_OPT_REQUIRE_ACK              = 0x2,
   CMD_OPT_HOST_ONLY                = 0x4,
   CMD_OPT_CLIENT_ONLY              = 0x8,
   CMD_OPT_REQUIRE_SYNC             = 0x10,
};

/**
 * warn_hangup:
 *
 * Warns that netplay has disconnected.
 **/
static void warn_hangup(void)
{
   RARCH_WARN("Netplay has disconnected. Will continue without connection ...\n");
   rarch_main_msg_queue_push("Netplay has disconnected. Will continue without connection.", 0, 480, false);
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
      if (sendto(netplay->udp_fd, (const char*)netplay->packet_buffer,
               sizeof(netplay->packet_buffer), 0, addr,
#ifdef ANDROID
               sizeof(struct sockaddr_in6)
#else
               sizeof(struct sockaddr_in)
#endif
         )
            != sizeof(netplay->packet_buffer))
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
   uint32_t state          = 0;
   struct delta_frame *ptr = &netplay->buffer[netplay->self_ptr];
   driver_t *driver        = driver_get_ptr();
   settings_t *settings    = config_get_ptr();

   if (!driver->block_libretro_input && netplay->frame_count > 0)
   {
      unsigned i;

      /* First frame we always give zero input since relying on 
       * input from first frame screws up when we use -F 0. */
      retro_input_state_t cb = netplay->cbs.state_cb;
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         int16_t tmp = cb(settings->input.netplay_client_swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_JOYPAD, 0, i);
         state |= tmp ? 1 << i : 0;
      }

      for (i = RARCH_FIRST_CUSTOM_BIND; i < RARCH_FIRST_META_KEY; i++)
      {
         int16_t tmp = cb(settings->input.netplay_client_swap_input ?
               0 : !netplay->port,
               RETRO_DEVICE_ANALOG, 0, i);
         state |= tmp ? 1 << i : 0;
      }
   }

   memmove(netplay->packet_buffer, netplay->packet_buffer + 2,
         sizeof (netplay->packet_buffer) - 2 * sizeof(uint32_t));
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * 2] = htonl(netplay->frame_count); 
   netplay->packet_buffer[(UDP_FRAME_PACKETS - 1) * 2 + 1] = htonl(state);

   if (!send_chunk(netplay))
   {
      warn_hangup();
      netplay->has_connection = false;
      return false;
   }

   ptr->self_state = state;
   netplay->self_ptr = NEXT_PTR(netplay->self_ptr);
   return true;
}

static bool netplay_cmd_ack(netplay_t *netplay)
{
   uint32_t cmd = htonl(NETPLAY_CMD_ACK);
   return socket_send_all_blocking(netplay->fd, &cmd, sizeof(cmd));
}

static bool netplay_cmd_nak(netplay_t *netplay)
{
   uint32_t cmd = htonl(NETPLAY_CMD_NAK);
   return socket_send_all_blocking(netplay->fd, &cmd, sizeof(cmd));
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
   uint32_t cmd, flip_frame;
   size_t cmd_size;

   if (!socket_receive_all_blocking(netplay->fd, &cmd, sizeof(cmd)))
      return false;

   cmd = ntohl(cmd);

   cmd_size = cmd & 0xffff;
   cmd      = cmd >> 16;

   switch (cmd)
   {
      case NETPLAY_CMD_FLIP_PLAYERS:
         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("CMD_FLIP_PLAYERS has unexpected command size.\n");
            return netplay_cmd_nak(netplay);
         }

         if (!socket_receive_all_blocking(netplay->fd, &flip_frame, sizeof(flip_frame)))
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
         rarch_main_msg_queue_push("Netplay users are flipped.", 1, 180, false);

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

      case NETPLAY_CFG_SWAP_INPUT:
         RARCH_ERR("NETPLAY_CFG_SWAP_INPUT unimplemented.\n");
         return netplay_cmd_nak(netplay);

      case NETPLAY_CFG_DELAY_FRAMES:
         RARCH_ERR("NETPLAY_CFG_DELAY_FRAMES unimplemented.\n");
         return netplay_cmd_nak(netplay);

      case NETPLAY_CFG_CHEATS:
         RARCH_ERR("NETPLAY_CFG_CHEATS unimplemented.\n");
         return netplay_cmd_nak(netplay);

      case NETPLAY_CMD_PAUSE:
         event_command(EVENT_CMD_PAUSE);
         return netplay_cmd_ack(netplay);

      case NETPLAY_CMD_RESUME:
         event_command(EVENT_CMD_UNPAUSE);
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
   int max_fd        = (netplay->fd > netplay->udp_fd ? netplay->fd : netplay->udp_fd) + 1;
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

   for (i = 0; i < size * 2; i++)
      buffer[i] = ntohl(buffer[i]);

   for (i = 0; i < size && netplay->read_frame_count <= netplay->frame_count; i++)
   {
      uint32_t frame = buffer[2 * i + 0];
      uint32_t state = buffer[2 * i + 1];

      if (frame != netplay->read_frame_count)
         continue;

      netplay->buffer[netplay->read_ptr].is_simulated = false;
      netplay->buffer[netplay->read_ptr].real_input_state = state;
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

   netplay->buffer[ptr].simulated_input_state = 
      netplay->buffer[prev].real_input_state;
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
      netplay->buffer[0].real_input_state = 0;
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
         uint32_t buffer[UDP_FRAME_PACKETS * 2];
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
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (!netplay_should_skip(netplay) && netplay_can_poll(netplay))
      netplay_poll(netplay);
}

void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.frame_cb(data, width, height, pitch);
}

void audio_sample_net(int16_t left, int16_t right)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.sample_cb(left, right);
}

size_t audio_sample_batch_net(const int16_t *data, size_t frames)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
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

static int16_t netplay_input_state(netplay_t *netplay, bool port, unsigned device,
      unsigned idx, unsigned id)
{
   size_t ptr = netplay->is_replay ? 
      netplay->tmp_ptr : PREV_PTR(netplay->self_ptr);
   uint16_t curr_input_state = netplay->buffer[ptr].self_state;

   if (netplay->port == (netplay_flip_port(netplay, port) ? 1 : 0))
   {
      if (netplay->buffer[ptr].is_simulated)
         curr_input_state = netplay->buffer[ptr].simulated_input_state;
      else
         curr_input_state = netplay->buffer[ptr].real_input_state;
   }

   return ((1 << id) & curr_input_state) ? 1 : 0;
}

int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (netplay_is_alive(netplay))
      return netplay_input_state(netplay, port, device, idx, id);
   return netplay->cbs.state_cb(port, device, idx, id);
}

#ifndef HAVE_SOCKET_LEGACY
/* Custom inet_ntop. Win32 doesn't seem to support this ... */
static void log_connection(const struct sockaddr_storage *their_addr,
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

   if (their_addr->ss_family == AF_INET)
   {
      struct sockaddr_in in;

      str = buf_v4;

      memset(&in, 0, sizeof(in));
      in.sin_family = AF_INET;
      memcpy(&in.sin_addr, &u.v4->sin_addr, sizeof(struct in_addr));

      getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in),
            buf_v4, sizeof(buf_v4),
            NULL, 0, NI_NUMERICHOST);
   }
   else if (their_addr->ss_family == AF_INET6)
   {
      struct sockaddr_in6 in;

      str = buf_v6;
      memset(&in, 0, sizeof(in));
      in.sin6_family = AF_INET6;
      memcpy(&in.sin6_addr, &u.v6->sin6_addr, sizeof(struct in6_addr));

      getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in6),
            buf_v6, sizeof(buf_v6), NULL, 0, NI_NUMERICHOST);
   }

   if (str)
   {
      char msg[512] = {0};

      snprintf(msg, sizeof(msg), "Got connection from: \"%s (%s)\" (#%u)",
            nick, str, slot);
      rarch_main_msg_queue_push(msg, 1, 180, false);
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
      if (connect(fd, res->ai_addr, res->ai_addrlen) < 0)
      {
         ret = false;
         goto end;
      }
   }
   else
   {
      int yes = 1;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(int));

      if (bind(fd, res->ai_addr, res->ai_addrlen) < 0 ||
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
   struct addrinfo hints, *res     = NULL;

   memset(&hints, 0, sizeof(hints));

#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif

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
      int fd;
      if ((fd = init_tcp_connection(tmp_info, server, netplay->spectate,
               (struct sockaddr*)&netplay->other_addr,
               sizeof(netplay->other_addr))) >= 0)
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
   char port_buf[16]     = {0};
   struct addrinfo hints = {0};

   memset(&hints, 0, sizeof(hints));
#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_DGRAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   if (getaddrinfo_retro(server, port_buf, &hints, &netplay->addr) < 0)
      return false;

   if (!netplay->addr)
      return false;

   netplay->udp_fd = socket(netplay->addr->ai_family,
         netplay->addr->ai_socktype, netplay->addr->ai_protocol);

   if (netplay->udp_fd < 0)
   {
      RARCH_ERR("Failed to initialize socket.\n");
      return false;
   }

   if (!server)
   {
      /* Not sure if we have to do this for UDP, but hey :) */
      int yes = 1;

      setsockopt(netplay->udp_fd, SOL_SOCKET, SO_REUSEADDR,
            (const char*)&yes, sizeof(int));

      if (bind(netplay->udp_fd, netplay->addr->ai_addr,
               netplay->addr->ai_addrlen) < 0)
      {
         RARCH_ERR("Failed to bind socket.\n");
         socket_close(netplay->udp_fd);
         netplay->udp_fd = -1;
      }

      freeaddrinfo_retro(netplay->addr);
      netplay->addr = NULL;
   }

   return true;
}

static bool init_socket(netplay_t *netplay, const char *server, uint16_t port)
{
   if (!network_init())
      return false;

   if (!init_tcp_socket(netplay, server, port, netplay->spectate))
      return false;
   if (!netplay->spectate && !init_udp_socket(netplay, server, port))
      return false;

   return true;
}


/**
 * implementation_magic_value:
 *
 * Not really a hash, but should be enough to differentiate 
 * implementations from each other.
 *
 * Subtle differences in the implementation will not be possible to spot.
 * The alternative would have been checking serialization sizes, but it 
 * was troublesome for cross platform compat.
 **/
static uint32_t implementation_magic_value(void)
{
   size_t i, len;
   uint32_t res                        = 0;
   const char *ver                     = PACKAGE_VERSION;
   unsigned api                        = core.retro_api_version();
   rarch_system_info_t *info           = rarch_system_info_get_ptr();
   const char *lib                     = info ? info->info.library_name : NULL;

   res |= api;

   len = strlen(lib);
   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   lib = info->info.library_version;
   len = strlen(lib);

   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   len = strlen(ver);
   for (i = 0; i < len; i++)
      res ^= ver[i] << ((i & 0xf) + 16);

   return res;
}

static bool send_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size = strlen(netplay->nick);

   if (!socket_send_all_blocking(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to send nick size.\n");
      return false;
   }

   if (!socket_send_all_blocking(fd, netplay->nick, nick_size))
   {
      RARCH_ERR("Failed to send nick.\n");
      return false;
   }

   return true;
}

static bool get_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size;

   if (!socket_receive_all_blocking(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to receive nick size from host.\n");
      return false;
   }

   if (nick_size >= sizeof(netplay->other_nick))
   {
      RARCH_ERR("Invalid nick size.\n");
      return false;
   }

   if (!socket_receive_all_blocking(fd, netplay->other_nick, nick_size))
   {
      RARCH_ERR("Failed to receive nick.\n");
      return false;
   }

   return true;
}

static bool send_info(netplay_t *netplay)
{
   unsigned sram_size;
   char msg[512]      = {0};
   void *sram         = NULL;
   uint32_t header[3] = {0};
   global_t *global   = global_get_ptr();
   
   header[0] = htonl(global->content_crc);
   header[1] = htonl(implementation_magic_value());
   header[2] = htonl(core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM));

   if (!socket_send_all_blocking(netplay->fd, header, sizeof(header)))
      return false;

   if (!send_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to send nick to host.\n");
      return false;
   }

   /* Get SRAM data from User 1. */
   sram      = core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   sram_size = core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

   if (!socket_receive_all_blocking(netplay->fd, sram, sram_size))
   {
      RARCH_ERR("Failed to receive SRAM data from host.\n");
      return false;
   }

   if (!get_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to receive nick from host.\n");
      return false;
   }

   snprintf(msg, sizeof(msg), "Connected to: \"%s\"", netplay->other_nick);
   RARCH_LOG("%s\n", msg);
   rarch_main_msg_queue_push(msg, 1, 180, false);

   return true;
}

static bool get_info(netplay_t *netplay)
{
   unsigned sram_size;
   uint32_t header[3];
   const void *sram = NULL;
   global_t *global = global_get_ptr();

   if (!socket_receive_all_blocking(netplay->fd, header, sizeof(header)))
   {
      RARCH_ERR("Failed to receive header from client.\n");
      return false;
   }

   if (global->content_crc != ntohl(header[0]))
   {
      RARCH_ERR("Content CRC32s differ. Cannot use different games.\n");
      return false;
   }

   if (implementation_magic_value() != ntohl(header[1]))
   {
      RARCH_ERR("Implementations differ, make sure you're using exact same libretro implementations and RetroArch version.\n");
      return false;
   }

   if (core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) != ntohl(header[2]))
   {
      RARCH_ERR("Content SRAM sizes do not correspond.\n");
      return false;
   }

   if (!get_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to get nickname from client.\n");
      return false;
   }

   /* Send SRAM data to our User 2. */
   sram      = core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   sram_size = core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

   if (!socket_send_all_blocking(netplay->fd, sram, sram_size))
   {
      RARCH_ERR("Failed to send SRAM data to client.\n");
      return false;
   }

   if (!send_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to send nickname to client.\n");
      return false;
   }

#ifndef HAVE_SOCKET_LEGACY
   log_connection(&netplay->other_addr, 0, netplay->other_nick);
#endif

   return true;
}

static uint32_t *bsv_header_generate(size_t *size, uint32_t magic)
{
   uint32_t *header, bsv_header[4] = {0};
   size_t serialize_size = core.retro_serialize_size();
   size_t header_size = sizeof(bsv_header) + serialize_size;
   global_t *global = global_get_ptr();

   *size = header_size;

   header = (uint32_t*)malloc(header_size);
   if (!header)
      return NULL;

   bsv_header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   bsv_header[SERIALIZER_INDEX] = swap_if_big32(magic);
   bsv_header[CRC_INDEX]        = swap_if_big32(global->content_crc);
   bsv_header[STATE_SIZE_INDEX] = swap_if_big32(serialize_size);

   if (serialize_size && !core.retro_serialize(header + 4, serialize_size))
   {
      free(header);
      return NULL;
   }

   memcpy(header, bsv_header, sizeof(bsv_header));
   return header;
}

static bool bsv_parse_header(const uint32_t *header, uint32_t magic)
{
   uint32_t in_crc, in_magic, in_state_size;
   uint32_t in_bsv = swap_if_little32(header[MAGIC_INDEX]);
   global_t *global = global_get_ptr();

   if (in_bsv != BSV_MAGIC)
   {
      RARCH_ERR("BSV magic mismatch, got 0x%x, expected 0x%x.\n",
            in_bsv, BSV_MAGIC);
      return false;
   }

   in_magic = swap_if_big32(header[SERIALIZER_INDEX]);
   if (in_magic != magic)
   {
      RARCH_ERR("Magic mismatch, got 0x%x, expected 0x%x.\n", in_magic, magic);
      return false;
   }

   in_crc = swap_if_big32(header[CRC_INDEX]);
   if (in_crc != global->content_crc)
   {
      RARCH_ERR("CRC32 mismatch, got 0x%x, expected 0x%x.\n", in_crc,
            global->content_crc);
      return false;
   }

   in_state_size = swap_if_big32(header[STATE_SIZE_INDEX]);
   if (in_state_size != core.retro_serialize_size())
   {
      RARCH_ERR("Serialization size mismatch, got 0x%x, expected 0x%x.\n",
            (unsigned)in_state_size, (unsigned)core.retro_serialize_size());
      return false;
   }

   return true;
}

static bool get_info_spectate(netplay_t *netplay)
{
   size_t save_state_size, size;
   void *buf          = NULL;
   uint32_t header[4] = {0};
   char msg[512]      = {0};
   bool ret           = true;

   if (!send_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to send nickname to host.\n");
      return false;
   }

   if (!get_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to receive nickname from host.\n");
      return false;
   }

   snprintf(msg, sizeof(msg), "Connected to \"%s\"", netplay->other_nick);
   rarch_main_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);


   if (!socket_receive_all_blocking(netplay->fd, header, sizeof(header)))
   {
      RARCH_ERR("Cannot get header from host.\n");
      return false;
   }

   save_state_size = core.retro_serialize_size();
   if (!bsv_parse_header(header, implementation_magic_value()))
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

static bool init_buffers(netplay_t *netplay)
{
   unsigned i;

   if (!netplay)
      return false;

   netplay->buffer = (struct delta_frame*)calloc(netplay->buffer_size,
         sizeof(*netplay->buffer));
   
   if (!netplay->buffer)
      return false;

   netplay->state_size = core.retro_serialize_size();

   for (i = 0; i < netplay->buffer_size; i++)
   {
      netplay->buffer[i].state = malloc(netplay->state_size);

      if (!netplay->buffer[i].state)
         return false;

      netplay->buffer[i].is_simulated = true;
   }

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
   unsigned i;
   netplay_t *netplay = NULL;

   if (frames > UDP_FRAME_PACKETS)
      frames = UDP_FRAME_PACKETS;

   netplay = (netplay_t*)calloc(1, sizeof(*netplay));
   if (!netplay)
      return NULL;

   netplay->fd              = -1;
   netplay->udp_fd          = -1;
   netplay->cbs             = *cb;
   netplay->port            = server ? 0 : 1;
   netplay->spectate        = spectate;
   netplay->spectate_client = server != NULL;
   strlcpy(netplay->nick, nick, sizeof(netplay->nick));

   if (!init_socket(netplay, server, port))
   {
      free(netplay);
      return NULL;
   }

   if (spectate)
   {
      if (server)
      {
         if (!get_info_spectate(netplay))
            goto error;
      }

      for (i = 0; i < MAX_SPECTATORS; i++)
         netplay->spectate_fds[i] = -1;
   }
   else
   {
      if (server)
      {
         if (!send_info(netplay))
            goto error;
      }
      else
      {
         if (!get_info(netplay))
            goto error;
      }

      netplay->buffer_size = frames + 1;

      if (!init_buffers(netplay))
         goto error;

      netplay->has_connection = true;
   }

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

   if (!socket_send_all_blocking(netplay->fd, &cmd, sizeof(cmd)))
      return false;

   if (!socket_send_all_blocking(netplay->fd, data, size))
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
   assert(netplay);
   const char* msg         = NULL;

   bool allowed_spectate   = !!(flags & CMD_OPT_ALLOWED_IN_SPECTATE_MODE);
   bool host_only          = !!(flags & CMD_OPT_HOST_ONLY);
   bool require_sync       = !!(flags & CMD_OPT_REQUIRE_SYNC);

   if (netplay->spectate && !allowed_spectate)
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
         rarch_main_msg_queue_push(success_msg, 1, 180, false);
      else
      {
         msg = "Failed to send command \"%s\"";
         goto error;
      }
   }
   return true;
error: ;
   char m[256];
   snprintf(m, 255, msg, command_str);
   RARCH_WARN("%s\n", m);
   rarch_main_msg_queue_push(m, 1, 180, false);
   return false;
}

/**
 * netplay_flip_users:
 * @netplay              : pointer to netplay object
 *
 * On regular netplay, flip who controls user 1 and 2.
 **/
void netplay_flip_users(netplay_t *netplay)
{
   assert(netplay);
   uint32_t flip_frame_net = htonl(netplay->frame_count + 2 * UDP_FRAME_PACKETS);
   bool command = netplay_command(
      netplay, NETPLAY_CMD_FLIP_PLAYERS,
      &flip_frame_net, sizeof flip_frame_net,
      CMD_OPT_HOST_ONLY | CMD_OPT_REQUIRE_SYNC,
      "flip users", "Succesfully flipped users.\n");
   
   if(command)
   {
      netplay->flip       ^= true;
      netplay->flip_frame  = true;
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

   if (netplay->spectate)
   {
      for (i = 0; i < MAX_SPECTATORS; i++)
         if (netplay->spectate_fds[i] >= 0)
            socket_close(netplay->spectate_fds[i]);

      free(netplay->spectate_input);
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

/**
 * netplay_pre_frame_net:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay (normal version).
 **/
static void netplay_pre_frame_net(netplay_t *netplay)
{
   core.retro_serialize(netplay->buffer[netplay->self_ptr].state,
         netplay->state_size);
   netplay->can_poll = true;

   input_poll_net();
}

static void netplay_set_spectate_input(netplay_t *netplay, int16_t input)
{
   if (netplay->spectate_input_ptr >= netplay->spectate_input_size)
   {
      netplay->spectate_input_size++;
      netplay->spectate_input_size *= 2;
      netplay->spectate_input = (uint16_t*)realloc(netplay->spectate_input,
            netplay->spectate_input_size * sizeof(uint16_t));
   }

   netplay->spectate_input[netplay->spectate_input_ptr++] = swap_if_big16(input);
}

int16_t input_state_spectate(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   driver_t *driver = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   int16_t res        = netplay->cbs.state_cb(port, device, idx, id);

   netplay_set_spectate_input(netplay, res);
   return res;
}

static int16_t netplay_get_spectate_input(netplay_t *netplay, bool port,
      unsigned device, unsigned idx, unsigned id)
{
   int16_t inp;

   if (socket_receive_all_blocking(netplay->fd, (char*)&inp, sizeof(inp)))
      return swap_if_big16(inp);

   RARCH_ERR("Connection with host was cut.\n");
   rarch_main_msg_queue_push("Connection with host was cut.", 1, 180, true);

   core.retro_set_input_state(netplay->cbs.state_cb);
   return netplay->cbs.state_cb(port, device, idx, id);
}

int16_t input_state_spectate_client(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   driver_t *driver = driver_get_ptr();
   return netplay_get_spectate_input((netplay_t*)driver->netplay_data, port,
         device, idx, id);
}

/**
 * netplay_pre_frame_spectate:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay (spectate mode version).
 **/
static void netplay_pre_frame_spectate(netplay_t *netplay)
{
   unsigned i;
   uint32_t *header;
   int new_fd, idx, bufsize;
   size_t header_size;
   struct sockaddr_storage their_addr;
   socklen_t addr_size;
   fd_set fds;
   struct timeval tmp_tv = {0};

   if (netplay->spectate_client)
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
      if (netplay->spectate_fds[i] == -1)
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

   if (!get_nickname(netplay, new_fd))
   {
      RARCH_ERR("Failed to get nickname from client.\n");
      socket_close(new_fd);
      return;
   }

   if (!send_nickname(netplay, new_fd))
   {
      RARCH_ERR("Failed to send nickname to client.\n");
      socket_close(new_fd);
      return;
   }

   header = bsv_header_generate(&header_size,
         implementation_magic_value());

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
   netplay->spectate_fds[idx] = new_fd;

#ifndef HAVE_SOCKET_LEGACY
   log_connection(&their_addr, idx, netplay->other_nick);
#endif
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
   if (netplay->spectate)
      netplay_pre_frame_spectate(netplay);
   else
      netplay_pre_frame_net(netplay);
}

/**
 * netplay_post_frame_net:   
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay (normal version).
 * We check if we have new input and replay from recorded input.
 **/
static void netplay_post_frame_net(netplay_t *netplay)
{
   netplay->frame_count++;

   /* Nothing to do... */
   if (netplay->other_frame_count == netplay->read_frame_count)
      return;

   /* Skip ahead if we predicted correctly.
    * Skip until our simulation failed. */
   while (netplay->other_frame_count < netplay->read_frame_count)
   {
      const struct delta_frame *ptr = &netplay->buffer[netplay->other_ptr];

      if ((ptr->simulated_input_state != ptr->real_input_state)
            && !ptr->used_real)
         break;
      netplay->other_ptr = NEXT_PTR(netplay->other_ptr);
      netplay->other_frame_count++;
   }

   if (netplay->other_frame_count < netplay->read_frame_count)
   {
      bool first = true;

      /* Replay frames. */
      netplay->is_replay = true;
      netplay->tmp_ptr = netplay->other_ptr;
      netplay->tmp_frame_count = netplay->other_frame_count;

      core.retro_unserialize(netplay->buffer[netplay->other_ptr].state,
            netplay->state_size);

      while (first || (netplay->tmp_ptr != netplay->self_ptr))
      {
         core.retro_serialize(netplay->buffer[netplay->tmp_ptr].state,
               netplay->state_size);
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         lock_autosave();
#endif
         core.retro_run();
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         unlock_autosave();
#endif
         netplay->tmp_ptr = NEXT_PTR(netplay->tmp_ptr);
         netplay->tmp_frame_count++;
         first = false;
      }

      netplay->other_ptr = netplay->read_ptr;
      netplay->other_frame_count = netplay->read_frame_count;
      netplay->is_replay = false;
   }
}

/**
 * netplay_post_frame_spectate:   
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay (spectate mode version).
 * We check if we have new input and replay from recorded input.
 **/
static void netplay_post_frame_spectate(netplay_t *netplay)
{
   unsigned i;

   if (netplay->spectate_client)
      return;

   for (i = 0; i < MAX_SPECTATORS; i++)
   {
      char msg[PATH_MAX_LENGTH] = {0};

      if (netplay->spectate_fds[i] == -1)
         continue;

      if (socket_send_all_blocking(netplay->spectate_fds[i],
               netplay->spectate_input,
               netplay->spectate_input_ptr * sizeof(int16_t)))
         continue;

      RARCH_LOG("Client (#%u) disconnected ...\n", i);

      snprintf(msg, sizeof(msg), "Client (#%u) disconnected.", i);
      rarch_main_msg_queue_push(msg, 1, 180, false);

      socket_close(netplay->spectate_fds[i]);
      netplay->spectate_fds[i] = -1;
      break;
   }

   netplay->spectate_input_ptr = 0;
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
   if (netplay->spectate)
      netplay_post_frame_spectate(netplay);
   else
      netplay_post_frame_net(netplay);
}

void deinit_netplay(void)
{
   driver_t *driver     = driver_get_ptr();
   netplay_t *netplay = (netplay_t*)driver->netplay_data;
   if (netplay)
      netplay_free(netplay);
   driver->netplay_data = NULL;
}

#define RARCH_DEFAULT_PORT 55435

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
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();

   if (!global->netplay.enable)
      return false;

   if (global->bsv.movie_start_playback)
   {
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED));
      return false;
   }

   retro_set_default_callbacks(&cbs);

   if (*global->netplay.server)
   {
      RARCH_LOG("Connecting to netplay host...\n");
      global->netplay.is_client = true;
   }
   else
      RARCH_LOG("Waiting for client...\n");

   driver->netplay_data = (netplay_t*)netplay_new(
         global->netplay.is_client ? global->netplay.server : NULL,
         global->netplay.port ? global->netplay.port : RARCH_DEFAULT_PORT,
         global->netplay.sync_frames, &cbs, global->netplay.is_spectate,
         settings->username);

   if (driver->netplay_data)
      return true;

   global->netplay.is_client = false;
   RARCH_WARN("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));

   rarch_main_msg_queue_push_new(
         MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED,
         0, 180, false);
   return false;
}

#ifdef HAVE_SOCKET_LEGACY

#undef sockaddr_storage
#undef addrinfo

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define addrinfo addrinfo_retro__

#ifdef _XBOX
/* TODO - implement h_length and h_addrtype */
struct hostent
{
   int h_addrtype;     /* host address type   */
   int h_length;       /* length of addresses */
   char **h_addr_list; /* list of addresses   */
};

static struct hostent *gethostbyname(const char *name)
{
   WSAEVENT event;
   static struct hostent he;
   static struct in_addr addr;
   static char *addr_ptr;
   XNDNS *dns = NULL;

   he.h_addr_list = &addr_ptr;
   addr_ptr = (char*)&addr;

   if (!name)
      return NULL;

   event = WSACreateEvent();
   XNetDnsLookup(name, event, &dns);
   if (!dns)
      goto error;

   WaitForSingleObject((HANDLE)event, INFINITE);
   if (dns->iStatus)
      goto error;

   memcpy(&addr, dns->aina, sizeof(addr));

   WSACloseEvent(event);
   XNetDnsRelease(dns);

   return &he;

error:
   if (event)
      WSACloseEvent(event);
   return NULL;
}
#endif

#endif
