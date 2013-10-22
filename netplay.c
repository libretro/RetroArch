/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "netplay_compat.h"
#include "netplay.h"
#include "general.h"
#include "autosave.h"
#include "dynamic.h"
#include "message.h"
#include <stdlib.h>
#include <string.h>

// Checks if input port/index is controlled by netplay or not.
static bool netplay_is_alive(netplay_t *handle);

static bool netplay_poll(netplay_t *handle);
static int16_t netplay_input_state(netplay_t *handle, bool port, unsigned device, unsigned index, unsigned id);

// If we're fast-forward replaying to resync, check if we should actually show frame.
static bool netplay_should_skip(netplay_t *handle);
static bool netplay_can_poll(netplay_t *handle);
static void netplay_set_spectate_input(netplay_t *handle, int16_t input);

static bool netplay_send_cmd(netplay_t *handle, uint32_t cmd, const void *data, size_t size);
static bool netplay_get_cmd(netplay_t *handle);

#define PREV_PTR(x) ((x) == 0 ? handle->buffer_size - 1 : (x) - 1)
#define NEXT_PTR(x) ((x + 1) % handle->buffer_size)

struct delta_frame
{
   void *state;

   uint16_t real_input_state;
   uint16_t simulated_input_state;
   bool is_simulated;
   uint16_t self_state;
   bool used_real;
};

#define UDP_FRAME_PACKETS 16
#define MAX_SPECTATORS 16

#define NETPLAY_CMD_ACK 0
#define NETPLAY_CMD_NAK 1
#define NETPLAY_CMD_FLIP_PLAYERS 2

struct netplay
{
   char nick[32];
   char other_nick[32];
   struct sockaddr_storage other_addr;

   struct retro_callbacks cbs;
   int fd; // TCP connection for state sending, etc. Also used for commands.
   int udp_fd; // UDP connection for game state updates.
   unsigned port; // Which port is governed by netplay (other player)?
   bool has_connection;

   struct delta_frame *buffer;
   size_t buffer_size;

   size_t self_ptr; // Ptr where we are now.
   size_t other_ptr; // Points to the last reliable state that self ever had.
   size_t read_ptr; // Ptr to where we are reading. Generally, other_ptr <= read_ptr <= self_ptr.
   size_t tmp_ptr; // A temporary pointer used on replay.

   size_t state_size;

   bool is_replay; // Are we replaying old frames?
   bool can_poll; // We don't want to poll several times on a frame.

   uint32_t packet_buffer[UDP_FRAME_PACKETS * 2]; // To compat UDP packet loss we also send old data along with the packets.
   uint32_t frame_count;
   uint32_t read_frame_count;
   uint32_t other_frame_count;
   uint32_t tmp_frame_count;
   struct addrinfo *addr;
   struct sockaddr_storage their_addr;
   bool has_client_addr;

   unsigned timeout_cnt;

   // Spectating.
   bool spectate;
   bool spectate_client;
   int spectate_fds[MAX_SPECTATORS];
   uint16_t *spectate_input;
   size_t spectate_input_ptr;
   size_t spectate_input_size;

   // Player flipping
   // Flipping state. If ptr >= flip_frame, we apply the flip.
   // If not, we apply the opposite, effectively creating a trigger point.
   // To avoid collition we need to make sure our client/host is synced up well after flip_frame
   // before allowing another flip.
   bool flip;
   uint32_t flip_frame;
};

static bool send_all(int fd, const void *data_, size_t size)
{
   const uint8_t *data = (const uint8_t*)data_;
   while (size)
   {
      ssize_t ret = send(fd, CONST_CAST data, size, 0);
      if (ret <= 0)
         return false;

      data += ret;
      size -= ret;
   }

   return true;
}

static bool recv_all(int fd, void *data_, size_t size)
{
   uint8_t *data = (uint8_t*)data_;
   while (size)
   {
      ssize_t ret = recv(fd, NONCONST_CAST data, size, 0);
      if (ret <= 0)
         return false;

      data += ret;
      size -= ret;
   }

   return true;
}

static void warn_hangup(void)
{
   RARCH_WARN("Netplay has disconnected. Will continue without connection ...\n");
   if (g_extern.msg_queue)
      msg_queue_push(g_extern.msg_queue, "Netplay has disconnected. Will continue without connection.", 0, 480);
}

void input_poll_net(void)
{
   if (!netplay_should_skip(g_extern.netplay) && netplay_can_poll(g_extern.netplay))
      netplay_poll(g_extern.netplay);
}

void video_frame_net(const void *data, unsigned width, unsigned height, size_t pitch)
{
   if (!netplay_should_skip(g_extern.netplay))
      g_extern.netplay->cbs.frame_cb(data, width, height, pitch);
}

void audio_sample_net(int16_t left, int16_t right)
{
   if (!netplay_should_skip(g_extern.netplay))
      g_extern.netplay->cbs.sample_cb(left, right);
}

size_t audio_sample_batch_net(const int16_t *data, size_t frames)
{
   if (!netplay_should_skip(g_extern.netplay))
      return g_extern.netplay->cbs.sample_batch_cb(data, frames);
   else
      return frames;
}

