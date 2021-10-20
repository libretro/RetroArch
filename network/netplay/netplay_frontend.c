/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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
#include <string.h>

#include <boolean.h>

#include <compat/strl.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <encodings/crc32.h>
#include <lrc_hash.h>
#include <retro_timers.h>

#include <string/stdstring.h>
#include <file/file_path.h>

#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../content.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../autosave.h"
#include "../../configuration.h"
#include "../../command.h"
#include "../../content.h"
#include "../../driver.h"
#include "../../retroarch.h"
#include "../../version.h"
#include "../../verbosity.h"

#include "../../tasks/tasks_internal.h"

#include "../../input/input_driver.h"

#ifdef HAVE_MENU
#include "../../menu/menu_input.h"
#endif

#ifdef HAVE_DISCORD
#include "../discord.h"
#endif

#include "netplay.h"
#include "netplay_discovery.h"
#include "netplay_private.h"

#if defined(AF_INET6) && !defined(HAVE_SOCKET_LEGACY) && !defined(_3DS)
#define HAVE_INET6 1
#endif

#define RECV(buf, sz) \
   recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), (sz), false); \
   if (recvd >= 0 && recvd < (ssize_t) (sz)) \
   { \
      netplay_recv_reset(&connection->recv_packet_buffer); \
      return true; \
   } \
   else if (recvd < 0)

#define NETPLAY_MAGIC 0x52414E50 /* RANP */
#define POKE_MAGIC    0x504F4B45 /* POKE */

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

struct vote_count
{
   uint16_t votes[32];
};

struct nick_buf_s
{
   uint32_t cmd[2];
   char nick[NETPLAY_NICK_LEN];
};

struct password_buf_s
{
   uint32_t cmd[2];
   char password[NETPLAY_PASS_HASH_LEN];
};

struct info_buf_s
{
   uint32_t cmd[2];
   uint32_t content_crc;
   char core_name[NETPLAY_NICK_LEN];
   char core_version[NETPLAY_NICK_LEN];
};

#ifdef HAVE_NETPLAYDISCOVERY
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
#endif

/* Forward declarations */
#ifdef HAVE_DISCORD
extern bool discord_is_inited;
#endif

/* TODO/FIXME - globals */
struct netplay_room *netplay_room_list       = NULL;
int netplay_room_count                       = 0;
static netplay_t *handshake_password_netplay = NULL;
static unsigned long simple_rand_next        = 1;


#ifdef HAVE_NETPLAYDISCOVERY
/* LAN discovery sockets */
static int lan_ad_server_fd            = -1;
static int lan_ad_client_fd            = -1;

/* Packet buffer for advertisement and responses */
static struct ad_packet ad_packet_buffer;

/* List of discovered hosts */
static struct netplay_host_list discovered_hosts;

static size_t discovered_hosts_allocated;
#endif

/* The mapping of keys from netplay (network) to libretro (host) */
const uint16_t netplay_key_ntoh_mapping[] = {
   (uint16_t) RETROK_UNKNOWN,
#define K(k) (uint16_t) RETROK_ ## k,
#define KL(k,l) (uint16_t) l,
#include "netplay_keys.h"
#undef KL
#undef K
   0
};

/* TODO/FIXME - static global variables */
static uint16_t netplay_mapping[RETROK_LAST];

#ifdef HAVE_NETPLAYDISCOVERY
#ifdef HAVE_SOCKET_LEGACY

#ifndef htons
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
#define htons htons_for_morons
#endif

#endif

static bool netplay_lan_ad_client(void)
{
   unsigned i;
   fd_set fds;
   socklen_t addr_size;
   struct sockaddr their_addr;
   struct timeval tmp_tv    = {0};

   if (lan_ad_client_fd < 0)
      return false;

   their_addr.sa_family     = 0;
   for (i = 0; i < 14; i++)
      their_addr.sa_data[i] = 0;

   /* Check for any ad queries */
   for (;;)
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
         if (ntohl(ad_packet_buffer.protocol_version) 
               != NETPLAY_PROTOCOL_VERSION)
            continue;

         /* And that we know how to handle it */
         if (their_addr.sa_family == AF_INET)
         {
            struct sockaddr_in *sin = NULL;

            RARCH_WARN ("[Discovery] Using IPv4 for discovery\n");
            sin           = (struct sockaddr_in *) &their_addr;
            sin->sin_port = htons(ntohl(ad_packet_buffer.port));

         }
#ifdef HAVE_INET6
         else if (their_addr.sa_family == AF_INET6)
         {
            struct sockaddr_in6 *sin6 = NULL;
            RARCH_WARN ("[Discovery] Using IPv6 for discovery\n");
            sin6            = (struct sockaddr_in6 *) &their_addr;
            sin6->sin6_port = htons(ad_packet_buffer.port);

         }
#endif
         else
            continue;

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
               /* Should be equivalent to realloc, 
                * but I don't trust screwy libcs */
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

/** Initialize Netplay discovery (client) */
bool init_netplay_discovery(void)
{
   struct addrinfo *addr = NULL;
   int fd = socket_init((void **)&addr, 0, NULL, SOCKET_TYPE_DATAGRAM);

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
   RARCH_ERR("[Discovery] Failed to initialize netplay advertisement client socket.\n");
   return false;
}

/** Deinitialize and free Netplay discovery */
/* TODO/FIXME - this is apparently never called? */
void deinit_netplay_discovery(void)
{
   if (lan_ad_client_fd >= 0)
   {
      socket_close(lan_ad_client_fd);
      lan_ad_client_fd = -1;
   }
}

/** Discovery control */
/* TODO/FIXME: implement net_ifinfo and ntohs for consoles */
bool netplay_discovery_driver_ctl(
      enum rarch_netplay_discovery_ctl_state state, void *data)
{
   int ret;
   char port_str[6];
   unsigned k = 0;

   if (lan_ad_client_fd < 0)
      return false;

   switch (state)
   {
      case RARCH_NETPLAY_DISCOVERY_CTL_LAN_SEND_QUERY:
      {
         net_ifinfo_t interfaces;
         struct addrinfo hints = {0}, *addr;

         if (!net_ifinfo_new(&interfaces))
            return false;

         /* Get the broadcast address (IPv4 only for now) */
         snprintf(port_str, 6, "%hu", (unsigned short) RARCH_DEFAULT_PORT);
         if (getaddrinfo_retro("255.255.255.255", port_str, &hints, &addr) < 0)
            return false;

         /* Make it broadcastable */
#if defined(SOL_SOCKET) && defined(SO_BROADCAST)
         {
            int can_broadcast     = 1;
            if (setsockopt(lan_ad_client_fd, SOL_SOCKET, SO_BROADCAST,
                  (const char *)&can_broadcast, sizeof(can_broadcast)) < 0)
               RARCH_WARN("[Discovery] Failed to set netplay discovery port to broadcast\n");
         }
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
               RARCH_WARN("[Discovery] Failed to send netplay discovery query (error: %d)\n", errno);
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
   return false;
}

/**
 * netplay_lan_ad_server
 *
 * Respond to any LAN ad queries that the netplay server has received.
 */
bool netplay_lan_ad_server(netplay_t *netplay)
{
   /* TODO/FIXME: implement net_ifinfo and ntohs for consoles */
   fd_set fds;
   int ret;
   unsigned i;
   char buf[4096];
   net_ifinfo_t interfaces;
   socklen_t addr_size;
   char reply_addr[NETPLAY_HOST_STR_LEN], port_str[6];
   struct sockaddr their_addr;
   struct timeval tmp_tv            = {0};
   unsigned k                       = 0;
   struct addrinfo *our_addr, hints = {0};
   struct string_list *subsystem    = path_get_subsystem_list();

   interfaces.entries               = NULL;
   interfaces.size                  = 0;

   their_addr.sa_family             = 0;
   for (i = 0; i < 14; i++)
      their_addr.sa_data[i]         = 0;

   if (!net_ifinfo_new(&interfaces))
      return false;

   if (     (lan_ad_server_fd < 0)
         && !init_lan_ad_server_socket(netplay, RARCH_DEFAULT_PORT))
   {
      RARCH_ERR("[Discovery] Failed to initialize netplay advertisement socket\n");
      return false;
   }

   /* Check for any ad queries */
   for (;;)
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
            RARCH_LOG("[Discovery] Invalid query\n");
            continue;
         }

         /* For this version */
         if (ntohl(ad_packet_buffer.protocol_version) !=
               NETPLAY_PROTOCOL_VERSION)
         {
            RARCH_LOG("[Discovery] Invalid protocol version\n");
            continue;
         }

         if (!string_is_empty(ad_packet_buffer.address))
            strlcpy(reply_addr, ad_packet_buffer.address, NETPLAY_HOST_STR_LEN);

         for (k = 0; k < interfaces.size; k++)
         {
            char *p;
            char sub[NETPLAY_HOST_STR_LEN];
            char frontend_architecture_tmp[32];
            char frontend[256];
            const frontend_ctx_driver_t *frontend_drv = 
               (const frontend_ctx_driver_t*)
            frontend_driver_get_cpu_architecture_str(
                  frontend_architecture_tmp, sizeof(frontend_architecture_tmp));
            snprintf(frontend, sizeof(frontend), "%s %s",
                  frontend_drv->ident, frontend_architecture_tmp);

            p=strrchr(reply_addr,'.');
            if (p)
            {
               strlcpy(sub, reply_addr, p - reply_addr + 1);
               if (strstr(interfaces.entries[k].host, sub) &&
                  !strstr(interfaces.entries[k].host, "127.0.0.1"))
               {
                  struct retro_system_info *info = &runloop_state_get_ptr()->system.info;

                  RARCH_LOG ("[Discovery] Query received on common interface: %s/%s (theirs / ours) \n",
                     reply_addr, interfaces.entries[k].host);

                  /* Now build our response */
                  buf[0]      = '\0';
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

                  snprintf(s, sizeof(s), "%ld", (long)content_crc);
                  strlcpy(ad_packet_buffer.content_crc, s,
                     NETPLAY_HOST_STR_LEN);

                  /* Build up the destination address*/
                  snprintf(port_str, 6, "%hu", ntohs(((struct sockaddr_in*)(&their_addr))->sin_port));
                  if (getaddrinfo_retro(reply_addr, port_str, &hints, &our_addr) < 0)
                     continue;

                  RARCH_LOG ("[Discovery] Sending reply to %s \n", reply_addr);

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
   return true;
}
#endif

/* TODO/FIXME - replace netplay_log_connection with calls
 * to inet_ntop_compat and move runloop message queue pushing
 * outside */
#if !defined(HAVE_SOCKET_LEGACY) && !defined(WIIU) && !defined(_3DS)
/* Custom inet_ntop. Win32 doesn't seem to support this ... */
static void netplay_log_connection(
      const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick, char *s, size_t len)
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

   u.storage                     = their_addr;

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
      snprintf(s, len, msg_hash_to_str(MSG_GOT_CONNECTION_FROM_NAME),
            nick, str);
   else
      snprintf(s, len, msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
            nick);
}
#else
static void netplay_log_connection(
      const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick, char *s, size_t len)
{
   /* Stub code - will need to be implemented */
   snprintf(s, len, msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
         nick);
}
#endif

/**
 * netplay_impl_magic:
 *
 * A pseudo-hash of the RetroArch and Netplay version, so only compatible
 * versions play together.
 */
static uint32_t netplay_impl_magic(void)
{
   size_t i;
   uint32_t res                        = 0;
   const char *ver                     = PACKAGE_VERSION;
   size_t len                          = strlen(ver);

   for (i = 0; i < len; i++)
      res ^= ver[i] << (i & 0xf);

   res ^= NETPLAY_PROTOCOL_VERSION << (i & 0xf);

   return res;
}

/**
 * netplay_platform_magic
 *
 * Just enough info to tell us if our platforms mismatch: Endianness and a
 * couple of type sizes.
 *
 * Format:
 *    bit 31:     Reserved
 *    bit 30:     1 for big endian
 *    bits 29-15: sizeof(size_t)
 *    bits 14-0:  sizeof(long)
 */
static uint32_t netplay_platform_magic(void)
{
   uint32_t ret =
       ((1 == htonl(1)) << 30)
      |(sizeof(size_t) << 15)
      |(sizeof(long));
   return ret;
}

/**
 * netplay_endian_mismatch
 *
 * Do the platform magics mismatch on endianness?
 */
static bool netplay_endian_mismatch(uint32_t pma, uint32_t pmb)
{
   uint32_t ebit = (1<<30);
   return (pma & ebit) != (pmb & ebit);
}

static int simple_rand(void)
{
   simple_rand_next = simple_rand_next * 1103515245 + 12345;
   return((unsigned)(simple_rand_next / 65536) % 32768);
}

static void simple_srand(unsigned int seed)
{
   simple_rand_next = seed;
}

static uint32_t simple_rand_uint32(void)
{
   uint32_t parts[3];
   parts[0] = simple_rand();
   parts[1] = simple_rand();
   parts[2] = simple_rand();
   return ((parts[0] << 30) +
           (parts[1] << 15) +
            parts[2]);
}

/**
 * netplay_handshake_init_send
 *
 * Initialize our handshake and send the first part of the handshake protocol.
 */
bool netplay_handshake_init_send(netplay_t *netplay,
   struct netplay_connection *connection)
{
   uint32_t header[6];
   unsigned conn_salt   = 0;
   settings_t *settings = config_get_ptr();

   header[0] = htonl(NETPLAY_MAGIC);
   header[1] = htonl(netplay_platform_magic());
   header[2] = htonl(NETPLAY_COMPRESSION_SUPPORTED);
   header[3] = 0;
   header[4] = htonl(NETPLAY_PROTOCOL_VERSION);
   header[5] = htonl(netplay_impl_magic());

   if (netplay->is_server &&
       (settings->paths.netplay_password[0] ||
        settings->paths.netplay_spectate_password[0]))
   {
      /* Demand a password */
      if (simple_rand_next == 1)
         simple_srand((unsigned int) time(NULL));
      connection->salt = simple_rand_uint32();
      if (connection->salt == 0)
         connection->salt = 1;
      conn_salt           = connection->salt;
   }

   header[3] = htonl(conn_salt);

   if (!netplay_send(&connection->send_packet_buffer, connection->fd, header,
         sizeof(header)) ||
       !netplay_send_flush(&connection->send_packet_buffer, connection->fd, false))
      return false;

   return true;
}

#ifdef HAVE_MENU
static void handshake_password(void *ignore, const char *line)
{
   struct password_buf_s password_buf;
   char password[8+NETPLAY_PASS_LEN]; /* 8 for salt, 128 for password */
   char hash[NETPLAY_PASS_HASH_LEN+1]; /* + NULL terminator */
   netplay_t *netplay                    = handshake_password_netplay;
   struct netplay_connection *connection = &netplay->connections[0];

   snprintf(password, sizeof(password), "%08lX", (unsigned long)connection->salt);
   if (!string_is_empty(line))
      strlcpy(password + 8, line, sizeof(password)-8);

   password_buf.cmd[0] = htonl(NETPLAY_CMD_PASSWORD);
   password_buf.cmd[1] = htonl(sizeof(password_buf.password));
   sha256_hash(hash, (uint8_t *) password, strlen(password));
   memcpy(password_buf.password, hash, NETPLAY_PASS_HASH_LEN);

   /* We have no way to handle an error here, so we'll let the next function error out */
   if (netplay_send(&connection->send_packet_buffer, connection->fd, &password_buf, sizeof(password_buf)))
      netplay_send_flush(&connection->send_packet_buffer, connection->fd, false);

#ifdef HAVE_MENU
   menu_input_dialog_end();
   retroarch_menu_running_finished(false);
#endif
}
#endif

/**
 * netplay_deinit_socket_buffer
 *
 * Free a socket buffer.
 */
static void netplay_deinit_socket_buffer(struct socket_buffer *sbuf)
{
   if (sbuf->data)
      free(sbuf->data);
}


static bool netplay_poke(netplay_t *netplay, struct netplay_connection *connection, uint32_t netplay_magic)
{
   if (!netplay || !netplay->is_server)
      return false;
   if (!connection || !connection->active)
      return false;
   if (netplay_magic != POKE_MAGIC)
      return false;

   socket_close(connection->fd);

   connection->active = false;

   netplay_deinit_socket_buffer(&connection->send_packet_buffer);
   netplay_deinit_socket_buffer(&connection->recv_packet_buffer);

   return true;
}

/**
 * netplay_handshake_init
 *
 * Data receiver for the initial part of the handshake, i.e., waiting for the
 * netplay header.
 */
bool netplay_handshake_init(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   ssize_t recvd;
   struct nick_buf_s nick_buf;
   uint32_t header[6];
   uint32_t netplay_magic                = 0;
   uint32_t local_pmagic                 = 0;
   uint32_t remote_pmagic                = 0;
   uint32_t remote_version               = 0;
   uint32_t compression                  = 0;
   struct compression_transcoder *ctrans = NULL;
   const char *dmsg                      = NULL;

   memset(header, 0, sizeof(header));

   RECV(header, sizeof(uint32_t))
   {
      dmsg = msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT);
      goto error;
   }

   netplay_magic = ntohl(header[0]);

   if (netplay_poke(netplay, connection, netplay_magic))
      return true;

   if (netplay_magic != NETPLAY_MAGIC)
   {
      dmsg = msg_hash_to_str(MSG_NETPLAY_NOT_RETROARCH);
      goto error;
   }

   RECV(header + 1, sizeof(header) - sizeof(uint32_t))
   {
      dmsg = msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT);
      goto error;
   }

   remote_version = ntohl(header[4]);
   if (remote_version < NETPLAY_PROTOCOL_VERSION)
   {
      dmsg = msg_hash_to_str(MSG_NETPLAY_OUT_OF_DATE);
      goto error;
   }

   if (ntohl(header[5]) != netplay_impl_magic())
   {
      /* We allow the connection but warn that this could cause issues. */
      dmsg = msg_hash_to_str(MSG_NETPLAY_DIFFERENT_VERSIONS);
      RARCH_WARN("%s\n", dmsg);
      runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* We only care about platform magic if our core is quirky */
   local_pmagic  = netplay_platform_magic();
   remote_pmagic = ntohl(header[1]);

   if ((netplay->quirks & NETPLAY_QUIRK_ENDIAN_DEPENDENT) &&
       netplay_endian_mismatch(local_pmagic, remote_pmagic))
   {
      RARCH_ERR("Endianness mismatch with an endian-sensitive core.\n");
      dmsg = msg_hash_to_str(MSG_NETPLAY_ENDIAN_DEPENDENT);
      goto error;
   }
   if ((netplay->quirks & NETPLAY_QUIRK_PLATFORM_DEPENDENT) &&
       (local_pmagic != remote_pmagic))
   {
      RARCH_ERR("Platform mismatch with a platform-sensitive core.\n");
      dmsg = msg_hash_to_str(MSG_NETPLAY_PLATFORM_DEPENDENT);
      goto error;
   }

   /* Check what compression is supported */
   compression  = ntohl(header[2]);
   compression &= NETPLAY_COMPRESSION_SUPPORTED;

   if (compression & NETPLAY_COMPRESSION_ZLIB)
   {
      ctrans = &netplay->compress_zlib;
      if (!ctrans->compression_backend)
      {
         ctrans->compression_backend =
            trans_stream_get_zlib_deflate_backend();
         if (!ctrans->compression_backend)
            ctrans->compression_backend = trans_stream_get_pipe_backend();
      }
      connection->compression_supported = NETPLAY_COMPRESSION_ZLIB;
   }
   else
   {
      ctrans = &netplay->compress_nil;
      if (!ctrans->compression_backend)
      {
         ctrans->compression_backend =
            trans_stream_get_pipe_backend();
      }
      connection->compression_supported = 0;
   }

   if (!ctrans->decompression_backend)
      ctrans->decompression_backend = ctrans->compression_backend->reverse;

   /* Allocate our compression stream */
   if (!ctrans->compression_stream)
   {
      ctrans->compression_stream   = ctrans->compression_backend->stream_new();
      ctrans->decompression_stream = ctrans->decompression_backend->stream_new();
   }
   if (!ctrans->compression_stream || !ctrans->decompression_stream)
   {
      RARCH_ERR("Failed to allocate compression transcoder!\n");
      return false;
   }

   /* If a password is demanded, ask for it */
   if (!netplay->is_server && (connection->salt = ntohl(header[3])))
   {
#ifdef HAVE_MENU
      menu_input_ctx_line_t line;
      retroarch_menu_running();
#endif

      handshake_password_netplay = netplay;

#ifdef HAVE_MENU
      memset(&line, 0, sizeof(line));
      line.label         = msg_hash_to_str(MSG_NETPLAY_ENTER_PASSWORD);
      line.label_setting = "no_setting";
      line.cb            = handshake_password;
      if (!menu_input_dialog_start(&line))
         return false;
#endif
   }

   /* Send our nick */
   nick_buf.cmd[0] = htonl(NETPLAY_CMD_NICK);
   nick_buf.cmd[1] = htonl(sizeof(nick_buf.nick));
   memset(nick_buf.nick, 0, sizeof(nick_buf.nick));
   strlcpy(nick_buf.nick, netplay->nick, sizeof(nick_buf.nick));
   if (!netplay_send(&connection->send_packet_buffer, connection->fd, &nick_buf,
         sizeof(nick_buf)) ||
       !netplay_send_flush(&connection->send_packet_buffer, connection->fd, false))
      return false;

   /* Move on to the next mode */
   connection->mode = NETPLAY_CONNECTION_PRE_NICK;
   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;

error:
   if (dmsg)
   {
      RARCH_ERR("%s\n", dmsg);
      runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return false;
}

static void netplay_handshake_ready(netplay_t *netplay,
      struct netplay_connection *connection)
{
   char msg[512];
   msg[0] = '\0';

   if (netplay->is_server)
   {
      unsigned slot = (unsigned)(connection - netplay->connections);

      netplay_log_connection(&connection->addr,
            slot, connection->nick, msg, sizeof(msg));

      RARCH_LOG("%s %u\n", msg_hash_to_str(MSG_CONNECTION_SLOT), slot);

      /* Send them the savestate */
      if (!(netplay->quirks &
               (NETPLAY_QUIRK_NO_SAVESTATES|NETPLAY_QUIRK_NO_TRANSMISSION)))
         netplay->force_send_savestate = true;
   }
   else
   {
      netplay->is_connected = true;
      snprintf(msg, sizeof(msg), "%s: \"%s\"",
            msg_hash_to_str(MSG_CONNECTED_TO),
            connection->nick);
   }

   RARCH_LOG("%s\n", msg);
   runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   /* Unstall if we were waiting for this */
   if (netplay->stall == NETPLAY_STALL_NO_CONNECTION)
       netplay->stall = NETPLAY_STALL_NONE;
}

/**
 * netplay_handshake_info
 *
 * Send an INFO command.
 */
static bool netplay_handshake_info(netplay_t *netplay,
      struct netplay_connection *connection)
{
   struct info_buf_s info_buf;
   uint32_t      content_crc        = 0;
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;

   memset(&info_buf, 0, sizeof(info_buf));
   info_buf.cmd[0] = htonl(NETPLAY_CMD_INFO);
   info_buf.cmd[1] = htonl(sizeof(info_buf) - 2*sizeof(uint32_t));

   /* Get our core info */
   if (system)
   {
      strlcpy(info_buf.core_name,
            system->library_name, sizeof(info_buf.core_name));
      strlcpy(info_buf.core_version,
            system->library_version, sizeof(info_buf.core_version));
   }
   else
   {
      strlcpy(info_buf.core_name,
            "UNKNOWN", sizeof(info_buf.core_name));
      strlcpy(info_buf.core_version,
            "UNKNOWN", sizeof(info_buf.core_version));
   }

   /* Get our content CRC */
   content_crc = content_get_crc();

   if (content_crc != 0)
      info_buf.content_crc = htonl(content_crc);

   /* Send it off and wait for info back */
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
         &info_buf, sizeof(info_buf)) ||
       !netplay_send_flush(&connection->send_packet_buffer, connection->fd,
         false))
      return false;

   connection->mode = NETPLAY_CONNECTION_PRE_INFO;
   return true;
}

/**
 * netplay_handshake_sync
 *
 * Send a SYNC command.
 */
static bool netplay_handshake_sync(netplay_t *netplay,
      struct netplay_connection *connection)
{
   /* If we're the server, now we send sync info */
   size_t i;
   int matchct;
   uint32_t cmd[4];
   retro_ctx_memory_info_t mem_info;
   uint32_t client_num        = 0;
   uint32_t device            = 0;
   size_t nicklen, nickmangle = 0;
   bool nick_matched          = false;

#ifdef HAVE_THREADS
   autosave_lock();
#endif
   mem_info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&mem_info);
#ifdef HAVE_THREADS
   autosave_unlock();
#endif

   /* Send basic sync info */
   cmd[0]     = htonl(NETPLAY_CMD_SYNC);
   cmd[1]     = htonl(2*sizeof(uint32_t)
         /* Controller devices */
         + MAX_INPUT_DEVICES*sizeof(uint32_t)

         /* Share modes */
         + MAX_INPUT_DEVICES*sizeof(uint8_t)

         /* Device-client mapping */
         + MAX_INPUT_DEVICES*sizeof(uint32_t)

         /* Client nick */
         + NETPLAY_NICK_LEN

         /* And finally, sram */
         + mem_info.size);
   cmd[2]     = htonl(netplay->self_frame_count);
   client_num = (uint32_t)(connection - netplay->connections + 1);

   if (netplay->local_paused || netplay->remote_paused)
      client_num |= NETPLAY_CMD_SYNC_BIT_PAUSED;

   cmd[3]     = htonl(client_num);

   if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmd,
            sizeof(cmd)))
      return false;

   /* Now send the device info */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      device = htonl(netplay->config_devices[i]);
      if (!netplay_send(&connection->send_packet_buffer, connection->fd,
            &device, sizeof(device)))
         return false;
   }

   /* Then the share mode */
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
         netplay->device_share_modes, sizeof(netplay->device_share_modes)))
      return false;

   /* Then the device-client mapping */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      device = htonl(netplay->device_clients[i]);
      if (!netplay_send(&connection->send_packet_buffer, connection->fd,
            &device, sizeof(device)))
         return false;
   }

   /* Now see if we need to mangle their nick */
   nicklen = strlen(connection->nick);
   if (nicklen > NETPLAY_NICK_LEN - 5)
      nickmangle = NETPLAY_NICK_LEN - 5;
   else
      nickmangle = nicklen;
   matchct = 1;
   do
   {
      nick_matched = false;
      for (i = 0; i < netplay->connections_size; i++)
      {
         struct netplay_connection *sc = &netplay->connections[i];
         if (sc == connection)
            continue;
         if (sc->active &&
               sc->mode >= NETPLAY_CONNECTION_CONNECTED &&
               !strncmp(connection->nick, sc->nick, NETPLAY_NICK_LEN))
         {
            nick_matched = true;
            break;
         }
      }
      if (!strncmp(connection->nick, netplay->nick, NETPLAY_NICK_LEN))
         nick_matched = true;

      if (nick_matched)
      {
         /* Somebody has this nick, make a new one! */
         snprintf(connection->nick + nickmangle,
               NETPLAY_NICK_LEN - nickmangle, " (%d)", ++matchct);
         connection->nick[NETPLAY_NICK_LEN - 1] = '\0';
      }
   } while (nick_matched);

   /* Send the nick */
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
         connection->nick, NETPLAY_NICK_LEN))
      return false;

   /* And finally, the SRAM */
