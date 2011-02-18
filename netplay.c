/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

#include "netplay.h"
#include "general.h"
#include "dynamic.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// Woohoo, Winsock has headers from the STONE AGE! :D
#define close(x) closesocket(x)
#define CONST_CAST (const char*)
#define NONCONST_CAST (char*)
#else
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#define CONST_CAST
#define NONCONST_CAST
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

#define UDP_FRAME_PACKETS 32

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

   struct timeval last_tv;
   uint32_t packet_buffer[UDP_FRAME_PACKETS * 2]; // To compat UDP packet loss we also send old data along with the packets.
   uint32_t frame_count;
   uint32_t read_frame_count;
   struct addrinfo *addr;
   struct sockaddr_storage their_addr;
   bool has_client_addr;
};

void input_poll_net(void)
{
   if (!netplay_should_skip(g_extern.netplay) && netplay_can_poll(g_extern.netplay))
   {
      netplay_callbacks(g_extern.netplay)->poll_cb();
      netplay_poll(g_extern.netplay);
   }
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

static bool init_tcp_socket(netplay_t *handle, const char *server, uint16_t port)
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
   else
   {
      int yes = 1;
      setsockopt(handle->fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));

      if (bind(handle->fd, res->ai_addr, res->ai_addrlen) < 0 || listen(handle->fd, 1) < 0)
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

   // Just get some initial value.
   gettimeofday(&handle->last_tv, NULL);

   return true;
}

static bool init_socket(netplay_t *handle, const char *server, uint16_t port)
{
#ifdef _WIN32
   WSADATA wsaData;
   if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
   {
      WSACleanup();
      return false;
   }
#else
   signal(SIGPIPE, SIG_IGN); // Do not like SIGPIPE killing our app :(
#endif

   if (!init_tcp_socket(handle, server, port))
      return false;
   if (!init_udp_socket(handle, server, port))
      return false;
   return true;
}

bool netplay_can_poll(netplay_t *handle)
{
   return handle->can_poll;
}