int16_t input_state_net(unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (netplay_is_alive(g_extern.netplay))
      return netplay_input_state(g_extern.netplay, port, device, index, id);
   else
      return g_extern.netplay->cbs.state_cb(port, device, index, id);
}

#ifndef HAVE_SOCKET_LEGACY
// Custom inet_ntop. Win32 doesn't seem to support this ...
static void log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick)
{
   union
   {
      const struct sockaddr_storage *storage;
      const struct sockaddr_in *v4;
      const struct sockaddr_in6 *v6;
   } u;
   u.storage = their_addr;

   const char *str = NULL;
   char buf_v4[INET_ADDRSTRLEN] = {0};
   char buf_v6[INET6_ADDRSTRLEN] = {0};

   if (their_addr->ss_family == AF_INET)
   {
      str = buf_v4;
      struct sockaddr_in in;
      memset(&in, 0, sizeof(in));
      in.sin_family = AF_INET;
      memcpy(&in.sin_addr, &u.v4->sin_addr, sizeof(struct in_addr));

      getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in), buf_v4, sizeof(buf_v4),
            NULL, 0, NI_NUMERICHOST);
   }
   else if (their_addr->ss_family == AF_INET6)
   {
      str = buf_v6;
      struct sockaddr_in6 in;
      memset(&in, 0, sizeof(in));
      in.sin6_family = AF_INET6;
      memcpy(&in.sin6_addr, &u.v6->sin6_addr, sizeof(struct in6_addr));

      getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in6),
            buf_v6, sizeof(buf_v6), NULL, 0, NI_NUMERICHOST);
   }

   if (str)
   {
      char msg[512];
      snprintf(msg, sizeof(msg), "Got connection from: \"%s (%s)\" (#%u)", nick, str, slot);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_LOG("%s\n", msg);
   }
}
#endif

static int init_tcp_connection(const struct addrinfo *res, bool server, bool spectate,
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
   else if (spectate)
   {
      int yes = 1;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));

      if (bind(fd, res->ai_addr, res->ai_addrlen) < 0 ||
            listen(fd, MAX_SPECTATORS) < 0)
      {
         ret = false;
         goto end;
      }
   }
   else
   {
      int yes = 1;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));

      if (bind(fd, res->ai_addr, res->ai_addrlen) < 0 ||
            listen(fd, 1) < 0)
      {
         ret = false;
         goto end;
      }

      int new_fd = accept(fd, other_addr, &addr_size);
      if (new_fd < 0)
      {
         ret = false;
         goto end;
      }

      close(fd);
      fd = new_fd;
   }

end:
   if (!ret && fd >= 0)
   {
      close(fd);
      fd = -1;
   }

   return fd;
}

static bool init_tcp_socket(netplay_t *handle, const char *server, uint16_t port, bool spectate)
{
   struct addrinfo hints, *res = NULL;
   memset(&hints, 0, sizeof(hints));

#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif

   hints.ai_socktype = SOCK_STREAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   bool ret = false;
   char port_buf[16];
   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo(server, port_buf, &hints, &res) < 0)
      return false;

   if (!res)
      return false;

   // If "localhost" is used, it is important to check every possible address for ipv4/ipv6.
   const struct addrinfo *tmp_info = res;
   while (tmp_info)
   {
      int fd;
      if ((fd = init_tcp_connection(tmp_info, server, handle->spectate,
               (struct sockaddr*)&handle->other_addr, sizeof(handle->other_addr))) >= 0)
      {
         ret = true;
         handle->fd = fd;
         break;
      }

      tmp_info = tmp_info->ai_next;
   }

   if (res)
      freeaddrinfo(res);

   if (!ret)
      RARCH_ERR("Failed to set up netplay sockets.\n");

   return ret;
}

static bool init_udp_socket(netplay_t *handle, const char *server, uint16_t port)
{
   struct addrinfo hints;
   memset(&hints, 0, sizeof(hints));
#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_DGRAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   char port_buf[16];
   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo(server, port_buf, &hints, &handle->addr) < 0)
      return false;

   if (!handle->addr)
      return false;

   handle->udp_fd = socket(handle->addr->ai_family, handle->addr->ai_socktype, handle->addr->ai_protocol);
   if (handle->udp_fd < 0)
   {
      RARCH_ERR("Failed to init socket...\n");
      return false;
   }

   if (!server)
   {
      // Note sure if we have to do this for UDP, but hey :)
      int yes = 1;
      setsockopt(handle->udp_fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));

      if (bind(handle->udp_fd, handle->addr->ai_addr, handle->addr->ai_addrlen) < 0)
      {
         RARCH_ERR("Failed to bind socket.\n");
         close(handle->udp_fd);
         handle->udp_fd = -1;
      }

      freeaddrinfo(handle->addr);
      handle->addr = NULL;
   }

   return true;
}

// Platform specific socket library init.
bool netplay_init_network(void)
{
   static bool inited = false;
   if (inited)
      return true;

#if defined(_WIN32)
   WSADATA wsaData;
   if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
   {
      WSACleanup();
      return false;
   }
#elif defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
   sys_net_initialize_network();
#else
   signal(SIGPIPE, SIG_IGN); // Do not like SIGPIPE killing our app :(
#endif

   inited = true;
   return true;
}