#ifdef HAVE_THREADS
   autosave_lock();
#endif
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
            mem_info.data, mem_info.size) ||
         !netplay_send_flush(&connection->send_packet_buffer, connection->fd,
            false))
   {
#ifdef HAVE_THREADS
      autosave_unlock();
#endif
      return false;
   }
#ifdef HAVE_THREADS
   autosave_unlock();
#endif

   /* Now we're ready! */
   connection->mode = NETPLAY_CONNECTION_SPECTATING;
   netplay_handshake_ready(netplay, connection);

   return true;
}

/**
 * netplay_handshake_pre_nick
 *
 * Data receiver for the second stage of handshake, receiving the other side's
 * nickname.
 */
static bool netplay_handshake_pre_nick(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   struct nick_buf_s nick_buf;
   ssize_t recvd;
   char msg[512];

   msg[0] = '\0';

   RECV(&nick_buf, sizeof(nick_buf)) {}

   /* Expecting only a nick command */
   if (recvd < 0 ||
       ntohl(nick_buf.cmd[0]) != NETPLAY_CMD_NICK ||
       ntohl(nick_buf.cmd[1]) != sizeof(nick_buf.nick))
   {
      if (netplay->is_server)
         strlcpy(msg, msg_hash_to_str(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT),
            sizeof(msg));
      else
         strlcpy(msg,
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST),
            sizeof(msg));
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return false;
   }

   strlcpy(connection->nick, nick_buf.nick,
      (sizeof(connection->nick) < sizeof(nick_buf.nick)) ?
      sizeof(connection->nick) : sizeof(nick_buf.nick));

   if (netplay->is_server)
   {
      settings_t *settings = config_get_ptr();

      /* There's a password, so just put them in PRE_PASSWORD mode */
      if (  settings->paths.netplay_password[0] ||
            settings->paths.netplay_spectate_password[0])
         connection->mode = NETPLAY_CONNECTION_PRE_PASSWORD;
      else
      {
         connection->can_play = true;
         if (!netplay_handshake_info(netplay, connection))
            return false;
         connection->mode = NETPLAY_CONNECTION_PRE_INFO;
      }
   }
   /* Client needs to wait for INFO */
   else
      connection->mode = NETPLAY_CONNECTION_PRE_INFO;

   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;
}

/**
 * netplay_handshake_pre_password
 *
 * Data receiver for the third, optional stage of server handshake, receiving
 * the password and sending core/content info.
 */
static bool netplay_handshake_pre_password(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   struct password_buf_s password_buf;
   char password[8+NETPLAY_PASS_LEN]; /* 8 for salt */
   char hash[NETPLAY_PASS_HASH_LEN+1]; /* + NULL terminator */
   ssize_t recvd;
   char msg[512];
   bool correct         = false;
   settings_t *settings = config_get_ptr();

   msg[0] = '\0';

   RECV(&password_buf, sizeof(password_buf)) {}

   /* Expecting only a password command */
   if (recvd < 0 ||
       ntohl(password_buf.cmd[0]) != NETPLAY_CMD_PASSWORD ||
       ntohl(password_buf.cmd[1]) != sizeof(password_buf.password))
   {
      if (netplay->is_server)
         strlcpy(msg, msg_hash_to_str(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT),
            sizeof(msg));
      else
         strlcpy(msg,
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST),
            sizeof(msg));
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return false;
   }

   /* Calculate the correct password hash(es) and compare */
   correct = false;
   snprintf(password, sizeof(password), "%08lX", (unsigned long)connection->salt);

   if (settings->paths.netplay_password[0])
   {
      strlcpy(password + 8,
            settings->paths.netplay_password, sizeof(password)-8);

      sha256_hash(hash, (uint8_t *) password, strlen(password));

      if (!memcmp(password_buf.password, hash, NETPLAY_PASS_HASH_LEN))
      {
         correct              = true;
         connection->can_play = true;
      }
   }
   if (settings->paths.netplay_spectate_password[0])
   {
      strlcpy(password + 8,
            settings->paths.netplay_spectate_password, sizeof(password)-8);

      sha256_hash(hash, (uint8_t *) password, strlen(password));

      if (!memcmp(password_buf.password, hash, NETPLAY_PASS_HASH_LEN))
         correct = true;
   }

   /* Just disconnect if it was wrong */
   if (!correct)
      return false;

   /* Otherwise, exchange info */
   if (!netplay_handshake_info(netplay, connection))
      return false;

   *had_input = true;
   connection->mode = NETPLAY_CONNECTION_PRE_INFO;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;
}

/**
 * netplay_handshake_pre_info
 *
 * Data receiver for the third stage of server handshake, receiving
 * the password.
 */
