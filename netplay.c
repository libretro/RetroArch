/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#endif

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
static const struct snes_callbacks* netplay_callbacks(netplay_t *handle);
static void netplay_set_spectate_input(netplay_t *handle, int16_t input);

#ifdef _WIN32
// Woohoo, Winsock has headers from the STONE AGE! :D
#define close(x) closesocket(x)
#define CONST_CAST (const char*)
#define NONCONST_CAST (char*)
#else
#define CONST_CAST
#define NONCONST_CAST
#include <sys/time.h>
#include <unistd.h>
#endif

#define PREV_PTR(x) ((x) == 0 ? handle->buffer_size - 1 : (x) - 1)
#define NEXT_PTR(x) ((x + 1) % handle->buffer_size)

struct delta_frame
{
   uint8_t *state;

   uint16_t real_input_state;
   uint16_t simulated_input_state;
   bool is_simulated;
   uint16_t self_state;
   bool used_real;
};

#define UDP_FRAME_PACKETS 16
#define MAX_SPECTATORS 16

struct netplay
{
   struct snes_callbacks cbs;
   int fd; // TCP connection for state sending, etc. Could perhaps be used for messaging later on. :)
   int udp_fd; // UDP connection for game state updates.
   unsigned port; // Which port is governed by netplay?
   bool has_connection;

   struct delta_frame *buffer;
   size_t buffer_size;

   size_t self_ptr; // Ptr where we are now.
   size_t other_ptr; // Points to the last reliable state that self ever had.
   size_t read_ptr; // Ptr to where we are reading. Generally, other_ptr <= read_ptr <= self_ptr.
   size_t tmp_ptr; // A temporary pointer used on replay.

   size_t state_size;

   size_t is_replay; // Are we replaying old frames?
   bool can_poll; // We don't want to poll several times on a frame.

   uint32_t packet_buffer[UDP_FRAME_PACKETS * 2]; // To compat UDP packet loss we also send old data along with the packets.
   uint32_t frame_count;
   uint32_t read_frame_count;
   uint32_t other_frame_count;
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
};

static void warn_hangup(void)
{
   SSNES_WARN("Netplay has disconnected. Will continue without connection ...\n");
   if (g_extern.msg_queue)
      msg_queue_push(g_extern.msg_queue, "Netplay has disconnected. Will continue without connection.", 0, 480);
}

void input_poll_net(void)
{
   if (!netplay_should_skip(g_extern.netplay) && netplay_can_poll(g_extern.netplay))
      netplay_poll(g_extern.netplay);
}

void video_frame_net(const uint16_t *data, unsigned width, unsigned height)
{
   if (!netplay_should_skip(g_extern.netplay))
      netplay_callbacks(g_extern.netplay)->frame_cb(data, width, height);
}

void audio_sample_net(uint16_t left, uint16_t right)
{
   if (!netplay_should_skip(g_extern.netplay))
      netplay_callbacks(g_extern.netplay)->sample_cb(left, right);
}

int16_t input_state_net(bool port, unsigned device, unsigned index, unsigned id)
{
   if (netplay_is_alive(g_extern.netplay))
      return netplay_input_state(g_extern.netplay, port, device, index, id);
   else
      return netplay_callbacks(g_extern.netplay)->state_cb(port, device, index, id);
}

static bool init_tcp_socket(netplay_t *handle, const char *server, uint16_t port, bool spectate)
{
   struct addrinfo hints, *res = NULL;
   memset(&hints, 0, sizeof(hints));
#ifdef _WIN32 // Lolol, no AF_UNSPEC, wtf.
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_STREAM;
   if (!server)
      hints.ai_flags = AI_PASSIVE;

   char port_buf[16];
   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo(server, port_buf, &hints, &res) < 0)
      return false;

   if (!res)
      return false;

   handle->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   if (handle->fd < 0)
   {
      SSNES_ERR("Failed to init socket...\n");

      if (res)
         freeaddrinfo(res);
      return false;
   }

   if (server)
   {
      if (connect(handle->fd, res->ai_addr, res->ai_addrlen) < 0)
      {
         SSNES_ERR("Failed to connect to server.\n");
         close(handle->fd);
         freeaddrinfo(res);
         return false;
      }
   }
   else if (handle->spectate)
   {
      int yes = 1;
      setsockopt(handle->fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));

      if (bind(handle->fd, res->ai_addr, res->ai_addrlen) < 0 ||
            listen(handle->fd, MAX_SPECTATORS) < 0)
      {
         SSNES_ERR("Failed to bind socket.\n");
         close(handle->fd);
         freeaddrinfo(res);
         return false;
      }
   }
   else
   {
      int yes = 1;
      setsockopt(handle->fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));

      if (bind(handle->fd, res->ai_addr, res->ai_addrlen) < 0 ||
            listen(handle->fd, 1) < 0)
      {
         SSNES_ERR("Failed to bind socket.\n");
         close(handle->fd);
         freeaddrinfo(res);
         return false;
      }
      int new_fd = accept(handle->fd, NULL, NULL);
      if (new_fd < 0)
      {
         SSNES_ERR("Failed to accept socket.\n");
         close(handle->fd);
         freeaddrinfo(res);
         return false;
      }
      close(handle->fd);
      handle->fd = new_fd;
   }

   freeaddrinfo(res);

   return true;
}

