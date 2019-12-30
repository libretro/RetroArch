/*  RetroArch - A frontend for libretro.
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

/*
 * AD PACKET FORMAT:
 *
 * Request:
 *    1 word: RANQ (RetroArch Netplay Query)
 *    1 word: Netplay protocol version
 *
 * Reply:
 *    1 word : RANS (RetroArch Netplay Server)
 *    1 word : Netplay protocol version
 *    1 word : Port
 *    8 words: RetroArch version
 *    8 words: Nick
 *    8 words: Core name
 *    8 words: Core version
 *    8 words: Content name (currently always blank)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../content.h"

#include <compat/strl.h>
#include <net/net_compat.h>

#include "../../retroarch.h"
#include "../../version.h"
#include "../../verbosity.h"

#include "netplay.h"
#include "netplay_discovery.h"
#include "netplay_private.h"

#if defined(AF_INET6) && !defined(HAVE_SOCKET_LEGACY)
#define HAVE_INET6 1
#endif

struct ad_packet
{
   uint32_t header;
   uint32_t protocol_version;
   uint32_t port;
   char address[NETPLAY_HOST_STR_LEN];
   char retroarch_version[NETPLAY_HOST_STR_LEN];
   char nick[NETPLAY_HOST_STR_LEN];
   char frontend[NETPLAY_HOST_STR_LEN];
   char core[NETPLAY_HOST_STR_LEN];
   char core_version[NETPLAY_HOST_STR_LEN];
   char content[NETPLAY_HOST_LONGSTR_LEN];
   char content_crc[NETPLAY_HOST_STR_LEN];
   char subsystem_name[NETPLAY_HOST_STR_LEN];
};

/* TODO/FIXME - globals - remove to make code thread-safe */
int netplay_room_count                 = 0;
struct netplay_room *netplay_room_list = NULL;

static bool netplay_lan_ad_client(void);

/* LAN discovery sockets */
static int lan_ad_server_fd            = -1;
static int lan_ad_client_fd            = -1;

/* Packet buffer for advertisement and responses */
static struct ad_packet ad_packet_buffer;

/* List of discovered hosts */
static struct netplay_host_list discovered_hosts;
static size_t discovered_hosts_allocated;

static struct netplay_room netplay_host_room = {0};

struct netplay_room* netplay_get_host_room(void)
{
   return &netplay_host_room;
}