static bool netplay_handshake_pre_info(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   struct info_buf_s info_buf;
   uint32_t cmd_size;
   ssize_t recvd;
   uint32_t content_crc             = 0;
   const char *dmsg                 = NULL;
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;

   RECV(&info_buf, sizeof(info_buf.cmd)) {}

   if (recvd < 0 ||
       ntohl(info_buf.cmd[0]) != NETPLAY_CMD_INFO)
   {
      RARCH_ERR("Failed to receive netplay info.\n");
      return false;
   }

   cmd_size = ntohl(info_buf.cmd[1]);
   if (cmd_size != sizeof(info_buf) - 2*sizeof(uint32_t))
   {
      /* Either the host doesn't have anything loaded, or this is just screwy */
      if (cmd_size != 0)
      {
         /* Huh? */
         RARCH_ERR("Invalid NETPLAY_CMD_INFO payload size.\n");
         return false;
      }

      /* Send our info and hope they load it! */
      if (!netplay_handshake_info(netplay, connection))
         return false;

      *had_input = true;
      netplay_recv_flush(&connection->recv_packet_buffer);
      return true;
   }

   RECV(&info_buf.content_crc, cmd_size)
   {
      RARCH_ERR("Failed to receive netplay info payload.\n");
      return false;
   }

   /* Check the core info */
   if (system)
   {
      if (strncmp(info_buf.core_name,
               system->library_name, sizeof(info_buf.core_name)))
      {
         /* Wrong core! */
         dmsg = msg_hash_to_str(MSG_NETPLAY_DIFFERENT_CORES);
         RARCH_ERR("%s\n", dmsg);
         runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         /* FIXME: Should still send INFO, so the other side knows what's what */
         return false;
      }
      if (strncmp(info_buf.core_version,
             system->library_version, sizeof(info_buf.core_version)))
      {
         dmsg = msg_hash_to_str(MSG_NETPLAY_DIFFERENT_CORE_VERSIONS);
         RARCH_WARN("%s\n", dmsg);
         runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* Check the content CRC */
   content_crc = content_get_crc();

   if (content_crc != 0)
   {
      if (ntohl(info_buf.content_crc) != content_crc)
      {
         dmsg = msg_hash_to_str(MSG_CONTENT_CRC32S_DIFFER);
         RARCH_WARN("%s\n", dmsg);
         runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* Now switch to the right mode */
   if (netplay->is_server)
   {
      if (!netplay_handshake_sync(netplay, connection))
         return false;
   }
   else
   {
      if (!netplay_handshake_info(netplay, connection))
         return false;
      connection->mode = NETPLAY_CONNECTION_PRE_SYNC;
   }

   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;
}

/**
 * netplay_handshake_pre_sync
 *
 * Data receiver for the client's third handshake stage, receiving the
 * synchronization information.
 */
static bool netplay_handshake_pre_sync(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   uint32_t cmd[2];
   uint32_t new_frame_count, client_num;
   uint32_t device;
   uint32_t local_sram_size, remote_sram_size;
   size_t i, j;
   ssize_t recvd;
   retro_ctx_controller_info_t pad;
   char new_nick[NETPLAY_NICK_LEN];
   retro_ctx_memory_info_t mem_info;

   RECV(cmd, sizeof(cmd))
   {
      const char *msg = msg_hash_to_str(MSG_NETPLAY_INCORRECT_PASSWORD);
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return false;
   }

   /* Only expecting a sync command */
   if (ntohl(cmd[0]) != NETPLAY_CMD_SYNC ||
         ntohl(cmd[1]) < (2+2*MAX_INPUT_DEVICES)*sizeof(uint32_t) + (MAX_INPUT_DEVICES)*sizeof(uint8_t) +
         NETPLAY_NICK_LEN)
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
      return false;
   }

   /* Get the frame count */
   RECV(&new_frame_count, sizeof(new_frame_count))
      return false;
   new_frame_count = ntohl(new_frame_count);

   /* Get our client number and pause mode */
   RECV(&client_num, sizeof(client_num))
      return false;
   client_num = ntohl(client_num);
   if (client_num & NETPLAY_CMD_SYNC_BIT_PAUSED)
   {
      netplay->remote_paused = true;
      client_num ^= NETPLAY_CMD_SYNC_BIT_PAUSED;
   }
   netplay->self_client_num = client_num;

   /* Set our frame counters as requested */
   netplay->self_frame_count      = netplay->run_frame_count    =
      netplay->other_frame_count  = netplay->unread_frame_count =
      netplay->server_frame_count = new_frame_count;

   /* And clear out the framebuffer */
   for (i = 0; i < netplay->buffer_size; i++)
   {
      struct delta_frame *ptr = &netplay->buffer[i];

      ptr->used               = false;

      if (i == netplay->self_ptr)
      {
         /* Clear out any current data but still use this frame */
         if (!netplay_delta_frame_ready(netplay, ptr, 0))
            return false;

         ptr->frame       = new_frame_count;
         ptr->have_local  = true;
         netplay->run_ptr = netplay->other_ptr = netplay->unread_ptr =
            netplay->server_ptr = i;

      }
   }
   for (i = 0; i < MAX_CLIENTS; i++)
   {
      netplay->read_ptr[i]         = netplay->self_ptr;
      netplay->read_frame_count[i] = netplay->self_frame_count;
   }

   /* Get and set each input device */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      RECV(&device, sizeof(device))
         return false;

      pad.port   = (unsigned)i;
      pad.device = ntohl(device);
      netplay->config_devices[i] = pad.device;
      if ((pad.device&RETRO_DEVICE_MASK) == RETRO_DEVICE_KEYBOARD)
      {
         netplay->have_updown_device = true;
         netplay_key_hton_init();
      }

      core_set_controller_port_device(&pad);
   }

   /* Get the share modes */
   RECV(netplay->device_share_modes, sizeof(netplay->device_share_modes))
      return false;

   /* Get the client-controller mapping */
   netplay->connected_players =
         netplay->connected_slaves =
         netplay->self_devices = 0;
   for (i = 0; i < MAX_CLIENTS; i++)
      netplay->client_devices[i] = 0;
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      RECV(&device, sizeof(device))
         return false;
      device = ntohl(device);

      netplay->device_clients[i] = device;
      netplay->connected_players |= device;
      for (j = 0; j < MAX_CLIENTS; j++)
      {
         if (device & (1<<j))
            netplay->client_devices[j] |= 1<<i;
      }
   }

   /* Get our nick */
   RECV(new_nick, NETPLAY_NICK_LEN)
      return false;

   if (strncmp(netplay->nick, new_nick, NETPLAY_NICK_LEN))
   {
      char msg[512];
      strlcpy(netplay->nick, new_nick, NETPLAY_NICK_LEN);
      snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_NETPLAY_CHANGED_NICK), netplay->nick);
      RARCH_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* Now check the SRAM */
#ifdef HAVE_THREADS
   autosave_lock();
#endif
   mem_info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&mem_info);

   local_sram_size  = (unsigned)mem_info.size;
   remote_sram_size = ntohl(cmd[1]) -
         (2+2*MAX_INPUT_DEVICES)*sizeof(uint32_t) - (MAX_INPUT_DEVICES)*sizeof(uint8_t) - NETPLAY_NICK_LEN;

   if (local_sram_size != 0 && local_sram_size == remote_sram_size)
   {
      RECV(mem_info.data, local_sram_size)
      {
         RARCH_ERR("%s\n",
               msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
#ifdef HAVE_THREADS
         autosave_unlock();
#endif
         return false;
      }

   }
   else if (remote_sram_size != 0)
   {
      /* We can't load this, but we still need to get rid of the data */
      uint32_t quickbuf;
      while (remote_sram_size > 0)
      {
         RECV(&quickbuf, (remote_sram_size > sizeof(uint32_t)) ? sizeof(uint32_t) : remote_sram_size)
         {
            RARCH_ERR("%s\n",
                  msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
#ifdef HAVE_THREADS
            autosave_unlock();
#endif
            return false;
         }
         if (remote_sram_size > sizeof(uint32_t))
            remote_sram_size -= sizeof(uint32_t);
         else
            remote_sram_size = 0;
      }

   }
#ifdef HAVE_THREADS
   autosave_unlock();
#endif

   /* We're ready! */
   *had_input = true;
   netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
   connection->mode = NETPLAY_CONNECTION_PLAYING;
   netplay_handshake_ready(netplay, connection);
   netplay_recv_flush(&connection->recv_packet_buffer);

   /* Ask to switch to playing mode if we should */
   {
      settings_t *settings = config_get_ptr();
      if (!settings->bools.netplay_start_as_spectator)
         return netplay_cmd_mode(netplay, NETPLAY_CONNECTION_PLAYING);
   }

   return true;
}

/**
 * netplay_handshake
 *
 * Data receiver for all handshake states.
 */
bool netplay_handshake(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   bool ret = false;

   switch (connection->mode)
   {
      case NETPLAY_CONNECTION_INIT:
         ret = netplay_handshake_init(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_NICK:
         ret = netplay_handshake_pre_nick(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_PASSWORD:
         ret = netplay_handshake_pre_password(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_INFO:
         ret = netplay_handshake_pre_info(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_SYNC:
         ret = netplay_handshake_pre_sync(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_NONE:
      default:
         return false;
   }

   if (connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
         !netplay_send_cur_input(netplay, connection))
      return false;

   return ret;
}

/* The mapping of keys from libretro (host) to netplay (network) */
uint32_t netplay_key_hton(unsigned key)
{
   if (key >= RETROK_LAST)
      return NETPLAY_KEY_UNKNOWN;
   return netplay_mapping[key];
}

/* Because the hton keymapping has to be generated, call this before using
 * netplay_key_hton */
void netplay_key_hton_init(void)
{
   static bool mapping_defined = false;

   if (!mapping_defined)
   {
      uint16_t i;
      for (i = 0; i < NETPLAY_KEY_LAST; i++)
         netplay_mapping[NETPLAY_KEY_NTOH(i)] = i;
      mapping_defined = true;
   }
}

static void clear_input(netplay_input_state_t istate)
{
   while (istate)
   {
      istate->used = false;
      istate       = istate->next;
   }
}

/**
 * netplay_delta_frame_ready
 *
 * Prepares, if possible, a delta frame for input, and reports whether it is
 * ready.
 *
 * Returns: True if the delta frame is ready for input at the given frame,
 * false otherwise.
 */
bool netplay_delta_frame_ready(netplay_t *netplay, struct delta_frame *delta,
   uint32_t frame)
{
   size_t i;
   if (delta->used)
   {
      if (delta->frame == frame)
         return true;
      /* We haven't even replayed this frame yet,
       * so we can't overwrite it! */
      if (netplay->other_frame_count <= delta->frame)
         return false;
   }

   delta->used  = true;
   delta->frame = frame;
   delta->crc   = 0;

   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      clear_input(delta->resolved_input[i]);
      clear_input(delta->real_input[i]);
      clear_input(delta->simlated_input[i]);
   }
   delta->have_local = false;
   for (i = 0; i < MAX_CLIENTS; i++)
      delta->have_real[i] = false;
   return true;
}

/**
 * netplay_delta_frame_crc
 *
 * Get the CRC for the serialization of this frame.
 */
static uint32_t netplay_delta_frame_crc(netplay_t *netplay,
      struct delta_frame *delta)
{
   return encoding_crc32(0L, (const unsigned char*)delta->state,
         netplay->state_size);
}

/*
 * Free an input state list
 */
static void free_input_state(netplay_input_state_t *list)
{
   netplay_input_state_t cur, next;
   cur = *list;
   while (cur)
   {
      next = cur->next;
      free(cur);
      cur = next;
   }
   *list = NULL;
}

/**
 * netplay_delta_frame_free
 *
 * Free a delta frame's dependencies
 */
static void netplay_delta_frame_free(struct delta_frame *delta)
{
   uint32_t i;

   if (delta->state)
   {
      free(delta->state);
      delta->state = NULL;
   }

   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      free_input_state(&delta->resolved_input[i]);
      free_input_state(&delta->real_input[i]);
      free_input_state(&delta->simlated_input[i]);
   }
}

/**
 * netplay_input_state_for
 *
 * Get an input state for a particular client
 */
netplay_input_state_t netplay_input_state_for(
      netplay_input_state_t *list,
      uint32_t client_num, size_t size,
      bool must_create, bool must_not_create)
{
   netplay_input_state_t ret;
   while (*list)
   {
      ret = *list;
      if (!ret->used && !must_not_create && ret->size == size)
      {
         ret->client_num = client_num;
         ret->used       = true;
         memset(ret->data, 0, size*sizeof(uint32_t));
         return ret;
      }
      else if (ret->used && ret->client_num == client_num)
      {
         if (!must_create && ret->size == size)
            return ret;
         return NULL;
      }
      list = &(ret->next);
   }

   if (must_not_create)
      return NULL;

   /* Couldn't find a slot, allocate a fresh one */
   if (size > 1)
      ret = (netplay_input_state_t)calloc(1, sizeof(struct netplay_input_state) + (size-1) * sizeof(uint32_t));
	else
      ret = (netplay_input_state_t)calloc(1, sizeof(struct netplay_input_state));
   if (!ret)
      return NULL;
   *list           = ret;
   ret->client_num = client_num;
   ret->used       = true;
   ret->size       = (uint32_t)size;
   return ret;
}

/**
 * netplay_expected_input_size
 *
 * Size in words for a given set of devices.
 */
uint32_t netplay_expected_input_size(netplay_t *netplay, uint32_t devices)
{
   uint32_t ret = 0, device;

   for (device = 0; device < MAX_INPUT_DEVICES; device++)
   {
      if (!(devices & (1<<device)))
         continue;

      switch (netplay->config_devices[device]&RETRO_DEVICE_MASK)
      {
         /* These are all essentially magic numbers, but each device has a
          * fixed size, documented in network/netplay/README */
         case RETRO_DEVICE_JOYPAD:
            ret += 1;
            break;
         case RETRO_DEVICE_MOUSE:
            ret += 2;
            break;
         case RETRO_DEVICE_KEYBOARD:
            ret += 5;
            break;
         case RETRO_DEVICE_LIGHTGUN:
            ret += 2;
            break;
         case RETRO_DEVICE_ANALOG:
            ret += 3;
            break;
         default:
            break; /* Unsupported */
      }
   }

   return ret;
}

static size_t buf_used(struct socket_buffer *sbuf)
{
   if (sbuf->end < sbuf->start)
   {
      size_t newend = sbuf->end;
      while (newend < sbuf->start)
         newend += sbuf->bufsz;
      return newend - sbuf->start;
   }

   return sbuf->end - sbuf->start;
}

static size_t buf_unread(struct socket_buffer *sbuf)
{
   if (sbuf->end < sbuf->read)
   {
      size_t newend = sbuf->end;
      while (newend < sbuf->read)
         newend += sbuf->bufsz;
      return newend - sbuf->read;
   }

   return sbuf->end - sbuf->read;
}

static size_t buf_remaining(struct socket_buffer *sbuf)
{
   return sbuf->bufsz - buf_used(sbuf) - 1;
}

/**
 * netplay_init_socket_buffer
 *
 * Initialize a new socket buffer.
 */
static bool netplay_init_socket_buffer(
      struct socket_buffer *sbuf, size_t size)
{
   sbuf->data = (unsigned char*)malloc(size);
   if (!sbuf->data)
      return false;
   sbuf->bufsz = size;
   sbuf->start = sbuf->read = sbuf->end = 0;
   return true;
}

/**
 * netplay_resize_socket_buffer
 *
 * Resize the given socket_buffer's buffer to the requested size.
 */
static bool netplay_resize_socket_buffer(
      struct socket_buffer *sbuf, size_t newsize)
{
   unsigned char *newdata = (unsigned char*)malloc(newsize);
   if (!newdata)
      return false;

    /* Copy in the old data */
    if (sbuf->end < sbuf->start)
    {
       memcpy(newdata,
             sbuf->data + sbuf->start,
             sbuf->bufsz - sbuf->start);
       memcpy(newdata + sbuf->bufsz - sbuf->start,
             sbuf->data,
             sbuf->end);
    }
    else if (sbuf->end > sbuf->start)
       memcpy(newdata,
             sbuf->data + sbuf->start,
             sbuf->end - sbuf->start);

    /* Adjust our read offset */
    if (sbuf->read < sbuf->start)
       sbuf->read += sbuf->bufsz - sbuf->start;
    else
       sbuf->read -= sbuf->start;

    /* Adjust start and end */
    sbuf->end      = buf_used(sbuf);
    sbuf->start    = 0;

    /* Free the old one and replace it with the new one */
    free(sbuf->data);
    sbuf->data     = newdata;
    sbuf->bufsz    = newsize;

    return true;
}

#if 0
static void netplay_clear_socket_buffer(struct socket_buffer *sbuf)
{
   sbuf->start = sbuf->read = sbuf->end = 0;
}
#endif

/**
 * netplay_send
 *
 * Queue the given data for sending.
 */
bool netplay_send(
      struct socket_buffer *sbuf,
      int sockfd, const void *buf,
      size_t len)
{
   if (buf_remaining(sbuf) < len)
   {
      /* Need to force a blocking send */
      if (!netplay_send_flush(sbuf, sockfd, true))
         return false;
   }

   if (buf_remaining(sbuf) < len)
   {
      /* Can only be that this is simply too big 
       * for our buffer, in which case we just 
       * need to do a blocking send */
      if (!socket_send_all_blocking(sockfd, buf, len, false))
         return false;
      return true;
   }

   /* Copy it into our buffer */
   if (sbuf->bufsz - sbuf->end < len)
   {
      /* Half at a time */
      size_t chunka = sbuf->bufsz - sbuf->end,
             chunkb = len - chunka;
      memcpy(sbuf->data + sbuf->end, buf, chunka);
      memcpy(sbuf->data, (const unsigned char *)buf + chunka, chunkb);
      sbuf->end = chunkb;

   }
   else
   {
      /* Straight in */
      memcpy(sbuf->data + sbuf->end, buf, len);
      sbuf->end += len;
   }

   return true;
}

/**
 * netplay_send_flush
 *
 * Flush unsent data in the given socket buffer, blocking to do so if
 * requested.
 *
 * Returns false only on socket failures, true otherwise.
 */
bool netplay_send_flush(struct socket_buffer *sbuf, int sockfd, bool block)
{
   ssize_t sent;

   if (buf_used(sbuf) == 0)
      return true;

   if (sbuf->end > sbuf->start)
   {
      /* Usual case: Everything's in order */
      if (block)
      {
         if (!socket_send_all_blocking(
                  sockfd, sbuf->data + sbuf->start,
                  buf_used(sbuf), true))
            return false;

         sbuf->start = sbuf->end = 0;
      }
      else
      {
         sent = socket_send_all_nonblocking(
               sockfd, sbuf->data + sbuf->start,
               buf_used(sbuf), true);

         if (sent < 0)
            return false;

         sbuf->start += sent;

         if (sbuf->start == sbuf->end)
            sbuf->start = sbuf->end = 0;
      }
   }
   else
   {
      /* Unusual case: Buffer overlaps break */
      if (block)
      {
         if (!socket_send_all_blocking(
                  sockfd, sbuf->data + sbuf->start,
                  sbuf->bufsz - sbuf->start, true))
            return false;

         sbuf->start = 0;

         return netplay_send_flush(sbuf, sockfd, true);
      }
      else
      {
         sent = socket_send_all_nonblocking(
               sockfd, sbuf->data + sbuf->start,
               sbuf->bufsz - sbuf->start, true);

         if (sent < 0)
            return false;

         sbuf->start += sent;

         if (sbuf->start >= sbuf->bufsz)
         {
            sbuf->start = 0;
            return netplay_send_flush(sbuf, sockfd, false);
         }
      }

   }

   return true;
}

/**
 * netplay_recv
 *
 * Receive buffered or fresh data.
 *
 * Returns number of bytes returned, which may be short or 0, or -1 on error.
 */
ssize_t netplay_recv(struct socket_buffer *sbuf, int sockfd, void *buf,
   size_t len, bool block)
{
   ssize_t recvd;
   bool error    = false;

   /* Receive whatever we can into the buffer */
   if (sbuf->end >= sbuf->start)
   {
      recvd = socket_receive_all_nonblocking(sockfd, &error,
         sbuf->data + sbuf->end, sbuf->bufsz - sbuf->end -
         ((sbuf->start == 0) ? 1 : 0));

      if (recvd < 0 || error)
         return -1;

      sbuf->end += recvd;

      if (sbuf->end >= sbuf->bufsz)
      {
         sbuf->end = 0;
         error     = false;
         recvd     = socket_receive_all_nonblocking(
               sockfd, &error, sbuf->data, sbuf->start - 1);

         if (recvd < 0 || error)
            return -1;

         sbuf->end += recvd;
      }
   }
   else
   {
      recvd = socket_receive_all_nonblocking(
            sockfd, &error, sbuf->data + sbuf->end,
            sbuf->start - sbuf->end - 1);

      if (recvd < 0 || error)
         return -1;

      sbuf->end += recvd;
   }

   /* Now copy it into the reader */
   if (sbuf->end >= sbuf->read || (sbuf->bufsz - sbuf->read) >= len)
   {
      size_t unread = buf_unread(sbuf);
      if (len <= unread)
      {
         memcpy(buf, sbuf->data + sbuf->read, len);
         sbuf->read += len;
         if (sbuf->read >= sbuf->bufsz)
            sbuf->read = 0;
         recvd = len;

      }
      else
      {
         memcpy(buf, sbuf->data + sbuf->read, unread);
         sbuf->read += unread;
         if (sbuf->read >= sbuf->bufsz)
            sbuf->read = 0;
         recvd = unread;
      }
   }
   else
   {
      /* Our read goes around the edge */
      size_t chunka = sbuf->bufsz - sbuf->read,
             pchunklen = len - chunka,
             chunkb = (pchunklen >= sbuf->end) ? sbuf->end : pchunklen;
      memcpy(buf, sbuf->data + sbuf->read, chunka);
      memcpy((unsigned char *) buf + chunka, sbuf->data, chunkb);
      sbuf->read = chunkb;
      recvd      = chunka + chunkb;
   }

   /* Perhaps block for more data */
   if (block)
   {
      sbuf->start = sbuf->read;
      if (recvd < 0 || recvd < (ssize_t) len)
      {
         if (!socket_receive_all_blocking(
                  sockfd, (unsigned char *)buf + recvd, len - recvd))
            return -1;
         recvd = len;
      }
   }

   return recvd;
}

/**
 * netplay_recv_reset
 *
 * Reset our recv buffer so that future netplay_recvs 
 * will read the same data again.
 */
void netplay_recv_reset(struct socket_buffer *sbuf)
{
   sbuf->read = sbuf->start;
}

/**
 * netplay_recv_flush
 *
 * Flush our recv buffer, so a future netplay_recv_reset will reset to this
 * point.
 */
void netplay_recv_flush(struct socket_buffer *sbuf)
{
   sbuf->start = sbuf->read;
}

/**
 * netplay_cmd_crc
 *
 * Send a CRC command to all active clients.
 */
static bool netplay_cmd_crc(netplay_t *netplay, struct delta_frame *delta)
{
   size_t i;
   uint32_t payload[2];
   bool success = true;

   payload[0]   = htonl(delta->frame);
   payload[1]   = htonl(delta->crc);

   for (i = 0; i < netplay->connections_size; i++)
   {
      if (netplay->connections[i].active &&
            netplay->connections[i].mode >= NETPLAY_CONNECTION_CONNECTED)
         success = netplay_send_raw_cmd(netplay, &netplay->connections[i],
            NETPLAY_CMD_CRC, payload, sizeof(payload)) && success;
   }
   return success;
}

/**
 * netplay_cmd_request_savestate
 *
 * Send a savestate request command.
 */
static bool netplay_cmd_request_savestate(netplay_t *netplay)
{
   if (netplay->connections_size == 0 ||
       !netplay->connections[0].active ||
       netplay->connections[0].mode < NETPLAY_CONNECTION_CONNECTED)
      return false;
   if (netplay->savestate_request_outstanding)
      return true;
   netplay->savestate_request_outstanding = true;
   return netplay_send_raw_cmd(netplay, &netplay->connections[0],
      NETPLAY_CMD_REQUEST_SAVESTATE, NULL, 0);
}

/**
 * netplay_cmd_stall
 *
 * Send a stall command.
 */
static bool netplay_cmd_stall(netplay_t *netplay,
   struct netplay_connection *connection,
   uint32_t frames)
{
   frames = htonl(frames);
   return netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_STALL, &frames, sizeof(frames));
}



static void handle_play_spectate(netplay_t *netplay, uint32_t client_num,
      struct netplay_connection *connection, uint32_t cmd, uint32_t cmd_size,
      uint32_t *payload);

#if 0
#define DEBUG_NONDETERMINISTIC_CORES
#endif

/**
 * netplay_update_unread_ptr
 *
 * Update the global unread_ptr and unread_frame_count to correspond to the
 * earliest unread frame count of any connected player
 */
void netplay_update_unread_ptr(netplay_t *netplay)
{
   if (netplay->is_server && netplay->connected_players<=1)
   {
      /* Nothing at all to read! */
      netplay->unread_ptr         = netplay->self_ptr;
      netplay->unread_frame_count = netplay->self_frame_count;

   }
   else
   {
      size_t           new_unread_ptr = 0;
      uint32_t new_unread_frame_count = (uint32_t) -1;
      uint32_t client;

      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(netplay->connected_players & (1 << client)))
            continue;
         if ((netplay->connected_slaves   & (1 << client)))
            continue;

         if (netplay->read_frame_count[client] < new_unread_frame_count)
         {
            new_unread_ptr         = netplay->read_ptr[client];
            new_unread_frame_count = netplay->read_frame_count[client];
         }
      }

      if ( !netplay->is_server && 
            netplay->server_frame_count < new_unread_frame_count)
      {
         new_unread_ptr              = netplay->server_ptr;
         new_unread_frame_count      = netplay->server_frame_count;
      }

      if (new_unread_frame_count != (uint32_t) -1)
      {
         netplay->unread_ptr         = new_unread_ptr;
         netplay->unread_frame_count = new_unread_frame_count;
      }
      else
      {
         netplay->unread_ptr         = netplay->self_ptr;
         netplay->unread_frame_count = netplay->self_frame_count;
      }
   }
}

/**
 * netplay_device_client_state
 * @netplay             : pointer to netplay object
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @client              : client to find state for
 */
netplay_input_state_t netplay_device_client_state(netplay_t *netplay,
      struct delta_frame *simframe, uint32_t device, uint32_t client)
{
   uint32_t                 dsize = 
      netplay_expected_input_size(netplay, 1 << device);
   netplay_input_state_t simstate =
      netplay_input_state_for(
         &simframe->real_input[device], client,
         dsize, false, true);

   if (!simstate)
   {
      if (netplay->read_frame_count[client] > simframe->frame)
         return NULL;
      simstate = netplay_input_state_for(&simframe->simlated_input[device],
            client, dsize, false, true);
   }
   return simstate;
}

/**
 * netplay_merge_digital
 * @netplay             : pointer to netplay object
 * @resstate            : state being resolved
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @clients             : bitmap of clients being merged
 * @digital             : bitmap of digital bits
 */
static void netplay_merge_digital(netplay_t *netplay,
      netplay_input_state_t resstate, struct delta_frame *simframe,
      uint32_t device, uint32_t clients, const uint32_t *digital)
{
   netplay_input_state_t simstate;
   uint32_t word, bit, client;
   uint8_t share_mode = netplay->device_share_modes[device]
      & NETPLAY_SHARE_DIGITAL_BITS;

   /* Make sure all real clients are accounted for */
   for (simstate = simframe->real_input[device];
         simstate; simstate = simstate->next)
   {
      if (!simstate->used || simstate->size != resstate->size)
         continue;
      clients |= 1 << simstate->client_num;
   }

   if (share_mode == NETPLAY_SHARE_DIGITAL_VOTE)
   {
      unsigned i, j;
      /* This just assumes we have no more than
       * three words, will need to be adjusted for new devices */
      struct vote_count votes[3];
      /* Vote mode requires counting all the bits */
      uint32_t client_count      = 0;

      for (i = 0; i < 3; i++)
         for (j = 0; j < 32; j++)
            votes[i].votes[j] = 0;

      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(clients & (1 << client)))
            continue;

         simstate = netplay_device_client_state(
               netplay, simframe, device, client);

         if (!simstate)
            continue;
         client_count++;

         for (word = 0; word < resstate->size; word++)
         {
            if (!digital[word])
               continue;
            for (bit = 0; bit < 32; bit++)
            {
               if (!(digital[word] & (1 << bit)))
                  continue;
               if (simstate->data[word] & (1 << bit))
                  votes[word].votes[bit]++;
            }
         }
      }

      /* Now count all the bits */
      client_count /= 2;
      for (word = 0; word < resstate->size; word++)
      {
         for (bit = 0; bit < 32; bit++)
         {
            if (votes[word].votes[bit] > client_count)
               resstate->data[word] |= (1 << bit);
         }
      }
   }
   else /* !VOTE */
   {
      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(clients & (1 << client)))
            continue;
         simstate = netplay_device_client_state(
               netplay, simframe, device, client);

         if (!simstate)
            continue;
         for (word = 0; word < resstate->size; word++)
         {
            uint32_t part;
            if (!digital[word])
               continue;
            part = simstate->data[word];

            if (digital[word] == (uint32_t) -1)
            {
               /* Combine the whole word */
               switch (share_mode)
               {
                  case NETPLAY_SHARE_DIGITAL_XOR:
                     resstate->data[word] ^= part;
                     break;
                  default:
                     resstate->data[word] |= part;
               }

            }
            else /* !whole word */
            {
               for (bit = 0; bit < 32; bit++)
               {
                  if (!(digital[word] & (1 << bit)))
                     continue;
                  switch (share_mode)
                  {
                     case NETPLAY_SHARE_DIGITAL_XOR:
                        resstate->data[word] ^= part & (1 << bit);
                        break;
                     default:
                        resstate->data[word] |= part & (1 << bit);
                  }
               }
            }
         }
      }

   }
}