static bool init_socket(netplay_t *handle, const char *server, uint16_t port)
{
   if (!netplay_init_network())
      return false;

   if (!init_tcp_socket(handle, server, port, handle->spectate))
      return false;
   if (!handle->spectate && !init_udp_socket(handle, server, port))
      return false;

   return true;
}

bool netplay_can_poll(netplay_t *handle)
{
   return handle->can_poll;
}

// Not really a hash, but should be enough to differentiate implementations from each other.
// Subtle differences in the implementation will not be possible to spot.
// The alternative would have been checking serialization sizes, but it was troublesome for cross platform compat.
static uint32_t implementation_magic_value(void)
{
   size_t i;
   uint32_t res = 0;
   unsigned api = pretro_api_version();

   res |= api;

   const char *lib = g_extern.system.info.library_name;
   size_t len = strlen(lib);
   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   lib = g_extern.system.info.library_version;
   len = strlen(lib);
   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   const char *ver = PACKAGE_VERSION;
   len = strlen(ver);
   for (i = 0; i < len; i++)
      res ^= ver[i] << ((i & 0xf) + 16);

   return res;
}

static bool send_nickname(netplay_t *handle, int fd)
{
   uint8_t nick_size = strlen(handle->nick);

   if (!send_all(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to send nick size.\n");
      return false;
   }

   if (!send_all(fd, handle->nick, nick_size))
   {
      RARCH_ERR("Failed to send nick.\n");
      return false;
   }

   return true;
}

static bool get_nickname(netplay_t *handle, int fd)
{
   uint8_t nick_size;

   if (!recv_all(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to receive nick size from host.\n");
      return false;
   }

   if (nick_size >= sizeof(handle->other_nick))
   {
      RARCH_ERR("Invalid nick size.\n");
      return false;
   }

   if (!recv_all(fd, handle->other_nick, nick_size))
   {
      RARCH_ERR("Failed to receive nick.\n");
      return false;
   }

   return true;
}

static bool send_info(netplay_t *handle)
{
   uint32_t header[3] = {
      htonl(g_extern.cart_crc),
      htonl(implementation_magic_value()),
      htonl(pretro_get_memory_size(RETRO_MEMORY_SAVE_RAM))
   };

   if (!send_all(handle->fd, header, sizeof(header)))
      return false;

   if (!send_nickname(handle, handle->fd))
   {
      RARCH_ERR("Failed to send nick to host.\n");
      return false;
   }

   // Get SRAM data from Player 1.
   void *sram = pretro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   unsigned sram_size = pretro_get_memory_size(RETRO_MEMORY_SAVE_RAM);

   if (!recv_all(handle->fd, sram, sram_size))
   {
      RARCH_ERR("Failed to receive SRAM data from host.\n");
      return false;
   }

   if (!get_nickname(handle, handle->fd))
   {
      RARCH_ERR("Failed to receive nick from host.\n");
      return false;
   }

   char msg[512];
   snprintf(msg, sizeof(msg), "Connected to: \"%s\"", handle->other_nick);
   RARCH_LOG("%s\n", msg);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);

   return true;
}

static bool get_info(netplay_t *handle)
{
   uint32_t header[3];

   if (!recv_all(handle->fd, header, sizeof(header)))
   {
      RARCH_ERR("Failed to receive header from client.\n");
      return false;
   }

   if (g_extern.cart_crc != ntohl(header[0]))
   {
      RARCH_ERR("Cart CRC32s differ. Cannot use different games.\n");
      return false;
   }

   if (implementation_magic_value() != ntohl(header[1]))
   {
      RARCH_ERR("Implementations differ, make sure you're using exact same libretro implementations and RetroArch version.\n");
      return false;
   }

   if (pretro_get_memory_size(RETRO_MEMORY_SAVE_RAM) != ntohl(header[2]))
   {
      RARCH_ERR("Cartridge SRAM sizes do not correspond.\n");
      return false;
   }

   if (!get_nickname(handle, handle->fd))
   {
      RARCH_ERR("Failed to get nickname from client.\n");
      return false;
   }

   // Send SRAM data to our Player 2.
   const void *sram = pretro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   unsigned sram_size = pretro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
   if (!send_all(handle->fd, sram, sram_size))
   {
      RARCH_ERR("Failed to send SRAM data to client.\n");
      return false;
   }

   if (!send_nickname(handle, handle->fd))
   {
      RARCH_ERR("Failed to send nickname to client.\n");
      return false;
   }

#ifndef HAVE_SOCKET_LEGACY
   log_connection(&handle->other_addr, 0, handle->other_nick);
#endif

   return true;
}

static uint32_t *bsv_header_generate(size_t *size, uint32_t magic)
{
   uint32_t bsv_header[4] = {0};
   size_t serialize_size = pretro_serialize_size();
   size_t header_size = sizeof(bsv_header) + serialize_size;
   *size = header_size;

   uint32_t *header = (uint32_t*)malloc(header_size);
   if (!header)
      return NULL;

   bsv_header[MAGIC_INDEX] = swap_if_little32(BSV_MAGIC);
   bsv_header[SERIALIZER_INDEX] = swap_if_big32(magic);
   bsv_header[CRC_INDEX] = swap_if_big32(g_extern.cart_crc);
   bsv_header[STATE_SIZE_INDEX] = swap_if_big32(serialize_size);

   if (serialize_size && !pretro_serialize(header + 4, serialize_size))
   {
      free(header);
      return NULL;
   }

   memcpy(header, bsv_header, sizeof(bsv_header));
   return header;
}

static bool bsv_parse_header(const uint32_t *header, uint32_t magic)
{
   uint32_t in_bsv = swap_if_little32(header[MAGIC_INDEX]);
   if (in_bsv != BSV_MAGIC)
   {
      RARCH_ERR("BSV magic mismatch, got 0x%x, expected 0x%x.\n",
            in_bsv, BSV_MAGIC);
      return false;
   }

   uint32_t in_magic = swap_if_big32(header[SERIALIZER_INDEX]);
   if (in_magic != magic)
   {
      RARCH_ERR("Magic mismatch, got 0x%x, expected 0x%x.\n", in_magic, magic);
      return false;
   }

   uint32_t in_crc = swap_if_big32(header[CRC_INDEX]);
   if (in_crc != g_extern.cart_crc)
   {
      RARCH_ERR("CRC32 mismatch, got 0x%x, expected 0x%x.\n", in_crc, g_extern.cart_crc);
      return false;
   }

   uint32_t in_state_size = swap_if_big32(header[STATE_SIZE_INDEX]);
   if (in_state_size != pretro_serialize_size())
   {
      RARCH_ERR("Serialization size mismatch, got 0x%x, expected 0x%x.\n",
            (unsigned)in_state_size, (unsigned)pretro_serialize_size());
      return false;
   }

   return true;
}

static bool get_info_spectate(netplay_t *handle)
{
   if (!send_nickname(handle, handle->fd))
   {
      RARCH_ERR("Failed to send nickname to host.\n");
      return false;
   }

   if (!get_nickname(handle, handle->fd))
   {
      RARCH_ERR("Failed to receive nickname from host.\n");
      return false;
   }

   char msg[512];
   snprintf(msg, sizeof(msg), "Connected to \"%s\"", handle->other_nick);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);

   uint32_t header[4];

   if (!recv_all(handle->fd, header, sizeof(header)))
   {
      RARCH_ERR("Cannot get header from host.\n");
      return false;
   }

   size_t save_state_size = pretro_serialize_size();
   if (!bsv_parse_header(header, implementation_magic_value()))
   {
      RARCH_ERR("Received invalid BSV header from host.\n");
      return false;
   }

   void *buf = malloc(save_state_size);
   if (!buf)
      return false;

   size_t size = save_state_size;

   if (!recv_all(handle->fd, buf, size))
   {
      RARCH_ERR("Failed to receive save state from host.\n");
      free(buf);
      return false;
   }

   bool ret = true;
   if (save_state_size)
      ret = pretro_unserialize(buf, save_state_size);

   free(buf);
   return ret;
}

