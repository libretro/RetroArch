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

struct netplay
{
   struct snes_callbacks cbs;
   int fd;
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
   bool can_poll;
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

static bool init_socket(netplay_t *handle, const char *server, uint16_t port)
{
#ifdef _WIN32
   WSADATA wsaData;
   if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
   {
      WSACleanup();
      return false;
   }
#endif

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
         if (res)
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
         if (res)
            freeaddrinfo(res);
         return false;
      }
      int new_fd = accept(handle->fd, NULL, NULL);
      if (new_fd < 0)
      {
         SSNES_ERR("Failed to accept socket.\n");
         close(handle->fd);
         if (res)
            freeaddrinfo(res);
         return false;
      }
      close(handle->fd);
      handle->fd = new_fd;
   }

   if (res)
      freeaddrinfo(res);

   // No nagle for you!
   const int nodelay = 1;
   setsockopt(handle->fd, SOL_SOCKET, TCP_NODELAY, CONST_CAST &nodelay, sizeof(int));

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
      handle->buffer[i].state = malloc(handle->state_size);
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
   return handle;
}


bool netplay_is_alive(netplay_t *handle)
{
   return handle->has_connection;
}


static bool poll_input(netplay_t *handle, bool block)
{
   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(handle->fd, &fds);

   struct timeval tv = {
      .tv_sec = 0,
      .tv_usec = 0
   };

   if (select(handle->fd + 1, &fds, NULL, NULL, block ? NULL : &tv) < 0)
      return false;

   if (FD_ISSET(handle->fd, &fds))
      return true;
   return false;
}

// Grab our own input state and send this over the network.
static bool get_self_input_state(netplay_t *handle)
{
   struct delta_frame *ptr = &handle->buffer[handle->self_ptr];

   uint16_t state = 0;
   snes_input_state_t cb = handle->cbs.state_cb;
   for (int i = 0; i <= 11; i++)
   {
      int16_t tmp = cb(!handle->port, SNES_DEVICE_JOYPAD, 0, i);
      state |= tmp ? 1 << i : 0;
   }

   uint16_t send_state = htons(state);
   if (send(handle->fd, CONST_CAST &send_state, sizeof(send_state), 0) != sizeof(send_state))
   {
      SSNES_WARN("Netplay connection hung up. Will continue without netplay.\n");
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
}

// Poll network to see if we have anything new. If our network buffer is full, we simply have to block for new input data.
bool netplay_poll(netplay_t *handle)
{
   if (!handle->has_connection)
      return false;

   handle->can_poll = false;

   if (!get_self_input_state(handle))
      return false;

   // We might have reached the end of the buffer, where we simply have to block.
   if (poll_input(handle, handle->other_ptr == NEXT_PTR(handle->self_ptr)))
   {
      do 
      {
         struct delta_frame *ptr = &handle->buffer[handle->read_ptr];
         if (recv(handle->fd, NONCONST_CAST &ptr->real_input_state, sizeof(ptr->real_input_state), 0) != sizeof(ptr->real_input_state))
         {
            SSNES_WARN("Netplay connection hung up. Will continue without netplay.\n");
            handle->has_connection = false;
            return false;
         }
         ptr->real_input_state = ntohs(ptr->real_input_state);
         ptr->is_simulated = false;

         handle->read_ptr = NEXT_PTR(handle->read_ptr);

      } while ((handle->read_ptr != handle->self_ptr) && poll_input(handle, false));
   }
   else
   {
      // Cannot allow this. Should not happen though.
      if (handle->self_ptr == handle->read_ptr)
      {
         SSNES_WARN("Netplay connection hung up. Will continue without netplay.\n");
         return false;
      }

   }

   if (handle->read_ptr != handle->self_ptr)
   {
      simulate_input(handle);
      handle->buffer[PREV_PTR(handle->self_ptr)].used_real = false;
   }
   else
   {
      handle->buffer[PREV_PTR(handle->self_ptr)].is_simulated = false;
      handle->buffer[PREV_PTR(handle->self_ptr)].used_real = true;
   }

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
   
   for (unsigned i = 0; i < handle->buffer_size; i++)
      free(handle->buffer[i].state);

   free(handle->buffer);
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