/**
 * merge_analog_part
 * @netplay             : pointer to netplay object
 * @resstate            : state being resolved
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @clients             : bitmap of clients being merged
 * @word                : word to merge
 * @bit                 : first bit to merge
 */
static void merge_analog_part(netplay_t *netplay,
      netplay_input_state_t resstate, struct delta_frame *simframe,
      uint32_t device, uint32_t clients, uint32_t word, uint8_t bit)
{
   netplay_input_state_t simstate;
   uint32_t client, client_count = 0;
   uint8_t share_mode            = netplay->device_share_modes[device]
      & NETPLAY_SHARE_ANALOG_BITS;
   int32_t value                 = 0, new_value;

   /* Make sure all real clients are accounted for */
   for (simstate = simframe->real_input[device]; simstate; simstate = simstate->next)
   {
      if (!simstate->used || simstate->size != resstate->size)
         continue;
      clients |= 1 << simstate->client_num;
   }

   for (client = 0; client < MAX_CLIENTS; client++)
   {
      if (!(clients & (1 << client)))
         continue;
      simstate = netplay_device_client_state(
            netplay, simframe, device, client);
      if (!simstate)
         continue;
      client_count++;
      new_value = (int16_t) ((simstate->data[word]>>bit) & 0xFFFF);
      switch (share_mode)
      {
         case NETPLAY_SHARE_ANALOG_AVERAGE:
            value += (int32_t) new_value;
            break;
         default:
            if (abs(new_value) > abs(value) ||
                (abs(new_value) == abs(value) && new_value > value))
               value = new_value;
      }
   }

   if (share_mode == NETPLAY_SHARE_ANALOG_AVERAGE)
      if (client_count > 0) /* Prevent potential divide by zero */
         value /= client_count;

   resstate->data[word] |= ((uint32_t) (uint16_t) value) << bit;
}

/**
 * netplay_merge_analog
 * @netplay             : pointer to netplay object
 * @resstate            : state being resolved
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @clients             : bitmap of clients being merged
 * @dtype               : device type
 */
static void netplay_merge_analog(netplay_t *netplay,
      netplay_input_state_t resstate, struct delta_frame *simframe,
      uint32_t device, uint32_t clients, unsigned dtype)
{
   /* Devices with no analog parts */
   if (dtype == RETRO_DEVICE_JOYPAD || dtype == RETRO_DEVICE_KEYBOARD)
      return;

   /* All other devices have at least one analog word */
   merge_analog_part(netplay, resstate, simframe, device, clients, 1, 0);
   merge_analog_part(netplay, resstate, simframe, device, clients, 1, 16);

   /* And the ANALOG device has two (two sticks) */
   if (dtype == RETRO_DEVICE_ANALOG)
   {
      merge_analog_part(netplay, resstate, simframe, device, clients, 2, 0);
      merge_analog_part(netplay, resstate, simframe, device, clients, 2, 16);
   }
}

/**
 * netplay_resolve_input
 * @netplay             : pointer to netplay object
 * @sim_ptr             : frame pointer for which to resolve input
 * @resim               : are we resimulating, or simulating this frame for the
 *                        first time?
 *
 * "Simulate" input by assuming it hasn't changed since the last read input.
 * Returns true if the resolved input changed from the last time it was
 * resolved.
 */
bool netplay_resolve_input(netplay_t *netplay, size_t sim_ptr, bool resim)
{
   size_t prev;
   uint32_t device;
   uint32_t clients, client, client_count;
   netplay_input_state_t simstate, client_state = NULL,
                         resstate, oldresstate, pstate;
   bool ret                     = false;
   struct delta_frame *pframe   = NULL;
   struct delta_frame *simframe = &netplay->buffer[sim_ptr];

   for (device = 0; device < MAX_INPUT_DEVICES; device++)
   {
      unsigned dtype = netplay->config_devices[device]&RETRO_DEVICE_MASK;
      uint32_t dsize = netplay_expected_input_size(netplay, 1 << device);
      clients        = netplay->device_clients[device];
      client_count   = 0;

      /* Make sure all real clients are accounted for */
      for (simstate = simframe->real_input[device]; simstate; simstate = simstate->next)
      {
         if (!simstate->used || simstate->size != dsize)
            continue;
         clients |= 1 << simstate->client_num;
      }

      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(clients & (1 << client)))
            continue;

         /* Resolve this client-device */
         simstate = netplay_input_state_for(
               &simframe->real_input[device], client, dsize, false, true);
         if (!simstate)
         {
            /* Don't already have this input, so must
             * simulate if we're supposed to have it at all */
            if (netplay->read_frame_count[client] > simframe->frame)
               continue;
            simstate = netplay_input_state_for(&simframe->simlated_input[device], client, dsize, false, false);
            if (!simstate)
               continue;

            prev = PREV_PTR(netplay->read_ptr[client]);
            pframe = &netplay->buffer[prev];
            pstate = netplay_input_state_for(&pframe->real_input[device], client, dsize, false, true);
            if (!pstate)
               continue;

            if (resim && (dtype == RETRO_DEVICE_JOYPAD || dtype == RETRO_DEVICE_ANALOG))
            {
               /* In resimulation mode, we only copy the buttons. The reason for this
                * is nonobvious:
                *
                * If we resimulated nothing, then the /duration/ with which any input
                * was pressed would be approximately correct, since the original
                * simulation came in as the input came in, but the /number of times/
                * the input was pressed would be wrong, as there would be an
                * advancing wavefront of real data overtaking the simulated data
                * (which is really just real data offset by some frames).
                *
                * That's acceptable for arrows in most situations, since the amount
                * you move is tied to the duration, but unacceptable for buttons,
                * which will seem to jerkily be pressed numerous times with those
                * wavefronts.
                */
               const uint32_t keep =
                  (UINT32_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) |
                  (UINT32_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) |
                  (UINT32_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) |
                  (UINT32_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT);
               simstate->data[0] &= keep;
               simstate->data[0] |= pstate->data[0] & ~keep;
            }
            else
               memcpy(simstate->data, pstate->data,
                     dsize * sizeof(uint32_t));
         }

         client_state = simstate;
         client_count++;
      }

      /* The frontend always uses the first resolved input,
       * so make sure it's right */
      while (simframe->resolved_input[device]
            && (simframe->resolved_input[device]->size != dsize
                  || simframe->resolved_input[device]->client_num != 0))
      {
         /* The default resolved input is of the wrong size! */
         netplay_input_state_t nextistate =
            simframe->resolved_input[device]->next;
         free(simframe->resolved_input[device]);
         simframe->resolved_input[device] = nextistate;
      }

      /* Now we copy the state, whether real or simulated,
       * out into the resolved state */
      resstate = netplay_input_state_for(
            &simframe->resolved_input[device], 0,
            dsize, false, false);
      if (!resstate)
         continue;

      if (client_count == 1)
      {
         /* Trivial in the common 1-client case */
         if (memcmp(resstate->data, client_state->data,
                  dsize * sizeof(uint32_t)))
            ret = true;
         memcpy(resstate->data, client_state->data,
               dsize * sizeof(uint32_t));

      }
      else if (client_count == 0)
      {
         uint32_t word;
         for (word = 0; word < dsize; word++)
         {
            if (resstate->data[word])
               ret = true;
            resstate->data[word] = 0;
         }

      }
      else
      {
         /* Merge them */
         /* Most devices have all the digital parts in the first word. */
         static const uint32_t digital_common[3]   = {~0u, 0u, 0u};
         static const uint32_t digital_keyboard[5] = {~0u, ~0u, ~0u, ~0u, ~0u};
         const uint32_t *digital                   = digital_common;

         if (dtype == RETRO_DEVICE_KEYBOARD)
            digital = digital_keyboard;

         oldresstate = netplay_input_state_for(
               &simframe->resolved_input[device], 1, dsize, false, false);

         if (!oldresstate)
            continue;
         memcpy(oldresstate->data, resstate->data, dsize * sizeof(uint32_t));
         memset(resstate->data, 0, dsize * sizeof(uint32_t));

         netplay_merge_digital(netplay, resstate, simframe,
               device, clients, digital);
         netplay_merge_analog(netplay, resstate, simframe,
               device, clients, dtype);

         if (memcmp(resstate->data, oldresstate->data,
                  dsize * sizeof(uint32_t)))
            ret = true;

      }
   }

   return ret;
}

static void netplay_handle_frame_hash(netplay_t *netplay,
      struct delta_frame *delta)
{
   if (netplay->is_server)
   {
      if (netplay->check_frames &&
          delta->frame % abs(netplay->check_frames) == 0)
      {
         if (netplay->state_size)
            delta->crc = netplay_delta_frame_crc(netplay, delta);
         else
            delta->crc = 0;
         netplay_cmd_crc(netplay, delta);
      }
   }
   else if (delta->crc && netplay->crcs_valid)
   {
      /* We have a remote CRC, so check it */
      uint32_t local_crc = 0;
      if (netplay->state_size)
         local_crc = netplay_delta_frame_crc(netplay, delta);

      if (local_crc != delta->crc)
      {
         /* If the very first check frame is wrong,
          * they probably just don't work */
         if (!netplay->crc_validity_checked)
            netplay->crcs_valid = false;
         else if (netplay->crcs_valid)
         {
            /* Fix this! */
            if (netplay->check_frames < 0)
            {
               /* Just report */
               RARCH_ERR("Netplay CRCs mismatch!\n");
            }
            else
               netplay_cmd_request_savestate(netplay);
         }
      }
      else if (!netplay->crc_validity_checked)
         netplay->crc_validity_checked = true;
   }
}

/**
 * netplay_sync_pre_frame
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay synchronization.
 */
