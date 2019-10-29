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

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "ws2_32")
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <boolean.h>
#include <compat/strl.h>

#include "netplay_private.h"

#include "netplay_discovery.h"

#include "../../autosave.h"
#include "../../retroarch.h"

#if defined(AF_INET6) && !defined(HAVE_SOCKET_LEGACY)
#define HAVE_INET6 1
#endif

static int init_tcp_connection(const struct addrinfo *res,
      bool server,
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
      if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
         (const char*)
#else
         (const void*)
#endif
         &flag,
         sizeof(int)) < 0)
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
#if defined(HAVE_INET6) && defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
      /* Make sure we accept connections on both IPv6 and IPv4 */
      int on = 0;
      if (res->ai_family == AF_INET6)
      {
         if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on)) < 0)
            RARCH_WARN("Failed to listen on both IPv6 and IPv4\n");
      }
#endif
      if (  !socket_bind(fd, (void*)res) ||
            listen(fd, 1024) < 0)
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

static bool init_tcp_socket(netplay_t *netplay, void *direct_host,
      const char *server, uint16_t port)
{
   char port_buf[16];
   bool ret                        = false;
   const struct addrinfo *tmp_info = NULL;
   struct addrinfo *res            = NULL;
   struct addrinfo hints           = {0};

   port_buf[0] = '\0';

   if (!direct_host)
   {
#ifdef HAVE_INET6
      /* Default to hosting on IPv6 and IPv4 */
      if (!server)
         hints.ai_family = AF_INET6;
#endif
      hints.ai_socktype = SOCK_STREAM;
      if (!server)
         hints.ai_flags = AI_PASSIVE;

      snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
      if (getaddrinfo_retro(server, port_buf, &hints, &res) != 0)
      {
#ifdef HAVE_INET6
         if (!server)
         {
            /* Didn't work with IPv6, try wildcard */
            hints.ai_family = 0;
            if (getaddrinfo_retro(server, port_buf, &hints, &res) != 0)
               return false;
         }
         else
#endif
         return false;
      }

      if (!res)
         return false;

   }
   else
   {
      /* I'll build my own addrinfo! */
      struct netplay_host *host = (struct netplay_host *)direct_host;
      hints.ai_family           = host->addr.sa_family;
      hints.ai_socktype         = SOCK_STREAM;
      hints.ai_protocol         = 0;
      hints.ai_addrlen          = host->addrlen;
      hints.ai_addr             = &host->addr;
      res                       = &hints;

   }

   /* If we're serving on IPv6, make sure we accept all connections, including
    * IPv4 */
#ifdef HAVE_INET6
   if (!direct_host && !server && res->ai_family == AF_INET6)
   {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) res->ai_addr;
#if defined(_MSC_VER) && _MSC_VER <= 1200
	  IN6ADDR_SETANY(sin6);
#else
      sin6->sin6_addr           = in6addr_any;
#endif
   }
#endif

   /* If "localhost" is used, it is important to check every possible
    * address for IPv4/IPv6. */
   tmp_info = res;

   while (tmp_info)
   {
      struct sockaddr_storage sad = {0};
      int fd = init_tcp_connection(
            tmp_info,
            direct_host || server,
            (struct sockaddr*)&sad,
            sizeof(sad));

      if (fd >= 0)
      {
         ret = true;
         if (direct_host || server)
         {
            netplay->connections[0].active = true;
            netplay->connections[0].fd     = fd;
            netplay->connections[0].addr   = sad;
         }
         else
         {
            netplay->listen_fd = fd;
         }
         break;
      }

      tmp_info = tmp_info->ai_next;
   }

   if (res && !direct_host)
      freeaddrinfo_retro(res);

   if (!ret)
      RARCH_ERR("Failed to set up netplay sockets.\n");

   return ret;
}

static bool init_socket(netplay_t *netplay, void *direct_host,
      const char *server, uint16_t port)
{
   if (!network_init())
      return false;

   if (!init_tcp_socket(netplay, direct_host, server, port))
      return false;

   if (netplay->is_server && netplay->nat_traversal)
      netplay_init_nat_traversal(netplay);

   return true;
}