static bool send_info(netplay_t *handle)
{
   uint32_t header[3] = { htonl(g_extern.cart_crc), htonl(psnes_serialize_size()), htonl(psnes_get_memory_size(SNES_MEMORY_CARTRIDGE_RAM)) };
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
   if (psnes_serialize_size() != ntohl(header[1]))
   {
      SSNES_ERR("Serialization sizes differ, make sure you're using exact same libsnes implementations!\n");
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
   while (sram_size > 0)
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

static void init_buffers(netplay_t *handle)
{
   handle->buffer = calloc(handle->buffer_size, sizeof(*handle->buffer));
   handle->state_size = psnes_serialize_size();
   for (unsigned i = 0; i < handle->buffer_size; i++)
   {
      handle->buffer[i].state = malloc(handle->state_size);
      handle->buffer[i].is_simulated = true;
   }
}

netplay_t *netplay_new(const char *server, uint16_t port, unsigned frames, const struct snes_callbacks *cb)
{
   netplay_t *handle = calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->cbs = *cb;
   handle->port = server ? 0 : 1;

   if (!init_socket(handle, server, port))
   {
      free(handle);
      return NULL;
   }

   if (server)
   {
      if (!send_info(handle))
      {
         close(handle->fd);
         free(handle);
         return NULL;
      }
   }
   else
   {
      if (!get_info(handle))
      {
         close(handle->fd);
         free(handle);
         return NULL;
      }
   }

   handle->buffer_size = frames + 1;

   init_buffers(handle);
   handle->has_connection = true;

   memset(handle->packet_buffer, 0xFF, sizeof(handle->packet_buffer));
   return handle;
}


bool netplay_is_alive(netplay_t *handle)
{
   return handle->has_connection;
}


static int poll_input(netplay_t *handle, bool block)
{
   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(handle->udp_fd, &fds);

   struct timeval tv = {
      .tv_sec = block ? 5 : 0,
      .tv_usec = 0
   };

   if (select(handle->udp_fd + 1, &fds, NULL, NULL, &tv) < 0)
      return -1;

   if (block && !FD_ISSET(handle->udp_fd, &fds))
      return -1;

   if (FD_ISSET(handle->udp_fd, &fds))
      return 1;

   return 0;
}

// Grab our own input state and send this over the network.
static bool get_self_input_state(netplay_t *handle)
{
   struct delta_frame *ptr = &handle->buffer[handle->self_ptr];

   uint32_t state = 0;
   snes_input_state_t cb = handle->cbs.state_cb;
   for (int i = 0; i <= 11; i++)
   {
      int16_t tmp = cb(!handle->port, SNES_DEVICE_JOYPAD, 0, i);
      state |= tmp ? 1 << i : 0;
   }

   memmove(handle->packet_buffer, handle->packet_buffer + 2, sizeof (handle->packet_buffer) - 2 * sizeof(uint32_t));
   handle->packet_buffer[(UDP_FRAME_PACKETS - 1) * 2] = htonl(handle->frame_count); 
   handle->packet_buffer[(UDP_FRAME_PACKETS - 1) * 2 + 1] = htonl(state);

   const struct sockaddr *addr = NULL;
   if (handle->addr)
      addr = handle->addr->ai_addr;
   else if (handle->has_client_addr)
      addr = (const struct sockaddr*)&handle->their_addr;

   if (addr)
   {
      fprintf(stderr, "Sending a packet! :D\n");
      if (sendto(handle->udp_fd, CONST_CAST handle->packet_buffer, sizeof(handle->packet_buffer), 0, addr, sizeof(struct sockaddr)) != sizeof(handle->packet_buffer))
      {
         SSNES_WARN("Netplay connection hung up. Will continue without netplay.\n");
         handle->has_connection = false;
         return false;
      }
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

   for (unsigned i = 0; i < size; i++)
   {
      uint32_t frame = buffer[2 * i];
      uint32_t state = buffer[2 * i + 1];

      fprintf(stderr, "Got frame %u, state 0x%x\n", frame, state);

      if (frame <= handle->frame_count && frame >= handle->read_frame_count)
      {
         size_t ptr = (handle->read_ptr + frame - handle->read_frame_count) % handle->buffer_size;
         handle->buffer[ptr].is_simulated = false;
         handle->buffer[ptr].real_input_state = state;
      }
   }

   while (!handle->buffer[handle->read_ptr].is_simulated && handle->read_ptr != handle->self_ptr)
   {
      handle->read_ptr = NEXT_PTR(handle->read_ptr);
      handle->read_frame_count++;
   }
}

static bool receive_data(netplay_t *handle, uint32_t *buffer, size_t size)
{
   socklen_t addrlen = sizeof(handle->their_addr);
   if (recvfrom(handle->udp_fd, NONCONST_CAST buffer, size, 0, (struct sockaddr*)&handle->their_addr, &addrlen) != size)
      return false;
   handle->has_client_addr = true;
   fprintf(stderr, "Received some data!\n");
   return true;
}

// Poll network to see if we have anything new. If our network buffer is full, we simply have to block for new input data.
bool netplay_poll(netplay_t *handle)
{
   if (!handle->has_connection)
      return false;

   handle->can_poll = false;

   if (!get_self_input_state(handle))
      return false;

   // We skip reading the first frame so the host has a change to grab our host info so we don't block forever :')
   if (handle->frame_count == 0)
   {
      simulate_input(handle);
      handle->buffer[PREV_PTR(handle->self_ptr)].used_real = false;
      return true;
   }

   // We might have reached the end of the buffer, where we simply have to block.
   int res = poll_input(handle, handle->other_ptr == NEXT_PTR(handle->self_ptr));
   if (res == -1)
   {
      handle->has_connection = false;
      SSNES_WARN("Netplay connection timed out. Will continue without netplay.\n");
      return false;
   }

   fprintf(stderr, "Other %lu, Read: %lu, Self: %lu, Buffer_size: %lu\n", handle->other_ptr, handle->read_ptr, handle->self_ptr, handle->buffer_size);

   if (res == 1)
   {
      size_t first_read = handle->read_ptr;
      do 
      {
         uint32_t buffer[UDP_FRAME_PACKETS * 2];
         if (!receive_data(handle, buffer, sizeof(buffer)))
         {
            SSNES_WARN("Netplay connection hung up. Will continue without netplay.\n");
            handle->has_connection = false;
            return false;
         }
         parse_packet(handle, buffer, UDP_FRAME_PACKETS);

      } while ((handle->read_ptr != handle->self_ptr) && poll_input(handle, handle->other_ptr == NEXT_PTR(handle->self_ptr) && first_read == handle->read_ptr) == 1);
   }
   else
   {
      // Cannot allow this. Should not happen though.
      if (NEXT_PTR(handle->self_ptr) == handle->other_ptr)
      {
         SSNES_WARN("Netplay connection hung up. Will continue without netplay.\n");
         return false;
      }
   }

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
   close(handle->udp_fd);
   
   for (unsigned i = 0; i < handle->buffer_size; i++)
      free(handle->buffer[i].state);

   free(handle->buffer);
   if (handle->addr)
      freeaddrinfo(handle->addr);
   free(handle);
}

const struct snes_callbacks* netplay_callbacks(netplay_t *handle)
{
   return &handle->cbs;
}

bool netplay_should_skip(netplay_t *handle)
{
   return handle->is_replay && handle->has_connection;
}

void netplay_pre_frame(netplay_t *handle)
{
   psnes_serialize(handle->buffer[handle->self_ptr].state, handle->state_size);
   handle->can_poll = true;
}

// Here we check if we have new input and replay from recorded input.
void netplay_post_frame(netplay_t *handle)
{
   handle->frame_count++;

   // Nothing to do...
   if (handle->other_ptr == handle->read_ptr)
      return;

   // Skip ahead if we predicted correctly. Skip until our simulation failed.
   while (handle->other_ptr != handle->read_ptr)
   {
      struct delta_frame *ptr = &handle->buffer[handle->other_ptr];
      if ((ptr->simulated_input_state != ptr->real_input_state) && !ptr->used_real)
         break;
      handle->other_ptr = NEXT_PTR(handle->other_ptr);
   }

   if (handle->other_ptr != handle->read_ptr)
   {
      // Replay frames
      handle->is_replay = true;
      handle->tmp_ptr = handle->other_ptr;
      psnes_unserialize(handle->buffer[handle->other_ptr].state, handle->state_size);
      while (handle->tmp_ptr != handle->self_ptr)
      {
         psnes_serialize(handle->buffer[handle->tmp_ptr].state, handle->state_size);
         psnes_run();
         handle->tmp_ptr = NEXT_PTR(handle->tmp_ptr);
      }
      handle->other_ptr = handle->read_ptr;
      handle->is_replay = false;
   }

}