static void init_buffers(netplay_t *handle)
{
   unsigned i;
   handle->buffer = (struct delta_frame*)calloc(handle->buffer_size, sizeof(*handle->buffer));
   handle->state_size = pretro_serialize_size();
   for (i = 0; i < handle->buffer_size; i++)
   {
      handle->buffer[i].state = malloc(handle->state_size);
      handle->buffer[i].is_simulated = true;
   }
}

netplay_t *netplay_new(const char *server, uint16_t port,
      unsigned frames, const struct retro_callbacks *cb,
      bool spectate,
      const char *nick)
{
   unsigned i;
   if (frames > UDP_FRAME_PACKETS)
      frames = UDP_FRAME_PACKETS;

   netplay_t *handle = (netplay_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->fd = -1;
   handle->udp_fd = -1;
   handle->cbs = *cb;
   handle->port = server ? 0 : 1;
   handle->spectate = spectate;
   handle->spectate_client = server != NULL;
   strlcpy(handle->nick, nick, sizeof(handle->nick));

   if (!init_socket(handle, server, port))
   {
      free(handle);
      return NULL;
   }

   if (spectate)
   {
      if (server)
      {
         if (!get_info_spectate(handle))
            goto error;
      }

      for (i = 0; i < MAX_SPECTATORS; i++)
         handle->spectate_fds[i] = -1;
   }
   else
   {
      if (server)
      {
         if (!send_info(handle))
            goto error;
      }
      else
      {
         if (!get_info(handle))
            goto error;
      }

      handle->buffer_size = frames + 1;

      init_buffers(handle);
      handle->has_connection = true;
   }

   return handle;

error:
   if (handle->fd >= 0)
      close(handle->fd);
   if (handle->udp_fd >= 0)
      close(handle->udp_fd);

   free(handle);
   return NULL;
}


static bool netplay_is_alive(netplay_t *handle)
{
   return handle->has_connection;
}

static bool send_chunk(netplay_t *handle)
{
   const struct sockaddr *addr = NULL;
   if (handle->addr)
      addr = handle->addr->ai_addr;
   else if (handle->has_client_addr)
      addr = (const struct sockaddr*)&handle->their_addr;

   if (addr)
   {
      if (sendto(handle->udp_fd, CONST_CAST handle->packet_buffer,
               sizeof(handle->packet_buffer), 0, addr,
               sizeof(struct sockaddr)) != sizeof(handle->packet_buffer))
      {
         warn_hangup();
         handle->has_connection = false;
         return false;
      }
   }
   return true;
}

#define MAX_RETRIES 16
#define RETRY_MS 500

static int poll_input(netplay_t *handle, bool block)
{
   int max_fd = (handle->fd > handle->udp_fd ? handle->fd : handle->udp_fd) + 1;

   struct timeval tv = {0};
   tv.tv_sec = 0;
   tv.tv_usec = block ? (RETRY_MS * 1000) : 0;

   do
   { 
      handle->timeout_cnt++;

      // select() does not take pointer to const struct timeval.
      // Technically possible for select() to modify tmp_tv, so we go paranoia mode.
      struct timeval tmp_tv = tv;

      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(handle->udp_fd, &fds);
      FD_SET(handle->fd, &fds);

      if (select(max_fd, &fds, NULL, NULL, &tmp_tv) < 0)
         return -1;

      // Somewhat hacky,
      // but we aren't using the TCP connection for anything useful atm.
      if (FD_ISSET(handle->fd, &fds) && !netplay_get_cmd(handle))
         return -1; 

      if (FD_ISSET(handle->udp_fd, &fds))
         return 1;

      if (block && !send_chunk(handle))
      {
         warn_hangup();
         handle->has_connection = false;
         return -1;
      }

      if (block)
      {
         RARCH_LOG("Network is stalling, resending packet... Count %u of %d ...\n",
               handle->timeout_cnt, MAX_RETRIES);
      }
   } while ((handle->timeout_cnt < MAX_RETRIES) && block);

   if (block)
      return -1;
   return 0;
}

// Grab our own input state and send this over the network.
static bool get_self_input_state(netplay_t *handle)
{
   unsigned i;
   struct delta_frame *ptr = &handle->buffer[handle->self_ptr];

   uint32_t state = 0;
   if (handle->frame_count > 0) // First frame we always give zero input since relying on input from first frame screws up when we use -F 0.
   {
      retro_input_state_t cb = handle->cbs.state_cb;
      for (i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         int16_t tmp = cb(g_settings.input.netplay_client_swap_input ? 0 : !handle->port,
               RETRO_DEVICE_JOYPAD, 0, i);
         state |= tmp ? 1 << i : 0;
      }
   }

   memmove(handle->packet_buffer, handle->packet_buffer + 2,
         sizeof (handle->packet_buffer) - 2 * sizeof(uint32_t));
   handle->packet_buffer[(UDP_FRAME_PACKETS - 1) * 2] = htonl(handle->frame_count); 
   handle->packet_buffer[(UDP_FRAME_PACKETS - 1) * 2 + 1] = htonl(state);

   if (!send_chunk(handle))
   {
      warn_hangup();
      handle->has_connection = false;
      return false;
   }

   ptr->self_state = state;
   handle->self_ptr = NEXT_PTR(handle->self_ptr);
   return true;
}

// TODO: Somewhat better prediction. :P
static void simulate_input(netplay_t *handle)
{
   size_t ptr = PREV_PTR(handle->self_ptr);
   size_t prev = PREV_PTR(handle->read_ptr);

   handle->buffer[ptr].simulated_input_state = handle->buffer[prev].real_input_state;
   handle->buffer[ptr].is_simulated = true;
   handle->buffer[ptr].used_real = false;
}

static void parse_packet(netplay_t *handle, uint32_t *buffer, unsigned size)
{
   unsigned i;
   for (i = 0; i < size * 2; i++)
      buffer[i] = ntohl(buffer[i]);

   for (i = 0; i < size && handle->read_frame_count <= handle->frame_count; i++)
   {
      uint32_t frame = buffer[2 * i + 0];
      uint32_t state = buffer[2 * i + 1];

      if (frame == handle->read_frame_count)
      {
         handle->buffer[handle->read_ptr].is_simulated = false;
         handle->buffer[handle->read_ptr].real_input_state = state;
         handle->read_ptr = NEXT_PTR(handle->read_ptr);
         handle->read_frame_count++;
         handle->timeout_cnt = 0;
      }
   }
}

static bool receive_data(netplay_t *handle, uint32_t *buffer, size_t size)
{
   socklen_t addrlen = sizeof(handle->their_addr);
   if (recvfrom(handle->udp_fd, NONCONST_CAST buffer, size, 0, (struct sockaddr*)&handle->their_addr, &addrlen) != (ssize_t)size)
      return false;
   handle->has_client_addr = true;
   return true;
}

// Poll network to see if we have anything new. If our network buffer is full, we simply have to block for new input data.
static bool netplay_poll(netplay_t *handle)
{
   if (!handle->has_connection)
      return false;

   handle->can_poll = false;

   if (!get_self_input_state(handle))
      return false;

   // We skip reading the first frame so the host has a chance to grab our host info so we don't block forever :')
   if (handle->frame_count == 0)
   {
      handle->buffer[0].used_real = true;
      handle->buffer[0].is_simulated = false;
      handle->buffer[0].real_input_state = 0;
      handle->read_ptr = NEXT_PTR(handle->read_ptr);
      handle->read_frame_count++;
      return true;
   }

   // We might have reached the end of the buffer, where we simply have to block.
   int res = poll_input(handle, handle->other_ptr == handle->self_ptr);
   if (res == -1)
   {
      handle->has_connection = false;
      warn_hangup();
      return false;
   }

   if (res == 1)
   {
      uint32_t first_read = handle->read_frame_count;
      do 
      {
         uint32_t buffer[UDP_FRAME_PACKETS * 2];
         if (!receive_data(handle, buffer, sizeof(buffer)))
         {
            warn_hangup();
            handle->has_connection = false;
            return false;
         }
         parse_packet(handle, buffer, UDP_FRAME_PACKETS);

      } while ((handle->read_frame_count <= handle->frame_count) && 
            poll_input(handle, (handle->other_ptr == handle->self_ptr) && 
               (first_read == handle->read_frame_count)) == 1);
   }
   else
   {
      // Cannot allow this. Should not happen though.
      if (handle->self_ptr == handle->other_ptr)
      {
         warn_hangup();
         return false;
      }
   }

   if (handle->read_ptr != handle->self_ptr)
      simulate_input(handle);
   else
      handle->buffer[PREV_PTR(handle->self_ptr)].used_real = true;

   return true;
}

static bool netplay_send_cmd(netplay_t *handle, uint32_t cmd, const void *data, size_t size)
{
   cmd = (cmd << 16) | (size & 0xffff);
   cmd = htonl(cmd);

   if (!send_all(handle->fd, &cmd, sizeof(cmd)))
      return false;

   if (!send_all(handle->fd, data, size))
      return false;

   return true;
}

static bool netplay_cmd_ack(netplay_t *handle)
{
   uint32_t cmd = htonl(NETPLAY_CMD_ACK);
   return send_all(handle->fd, &cmd, sizeof(cmd));
}

static bool netplay_cmd_nak(netplay_t *handle)
{
   uint32_t cmd = htonl(NETPLAY_CMD_NAK);
   return send_all(handle->fd, &cmd, sizeof(cmd));
}

static bool netplay_get_response(netplay_t *handle)
{
   uint32_t response;
   if (!recv_all(handle->fd, &response, sizeof(response)))
      return false;

   return ntohl(response) == NETPLAY_CMD_ACK;
}

static bool netplay_get_cmd(netplay_t *handle)
{
   uint32_t cmd;
   if (!recv_all(handle->fd, &cmd, sizeof(cmd)))
      return false;

   cmd = ntohl(cmd);

   size_t cmd_size = cmd & 0xffff;
   cmd = cmd >> 16;

   switch (cmd)
   {
      case NETPLAY_CMD_FLIP_PLAYERS:
      {
         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("CMD_FLIP_PLAYERS has unexpected command size.\n");
            return netplay_cmd_nak(handle);
         }

         uint32_t flip_frame;
         if (!recv_all(handle->fd, &flip_frame, sizeof(flip_frame)))
         {
            RARCH_ERR("Failed to receive CMD_FLIP_PLAYERS argument.\n");
            return netplay_cmd_nak(handle);
         }

         flip_frame = ntohl(flip_frame);
         if (flip_frame < handle->flip_frame)
         {
            RARCH_ERR("Host asked us to flip players in the past. Not possible ...\n");
            return netplay_cmd_nak(handle);
         }

         handle->flip ^= true;
         handle->flip_frame = flip_frame;

         RARCH_LOG("Netplay players are flipped.\n");
         msg_queue_push(g_extern.msg_queue, "Netplay players are flipped.", 1, 180);

         return netplay_cmd_ack(handle);
      }

      default:
         RARCH_ERR("Unknown netplay command received.\n");
         return netplay_cmd_nak(handle);
   }
}