static bool init_udp_socket(netplay_t *handle, const char *server, uint16_t port)
{
   struct addrinfo hints;
   memset(&hints, 0, sizeof(hints));
#ifdef _WIN32 // Lolol, no AF_UNSPEC, wtf.
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
      SSNES_ERR("Failed to init socket...\n");
      return false;
   }

   if (!server)
   {
      // Note sure if we have to do this for UDP, but hey :)
      int yes = 1;
      setsockopt(handle->udp_fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));

      if (bind(handle->udp_fd, handle->addr->ai_addr, handle->addr->ai_addrlen) < 0)
      {
         SSNES_ERR("Failed to bind socket.\n");
         close(handle->udp_fd);
      }

      freeaddrinfo(handle->addr);
      handle->addr = NULL;
   }

   return true;
}

static bool init_socket(netplay_t *handle, const char *server, uint16_t port)
{
#ifdef _WIN32
   WSADATA wsaData;
   if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
   {
      WSACleanup();
      return false;
   }
#else
   signal(SIGPIPE, SIG_IGN); // Do not like SIGPIPE killing our app :(
#endif

   if (!init_tcp_socket(handle, server, port, handle->spectate))
      return false;
   if (!handle->spectate)
   {
      if (!init_udp_socket(handle, server, port))
         return false;
   }
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
   uint32_t res = 0;
   res |= (psnes_library_revision_major() & 0xf) << 0;
   res |= (psnes_library_revision_minor() & 0xf) << 4;

   // Shouldn't really use this, but oh well :) It'll do the job.
   const char *lib = psnes_library_id();
   size_t len = strlen(lib);
   for (size_t i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   return res;
}

static bool send_info(netplay_t *handle)
{
   uint32_t header[3] = { htonl(g_extern.cart_crc), htonl(implementation_magic_value()), htonl(psnes_get_memory_size(SNES_MEMORY_CARTRIDGE_RAM)) };
   if (send(handle->fd, CONST_CAST header, sizeof(header), 0) != sizeof(header))
      return false;

   // Get SRAM data from Player 1 :)
   uint8_t *sram = psnes_get_memory_data(SNES_MEMORY_CARTRIDGE_RAM);
   unsigned sram_size = psnes_get_memory_size(SNES_MEMORY_CARTRIDGE_RAM);
   while (sram_size > 0)
   {
      ssize_t ret = recv(handle->fd, NONCONST_CAST sram, sram_size, 0);
      if (ret <= 0)
      {
         SSNES_ERR("Failed to receive SRAM data from host.\n");
         return false;
      }
      sram += ret;
      sram_size -= ret;
   }

   return true;
}

static bool get_info(netplay_t *handle)
{
   uint32_t header[3];
   if (recv(handle->fd, NONCONST_CAST header, sizeof(header), 0) != sizeof(header))
   {
      SSNES_ERR("Failed to receive header from client.\n");
      return false;
   }
   if (g_extern.cart_crc != ntohl(header[0]))
   {
      SSNES_ERR("Cart CRC32s differ! Cannot use different games!\n");
      return false;
   }
   if (implementation_magic_value() != ntohl(header[1]))
   {
      SSNES_ERR("Implementations differ, make sure you're using exact same libsnes implementations!\n");
      return false;
   }
   if (psnes_get_memory_size(SNES_MEMORY_CARTRIDGE_RAM) != ntohl(header[2]))
   {
      SSNES_ERR("Cartridge SRAM sizes do not correspond!\n");
      return false;
   }

   // Send SRAM data to our Player 2 :)
   const uint8_t *sram = psnes_get_memory_data(SNES_MEMORY_CARTRIDGE_RAM);
   unsigned sram_size = psnes_get_memory_size(SNES_MEMORY_CARTRIDGE_RAM);
   while (sram_size)
   {
      ssize_t ret = send(handle->fd, CONST_CAST sram, sram_size, 0);
      if (ret <= 0)
      {
         SSNES_ERR("Failed to send SRAM data to client.\n");
         return false;
      }
      sram += ret;
      sram_size -= ret;
   }

   return true;
}

static bool get_info_spectate(netplay_t *handle)
{
   uint32_t header[4];
   if (recv(handle->fd, NONCONST_CAST header, sizeof(header), 0) != (ssize_t)sizeof(header))
   {
      SSNES_ERR("Cannot get header from host!\n");
      return false;
   }

   unsigned save_state_size = psnes_serialize_size();
   if (!bsv_parse_header(header))
   {
      SSNES_ERR("Received invalid BSV header from host!\n");
      return false;
   }

   uint8_t *buf = (uint8_t*)malloc(save_state_size);
   if (!buf)
      return false;

   size_t size = save_state_size;
   uint8_t *tmp_buf = buf;
   while (size)
   {
      ssize_t ret = recv(handle->fd, NONCONST_CAST tmp_buf, size, 0);
      if (ret <= 0)
      {
         SSNES_ERR("Failed to receive save state from host!\n");
         free(tmp_buf);
         return false;
      }

      size -= ret;
      tmp_buf += ret;
   }

   bool ret = true;
   if (save_state_size)
      ret = psnes_unserialize(buf, save_state_size);

   free(buf);
   return ret;
}

static void init_buffers(netplay_t *handle)
{
   handle->buffer = (struct delta_frame*)calloc(handle->buffer_size, sizeof(*handle->buffer));
   handle->state_size = psnes_serialize_size();
   for (unsigned i = 0; i < handle->buffer_size; i++)
   {
      handle->buffer[i].state = (uint8_t*)malloc(handle->state_size);
      handle->buffer[i].is_simulated = true;
   }
}

netplay_t *netplay_new(const char *server, uint16_t port, unsigned frames, const struct snes_callbacks *cb, bool spectate)
{
   (void)spectate;

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

      for (unsigned i = 0; i < MAX_SPECTATORS; i++)
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
      if (sendto(handle->udp_fd, CONST_CAST handle->packet_buffer, sizeof(handle->packet_buffer), 0, addr, sizeof(struct sockaddr)) != sizeof(handle->packet_buffer))
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

      // Somewhat hacky, but we aren't using the TCP connection for anything useful atm.
      // Will probably add some proper messaging system here later.
      if (FD_ISSET(handle->fd, &fds))
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
         SSNES_LOG("Network is stalling, resending packet... Count %u of %d ...\n",
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
   struct delta_frame *ptr = &handle->buffer[handle->self_ptr];

   uint32_t state = 0;
   if (handle->frame_count > 0) // First frame we always give zero input since relying on input from first frame screws up when we use -F 0.
   {
      snes_input_state_t cb = handle->cbs.state_cb;
      for (unsigned i = 0; i <= 11; i++)
      {
         int16_t tmp = cb(g_settings.input.netplay_client_swap_input ? 0 : !handle->port, SNES_DEVICE_JOYPAD, 0, i);
         state |= tmp ? 1 << i : 0;
      }
   }

   memmove(handle->packet_buffer, handle->packet_buffer + 2, sizeof (handle->packet_buffer) - 2 * sizeof(uint32_t));
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
   for (unsigned i = 0; i < size * 2; i++)
      buffer[i] = ntohl(buffer[i]);

   for (unsigned i = 0; i < size && handle->read_frame_count <= handle->frame_count; i++)
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

   //fprintf(stderr, "After poll: Other ptr: %lu, Read ptr: %lu, Self ptr: %lu\n", handle->other_ptr, handle->read_ptr, handle->self_ptr);

   if (handle->read_ptr != handle->self_ptr)
      simulate_input(handle);
   else
      handle->buffer[PREV_PTR(handle->self_ptr)].used_real = true;

   return true;
}

int16_t netplay_input_state(netplay_t *handle, bool port, unsigned device, unsigned index, unsigned id)
{
   uint16_t input_state = 0;
   size_t ptr = 0;

   if (handle->is_replay)
      ptr = handle->tmp_ptr;
   else
      ptr = PREV_PTR(handle->self_ptr);

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
   close(handle->fd);

   if (handle->spectate)
   {
      for (unsigned i = 0; i < MAX_SPECTATORS; i++)
         if (handle->spectate_fds[i] >= 0)
            close(handle->spectate_fds[i]);

      free(handle->spectate_input);
   }
   else
   {
      close(handle->udp_fd);

      for (unsigned i = 0; i < handle->buffer_size; i++)
         free(handle->buffer[i].state);

      free(handle->buffer);
   }

   if (handle->addr)
      freeaddrinfo(handle->addr);

   free(handle);
}

static const struct snes_callbacks* netplay_callbacks(netplay_t *handle)
{
   return &handle->cbs;
}

static bool netplay_should_skip(netplay_t *handle)
{
   return handle->is_replay && handle->has_connection;
}

static void netplay_pre_frame_net(netplay_t *handle)
{
   psnes_serialize(handle->buffer[handle->self_ptr].state, handle->state_size);
   handle->can_poll = true;

   input_poll_net();
}

static inline uint16_t swap_if_big16(uint16_t input)
{
   if (is_little_endian())
      return input;
   else
      return (input << 8) | (input >> 8);
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

int16_t input_state_spectate(bool port, unsigned device, unsigned index, unsigned id)
{
   int16_t res = netplay_callbacks(g_extern.netplay)->state_cb(port, device, index, id);
   netplay_set_spectate_input(g_extern.netplay, res);
   return res;
}

static int16_t netplay_get_spectate_input(netplay_t *handle, bool port, unsigned device, unsigned index, unsigned id)
{
   int16_t inp;
   if (recv(handle->fd, NONCONST_CAST &inp, sizeof(inp), 0) == (ssize_t)sizeof(inp))
      return swap_if_big16(inp);
   else
   {
      SSNES_ERR("Connection with host was cut!\n");
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, "Connection with host was cut!", 1, 180);

      psnes_set_input_state(netplay_callbacks(g_extern.netplay)->state_cb);
      return netplay_callbacks(g_extern.netplay)->state_cb(port, device, index, id);
   }
}

int16_t input_state_spectate_client(bool port, unsigned device, unsigned index, unsigned id)
{
   return netplay_get_spectate_input(g_extern.netplay, port, device, index, id);
}

static void netplay_pre_frame_spectate(netplay_t *handle)
{
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

   int new_fd = accept(handle->fd, NULL, NULL);
   if (new_fd < 0)
   {
      SSNES_ERR("Failed to accept incoming spectator!\n");
      return;
   }

   int index = -1;
   for (unsigned i = 0; i < MAX_SPECTATORS; i++)
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

   size_t header_size;
   uint8_t *header = bsv_header_generate(&header_size);
   if (!header)
   {
      SSNES_ERR("Failed to generate BSV header!\n");
      close(new_fd);
      return;
   }

   const uint8_t *tmp_header = header;
   while (header_size)
   {
      ssize_t ret = send(new_fd, CONST_CAST tmp_header, header_size, 0);
      if (ret <= 0)
      {
         SSNES_ERR("Failed to send header to client!\n");
         close(new_fd);
         free(header);
         return;
      }

      header_size -= ret;
      tmp_header += ret; 
   }

   free(header);
   handle->spectate_fds[index] = new_fd;
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
      psnes_unserialize(handle->buffer[handle->other_ptr].state, handle->state_size);
      bool first = true;
      while (first || (handle->tmp_ptr != handle->self_ptr))
      {
         psnes_serialize(handle->buffer[handle->tmp_ptr].state, handle->state_size);
#ifdef HAVE_THREADS
         lock_autosave();
#endif
         psnes_run();
#ifdef HAVE_THREADS
         unlock_autosave();
#endif
         handle->tmp_ptr = NEXT_PTR(handle->tmp_ptr);
         first = false;
      }
      handle->other_ptr = handle->read_ptr;
      handle->other_frame_count = handle->read_frame_count;
      handle->is_replay = false;
   }
}

static void netplay_post_frame_spectate(netplay_t *handle)
{
   if (handle->spectate_client)
      return;

   for (unsigned i = 0; i < MAX_SPECTATORS; i++)
   {
      if (handle->spectate_fds[i] == -1)
         continue;

      size_t send_size = handle->spectate_input_ptr * sizeof(int16_t);
      const uint8_t *tmp_buf = (const uint8_t*)handle->spectate_input;
      while (send_size)
      {
         ssize_t ret = send(handle->spectate_fds[i], CONST_CAST tmp_buf, send_size, 0);
         if (ret <= 0)
         {
            SSNES_LOG("Client disconnected ...\n");
            close(handle->spectate_fds[i]);
            handle->spectate_fds[i] = -1;
            break;
         }

         tmp_buf += ret;
         send_size -= ret;
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