static bool netplay_init_socket_buffers(netplay_t *netplay)
{
   /* Make our packet buffer big enough for a save state and stall-frames-many
    * frames of input data, plus the headers for each of them */
   size_t i;
   size_t packet_buffer_size = netplay->zbuffer_size +
      NETPLAY_MAX_STALL_FRAMES * 16;
   netplay->packet_buffer_size = packet_buffer_size;

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active)
      {
         if (connection->send_packet_buffer.data)
         {
            if (!netplay_resize_socket_buffer(&connection->send_packet_buffer,
                  packet_buffer_size) ||
                !netplay_resize_socket_buffer(&connection->recv_packet_buffer,
                  packet_buffer_size))
               return false;
         }
         else
         {
            if (!netplay_init_socket_buffer(&connection->send_packet_buffer,
                  packet_buffer_size) ||
                !netplay_init_socket_buffer(&connection->recv_packet_buffer,
                  packet_buffer_size))
               return false;
         }
      }
   }

   return true;
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

/**
 * netplay_try_init_serialization
 *
 * Try to initialize serialization. For quirky cores.
 *
 * Returns true if serialization is now ready, false otherwise.
 */
bool netplay_try_init_serialization(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   if (netplay->state_size)
      return true;

   if (!netplay_init_serialization(netplay))
      return false;

   /* Check if we can actually save */
   serial_info.data_const = NULL;
   serial_info.data       = netplay->buffer[netplay->run_ptr].state;
   serial_info.size       = netplay->state_size;

   if (!core_serialize(&serial_info))
      return false;

   /* Once initialized, we no longer exhibit this quirk */
   netplay->quirks &= ~((uint64_t) NETPLAY_QUIRK_INITIALIZATION);

   return netplay_init_socket_buffers(netplay);
}

/**
 * netplay_wait_and_init_serialization
 *
 * Try very hard to initialize serialization, simulating multiple frames if
 * necessary. For quirky cores.
 *
 * Returns true if serialization is now ready, false otherwise.
 */