void netplay_flip_players(netplay_t *handle)
{
   uint32_t flip_frame = handle->frame_count + 2 * UDP_FRAME_PACKETS;
   uint32_t flip_frame_net = htonl(flip_frame);
   const char *msg = NULL;

   if (handle->spectate)
   {
      msg = "Cannot flip players in spectate mode.";
      goto error;
   }

   if (handle->port == 0)
   {
      msg = "Cannot flip players if you're not the host.";
      goto error;
   }

   // Make sure both clients are definitely synced up.
   if (handle->frame_count < (handle->flip_frame + 2 * UDP_FRAME_PACKETS))
   {
      msg = "Cannot flip players yet. Wait a second or two before attempting flip.";
      goto error;
   }

   if (netplay_send_cmd(handle, NETPLAY_CMD_FLIP_PLAYERS, &flip_frame_net, sizeof(flip_frame_net))
         && netplay_get_response(handle))
   {
      RARCH_LOG("Netplay players are flipped.\n");
      msg_queue_push(g_extern.msg_queue, "Netplay players are flipped.", 1, 180);

      // Queue up a flip well enough in the future.
      handle->flip ^= true;
      handle->flip_frame = flip_frame;
   }
   else
   {
      msg = "Failed to flip players.";
      goto error;
   }

   return;

error:
   RARCH_WARN("%s\n", msg);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
}