/** Initialize Netplay discovery (client) */
bool init_netplay_discovery(void)
{
   struct addrinfo *addr = NULL;
   int fd = socket_init((void **) &addr, 0, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   if (!socket_bind(fd, (void*)addr))
   {
      socket_close(fd);
      goto error;
   }

   lan_ad_client_fd = fd;
   freeaddrinfo_retro(addr);
   return true;

error:
   if (addr)
      freeaddrinfo_retro(addr);
   RARCH_ERR("[discovery] Failed to initialize netplay advertisement client socket.\n");
   return false;
}

/** Deinitialize and free Netplay discovery */
void deinit_netplay_discovery(void)
{
   if (lan_ad_client_fd >= 0)
   {
      socket_close(lan_ad_client_fd);
      lan_ad_client_fd = -1;
   }
}

/** Discovery control */
/* Todo: implement net_ifinfo and ntohs for consoles */
bool netplay_discovery_driver_ctl(enum rarch_netplay_discovery_ctl_state state, void *data)
{
#ifdef HAVE_NETPLAYDISCOVERY
   char port_str[6];
   int ret;
   unsigned k = 0;

   if (lan_ad_client_fd < 0)
      return false;

   switch (state)
   {
      case RARCH_NETPLAY_DISCOVERY_CTL_LAN_SEND_QUERY:
      {
         net_ifinfo_t interfaces;
         struct addrinfo hints = {0}, *addr;
         int canBroadcast      = 1;

         if (!net_ifinfo_new(&interfaces))
            return false;

         /* Get the broadcast address (IPv4 only for now) */
         snprintf(port_str, 6, "%hu", (unsigned short) RARCH_DEFAULT_PORT);
         if (getaddrinfo_retro("255.255.255.255", port_str, &hints, &addr) < 0)
            return false;

         /* Make it broadcastable */
#if defined(SOL_SOCKET) && defined(SO_BROADCAST)
         if (setsockopt(lan_ad_client_fd, SOL_SOCKET, SO_BROADCAST,
                  (const char *)&canBroadcast, sizeof(canBroadcast)) < 0)
            RARCH_WARN("[discovery] Failed to set netplay discovery port to broadcast\n");
#endif

         /* Put together the request */
         memcpy((void *) &ad_packet_buffer, "RANQ", 4);
         ad_packet_buffer.protocol_version = htonl(NETPLAY_PROTOCOL_VERSION);

         for (k = 0; k < (unsigned)interfaces.size; k++)
         {
            strlcpy(ad_packet_buffer.address, interfaces.entries[k].host,
               NETPLAY_HOST_STR_LEN);

            /* And send it off */
            ret = (int)sendto(lan_ad_client_fd, (const char *) &ad_packet_buffer,
               sizeof(struct ad_packet), 0, addr->ai_addr, addr->ai_addrlen);
            if (ret < (ssize_t) (2*sizeof(uint32_t)))
               RARCH_WARN("[discovery] Failed to send netplay discovery query (error: %d)\n", errno);
         }

         freeaddrinfo_retro(addr);
         net_ifinfo_free(&interfaces);

         break;
      }

      case RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES:
         if (!netplay_lan_ad_client())
            return false;
         *((struct netplay_host_list **) data) = &discovered_hosts;
         break;

      case RARCH_NETPLAY_DISCOVERY_CTL_LAN_CLEAR_RESPONSES:
         discovered_hosts.size = 0;
         break;

      default:
         return false;
   }
#endif
   return true;
}

static bool init_lan_ad_server_socket(netplay_t *netplay, uint16_t port)
{
   struct addrinfo *addr = NULL;
   int fd = socket_init((void **) &addr, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   if (!socket_bind(fd, (void*)addr))
   {
      socket_close(fd);
      goto error;
   }

   lan_ad_server_fd = fd;
   freeaddrinfo_retro(addr);

   return true;

error:
   if (addr)
      freeaddrinfo_retro(addr);
   RARCH_ERR("[discovery] Failed to initialize netplay advertisement socket\n");
   return false;
}

/**
 * netplay_lan_ad_server
 *
 * Respond to any LAN ad queries that the netplay server has received.
 */
bool netplay_lan_ad_server(netplay_t *netplay)
{
/* Todo: implement net_ifinfo and ntohs for consoles */
#ifdef HAVE_NETPLAYDISCOVERY
   fd_set fds;
   int ret;
   struct timeval tmp_tv = {0};
   struct sockaddr their_addr;
   socklen_t addr_size;
   unsigned k = 0;
   char reply_addr[NETPLAY_HOST_STR_LEN], port_str[6];
   struct addrinfo *our_addr, hints = {0};
   struct string_list *subsystem    = path_get_subsystem_list();
   char buf[4096];

   net_ifinfo_t interfaces;

   if (!net_ifinfo_new(&interfaces))
      return false;

   if (lan_ad_server_fd < 0 && !init_lan_ad_server_socket(netplay, RARCH_DEFAULT_PORT))
      return false;

   /* Check for any ad queries */
   while (1)
   {
      FD_ZERO(&fds);
      FD_SET(lan_ad_server_fd, &fds);
      if (socket_select(lan_ad_server_fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
         break;
      if (!FD_ISSET(lan_ad_server_fd, &fds))
         break;

      /* Somebody queried, so check that it's valid */
      addr_size = sizeof(their_addr);
      ret       = (int)recvfrom(lan_ad_server_fd, (char*)&ad_packet_buffer,
            sizeof(struct ad_packet), 0, &their_addr, &addr_size);
      if (ret >= (ssize_t) (2 * sizeof(uint32_t)))
      {
         char s[NETPLAY_HOST_STR_LEN];
         uint32_t content_crc         = 0;

         /* Make sure it's a valid query */
         if (memcmp((void *) &ad_packet_buffer, "RANQ", 4))
         {
            RARCH_LOG("[discovery] invalid query\n");
            continue;
         }

         /* For this version */
         if (ntohl(ad_packet_buffer.protocol_version) !=
               NETPLAY_PROTOCOL_VERSION)
         {
            RARCH_LOG("[discovery] invalid protocol version\n");
            continue;
         }

         if (!string_is_empty(ad_packet_buffer.address))
            strlcpy(reply_addr, ad_packet_buffer.address, NETPLAY_HOST_STR_LEN);

         for (k = 0; k < interfaces.size; k++)
         {
            char *p;
            char sub[NETPLAY_HOST_STR_LEN];
            char frontend[NETPLAY_HOST_STR_LEN];
            netplay_get_architecture(frontend, sizeof(frontend));

            p=strrchr(reply_addr,'.');
            if (p)
            {
               strlcpy(sub, reply_addr, p - reply_addr + 1);
               if (strstr(interfaces.entries[k].host, sub) &&
                  !strstr(interfaces.entries[k].host, "127.0.0.1"))
               {
                  struct retro_system_info *info = runloop_get_libretro_system_info();

                  RARCH_LOG ("[discovery] query received on common interface: %s/%s (theirs / ours) \n",
                     reply_addr, interfaces.entries[k].host);

                  /* Now build our response */
                  buf[0] = '\0';
                  content_crc = content_get_crc();

                  memset(&ad_packet_buffer, 0, sizeof(struct ad_packet));
                  memcpy(&ad_packet_buffer, "RANS", 4);

                  if (subsystem)
                  {
                     unsigned i;

                     for (i = 0; i < subsystem->size; i++)
                     {
                        strlcat(buf, path_basename(subsystem->elems[i].data), NETPLAY_HOST_LONGSTR_LEN);
                        if (i < subsystem->size - 1)
                           strlcat(buf, "|", NETPLAY_HOST_LONGSTR_LEN);
                     }
                     strlcpy(ad_packet_buffer.content, buf,
                        NETPLAY_HOST_LONGSTR_LEN);
                     strlcpy(ad_packet_buffer.subsystem_name, path_get(RARCH_PATH_SUBSYSTEM),
                        NETPLAY_HOST_STR_LEN);
                  }
                  else
                  {
                     strlcpy(ad_packet_buffer.content, !string_is_empty(
                              path_basename(path_get(RARCH_PATH_BASENAME)))
                           ? path_basename(path_get(RARCH_PATH_BASENAME)) : "N/A",
                           NETPLAY_HOST_LONGSTR_LEN);
                     strlcpy(ad_packet_buffer.subsystem_name, "N/A", NETPLAY_HOST_STR_LEN);
                  }

                  strlcpy(ad_packet_buffer.address, interfaces.entries[k].host,
                     NETPLAY_HOST_STR_LEN);
                  ad_packet_buffer.protocol_version =
                     htonl(NETPLAY_PROTOCOL_VERSION);
                  ad_packet_buffer.port = htonl(netplay->tcp_port);
                  strlcpy(ad_packet_buffer.retroarch_version, PACKAGE_VERSION,
                     NETPLAY_HOST_STR_LEN);
                  strlcpy(ad_packet_buffer.nick, netplay->nick, NETPLAY_HOST_STR_LEN);
                  strlcpy(ad_packet_buffer.frontend, frontend, NETPLAY_HOST_STR_LEN);

                  if (info)
                  {
                     strlcpy(ad_packet_buffer.core, info->library_name,
                        NETPLAY_HOST_STR_LEN);
                     strlcpy(ad_packet_buffer.core_version, info->library_version,
                        NETPLAY_HOST_STR_LEN);
                  }

                  snprintf(s, sizeof(s), "%d", content_crc);
                  strlcpy(ad_packet_buffer.content_crc, s,
                     NETPLAY_HOST_STR_LEN);

                  /* Build up the destination address*/
                  snprintf(port_str, 6, "%hu", ntohs(((struct sockaddr_in*)(&their_addr))->sin_port));
                  if (getaddrinfo_retro(reply_addr, port_str, &hints, &our_addr) < 0)
                     continue;

                  RARCH_LOG ("[discovery] sending reply to %s \n", reply_addr);

                  /* And send it */
                  sendto(lan_ad_server_fd, (const char*)&ad_packet_buffer,
                        sizeof(struct ad_packet), 0, our_addr->ai_addr, our_addr->ai_addrlen);
                  freeaddrinfo_retro(our_addr);
               }
               else
                  continue;
            }
            else
               continue;
         }
      }
   }
   net_ifinfo_free(&interfaces);
#endif
   return true;
}

#ifdef HAVE_SOCKET_LEGACY
/* The fact that I need to write this is deeply depressing */
static int16_t htons_for_morons(int16_t value)
{
   union {
      int32_t l;
      int16_t s[2];
   } val;
   val.l = htonl(value);
   return val.s[1];
}
#ifndef htons
#define htons htons_for_morons
#endif
#endif

static bool netplay_lan_ad_client(void)
{
   fd_set fds;
   socklen_t addr_size;
   struct sockaddr their_addr;
   struct timeval tmp_tv = {0};

   if (lan_ad_client_fd < 0)
      return false;

   /* Check for any ad queries */
   while (1)
   {
      FD_ZERO(&fds);
      FD_SET(lan_ad_client_fd, &fds);
      if (socket_select(lan_ad_client_fd + 1,
               &fds, NULL, NULL, &tmp_tv) <= 0)
         break;

      if (!FD_ISSET(lan_ad_client_fd, &fds))
         break;

      /* Somebody queried, so check that it's valid */
      addr_size = sizeof(their_addr);

      if (recvfrom(lan_ad_client_fd, (char*)&ad_packet_buffer,
            sizeof(struct ad_packet), 0, &their_addr, &addr_size) >=
            (ssize_t) sizeof(struct ad_packet))
      {
         struct netplay_host *host = NULL;

         /* Make sure it's a valid response */
         if (memcmp((void *) &ad_packet_buffer, "RANS", 4))
            continue;

         /* For this version */
         if (ntohl(ad_packet_buffer.protocol_version) != NETPLAY_PROTOCOL_VERSION)
            continue;

         /* And that we know how to handle it */
         if (their_addr.sa_family == AF_INET)
         {
            struct sockaddr_in *sin = NULL;

            RARCH_WARN ("[discovery] using IPv4 for discovery\n");
            sin           = (struct sockaddr_in *) &their_addr;
            sin->sin_port = htons(ntohl(ad_packet_buffer.port));

         }
#ifdef HAVE_INET6
         else if (their_addr.sa_family == AF_INET6)
         {
            struct sockaddr_in6 *sin6 = NULL;
            RARCH_WARN ("[discovery] using IPv6 for discovery\n");
            sin6            = (struct sockaddr_in6 *) &their_addr;
            sin6->sin6_port = htons(ad_packet_buffer.port);

         }
#endif
         else continue;

         /* Allocate space for it */
         if (discovered_hosts.size >= discovered_hosts_allocated)
         {
            size_t allocated               = discovered_hosts_allocated;
            struct netplay_host *new_hosts = NULL;

            if (allocated == 0)
               allocated  = 2;
            else
               allocated *= 2;

            if (discovered_hosts.hosts)
               new_hosts  = (struct netplay_host *)
                  realloc(discovered_hosts.hosts, allocated * sizeof(struct
                  netplay_host));
            else
               /* Should be equivalent to realloc, but I don't trust screwy libcs */
               new_hosts = (struct netplay_host *)
                  malloc(allocated * sizeof(struct netplay_host));

            if (!new_hosts)
               return false;

            discovered_hosts.hosts     = new_hosts;
            discovered_hosts_allocated = allocated;
         }

         /* Get our host structure */
         host = &discovered_hosts.hosts[discovered_hosts.size++];

         /* Copy in the response */
         memset(host, 0, sizeof(struct netplay_host));
         host->addr    = their_addr;
         host->addrlen = addr_size;

         host->port = ntohl(ad_packet_buffer.port);

         strlcpy(host->address, ad_packet_buffer.address, NETPLAY_HOST_STR_LEN);
         strlcpy(host->nick, ad_packet_buffer.nick, NETPLAY_HOST_STR_LEN);
         strlcpy(host->core, ad_packet_buffer.core, NETPLAY_HOST_STR_LEN);
         strlcpy(host->retroarch_version, ad_packet_buffer.retroarch_version,
            NETPLAY_HOST_STR_LEN);
         strlcpy(host->core_version, ad_packet_buffer.core_version,
            NETPLAY_HOST_STR_LEN);
         strlcpy(host->content, ad_packet_buffer.content,
            NETPLAY_HOST_LONGSTR_LEN);
         strlcpy(host->subsystem_name, ad_packet_buffer.subsystem_name,
            NETPLAY_HOST_LONGSTR_LEN);
         strlcpy(host->frontend, ad_packet_buffer.frontend,
            NETPLAY_HOST_STR_LEN);

         host->content_crc                  =
            atoi(ad_packet_buffer.content_crc);
         host->nick[NETPLAY_HOST_STR_LEN-1] =
            host->core[NETPLAY_HOST_STR_LEN-1] =
            host->core_version[NETPLAY_HOST_STR_LEN-1] =
            host->content[NETPLAY_HOST_LONGSTR_LEN-1] = '\0';
      }
   }

   return true;
}