bool netplay_wait_and_init_serialization(netplay_t *netplay)
{
   int frame;

   if (netplay->state_size)
      return true;

   /* Wait a maximum of 60 frames */
   for (frame = 0; frame < 60; frame++)
   {
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

static bool netplay_init_buffers(netplay_t *netplay)
{
   struct delta_frame *delta_frames = NULL;

   /* Enough to get ahead or behind by MAX_STALL_FRAMES frames, plus one for
    * other remote clients, plus one to send the stall message */
   netplay->buffer_size = NETPLAY_MAX_STALL_FRAMES + 2;

   /* If we're the server, we need enough to get ahead AND behind by
    * MAX_STALL_FRAMES frame */
   if (netplay->is_server)
      netplay->buffer_size *= 2;

   delta_frames = (struct delta_frame*)calloc(netplay->buffer_size,
         sizeof(*delta_frames));

   if (!delta_frames)
      return false;

   netplay->buffer = delta_frames;

   if (!(netplay->quirks & (NETPLAY_QUIRK_NO_SAVESTATES|NETPLAY_QUIRK_INITIALIZATION)))
      netplay_init_serialization(netplay);

   return netplay_init_socket_buffers(netplay);
}

/**
 * netplay_new:
 * @direct_host          : Netplay host discovered from scanning.
 * @server               : IP address of server.
 * @port                 : Port of server.
 * @stateless_mode       : Shall we use stateless mode?
 * @check_frames         : Frequency with which to check CRCs.
 * @cb                   : Libretro callbacks.
 * @nat_traversal        : If true, attempt NAT traversal.
 * @nick                 : Nickname of user.
 * @quirks               : Netplay quirks required for this session.
 *
 * Creates a new netplay handle. A NULL server means we're
 * hosting.
 *
 * Returns: new netplay data.
 */
netplay_t *netplay_new(void *direct_host, const char *server, uint16_t port,
   bool stateless_mode, int check_frames,
   const struct retro_callbacks *cb, bool nat_traversal, const char *nick,
   uint64_t quirks)
{
   netplay_t *netplay = (netplay_t*)calloc(1, sizeof(*netplay));
   if (!netplay)
      return NULL;

   netplay->listen_fd            = -1;
   netplay->tcp_port             = port;
   netplay->cbs                  = *cb;
   netplay->is_server            = (direct_host == NULL && server == NULL);
   netplay->is_connected         = false;;
   netplay->nat_traversal        = netplay->is_server ? nat_traversal : false;
   netplay->stateless_mode       = stateless_mode;
   netplay->check_frames         = check_frames;
   netplay->crc_validity_checked = false;
   netplay->crcs_valid           = true;
   netplay->quirks               = quirks;
   netplay->self_mode            = netplay->is_server ?
                                NETPLAY_CONNECTION_SPECTATING :
                                NETPLAY_CONNECTION_NONE;

   if (netplay->is_server)
   {
      netplay->connections       = NULL;
      netplay->connections_size  = 0;
   }
   else
   {
      netplay->connections       = &netplay->one_connection;
      netplay->connections_size  = 1;
      netplay->connections[0].fd = -1;
   }

   strlcpy(netplay->nick, nick[0]
         ? nick : RARCH_DEFAULT_NICK,
         sizeof(netplay->nick));

   if (!init_socket(netplay, direct_host, server, port))
   {
      free(netplay);
      return NULL;
   }

   if (!netplay_init_buffers(netplay))
   {
      free(netplay);
      return NULL;
   }

   if (netplay->is_server)
   {
      /* Clients get device info from the server */
      unsigned i;
      for (i = 0; i < MAX_INPUT_DEVICES; i++)
      {
         uint32_t dtype = input_config_get_device(i);
         netplay->config_devices[i] = dtype;
         if ((dtype&RETRO_DEVICE_MASK) == RETRO_DEVICE_KEYBOARD)
         {
            netplay->have_updown_device = true;
            netplay_key_hton_init();
         }
         if (dtype != RETRO_DEVICE_NONE && !netplay_expected_input_size(netplay, 1<<i))
            RARCH_WARN("Netplay does not support input device %u\n", i+1);
      }
   }
   else
   {
      /* Start our handshake */
      netplay_handshake_init_send(netplay, &netplay->connections[0]);

      netplay->connections[0].mode = NETPLAY_CONNECTION_INIT;
      netplay->self_mode           = NETPLAY_CONNECTION_INIT;
   }

   /* FIXME: Not really the right place to do this,
    * socket initialization needs to be fixed in general. */
   if (netplay->is_server)
   {
      if (!socket_nonblock(netplay->listen_fd))
         goto error;
   }
   else
   {
      if (!socket_nonblock(netplay->connections[0].fd))
         goto error;
   }

   return netplay;

error:
   if (netplay->listen_fd >= 0)
      socket_close(netplay->listen_fd);

   if (netplay->connections && netplay->connections[0].fd >= 0)
      socket_close(netplay->connections[0].fd);

   free(netplay);
   return NULL;
}

/**
 * netplay_free
 * @netplay              : pointer to netplay object
 *
 * Frees netplay data/
 */
void netplay_free(netplay_t *netplay)
{
   size_t i;

   if (netplay->listen_fd >= 0)
      socket_close(netplay->listen_fd);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active)
      {
         socket_close(connection->fd);
         netplay_deinit_socket_buffer(&connection->send_packet_buffer);
         netplay_deinit_socket_buffer(&connection->recv_packet_buffer);
      }
   }

   if (netplay->connections && netplay->connections != &netplay->one_connection)
      free(netplay->connections);

   if (netplay->nat_traversal)
      natt_free(&netplay->nat_traversal_state);

   if (netplay->buffer)
   {
      for (i = 0; i < netplay->buffer_size; i++)
         netplay_delta_frame_free(&netplay->buffer[i]);

      free(netplay->buffer);
   }

   if (netplay->zbuffer)
      free(netplay->zbuffer);

   if (netplay->compress_nil.compression_stream)
   {
      netplay->compress_nil.compression_backend->stream_free(netplay->compress_nil.compression_stream);
      netplay->compress_nil.decompression_backend->stream_free(netplay->compress_nil.decompression_stream);
   }
   if (netplay->compress_zlib.compression_stream)
   {
      netplay->compress_zlib.compression_backend->stream_free(netplay->compress_zlib.compression_stream);
      netplay->compress_zlib.decompression_backend->stream_free(netplay->compress_zlib.decompression_stream);
   }

   if (netplay->addr)
      freeaddrinfo_retro(netplay->addr);

   free(netplay);
}