static bool netplay_flip_port(netplay_t *handle, bool port)
{
   if (handle->flip_frame == 0)
      return port;

   size_t frame = handle->is_replay ? handle->tmp_frame_count : handle->frame_count;

   return port ^ handle->flip ^ (frame < handle->flip_frame);
}

int16_t netplay_input_state(netplay_t *handle, bool port, unsigned device, unsigned index, unsigned id)
{
   uint16_t input_state = 0;
   size_t ptr = handle->is_replay ? handle->tmp_ptr : PREV_PTR(handle->self_ptr);

   port = netplay_flip_port(handle, port);

   if ((port ? 1 : 0) == handle->port)
   {
      if (handle->buffer[ptr].is_simulated)
         input_state = handle->buffer[ptr].simulated_input_state;
      else
         input_state = handle->buffer[ptr].real_input_state;
   }
   else
      input_state = handle->buffer[ptr].self_state;

   return ((1 << id) & input_state) ? 1 : 0;
}

void netplay_free(netplay_t *handle)
{
   unsigned i;
   close(handle->fd);

   if (handle->spectate)
   {
      for (i = 0; i < MAX_SPECTATORS; i++)
         if (handle->spectate_fds[i] >= 0)
            close(handle->spectate_fds[i]);

      free(handle->spectate_input);
   }
   else
   {
      close(handle->udp_fd);

      for (i = 0; i < handle->buffer_size; i++)
         free(handle->buffer[i].state);

      free(handle->buffer);
   }

   if (handle->addr)
      freeaddrinfo(handle->addr);

   free(handle);
}