bool netplay_sync_pre_frame(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   if (netplay_delta_frame_ready(netplay,
            &netplay->buffer[netplay->run_ptr], netplay->run_frame_count))
   {
      serial_info.data_const = NULL;
      serial_info.data       = netplay->buffer[netplay->run_ptr].state;
      serial_info.size       = netplay->state_size;

      memset(serial_info.data, 0, serial_info.size);
      if ((netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            || netplay->run_frame_count == 0)
      {
         /* Don't serialize until it's safe */
      }
      else if (!(netplay->quirks & NETPLAY_QUIRK_NO_SAVESTATES)
            && core_serialize(&serial_info))
      {
         if (netplay->force_send_savestate && !netplay->stall
               && !netplay->remote_paused)
         {
            /* Bring our running frame and input frames into
             * parity so we don't send old info. */
            if (netplay->run_ptr != netplay->self_ptr)
            {
               memcpy(netplay->buffer[netplay->self_ptr].state,
                  netplay->buffer[netplay->run_ptr].state,
                  netplay->state_size);
               netplay->run_ptr         = netplay->self_ptr;
               netplay->run_frame_count = netplay->self_frame_count;
            }

            /* Send this along to the other side */
            serial_info.data_const = netplay->buffer[netplay->run_ptr].state;
            netplay_load_savestate(netplay, &serial_info, false);
            netplay->force_send_savestate = false;
         }
      }
      else
      {
         /* If the core can't serialize properly, we must stall for the
          * remote input on EVERY frame, because we can't recover */
         netplay->quirks |= NETPLAY_QUIRK_NO_SAVESTATES;
         netplay->stateless_mode = true;
      }

      /* If we can't transmit savestates, we must stall
       * until the client is ready. */
      if (netplay->run_frame_count > 0 &&
          (netplay->quirks & (NETPLAY_QUIRK_NO_SAVESTATES|NETPLAY_QUIRK_NO_TRANSMISSION)) &&
          (netplay->connections_size == 0 || !netplay->connections[0].active ||
           netplay->connections[0].mode < NETPLAY_CONNECTION_CONNECTED))
         netplay->stall = NETPLAY_STALL_NO_CONNECTION;
   }

   if (netplay->is_server)
   {
      fd_set fds;
      struct timeval tmp_tv = {0};
      int new_fd;
      struct sockaddr_storage their_addr;
      socklen_t addr_size;
      struct netplay_connection *connection;
      size_t connection_num;

      /* Check for a connection */
      FD_ZERO(&fds);
      FD_SET(netplay->listen_fd, &fds);
      if (socket_select(netplay->listen_fd + 1,
               &fds, NULL, NULL, &tmp_tv) > 0 &&
          FD_ISSET(netplay->listen_fd, &fds))
      {
         addr_size = sizeof(their_addr);
         new_fd = accept(netplay->listen_fd,
               (struct sockaddr*)&their_addr, &addr_size);

         if (new_fd < 0)
         {
            RARCH_ERR("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));
            goto process;
         }

         /* Set the socket nonblocking */
         if (!socket_nonblock(new_fd))
         {
            /* Catastrophe! */
            socket_close(new_fd);
            goto process;
         }

#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
         {
            int flag = 1;
            if (setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY,
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
         if (fcntl(new_fd, F_SETFD, FD_CLOEXEC) < 0)
            RARCH_WARN("Cannot set Netplay port to close-on-exec. It may fail to reopen if the client disconnects.\n");
#endif

         /* Allocate a connection */
         for (connection_num = 0; connection_num < netplay->connections_size; connection_num++)
            if (!netplay->connections[connection_num].active &&
                  netplay->connections[connection_num].mode != NETPLAY_CONNECTION_DELAYED_DISCONNECT)
               break;
         if (connection_num == netplay->connections_size)
         {
            if (connection_num == 0)
            {
               netplay->connections = (struct netplay_connection*)
                  malloc(sizeof(struct netplay_connection));

               if (!netplay->connections)
               {
                  socket_close(new_fd);
                  goto process;
               }
               netplay->connections_size = 1;

            }
            else
            {
               size_t new_connections_size = netplay->connections_size * 2;
               struct netplay_connection
                  *new_connections         = (struct netplay_connection*)

                  realloc(netplay->connections,
                     new_connections_size*sizeof(struct netplay_connection));

               if (!new_connections)
               {
                  socket_close(new_fd);
                  goto process;
               }

               memset(new_connections + netplay->connections_size, 0,
                  netplay->connections_size * sizeof(struct netplay_connection));
               netplay->connections = new_connections;
               netplay->connections_size = new_connections_size;

            }
         }
         connection         = &netplay->connections[connection_num];

         /* Set it up */
         memset(connection, 0, sizeof(*connection));
         connection->active = true;
         connection->fd     = new_fd;
         connection->mode   = NETPLAY_CONNECTION_INIT;

         if (!netplay_init_socket_buffer(&connection->send_packet_buffer,
               netplay->packet_buffer_size) ||
             !netplay_init_socket_buffer(&connection->recv_packet_buffer,
               netplay->packet_buffer_size))
         {
            if (connection->send_packet_buffer.data)
               netplay_deinit_socket_buffer(&connection->send_packet_buffer);
            connection->active = false;
            socket_close(new_fd);
            goto process;
         }

         netplay_handshake_init_send(netplay, connection);

      }
   }

process:
   netplay->can_poll = true;
   input_poll_net();

   return (netplay->stall != NETPLAY_STALL_NO_CONNECTION);
}

/**
 * netplay_sync_post_frame
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay synchronization.
 * We check if we have new input and replay from recorded input.
 */
void netplay_sync_post_frame(netplay_t *netplay, bool stalled)
{
   uint32_t lo_frame_count, hi_frame_count;

   /* Unless we're stalling, we've just finished running a frame */
   if (!stalled)
   {
      netplay->run_ptr = NEXT_PTR(netplay->run_ptr);
      netplay->run_frame_count++;
   }

   /* We've finished an input frame even if we're stalling */
   if ((!stalled || netplay->stall == NETPLAY_STALL_INPUT_LATENCY) &&
       netplay->self_frame_count <
       netplay->run_frame_count + netplay->input_latency_frames)
   {
      netplay->self_ptr = NEXT_PTR(netplay->self_ptr);
      netplay->self_frame_count++;
   }

   /* Only relevant if we're connected and not in a desynching operation */
   if ((netplay->is_server && (netplay->connected_players<=1)) ||
       (netplay->self_mode < NETPLAY_CONNECTION_CONNECTED)     ||
       (netplay->desync))
   {
      netplay->other_frame_count = netplay->self_frame_count;
      netplay->other_ptr         = netplay->self_ptr;

      /* FIXME: Duplication */
      if (netplay->catch_up)
      {
         netplay->catch_up = false;
         input_state_get_ptr()->nonblocking_flag = false;
         driver_set_nonblock_state();
      }
      return;
   }

   /* Reset if it was requested */
   if (netplay->force_reset)
   {
      core_reset();
      netplay->force_reset = false;
   }

   netplay->replay_ptr = netplay->other_ptr;
   netplay->replay_frame_count = netplay->other_frame_count;

#ifndef DEBUG_NONDETERMINISTIC_CORES
   if (!netplay->force_rewind)
   {
      bool cont = true;

      /* Skip ahead if we predicted correctly.
       * Skip until our simulation failed. */
      while (netplay->other_frame_count < netplay->unread_frame_count &&
             netplay->other_frame_count < netplay->run_frame_count)
      {
         struct delta_frame *ptr = &netplay->buffer[netplay->other_ptr];

         /* If resolving the input changes it, we used bad input */
         if (netplay_resolve_input(netplay, netplay->other_ptr, true))
         {
            cont = false;
            break;
         }

         netplay_handle_frame_hash(netplay, ptr);
         netplay->other_ptr = NEXT_PTR(netplay->other_ptr);
         netplay->other_frame_count++;
      }
      netplay->replay_ptr = netplay->other_ptr;
      netplay->replay_frame_count = netplay->other_frame_count;

      if (cont)
      {
         while (netplay->replay_frame_count < netplay->run_frame_count)
         {
            if (netplay_resolve_input(netplay, netplay->replay_ptr, true))
               break;
            netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
            netplay->replay_frame_count++;
         }
      }
   }
#endif

   /* Now replay the real input if we've gotten ahead of it */
   if (netplay->force_rewind ||
       netplay->replay_frame_count < netplay->run_frame_count)
   {
      retro_ctx_serialize_info_t serial_info;

      /* Replay frames. */
      netplay->is_replay = true;

      /* If we have a keyboard device, we replay the previous frame's input
       * just to assert that the keydown/keyup events work if the core
       * translates them in that way */
      if (netplay->have_updown_device)
      {
         netplay->replay_ptr = PREV_PTR(netplay->replay_ptr);
         netplay->replay_frame_count--;
#ifdef HAVE_THREADS
         autosave_lock();
#endif
         core_run();
#ifdef HAVE_THREADS
         autosave_unlock();
#endif
         netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
         netplay->replay_frame_count++;
      }

      if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
         /* Make sure we're initialized before we start loading things */
         netplay_wait_and_init_serialization(netplay);

      serial_info.data       = NULL;
      serial_info.data_const = netplay->buffer[netplay->replay_ptr].state;
      serial_info.size       = netplay->state_size;

      if (!core_unserialize(&serial_info))
      {
         RARCH_ERR("Netplay savestate loading failed: Prepare for desync!\n");
      }

      while (netplay->replay_frame_count < netplay->run_frame_count)
      {
         retro_time_t start, tm;
         struct delta_frame *ptr = &netplay->buffer[netplay->replay_ptr];

         serial_info.data        = ptr->state;
         serial_info.size        = netplay->state_size;
         serial_info.data_const  = NULL;

         start                   = cpu_features_get_time_usec();

         /* Remember the current state */
         memset(serial_info.data, 0, serial_info.size);
         core_serialize(&serial_info);
         if (netplay->replay_frame_count < netplay->unread_frame_count)
            netplay_handle_frame_hash(netplay, ptr);

         /* Re-simulate this frame's input */
         netplay_resolve_input(netplay, netplay->replay_ptr, true);

#ifdef HAVE_THREADS
         autosave_lock();
#endif
         core_run();
#ifdef HAVE_THREADS
         autosave_unlock();
#endif
         netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
         netplay->replay_frame_count++;

#ifdef DEBUG_NONDETERMINISTIC_CORES
         if (ptr->have_remote && netplay_delta_frame_ready(netplay, &netplay->buffer[netplay->replay_ptr], netplay->replay_frame_count))
         {
            RARCH_LOG("PRE  %u: %X\n", netplay->replay_frame_count-1, netplay->state_size ? netplay_delta_frame_crc(netplay, ptr) : 0);
            if (netplay->is_server)
               RARCH_LOG("INP  %X %X\n", ptr->real_input_state[0], ptr->self_state[0]);
            else
               RARCH_LOG("INP  %X %X\n", ptr->self_state[0], ptr->real_input_state[0]);
            ptr = &netplay->buffer[netplay->replay_ptr];
            serial_info.data = ptr->state;
            memset(serial_info.data, 0, serial_info.size);
            core_serialize(&serial_info);
            RARCH_LOG("POST %u: %X\n", netplay->replay_frame_count-1, netplay->state_size ? netplay_delta_frame_crc(netplay, ptr) : 0);
         }
#endif

         /* Get our time window */
         tm = cpu_features_get_time_usec() - start;
         netplay->frame_run_time_sum -= netplay->frame_run_time[netplay->frame_run_time_ptr];
         netplay->frame_run_time[netplay->frame_run_time_ptr] = tm;
         netplay->frame_run_time_sum += tm;
         netplay->frame_run_time_ptr++;
         if (netplay->frame_run_time_ptr >= NETPLAY_FRAME_RUN_TIME_WINDOW)
            netplay->frame_run_time_ptr = 0;
      }

      /* Average our time */
      netplay->frame_run_time_avg   = netplay->frame_run_time_sum / NETPLAY_FRAME_RUN_TIME_WINDOW;

      if (netplay->unread_frame_count < netplay->run_frame_count)
      {
         netplay->other_ptr         = netplay->unread_ptr;
         netplay->other_frame_count = netplay->unread_frame_count;
      }
      else
      {
         netplay->other_ptr         = netplay->run_ptr;
         netplay->other_frame_count = netplay->run_frame_count;
      }
      netplay->is_replay            = false;
      netplay->force_rewind         = false;
   }

   if (netplay->is_server)
   {
      uint32_t client;

      lo_frame_count = hi_frame_count = netplay->unread_frame_count;

      /* Look for players that are ahead of us */
      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(netplay->connected_players & (1 << client)))
            continue;
         if (netplay->read_frame_count[client] > hi_frame_count)
            hi_frame_count = netplay->read_frame_count[client];
      }
   }
   else
      lo_frame_count = hi_frame_count = netplay->server_frame_count;

   /* If we're behind, try to catch up */
   if (netplay->catch_up)
   {
      /* Are we caught up? */
      if (netplay->self_frame_count + 1 >= lo_frame_count)
      {
         netplay->catch_up = false;
         input_state_get_ptr()->nonblocking_flag = false;
         driver_set_nonblock_state();
      }

   }
   else if (!stalled)
   {
      if (netplay->self_frame_count + 3 < lo_frame_count)
      {
         retro_time_t cur_time = cpu_features_get_time_usec();
         uint32_t cur_behind = lo_frame_count - netplay->self_frame_count;

         /* We're behind, but we'll only try to catch up if we're actually
          * falling behind, i.e. if we're more behind after some time */
         if (netplay->catch_up_time == 0)
         {
            /* Record our current time to check for catch-up later */
            netplay->catch_up_time = cur_time;
            netplay->catch_up_behind = cur_behind;

         }
         else if (cur_time - netplay->catch_up_time > CATCH_UP_CHECK_TIME_USEC)
         {
            /* Time to check how far behind we are */
            if (netplay->catch_up_behind <= cur_behind)
            {
               /* We're definitely falling behind! */
               netplay->catch_up                       = true;
               netplay->catch_up_time                  = 0;
               input_state_get_ptr()->nonblocking_flag = true;
               driver_set_nonblock_state();
            }
            else
            {
               /* Check again in another period */
               netplay->catch_up_time   = cur_time;
               netplay->catch_up_behind = cur_behind;
            }
         }

      }
      else if (netplay->self_frame_count + 3 < hi_frame_count)
      {
         size_t i;
         netplay->catch_up_time = 0;

         /* We're falling behind some clients but not others, so request that
          * clients ahead of us stall */
         for (i = 0; i < netplay->connections_size; i++)
         {
            uint32_t client_num;
            struct netplay_connection *connection = &netplay->connections[i];

            if (!connection->active ||
                connection->mode != NETPLAY_CONNECTION_PLAYING)
               continue;

            client_num = (uint32_t)(i + 1);

            /* Are they ahead? */
            if (netplay->self_frame_count + 3 < netplay->read_frame_count[client_num])
            {
               /* Tell them to stall */
               if (connection->stall_frame + NETPLAY_MAX_REQ_STALL_FREQUENCY <
                     netplay->self_frame_count)
               {
                  connection->stall_frame = netplay->self_frame_count;
                  netplay_cmd_stall(netplay, connection,
                     netplay->read_frame_count[client_num] -
                     netplay->self_frame_count + 1);
               }
            }
         }
      }
      else
         netplay->catch_up_time = 0;
   }
   else
      netplay->catch_up_time =  0;
}

#if 0
#define DEBUG_NETPLAY_STEPS 1

static void print_state(netplay_t *netplay)
{
   char msg[512];
   size_t cur = 0;
   uint32_t client;

#define APPEND(out) cur += snprintf out
#define M msg + cur, sizeof(msg) - cur

   APPEND((M, "NETPLAY: S:%u U:%u O:%u", netplay->self_frame_count, netplay->unread_frame_count, netplay->other_frame_count));
   if (!netplay->is_server)
      APPEND((M, " H:%u", netplay->server_frame_count));
   for (client = 0; client < MAX_USERS; client++)
   {
      if ((netplay->connected_players & (1<<client)))
         APPEND((M, " %u:%u", client, netplay->read_frame_count[client]));
   }
   msg[sizeof(msg)-1] = '\0';

   RARCH_LOG("[netplay] %s\n", msg);

#undef APPEND
#undef M
}
#endif

/**
 * remote_unpaused
 *
 * Mark a particular remote connection as unpaused and, if relevant, inform
 * every one else that they may resume.
 */
static void remote_unpaused(netplay_t *netplay,
      struct netplay_connection *connection)
{
    size_t i;
    connection->paused = false;
    netplay->remote_paused = false;
    for (i = 0; i < netplay->connections_size; i++)
    {
       struct netplay_connection *sc = &netplay->connections[i];
       if (sc->active && sc->paused)
       {
          netplay->remote_paused = true;
          break;
       }
    }
    if (!netplay->remote_paused && !netplay->local_paused)
       netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_RESUME, NULL, 0);
}

/**
 * netplay_hangup:
 *
 * Disconnects an active Netplay connection due to an error
 */
void netplay_hangup(netplay_t *netplay,
      struct netplay_connection *connection)
{
   char msg[512];
   const char *dmsg;
   size_t i;

   if (!netplay)
      return;
   if (!connection->active)
      return;

   msg[0] = msg[sizeof(msg)-1] = '\0';
   dmsg = msg;

   /* Report this disconnection */
   if (netplay->is_server)
   {
      if (connection->nick[0])
         snprintf(msg, sizeof(msg)-1, msg_hash_to_str(MSG_NETPLAY_SERVER_NAMED_HANGUP), connection->nick);
      else
         dmsg = msg_hash_to_str(MSG_NETPLAY_SERVER_HANGUP);
   }
   else
   {
      dmsg = msg_hash_to_str(MSG_NETPLAY_CLIENT_HANGUP);
#ifdef HAVE_DISCORD
      if (discord_is_inited)
      {
         discord_userdata_t userdata;
         userdata.status = DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED;
         command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
      }
#endif
      netplay->is_connected = false;
   }
   RARCH_LOG("[netplay] %s\n", dmsg);
   runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   socket_close(connection->fd);
   connection->active = false;
   netplay_deinit_socket_buffer(&connection->send_packet_buffer);
   netplay_deinit_socket_buffer(&connection->recv_packet_buffer);

   if (!netplay->is_server)
   {
      netplay->self_mode = NETPLAY_CONNECTION_NONE;
      netplay->connected_players &= (1L<<netplay->self_client_num);
      for (i = 0; i < MAX_CLIENTS; i++)
      {
         if (i == netplay->self_client_num)
            continue;
         netplay->client_devices[i] = 0;
      }
      for (i = 0; i < MAX_INPUT_DEVICES; i++)
         netplay->device_clients[i] &= (1L<<netplay->self_client_num);
      netplay->stall = NETPLAY_STALL_NONE;

   }
   else
   {
      uint32_t client_num = (uint32_t)(connection - netplay->connections + 1);

      /* Mark the player for removal */
      if (connection->mode == NETPLAY_CONNECTION_PLAYING ||
          connection->mode == NETPLAY_CONNECTION_SLAVE)
      {
         /* This special mode keeps the connection object alive long enough to
          * send the disconnection message at the correct time */
         connection->mode = NETPLAY_CONNECTION_DELAYED_DISCONNECT;
         connection->delay_frame = netplay->read_frame_count[client_num];

         /* Mark them as not playing anymore */
         netplay->connected_players &= ~(1L<<client_num);
         netplay->connected_slaves  &= ~(1L<<client_num);
         netplay->client_devices[client_num] = 0;
         for (i = 0; i < MAX_INPUT_DEVICES; i++)
            netplay->device_clients[i] &= ~(1L<<client_num);

      }

   }

   /* Unpause them */
   if (connection->paused)
      remote_unpaused(netplay, connection);
}

/**
 * netplay_delayed_state_change:
 *
 * Handle any pending state changes which are ready 
 * as of the beginning of the current frame.
 */
void netplay_delayed_state_change(netplay_t *netplay)
{
   unsigned i;

   for (i = 0; i < netplay->connections_size; i++)
   {
      uint32_t client_num                   = (uint32_t)(i + 1);
      struct netplay_connection *connection = &netplay->connections[i];

      if ((connection->active || connection->mode == NETPLAY_CONNECTION_DELAYED_DISCONNECT) &&
          connection->delay_frame &&
          connection->delay_frame <= netplay->self_frame_count)
      {
         /* Something was delayed! Prepare the MODE command */
         uint32_t payload[15] = {0};
         payload[0]           = htonl(connection->delay_frame);
         payload[1]           = htonl(client_num);
         payload[2]           = htonl(0);

         memcpy(payload + 3, netplay->device_share_modes, sizeof(netplay->device_share_modes));
         strncpy((char *) (payload + 7), connection->nick, NETPLAY_NICK_LEN);

         /* Remove the connection entirely if relevant */
         if (connection->mode == NETPLAY_CONNECTION_DELAYED_DISCONNECT)
            connection->mode = NETPLAY_CONNECTION_NONE;

         /* Then send the mode change packet */
         netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

         /* And forget the delay frame */
         connection->delay_frame = 0;
      }
   }
}

/* Send the specified input data */
static bool send_input_frame(netplay_t *netplay, struct delta_frame *dframe,
      struct netplay_connection *only, struct netplay_connection *except,
      uint32_t client_num, bool slave)
{
#define BUFSZ 16 /* FIXME: Arbitrary restriction */
   uint32_t buffer[BUFSZ], devices, device;
   size_t bufused, i;

   /* Set up the basic buffer */
   bufused = 4;
   buffer[0] = htonl(NETPLAY_CMD_INPUT);
   buffer[2] = htonl(dframe->frame);
   buffer[3] = htonl(client_num);

   /* Add the device data */
   devices = netplay->client_devices[client_num];
   for (device = 0; device < MAX_INPUT_DEVICES; device++)
   {
      netplay_input_state_t istate;
      if (!(devices & (1<<device)))
         continue;
      istate = dframe->real_input[device];
      while (istate && (!istate->used || istate->client_num != (slave?MAX_CLIENTS:client_num)))
         istate = istate->next;
      if (!istate)
         continue;
      if (bufused + istate->size >= BUFSZ)
         continue; /* FIXME: More severe? */
      for (i = 0; i < istate->size; i++)
         buffer[bufused+i] = htonl(istate->data[i]);
      bufused += istate->size;
   }
   buffer[1] = htonl((bufused-2) * sizeof(uint32_t));

#ifdef DEBUG_NETPLAY_STEPS
   RARCH_LOG("[netplay] Sending input for client %u\n", (unsigned) client_num);
   print_state(netplay);
#endif

   if (only)
   {
      if (!netplay_send(&only->send_packet_buffer, only->fd, buffer, bufused*sizeof(uint32_t)))
      {
         netplay_hangup(netplay, only);
         return false;
      }
   }
   else
   {
      for (i = 0; i < netplay->connections_size; i++)
      {
         struct netplay_connection *connection = &netplay->connections[i];
         if (connection == except)
            continue;
         if (connection->active &&
             connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
             (connection->mode != NETPLAY_CONNECTION_PLAYING ||
              i+1 != client_num))
         {
            if (!netplay_send(&connection->send_packet_buffer, connection->fd,
                  buffer, bufused*sizeof(uint32_t)))
               netplay_hangup(netplay, connection);
         }
      }
   }

   return true;
#undef BUFSZ
}

/**
 * netplay_send_cur_input
 *
 * Send the current input frame to a given connection.
 *
 * Returns true if successful, false otherwise.
 */
bool netplay_send_cur_input(netplay_t *netplay,
   struct netplay_connection *connection)
{
   uint32_t from_client, to_client;
   struct delta_frame *dframe = &netplay->buffer[netplay->self_ptr];

   if (netplay->is_server)
   {
      to_client = (uint32_t)(connection - netplay->connections + 1);

      /* Send the other players' input data (FIXME: This involves an
       * unacceptable amount of recalculating) */
      for (from_client = 1; from_client < MAX_CLIENTS; from_client++)
      {
         if (from_client == to_client)
            continue;

         if ((netplay->connected_players & (1<<from_client)))
         {
            if (dframe->have_real[from_client])
            {
               if (!send_input_frame(netplay, dframe, connection, NULL, from_client, false))
                  return false;
            }
         }
      }

      /* If we're not playing, send a NOINPUT */
      if (netplay->self_mode != NETPLAY_CONNECTION_PLAYING)
      {
         uint32_t payload = htonl(netplay->self_frame_count);
         if (!netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_NOINPUT,
               &payload, sizeof(payload)))
            return false;
      }

   }

   /* Send our own data */
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING
         || netplay->self_mode == NETPLAY_CONNECTION_SLAVE)
   {
      if (!send_input_frame(netplay, dframe, connection, NULL,
            netplay->self_client_num,
            netplay->self_mode == NETPLAY_CONNECTION_SLAVE))
         return false;
   }

   if (!netplay_send_flush(&connection->send_packet_buffer, connection->fd,
         false))
      return false;

   return true;
}

/**
 * netplay_send_raw_cmd
 *
 * Send a raw Netplay command to the given connection.
 *
 * Returns true on success, false on failure.
 */
bool netplay_send_raw_cmd(netplay_t *netplay,
   struct netplay_connection *connection, uint32_t cmd, const void *data,
   size_t size)
{
   uint32_t cmdbuf[2];

   cmdbuf[0] = htonl(cmd);
   cmdbuf[1] = htonl(size);

   if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmdbuf,
         sizeof(cmdbuf)))
      return false;

   if (size > 0)
      if (!netplay_send(&connection->send_packet_buffer, connection->fd, data, size))
         return false;

   return true;
}

/**
 * netplay_send_raw_cmd_all
 *
 * Send a raw Netplay command to all connections, optionally excluding one
 * (typically the client that the relevant command came from)
 */
void netplay_send_raw_cmd_all(netplay_t *netplay,
   struct netplay_connection *except, uint32_t cmd, const void *data,
   size_t size)
{
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection == except)
         continue;
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
      {
         if (!netplay_send_raw_cmd(netplay, connection, cmd, data, size))
            netplay_hangup(netplay, connection);
      }
   }
}

/**
 * netplay_send_flush_all
 *
 * Flush all of our output buffers
 */
static void netplay_send_flush_all(netplay_t *netplay,
   struct netplay_connection *except)
{
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection == except)
         continue;
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
      {
         if (!netplay_send_flush(&connection->send_packet_buffer,
            connection->fd, true))
            netplay_hangup(netplay, connection);
      }
   }
}

static bool netplay_cmd_nak(netplay_t *netplay,
   struct netplay_connection *connection)
{
   netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_NAK, NULL, 0);
   return false;
}

/**
 * netplay_settings_share_mode
 *
 * Get the preferred share mode
 */
static uint8_t netplay_settings_share_mode(
      unsigned share_digital, unsigned share_analog)
{
   if (share_digital || share_analog)
   {
      uint8_t share_mode     = 0;

      switch (share_digital)
      {
         case RARCH_NETPLAY_SHARE_DIGITAL_OR:
            share_mode |= NETPLAY_SHARE_DIGITAL_OR;
            break;
         case RARCH_NETPLAY_SHARE_DIGITAL_XOR:
            share_mode |= NETPLAY_SHARE_DIGITAL_XOR;
            break;
         case RARCH_NETPLAY_SHARE_DIGITAL_VOTE:
            share_mode |= NETPLAY_SHARE_DIGITAL_VOTE;
            break;
         default:
            share_mode |= NETPLAY_SHARE_NO_PREFERENCE;
      }

      switch (share_analog)
      {
         case RARCH_NETPLAY_SHARE_ANALOG_MAX:
            share_mode |= NETPLAY_SHARE_ANALOG_MAX;
            break;
         case RARCH_NETPLAY_SHARE_ANALOG_AVERAGE:
            share_mode |= NETPLAY_SHARE_ANALOG_AVERAGE;
            break;
         default:
            share_mode |= NETPLAY_SHARE_NO_PREFERENCE;
      }

      return share_mode;
   }
   return 0;
}

/**
 * netplay_cmd_mode
 *
 * Send a mode change request. As a server, the request is to ourself, and so
 * honored instantly.
 */
bool netplay_cmd_mode(netplay_t *netplay,
   enum rarch_netplay_connection_mode mode)
{
   uint32_t cmd, device;
   uint32_t payload_buf = 0, *payload    = NULL;
   uint8_t share_mode                    = 0;
   struct netplay_connection *connection = NULL;

   if (!netplay->is_server)
      connection = &netplay->one_connection;

   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         cmd = NETPLAY_CMD_SPECTATE;
         break;

      case NETPLAY_CONNECTION_SLAVE:
         payload_buf = NETPLAY_CMD_PLAY_BIT_SLAVE;
         /* no break */

      case NETPLAY_CONNECTION_PLAYING:
         {
            settings_t *settings = config_get_ptr();
            payload = &payload_buf;

            /* Add a share mode if requested */
            share_mode = netplay_settings_share_mode(
                  settings->uints.netplay_share_digital,
                  settings->uints.netplay_share_analog
                  );
            payload_buf |= ((uint32_t) share_mode) << 16;

            /* Request devices */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (settings->bools.netplay_request_devices[device])
                  payload_buf |= 1<<device;
            }

            payload_buf = htonl(payload_buf);
            cmd         = NETPLAY_CMD_PLAY;
         }
         break;

      default:
         return false;
   }

   if (netplay->is_server)
   {
      handle_play_spectate(netplay, 0, NULL, cmd, payload ? sizeof(uint32_t) : 0, payload);
      return true;
   }

   return netplay_send_raw_cmd(netplay, connection, cmd, payload,
         payload ? sizeof(uint32_t) : 0);
}

/**
 * announce_play_spectate
 *
 * Announce a play or spectate mode change
 */
