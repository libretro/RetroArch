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
#define close(x) closesocket(x)
#define CONST_CAST (const char*)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#define CONST_CAST
#endif

struct netplay
{
   struct snes_callbacks cbs;
   int fd;
   unsigned port;
   bool has_connection;
   unsigned frames;

   uint16_t input_state;
};

void input_poll_net(void)
{
   netplay_callbacks(g_extern.netplay)->poll_cb();
   netplay_poll(g_extern.netplay);
}

void video_frame_net(const uint16_t *data, unsigned width, unsigned height)
{
   netplay_callbacks(g_extern.netplay)->frame_cb(data, width, height);
}

void audio_sample_net(uint16_t left, uint16_t right)
{
   netplay_callbacks(g_extern.netplay)->sample_cb(left, right);
}

int16_t input_state_net(bool port, unsigned device, unsigned index, unsigned id)
{
   if (netplay_is_port(g_extern.netplay, port, index))
      return netplay_input_state(g_extern.netplay, port, device, index, id);
   else
      return netplay_callbacks(g_extern.netplay)->state_cb(port, device, index, id);
}

static bool init_socket(netplay_t *handle, const char *server, uint16_t port)
{
#ifdef _WIN32
   WSADATA wsaData;
   int retval;

   if ((retval = WSAStartup(MAKEWORD(2,2), &wsaData)) != 0)
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

   handle->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   if (handle->fd < 0)
   {
      freeaddrinfo(res);
      return false;
   }

   if (server)
   {
      if (connect(handle->fd, res->ai_addr, res->ai_addrlen) < 0)
      {
         close(handle->fd);
         freeaddrinfo(res);
         return false;
      }
   }
   else
   {
      if (bind(handle->fd, res->ai_addr, res->ai_addrlen) < 0 || listen(handle->fd, 1) < 0)
      {
         close(handle->fd);
         freeaddrinfo(res);
         return false;
      }
      int new_fd = accept(handle->fd, NULL, NULL);
      if (new_fd < 0)
      {
         close(handle->fd);
         freeaddrinfo(res);
         return false;
      }
      close(handle->fd);
      handle->fd = new_fd;
   }

   freeaddrinfo(res);

   const int nodelay = 1;
   setsockopt(handle->fd, SOL_SOCKET, TCP_NODELAY, &nodelay, sizeof(int));

   return true;
}

static bool send_info(netplay_t *handle)
{
   uint32_t header[2] = { htonl(g_extern.cart_crc), htonl(psnes_serialize_size()) };
   if (send(handle->fd, header, sizeof(header), 0) != sizeof(header))
      return false;
   return true;
}

static bool get_info(netplay_t *handle)
{
   uint32_t header[2];
   if (recv(handle->fd, header, sizeof(header), 0) != sizeof(header))
      return false;
   if (g_extern.cart_crc != ntohl(header[0]))
      return false;
   if (psnes_serialize_size() != ntohl(header[1]))
      return false;
   return true;
}

netplay_t *netplay_new(const char *server, uint16_t port, unsigned frames, const struct snes_callbacks *cb)
{
   netplay_t *handle = calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->cbs = *cb;
   handle->port = server ? 0 : 1;
   handle->frames = frames;

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

   handle->has_connection = true;
   return handle;
}

bool netplay_is_port(netplay_t *handle, bool port, unsigned index)
{
   if (!handle->has_connection)
      return false;
   unsigned port_num = port ? 1 : 0;
   if (handle->port == port_num)
      return true;
   else
      return false;
}

bool netplay_poll(netplay_t *handle)
{
   uint16_t state = 0;
   snes_input_state_t cb = handle->cbs.state_cb;
   for (int i = 0; i <= 11; i++)
   {
      int16_t tmp = cb(!handle->port, SNES_DEVICE_JOYPAD, 0, i);
      state |= tmp ? 1 << i : 0;
   }

   state = htons(state);
   if (send(handle->fd, &state, sizeof(state), 0) != sizeof(state))
   {
      handle->has_connection = false;
      return false;
   }

   if (recv(handle->fd, &handle->input_state, sizeof(handle->input_state), 0) != sizeof(handle->input_state))
   {
      handle->has_connection = false;
      return false;
   }

   handle->input_state = ntohs(handle->input_state);
   return true;
}

int16_t netplay_input_state(netplay_t *handle, bool port, unsigned device, unsigned index, unsigned id)
{
   return ((1 << id) & handle->input_state) ? 1 : 0;
}

void netplay_free(netplay_t *handle)
{
   close(handle->fd);
   free(handle);
}

const struct snes_callbacks* netplay_callbacks(netplay_t *handle)
{
   return &handle->cbs;
}