static bool netplay_should_skip(netplay_t *handle)
{
   return handle->is_replay && handle->has_connection;
}

static void netplay_pre_frame_net(netplay_t *handle)
{
   pretro_serialize(handle->buffer[handle->self_ptr].state, handle->state_size);
   handle->can_poll = true;

   input_poll_net();
}

static void netplay_set_spectate_input(netplay_t *handle, int16_t input)
{
   if (handle->spectate_input_ptr >= handle->spectate_input_size)
   {
      handle->spectate_input_size++;
      handle->spectate_input_size *= 2;
      handle->spectate_input = (uint16_t*)realloc(handle->spectate_input,
            handle->spectate_input_size * sizeof(uint16_t));
   }

   handle->spectate_input[handle->spectate_input_ptr++] = swap_if_big16(input);
}

int16_t input_state_spectate(unsigned port, unsigned device, unsigned index, unsigned id)
{
   int16_t res = g_extern.netplay->cbs.state_cb(port, device, index, id);
   netplay_set_spectate_input(g_extern.netplay, res);
   return res;
}

static int16_t netplay_get_spectate_input(netplay_t *handle, bool port, unsigned device, unsigned index, unsigned id)
{
   int16_t inp;
   if (recv_all(handle->fd, NONCONST_CAST &inp, sizeof(inp)))
      return swap_if_big16(inp);
   else
   {
      RARCH_ERR("Connection with host was cut.\n");
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, "Connection with host was cut.", 1, 180);

      pretro_set_input_state(g_extern.netplay->cbs.state_cb);
      return g_extern.netplay->cbs.state_cb(port, device, index, id);
   }
}

int16_t input_state_spectate_client(unsigned port, unsigned device, unsigned index, unsigned id)
{
   return netplay_get_spectate_input(g_extern.netplay, port, device, index, id);
}