static void announce_play_spectate(netplay_t *netplay,
      const char *nick,
      enum rarch_netplay_connection_mode mode, uint32_t devices)
{
   char msg[512];
   msg[0] = msg[sizeof(msg) - 1] = '\0';

   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         if (nick)
            snprintf(msg, sizeof(msg) - 1,
                  msg_hash_to_str(MSG_NETPLAY_PLAYER_S_LEFT), NETPLAY_NICK_LEN,
                  nick);
         else
            strlcpy(msg, msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME), sizeof(msg));
         break;

      case NETPLAY_CONNECTION_PLAYING:
      case NETPLAY_CONNECTION_SLAVE:
      {
         uint32_t device;
         uint32_t one_device = (uint32_t) -1;
         char device_str[512];
         size_t device_str_len;

         for (device = 0; device < MAX_INPUT_DEVICES; device++)
         {
            if (!(devices & (1<<device)))
               continue;
            if (one_device == (uint32_t) -1)
               one_device = device;
            else
            {
               one_device = (uint32_t) -1;
               break;
            }
         }

         if (one_device != (uint32_t) -1)
         {
            /* Only have one device, simpler message */
            if (nick)
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N),
                     NETPLAY_NICK_LEN, nick, one_device + 1);
            else
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N),
                     one_device + 1);
         }
         else
         {
            /* Multiple devices, so step one is to make the device string listing them all */
            device_str[0] = 0;
            device_str_len = 0;
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (!(devices & (1<<device)))
                  continue;
               if (device_str_len)
                  device_str_len += snprintf(device_str + device_str_len,
                        sizeof(device_str) - 1 - device_str_len, ", ");
               device_str_len += snprintf(device_str + device_str_len,
                     sizeof(device_str) - 1 - device_str_len, "%u",
                     (unsigned) (device+1));
            }

            /* Then we make the final string */
            if (nick)
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(
                           MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S),
                     NETPLAY_NICK_LEN, nick, sizeof(device_str),
                     device_str);
            else
               snprintf(msg, sizeof(msg) - 1,
                     msg_hash_to_str(
                           MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S),
                     sizeof(device_str), device_str);
         }

         break;
      }

      default: /* wrong usage */
         break;
   }

   if (msg[0])
   {
      RARCH_LOG("[netplay] %s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

/**
 * handle_play_spectate
 *
 * Handle a play or spectate request
 */
static void handle_play_spectate(netplay_t *netplay, uint32_t client_num,
      struct netplay_connection *connection, uint32_t cmd, uint32_t cmd_size,
      uint32_t *in_payload)
{
   /*
    * MODE payload:
    * word 0: frame number
    * word 1: mode info (playing, slave, client number)
    * word 2: device bitmap
    * words 3-6: share modes for all devices
    * words 7-14: client nick
    */
   uint32_t payload[15] = {0};

   switch (cmd)
   {
      case NETPLAY_CMD_SPECTATE:
      {
         size_t i;

         /* The frame we haven't received is their end frame */
         if (connection)
            connection->delay_frame = netplay->read_frame_count[client_num];

         /* Mark them as not playing anymore */
         if (connection)
            connection->mode = NETPLAY_CONNECTION_SPECTATING;
         else
         {
            netplay->self_devices = 0;
            netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
         }
         netplay->connected_players &= ~(1 << client_num);
         netplay->connected_slaves &= ~(1 << client_num);
         netplay->client_devices[client_num] = 0;
         for (i = 0; i < MAX_INPUT_DEVICES; i++)
            netplay->device_clients[i] &= ~(1 << client_num);

         /* Tell someone */
         payload[0] = htonl(netplay->read_frame_count[client_num]);
         payload[2] = htonl(0);
         memcpy(payload + 3, netplay->device_share_modes, sizeof(netplay->device_share_modes));
         if (connection)
         {
            /* Only tell the player. The others will be told at delay_frame */
            payload[1] = htonl(NETPLAY_CMD_MODE_BIT_YOU | client_num);
            strncpy((char *) (payload + 7), connection->nick, NETPLAY_NICK_LEN);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));

         }
         else
         {
            /* It was the server, so tell everyone else */
            payload[1] = htonl(0);
            strncpy((char *) (payload + 7), netplay->nick, NETPLAY_NICK_LEN);
            netplay_send_raw_cmd_all(netplay, NULL, NETPLAY_CMD_MODE, payload, sizeof(payload));

         }

         /* Announce it */
         announce_play_spectate(netplay, connection ? connection->nick : NULL,
               NETPLAY_CONNECTION_SPECTATING, 0);
         break;
      }

      case NETPLAY_CMD_PLAY:
      {
         uint32_t mode, devices = 0, device;
         uint8_t share_mode;
         bool slave = false;
         settings_t *settings = config_get_ptr();

         if (cmd_size != sizeof(uint32_t) || !in_payload)
            return;
         mode = ntohl(in_payload[0]);

         /* Check the requested mode */
         slave = (mode&NETPLAY_CMD_PLAY_BIT_SLAVE)?true:false;
         share_mode = (mode>>16)&0xFF;

         /* And the requested devices */
         devices = mode&0xFFFF;

         /* Check if their slave mode request corresponds with what we allow */
         if (connection)
         {
            if (settings->bools.netplay_require_slaves)
               slave = true;
            else if (!settings->bools.netplay_allow_slaves)
               slave = false;
         }
         else
            slave = false;

         /* Fix our share mode */
         if (share_mode)
         {
            if ((share_mode & NETPLAY_SHARE_DIGITAL_BITS) == 0)
               share_mode |= NETPLAY_SHARE_DIGITAL_OR;
            if ((share_mode & NETPLAY_SHARE_ANALOG_BITS) == 0)
               share_mode |= NETPLAY_SHARE_ANALOG_MAX;
            share_mode &= ~NETPLAY_SHARE_NO_PREFERENCE;
         }

         /* They start at the next frame, but we start immediately */
         if (connection)
         {
            netplay->read_ptr[client_num] = NEXT_PTR(netplay->self_ptr);
            netplay->read_frame_count[client_num] = netplay->self_frame_count + 1;
         }
         else
         {
            netplay->read_ptr[client_num] = netplay->self_ptr;
            netplay->read_frame_count[client_num] = netplay->self_frame_count;
         }
         payload[0] = htonl(netplay->read_frame_count[client_num]);

         if (devices)
         {
            /* Make sure the devices are available and/or shareable */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (!(devices & (1<<device)))
                  continue;
               if (!netplay->device_clients[device])
                  continue;
               if (netplay->device_share_modes[device] && share_mode)
                  continue;

               /* Device already taken and unshareable */
               payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_NOT_AVAILABLE);
               /* FIXME: Refusal message for the server */
               if (connection)
                  netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
               devices = 0;
               break;
            }
            if (devices == 0)
               break;

            /* Set the share mode on any new devices */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (!(devices & (1<<device)))
                  continue;
               if (!netplay->device_clients[device])
                  netplay->device_share_modes[device] = share_mode;
            }

         }
         else
         {
            /* Find an available device */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (netplay->config_devices[device] == RETRO_DEVICE_NONE)
               {
                  device = MAX_INPUT_DEVICES;
                  break;
               }
               if (!netplay->device_clients[device])
                  break;
            }
            if (device >= MAX_INPUT_DEVICES &&
                netplay->config_devices[1] == RETRO_DEVICE_NONE && share_mode)
            {
               /* No device free and no device specifically asked for, but only
                * one device, so share it */
               if (netplay->device_share_modes[0])
               {
                  device     = 0;
                  share_mode = netplay->device_share_modes[0];
                  break;
               }
            }
            if (device >= MAX_INPUT_DEVICES)
            {
               /* No slots free! */
               payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS);
               /* FIXME: Message for the server */
               if (connection)
                  netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
               break;
            }
            devices = 1<<device;
            netplay->device_share_modes[device] = share_mode;

         }

         payload[2] = htonl(devices);

         /* Mark them as playing */
         if (connection)
            connection->mode =
                  slave ? NETPLAY_CONNECTION_SLAVE : NETPLAY_CONNECTION_PLAYING;
         else
         {
            netplay->self_devices = devices;
            netplay->self_mode = NETPLAY_CONNECTION_PLAYING;
         }
         netplay->connected_players |= 1 << client_num;
         if (slave)
            netplay->connected_slaves |= 1 << client_num;
         netplay->client_devices[client_num] = devices;
         for (device = 0; device < MAX_INPUT_DEVICES; device++)
         {
            if (!(devices & (1<<device)))
               continue;
            netplay->device_clients[device] |= 1 << client_num;
         }

         /* Tell everyone */
         payload[1] = htonl(
               NETPLAY_CMD_MODE_BIT_PLAYING
                     | (slave ? NETPLAY_CMD_MODE_BIT_SLAVE : 0) | client_num);
         memcpy(payload + 3, netplay->device_share_modes, sizeof(netplay->device_share_modes));
         if (connection)
            strncpy((char *) (payload + 7), connection->nick, NETPLAY_NICK_LEN);
         else
            strncpy((char *) (payload + 7), netplay->nick, NETPLAY_NICK_LEN);
         netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_MODE,
               payload, sizeof(payload));

         /* Tell the player */
         if (connection)
         {
            payload[1] = htonl(NETPLAY_CMD_MODE_BIT_PLAYING |
                               ((connection->mode == NETPLAY_CONNECTION_SLAVE)?
                                NETPLAY_CMD_MODE_BIT_SLAVE:0) |
                               NETPLAY_CMD_MODE_BIT_YOU |
                               client_num);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE, payload, sizeof(payload));
         }

         /* Announce it */
         announce_play_spectate(netplay, connection ? connection->nick : NULL,
               NETPLAY_CONNECTION_PLAYING, devices);
         break;
      }
   }
}

#undef RECV
#define RECV(buf, sz) \
recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), \
(sz), false); \
if (recvd >= 0 && recvd < (ssize_t) (sz)) goto shrt; \
else if (recvd < 0)

static bool netplay_get_cmd(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   uint32_t cmd;
   uint32_t cmd_size;
   ssize_t recvd;

   /* We don't handle the initial handshake here */
   if (connection->mode < NETPLAY_CONNECTION_CONNECTED)
      return netplay_handshake(netplay, connection, had_input);

   RECV(&cmd, sizeof(cmd))
      return false;

   cmd      = ntohl(cmd);

   RECV(&cmd_size, sizeof(cmd_size))
      return false;

   cmd_size = ntohl(cmd_size);

#ifdef DEBUG_NETPLAY_STEPS
   RARCH_LOG("[netplay] Received netplay command %X (%u) from %u\n", cmd, cmd_size,
         (unsigned) (connection - netplay->connections));
#endif

   netplay->timeout_cnt = 0;

   switch (cmd)
   {
      case NETPLAY_CMD_ACK:
         /* Why are we even bothering? */
         break;

      case NETPLAY_CMD_NAK:
         /* Disconnect now! */
         return false;

      case NETPLAY_CMD_INPUT:
         {
            uint32_t frame_num, client_num, input_size, devices, device;
            struct delta_frame *dframe;

            if (cmd_size < 2*sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_INPUT too short, no frame/client number.");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frame_num, sizeof(frame_num))
               return false;
            RECV(&client_num, sizeof(client_num))
               return false;
            frame_num = ntohl(frame_num);
            client_num = ntohl(client_num);
            client_num &= 0xFFFF;

            if (netplay->is_server)
            {
               /* Ignore the claimed client #, must be this client */
               if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
                   connection->mode != NETPLAY_CONNECTION_SLAVE)
               {
                  RARCH_ERR("Netplay input from non-participating player.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
               client_num = (uint32_t)(connection - netplay->connections + 1);
            }

            if (client_num > MAX_CLIENTS)
            {
               RARCH_ERR("NETPLAY_CMD_INPUT received data for an unsupported client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Figure out how much input is expected */
            devices = netplay->client_devices[client_num];
            input_size = netplay_expected_input_size(netplay, devices);

            if (cmd_size != (2+input_size) * sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_INPUT received an unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (client_num >= MAX_CLIENTS || !(netplay->connected_players & (1<<client_num)))
            {
               RARCH_ERR("Invalid NETPLAY_CMD_INPUT player number.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Check the frame number only if they're not in slave mode */
            if (connection->mode == NETPLAY_CONNECTION_PLAYING)
            {
               if (frame_num < netplay->read_frame_count[client_num])
               {
                  uint32_t buf;
                  /* We already had this, so ignore the new transmission */
                  for (; input_size; input_size--)
                  {
                     RECV(&buf, sizeof(uint32_t))
                        return netplay_cmd_nak(netplay, connection);
                  }
                  break;
               }
               else if (frame_num > netplay->read_frame_count[client_num])
               {
                  /* Out of order = out of luck */
                  RARCH_ERR("Netplay input out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
            }

            /* The data's good! */
            dframe = &netplay->buffer[netplay->read_ptr[client_num]];
            if (!netplay_delta_frame_ready(netplay, dframe, netplay->read_frame_count[client_num]))
            {
               /* Hopefully we'll be ready after another round of input */
               goto shrt;
            }

            /* Copy in the input */
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               netplay_input_state_t istate;
               uint32_t dsize, di;
               if (!(devices & (1<<device)))
                  continue;

               dsize = netplay_expected_input_size(netplay, 1 << device);
               istate = netplay_input_state_for(&dframe->real_input[device],
                     client_num, dsize,
                     false /* Must be false because of slave-mode clients */,
                     false);
               if (!istate)
               {
                  /* Catastrophe! */
                  return netplay_cmd_nak(netplay, connection);
               }
               RECV(istate->data, dsize*sizeof(uint32_t))
                  return false;
               for (di = 0; di < dsize; di++)
                  istate->data[di] = ntohl(istate->data[di]);
            }
            dframe->have_real[client_num] = true;

            /* Slaves may go through several packets of data in the same frame
             * if latency is choppy, so we advance and send their data after
             * handling all network data this frame */
            if (connection->mode == NETPLAY_CONNECTION_PLAYING)
            {
               netplay->read_ptr[client_num] = NEXT_PTR(netplay->read_ptr[client_num]);
               netplay->read_frame_count[client_num]++;

               if (netplay->is_server)
               {
                  /* Forward it on if it's past data */
                  if (dframe->frame <= netplay->self_frame_count)
                     send_input_frame(netplay, dframe, NULL, connection, client_num, false);
               }
            }

            /* If this was server data, advance our server pointer too */
            if (!netplay->is_server && client_num == 0)
            {
               netplay->server_ptr = netplay->read_ptr[0];
               netplay->server_frame_count = netplay->read_frame_count[0];
            }

#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[netplay] Received input from %u\n", client_num);
            print_state(netplay);
#endif
            break;
         }

      case NETPLAY_CMD_NOINPUT:
         {
            uint32_t frame;

            if (netplay->is_server)
            {
               RARCH_ERR("NETPLAY_CMD_NOINPUT from a client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frame, sizeof(frame))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_NOINPUT payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            frame = ntohl(frame);

            /* We already had this, so ignore the new transmission */
            if (frame < netplay->server_frame_count)
               break;

            if (frame != netplay->server_frame_count)
            {
               RARCH_ERR("NETPLAY_CMD_NOINPUT for invalid frame.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            netplay->server_ptr = NEXT_PTR(netplay->server_ptr);
            netplay->server_frame_count++;
#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[netplay] Received server noinput\n");
            print_state(netplay);
#endif
            break;
         }

      case NETPLAY_CMD_SPECTATE:
      {
         uint32_t client_num;

         if (!netplay->is_server)
         {
            RARCH_ERR("NETPLAY_CMD_SPECTATE from a server.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (cmd_size != 0)
         {
            RARCH_ERR("Unexpected payload in NETPLAY_CMD_SPECTATE.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
             connection->mode != NETPLAY_CONNECTION_SLAVE)
         {
            /* They were confused */
            return netplay_cmd_nak(netplay, connection);
         }

         client_num = (uint32_t)(connection - netplay->connections + 1);

         handle_play_spectate(netplay, client_num, connection, cmd, 0, NULL);
         break;
      }

      case NETPLAY_CMD_PLAY:
      {
         uint32_t client_num;
         uint32_t payload[1];

         if (cmd_size != sizeof(uint32_t))
         {
            RARCH_ERR("Incorrect NETPLAY_CMD_PLAY payload size.\n");
            return netplay_cmd_nak(netplay, connection);
         }
         RECV(payload, sizeof(uint32_t))
         {
            RARCH_ERR("Failed to receive NETPLAY_CMD_PLAY payload.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (!netplay->is_server)
         {
            RARCH_ERR("NETPLAY_CMD_PLAY from a server.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->delay_frame)
         {
            /* Can't switch modes while a mode switch is already in progress. */
            payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_TOO_FAST);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
            break;
         }

         if (!connection->can_play)
         {
            /* Not allowed to play */
            payload[0] = htonl(NETPLAY_CMD_MODE_REFUSED_REASON_UNPRIVILEGED);
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_MODE_REFUSED, payload, sizeof(uint32_t));
            break;
         }

         /* They were obviously confused */
         if (
                  connection->mode == NETPLAY_CONNECTION_PLAYING
               || connection->mode == NETPLAY_CONNECTION_SLAVE)
            return netplay_cmd_nak(netplay, connection);

         client_num = (unsigned)(connection - netplay->connections + 1);

         handle_play_spectate(netplay, client_num, connection, cmd, cmd_size, payload);
         break;
      }

      case NETPLAY_CMD_MODE:
      {
         uint32_t payload[15];
         uint32_t frame, mode, client_num, devices, device;
         size_t ptr;
         struct delta_frame *dframe;
         const char *nick;

#define START(which) \
         do { \
            ptr    = which; \
            dframe = &netplay->buffer[ptr]; \
         } while (0)
#define NEXT() \
         do { \
            ptr    = NEXT_PTR(ptr); \
            dframe = &netplay->buffer[ptr]; \
         } while (0)

         if (netplay->is_server)
         {
            RARCH_ERR("NETPLAY_CMD_MODE from client.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (cmd_size != sizeof(payload))
         {
            RARCH_ERR("Invalid payload size for NETPLAY_CMD_MODE.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         RECV(payload, sizeof(payload))
         {
            RARCH_ERR("NETPLAY_CMD_MODE failed to receive payload.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         frame = ntohl(payload[0]);

         /* We're changing past input, so must replay it */
         if (frame < netplay->self_frame_count)
            netplay->force_rewind = true;

         mode = ntohl(payload[1]);
         client_num = mode & 0xFFFF;
         if (client_num >= MAX_CLIENTS)
         {
            RARCH_ERR("Received NETPLAY_CMD_MODE for a higher player number than we support.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         devices = ntohl(payload[2]);
         memcpy(netplay->device_share_modes, payload + 3, sizeof(netplay->device_share_modes));
         nick = (const char *) (payload + 7);

         if (mode & NETPLAY_CMD_MODE_BIT_YOU)
         {
            /* A change to me! */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               if (frame != netplay->server_frame_count)
               {
                  RARCH_ERR("Received mode change out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Hooray, I get to play now! */
               if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
               {
                  RARCH_ERR("Received player mode change even though I'm already a player.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Our mode is based on whether we have the slave bit set */
               if (mode & NETPLAY_CMD_MODE_BIT_SLAVE)
                  netplay->self_mode = NETPLAY_CONNECTION_SLAVE;
               else
                  netplay->self_mode = NETPLAY_CONNECTION_PLAYING;

               netplay->connected_players |= (1<<client_num);
               netplay->client_devices[client_num] = devices;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  if (devices & (1<<device))
                     netplay->device_clients[device] |= (1<<client_num);
               netplay->self_devices = devices;

               netplay->read_ptr[client_num] = netplay->server_ptr;
               netplay->read_frame_count[client_num] = netplay->server_frame_count;

               /* Fix up current frame info */
               if (!(mode & NETPLAY_CMD_MODE_BIT_SLAVE) && frame <= netplay->self_frame_count)
               {
                  /* It wanted past frames, better send 'em! */
                  START(netplay->server_ptr);
                  while (dframe->used && dframe->frame <= netplay->self_frame_count)
                  {
                     for (device = 0; device < MAX_INPUT_DEVICES; device++)
                     {
                        uint32_t dsize;
                        netplay_input_state_t istate;
                        if (!(devices & (1<<device)))
                           continue;
                        dsize = netplay_expected_input_size(netplay, 1 << device);
                        istate = netplay_input_state_for(
                              &dframe->real_input[device], client_num, dsize,
                              false, false);
                        if (!istate)
                           continue;
                        memset(istate->data, 0, dsize*sizeof(uint32_t));
                     }
                     dframe->have_local = true;
                     dframe->have_real[client_num] = true;
                     send_input_frame(netplay, dframe, connection, NULL, client_num, false);
                     if (dframe->frame == netplay->self_frame_count) break;
                     NEXT();
                  }

               }
               else
               {
                  uint32_t frame_count;

                  /* It wants future frames, make sure we don't capture or send intermediate ones */
                  START(netplay->self_ptr);
                  frame_count = netplay->self_frame_count;

                  for (;;)
                  {
                     if (!dframe->used)
                     {
                        /* Make sure it's ready */
                        if (!netplay_delta_frame_ready(netplay, dframe, frame_count))
                        {
                           RARCH_ERR("Received mode change but delta frame isn't ready!\n");
                           return netplay_cmd_nak(netplay, connection);
                        }
                     }

                     dframe->have_local = true;

                     /* Go on to the next delta frame */
                     NEXT();
                     frame_count++;

                     if (frame_count >= frame)
                        break;
                  }

               }

               /* Announce it */
               announce_play_spectate(netplay, NULL, NETPLAY_CONNECTION_PLAYING, devices);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change self->%X\n", devices);
               print_state(netplay);
#endif

            }
            else /* YOU && !PLAYING */
            {
               /* I'm no longer playing, but I should already know this */
               if (netplay->self_mode != NETPLAY_CONNECTION_SPECTATING)
               {
                  RARCH_ERR("Received mode change to spectator unprompted.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Unmark ourself, in case we were in slave mode */
               netplay->connected_players &= ~(1<<client_num);
               netplay->client_devices[client_num] = 0;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  netplay->device_clients[device] &= ~(1<<client_num);

               /* Announce it */
               announce_play_spectate(netplay, NULL, NETPLAY_CONNECTION_SPECTATING, 0);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change self->spectating\n");
               print_state(netplay);
#endif

            }

         }
         else /* !YOU */
         {
            /* Somebody else is joining or parting */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               if (frame != netplay->server_frame_count)
               {
                  RARCH_ERR("Received mode change out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               netplay->connected_players |= (1<<client_num);
               netplay->client_devices[client_num] = devices;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  if (devices & (1<<device))
                     netplay->device_clients[device] |= (1<<client_num);

               netplay->read_ptr[client_num] = netplay->server_ptr;
               netplay->read_frame_count[client_num] = netplay->server_frame_count;

               /* Announce it */
               announce_play_spectate(netplay, nick, NETPLAY_CONNECTION_PLAYING, devices);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change %u->%u\n", client_num, devices);
               print_state(netplay);
#endif

            }
            else
            {
               netplay->connected_players &= ~(1<<client_num);
               netplay->client_devices[client_num] = 0;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  netplay->device_clients[device] &= ~(1<<client_num);

               /* Announce it */
               announce_play_spectate(netplay, nick, NETPLAY_CONNECTION_SPECTATING, 0);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[netplay] Received mode change %u->spectator\n", client_num);
               print_state(netplay);
#endif

            }

         }

         break;

#undef START
#undef NEXT
      }

      case NETPLAY_CMD_MODE_REFUSED:
         {
            uint32_t reason;
            const char *dmsg = NULL;

            if (netplay->is_server)
            {
               RARCH_ERR("NETPLAY_CMD_MODE_REFUSED from client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(uint32_t))
            {
               RARCH_ERR("Received invalid payload size for NETPLAY_CMD_MODE_REFUSED.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&reason, sizeof(reason))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_MODE_REFUSED payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            reason = ntohl(reason);

            switch (reason)
            {
               case NETPLAY_CMD_MODE_REFUSED_REASON_UNPRIVILEGED:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED);
                  break;

               case NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS);
                  break;

               case NETPLAY_CMD_MODE_REFUSED_REASON_NOT_AVAILABLE:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE);
                  break;

               default:
                  dmsg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY);
            }

            if (dmsg)
            {
               RARCH_LOG("[netplay] %s\n", dmsg);
               runloop_msg_queue_push(dmsg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            break;
         }

      case NETPLAY_CMD_DISCONNECT:
         netplay_hangup(netplay, connection);
         return true;

      case NETPLAY_CMD_CRC:
         {
            uint32_t buffer[2];
            size_t tmp_ptr = netplay->run_ptr;
            bool found = false;

            if (cmd_size != sizeof(buffer))
            {
               RARCH_ERR("NETPLAY_CMD_CRC received unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(buffer, sizeof(buffer))
            {
               RARCH_ERR("NETPLAY_CMD_CRC failed to receive payload.\n");
               return netplay_cmd_nak(netplay, connection);
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
            } while (tmp_ptr != netplay->run_ptr);

            /* Oh well, we got rid of it! */
            if (!found)
               break;

            if (buffer[0] <= netplay->other_frame_count)
            {
               /* We've already replayed up to this frame, so we can check it
                * directly */
               uint32_t local_crc = 0;
               if (netplay->state_size)
                  local_crc       = netplay_delta_frame_crc(
                        netplay, &netplay->buffer[tmp_ptr]);

               /* Problem! */
               if (buffer[1] != local_crc)
                  netplay_cmd_request_savestate(netplay);
            }
            else
            {
               /* We'll have to check it when we catch up */
               netplay->buffer[tmp_ptr].crc = buffer[1];
            }

            break;
         }

      case NETPLAY_CMD_REQUEST_SAVESTATE:
         /* Delay until next frame so we don't send the savestate after the
          * input */
         netplay->force_send_savestate = true;
         break;

      case NETPLAY_CMD_LOAD_SAVESTATE:
      case NETPLAY_CMD_RESET:
         {
            uint32_t frame;
            uint32_t isize;
            uint32_t rd, wn;
            uint32_t client;
            uint32_t load_frame_count;
            size_t load_ptr;
            struct compression_transcoder *ctrans = NULL;
            uint32_t                   client_num = (uint32_t)
             (connection - netplay->connections + 1);

            /* Make sure we're ready for it */
            if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            {
               if (!netplay->is_replay)
               {
                  netplay->is_replay          = true;
                  netplay->replay_ptr         = netplay->run_ptr;
                  netplay->replay_frame_count = netplay->run_frame_count;
                  netplay_wait_and_init_serialization(netplay);
                  netplay->is_replay         = false;
               }
               else
                  netplay_wait_and_init_serialization(netplay);
            }

            /* Only players may load states */
            if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
                connection->mode != NETPLAY_CONNECTION_SLAVE)
            {
               RARCH_ERR("Netplay state load from a spectator.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* We only allow players to load state if we're in a simple
             * two-player situation */
            if (netplay->is_server && netplay->connections_size > 1)
            {
               RARCH_ERR("Netplay state load from a client with other clients connected disallowed.\n");
               return netplay_cmd_nak(netplay, connection);
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

            /* Check the payload size */
            if ((cmd == NETPLAY_CMD_LOAD_SAVESTATE &&
                 (cmd_size < 2*sizeof(uint32_t) || cmd_size > netplay->zbuffer_size + 2*sizeof(uint32_t))) ||
                (cmd == NETPLAY_CMD_RESET && cmd_size != sizeof(uint32_t)))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE received an unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frame, sizeof(frame))
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive savestate frame.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            frame = ntohl(frame);

            if (netplay->is_server)
            {
               load_ptr = netplay->read_ptr[client_num];
               load_frame_count = netplay->read_frame_count[client_num];
            }
            else
            {
               load_ptr = netplay->server_ptr;
               load_frame_count = netplay->server_frame_count;
            }

            if (frame != load_frame_count)
            {
               RARCH_ERR("CMD_LOAD_SAVESTATE loading a state out of order!\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (!netplay_delta_frame_ready(netplay, &netplay->buffer[load_ptr], load_frame_count))
            {
               /* Hopefully it will be after another round of input */
               goto shrt;
            }

            /* Now we switch based on whether we're loading a state or resetting */
            if (cmd == NETPLAY_CMD_LOAD_SAVESTATE)
            {
               RECV(&isize, sizeof(isize))
               {
                  RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive inflated size.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
               isize = ntohl(isize);

               if (isize != netplay->state_size)
               {
                  RARCH_ERR("CMD_LOAD_SAVESTATE received an unexpected save state size.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               RECV(netplay->zbuffer, cmd_size - 2*sizeof(uint32_t))
               {
                  RARCH_ERR("CMD_LOAD_SAVESTATE failed to receive savestate.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* And decompress it */
               switch (connection->compression_supported)
               {
                  case NETPLAY_COMPRESSION_ZLIB:
                     ctrans = &netplay->compress_zlib;
                     break;
                  default:
                     ctrans = &netplay->compress_nil;
               }
               ctrans->decompression_backend->set_in(ctrans->decompression_stream,
                  netplay->zbuffer, cmd_size - 2*sizeof(uint32_t));
               ctrans->decompression_backend->set_out(ctrans->decompression_stream,
                  (uint8_t*)netplay->buffer[load_ptr].state,
                  (unsigned)netplay->state_size);
               ctrans->decompression_backend->trans(ctrans->decompression_stream,
                  true, &rd, &wn, NULL);

               /* Force a rewind to the relevant frame */
               netplay->force_rewind = true;
            }
            else
            {
               /* Resetting */
               netplay->force_reset = true;

            }

            /* Skip ahead if it's past where we are */
            if (load_frame_count > netplay->run_frame_count ||
                cmd == NETPLAY_CMD_RESET)
            {
               /* This is squirrely: We need to assure that when we advance the
                * frame in post_frame, THEN we're referring to the frame to
                * load into. If we refer directly to read_ptr, then we'll end
                * up never reading the input for read_frame_count itself, which
                * will make the other side unhappy. */
               netplay->run_ptr           = PREV_PTR(load_ptr);
               netplay->run_frame_count   = load_frame_count - 1;
               if (frame > netplay->self_frame_count)
               {
                  netplay->self_ptr         = netplay->run_ptr;
                  netplay->self_frame_count = netplay->run_frame_count;
               }
            }

            /* Don't expect earlier data from other clients */
            for (client = 0; client < MAX_CLIENTS; client++)
            {
               if (!(netplay->connected_players & (1<<client)))
                  continue;

               if (frame > netplay->read_frame_count[client])
               {
                  netplay->read_ptr[client] = load_ptr;
                  netplay->read_frame_count[client] = load_frame_count;
               }
            }

            /* Make sure our states are correct */
            netplay->savestate_request_outstanding = false;
            netplay->other_ptr                     = load_ptr;
            netplay->other_frame_count             = load_frame_count;

#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[netplay] Loading state at %u\n", load_frame_count);
            print_state(netplay);
#endif

            break;
         }

      case NETPLAY_CMD_PAUSE:
         {
            char msg[512], nick[NETPLAY_NICK_LEN];
            msg[sizeof(msg)-1] = '\0';

            /* Read in the paused nick */
            if (cmd_size != sizeof(nick))
            {
               RARCH_ERR("NETPLAY_CMD_PAUSE received invalid payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            RECV(nick, sizeof(nick))
            {
               RARCH_ERR("Failed to receive paused nickname.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            nick[sizeof(nick)-1] = '\0';

            /* We outright ignore pausing from spectators and slaves */
            if (connection->mode != NETPLAY_CONNECTION_PLAYING)
               break;

            connection->paused = true;
            netplay->remote_paused = true;
            if (netplay->is_server)
            {
               /* Inform peers */
               snprintf(msg, sizeof(msg)-1, msg_hash_to_str(MSG_NETPLAY_PEER_PAUSED), connection->nick);
               netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_PAUSE,
                     connection->nick, NETPLAY_NICK_LEN);

               /* We may not reach post_frame soon, so flush the pause message
                * immediately. */
               netplay_send_flush_all(netplay, connection);
            }
            else
            {
               snprintf(msg, sizeof(msg)-1, msg_hash_to_str(MSG_NETPLAY_PEER_PAUSED), nick);
            }
            RARCH_LOG("[netplay] %s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;
         }

      case NETPLAY_CMD_RESUME:
         remote_unpaused(netplay, connection);
         break;

      case NETPLAY_CMD_STALL:
         {
            uint32_t frames;

            if (cmd_size != sizeof(uint32_t))
            {
               RARCH_ERR("NETPLAY_CMD_STALL with incorrect payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frames, sizeof(frames))
            {
               RARCH_ERR("Failed to receive NETPLAY_CMD_STALL payload.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            frames = ntohl(frames);
            if (frames > NETPLAY_MAX_REQ_STALL_TIME)
               frames = NETPLAY_MAX_REQ_STALL_TIME;

            if (netplay->is_server)
            {
               /* Only servers can request a stall! */
               RARCH_ERR("Netplay client requested a stall?\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* We can only stall for one reason at a time */
            if (!netplay->stall)
            {
               connection->stall = netplay->stall = NETPLAY_STALL_SERVER_REQUESTED;
               netplay->stall_time = 0;
               connection->stall_frame = frames;
            }
            break;
         }

      default:
         RARCH_ERR("%s.\n", msg_hash_to_str(MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED));
         return netplay_cmd_nak(netplay, connection);
   }

   netplay_recv_flush(&connection->recv_packet_buffer);
   netplay->timeout_cnt = 0;
   if (had_input)
      *had_input = true;
   return true;

shrt:
   /* No more data, reset and try again */
   netplay_recv_reset(&connection->recv_packet_buffer);
   return true;

#undef RECV
}

/**
 * netplay_poll_net_input
 *
 * Poll input from the network
 */
int netplay_poll_net_input(netplay_t *netplay, bool block)
{
   bool had_input = false;
   int max_fd = 0;
   size_t i;

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->fd >= max_fd)
         max_fd = connection->fd + 1;
   }

   if (max_fd == 0)
      return 0;

   netplay->timeout_cnt = 0;

   do
   {
      had_input = false;

      netplay->timeout_cnt++;

      /* Read input from each connection */
      for (i = 0; i < netplay->connections_size; i++)
      {
         struct netplay_connection *connection = &netplay->connections[i];
         if (connection->active && !netplay_get_cmd(netplay, connection, &had_input))
            netplay_hangup(netplay, connection);
      }

      if (block)
      {
         netplay_update_unread_ptr(netplay);

         /* If we were blocked for input, pass if we have this frame's input */
         if (netplay->unread_frame_count > netplay->run_frame_count)
            break;

         /* If we're supposed to block but we didn't have enough input, wait for it */
         if (!had_input)
         {
            fd_set fds;
            struct timeval tv = {0};
            tv.tv_usec = RETRY_MS * 1000;

            FD_ZERO(&fds);
            for (i = 0; i < netplay->connections_size; i++)
            {
               struct netplay_connection *connection = &netplay->connections[i];
               if (connection->active)
                  FD_SET(connection->fd, &fds);
            }

            if (socket_select(max_fd, &fds, NULL, NULL, &tv) < 0)
               return -1;

            RARCH_LOG("[netplay] Network is stalling at frame %u, count %u of %d ...\n",
                  netplay->run_frame_count, netplay->timeout_cnt, MAX_RETRIES);

            if (netplay->timeout_cnt >= MAX_RETRIES && !netplay->remote_paused)
               return -1;
         }
      }
   } while (had_input || block);

   return 0;
}

/**
 * netplay_handle_slaves
 *
 * Handle any slave connections
 */
void netplay_handle_slaves(netplay_t *netplay)
{
   struct delta_frame *oframe, *frame = &netplay->buffer[netplay->self_ptr];
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active &&
          connection->mode == NETPLAY_CONNECTION_SLAVE)
      {
         uint32_t devices, device;
         uint32_t client_num = (uint32_t)(i + 1);

         /* This is a slave connection. First, should we do anything at all? If
          * we've already "read" this data, then we can just ignore it */
         if (netplay->read_frame_count[client_num] > netplay->self_frame_count)
            continue;

         /* Alright, we have to send something. Do we need to generate it first? */
         if (!frame->have_real[client_num])
         {
            devices = netplay->client_devices[client_num];

            /* Copy the previous frame's data */
            oframe = &netplay->buffer[PREV_PTR(netplay->self_ptr)];
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               netplay_input_state_t istate_out, istate_in;
               if (!(devices & (1<<device)))
                  continue;
               istate_in = oframe->real_input[device];
               while (istate_in && istate_in->client_num != client_num)
                  istate_in = istate_in->next;
               if (!istate_in)
               {
                  /* Start with blank input */
                  netplay_input_state_for(&frame->real_input[device],
                        client_num,
                        netplay_expected_input_size(netplay, 1 << device), true,
                        false);

               }
               else
               {
                  /* Copy the previous input */
                  istate_out = netplay_input_state_for(&frame->real_input[device],
                        client_num, istate_in->size, true, false);
                  memcpy(istate_out->data, istate_in->data,
                        istate_in->size * sizeof(uint32_t));
               }
            }
            frame->have_real[client_num] = true;
         }

         /* Send it along */
         send_input_frame(netplay, frame, NULL, NULL, client_num, false);

         /* And mark it as "read" */
         netplay->read_ptr[client_num] = NEXT_PTR(netplay->self_ptr);
         netplay->read_frame_count[client_num] = netplay->self_frame_count + 1;
      }
   }
}

/**
 * netplay_announce_nat_traversal
 *
 * Announce successful NAT traversal.
 */
void netplay_announce_nat_traversal(netplay_t *netplay)
{
#ifndef HAVE_SOCKET_LEGACY
   char msg[4200], host[PATH_MAX_LENGTH], port[6];

   if (netplay->nat_traversal_state.have_inet4)
   {
      if (getnameinfo((const struct sockaddr *) &netplay->nat_traversal_state.ext_inet4_addr,
               sizeof(struct sockaddr_in),
               host, PATH_MAX_LENGTH, port, 6, NI_NUMERICHOST|NI_NUMERICSERV) != 0)
         return;

   }
#ifdef HAVE_INET6
   else if (netplay->nat_traversal_state.have_inet6)
   {
      if (getnameinfo((const struct sockaddr *) &netplay->nat_traversal_state.ext_inet6_addr,
               sizeof(struct sockaddr_in6),
               host, PATH_MAX_LENGTH, port, 6, NI_NUMERICHOST|NI_NUMERICSERV) != 0)
         return;

   }
#endif
   else
   {
      snprintf(msg, sizeof(msg), "%s\n",
            msg_hash_to_str(MSG_UPNP_FAILED));
      runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("[netplay] %s\n", msg);
      return;
   }

   snprintf(msg, sizeof(msg), "%s: %s:%s\n",
         msg_hash_to_str(MSG_PUBLIC_ADDRESS),
         host, port);
   runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[netplay] %s\n", msg);
#endif
}

/**
 * netplay_init_nat_traversal
 *
 * Initialize the NAT traversal library and try to open a port
 */
void netplay_init_nat_traversal(netplay_t *netplay)
{
   memset(&netplay->nat_traversal_state, 0, sizeof(netplay->nat_traversal_state));
   netplay->nat_traversal_task_oustanding = true;
   task_push_netplay_nat_traversal(&netplay->nat_traversal_state, netplay->tcp_port);
}

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
      try_wildcard:
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
   {
#ifdef HAVE_INET6
      if (!direct_host && (hints.ai_family == AF_INET6))
         goto try_wildcard;
#endif
      RARCH_ERR("Failed to set up netplay sockets.\n");
   }

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

static bool netplay_init_serialization(netplay_t *netplay)
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
   netplay->is_connected         = false;
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

/**
 * netplay_send_savestate
 * @netplay              : pointer to netplay object
 * @serial_info          : the savestate being loaded
 * @cx                   : compression type
 * @z                    : compression backend to use
 *
 * Send a loaded savestate to those connected peers using the given compression
 * scheme.
 */
static void netplay_send_savestate(netplay_t *netplay,
   retro_ctx_serialize_info_t *serial_info, uint32_t cx,
   struct compression_transcoder *z)
{
   uint32_t header[4];
   uint32_t rd, wn;
   size_t i;

   /* Compress it */
   z->compression_backend->set_in(z->compression_stream,
      (const uint8_t*)serial_info->data_const, (uint32_t)serial_info->size);
   z->compression_backend->set_out(z->compression_stream,
      netplay->zbuffer, (uint32_t)netplay->zbuffer_size);
   if (!z->compression_backend->trans(z->compression_stream, true, &rd,
         &wn, NULL))
   {
      /* Catastrophe! */
      for (i = 0; i < netplay->connections_size; i++)
         netplay_hangup(netplay, &netplay->connections[i]);
      return;
   }

   /* Send it to relevant peers */
   header[0] = htonl(NETPLAY_CMD_LOAD_SAVESTATE);
   header[1] = htonl(wn + 2*sizeof(uint32_t));
   header[2] = htonl(netplay->run_frame_count);
   header[3] = htonl(serial_info->size);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (!connection->active ||
          connection->mode < NETPLAY_CONNECTION_CONNECTED ||
          connection->compression_supported != cx) continue;

      if (!netplay_send(&connection->send_packet_buffer, connection->fd, header,
            sizeof(header)) ||
          !netplay_send(&connection->send_packet_buffer, connection->fd,
            netplay->zbuffer, wn))
         netplay_hangup(netplay, connection);
   }
}

void netplay_frontend_paused(netplay_t *netplay, bool paused)
{
   size_t i;
   uint32_t paused_ct    = 0;

   netplay->local_paused = paused;

   /* Communicating this is a bit odd: If exactly one other connection is
    * paused, then we must tell them that we're unpaused, as from their
    * perspective we are. If more than one other connection is paused, then our
    * status as proxy means we are NOT unpaused to either of them. */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->paused)
         paused_ct++;
   }

   if (paused_ct > 1)
      return;

   /* Send our unpaused status. Must send manually because we must immediately
    * flush the buffer: If we're paused, we won't be polled. */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (     connection->active
            && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
      {
         if (paused)
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_PAUSE,
               netplay->nick, NETPLAY_NICK_LEN);
         else
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_RESUME,
               NULL, 0);

         /* We're not going to be polled, so we need to
          * flush this command now */
         netplay_send_flush(&connection->send_packet_buffer,
               connection->fd, true);
      }
   }
}

/**
 * netplay_force_future
 * @netplay              : pointer to netplay object
 *
 * Force netplay to ignore all past input, typically because we've just loaded
 * a state or reset.
 */
static void netplay_force_future(netplay_t *netplay)
{
   /* Wherever we're inputting, that's where we consider our state to be loaded */
   netplay->run_ptr         = netplay->self_ptr;
   netplay->run_frame_count = netplay->self_frame_count;

   /* We need to ignore any intervening data from the other side,
    * and never rewind past this */
   netplay_update_unread_ptr(netplay);

   if (netplay->unread_frame_count < netplay->run_frame_count)
   {
      uint32_t client;
      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(netplay->connected_players & (1 << client)))
            continue;

         if (netplay->read_frame_count[client] < netplay->run_frame_count)
         {
            netplay->read_ptr[client] = netplay->run_ptr;
            netplay->read_frame_count[client] = netplay->run_frame_count;
         }
      }
      if (netplay->server_frame_count < netplay->run_frame_count)
      {
         netplay->server_ptr = netplay->run_ptr;
         netplay->server_frame_count = netplay->run_frame_count;
      }
      netplay_update_unread_ptr(netplay);
   }
   if (netplay->other_frame_count < netplay->run_frame_count)
   {
      netplay->other_ptr = netplay->run_ptr;
      netplay->other_frame_count = netplay->run_frame_count;
   }
}


void netplay_core_reset(netplay_t *netplay)
{
   size_t i;
   uint32_t cmd[3];

   /* Ignore past input */
   netplay_force_future(netplay);

   /* Request that our peers reset */
   cmd[0] = htonl(NETPLAY_CMD_RESET);
   cmd[1] = htonl(sizeof(uint32_t));
   cmd[2] = htonl(netplay->self_frame_count);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (!connection->active ||
            connection->mode < NETPLAY_CONNECTION_CONNECTED) continue;

      if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmd,
               sizeof(cmd)))
         netplay_hangup(netplay, connection);
   }
}

void netplay_load_savestate(netplay_t *netplay,
      retro_ctx_serialize_info_t *serial_info, bool save)
{
   retro_ctx_serialize_info_t tmp_serial_info;

   netplay_force_future(netplay);

   /* Record it in our own buffer */
   if (save || !serial_info)
   {
      /* TODO/FIXME: This is a critical failure! */
      if (!netplay_delta_frame_ready(netplay,
               &netplay->buffer[netplay->run_ptr], netplay->run_frame_count))
         return;

      if (!serial_info)
      {
         tmp_serial_info.size = netplay->state_size;
         tmp_serial_info.data = netplay->buffer[netplay->run_ptr].state;
         if (!core_serialize(&tmp_serial_info))
            return;
         tmp_serial_info.data_const = tmp_serial_info.data;
         serial_info = &tmp_serial_info;
      }
      else
      {
         if (serial_info->size <= netplay->state_size)
            memcpy(netplay->buffer[netplay->run_ptr].state,
                  serial_info->data_const, serial_info->size);
      }
   }

   /* Don't send it if we're expected to be desynced */
   if (netplay->desync)
      return;

   /* If we can't send it to the peer, loading a state was a bad idea */
   if (netplay->quirks & (
              NETPLAY_QUIRK_NO_SAVESTATES
            | NETPLAY_QUIRK_NO_TRANSMISSION))
      return;

   /* Send this to every peer */
   if (netplay->compress_nil.compression_backend)
      netplay_send_savestate(netplay, serial_info, 0, &netplay->compress_nil);
   if (netplay->compress_zlib.compression_backend)
      netplay_send_savestate(netplay, serial_info, NETPLAY_COMPRESSION_ZLIB,
         &netplay->compress_zlib);
}

void netplay_toggle_play_spectate(netplay_t *netplay)
{
   switch (netplay->self_mode)
   {
      case NETPLAY_CONNECTION_PLAYING:
      case NETPLAY_CONNECTION_SLAVE:
         /* Switch to spectator mode immediately */
         netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
         netplay_cmd_mode(netplay, NETPLAY_CONNECTION_SPECTATING);
         break;
      case NETPLAY_CONNECTION_SPECTATING:
         /* Switch only after getting permission */
         netplay_cmd_mode(netplay, NETPLAY_CONNECTION_PLAYING);
         break;
      default:
         break;
   }
}

int16_t netplay_input_state(netplay_t *netplay,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   struct delta_frame *delta;
   netplay_input_state_t istate;
   const uint32_t *curr_input_state = NULL;
   size_t ptr                       =
      netplay->is_replay
      ? netplay->replay_ptr
      : netplay->run_ptr;

   if (port >= MAX_INPUT_DEVICES)
      return 0;

   /* If the port doesn't seem to correspond to the device, "correct" it. This
    * is common with devices that typically only have one instance, such as
    * keyboards, mice and lightguns. */
   if (device != RETRO_DEVICE_JOYPAD &&
       (netplay->config_devices[port]&RETRO_DEVICE_MASK) != device)
   {
      for (port = 0; port < MAX_INPUT_DEVICES; port++)
      {
         if ((netplay->config_devices[port]&RETRO_DEVICE_MASK) == device)
            break;
      }
      if (port == MAX_INPUT_DEVICES)
         return 0;
   }

   delta  = &netplay->buffer[ptr];
   istate = delta->resolved_input[port];
   if (!istate || !istate->used || istate->size == 0)
      return 0;

   curr_input_state = istate->data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return curr_input_state[0];
         return ((1 << id) & curr_input_state[0]) ? 1 : 0;

      case RETRO_DEVICE_ANALOG:
         if (istate->size == 3)
         {
            uint32_t state = curr_input_state[1 + idx];
            return (int16_t)(uint16_t)(state >> (id * 16));
         }
         break;
      case RETRO_DEVICE_MOUSE:
      case RETRO_DEVICE_LIGHTGUN:
         if (istate->size == 2)
         {
            if (id <= RETRO_DEVICE_ID_MOUSE_Y)
               return (int16_t)(uint16_t)(curr_input_state[1] >> (id * 16));
            return ((1 << id) & curr_input_state[0]) ? 1 : 0;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         {
            unsigned key = netplay_key_hton(id);
            if (key != NETPLAY_KEY_UNKNOWN)
            {
               unsigned word = key / 32;
               unsigned bit  = key % 32;
               if (word <= istate->size)
                  return ((UINT32_C(1) << bit) & curr_input_state[word]) ? 1 : 0;
            }
         }
         break;
      default:
         break;
   }

   return 0;
}

/**
 * get_self_input_state:
 * @netplay              : pointer to netplay object
 *
 * Grab our own input state and send this frame's input state (self and remote)
 * over the network
 *
 * Returns: true (1) if successful, otherwise false (0).
 */
static bool get_self_input_state(
      bool block_libretro_input,
      netplay_t *netplay)
{
   unsigned i;
   struct delta_frame *ptr        = &netplay->buffer[netplay->self_ptr];
   netplay_input_state_t istate   = NULL;
   uint32_t devices, used_devices = 0, devi, dev_type, local_device;

   if (!netplay_delta_frame_ready(netplay, ptr, netplay->self_frame_count))
      return false;

   /* We've already read this frame! */
   if (ptr->have_local)
      return true;

   devices      = netplay->self_devices;
   used_devices = 0;

   for (devi = 0; devi < MAX_INPUT_DEVICES; devi++)
   {
      if (!(devices & (1 << devi)))
         continue;

      /* Find an appropriate local device */
      dev_type = netplay->config_devices[devi]&RETRO_DEVICE_MASK;

      for (local_device = 0; local_device < MAX_INPUT_DEVICES; local_device++)
      {
         if (used_devices & (1 << local_device))
            continue;
         if ((netplay->config_devices[local_device]&RETRO_DEVICE_MASK) == dev_type)
            break;
      }

      if (local_device == MAX_INPUT_DEVICES)
         local_device = 0;
      used_devices |= (1 << local_device);

      istate = netplay_input_state_for(&ptr->real_input[devi],
            /* If we're a slave, we write our own input to MAX_CLIENTS to keep it separate */
            (netplay->self_mode==NETPLAY_CONNECTION_SLAVE)?MAX_CLIENTS:netplay->self_client_num,
            netplay_expected_input_size(netplay, 1 << devi),
            true, false);
      if (!istate)
         continue; /* FIXME: More severe? */

      /* First frame we always give zero input since relying on
       * input from first frame screws up when we use -F 0. */
      if (     !block_libretro_input
            &&  netplay->self_frame_count > 0)
      {
         uint32_t *state        = istate->data;
         retro_input_state_t cb = netplay->cbs.state_cb;
         unsigned dtype         = netplay->config_devices[devi]&RETRO_DEVICE_MASK;

         switch (dtype)
         {
            case RETRO_DEVICE_ANALOG:
               for (i = 0; i < 2; i++)
               {
                  int16_t tmp_x = cb(local_device,
                        RETRO_DEVICE_ANALOG, (unsigned)i, 0);
                  int16_t tmp_y = cb(local_device,
                        RETRO_DEVICE_ANALOG, (unsigned)i, 1);
                  state[1 + i] = (uint16_t)tmp_x | (((uint16_t)tmp_y) << 16);
               }
               /* no break */

            case RETRO_DEVICE_JOYPAD:
               for (i = 0; i <= RETRO_DEVICE_ID_JOYPAD_R3; i++)
               {
                  int16_t tmp = cb(local_device,
                        RETRO_DEVICE_JOYPAD, 0, (unsigned)i);
                  state[0] |= tmp ? 1 << i : 0;
               }
               break;

            case RETRO_DEVICE_MOUSE:
            case RETRO_DEVICE_LIGHTGUN:
            {
               int16_t tmp_x = cb(local_device, dtype, 0, 0);
               int16_t tmp_y = cb(local_device, dtype, 0, 1);
               state[1] = (uint16_t)tmp_x | (((uint16_t)tmp_y) << 16);
               for (i = 2;
                     i <= (unsigned)((dtype == RETRO_DEVICE_MOUSE) ?
                           RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN :
                           RETRO_DEVICE_ID_LIGHTGUN_START);
                     i++)
               {
                  int16_t tmp = cb(local_device, dtype, 0,
                        (unsigned) i);
                  state[0] |= tmp ? 1 << i : 0;
               }
               break;
            }

            case RETRO_DEVICE_KEYBOARD:
            {
               unsigned key, word = 0, bit = 1;
               for (key = 1; key < NETPLAY_KEY_LAST; key++)
               {
                  state[word] |=
                        cb(local_device, RETRO_DEVICE_KEYBOARD, 0,
                              NETPLAY_KEY_NTOH(key)) ?
                              (UINT32_C(1) << bit) : 0;
                  bit++;
                  if (bit >= 32)
                  {
                     bit = 0;
                     word++;
                     if (word >= istate->size)
                        break;
                  }
               }
               break;
            }
         }
      }
   }

   ptr->have_local = true;
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
   {
      ptr->have_real[netplay->self_client_num] = true;
      netplay->read_ptr[netplay->self_client_num] = NEXT_PTR(netplay->self_ptr);
      netplay->read_frame_count[netplay->self_client_num] = netplay->self_frame_count + 1;
   }

   /* And send this input to our peers */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active && connection->mode >= NETPLAY_CONNECTION_CONNECTED)
         netplay_send_cur_input(netplay, &netplay->connections[i]);
   }

   /* Handle any delayed state changes */
   if (netplay->is_server)
      netplay_delayed_state_change(netplay);

   return true;
}


bool netplay_poll(
      bool block_libretro_input,
      void *settings_data,
      netplay_t *netplay)
{
   size_t i;
   int res;
   uint32_t client;
   settings_t *settings = (settings_t*)settings_data;

   if (!get_self_input_state(block_libretro_input, netplay))
      goto catastrophe;

   /* If we're not connected, we're done */
   if (netplay->self_mode == NETPLAY_CONNECTION_NONE)
      return true;

   /* Read Netplay input, block if we're configured to stall for input every
    * frame */
   netplay_update_unread_ptr(netplay);
   if (netplay->stateless_mode &&
       (netplay->connected_players>1) &&
       netplay->unread_frame_count <= netplay->run_frame_count)
      res = netplay_poll_net_input(netplay, true);
   else
      res = netplay_poll_net_input(netplay, false);
   if (res == -1)
      goto catastrophe;

   /* Resolve and/or simulate the input if we don't have real input */
   netplay_resolve_input(netplay, netplay->run_ptr, false);

   /* Handle any slaves */
   if (netplay->is_server && netplay->connected_slaves)
      netplay_handle_slaves(netplay);

   netplay_update_unread_ptr(netplay);

   /* Figure out how many frames of input latency we should be using to hide
    * network latency */
   if (netplay->frame_run_time_avg || netplay->stateless_mode)
   {
      /* FIXME: Using fixed 60fps for this calculation */
      unsigned frames_per_frame    = netplay->frame_run_time_avg ?
         (16666 / netplay->frame_run_time_avg) :
         0;
      unsigned frames_ahead        = (netplay->run_frame_count > netplay->unread_frame_count) ?
         (netplay->run_frame_count - netplay->unread_frame_count) :
         0;
      int input_latency_frames_min = settings->uints.netplay_input_latency_frames_min -
            (settings->bools.run_ahead_enabled ? settings->uints.run_ahead_frames : 0);
      int input_latency_frames_max = input_latency_frames_min + settings->uints.netplay_input_latency_frames_range;

      /* Assume we need a couple frames worth of time to actually run the
       * current frame */
      if (frames_per_frame > 2)
         frames_per_frame -= 2;
      else
         frames_per_frame = 0;

      /* Shall we adjust our latency? */
      if (netplay->stateless_mode)
      {
         /* In stateless mode, we adjust up if we're "close" and down if we
          * have a lot of slack */
         if (netplay->input_latency_frames < input_latency_frames_min ||
               (netplay->unread_frame_count == netplay->run_frame_count + 1 &&
                netplay->input_latency_frames < input_latency_frames_max))
            netplay->input_latency_frames++;
         else if (netplay->input_latency_frames > input_latency_frames_max ||
               (netplay->unread_frame_count > netplay->run_frame_count + 2 &&
                netplay->input_latency_frames > input_latency_frames_min))
            netplay->input_latency_frames--;
      }
      else if (netplay->input_latency_frames < input_latency_frames_min ||
               (frames_per_frame < frames_ahead &&
                netplay->input_latency_frames < input_latency_frames_max))
      {
         /* We can't hide this much network latency with replay, so hide some
          * with input latency */
         netplay->input_latency_frames++;
      }
      else if (netplay->input_latency_frames > input_latency_frames_max ||
               (frames_per_frame > frames_ahead + 2 &&
                netplay->input_latency_frames > input_latency_frames_min))
      {
         /* We don't need this much latency (any more) */
         netplay->input_latency_frames--;
      }
   }

   /* If we're stalled, consider unstalling */
   switch (netplay->stall)
   {
      case NETPLAY_STALL_RUNNING_FAST:
         if (netplay->unread_frame_count + NETPLAY_MAX_STALL_FRAMES - 2
               > netplay->self_frame_count)
         {
            netplay->stall = NETPLAY_STALL_NONE;
            for (i = 0; i < netplay->connections_size; i++)
            {
               struct netplay_connection *connection = &netplay->connections[i];
               if (connection->active && connection->stall)
                  connection->stall = NETPLAY_STALL_NONE;
            }
         }
         break;

      case NETPLAY_STALL_SPECTATOR_WAIT:
         if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING || netplay->unread_frame_count > netplay->self_frame_count)
            netplay->stall = NETPLAY_STALL_NONE;
         break;

      case NETPLAY_STALL_INPUT_LATENCY:
         /* Just let it recalculate momentarily */
         netplay->stall = NETPLAY_STALL_NONE;
         break;

      case NETPLAY_STALL_SERVER_REQUESTED:
         /* See if the stall is done */
         if (netplay->connections[0].stall_frame == 0)
         {
            /* Stop stalling! */
            netplay->connections[0].stall = NETPLAY_STALL_NONE;
            netplay->stall = NETPLAY_STALL_NONE;
         }
         else
            netplay->connections[0].stall_frame--;
         break;
      case NETPLAY_STALL_NO_CONNECTION:
         /* We certainly haven't fixed this */
         break;
      default: /* not stalling */
         break;
   }

   /* If we're not stalled, consider stalling */
   if (!netplay->stall)
   {
      /* Have we not read enough latency frames? */
      if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING &&
          netplay->connected_players &&
          netplay->run_frame_count + netplay->input_latency_frames > netplay->self_frame_count)
      {
         netplay->stall = NETPLAY_STALL_INPUT_LATENCY;
         netplay->stall_time = 0;
      }

      /* Are we too far ahead? */
      if (netplay->unread_frame_count + NETPLAY_MAX_STALL_FRAMES
            <= netplay->self_frame_count)
      {
         netplay->stall      = NETPLAY_STALL_RUNNING_FAST;
         netplay->stall_time = cpu_features_get_time_usec();

         /* Figure out who to blame */
         if (netplay->is_server)
         {
            for (client = 1; client < MAX_CLIENTS; client++)
            {
               struct netplay_connection *connection;
               if (!(netplay->connected_players & (1 << client)))
                  continue;
               if (netplay->read_frame_count[client] > netplay->unread_frame_count)
                  continue;
               connection = &netplay->connections[client-1];
               if (connection->active &&
                   connection->mode == NETPLAY_CONNECTION_PLAYING)
               {
                  connection->stall = NETPLAY_STALL_RUNNING_FAST;
                  connection->stall_time = netplay->stall_time;
               }
            }
         }

      }

      /* If we're a spectator, are we ahead at all? */
      if (!netplay->is_server &&
          (netplay->self_mode == NETPLAY_CONNECTION_SPECTATING ||
           netplay->self_mode == NETPLAY_CONNECTION_SLAVE) &&
          netplay->unread_frame_count <= netplay->self_frame_count)
      {
         netplay->stall = NETPLAY_STALL_SPECTATOR_WAIT;
         netplay->stall_time = cpu_features_get_time_usec();
      }
   }

   /* If we're stalling, consider disconnection */
   if (netplay->stall && netplay->stall_time)
   {
      retro_time_t now = cpu_features_get_time_usec();

      /* Don't stall out while they're paused */
      if (netplay->remote_paused)
         netplay->stall_time = now;
      else if (now - netplay->stall_time >=
               (netplay->is_server ? MAX_SERVER_STALL_TIME_USEC :
                                          MAX_CLIENT_STALL_TIME_USEC))
      {
         /* Stalled out! */
         if (netplay->is_server)
         {
            bool fixed = false;
            for (i = 0; i < netplay->connections_size; i++)
            {
               struct netplay_connection *connection = &netplay->connections[i];
               if (connection->active &&
                   connection->mode == NETPLAY_CONNECTION_PLAYING &&
                   connection->stall)
               {
                  netplay_hangup(netplay, connection);
                  fixed = true;
               }
            }

            if (fixed)
            {
               /* Not stalled now :) */
               netplay->stall = NETPLAY_STALL_NONE;
               return true;
            }
         }
         else
            goto catastrophe;
         return false;
      }
   }

   return true;

catastrophe:
   for (i = 0; i < netplay->connections_size; i++)
      netplay_hangup(netplay, &netplay->connections[i]);
   return false;
}

bool netplay_is_alive(netplay_t *netplay)
{
   return (netplay->is_server) ||
          (!netplay->is_server &&
           netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED);
}

bool netplay_should_skip(netplay_t *netplay)
{
   if (!netplay)
      return false;
   return netplay->is_replay
      && (netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED);
}

void netplay_post_frame(netplay_t *netplay)
{
   size_t i;

   netplay_update_unread_ptr(netplay);
   netplay_sync_post_frame(netplay, false);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->active &&
          !netplay_send_flush(&connection->send_packet_buffer, connection->fd,
            false))
         netplay_hangup(netplay, connection);
   }

}