static void netplay_pre_frame_spectate(netplay_t *handle)
{
   unsigned i;
   if (handle->spectate_client)
      return;

   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(handle->fd, &fds);

   struct timeval tmp_tv = {0};
   if (select(handle->fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   if (!FD_ISSET(handle->fd, &fds))
      return;

   struct sockaddr_storage their_addr;
   socklen_t addr_size = sizeof(their_addr);
   int new_fd = accept(handle->fd, (struct sockaddr*)&their_addr, &addr_size);
   if (new_fd < 0)
   {
      RARCH_ERR("Failed to accept incoming spectator.\n");
      return;
   }

   int index = -1;
   for (i = 0; i < MAX_SPECTATORS; i++)
   {
      if (handle->spectate_fds[i] == -1)
      {
         index = i;
         break;
      }
   }

   // No vacant client streams :(
   if (index == -1)
   {
      close(new_fd);
      return;
   }

   if (!get_nickname(handle, new_fd))
   {
      RARCH_ERR("Failed to get nickname from client.\n");
      close(new_fd);
      return;
   }

   if (!send_nickname(handle, new_fd))
   {
      RARCH_ERR("Failed to send nickname to client.\n");
      close(new_fd);
      return;
   }

   size_t header_size;
   uint32_t *header = bsv_header_generate(&header_size, implementation_magic_value());
   if (!header)
   {
      RARCH_ERR("Failed to generate BSV header.\n");
      close(new_fd);
      return;
   }

   int bufsize = header_size;
   setsockopt(new_fd, SOL_SOCKET, SO_SNDBUF, CONST_CAST &bufsize, sizeof(int));

   if (!send_all(new_fd, header, header_size))
   {
      RARCH_ERR("Failed to send header to client.\n");
      close(new_fd);
      free(header);
      return;
   }

   free(header);
   handle->spectate_fds[index] = new_fd;

#ifndef HAVE_SOCKET_LEGACY
   log_connection(&their_addr, index, handle->other_nick);
#endif
}

void netplay_pre_frame(netplay_t *handle)
{
   if (handle->spectate)
      netplay_pre_frame_spectate(handle);
   else
      netplay_pre_frame_net(handle);
}

static void netplay_post_frame_net(netplay_t *handle)
{
   handle->frame_count++;

   // Nothing to do...
   if (handle->other_frame_count == handle->read_frame_count)
      return;

   // Skip ahead if we predicted correctly. Skip until our simulation failed.
   while (handle->other_frame_count < handle->read_frame_count)
   {
      const struct delta_frame *ptr = &handle->buffer[handle->other_ptr];
      if ((ptr->simulated_input_state != ptr->real_input_state) && !ptr->used_real)
         break;
      handle->other_ptr = NEXT_PTR(handle->other_ptr);
      handle->other_frame_count++;
   }

   if (handle->other_frame_count < handle->read_frame_count)
   {
      // Replay frames
      handle->is_replay = true;
      handle->tmp_ptr = handle->other_ptr;
      handle->tmp_frame_count = handle->other_frame_count;

      pretro_unserialize(handle->buffer[handle->other_ptr].state, handle->state_size);
      bool first = true;
      while (first || (handle->tmp_ptr != handle->self_ptr))
      {
         pretro_serialize(handle->buffer[handle->tmp_ptr].state, handle->state_size);
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         lock_autosave();
#endif
         pretro_run();
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         unlock_autosave();
#endif
         handle->tmp_ptr = NEXT_PTR(handle->tmp_ptr);
         handle->tmp_frame_count++;
         first = false;
      }

      handle->other_ptr = handle->read_ptr;
      handle->other_frame_count = handle->read_frame_count;
      handle->is_replay = false;
   }
}

static void netplay_post_frame_spectate(netplay_t *handle)
{
   unsigned i;
   if (handle->spectate_client)
      return;

   for (i = 0; i < MAX_SPECTATORS; i++)
   {
      if (handle->spectate_fds[i] == -1)
         continue;

      if (!send_all(handle->spectate_fds[i],
               handle->spectate_input, handle->spectate_input_ptr * sizeof(int16_t)))
      {
         RARCH_LOG("Client (#%u) disconnected ...\n", i);

         char msg[512];
         snprintf(msg, sizeof(msg), "Client (#%u) disconnected.", i);
         msg_queue_push(g_extern.msg_queue, msg, 1, 180);

         close(handle->spectate_fds[i]);
         handle->spectate_fds[i] = -1;
         break;
      }
   }

   handle->spectate_input_ptr = 0;
}

// Here we check if we have new input and replay from recorded input.
void netplay_post_frame(netplay_t *handle)
{
   if (handle->spectate)
      netplay_post_frame_spectate(handle);
   else
      netplay_post_frame_net(handle);
}

#ifdef HAVE_SOCKET_LEGACY

#undef getaddrinfo
#undef freeaddrinfo
#undef sockaddr_storage
#undef addrinfo

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define addrinfo addrinfo_rarch__

// Yes, we love shitty implementations, don't we? :(
#ifdef _XBOX
struct hostent
{
   char **h_addr_list; // Just do the minimal needed ...
};

static struct hostent *gethostbyname(const char *name)
{
   static struct hostent he;
   static struct in_addr addr;
   static char *addr_ptr;

   he.h_addr_list = &addr_ptr;
   addr_ptr = (char*)&addr;

   if (!name)
      return NULL;

   XNDNS *dns = NULL;
   WSAEVENT event = WSACreateEvent();
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

int getaddrinfo_rarch__(const char *node, const char *service,
      const struct addrinfo *hints,
      struct addrinfo **res)
{
   struct addrinfo *info = (struct addrinfo*)calloc(1, sizeof(*info));
   if (!info)
      return -1;

   info->ai_family = AF_INET;
   info->ai_socktype = hints->ai_socktype;

   struct sockaddr_in *in_addr = (struct sockaddr_in*)calloc(1, sizeof(*in_addr));
   if (!in_addr)
   {
      free(info);
      return -1;
   }

   info->ai_addrlen = sizeof(*in_addr);

   in_addr->sin_family = AF_INET;
   in_addr->sin_port = htons(strtoul(service, NULL, 0));

   if (!node && (hints->ai_flags & AI_PASSIVE))
      in_addr->sin_addr.s_addr = INADDR_ANY;
   else if (node && isdigit(*node))
      in_addr->sin_addr.s_addr = inet_addr(node);
   else if (node && !isdigit(*node))
   {
      struct hostent *host = gethostbyname(node);
      if (!host || !host->h_addr_list[0])
         goto error;

      in_addr->sin_addr.s_addr = inet_addr(host->h_addr_list[0]);
   }
   else
      goto error;

   info->ai_addr = (struct sockaddr*)in_addr;
   *res = info;

   return 0;

error:
   free(in_addr);
   free(info);
   return -1;
}

void freeaddrinfo_rarch__(struct addrinfo *res)
{
   free(res->ai_addr);
   free(res);
}

#endif

