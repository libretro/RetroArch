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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_timers.h>
#include <time.h>

#include <math/float_minmax.h>
#include <string/stdstring.h>
#include <net/net_socket.h>
#include <net/net_http.h>
#include <file/file_path.h>
#include <encodings/crc32.h>
#include <encodings/base64.h>
#include <features/features_cpu.h>
#include <lrc_hash.h>

#ifdef HAVE_IFINFO
#include <net/net_ifinfo.h>
#endif

#include "../../autosave.h"
#include "../../configuration.h"
#include "../../command.h"
#include "../../content.h"
#include "../../core.h"
#include "../../driver.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../version.h"
#include "../../verbosity.h"

#include "../../tasks/tasks_internal.h"
#include "../../input/input_driver.h"

#ifdef HAVE_MENU
#include "../../menu/menu_input.h"
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../../gfx/gfx_widgets.h"
#endif

#ifdef HAVE_PRESENCE
#include "../presence.h"
#endif

#ifdef HAVE_DISCORD
#include "../discord.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#include "netplay_private.h"

#ifdef TCP_NODELAY
#define SET_TCP_NODELAY(fd) \
   { \
      int on = 1; \
      if (setsockopt((fd), IPPROTO_TCP, TCP_NODELAY, \
            (const char *) &on, sizeof(on)) < 0) \
         RARCH_WARN("[Netplay] Could not set netplay TCP socket to nodelay. Expect jitter.\n"); \
   }
#else
#define SET_TCP_NODELAY(fd)
#endif

#if defined(F_SETFD) && defined(FD_CLOEXEC)
#define SET_FD_CLOEXEC(fd) \
   if (fcntl((fd), F_SETFD, FD_CLOEXEC) < 0) \
      RARCH_WARN("[Netplay] Cannot set netplay port to close-on-exec. It may fail to reopen.\n");
#else
#define SET_FD_CLOEXEC(fd)
#endif

#define RECV(buf, sz) \
   recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), (sz)); \
   if (recvd >= 0) \
   { \
      if (recvd < (ssize_t) (sz)) \
      { \
         netplay_recv_reset(&connection->recv_packet_buffer); \
         return true; \
      } \
   } \
   else

#define STRING_SAFE(str, sz) (str)[(sz) - 1] = '\0'

#define SET_PING(connection) \
   ping = (int32_t)((cpu_features_get_time_usec() - (connection)->ping_timer) / 1000); \
   if ((connection)->ping < 0 || ping < (connection)->ping) \
      (connection)->ping = ping;

#define REQUIRE_PROTOCOL_VERSION(connection, version) \
   if ((connection)->netplay_protocol >= (version))

#define REQUIRE_PROTOCOL_RANGE(connection, vmin, vmax) \
   if ((connection)->netplay_protocol >= (vmin) && \
         (connection)->netplay_protocol <= (vmax))

#define NETPLAY_MAGIC 0x52414E50 /* RANP */
#define FULL_MAGIC    0x46554C4C /* FULL */
#define POKE_MAGIC    0x504F4B45 /* POKE */
#define BANNED_MAGIC  0x44454E59 /* DENY */

/* Discovery magics */
#define DISCOVERY_QUERY_MAGIC    0x52414E51 /* RANQ */
#define DISCOVERY_RESPONSE_MAGIC 0x52414E53 /* RANS */

/* MITM magics */
#define MITM_SESSION_MAGIC 0x52415453 /* RATS */
#define MITM_LINK_MAGIC    0x5241544C /* RATL */
#define MITM_ADDR_MAGIC    0x52415441 /* RATA */
#define MITM_PING_MAGIC    0x52415450 /* RATP */

#if 0
/* Activate this to enable assertions on code sections
 * that should be exclusive to one modus */
#include <assert.h>
#define NETPLAY_ASSERT_MODUS(m) assert(networking_driver_st.data->modus==(m));
#else
#define NETPLAY_ASSERT_MODUS(m)
#endif

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

struct ad_packet
{
   uint32_t header;
   int32_t  content_crc;
   int32_t  port;
   uint32_t has_password;
   char     nick[NETPLAY_NICK_LEN];
   char     frontend[NETPLAY_HOST_STR_LEN];
   char     core[NETPLAY_HOST_STR_LEN];
   char     core_version[NETPLAY_HOST_STR_LEN];
   char     retroarch_version[NETPLAY_HOST_STR_LEN];
   char     content[NETPLAY_HOST_LONGSTR_LEN];
   char     subsystem_name[NETPLAY_HOST_LONGSTR_LEN];
};

struct mode_payload
{
   uint32_t frame;
   uint32_t mode;
   uint32_t devices;
   uint8_t  share_modes[MAX_INPUT_DEVICES];
   char     nick[NETPLAY_NICK_LEN];
};

struct vote_count
{
   uint16_t votes[32];
};

const mitm_server_t netplay_mitm_server_list[NETPLAY_MITM_SERVERS] = {
   { "nyc",       MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1 },
   { "madrid",    MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2 },
   { "saopaulo",  MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3 },
   { "singapore", MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4 },
   { "chuncheon", MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_5 },
   { "custom",    MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM }
};

static net_driver_state_t networking_driver_st = {0};

net_driver_state_t *networking_state_get_ptr(void)
{
   return &networking_driver_st;
}

static bool netplay_build_savestate(netplay_t* netplay, retro_ctx_serialize_info_t* serial_info, bool force_capture_achievements);
static bool netplay_process_savestate(netplay_t* netplay, retro_ctx_serialize_info_t* serial_info);

/* Align to 8-byte boundary */
#define CONTENT_ALIGN_SIZE(size) ((((size) + 7) & ~7))
#define NETPLAYSTATE_VERSION 1
#define NETPLAYSTATE_MEM_BLOCK "MEM "
#define NETPLAYSTATE_CHEEVOS_BLOCK "ACHV"
#define NETPLAYSTATE_END_BLOCK "END "

#ifdef HAVE_NETPLAYDISCOVERY
/** Initialize Netplay discovery (client) */
bool init_netplay_discovery(void)
{
   struct addrinfo *addr      = NULL;
   net_driver_state_t *net_st = &networking_driver_st;
   int fd                     = socket_init((void**)&addr, 0, NULL,
      SOCKET_TYPE_DATAGRAM, AF_INET);
   bool ret                   = fd >= 0 && addr;

   if (ret)
   {
#ifdef HAVE_IFINFO
      net_ifinfo_best("223.255.255.255",
         &((struct sockaddr_in*)addr->ai_addr)->sin_addr, false);
#endif

#ifdef SO_BROADCAST
      /* Make it broadcastable */
      {
         int broadcast = 1;

         if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
               (const char*)&broadcast, sizeof(broadcast)) < 0)
            RARCH_WARN("[Discovery] Failed to set netplay discovery port to broadcast.\n");
      }
#endif

      net_st->lan_ad_client_fd = fd;

      ret = socket_bind(fd, addr) && socket_nonblock(fd);
   }
   if (!ret)
   {
      if (fd >= 0)
         socket_close(fd);

      net_st->lan_ad_client_fd = -1;

      RARCH_ERR("[Discovery] Failed to initialize netplay advertisement client socket.\n");
   }

   if (addr)
      freeaddrinfo_retro(addr);

#ifdef HAVE_NETPLAYDISCOVERY_NSNET
   netplay_mdns_start_discovery();
   return true;
#else
   return ret;
#endif
}

/** Deinitialize Netplay discovery (client) */
void deinit_netplay_discovery(void)
{
   net_driver_state_t *net_st = &networking_driver_st;

   if (net_st->lan_ad_client_fd >= 0)
   {
      socket_close(net_st->lan_ad_client_fd);
      net_st->lan_ad_client_fd = -1;
   }

#ifdef HAVE_NETPLAYDISCOVERY_NSNET
   netplay_mdns_finish_discovery(net_st);
#endif
}

static bool netplay_lan_ad_client_query(void)
{
   char port[6];
   uint32_t header;
   struct addrinfo *addr      = NULL;
   struct addrinfo hints      = {0};
   net_driver_state_t *net_st = &networking_driver_st;
   bool ret                   = false;

   /* Get the broadcast address (IPv4 only for now) */
   snprintf(port, sizeof(port), "%hu", (unsigned short)RARCH_DISCOVERY_PORT);
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags    = AI_NUMERICHOST | AI_NUMERICSERV;
   if (getaddrinfo_retro("255.255.255.255", port, &hints, &addr))
      return ret;
   if (!addr)
      return ret;

   /* Put together the request */
   header = htonl(DISCOVERY_QUERY_MAGIC);

   /* And send it off */
   if (sendto(net_st->lan_ad_client_fd, (char*)&header, sizeof(header), 0,
         addr->ai_addr, addr->ai_addrlen) == sizeof(header))
      ret = true;
   else
      RARCH_ERR("[Discovery] Failed to send netplay discovery query.\n");

   freeaddrinfo_retro(addr);

   return ret;
}

static bool netplay_lan_ad_client_response(void)
{
   size_t count;
   ssize_t ret;
   char address[16];
   struct ad_packet ad_packet_buffer;
   struct netplay_host *host;
   uint32_t has_password;
   net_driver_state_t *net_st = &networking_driver_st;

   /* Check for any ad queries */
   for (count = 0;;)
   {
      struct sockaddr_storage their_addr = {0};
      socklen_t addr_size                = sizeof(their_addr);

      ret = recvfrom(net_st->lan_ad_client_fd,
         (char*)&ad_packet_buffer, sizeof(ad_packet_buffer), 0,
         (struct sockaddr*)&their_addr, &addr_size);
      if (ret < 0)
         return isagain((int)ret) && count > 0;

      /* Somebody queried, so check that it's valid */
      if (ret != sizeof(ad_packet_buffer))
         continue;

      /* Make sure it's a valid response */
      if (ntohl(ad_packet_buffer.header) != DISCOVERY_RESPONSE_MAGIC)
         continue;

      /* And that we know how to handle it */
      if (!addr_6to4(&their_addr))
         continue;

      if (!ipv4_is_lan_address((struct sockaddr_in*)&their_addr))
         continue;

      if (getnameinfo_retro((struct sockaddr*)&their_addr, sizeof(their_addr),
            address, sizeof(address), NULL, 0, NI_NUMERICHOST))
         continue;

      /* Allocate space for it */
      if (net_st->discovered_hosts.size >= net_st->discovered_hosts.allocated)
      {
         if (!net_st->discovered_hosts.size)
         {
            net_st->discovered_hosts.hosts = (struct netplay_host*)
               malloc(sizeof(*net_st->discovered_hosts.hosts));
            if (!net_st->discovered_hosts.hosts)
               return false;
            net_st->discovered_hosts.allocated = 1;
         }
         else
         {
            size_t new_allocated = net_st->discovered_hosts.allocated + 4;
            struct netplay_host *new_hosts = (struct netplay_host*)realloc(
               net_st->discovered_hosts.hosts,
               new_allocated * sizeof(*new_hosts));

            if (!new_hosts)
            {
               free(net_st->discovered_hosts.hosts);
               memset(&net_st->discovered_hosts, 0,
                  sizeof(net_st->discovered_hosts));

               return false;
            }

            net_st->discovered_hosts.allocated = new_allocated;
            net_st->discovered_hosts.hosts     = new_hosts;
         }
      }

      /* Get our host structure */
      host = &net_st->discovered_hosts.hosts[net_st->discovered_hosts.size++];

      STRING_SAFE(ad_packet_buffer.nick, sizeof(ad_packet_buffer.nick));
      STRING_SAFE(ad_packet_buffer.frontend,
         sizeof(ad_packet_buffer.frontend));
      STRING_SAFE(ad_packet_buffer.core, sizeof(ad_packet_buffer.core));
      STRING_SAFE(ad_packet_buffer.core_version,
         sizeof(ad_packet_buffer.core_version));
      STRING_SAFE(ad_packet_buffer.retroarch_version,
         sizeof(ad_packet_buffer.retroarch_version));
      STRING_SAFE(ad_packet_buffer.content, sizeof(ad_packet_buffer.content));
      STRING_SAFE(ad_packet_buffer.subsystem_name,
         sizeof(ad_packet_buffer.subsystem_name));

      /* Copy in the response */
      host->content_crc = (int)ntohl(ad_packet_buffer.content_crc);
      host->port        = (int)ntohl(ad_packet_buffer.port);

      strlcpy(host->address, address, sizeof(host->address));
      strlcpy(host->nick, ad_packet_buffer.nick, sizeof(host->nick));
      strlcpy(host->frontend, ad_packet_buffer.frontend,
         sizeof(host->frontend));
      strlcpy(host->core, ad_packet_buffer.core, sizeof(host->core));
      strlcpy(host->core_version, ad_packet_buffer.core_version,
         sizeof(host->core_version));
      strlcpy(host->retroarch_version, ad_packet_buffer.retroarch_version,
         sizeof(host->retroarch_version));
      strlcpy(host->content, ad_packet_buffer.content, sizeof(host->content));
      strlcpy(host->subsystem_name, ad_packet_buffer.subsystem_name,
         sizeof(host->subsystem_name));

      has_password                = ntohl(ad_packet_buffer.has_password);
      host->has_password          = (has_password & 1) ? true : false;
      host->has_spectate_password = (has_password & 2) ? true : false;

      count++;
   }
}

/** Discovery control */
bool netplay_discovery_driver_ctl(
   enum rarch_netplay_discovery_ctl_state state, void *data)
{
   net_driver_state_t *net_st = &networking_driver_st;

   switch (state)
   {
      case RARCH_NETPLAY_DISCOVERY_CTL_LAN_SEND_QUERY:
         {
            bool rv = false;
            if (net_st->lan_ad_client_fd >= 0)
               rv = netplay_lan_ad_client_query();
#if HAVE_NETPLAYDISCOVERY_NSNET
            rv = true;
#endif
            return rv;
         }

      case RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES:
         return net_st->lan_ad_client_fd >= 0 &&
            netplay_lan_ad_client_response();

      case RARCH_NETPLAY_DISCOVERY_CTL_LAN_CLEAR_RESPONSES:
         net_st->discovered_hosts.size = 0;
         break;

      default:
         return false;
   }

   return true;
}

#ifndef VITA
/** Initialize Netplay discovery */
static bool init_lan_ad_server_socket(void)
{
   struct addrinfo *addr      = NULL;
   net_driver_state_t *net_st = &networking_driver_st;
   int fd                     = socket_init((void**)&addr, RARCH_DISCOVERY_PORT,
      NULL, SOCKET_TYPE_DATAGRAM, AF_INET);
   bool ret                   = fd >= 0 && addr &&
      socket_bind(fd, addr) && socket_nonblock(fd);

   if (ret)
      net_st->lan_ad_server_fd = fd;
   else
   {
      if (fd >= 0)
         socket_close(fd);

      net_st->lan_ad_server_fd = -1;

      RARCH_ERR("[Discovery] Failed to initialize netplay advertisement socket.\n");
   }

   if (addr)
      freeaddrinfo_retro(addr);

   return ret;
}
#endif

/** Deinitialize Netplay discovery */
static void deinit_lan_ad_server_socket(void)
{
   net_driver_state_t *net_st = &networking_driver_st;

   if (net_st->lan_ad_server_fd >= 0)
   {
      socket_close(net_st->lan_ad_server_fd);
      net_st->lan_ad_server_fd = -1;
   }
}

#ifndef VITA
/**
 * netplay_lan_ad_server
 *
 * Respond to any LAN ad queries that the netplay server has received.
 */
static bool netplay_lan_ad_server(netplay_t *netplay)
{
   uint32_t header;
   struct sockaddr_storage their_addr = {0};
   socklen_t addr_size                = sizeof(their_addr);
   net_driver_state_t *net_st         = &networking_driver_st;
   /* Check for any ad queries */
   ssize_t ret                        = recvfrom(net_st->lan_ad_server_fd,
      (char*)&header, sizeof(header), 0,
      (struct sockaddr*)&their_addr, &addr_size);

   if (ret < 0)
   {
      if (isagain((int)ret))
         return true;

      deinit_lan_ad_server_socket();

      return false;
   }

   /* Somebody queried, so check that it's valid */
   if (ret == sizeof(header))
   {
      char frontend_architecture_tmp[24];
      const frontend_ctx_driver_t *frontend_drv;
      uint32_t has_password              = 0;
      struct ad_packet ad_packet_buffer  = {0};
      struct retro_system_info *sysinfo  =
         &runloop_state_get_ptr()->system.info;
      struct string_list *subsystem      = path_get_subsystem_list();
      settings_t *settings               = config_get_ptr();

      /* Make sure it's a valid query */
      if (ntohl(header) != DISCOVERY_QUERY_MAGIC)
      {
         RARCH_WARN("[Discovery] Invalid query.\n");
         return true;
      }

      if (!addr_6to4(&their_addr))
         return true;

      if (!ipv4_is_lan_address((struct sockaddr_in*)&their_addr))
         return true;

      RARCH_LOG("[Discovery] Query received on LAN interface.\n");

      /* Now build our response */
      ad_packet_buffer.header = htonl(DISCOVERY_RESPONSE_MAGIC);
      ad_packet_buffer.port   = (int32_t)htonl(netplay->tcp_port);

      strlcpy(ad_packet_buffer.nick, netplay->nick,
         sizeof(ad_packet_buffer.nick));

      frontend_drv = (const frontend_ctx_driver_t*)
         frontend_driver_get_cpu_architecture_str(frontend_architecture_tmp,
            sizeof(frontend_architecture_tmp));
      if (frontend_drv)
      {
         size_t _len = strlcpy(ad_packet_buffer.frontend, frontend_drv->ident,
               sizeof(ad_packet_buffer.frontend));
         _len += strlcpy(ad_packet_buffer.frontend + _len,
               " ",
               sizeof(ad_packet_buffer.frontend)   - _len);
         strlcpy(ad_packet_buffer.frontend         + _len,
               frontend_architecture_tmp,
               sizeof(ad_packet_buffer.frontend)   - _len);
      }
      else
         strlcpy(ad_packet_buffer.frontend, "N/A",
            sizeof(ad_packet_buffer.frontend));

      strlcpy(ad_packet_buffer.core, sysinfo->library_name,
         sizeof(ad_packet_buffer.core));
      strlcpy(ad_packet_buffer.core_version, sysinfo->library_version,
         sizeof(ad_packet_buffer.core_version));

      strlcpy(ad_packet_buffer.retroarch_version, PACKAGE_VERSION,
         sizeof(ad_packet_buffer.retroarch_version));

      if (subsystem && subsystem->size > 0)
      {
         unsigned i = 0;

         for (;;)
         {
            strlcat(ad_packet_buffer.content,
               path_basename(subsystem->elems[i].data),
               sizeof(ad_packet_buffer.content));
            if (++i >= subsystem->size)
               break;
            strlcat(ad_packet_buffer.content, "|",
               sizeof(ad_packet_buffer.content));
         }

         strlcpy(ad_packet_buffer.subsystem_name,
            path_get(RARCH_PATH_SUBSYSTEM),
            sizeof(ad_packet_buffer.subsystem_name));

         ad_packet_buffer.content_crc = 0;
      }
      else
      {
         const char *basename = path_basename(path_get(RARCH_PATH_BASENAME));

         strlcpy(ad_packet_buffer.content,
            !string_is_empty(basename) ? basename : "N/A",
            sizeof(ad_packet_buffer.content));
         strlcpy(ad_packet_buffer.subsystem_name, "N/A",
            sizeof(ad_packet_buffer.subsystem_name));

         ad_packet_buffer.content_crc = (int32_t)htonl(content_get_crc());
      }

      if (!string_is_empty(settings->paths.netplay_password))
         has_password |= 1;
      if (!string_is_empty(settings->paths.netplay_spectate_password))
         has_password |= 2;
      ad_packet_buffer.has_password = htonl(has_password);

      /* Send our response */
      sendto(net_st->lan_ad_server_fd,
         (char*)&ad_packet_buffer, sizeof(ad_packet_buffer), 0,
         (struct sockaddr*)&their_addr, sizeof(their_addr));
   }

   return true;
}
#endif

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
   size_t _len                         = strlen(ver);

   for (i = 0; i < _len; i++)
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
      |(sizeof(size_t)  << 15)
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
   uint32_t ebit = (1 << 30);
   return (pma & ebit) != (pmb & ebit);
}

static int simple_rand(unsigned long *simple_rand_next)
{
   *simple_rand_next = *simple_rand_next * 1103515245 + 12345;
   return((unsigned)(*simple_rand_next / 65536) % 32768);
}

static uint32_t simple_rand_uint32(unsigned long *simple_rand_next)
{
   uint32_t part0 = simple_rand(simple_rand_next);
   uint32_t part1 = simple_rand(simple_rand_next);
   uint32_t part2 = simple_rand(simple_rand_next);
   return ((part0 << 30) + (part1 << 15) + part2);
}

static void netplay_send_cmd_netpacket(netplay_t *netplay, size_t conn_i,
      const void* buf, size_t len, uint16_t client_id);
static void RETRO_CALLCONV netplay_netpacket_send_cb(int flags,
      const void* buf, size_t len, uint16_t client_id);
static void RETRO_CALLCONV netplay_netpacket_poll_receive_cb(void);

/*
 * netplay_init_socket_buffer
 *
 * Initialize a new socket buffer.
 */
static bool netplay_init_socket_buffer(struct socket_buffer *sbuf, size_t len)
{
   sbuf->data  = (unsigned char*)malloc(len);
   if (!sbuf->data)
      return false;
   sbuf->bufsz = len;
   sbuf->start = sbuf->read = sbuf->end = 0;

   return true;
}

/**
 * netplay_deinit_socket_buffer
 *
 * Free a socket buffer.
 */
static void netplay_deinit_socket_buffer(struct socket_buffer *sbuf)
{
   free(sbuf->data);
   sbuf->data = NULL;
}


/**
 * netplay_handshake_init_send
 *
 * Initialize our handshake and send the first part of the handshake protocol.
 */
static bool netplay_handshake_init_send(netplay_t *netplay,
   struct netplay_connection *connection, uint32_t protocol)
{
   uint32_t header[6];
   settings_t *settings = config_get_ptr();

   header[0] = htonl(NETPLAY_MAGIC);
   header[1] = htonl(netplay_platform_magic());
   header[2] = htonl(NETPLAY_COMPRESSION_SUPPORTED);

   if (netplay->is_server)
   {
      if (     !string_is_empty(settings->paths.netplay_password)
            || !string_is_empty(settings->paths.netplay_spectate_password))
      {
         /* Demand a password */
         if (netplay->simple_rand_next == 1)
            netplay->simple_rand_next = (unsigned long) time(NULL);
         connection->salt = simple_rand_uint32(&netplay->simple_rand_next);
         if (!connection->salt)
            connection->salt = 1;
         header[3] = htonl(connection->salt);
      }
      else
         header[3] = 0;
   }
   else
   {
      /* HACK ALERT!!!
       * We need to do this in order to maintain full backwards compatibility.
       * Send our highest available protocol in the unused salt field.
       * Servers can then pick the best protocol choice for the client. */
      header[3] = htonl(HIGH_NETPLAY_PROTOCOL_VERSION);
   }

   header[4] = htonl(protocol);
   header[5] = htonl(netplay_impl_magic());

   /* First ping */
   connection->ping       = -1;
   connection->ping_timer = cpu_features_get_time_usec();

   if (   !netplay_send(&connection->send_packet_buffer,
            connection->fd, header,
            sizeof(header))
       || !netplay_send_flush(&connection->send_packet_buffer,
            connection->fd, false))
      return false;

   return true;
}

#ifdef HAVE_MENU
static void handshake_password(void *userdata, const char *line)
{
   size_t _len;
   struct password_buf_s password_buf;
   char password[8+NETPLAY_PASS_LEN]; /* 8 for salt, 128 for password */
   char hash[NETPLAY_PASS_HASH_LEN+1]; /* + NULL terminator */
   struct netplay_connection *connection;
   net_driver_state_t *net_st            = &networking_driver_st;
   netplay_t *netplay                    = net_st->data;

   if (!netplay)
      return;

   connection = &netplay->connections[0];
   _len       = snprintf(password, sizeof(password),
         "%08lX", (unsigned long)connection->salt);
   if (!string_is_empty(line))
      strlcpy(password       + _len,
            line,
            sizeof(password) - _len);

   password_buf.cmd[0] = htonl(NETPLAY_CMD_PASSWORD);
   password_buf.cmd[1] = htonl(sizeof(password_buf.password));
   sha256_hash(hash, (uint8_t *) password, strlen(password));
   memcpy(password_buf.password, hash, NETPLAY_PASS_HASH_LEN);

   /* We have no way to handle an error here, so we'll let the next function error out */
   if (netplay_send(&connection->send_packet_buffer, connection->fd, &password_buf, sizeof(password_buf)))
      netplay_send_flush(&connection->send_packet_buffer, connection->fd, false);

   menu_input_dialog_end();
   retroarch_menu_running_finished(false);
}
#endif

/**
 * netplay_handshake_nick
 *
 * Send a NICK command.
 */
static bool netplay_handshake_nick(netplay_t *netplay,
      struct netplay_connection *connection)
{
   struct nick_buf_s nick_buf = {0};

   /* Send our nick */
   nick_buf.cmd[0] = htonl(NETPLAY_CMD_NICK);
   nick_buf.cmd[1] = htonl(sizeof(nick_buf.nick));
   strlcpy(nick_buf.nick, netplay->nick, sizeof(nick_buf.nick));

   /* Second ping */
   connection->ping_timer = cpu_features_get_time_usec();
   if (    !netplay_send(&connection->send_packet_buffer,
            connection->fd, &nick_buf, sizeof(nick_buf))
         || !netplay_send_flush(&connection->send_packet_buffer,
            connection->fd, false))
      return false;
   return true;
}

static void send_info_and_disconnect(netplay_t *netplay,
      struct netplay_connection *connection)
{
   /* Send it our highest available protocol. */
   netplay_handshake_init_send(netplay, connection,
      HIGH_NETPLAY_PROTOCOL_VERSION);

   socket_close(connection->fd);
   connection->flags &= ~NETPLAY_CONN_FLAG_ACTIVE;
   netplay_deinit_socket_buffer(&connection->send_packet_buffer);
   netplay_deinit_socket_buffer(&connection->recv_packet_buffer);
}

static uint32_t select_protocol(uint32_t lo_protocol, uint32_t hi_protocol)
{
   if (!hi_protocol)
      /* Older clients don't send a high protocol. */
      return lo_protocol;
   else if (hi_protocol > HIGH_NETPLAY_PROTOCOL_VERSION)
      /* Run at our highest supported protocol. */
      return HIGH_NETPLAY_PROTOCOL_VERSION;
   /* Otherwise run at the client's highest supported protocol. */
   return hi_protocol;
}

static int select_compression(netplay_t *netplay, uint32_t compression)
{
   struct compression_transcoder *ctrans = NULL;
   int                           ret     = -1;

   compression &= NETPLAY_COMPRESSION_SUPPORTED;

   if (compression & NETPLAY_COMPRESSION_ZLIB)
   {
      ctrans = &netplay->compress_zlib;
      if (!ctrans->compression_backend)
         ctrans->compression_backend =
            trans_stream_get_zlib_deflate_backend();
      ret = NETPLAY_COMPRESSION_ZLIB;
   }
   else
   {
      ctrans = &netplay->compress_nil;
      if (!ctrans->compression_backend)
         ctrans->compression_backend =
            trans_stream_get_pipe_backend();
      ret = 0;
   }

   if (!ctrans->compression_backend)
      return -1;

   if (!ctrans->decompression_backend)
      ctrans->decompression_backend = ctrans->compression_backend->reverse;

   /* Allocate our compression stream */
   if (!ctrans->compression_stream)
      ctrans->compression_stream   = ctrans->compression_backend->stream_new();
   if (!ctrans->decompression_stream)
      ctrans->decompression_stream = ctrans->decompression_backend->stream_new();

   if (!ctrans->compression_stream || !ctrans->decompression_stream)
      return -1;

   return ret;
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
   ssize_t  recvd;
   int      compression;
   uint32_t header[6];
   uint32_t netplay_magic = 0;
   int32_t  ping          = 0;
   const char *_msg       = NULL;

   RECV(header, sizeof(header[0]))
   {
      if (netplay->is_server)
      {
         _msg = msg_hash_to_str(MSG_FAILED_TO_CONNECT_TO_CLIENT);
         RARCH_ERR("[Netplay] %s\n", _msg);
         return false;
      }
      else
      {
         _msg = msg_hash_to_str(MSG_FAILED_TO_CONNECT_TO_HOST);
         RARCH_ERR("[Netplay] %s\n", _msg);
         goto error;
      }
   }

   netplay_magic = ntohl(header[0]);

   if (netplay->is_server)
   {
      switch (netplay_magic)
      {
         case NETPLAY_MAGIC:
            break;
         case POKE_MAGIC:
            /* Poking the server for information? Just disconnect */
            send_info_and_disconnect(netplay, connection);
            return true;
         default:
            return false;
      }
   }
   else
   {
      /* Only the client is able to estimate latency at this point. */
      SET_PING(connection)

      switch (netplay_magic)
      {
         case NETPLAY_MAGIC:
            break;
         case FULL_MAGIC:
            _msg = msg_hash_to_str(MSG_NETPLAY_HOST_FULL);
            RARCH_ERR("[Netplay] %s\n", _msg);
            goto error;
         case BANNED_MAGIC:
            _msg = msg_hash_to_str(MSG_NETPLAY_BANNED);
            RARCH_ERR("[Netplay] %s\n", _msg);
            goto error;
         default:
            _msg = msg_hash_to_str(MSG_NETPLAY_NOT_RETROARCH);
            RARCH_ERR("[Netplay] %s\n", _msg);
            goto error;
      }
   }

   RECV(header + 1, sizeof(header) - sizeof(header[0]))
   {
      if (netplay->is_server)
      {
         _msg = msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT);
         RARCH_ERR("[Netplay] %s\n", _msg);
         return false;
      }
      else
      {
         _msg = msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST);
         RARCH_ERR("[Netplay] %s\n", _msg);
         goto error;
      }
   }

   /* HACK ALERT!!!
    * We need to do this in order to maintain full backwards compatibility.
    * If client sent a non zero salt, assume it's the highest supported protocol. */
   if (netplay->is_server)
   {
      connection->netplay_protocol = select_protocol(
         ntohl(header[4]),
         ntohl(header[3])
      );

      if (connection->netplay_protocol < LOW_NETPLAY_PROTOCOL_VERSION ||
            connection->netplay_protocol > HIGH_NETPLAY_PROTOCOL_VERSION)
      {
         /* Send it so that a proper notification can be shown there. */
         netplay_handshake_init_send(netplay, connection, 0);

         _msg = msg_hash_to_str(MSG_NETPLAY_OUT_OF_DATE);
         RARCH_ERR("[Netplay] %s\n", _msg);
         return false;
      }

      /* At this point,
         the server should send a header in order for the client
         to display proper error notifications/logs. */
      if (!netplay_handshake_init_send(netplay, connection,
            connection->netplay_protocol))
         return false;
   }
   else
   {
      /* Clients only see one protocol. */
      connection->netplay_protocol = ntohl(header[4]);

      if (connection->netplay_protocol < LOW_NETPLAY_PROTOCOL_VERSION ||
            connection->netplay_protocol > HIGH_NETPLAY_PROTOCOL_VERSION)
      {
         _msg = msg_hash_to_str(MSG_NETPLAY_OUT_OF_DATE);
         RARCH_ERR("[Netplay] %s\n", _msg);
         goto error;
      }
   }

   /* We only care about platform magic if our core is quirky */
   if (netplay->quirks & NETPLAY_QUIRK_PLATFORM_DEPENDENT)
   {
      if (ntohl(header[1]) != netplay_platform_magic())
      {
         _msg = msg_hash_to_str(MSG_NETPLAY_PLATFORM_DEPENDENT);
         RARCH_ERR("[Netplay] %s\n", _msg);

         if (netplay->is_server)
            return false;
         goto error;
      }
   }
   else if (netplay->quirks & NETPLAY_QUIRK_ENDIAN_DEPENDENT)
   {
      if (netplay_endian_mismatch(netplay_platform_magic(), ntohl(header[1])))
      {
         _msg = msg_hash_to_str(MSG_NETPLAY_ENDIAN_DEPENDENT);
         RARCH_ERR("[Netplay] %s\n", _msg);

         if (netplay->is_server)
            return false;
         goto error;
      }
   }

   if (ntohl(header[5]) != netplay_impl_magic())
   {
      settings_t *settings = config_get_ptr();

      /* We allow the connection but warn that this could cause issues. */
      _msg = msg_hash_to_str(MSG_NETPLAY_DIFFERENT_VERSIONS);
      RARCH_WARN("[Netplay] %s\n", _msg);
      if (!netplay->is_server && settings->bools.notification_show_netplay_extra)
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* Check what compression is supported */
   compression = select_compression(netplay, ntohl(header[2]));
   if (compression == -1)
      return false;
   connection->compression_supported = (uint32_t)compression;

   if (!netplay->is_server)
   {
      /* If a password is demanded, ask for it */
      if ((connection->salt = ntohl(header[3])))
      {
#ifdef HAVE_MENU
         menu_input_ctx_line_t line = {0};
         retroarch_menu_running();
         line.label         = msg_hash_to_str(MSG_NETPLAY_ENTER_PASSWORD);
         line.label_setting = "no_setting";
         line.cb            = handshake_password;
         if (!menu_input_dialog_start(&line))
            return false;
#endif
      }

      if (!netplay_handshake_nick(netplay, connection))
         return false;
   }

   /* Move on to the next mode */
   connection->mode = NETPLAY_CONNECTION_PRE_NICK;
   *had_input       = true;
   netplay_recv_flush(&connection->recv_packet_buffer);

   return true;

error:
   runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
      MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return false;
}

static void netplay_handshake_ready(netplay_t *netplay,
      struct netplay_connection *connection)
{
   size_t _len;
   char msg[512];
   settings_t *settings = config_get_ptr();

   if (netplay->is_server)
   {
      unsigned slot = (unsigned)(connection - netplay->connections);

      _len = snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
         connection->nick);

      RARCH_LOG("[Netplay] %s %u\n", msg_hash_to_str(MSG_CONNECTION_SLOT),
         slot);

      /* Send them the savestate */
      netplay->force_send_savestate = true;
   }
   else
   {
      _len  = strlcpy(msg, msg_hash_to_str(MSG_CONNECTED_TO),
            sizeof(msg));
      _len += snprintf(msg + _len, sizeof(msg) - _len, ": \"%s\"",
            connection->nick);
   }

   RARCH_LOG("[Netplay] %s\n", msg);
   /* Useful notification to the client in figuring out
      if a connection was successfully made before an error,
      but not as useful to the server.
      Let it be optional if server. */
   if (!netplay->is_server || settings->bools.notification_show_netplay_extra)
      runloop_msg_queue_push(msg, _len, 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/**
 * netplay_handshake_info
 *
 * Send an INFO command.
 */
static bool netplay_handshake_info(netplay_t *netplay,
      struct netplay_connection *connection)
{
   struct info_buf_s info_buf       = {0};
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;

   info_buf.cmd[0] = htonl(NETPLAY_CMD_INFO);
   info_buf.cmd[1] = htonl(sizeof(info_buf) - sizeof(info_buf.cmd));

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

   /* When using the netpacket interface, the core can override the version
    * with a custom string so multiple core versions can stay compatible. */
   if (networking_driver_st.core_netpacket_interface &&
         networking_driver_st.core_netpacket_interface->protocol_version)
   {
      strlcpy(info_buf.core_version,
            networking_driver_st.core_netpacket_interface->protocol_version,
            sizeof(info_buf.core_version));
   }

   info_buf.content_crc   = htonl(content_get_crc()); /* Get our content CRC */
   connection->ping_timer = cpu_features_get_time_usec(); /* Third ping */

   /* Send it off and wait for info back */
   if (  !netplay_send(&connection->send_packet_buffer,
            connection->fd, &info_buf, sizeof(info_buf))
       || !netplay_send_flush(&connection->send_packet_buffer,
            connection->fd, false))
      return false;

   return true;
}

static void select_nickname(netplay_t *netplay,
      struct netplay_connection *connection)
{
   size_t i;
   char nickname[NETPLAY_NICK_LEN];
   char suffix[8];
   int j = 1;

   strlcpy(nickname, connection->nick, sizeof(nickname));

try_next:
   /* Find an available nickname for this client. */
   if (string_is_equal(nickname, netplay->nick))
      /* Nickname conflict with host; try the next one. */
      goto gen_nick;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *conn = &netplay->connections[i];

      if (conn == connection)
         continue;
      if (!(conn->flags & NETPLAY_CONN_FLAG_ACTIVE))
         continue;
      if (conn->mode < NETPLAY_CONNECTION_CONNECTED)
         continue;

      if (string_is_equal(nickname, conn->nick))
         /* Nickname conflict with client; try the next one. */
         goto gen_nick;
   }

   /* Ensure that all unused bytes are NULL. */
   memset(connection->nick, 0, sizeof(connection->nick));

   /* Final nickname */
   strlcpy(connection->nick, nickname, sizeof(connection->nick));

   return;

gen_nick:
   strlcpy(nickname, connection->nick,
      sizeof(nickname) - snprintf(suffix, sizeof(suffix), " (%d)", ++j));
   strlcat(nickname, suffix, sizeof(nickname));

   goto try_next;
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
   uint32_t cmd[4];
   retro_ctx_memory_info_t mem_info;
   size_t sram_size    = 0;
   uint32_t client_num = (uint32_t)(connection - netplay->connections + 1);
   if (netplay->local_paused || netplay->remote_paused)
      client_num |= NETPLAY_CMD_SYNC_BIT_PAUSED;

   /* send sram unless running with netpacket interface */
   if (netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
   {
      mem_info.id = RETRO_MEMORY_SAVE_RAM;
#ifdef HAVE_THREADS
      autosave_lock();
#endif
      core_get_memory(&mem_info);
      sram_size = mem_info.size;
#ifdef HAVE_THREADS
      autosave_unlock();
#endif
   }

   /* Send basic sync info */
   cmd[0] = htonl(NETPLAY_CMD_SYNC);
   cmd[1] = htonl(2*sizeof(uint32_t)
      /* Controller devices */
      + MAX_INPUT_DEVICES*sizeof(uint32_t)
      /* Share modes */
      + MAX_INPUT_DEVICES*sizeof(uint8_t)
      /* Device-client mapping */
      + MAX_INPUT_DEVICES*sizeof(uint32_t)
      /* Client nick */
      + NETPLAY_NICK_LEN
      /* And finally, sram */
      + sram_size);
   cmd[2] = htonl(netplay->self_frame_count);
   cmd[3] = htonl(client_num);

   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
         cmd, sizeof(cmd)))
      return false;

   /* Now send the device info */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      uint32_t device = htonl(netplay->config_devices[i]);

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
      uint32_t device = htonl(netplay->device_clients[i]);

      if (!netplay_send(&connection->send_packet_buffer, connection->fd,
            &device, sizeof(device)))
         return false;
   }

   select_nickname(netplay, connection);

   /* Send the nick */
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
         connection->nick, sizeof(connection->nick)))
      return false;

   /* And finally, the SRAM */
   if (sram_size)
   {
      mem_info.id = RETRO_MEMORY_SAVE_RAM;
#ifdef HAVE_THREADS
      autosave_lock();
#endif
      core_get_memory(&mem_info);
      if (    !netplay_send(&connection->send_packet_buffer,
               connection->fd, mem_info.data, mem_info.size)
            || !netplay_send_flush(&connection->send_packet_buffer,
               connection->fd, false))
      {
#ifdef HAVE_THREADS
         autosave_unlock();
#endif
         return false;
      }
#ifdef HAVE_THREADS
      autosave_unlock();
#endif
   }

   /* Send our settings. */
   REQUIRE_PROTOCOL_VERSION(connection, 6)
   {
      uint32_t allow_pausing;
      int32_t frames[2];

      allow_pausing = htonl((uint32_t) netplay->allow_pausing);
      if (!netplay_send_raw_cmd(netplay, connection,
            NETPLAY_CMD_SETTING_ALLOW_PAUSING,
            &allow_pausing, sizeof(allow_pausing)))
         return false;

      frames[0] = htonl(netplay->input_latency_frames_min);
      frames[1] = htonl(netplay->input_latency_frames_max);
      if (!netplay_send_raw_cmd(netplay, connection,
            NETPLAY_CMD_SETTING_INPUT_LATENCY_FRAMES,
            frames, sizeof(frames)))
         return false;
   }

   if (!netplay_send_flush(&connection->send_packet_buffer,
         connection->fd, false))
      return false;

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
   ssize_t recvd;
   struct nick_buf_s nick_buf;
   int32_t ping         = 0;
   settings_t *settings = config_get_ptr();

   RECV(&nick_buf, sizeof(nick_buf)) {}

   /* Expecting only a nick command */
   if (     recvd < 0
         || ntohl(nick_buf.cmd[0]) != NETPLAY_CMD_NICK
         || ntohl(nick_buf.cmd[1]) != sizeof(nick_buf.nick))
   {
      const char *_msg = NULL;

      if (netplay->is_server)
         _msg = msg_hash_to_str(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT);
      else
      {
         _msg = msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      RARCH_ERR("[Netplay] %s\n", _msg);
      return false;
   }

   SET_PING(connection)

   STRING_SAFE(nick_buf.nick, sizeof(nick_buf.nick));
   strlcpy(connection->nick, nick_buf.nick,
      sizeof(connection->nick));

   if (netplay->is_server)
   {
      if (!netplay_handshake_nick(netplay, connection))
         return false;

      /* There's a password, so just put them in PRE_PASSWORD mode */
      if (     !string_is_empty(settings->paths.netplay_password)
            || !string_is_empty(settings->paths.netplay_spectate_password))
         connection->mode = NETPLAY_CONNECTION_PRE_PASSWORD;
      else
      {
         if (!netplay_handshake_info(netplay, connection))
            return false;

         connection->flags |= NETPLAY_CONN_FLAG_CAN_PLAY;
         connection->mode   = NETPLAY_CONNECTION_PRE_INFO;
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
   ssize_t recvd;
   struct password_buf_s password_buf;
   char password[8+NETPLAY_PASS_LEN]; /* 8 for salt */
   char hash[NETPLAY_PASS_HASH_LEN+1]; /* + NULL terminator */
   bool correct         = false;
   settings_t *settings = config_get_ptr();

   RECV(&password_buf, sizeof(password_buf)) {}

   /* Expecting only a password command */
   if (     recvd < 0
         || ntohl(password_buf.cmd[0]) != NETPLAY_CMD_PASSWORD
         || ntohl(password_buf.cmd[1]) != sizeof(password_buf.password))
   {
      RARCH_ERR("[Netplay] Failed to receive netplay password.\n");
      return false;
   }

   /* Calculate the correct password hash(es) and compare */
   snprintf(password, sizeof(password), "%08lX", (unsigned long)connection->salt);

   if (!string_is_empty(settings->paths.netplay_password))
   {
      strlcpy(password + 8, settings->paths.netplay_password,
         sizeof(password) - 8);
      sha256_hash(hash, (uint8_t *) password, strlen(password));

      if (!memcmp(password_buf.password, hash, NETPLAY_PASS_HASH_LEN))
      {
         correct              = true;
         connection->flags   |= NETPLAY_CONN_FLAG_CAN_PLAY;
      }
   }
   if (     !correct
         && !string_is_empty(settings->paths.netplay_spectate_password))
   {
      strlcpy(password + 8, settings->paths.netplay_spectate_password,
         sizeof(password) - 8);
      sha256_hash(hash, (uint8_t *) password, strlen(password));

      if (!memcmp(password_buf.password, hash, NETPLAY_PASS_HASH_LEN))
         correct = true;
   }

   /* Just disconnect if it was wrong */
   if (!correct)
   {
      RARCH_WARN("[Netplay] A client tried to connect with the wrong password.\n");
      return false;
   }

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
   int32_t  ping                    = 0;
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;
   settings_t *settings             = config_get_ptr();
   bool extra_notifications         = settings->bools.notification_show_netplay_extra;

   RECV(&info_buf, sizeof(info_buf.cmd))
   {
      if (!netplay->is_server)
      {
         const char *_msg =
            msg_hash_to_str(MSG_NETPLAY_INCORRECT_PASSWORD);
         RARCH_ERR("[Netplay] %s\n", _msg);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         return false;
      }
   }

   if (recvd < 0 || ntohl(info_buf.cmd[0]) != NETPLAY_CMD_INFO)
   {
      RARCH_ERR("[Netplay] Failed to receive netplay info.\n");
      return false;
   }

   if (netplay->is_server)
   {
      /* Only the server is able to estimate latency at this point. */
      SET_PING(connection)
   }

   cmd_size = ntohl(info_buf.cmd[1]);
   if (cmd_size != sizeof(info_buf) - sizeof(info_buf.cmd))
   {
      /* Either the host doesn't have anything loaded,
         or this is just screwy */
      if (cmd_size)
      {
         /* Huh? */
         RARCH_ERR("[Netplay] Invalid NETPLAY_CMD_INFO payload size.\n");
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
      return false;

   /* Check the core info */
   if (system)
   {
      const char* my_core_version = system->library_version;
      STRING_SAFE(info_buf.core_name, sizeof(info_buf.core_name));
      if (!string_is_equal_case_insensitive(
            info_buf.core_name, system->library_name))
      {
         /* Wrong core! */
         const char *_msg = msg_hash_to_str(MSG_NETPLAY_DIFFERENT_CORES);
         RARCH_ERR("[Netplay] %s\n", _msg);
         if (!netplay->is_server)
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
         return false;
      }

      /* When using the netpacket interface, the core can override the version
       * with a custom string so multiple core versions can stay compatible. */
      if (networking_driver_st.core_netpacket_interface &&
            networking_driver_st.core_netpacket_interface->protocol_version)
      {
         my_core_version =
               networking_driver_st.core_netpacket_interface->protocol_version;
      }

      STRING_SAFE(info_buf.core_version, sizeof(info_buf.core_version));
      if (!string_is_equal_case_insensitive(
            info_buf.core_version, my_core_version))
      {
         const char *_msg = msg_hash_to_str(
               MSG_NETPLAY_DIFFERENT_CORE_VERSIONS);
         RARCH_WARN("[Netplay] %s\n", _msg);
         if (!netplay->is_server && extra_notifications)
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      }
   }

   /* Check the content CRC */
   content_crc = content_get_crc();

   if (content_crc && ntohl(info_buf.content_crc) != content_crc)
   {
      /* Warning of a different severety when using netpacket interface */
      const char *_msg = msg_hash_to_str(
            netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE ?
            MSG_CONTENT_NETPACKET_CRC32S_DIFFER : MSG_CONTENT_CRC32S_DIFFER);
      RARCH_WARN("[Netplay] %s\n", _msg);
      if (!netplay->is_server && extra_notifications)
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
   }

   /* Now switch to the right mode */
   if (netplay->is_server)
   {
      unsigned max_ping = settings->uints.netplay_max_ping;

      if (max_ping && (unsigned)connection->ping > max_ping)
         return false;

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

static bool clear_framebuffer(netplay_t *netplay)
{
   size_t i;

   for (i = 0; i < netplay->buffer_size; i++)
   {
      struct delta_frame *ptr = &netplay->buffer[i];

      ptr->used               = false;

      if (i == netplay->self_ptr)
      {
         /* Clear out any current data but still use this frame */
         if (!netplay_delta_frame_ready(netplay, ptr, 0))
            return false;

         ptr->frame       = netplay->self_frame_count;
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
   uint32_t cmd_size;
   uint32_t new_frame_count, client_num;
   uint32_t local_sram_size = 0, remote_sram_size;
   size_t i, j;
   ssize_t recvd;
   char new_nick[NETPLAY_NICK_LEN];
   retro_ctx_memory_info_t mem_info;
   settings_t *settings = config_get_ptr();

   RECV(cmd, sizeof(cmd))
   {
      const char *_msg = msg_hash_to_str(MSG_PING_TOO_HIGH);
      RARCH_ERR("[Netplay] %s\n", _msg);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return false;
   }

   cmd_size = ntohl(cmd[1]);

   /* Only expecting a sync command */
   if (ntohl(cmd[0]) != NETPLAY_CMD_SYNC ||
         cmd_size < (2*sizeof(uint32_t)
            /* Controller devices */
            + MAX_INPUT_DEVICES*sizeof(uint32_t)
            /* Share modes */
            + MAX_INPUT_DEVICES*sizeof(uint8_t)
            /* Device-client mapping */
            + MAX_INPUT_DEVICES*sizeof(uint32_t)
            /* Client nick */
            + NETPLAY_NICK_LEN))
   {
      RARCH_ERR("[Netplay] Failed to receive netplay sync.\n");
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
   netplay->self_frame_count = netplay->run_frame_count =
      netplay->other_frame_count = netplay->unread_frame_count =
      netplay->server_frame_count = new_frame_count;

   /* And clear out the framebuffer */
   if (!clear_framebuffer(netplay))
      return false;

   /* Get and set each input device */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      uint32_t device;
      retro_ctx_controller_info_t pad;

      RECV(&device, sizeof(device))
         return false;
      device = ntohl(device);

      netplay->config_devices[i] = device;

      if ((device & RETRO_DEVICE_MASK) == RETRO_DEVICE_KEYBOARD)
         netplay->have_updown_device = true;

      pad.port   = (unsigned)i;
      pad.device = device;
      core_set_controller_port_device(&pad);
   }

   /* Get the share modes */
   RECV(netplay->device_share_modes, sizeof(netplay->device_share_modes))
      return false;

   /* Get the client-controller mapping */
   netplay->connected_players   =
      netplay->connected_slaves =
      netplay->self_devices     = 0;

   for (i = 0; i < MAX_CLIENTS; i++)
      netplay->client_devices[i] = 0;
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      uint32_t device;
      RECV(&device, sizeof(device))
         return false;
      device = ntohl(device);

      netplay->device_clients[i]  = device;
      netplay->connected_players |= device;

      for (j = 0; j < MAX_CLIENTS; j++)
      {
         if (device & (1<<j))
            netplay->client_devices[j] |= 1<<i;
      }
   }

   /* Get our nick */
   RECV(new_nick, sizeof(new_nick))
      return false;

   STRING_SAFE(new_nick, sizeof(new_nick));
   if (!string_is_equal(new_nick, netplay->nick))
   {
      size_t _len;
      char msg[512];

      memcpy(netplay->nick, new_nick, sizeof(netplay->nick));

      _len = snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_NETPLAY_CHANGED_NICK), new_nick);
      RARCH_LOG("[Netplay] %s\n", msg);
      runloop_msg_queue_push(msg, _len, 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* Now check the SRAM, but ignore it when using netpacket interface */
   if (netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
   {
      mem_info.id = RETRO_MEMORY_SAVE_RAM;
#ifdef HAVE_THREADS
      autosave_lock();
#endif
      core_get_memory(&mem_info);
      local_sram_size = (unsigned)mem_info.size;
#ifdef HAVE_THREADS
      autosave_unlock();
#endif
   }

   remote_sram_size = cmd_size - (2*sizeof(uint32_t)
      /* Controller devices */
      + MAX_INPUT_DEVICES*sizeof(uint32_t)
      /* Share modes */
      + MAX_INPUT_DEVICES*sizeof(uint8_t)
      /* Device-client mapping */
      + MAX_INPUT_DEVICES*sizeof(uint32_t)
      /* Client nick */
      + NETPLAY_NICK_LEN);

   if (local_sram_size && local_sram_size == remote_sram_size)
   {
      void *sram_buf = malloc(remote_sram_size);

      if (!sram_buf)
         return false;

      /* We cannot use the RECV macro here as we need to ALWAYS free sram_buf. */
      recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd,
         sram_buf, remote_sram_size);
      if (recvd < 0)
      {
         free(sram_buf);
         RARCH_ERR("[Netplay] %s\n",
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
         return false;
      }
      if (recvd < (ssize_t)remote_sram_size)
      {
         free(sram_buf);
         netplay_recv_reset(&connection->recv_packet_buffer);
         return true;
      }

      mem_info.id = RETRO_MEMORY_SAVE_RAM;
#ifdef HAVE_THREADS
      autosave_lock();
#endif
      core_get_memory(&mem_info);
      memcpy(mem_info.data, sram_buf, local_sram_size);
#ifdef HAVE_THREADS
      autosave_unlock();
#endif

      free(sram_buf);
   }
   else if (remote_sram_size)
   {
      /* We can't load this, but we still need to get rid of the data */
      unsigned char sram_buf[1024];

      do
      {
         RECV(sram_buf,
               (remote_sram_size > sizeof(sram_buf)) ?
                  sizeof(sram_buf) : remote_sram_size)
         {
            RARCH_ERR("[Netplay] %s\n",
               msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
            return false;
         }
         remote_sram_size -= recvd;
      } while (remote_sram_size);
   }

   /* We're ready! */
   *had_input = true;
   netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
   connection->mode = NETPLAY_CONNECTION_PLAYING;
   netplay_handshake_ready(netplay, connection);
   netplay_recv_flush(&connection->recv_packet_buffer);

   netplay->next_ping = cpu_features_get_time_usec() + NETPLAY_PING_AFTER;

   /* Tell a core that uses the netpacket interface that the client is ready */
   if (networking_driver_st.core_netpacket_interface &&
         networking_driver_st.core_netpacket_interface->start)
      networking_driver_st.core_netpacket_interface->start(
            (uint16_t)netplay->self_client_num,
            netplay_netpacket_send_cb, netplay_netpacket_poll_receive_cb);

   /* Ask to switch to playing mode if we should */
   if (!settings->bools.netplay_start_as_spectator)
      return netplay_cmd_mode(netplay, NETPLAY_CONNECTION_PLAYING);

#ifdef HAVE_CHEEVOS
   /* Not going to be promoted to player - let achievement system know that we're spectating */
   rcheevos_spectating_changed();
#endif

   return true;
}

/**
 * netplay_handshake
 *
 * Data receiver for all handshake states.
 */
static bool netplay_handshake(netplay_t *netplay,
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
         netplay->modus == NETPLAY_MODUS_INPUT_FRAME_SYNC &&
         !netplay_send_cur_input(netplay, connection))
      return false;

   return ret;
}

/* Because the keymapping has to be generated, call this before using
 * netplay_key_hton or netplay_key_ntoh */
static void netplay_key_init(netplay_t *netplay)
{
#define INIT_KEY(kn, kh) \
   netplay->mapping_hton[kh] = (uint16_t)kn; \
   netplay->mapping_ntoh[kn] = (uint16_t)kh;
#define K(k) INIT_KEY((NETPLAY_KEY_ ## k), (RETROK_ ## k))
#define KL(k,l) INIT_KEY((NETPLAY_KEY_ ## k), l)
#include "netplay_keys.h"
#undef KL
#undef K
#undef INIT_KEY
}

/* The mapping of keys from libretro (host) to netplay (network) */
static uint32_t netplay_key_hton(netplay_t *netplay, unsigned key)
{
   return (key < RETROK_LAST) ? netplay->mapping_hton[key] :
      NETPLAY_KEY_UNKNOWN;
}

/* The mapping of keys from netplay (network) to libretro (host) */
static uint32_t netplay_key_ntoh(netplay_t *netplay, unsigned key)
{
   return (key < NETPLAY_KEY_LAST) ? netplay->mapping_ntoh[key] :
      RETROK_UNKNOWN;
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
      clear_input(delta->simulated_input[i]);
   }

   delta->have_local = false;

   for (i = 0; i < MAX_CLIENTS; i++)
      delta->have_real[i] = false;

   return true;
}

static const uint8_t* netplay_get_savestate_coremem(netplay_t* netplay, const uint8_t* input)
{
   /* If the container header is detected, find the coremem block */
   if (memcmp(input, "NETPLAY", 7) == 0)
   {
      const uint8_t* stop = input + netplay->state_size;
      input += 8; /* NETPLAY# */

      while (input < stop)
      {
         size_t block_size = (input[7] << 24 | input[6] << 16 | input[5] << 8 | input[4]);
         const uint8_t* marker = input;

         input += 8;

         if (memcmp(marker, NETPLAYSTATE_MEM_BLOCK, 4) == 0)
            break;

         input += CONTENT_ALIGN_SIZE(block_size);
      }
   }

   return input;
}

/**
 * netplay_delta_frame_crc
 *
 * Get the CRC for the serialization of this frame.
 */
static uint32_t netplay_delta_frame_crc(netplay_t *netplay,
      struct delta_frame *delta)
{
   const uint8_t* input;

   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);
   input = netplay_get_savestate_coremem(netplay,
      (const uint8_t*)delta->state);

   return encoding_crc32(0L, input, netplay->coremem_size);
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
      free_input_state(&delta->simulated_input[i]);
   }
}

/**
 * netplay_input_state_for
 *
 * Get an input state for a particular client
 */
static netplay_input_state_t netplay_input_state_for(
      netplay_input_state_t *list,
      uint32_t client_num, size_t len,
      bool must_create, bool must_not_create)
{
   netplay_input_state_t ret;
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   while (*list)
   {
      ret = *list;
      if (!ret->used && !must_not_create && ret->size == len)
      {
         ret->client_num = client_num;
         ret->used       = true;
         memset(ret->data, 0, len * sizeof(uint32_t));
         return ret;
      }
      else if (ret->used && ret->client_num == client_num)
      {
         if (!must_create && ret->size == len)
            return ret;
         return NULL;
      }
      list = &(ret->next);
   }

   if (must_not_create)
      return NULL;

   /* Couldn't find a slot, allocate a fresh one */
   if (len > 1)
      ret = (netplay_input_state_t)calloc(1, sizeof(struct netplay_input_state) + (len - 1) * sizeof(uint32_t));
   else
      ret = (netplay_input_state_t)calloc(1, sizeof(struct netplay_input_state));
   if (!ret)
      return NULL;
   *list           = ret;
   ret->client_num = client_num;
   ret->used       = true;
   ret->size       = (uint32_t)len;
   return ret;
}

/**
 * netplay_expected_input_size
 *
 * Size in words for a given set of devices.
 */
static uint32_t netplay_expected_input_size(netplay_t *netplay,
      uint32_t devices)
{
   uint32_t ret = 0, device;
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

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
      if (!socket_send_all_blocking(sockfd, buf, len, true))
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
         ssize_t sent = socket_send_all_nonblocking(
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
         ssize_t sent = socket_send_all_nonblocking(
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
 * Returns number of bytes returned, which may be short, 0, or -1 on error.
 */
ssize_t netplay_recv(struct socket_buffer *sbuf, int sockfd,
      void *buf, size_t len)
{
   ssize_t recvd;
   bool error    = false;

   if (buf_unread(sbuf) >= len || !buf_remaining(sbuf))
      goto copy;

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

         if (sbuf->start > 1 && buf_unread(sbuf) < len)
         {
            error = false;
            recvd = socket_receive_all_nonblocking(sockfd, &error,
               sbuf->data, sbuf->start - 1);

            if (recvd < 0 || error)
               return -1;

            sbuf->end += recvd;
         }
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
copy:
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
      else if (unread > 0)
      {
         memcpy(buf, sbuf->data + sbuf->read, unread);
         sbuf->read += unread;
         if (sbuf->read >= sbuf->bufsz)
            sbuf->read = 0;
         recvd = unread;
      }
      else
         recvd = 0;
   }
   else
   {
      /* Our read goes around the edge */
      size_t chunka = sbuf->bufsz - sbuf->read;
      size_t chunkb = ((len - chunka) > sbuf->end) ? sbuf->end :
         (len - chunka);

      memcpy(buf, sbuf->data + sbuf->read, chunka);
      if (chunkb > 0)
         memcpy((unsigned char*)buf + chunka, sbuf->data, chunkb);

      sbuf->read = chunkb;
      recvd      = chunka + chunkb;
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

static bool netplay_full(netplay_t *netplay, int fd)
{
   size_t i;
   settings_t *settings     = config_get_ptr();
   unsigned max_connections = settings->uints.netplay_max_connections;
   unsigned total           = 0;

   if (!max_connections || max_connections >= MAX_CLIENTS)
      max_connections = MAX_CLIENTS - 1;

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];

      if (     (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
            || (connection->mode == NETPLAY_CONNECTION_DELAYED_DISCONNECT))
         total++;
   }

   if (total >= max_connections)
   {
      /* We send a header to let the client know we are full. */
      uint32_t header[6];

      /* The only parameter that we need to set is netplay magic;
         we set the protocol version parameter too
         for backwards compatibility. */
      memset(header, 0, sizeof(header));
      header[0] = htonl(FULL_MAGIC);
      header[4] = htonl(HIGH_NETPLAY_PROTOCOL_VERSION);

      /* The kernel might close the socket before sending our data.
         This is fine; the header is just a warning for the client. */
      socket_send_all_nonblocking(fd, header, sizeof(header), true);

      return true;
   }

   return false;
}

static bool netplay_banned(netplay_t *netplay, int fd, netplay_address_t *addr)
{
   size_t i;

   for (i = 0; i < netplay->ban_list.size; i++)
   {
      if (!memcmp(addr, &netplay->ban_list.list[i], sizeof(*addr)))
      {
         /* We send a header to let the client know it's banned. */
         uint32_t header[6];

         /* The only parameter that we need to set is netplay magic;
            we set the protocol version parameter too
            for backwards compatibility. */
         memset(header, 0, sizeof(header));
         header[0] = htonl(BANNED_MAGIC);
         header[4] = htonl(HIGH_NETPLAY_PROTOCOL_VERSION);

         /* The kernel might close the socket before sending our data.
            This is fine; the header is just a warning for the client. */
         socket_send_all_nonblocking(fd, header, sizeof(header), true);

         return true;
      }
   }

   return false;
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
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   payload[0]   = htonl(delta->frame);
   payload[1]   = htonl(delta->crc);

   for (i = 0; i < netplay->connections_size; i++)
   {
      if (     (netplay->connections[i].flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (netplay->connections[i].mode >= NETPLAY_CONNECTION_CONNECTED))
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
   if (     (netplay->connections_size == 0)
       || (!(netplay->connections[0].flags & NETPLAY_CONN_FLAG_ACTIVE))
       ||   (netplay->connections[0].mode  < NETPLAY_CONNECTION_CONNECTED)
       ||   (netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE))
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
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);
   frames = htonl(frames);
   return netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_STALL, &frames, sizeof(frames));
}

#if 0
#define DEBUG_NONDETERMINISTIC_CORES
#endif

/**
 * netplay_update_unread_ptr
 *
 * Update the global unread_ptr and unread_frame_count to correspond to the
 * earliest unread frame count of any connected player
 */
static void netplay_update_unread_ptr(netplay_t *netplay)
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
   if (simstate)
      return simstate;
   if (netplay->read_frame_count[client] > simframe->frame)
      return NULL;
   return netplay_input_state_for(&simframe->simulated_input[device],
         client, dsize, false, true);
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
            if (   abs(new_value)   > abs(value)
                || (abs(new_value) == abs(value)
                && new_value > value))
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
static bool netplay_resolve_input(netplay_t *netplay,
      size_t sim_ptr, bool resim)
{
   size_t prev;
   uint32_t device;
   uint32_t clients, client, client_count;
   netplay_input_state_t simstate, client_state = NULL,
                         resstate, oldresstate, pstate;
   bool ret                     = false;
   struct delta_frame *pframe   = NULL;
   struct delta_frame *simframe = &netplay->buffer[sim_ptr];
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

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
            simstate = netplay_input_state_for(
               &simframe->simulated_input[device],
               client, dsize, false, false);
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
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);
   if (netplay->is_server)
   {
      if (netplay->check_frames && (delta->frame % netplay->check_frames) == 0)
      {
         delta->crc = netplay->state_size ?
            netplay_delta_frame_crc(netplay, delta) : 0;
         netplay_cmd_crc(netplay, delta);
      }
   }
   else
   {
      if (netplay->crcs_valid && delta->crc)
      {
         /* We have a remote CRC, so check it. */
         uint32_t local_crc = netplay->state_size ?
            netplay_delta_frame_crc(netplay, delta) : 0;

         if (local_crc != delta->crc)
         {
            /* If the very first check frame is wrong,
               they probably just don't work. */
            if (!netplay->crc_validity_checked)
            {
               netplay->crcs_valid = false;
               return;
            }

            if (netplay->check_frames)
               netplay_cmd_request_savestate(netplay);
            else
               RARCH_WARN("[Netplay] Netplay CRCs mismatch!\n");
         }
         else
            netplay->crc_validity_checked = true;
      }
   }
}

/**
 * handle_connection
 * @netplay : pointer to netplay object
 * @addr    : value of pointer is set to the address of the peer on a completed connection
 * @error   : value of pointer is set to true if a critical error occurs
 *
 * Accepts a new client connection.
 *
 * Returns: fd of a new connection or -1 if there was no new connection.
 */
static int handle_connection(netplay_t *netplay, netplay_address_t *addr,
      bool *error)
{
   struct sockaddr_storage their_addr;
   socklen_t addr_size = sizeof(their_addr);
   int new_fd          = accept(netplay->listen_fd,
      (struct sockaddr*)&their_addr, &addr_size);
   if (new_fd < 0)
   {
      if (!isagain(new_fd))
         *error = true;

      return -1;
   }

#define INET_TO_NETPLAY(in_addr, out_addr) \
   { \
      uint16_t *prefix = (uint16_t*)&(out_addr)->addr[10]; \
      uint32_t *addr4   = (uint32_t*)&(out_addr)->addr[12]; \
      memset(&(out_addr)->addr[0], 0, 10); \
      *prefix = 0xffff; \
      memcpy(addr4, &((struct sockaddr_in*)(in_addr))->sin_addr, \
         sizeof(*addr4)); \
   }

#ifdef HAVE_INET6
   switch (their_addr.ss_family)
   {
      case AF_INET:
         /* For IPv4, we need to write the address as:
            ::ffff:a.b.c.d */
         INET_TO_NETPLAY(&their_addr, addr)
         break;
      case AF_INET6:
         /* For IPv6, we just copy the address data. */
         memcpy(addr, &((struct sockaddr_in6*)&their_addr)->sin6_addr,
            sizeof(*addr));
         break;
      default:
         /* We can't handle this. Not a critical error though. */
         socket_close(new_fd);
         return -1;
   }
#else
   INET_TO_NETPLAY(&their_addr, addr)
#endif

#undef INET_TO_NETPLAY

   /* Set the socket nonblocking */
   if (!socket_nonblock(new_fd))
   {
      socket_close(new_fd);
      *error = true;

      return -1;
   }

   SET_TCP_NODELAY(new_fd)
   SET_FD_CLOEXEC(new_fd)

   return new_fd;
}

static bool netplay_tunnel_connect(int fd, const struct addrinfo *addr)
{
   int result;

   if (!socket_nonblock(fd))
      return false;

   SET_TCP_NODELAY(fd)
   SET_FD_CLOEXEC(fd)

   result = socket_connect(fd, (void*)addr);
   if (result && !isinprogress(result) && !isagain(result))
      return false;

   return true;
}

/**
 * handle_mitm_connection
 * @netplay : pointer to netplay object
 * @addr    : value of pointer is set to the address of the peer on a completed connection
 * @error   : value of pointer is set to true if a critical error occurs
 *
 * Do three things here.
 * 1: Check if any pending tunnel connection is ready.
 * 2: Check the tunnel server to see if we need to link to a client.
 * 3: Reply to ping requests from the tunnel server.
 *
 * For performance reasons, only create and complete one connection per call.
 *
 * Returns: fd of a new completed connection or -1 if no connection was completed.
 */
static int handle_mitm_connection(netplay_t *netplay, netplay_address_t *addr,
      bool *error)
{
   size_t i;
   int new_fd         = -1;
   retro_time_t ctime = cpu_features_get_time_usec();

   for (i = 0; i < ARRAY_SIZE(netplay->mitm_handler->pending); i++)
   {
      int fd = netplay->mitm_handler->pending[i].fd;

      if (fd >= 0)
      {
         bool ready = true;

         if (!socket_wait(fd, NULL, &ready, 0))
         {
            /* Error */
            RARCH_ERR("[Netplay] Tunnel link connection failed.\n");
         }
         else if (ready)
         {
            /* Connection is ready. */
            mitm_id_t *id = &netplay->mitm_handler->pending[i].id;

            /* Check to see if the tunnel server has already reported
               the peer's address. */
            if (!netplay->mitm_handler->pending[i].has_addr)
               continue;

            /* Now send the linking id. */
            if (socket_send_all_nonblocking(fd, id, sizeof(*id), true) ==
                  sizeof(*id))
            {
               new_fd = fd;
               memcpy(addr, &netplay->mitm_handler->pending[i].addr,
                  sizeof(*addr));
               RARCH_LOG("[Netplay] Tunnel link connection completed.\n");
            }
            else
            {
               /* We couldn't send the peer id in one call. Assume error. */
               socket_close(fd);
               RARCH_ERR("[Netplay] Tunnel link connection failed after handshake.\n");
            }

            netplay->mitm_handler->pending[i].fd = -1;
            break;
         }
         else
         {
            /* Check if the connection timeouted. */
            retro_time_t timeout = netplay->mitm_handler->pending[i].timeout;

            if (ctime < timeout)
               continue;

            RARCH_ERR("[Netplay] Tunnel link connection timeout.\n");
         }

         socket_close(fd);
         netplay->mitm_handler->pending[i].fd = -1;
      }
   }

   if (netplay->mitm_handler->id_recvd < sizeof(netplay->mitm_handler->id_buf))
   {
      /* We haven't received a full id yet. */
      ssize_t recvd;
      size_t  len;

      /* Size depends on whether we've received the magic or not. */
      if (netplay->mitm_handler->id_recvd <
            sizeof(netplay->mitm_handler->id_buf.magic))
         len = sizeof(netplay->mitm_handler->id_buf.magic) -
            netplay->mitm_handler->id_recvd;
      else
         len = sizeof(netplay->mitm_handler->id_buf) -
            netplay->mitm_handler->id_recvd;

      recvd = socket_receive_all_nonblocking(netplay->listen_fd, error,
         (((uint8_t*)&netplay->mitm_handler->id_buf) +
            netplay->mitm_handler->id_recvd),
         len);
      if (recvd < 0 || (size_t)recvd > len)
      {
         RARCH_ERR("[Netplay] Tunnel server error.\n");
         goto critical_failure;
      }

      netplay->mitm_handler->id_recvd += recvd;
   }
   else
   {
      /* We've received a full id, receive any additional data now. */
      switch (ntohl(netplay->mitm_handler->id_buf.magic))
      {
         case MITM_ADDR_MAGIC:
         {
            size_t  len   = sizeof(netplay->mitm_handler->addr_buf) -
               netplay->mitm_handler->addr_recvd;
            ssize_t recvd = socket_receive_all_nonblocking(
                  netplay->listen_fd, error,
                  (((uint8_t*)&netplay->mitm_handler->addr_buf) +
                   netplay->mitm_handler->addr_recvd),
                  len);
            if (recvd < 0 || (size_t)recvd > len)
            {
               RARCH_ERR("[Netplay] Tunnel server error.\n");
               goto critical_failure;
            }

            netplay->mitm_handler->addr_recvd += recvd;

            break;
         }
         default:
            RARCH_ERR("[Netplay] Received unknown additional data from tunnel server.\n");
            goto critical_failure;
      }
   }

   if (netplay->mitm_handler->id_recvd >=
         sizeof(netplay->mitm_handler->id_buf.magic))
   {
      switch (ntohl(netplay->mitm_handler->id_buf.magic))
      {
         case MITM_LINK_MAGIC:
         {
            if (netplay->mitm_handler->id_recvd <
                  sizeof(netplay->mitm_handler->id_buf))
               break;

            netplay->mitm_handler->id_recvd = 0;

            /* Find a free spot to allocate this connection. */
            for (i = 0; i < ARRAY_SIZE(netplay->mitm_handler->pending); i++)
               if (netplay->mitm_handler->pending[i].fd < 0)
                  break;
            if (i < ARRAY_SIZE(netplay->mitm_handler->pending))
            {
               int fd = socket(
                  netplay->mitm_handler->addr->ai_family,
                  netplay->mitm_handler->addr->ai_socktype,
                  netplay->mitm_handler->addr->ai_protocol
               );

               if (fd >= 0)
               {
                  if (netplay_tunnel_connect(fd, netplay->mitm_handler->addr))
                  {
                     mitm_id_t req_addr;

                     /* Make sure to request the address of this peer. */
                     req_addr.magic = htonl(MITM_ADDR_MAGIC);
                     memcpy(req_addr.unique,
                        netplay->mitm_handler->id_buf.unique,
                        sizeof(req_addr.unique));
                     if (socket_send_all_nonblocking(netplay->listen_fd,
                              &req_addr, sizeof(req_addr), true) !=
                           sizeof(req_addr))
                     {
                        socket_close(fd);
                        RARCH_ERR("[Netplay] Tunnel peer address request failed.\n");
                        goto critical_failure;
                     }

                     /* Now queue the connection. */
                     netplay->mitm_handler->pending[i].fd       = fd;
                     netplay->mitm_handler->pending[i].has_addr = false;
                     memcpy(&netplay->mitm_handler->pending[i].id,
                        &netplay->mitm_handler->id_buf,
                        sizeof(netplay->mitm_handler->pending[i].id));
                     netplay->mitm_handler->pending[i].timeout  =
                        ctime + 15000000; /* 15 seconds */
                     RARCH_LOG("[Netplay] Queued tunnel link connection.\n");
                  }
                  else
                  {
                     socket_close(fd);
                     RARCH_ERR("[Netplay] Failed to connect to tunnel server.\n");
                  }
               }
               else
               {
                  RARCH_ERR("[Netplay] Failed to create socket for tunnel link connection.\n");
               }
            }
            else
            {
               RARCH_WARN("[Netplay] Cannot create any more tunnel link connections.\n");
            }

            break;
         }
         case MITM_ADDR_MAGIC:
         {
            if (netplay->mitm_handler->id_recvd <
                  sizeof(netplay->mitm_handler->id_buf))
               break;
            if (netplay->mitm_handler->addr_recvd <
                  sizeof(netplay->mitm_handler->addr_buf))
               break;

            netplay->mitm_handler->id_recvd   = 0;
            netplay->mitm_handler->addr_recvd = 0;

            /* Find the pending connection this address belongs to.
               If we can't find a pending connection, just ignore this data. */
            for (i = 0; i < ARRAY_SIZE(netplay->mitm_handler->pending); i++)
            {
               if (netplay->mitm_handler->pending[i].fd < 0)
                  continue;
               if (netplay->mitm_handler->pending[i].has_addr)
                  continue;

               if (!memcmp(netplay->mitm_handler->pending[i].id.unique,
                     netplay->mitm_handler->id_buf.unique,
                     sizeof(netplay->mitm_handler->pending[i].id.unique)))
               {
                  /* Now copy the received address into the
                     correct pending connection. */
                  memcpy(&netplay->mitm_handler->pending[i].addr,
                     &netplay->mitm_handler->addr_buf,
                     sizeof(netplay->mitm_handler->pending[i].addr));
                  netplay->mitm_handler->pending[i].has_addr = true;
                  break;
               }
            }

            break;
         }
         case MITM_PING_MAGIC:
         {
            /* Tunnel server requested for us to reply to a ping request. */
            void *ping  = &netplay->mitm_handler->id_buf.magic;
            size_t _len = sizeof(netplay->mitm_handler->id_buf.magic);

            netplay->mitm_handler->id_recvd = 0;

            if (socket_send_all_nonblocking(netplay->listen_fd,
                  ping, _len, true) != (ssize_t)_len)
            {
               /* We couldn't send our ping reply in one call. Assume error. */
               RARCH_ERR("[Netplay] Tunnel ping reply failed.\n");
               goto critical_failure;
            }

            break;
         }
         default:
            RARCH_ERR("[Netplay] Received unknown magic from tunnel server.\n");
            goto critical_failure;
      }
   }

   return new_fd;

critical_failure:
   if (new_fd >= 0)
      socket_close(new_fd);

   *error = true;

   return -1;
}

/**
 * allocate_connection
 * @netplay : pointer to netplay object
 *
 * Allocates a new client connection.
 *
 * Returns: pointer to connection object.
 */
static struct netplay_connection *allocate_connection(netplay_t *netplay)
{
   size_t i;
   struct netplay_connection *connection = NULL;

   /* Look for an existing non-used connection first. */
   for (i = 0; i < netplay->connections_size; i++)
   {
      connection = &netplay->connections[i];
      if (   (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (connection->mode != NETPLAY_CONNECTION_DELAYED_DISCONNECT)))
         break;
   }
   if (i < netplay->connections_size)
      memset(connection, 0, sizeof(*connection));
   else if (!netplay->connections_size)
   {
      netplay->connections =
         (struct netplay_connection*)calloc(1, sizeof(*netplay->connections));
      if (!netplay->connections)
         return NULL;
      netplay->connections_size = 1;

      connection = &netplay->connections[0];
   }
   else
   {
      size_t new_size;
      struct netplay_connection *new_connections;

      if (netplay->connections_size >= (MAX_CLIENTS - 1))
         return NULL;

      new_size        = netplay->connections_size + 3;
      new_connections = (struct netplay_connection*)realloc(
         netplay->connections, new_size * sizeof(*new_connections));
      if (!new_connections)
         return NULL;
      memset(new_connections + netplay->connections_size, 0,
         (new_size - netplay->connections_size) * sizeof(*new_connections));

      connection = &new_connections[netplay->connections_size];

      netplay->connections_size = new_size;
      netplay->connections      = new_connections;
   }

   return connection;
}

/**
 * netplay_sync_pre_frame
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay synchronization.
 */
static bool netplay_sync_pre_frame(netplay_t *netplay)
{
   bool ret = true;

   if (netplay->modus == NETPLAY_MODUS_INPUT_FRAME_SYNC &&
       netplay->run_frame_count > 0 && netplay_delta_frame_ready(netplay,
            &netplay->buffer[netplay->run_ptr], netplay->run_frame_count))
   {
      /* Don't serialize until it's safe. */
      if (!(netplay->quirks & NETPLAY_QUIRK_INITIALIZATION))
      {
         retro_ctx_serialize_info_t serial_info = {0};
         serial_info.data = netplay->buffer[netplay->run_ptr].state;

         if (netplay_build_savestate(netplay, &serial_info, false))
         {
            if (netplay->force_send_savestate && !netplay->stall &&
                  !netplay->remote_paused)
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

               /* Send this along to the other side. */
               netplay_load_savestate(netplay, &serial_info, false);

               netplay->force_send_savestate = false;
            }
         }
         else
         {
            ret = false;
            goto process;
         }
      }
   }

   if (netplay->is_server)
   {
      int               new_fd   = -1;
      netplay_address_t new_addr = {0};
      bool server_error          = false;

      if (netplay->mitm_handler)
         new_fd = handle_mitm_connection(netplay, &new_addr, &server_error);
      else
         new_fd = handle_connection(netplay, &new_addr, &server_error);
      if (server_error)
      {
         ret = false;
         goto process;
      }

      if (new_fd >= 0)
      {
         struct netplay_connection *connection;

         if (netplay_banned(netplay, new_fd, &new_addr))
         {
            /* Client is banned. */
            socket_close(new_fd);
            goto process;
         }

         if (netplay_full(netplay, new_fd))
         {
            /* Not accepting any more connections. */
            socket_close(new_fd);
            goto process;
         }

         /* Allocate a connection */
         connection = allocate_connection(netplay);
         if (!connection)
         {
            socket_close(new_fd);
            goto process;
         }

         if (    !netplay_init_socket_buffer(
                  &connection->send_packet_buffer,
                  netplay->packet_buffer_size)
               || !netplay_init_socket_buffer(
                  &connection->recv_packet_buffer,
                  netplay->packet_buffer_size))
         {
            netplay_deinit_socket_buffer(&connection->send_packet_buffer);
            netplay_deinit_socket_buffer(&connection->recv_packet_buffer);
            socket_close(new_fd);
            goto process;
         }

         /* Set it up */
         connection->flags |= NETPLAY_CONN_FLAG_ACTIVE;
         connection->fd     = new_fd;
         connection->mode   = NETPLAY_CONNECTION_INIT;

         memcpy(&connection->addr, &new_addr, sizeof(connection->addr));
      }
   }

process:
   input_poll_net(netplay);

   return ret;
}

/**
 * netplay_sync_input_post_frame
 * @netplay              : pointer to netplay object
 * @stalled              : true if we're currently stalled
 *
 * Post-frame for Netplay synchronization.
 * We check if we have new input and replay from recorded input.
 */
static void netplay_sync_input_post_frame(netplay_t *netplay, bool stalled)
{
   uint32_t lo_frame_count, hi_frame_count;
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   /* Unless we're stalling, we've just finished running a frame */
   if (!stalled)
   {
      netplay->run_ptr = NEXT_PTR(netplay->run_ptr);
      netplay->run_frame_count++;
   }

   /* We've finished an input frame even if we're stalling */
   if (( !stalled
       || netplay->stall == NETPLAY_STALL_INPUT_LATENCY)
       && netplay->self_frame_count <
          netplay->run_frame_count  + netplay->input_latency_frames)
   {
      netplay->self_ptr = NEXT_PTR(netplay->self_ptr);
      netplay->self_frame_count++;
   }

   /* Only relevant if we're connected and not in a desynching operation */
   if (   (netplay->is_server && (netplay->connected_players<=1))
       || (netplay->self_mode < NETPLAY_CONNECTION_CONNECTED)
       || (netplay->desync))
   {
      netplay->other_frame_count = netplay->self_frame_count;
      netplay->other_ptr         = netplay->self_ptr;

      /* FIXME: Duplication */
      if (netplay->catch_up)
      {
         netplay->catch_up             = false;
         input_state_get_ptr()->flags &= ~INP_FLAG_NONBLOCKING;
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
      if (!netplay_process_savestate(netplay, &serial_info))
         RARCH_ERR("[Netplay] Netplay savestate loading failed: Prepare for desync!\n");

      while (netplay->replay_frame_count < netplay->run_frame_count)
      {
         retro_time_t start, tm;
         struct delta_frame *ptr = &netplay->buffer[netplay->replay_ptr];

         serial_info.data_const  = NULL;
         serial_info.data        = ptr->state;
         serial_info.size        = netplay->state_size;

         start                   = cpu_features_get_time_usec();

         /* Remember the current state */
         memset(serial_info.data, 0, serial_info.size);
         netplay_build_savestate(netplay, &serial_info, true);

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

            ptr              = &netplay->buffer[netplay->replay_ptr];
            serial_info.data = ptr->state;
            memset(serial_info.data, 0, serial_info.size);
            core_serialize_special(&serial_info);

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
         netplay->catch_up             = false;
         input_state_get_ptr()->flags &= ~INP_FLAG_NONBLOCKING;
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
               netplay->catch_up             = true;
               netplay->catch_up_time        = 0;
               input_state_get_ptr()->flags |= INP_FLAG_NONBLOCKING;
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

            if (  !(connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
                || (connection->mode != NETPLAY_CONNECTION_PLAYING))
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

   RARCH_LOG("[Netplay] %s\n", msg);

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
    connection->flags     &= ~NETPLAY_CONN_FLAG_PAUSED;
    netplay->remote_paused = false;
    for (i = 0; i < netplay->connections_size; i++)
    {
       struct netplay_connection *sc = &netplay->connections[i];
       if (sc->flags &
                 ((NETPLAY_CONN_FLAG_ACTIVE | NETPLAY_CONN_FLAG_PAUSED)
             ==   (NETPLAY_CONN_FLAG_ACTIVE | NETPLAY_CONN_FLAG_PAUSED)))
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
static void netplay_hangup(netplay_t *netplay,
      struct netplay_connection *connection)
{
   size_t i;
   char msg[512];
   const char *_msg;
   bool was_playing     = false;
   settings_t *settings = config_get_ptr();

   if (!netplay || (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE)))
      return;

   was_playing = connection->mode == NETPLAY_CONNECTION_PLAYING ||
      connection->mode == NETPLAY_CONNECTION_SLAVE;

   /* Report this disconnection */
   if (netplay->is_server)
   {
      if (!string_is_empty(connection->nick))
      {
         snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_NETPLAY_SERVER_NAMED_HANGUP),
            connection->nick);
         _msg = msg;
      }
      else
         _msg = msg_hash_to_str(MSG_NETPLAY_SERVER_HANGUP);
   }
   else
   {
      _msg = msg_hash_to_str(MSG_NETPLAY_CLIENT_HANGUP);
#ifdef HAVE_PRESENCE
      {
         presence_userdata_t userdata;
         userdata.status = PRESENCE_NETPLAY_NETPLAY_STOPPED;
         command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
      }
#endif
   }

   if (networking_driver_st.core_netpacket_interface
         && was_playing && netplay->is_server
         && networking_driver_st.core_netpacket_interface->disconnected)
      networking_driver_st.core_netpacket_interface->disconnected
            ((uint16_t)(connection - netplay->connections + 1));

   RARCH_LOG("[Netplay] %s\n", _msg);
   /* This notification is really only important to the server if the client was playing.
    * Let it be optional if server and the client wasn't playing. */
   if (!netplay->is_server || was_playing ||
         settings->bools.notification_show_netplay_extra)
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   socket_close(connection->fd);
   connection->flags &= ~NETPLAY_CONN_FLAG_ACTIVE;
   netplay_deinit_socket_buffer(&connection->send_packet_buffer);
   netplay_deinit_socket_buffer(&connection->recv_packet_buffer);

   if (!netplay->is_server)
   {
      netplay->self_mode = NETPLAY_CONNECTION_NONE;
      netplay->connected_players &= (1L<<netplay->self_client_num);
      for (i = 0; i < MAX_CLIENTS; i++)
      {
         if (i != netplay->self_client_num)
            netplay->client_devices[i] = 0;
      }
      for (i = 0; i < MAX_INPUT_DEVICES; i++)
         netplay->device_clients[i] &= (1L<<netplay->self_client_num);
      netplay->stall = NETPLAY_STALL_NONE;
   }
   else
   {
      /* Mark the player for removal */
      if (was_playing)
      {
         uint32_t client_num = (uint32_t)
            (connection - netplay->connections + 1);

         if (netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
         {
            /* This special mode keeps the connection object
               alive long enough to send the disconnection
               message at the correct time */
            connection->mode         = NETPLAY_CONNECTION_DELAYED_DISCONNECT;
            connection->delay_frame  = netplay->read_frame_count[client_num];
         }
         else
         {
            /* With netpacket interface we can send the mode change now */
            struct mode_payload payload;
            payload.frame   = htonl(netplay->self_frame_count);
            payload.mode    = htonl(client_num);
            payload.devices = 0;
            memcpy(payload.share_modes, netplay->device_share_modes,
                  sizeof(payload.share_modes));
            memcpy(payload.nick, connection->nick, sizeof(payload.nick));
            netplay_send_raw_cmd_all(netplay, connection,
                  NETPLAY_CMD_MODE, &payload, sizeof(payload));

            connection->mode         = NETPLAY_CONNECTION_NONE;
         }

         /* Mark them as not playing anymore */
         netplay->connected_players &= ~(1L<<client_num);
         netplay->connected_slaves  &= ~(1L<<client_num);
         netplay->client_devices[client_num] = 0;
         for (i = 0; i < MAX_INPUT_DEVICES; i++)
            netplay->device_clients[i] &= ~(1L<<client_num);
      }
   }

   /* Unpause them */
   if (connection->flags & NETPLAY_CONN_FLAG_PAUSED)
      remote_unpaused(netplay, connection);
}

/**
 * netplay_delayed_state_change:
 *
 * Handle any pending state changes which are ready
 * as of the beginning of the current frame.
 */
static void netplay_delayed_state_change(netplay_t *netplay)
{
   size_t i;
   struct mode_payload payload;
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   payload.devices = 0;
   memcpy(payload.share_modes, netplay->device_share_modes,
      sizeof(payload.share_modes));

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];

      if (    (!(connection->delay_frame))
            ||  (connection->delay_frame > netplay->self_frame_count))
         continue;

      if (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
      {
         if (connection->mode != NETPLAY_CONNECTION_DELAYED_DISCONNECT)
            continue;

         /* Remove the connection entirely if relevant */
         connection->mode = NETPLAY_CONNECTION_NONE;
      }

      /* Something was delayed! Prepare the MODE command */
      payload.frame = htonl(connection->delay_frame);
      payload.mode  = htonl(i + 1);
      memcpy(payload.nick, connection->nick, sizeof(payload.nick));

      netplay_send_raw_cmd_all(netplay, connection,
         NETPLAY_CMD_MODE, &payload, sizeof(payload));

      /* Forget the delay frame */
      connection->delay_frame = 0;
   }
}

/* Send the specified input data */
static bool send_input_frame(netplay_t *netplay, struct delta_frame *dframe,
      struct netplay_connection *only, struct netplay_connection *except,
      uint32_t client_num, bool slave)
{
#define BUFSZ 16 /* FIXME: Arbitrary restriction */
   size_t i;
   uint32_t buffer[BUFSZ], devices, device;
   /* Set up the basic buffer */
   size_t bufused   = 4;
   buffer[0]        = htonl(NETPLAY_CMD_INPUT);
   buffer[2]        = htonl(dframe->frame);
   buffer[3]        = htonl(client_num);
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

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
   buffer[1] = htonl((bufused - 2) * sizeof(uint32_t));

#ifdef DEBUG_NETPLAY_STEPS
   RARCH_LOG("[Netplay] Sending input for client %u\n", (unsigned) client_num);
   print_state(netplay);
#endif

   if (only)
   {
      if (!netplay_send(&only->send_packet_buffer, only->fd, buffer, bufused * sizeof(uint32_t)))
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
         if (   (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
             && (connection->mode >= NETPLAY_CONNECTION_CONNECTED)
             && (connection->mode != NETPLAY_CONNECTION_PLAYING
             || (i+1 != client_num)))
         {
            if (!netplay_send(&connection->send_packet_buffer, connection->fd,
                  buffer, bufused * sizeof(uint32_t)))
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
   struct delta_frame *dframe = &netplay->buffer[netplay->self_ptr];
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   if (netplay->is_server)
   {
      uint32_t from_client;
      uint32_t to_client = (uint32_t)(connection - netplay->connections + 1);

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
   size_t len)
{
   uint32_t cmdbuf[2];

   cmdbuf[0] = htonl(cmd);
   cmdbuf[1] = htonl(len);

   if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmdbuf,
         sizeof(cmdbuf)))
      return false;
   if (len > 0)
      if (!netplay_send(&connection->send_packet_buffer, connection->fd, data, len))
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
   size_t len)
{
   size_t i;
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection == except)
         continue;
      if (     (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (connection->mode >= NETPLAY_CONNECTION_CONNECTED))
      {
         if (!netplay_send_raw_cmd(netplay, connection, cmd, data, len))
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
      if (     (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (connection->mode >= NETPLAY_CONNECTION_CONNECTED))
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
 * netplay_announce_play_spectate
 *
 * Announce a play or spectate mode change
 */
static void netplay_announce_play_spectate(netplay_t *netplay,
      const char *nick,
      enum rarch_netplay_connection_mode mode, uint32_t devices,
      int32_t ping, uint32_t client_num)
{
   size_t _len;
   char msg[512];
   const char *_msg = NULL;

   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         if (nick)
         {
            snprintf(msg, sizeof(msg),
               msg_hash_to_str(MSG_NETPLAY_PLAYER_S_LEFT),
               NETPLAY_NICK_LEN, nick);
            _msg = msg;
         }
         else
            _msg = msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME);
         break;
      case NETPLAY_CONNECTION_PLAYING:
      case NETPLAY_CONNECTION_SLAVE:
      {
         char device_str[256];
         char ping_str[32];
         uint32_t device;
         uint32_t one_device = (uint32_t) -1;
         char *pdevice_str   = NULL;

         if (netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE)
            one_device = client_num;
         else
         {
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
         }

         if (one_device != (uint32_t) -1)
         {
            /* Only have one device, simpler message */
            if (nick)
               _len = snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N),
                  NETPLAY_NICK_LEN, nick, one_device + 1);
            else
               _len = snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N),
                  one_device + 1);
         }
         else
         {
            /* Multiple devices, so step one is to make the
               device string listing them all */
            pdevice_str = device_str;
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               if (devices & (1<<device))
                  pdevice_str += snprintf(pdevice_str,
                     sizeof(device_str) - (size_t)(pdevice_str - device_str),
                     "%u, ", (unsigned)(device + 1));
            }
            if (pdevice_str > device_str)
               pdevice_str -= 2;
            *pdevice_str = '\0';

            /* Then we make the final string */
            if (nick)
               _len = snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S),
                  NETPLAY_NICK_LEN, nick, sizeof(device_str), device_str);
            else
               _len = snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S),
                  sizeof(device_str), device_str);
         }

         if (ping >= 0)
         {
            snprintf(ping_str, sizeof(ping_str), " (ping: %i ms)",
               (int)ping);
            strlcpy(msg + _len, ping_str, sizeof(msg) - _len);
         }

         _msg = msg;
         break;
      }
      default: /* wrong usage */
         return;
   }

#ifdef HAVE_CHEEVOS
   rcheevos_spectating_changed();

   if (!netplay->is_server && !netplay_is_spectating()) /* force sync of achievement state */
      netplay_cmd_request_savestate(netplay);
#endif

   RARCH_LOG("[Netplay] %s\n", _msg);
   runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
      MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/**
 * netplay_handle_play_spectate
 *
 * Handle a play or spectate request
 */
static void netplay_handle_play_spectate(netplay_t *netplay,
      uint32_t client_num, struct netplay_connection *connection,
      uint32_t cmd, uint32_t cmd_size, uint32_t *in_payload)
{
   uint32_t i;
   uint32_t client_mask;
   struct mode_payload payload;

   switch (cmd)
   {
      case NETPLAY_CMD_SPECTATE:
         {
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);
            if (cmd_size || in_payload)
               return;

            client_mask = ~(1 << client_num);

            netplay->connected_players &= client_mask;
            netplay->connected_slaves  &= client_mask;

            netplay->client_devices[client_num] = 0;
            for (i = 0; i < MAX_INPUT_DEVICES; i++)
               netplay->device_clients[i] &= client_mask;

            payload.frame   = htonl(netplay->read_frame_count[client_num]);
            payload.devices = 0;
            memcpy(payload.share_modes, netplay->device_share_modes,
               sizeof(payload.share_modes));

            if (connection)
            {
               payload.mode = htonl(NETPLAY_CMD_MODE_BIT_YOU | client_num);
               memcpy(payload.nick, connection->nick, sizeof(payload.nick));

               /* The frame we haven't received is their end frame */
               connection->delay_frame = netplay->read_frame_count[client_num];
               connection->mode = NETPLAY_CONNECTION_SPECTATING;

               /* Only tell the player.
                  The others will be told at delay_frame */
               netplay_send_raw_cmd(netplay, connection,
                  NETPLAY_CMD_MODE, &payload, sizeof(payload));

               netplay_announce_play_spectate(netplay, connection->nick,
                  NETPLAY_CONNECTION_SPECTATING, 0, -1, client_num);
            }
            else
            {
               payload.mode = 0;
               memcpy(payload.nick, netplay->nick, sizeof(payload.nick));

               netplay->self_devices = 0;
               netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;

               netplay_announce_play_spectate(netplay, NULL,
                  NETPLAY_CONNECTION_SPECTATING, 0, -1, client_num);

               /* It was the server, so tell everyone else */
               netplay_send_raw_cmd_all(netplay, NULL,
                  NETPLAY_CMD_MODE, &payload, sizeof(payload));
            }
         }
         break;

      case NETPLAY_CMD_PLAY:
         {
            uint32_t mode;
            uint32_t devices;
            uint8_t  share_mode;

            if (cmd_size != sizeof(mode) || !in_payload)
               return;

            client_mask = 1 << client_num;

            mode       = ntohl(*in_payload);
            devices    = mode & 0xFFFF;
            share_mode = (mode >> 16) & 0xFF;

            /* Fix our share mode */
            if (share_mode)
            {
               if (!(share_mode & NETPLAY_SHARE_DIGITAL_BITS))
                  share_mode |= NETPLAY_SHARE_DIGITAL_OR;
               if (!(share_mode & NETPLAY_SHARE_ANALOG_BITS))
                  share_mode |= NETPLAY_SHARE_ANALOG_MAX;
               share_mode &= ~NETPLAY_SHARE_NO_PREFERENCE;
            }

            /* No device needs to be assigned with netpacket interface */
            if (netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE)
               devices = 0;
            else if (devices)
            {
               /* Make sure the devices are available and/or shareable */
               for (i = 0; i < MAX_INPUT_DEVICES; i++)
               {
                  if (!(devices & (1 << i)))
                     continue;
                  if (!netplay->device_clients[i])
                     continue;

                  if (!netplay->device_share_modes[i] || !share_mode)
                  {
                     /* Device already taken and unshareable */
                     if (connection)
                     {
                        uint32_t reason = htonl(
                           NETPLAY_CMD_MODE_REFUSED_REASON_NOT_AVAILABLE);
                        netplay_send_raw_cmd(netplay, connection,
                           NETPLAY_CMD_MODE_REFUSED, &reason, sizeof(reason));
                     }
                     else
                     {
                        const char *_msg = msg_hash_to_str(
                           MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE);
                        RARCH_LOG("[Netplay] %s\n", _msg);
                        runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
                           MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     }
                     return;
                  }
               }
               for (i = 0; i < MAX_INPUT_DEVICES; i++)
               {
                  if (!(devices & (1 << i)))
                     continue;

                  if (!netplay->device_clients[i])
                  {
                     retro_ctx_controller_info_t pad;

                     pad.port   = (unsigned)i;
                     pad.device = netplay->config_devices[i];
                     core_set_controller_port_device(&pad);

                     netplay->device_share_modes[i] = share_mode;
                  }

                  netplay->device_clients[i] |= client_mask;
               }
            }
            else
            {
               /* Find an available device */
               for (i = 0; i < MAX_INPUT_DEVICES; i++)
               {
                  if (netplay->config_devices[i] == RETRO_DEVICE_NONE)
                  {
                     i = MAX_INPUT_DEVICES;
                     break;
                  }

                  if (!netplay->device_clients[i])
                     break;
               }
               if (i >= MAX_INPUT_DEVICES)
               {
                  /* No device free and no device specifically asked for,
                     but only one device, so share it */
                  if (   netplay->config_devices[1]     == RETRO_DEVICE_NONE
                      && netplay->device_share_modes[0] && share_mode)
                  {
                     i       = 0;
                     devices = 1;
                  }
                  else
                  {
                     /* No slots free! */
                     if (connection)
                     {
                        uint32_t reason = htonl(
                           NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS);
                        netplay_send_raw_cmd(netplay, connection,
                           NETPLAY_CMD_MODE_REFUSED, &reason, sizeof(reason));
                     }
                     else
                     {
                        const char *_msg = msg_hash_to_str(
                           MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS);
                        RARCH_LOG("[Netplay] %s\n", _msg);
                        runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
                           MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     }
                     return;
                  }
               }
               else
               {
                  retro_ctx_controller_info_t pad;

                  devices    = 1 << i;

                  pad.port   = (unsigned)i;
                  pad.device = netplay->config_devices[i];
                  core_set_controller_port_device(&pad);

                  netplay->device_share_modes[i] = share_mode;
               }
               netplay->device_clients[i] |= client_mask;
            }
            netplay->client_devices[client_num] = devices;

            payload.devices = htonl(devices);
            memcpy(payload.share_modes, netplay->device_share_modes,
               sizeof(payload.share_modes));

            netplay->connected_players |= client_mask;

            if (connection)
            {
               bool       slave     = false;
               settings_t *settings = config_get_ptr();

               /* Slave mode unused when core uses netpacket interface */
               if (     settings->bools.netplay_allow_slaves
                     && netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
               {
                  if (settings->bools.netplay_require_slaves)
                     slave = true;
                  else if (mode & NETPLAY_CMD_PLAY_BIT_SLAVE)
                     slave = true;
               }

               /* They start at the next frame */
               netplay->read_ptr[client_num]         = NEXT_PTR(netplay->self_ptr);
               netplay->read_frame_count[client_num] =
                  netplay->self_frame_count + 1;

               payload.frame = htonl(netplay->read_frame_count[client_num]);
               memcpy(payload.nick, connection->nick, sizeof(payload.nick));

               mode = NETPLAY_CMD_MODE_BIT_PLAYING | client_num;

               if (slave)
               {
                  netplay->connected_slaves |= client_mask;
                  mode |= NETPLAY_CMD_MODE_BIT_SLAVE;
                  connection->mode = NETPLAY_CONNECTION_SLAVE;
               }
               else
                  connection->mode = NETPLAY_CONNECTION_PLAYING;

               payload.mode = htonl(mode | NETPLAY_CMD_MODE_BIT_YOU);

               if (networking_driver_st.core_netpacket_interface
                   && networking_driver_st.core_netpacket_interface->connected
                   && !networking_driver_st.core_netpacket_interface->connected(
                         (uint16_t)(connection - netplay->connections + 1)))
               {
                  /* core wants us to drop this new client */
                   uint32_t reason = htonl(
                      NETPLAY_CMD_MODE_REFUSED_REASON_OTHER);
                  connection->mode = NETPLAY_CONNECTION_CONNECTED;
                   netplay_send_raw_cmd(netplay, connection,
                      NETPLAY_CMD_MODE_REFUSED, &reason, sizeof(reason));
                   netplay_hangup(netplay, connection);
                   return;
               }

               /* Tell the player */
               netplay_send_raw_cmd(netplay, connection,
                  NETPLAY_CMD_MODE, &payload, sizeof(payload));

               netplay_announce_play_spectate(netplay, connection->nick,
                  connection->mode, devices, connection->ping, client_num);
            }
            else
            {
               /* We start immediately */
               netplay->read_ptr[client_num] = netplay->self_ptr;
               netplay->read_frame_count[client_num] =
                  netplay->self_frame_count;

               payload.frame = htonl(netplay->read_frame_count[client_num]);
               memcpy(payload.nick, netplay->nick, sizeof(payload.nick));

               mode = NETPLAY_CMD_MODE_BIT_PLAYING;

               netplay->self_devices = devices;
               netplay->self_mode = NETPLAY_CONNECTION_PLAYING;

               netplay_announce_play_spectate(netplay, NULL,
                  netplay->self_mode, devices, -1, client_num);
            }

            payload.mode = htonl(mode);

            /* Tell everyone */
            netplay_send_raw_cmd_all(netplay, connection,
               NETPLAY_CMD_MODE, &payload, sizeof(payload));
         }
         break;

      default:
         break;
   }
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
   uint32_t cmd      = 0;
   uint32_t *payload = NULL;
   uint32_t cmd_size = 0;
   uint32_t buf      = 0;

   switch (mode)
   {
      case NETPLAY_CONNECTION_SPECTATING:
         cmd      = NETPLAY_CMD_SPECTATE;
         break;
      case NETPLAY_CONNECTION_SLAVE:
         buf = NETPLAY_CMD_PLAY_BIT_SLAVE;
         /* fallthrough */
      case NETPLAY_CONNECTION_PLAYING:
         {
            uint32_t i;
            uint8_t share_mode;
            settings_t *settings = config_get_ptr();

            /* Add a share mode if requested */
            share_mode = netplay_settings_share_mode(
               settings->uints.netplay_share_digital,
               settings->uints.netplay_share_analog
            );
            buf |= (uint32_t)share_mode << 16;

            /* Request devices */
            for (i = 0; i < MAX_INPUT_DEVICES; i++)
            {
               if (settings->bools.netplay_request_devices[i])
                  buf |= 1 << i;
            }

            buf      = htonl(buf);
            cmd      = NETPLAY_CMD_PLAY;
            cmd_size = sizeof(buf);
            payload  = &buf;
         }
         break;

      default:
         return false;
   }

   if (netplay->is_server)
   {
      netplay_handle_play_spectate(netplay, 0, NULL, cmd, cmd_size, payload);
      return true;
   }

   return netplay_send_raw_cmd(netplay, &netplay->connections[0],
      cmd, payload, cmd_size);
}

static void netplay_relay_chat(netplay_t *netplay, const char *nick, const char *msg)
{
   size_t i;
   char data[NETPLAY_NICK_LEN + NETPLAY_CHAT_MAX_SIZE];
   size_t msg_len  = strlen(msg);
   size_t data_len = NETPLAY_NICK_LEN + msg_len;

   memcpy(data, nick, NETPLAY_NICK_LEN);
   memcpy(data + NETPLAY_NICK_LEN, msg, msg_len);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];

      if (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
         continue;

      /* Only playing clients can receive chat.
         Protocol 6+ is required. */
      if (connection->mode == NETPLAY_CONNECTION_PLAYING ||
            connection->mode == NETPLAY_CONNECTION_SLAVE)
      {
         REQUIRE_PROTOCOL_VERSION(connection, 6)
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_PLAYER_CHAT,
               data, data_len);
      }
   }
   /* We don't flush. Chat is not time essential. */
}

static void netplay_show_chat(netplay_t *netplay, const char *nick, const char *msg)
{
   char formatted_chat[NETPLAY_CHAT_MAX_SIZE];

   /* Truncate the message if necessary.
      Truncation here is intentional. */
#ifdef GEKKO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
   size_t _len = snprintf(formatted_chat, sizeof(formatted_chat), "%s: %s", nick, msg);
#ifdef GEKKO
#pragma GCC diagnostic pop
#endif

   RARCH_LOG("[Netplay] %s\n", formatted_chat);

#ifdef HAVE_GFX_WIDGETS
   if (gfx_widgets_ready())
   {
      int i;
      struct netplay_chat *chat = &netplay->chat;

      /* Get rid of the oldest message, while moving the rest up. */
      for (i = ARRAY_SIZE(chat->messages) - 2; i >= 0; i--)
         memcpy(&chat->messages[i+1], &chat->messages[i],
            sizeof(*chat->messages));

      chat->messages[0].frames = NETPLAY_CHAT_FRAME_TIME;
      strlcpy(chat->messages[0].nick, nick, sizeof(chat->messages[0].nick));
      strlcpy(chat->messages[0].msg, msg, sizeof(chat->messages[0].msg));
   }
   else
#endif
      runloop_msg_queue_push(formatted_chat, _len, 1, NETPLAY_CHAT_FRAME_TIME, false,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

#ifdef HAVE_MENU
static bool netplay_chat_check(netplay_t *netplay)
{
   if (!netplay)
      return false;

   /* Do nothing if we don't have a nickname. */
   if (string_is_empty(netplay->nick))
      return false;

   /* Do nothing if we are not playing. */
   if (   netplay->self_mode != NETPLAY_CONNECTION_PLAYING
       && netplay->self_mode != NETPLAY_CONNECTION_SLAVE)
      return false;

   /* If we are the server,
      check if someone is able to read us. */
   if (netplay->is_server)
   {
      size_t i;

      for (i = 0; i < netplay->connections_size; i++)
      {
         struct netplay_connection *connection = &netplay->connections[i];

         if (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
            continue;

         if (     (connection->mode == NETPLAY_CONNECTION_PLAYING)
               || (connection->mode == NETPLAY_CONNECTION_SLAVE))
         {
            REQUIRE_PROTOCOL_VERSION(connection, 6)
               return true;
         }
      }
   }
   /* Otherwise, just check whether our connection is active
      and the server is running protocol 6+. */
   else
   {
      if (netplay->connections[0].flags & NETPLAY_CONN_FLAG_ACTIVE)
      {
         REQUIRE_PROTOCOL_VERSION(&netplay->connections[0], 6)
            return true;
      }
   }

   return false;
}

static void netplay_send_chat(void *userdata, const char *line)
{
   char msg[NETPLAY_CHAT_MAX_SIZE];
   net_driver_state_t *net_st  = &networking_driver_st;
   netplay_t          *netplay = net_st->data;

   /* We perform the same checks,
      just in case something has changed. */
   if (    !string_is_empty(line)
         && netplay_chat_check(netplay))
   {
      /* Truncate line to NETPLAY_CHAT_MAX_SIZE. */
      strlcpy(msg, line, sizeof(msg));

      /* For servers, we need to relay it ourselves. */
      if (netplay->is_server)
      {
         netplay_relay_chat(netplay, netplay->nick, msg);
         netplay_show_chat(netplay, netplay->nick, msg);
      }
      /* For clients, we just send it to the server. */
      else
         netplay_send_raw_cmd(netplay, &netplay->connections[0],
            NETPLAY_CMD_PLAYER_CHAT, msg, strlen(msg));
         /* We don't flush. Chat is not time essential. */
   }

   menu_input_dialog_end();
   retroarch_menu_running_finished(false);
}
#endif

/**
 * netplay_input_chat
 *
 * Opens an input menu for sending netplay chat
 */
static void netplay_input_chat(netplay_t *netplay)
{
#ifdef HAVE_MENU
   if (netplay_chat_check(netplay))
   {
      menu_input_ctx_line_t chat_input = {0};

      retroarch_menu_running();

      chat_input.label         = msg_hash_to_str(MSG_NETPLAY_ENTER_CHAT);
      chat_input.label_setting = "no_setting";
      chat_input.cb            = netplay_send_chat;

      menu_input_dialog_start(&chat_input);
   }
#endif
}

/**
 * netplay_handle_chat
 *
 * Handle a received chat message
 */
static bool netplay_handle_chat(netplay_t *netplay,
      struct netplay_connection *connection,
      const char *nick, const char *msg)
{
   if (    (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
         || string_is_empty(nick)
         || string_is_empty(msg))
      return false;

   REQUIRE_PROTOCOL_VERSION(connection, 6)
   {
      /* Client sent a chat message;
         Relay it to the other clients,
         including the one who sent it. */
      if (netplay->is_server)
      {
         /* Only playing clients can send chat. */
         if (     connection->mode != NETPLAY_CONNECTION_PLAYING
               && connection->mode != NETPLAY_CONNECTION_SLAVE)
            return false;
         netplay_relay_chat(netplay, nick, msg);
      }

      /* If we still got a message even though we are not playing,
         ignore it! */
      if (     netplay->self_mode == NETPLAY_CONNECTION_PLAYING
            || netplay->self_mode == NETPLAY_CONNECTION_SLAVE)
         netplay_show_chat(netplay, nick, msg);

      return true;
   }

   return false;
}

static void netplay_request_ping(netplay_t *netplay,
      struct netplay_connection *connection)
{
   if (    (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
         ||  (connection->mode  < NETPLAY_CONNECTION_CONNECTED))
      return;

   /* Only protocol 6+ supports the ping command. */
   REQUIRE_PROTOCOL_VERSION(connection, 6)
   {
      connection->ping_timer = cpu_features_get_time_usec();

      if (netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_PING_REQUEST,
            NULL, 0))
      {
         /* We need to get this sent asap. */
         if (netplay_send_flush(&connection->send_packet_buffer,
               connection->fd, false))
            connection->flags |= NETPLAY_CONN_FLAG_PING_REQUESTED;
      }
   }
}

static void netplay_answer_ping(netplay_t *netplay,
      struct netplay_connection *connection)
{
   if (netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_PING_RESPONSE,
         NULL, 0))
      /* We need to get this sent asap. */
      netplay_send_flush(&connection->send_packet_buffer, connection->fd,
         false);
}

#undef RECV
#define RECV(buf, sz) \
   recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), (sz)); \
   if (recvd >= 0) \
   { \
      if (recvd < (ssize_t) (sz)) \
         goto shrt; \
   } \
   else

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
   RARCH_LOG("[Netplay] Received netplay command %X (%u) from %u\n", cmd, cmd_size,
         (unsigned) (connection - netplay->connections));
#endif

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
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

            if (cmd_size < 2*sizeof(uint32_t))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_INPUT too short, no frame/client number.\n");
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
               if (   connection->mode != NETPLAY_CONNECTION_PLAYING
                   && connection->mode != NETPLAY_CONNECTION_SLAVE)
               {
                  RARCH_ERR("[Netplay] Netplay input from non-participating player.\n");
                  return netplay_cmd_nak(netplay, connection);
               }
               client_num = (uint32_t)(connection - netplay->connections + 1);
            }

            if (client_num >= MAX_CLIENTS)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_INPUT received data for an unsupported client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (!(netplay->connected_players & (1<<client_num)))
            {
               RARCH_ERR("[Netplay] Invalid NETPLAY_CMD_INPUT player number.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Figure out how much input is expected */
            devices = netplay->client_devices[client_num];
            input_size = netplay_expected_input_size(netplay, devices);

            if (cmd_size != (2+input_size) * sizeof(uint32_t))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_INPUT received an unexpected payload size.\n");
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
                        return false;
                  }
                  break;
               }
               else if (frame_num > netplay->read_frame_count[client_num])
               {
                  /* Out of order = out of luck */
                  RARCH_ERR("[Netplay] Netplay input out of order.\n");
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

               dsize  = netplay_expected_input_size(netplay, 1 << device);
               istate = netplay_input_state_for(&dframe->real_input[device],
                     client_num, dsize,
                     false /* Must be false because of slave-mode clients */,
                     false);

               /* Catastrophe! */
               if (!istate)
                  return netplay_cmd_nak(netplay, connection);

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
               netplay->server_ptr         = netplay->read_ptr[0];
               netplay->server_frame_count = netplay->read_frame_count[0];
            }

#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[Netplay] Received input from %u\n", client_num);
            print_state(netplay);
#endif
            break;
         }

      case NETPLAY_CMD_NOINPUT:
         {
            uint32_t frame;
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

            if (netplay->is_server)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_NOINPUT from a client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(frame))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_NOINPUT received"
                     " an unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frame, sizeof(frame))
               return false;

            frame = ntohl(frame);

            /* We already had this, so ignore the new transmission */
            if (frame < netplay->server_frame_count)
               break;

            if (frame != netplay->server_frame_count)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_NOINPUT for invalid frame.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            netplay->server_ptr = NEXT_PTR(netplay->server_ptr);
            netplay->server_frame_count++;
#ifdef DEBUG_NETPLAY_STEPS
            RARCH_LOG("[Netplay] Received server noinput\n");
            print_state(netplay);
#endif
            break;
         }

      case NETPLAY_CMD_SPECTATE:
      {
         uint32_t client_num;
         NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

         if (!netplay->is_server)
         {
            RARCH_ERR("[Netplay] NETPLAY_CMD_SPECTATE from a server.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (cmd_size)
         {
            RARCH_ERR("[Netplay] Unexpected payload in NETPLAY_CMD_SPECTATE.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
               connection->mode != NETPLAY_CONNECTION_SLAVE)
         {
            /* They were confused */
            RARCH_ERR("[Netplay] NETPLAY_CMD_SPECTATE from client not currently playing.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (connection->delay_frame)
         {
            /* Can't switch modes while a mode switch
               is already in progress. */
            uint32_t reason =
               htonl(NETPLAY_CMD_MODE_REFUSED_REASON_TOO_FAST);
            netplay_send_raw_cmd(netplay, connection,
               NETPLAY_CMD_MODE_REFUSED, &reason, sizeof(reason));
            break;
         }

         client_num = (uint32_t)(connection - netplay->connections + 1);

         netplay_handle_play_spectate(netplay, client_num, connection,
            cmd, 0, NULL);
         break;
      }

      case NETPLAY_CMD_PLAY:
      {
         uint32_t client_num;
         uint32_t payload;

         if (!netplay->is_server)
         {
            RARCH_ERR("[Netplay] NETPLAY_CMD_PLAY from a server.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (cmd_size != sizeof(payload))
         {
            RARCH_ERR("[Netplay] Incorrect NETPLAY_CMD_PLAY payload size.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (     connection->mode == NETPLAY_CONNECTION_PLAYING
               || connection->mode == NETPLAY_CONNECTION_SLAVE)
         {
            /* They were confused */
            RARCH_ERR("[Netplay] NETPLAY_CMD_PLAY from client already playing.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         RECV(&payload, sizeof(payload))
            return false;

         if (!(connection->flags & NETPLAY_CONN_FLAG_CAN_PLAY))
         {
            /* Not allowed to play */
            uint32_t reason =
               htonl(NETPLAY_CMD_MODE_REFUSED_REASON_UNPRIVILEGED);
            netplay_send_raw_cmd(netplay, connection,
               NETPLAY_CMD_MODE_REFUSED, &reason, sizeof(reason));
            break;
         }

         if (connection->delay_frame)
         {
            /* Can't switch modes while a mode switch
               is already in progress. */
            uint32_t reason =
               htonl(NETPLAY_CMD_MODE_REFUSED_REASON_TOO_FAST);
            netplay_send_raw_cmd(netplay, connection,
               NETPLAY_CMD_MODE_REFUSED, &reason, sizeof(reason));
            break;
         }

         client_num = (uint32_t)(connection - netplay->connections + 1);

         netplay_handle_play_spectate(netplay, client_num, connection,
            cmd, cmd_size, &payload);
         break;
      }

      case NETPLAY_CMD_MODE:
      {
         struct mode_payload payload;
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
            RARCH_ERR("[Netplay] NETPLAY_CMD_MODE from client.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         if (cmd_size != sizeof(payload))
         {
            RARCH_ERR("[Netplay] Invalid payload size for NETPLAY_CMD_MODE.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         RECV(&payload, sizeof(payload))
            return false;

         mode = ntohl(payload.mode);

         client_num = mode & 0xFFFF;

         if (client_num >= MAX_CLIENTS)
         {
            RARCH_ERR("[Netplay] Received NETPLAY_CMD_MODE for a higher player number than we support.\n");
            return netplay_cmd_nak(netplay, connection);
         }

         frame   = ntohl(payload.frame);
         devices = ntohl(payload.devices);
         memcpy(netplay->device_share_modes, payload.share_modes,
            sizeof(netplay->device_share_modes));
         STRING_SAFE(payload.nick, sizeof(payload.nick));
         nick    = payload.nick;

         /* We're changing past input, so must replay it */
         if (frame < netplay->self_frame_count)
            netplay->force_rewind = true;

         if (mode & NETPLAY_CMD_MODE_BIT_YOU)
         {
            /* A change to me! */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               /* When a core uses the netpacket interface this is OK */
               if (frame != netplay->server_frame_count
                     && netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
               {
                  RARCH_ERR("[Netplay] Received mode change out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Hooray, I get to play now! */
               if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
               {
                  RARCH_ERR("[Netplay] Received player mode change even though I'm already a player.\n");
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

               if (netplay->modus == NETPLAY_MODUS_INPUT_FRAME_SYNC)
               {
                  /* Fix up current frame info */
                  if (frame <= netplay->self_frame_count &&
                        netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
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
                           dsize  = netplay_expected_input_size(netplay, 1 << device);
                           istate = netplay_input_state_for(
                                 &dframe->real_input[device], client_num, dsize,
                                 false, false);
                           if (!istate)
                              continue;
                           memset(istate->data, 0, dsize * sizeof(uint32_t));
                        }
                        dframe->have_local            = true;
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

                     do
                     {
                        if (!dframe->used)
                        {
                           /* Make sure it's ready */
                           if (!netplay_delta_frame_ready(netplay, dframe, frame_count))
                           {
                              RARCH_ERR("[Netplay] Received mode change but delta frame isn't ready!\n");
                              return netplay_cmd_nak(netplay, connection);
                           }
                        }

                        dframe->have_local = true;

                        /* Go on to the next delta frame */
                        NEXT();
                        frame_count++;
                     } while (frame_count < frame);
                  }
               }

               /* Announce it */
               netplay_announce_play_spectate(netplay, NULL, netplay->self_mode, devices,
                  connection->ping, client_num);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[Netplay] Received mode change self->%X\n", devices);
               print_state(netplay);
#endif
            }
            else /* YOU && !PLAYING */
            {
#ifdef HAVE_CHEEVOS
               rcheevos_spectating_changed(); /* should be a no-op, but synchronize anyway */
#endif
               /* I'm no longer playing, but I should already know this */
               if (netplay->self_mode != NETPLAY_CONNECTION_SPECTATING)
               {
                  RARCH_ERR("[Netplay] Received mode change to spectator unprompted.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[Netplay] Received mode change self->spectating\n");
               print_state(netplay);
#endif
            }
         }
         else /* !YOU */
         {
            /* Somebody else is joining or parting */
            if (mode & NETPLAY_CMD_MODE_BIT_PLAYING)
            {
               /* When a core uses the netpacket interface this is OK */
               if (     frame != netplay->server_frame_count
                     && netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
               {
                  RARCH_ERR("[Netplay] Received mode change out of order.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               netplay->connected_players         |= (1 << client_num);
               netplay->client_devices[client_num] = devices;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  if (devices & (1<<device))
                     netplay->device_clients[device] |= (1<<client_num);

               netplay->read_ptr[client_num] = netplay->server_ptr;
               netplay->read_frame_count[client_num] = netplay->server_frame_count;

               /* Announce it */
               netplay_announce_play_spectate(netplay, nick, NETPLAY_CONNECTION_PLAYING, devices, -1, client_num);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[Netplay] Received mode change %u->%u\n", client_num, devices);
               print_state(netplay);
#endif
            }
            else
            {
               netplay->connected_players          &= ~(1 << client_num);
               netplay->client_devices[client_num]  = 0;
               for (device = 0; device < MAX_INPUT_DEVICES; device++)
                  netplay->device_clients[device]  &= ~(1 << client_num);

               /* Announce it */
               netplay_announce_play_spectate(netplay, nick, NETPLAY_CONNECTION_SPECTATING, 0, -1, client_num);

#ifdef DEBUG_NETPLAY_STEPS
               RARCH_LOG("[Netplay] Received mode change %u->spectator\n", client_num);
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
            const char *_msg = NULL;

            if (netplay->is_server)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_MODE_REFUSED from client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(reason))
            {
               RARCH_ERR("[Netplay] Received invalid payload size for NETPLAY_CMD_MODE_REFUSED.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&reason, sizeof(reason))
               return false;
            reason = ntohl(reason);

            switch (reason)
            {
               case NETPLAY_CMD_MODE_REFUSED_REASON_UNPRIVILEGED:
                  _msg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED);
                  break;
               case NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS:
                  _msg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS);
                  break;
               case NETPLAY_CMD_MODE_REFUSED_REASON_NOT_AVAILABLE:
                  _msg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE);
                  break;
               default:
                  _msg = msg_hash_to_str(MSG_NETPLAY_CANNOT_PLAY);
            }

            RARCH_LOG("[Netplay] %s\n", _msg);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
#ifdef HAVE_CHEEVOS
         rcheevos_spectating_changed(); /* synchronize mode */
#endif
         break;

      case NETPLAY_CMD_DISCONNECT:
         netplay_hangup(netplay, connection);
         return true;

      case NETPLAY_CMD_CRC:
         {
            uint32_t buffer[2];
            size_t tmp_ptr = netplay->run_ptr;
            bool found = false;
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

            if (cmd_size != sizeof(buffer))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_CRC received unexpected payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(buffer, sizeof(buffer))
               return false;

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
            /* We'll have to check it when we catch up */
            else
               netplay->buffer[tmp_ptr].crc = buffer[1];

            break;
         }

      case NETPLAY_CMD_REQUEST_SAVESTATE:
         NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);
         /* Delay until next frame so we don't send the savestate after the
          * input */
         netplay->force_send_savestate = true;
         break;

      case NETPLAY_CMD_LOAD_SAVESTATE:
         {
            uint32_t i;
            uint32_t frame;
            uint32_t state_size, state_size_raw;
            size_t   load_ptr;
            uint32_t load_frame_count;
            uint32_t rd, wn;
            struct compression_transcoder *ctrans = NULL;
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

            if (netplay->is_server)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_LOAD_SAVESTATE from client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size < sizeof(frame) + sizeof(state_size))
            {
               RARCH_ERR("[Netplay] Received invalid payload size for NETPLAY_CMD_LOAD_SAVESTATE.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Only players may load states. */
            if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
                  connection->mode != NETPLAY_CONNECTION_SLAVE)
            {
               RARCH_ERR("[Netplay] Netplay state load from a spectator.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Make sure we're ready for it. */
            if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            {
               if (!netplay->is_replay)
               {
                  netplay->is_replay          = true;
                  netplay->replay_ptr         = netplay->run_ptr;
                  netplay->replay_frame_count = netplay->run_frame_count;
                  netplay_wait_and_init_serialization(netplay);
                  netplay->is_replay          = false;
               }
               else
                  netplay_wait_and_init_serialization(netplay);
            }

            /* There is a subtlety in whether the load comes before or after
             * the current frame:
             *
             * If it comes before the current frame, then we need to force a
             * rewind to that point.
             *
             * If it comes after the current frame, we need to jump ahead,
             * then (strangely) force a rewind to the frame we're already on,
             * so it gets loaded.
             * This is just to avoid having reloading implemented
             * in too many places. */

            RECV(&frame, sizeof(frame))
               return false;
            frame = ntohl(frame);

            load_ptr         = netplay->server_ptr;
            load_frame_count = netplay->server_frame_count;

            if (frame != load_frame_count)
            {
               RARCH_ERR("[Netplay] Netplay state load out of order!\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (!netplay_delta_frame_ready(netplay,
                  &netplay->buffer[load_ptr], load_frame_count))
               /* Hopefully it will be ready after another round of input. */
               goto shrt;

            RECV(&state_size, sizeof(state_size))
               return false;
            state_size     = ntohl(state_size);
            state_size_raw = cmd_size - (sizeof(frame) + sizeof(state_size));

            if (state_size_raw > netplay->zbuffer_size)
            {
               RARCH_ERR("[Netplay] Netplay state load with an unexpected save state size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(netplay->zbuffer, state_size_raw)
               return false;

            switch (connection->compression_supported)
            {
               case NETPLAY_COMPRESSION_ZLIB:
                  ctrans = &netplay->compress_zlib;
                  break;
               default:
                  ctrans = &netplay->compress_nil;
                  break;
            }

            if (state_size > netplay->state_size)
            {
               /* other client state size is larger than ours, grow ours */
               netplay->state_size = state_size;
               for (i = 0; i < netplay->buffer_size; i++)
               {
                  netplay->buffer[i].state = realloc(netplay->buffer[i].state, netplay->state_size);
                  if (!netplay->buffer[i].state)
                     return false;
               }
            }

            ctrans->decompression_backend->set_in(
               ctrans->decompression_stream,
               netplay->zbuffer, state_size_raw);
            ctrans->decompression_backend->set_out(
               ctrans->decompression_stream,
               (uint8_t*)netplay->buffer[load_ptr].state, state_size);
            ctrans->decompression_backend->trans(
               ctrans->decompression_stream,
               true, &rd, &wn, NULL);

            if (memcmp(netplay->buffer[load_ptr].state, "NETPLAY", 7) != 0)
            {
               if (state_size != netplay->coremem_size)
               {
                  RARCH_ERR("[Netplay] Netplay state load with an unexpected save state size.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

#ifdef HAVE_CHEEVOS
               /* did not receive a protocol 7 packet. server isn't sending achievement data. disable hardcore */
               if (     !netplay->is_server
                     &&  rcheevos_hardcore_active()
                     && !netplay_is_spectating())
               {
                  const char* msg = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST);
                  runloop_msg_queue_push(msg, strlen(msg), 0, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  RARCH_WARN("[Netplay] Server did not send achievement information.\n", msg);

                  rcheevos_pause_hardcore();
               }
#endif
            }

            /* Force a rewind to the relevant frame. */
            netplay->force_rewind = true;

            /* Skip ahead if it's past where we are. */
            if (load_frame_count > netplay->run_frame_count)
            {
               /* This is squirrely:
                * We need to assure that when we advance the frame in post_frame,
                * THEN we're referring to the frame to load into.
                * If we refer directly to read_ptr,
                * then we'll end up never reading the input for read_frame_count itself,
                * which will make the other side unhappy. */
               netplay->run_ptr         = PREV_PTR(load_ptr);
               netplay->run_frame_count = load_frame_count - 1;

               if (frame > netplay->self_frame_count)
               {
                  netplay->self_ptr         = netplay->run_ptr;
                  netplay->self_frame_count = netplay->run_frame_count;
               }
            }

            /* Don't expect earlier data from other clients. */
            for (i = 0; i < MAX_CLIENTS; i++)
            {
               if (!(netplay->connected_players & (1 << i)))
                  continue;

               if (frame > netplay->read_frame_count[i])
               {
                  netplay->read_ptr[i]         = load_ptr;
                  netplay->read_frame_count[i] = load_frame_count;
               }
            }

            /* Make sure our states are correct. */
            netplay->savestate_request_outstanding = false;
            netplay->other_ptr                     = load_ptr;
            netplay->other_frame_count             = load_frame_count;

            break;
         }

      case NETPLAY_CMD_RESET:
         {
            uint32_t i;
            uint32_t frame;
            size_t   reset_ptr;
            uint32_t reset_frame_count;
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

            if (netplay->is_server)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_RESET from client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(frame))
            {
               RARCH_ERR("[Netplay] Received invalid payload size for NETPLAY_CMD_RESET.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Only players may reset the core. */
            if (connection->mode != NETPLAY_CONNECTION_PLAYING &&
                  connection->mode != NETPLAY_CONNECTION_SLAVE)
            {
               RARCH_ERR("[Netplay] Netplay core reset from a spectator.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            /* Make sure we're ready for it. */
            if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            {
               if (!netplay->is_replay)
               {
                  netplay->is_replay          = true;
                  netplay->replay_ptr         = netplay->run_ptr;
                  netplay->replay_frame_count = netplay->run_frame_count;
                  netplay_wait_and_init_serialization(netplay);
                  netplay->is_replay          = false;
               }
               else
                  netplay_wait_and_init_serialization(netplay);
            }

            RECV(&frame, sizeof(frame))
               return false;
            frame = ntohl(frame);

            reset_ptr         = netplay->server_ptr;
            reset_frame_count = netplay->server_frame_count;

            if (frame != reset_frame_count)
            {
               RARCH_ERR("[Netplay] Netplay core reset out of order!\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (!netplay_delta_frame_ready(netplay,
                  &netplay->buffer[reset_ptr], reset_frame_count))
               /* Hopefully it will be ready after another round of input. */
               goto shrt;

            netplay->force_reset = true;

            /* This is squirrely:
             * We need to assure that when we advance the frame in post_frame,
             * THEN we're referring to the frame to load into.
             * If we refer directly to read_ptr,
             * then we'll end up never reading the input for read_frame_count itself,
             * which will make the other side unhappy. */

            netplay->run_ptr             = PREV_PTR(reset_ptr);
            netplay->run_frame_count     = reset_frame_count - 1;

            if (frame > netplay->self_frame_count)
            {
               netplay->self_ptr         = netplay->run_ptr;
               netplay->self_frame_count = netplay->run_frame_count;
            }

            /* Don't expect earlier data from other clients. */
            for (i = 0; i < MAX_CLIENTS; i++)
            {
               if (!(netplay->connected_players & (1 << i)))
                  continue;

               if (frame > netplay->read_frame_count[i])
               {
                  netplay->read_ptr[i]         = reset_ptr;
                  netplay->read_frame_count[i] = reset_frame_count;
               }
            }

            /* Make sure our states are correct. */
            netplay->savestate_request_outstanding = false;
            netplay->other_ptr                     = reset_ptr;
            netplay->other_frame_count             = reset_frame_count;

            break;
         }

      case NETPLAY_CMD_PAUSE:
         {
            size_t _len;
            char msg[512], nick[NETPLAY_NICK_LEN];
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

            /* Read in the paused nick */
            if (cmd_size != sizeof(nick))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_PAUSE received invalid payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(nick, sizeof(nick))
               return false;

            if (netplay->is_server)
            {
               /* We outright ignore pausing from spectators and slaves */
               if (connection->mode != NETPLAY_CONNECTION_PLAYING)
                  break;

               /* If the client does not honor our setting,
                  refuse to globally pause. */
               if (!netplay->allow_pausing)
               {
                  RARCH_ERR("[Netplay] Client pausing with allow pausing disabled.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               /* Inform peers */
               _len = snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_NETPLAY_PEER_PAUSED), connection->nick);
               netplay_send_raw_cmd_all(netplay, connection, NETPLAY_CMD_PAUSE,
                  connection->nick, sizeof(connection->nick));

               /* We may not reach post_frame soon, so flush the pause message
                * immediately. */
               netplay_send_flush_all(netplay, connection);
            }
            else
            {
               STRING_SAFE(nick, sizeof(nick));
               _len = snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_NETPLAY_PEER_PAUSED), nick);
            }

            connection->flags     |= NETPLAY_CONN_FLAG_PAUSED;
            netplay->remote_paused = true;

            RARCH_LOG("[Netplay] %s\n", msg);
            runloop_msg_queue_push(msg, _len, 1, 180, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;
         }

      case NETPLAY_CMD_RESUME:
         NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);
         remote_unpaused(netplay, connection);
         break;

      case NETPLAY_CMD_STALL:
         {
            uint32_t frames;
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

            if (netplay->is_server)
            {
               /* Only servers can request a stall! */
               RARCH_ERR("[Netplay] Netplay client requested a stall?\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(frames))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_STALL with incorrect payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&frames, sizeof(frames))
               return false;

            frames = ntohl(frames);

            if (frames > NETPLAY_MAX_REQ_STALL_TIME)
               frames = NETPLAY_MAX_REQ_STALL_TIME;

            /* We can only stall for one reason at a time */
            if (!netplay->stall)
            {
               connection->stall = netplay->stall = NETPLAY_STALL_SERVER_REQUESTED;
               netplay->stall_time = 0;
               connection->stall_frame = frames;
            }
            break;
         }

      case NETPLAY_CMD_NETPACKET:
         {
            uint32_t pkt_client_id;
            void* buf = netplay->zbuffer;
            NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_CORE_PACKET_INTERFACE);

            if (!networking_driver_st.core_netpacket_interface)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_NETPACKET while core netpacket interface is not set.\n");
               return netplay_cmd_nak(netplay, connection);
            }
            if (cmd_size > netplay->zbuffer_size)
            {
               RARCH_ERR("[Netplay] Received netpacket of unexpected size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&pkt_client_id, sizeof(pkt_client_id))
               return false;
            pkt_client_id = ntohl(pkt_client_id);
            RECV(buf, cmd_size)
               return false;

            if (!netplay->is_server)
            {
               /* packets arriving at a client are always meant for us
                * pkt_client_id contains the original sender of the packet */
               if (networking_driver_st.core_netpacket_interface->receive)
                  networking_driver_st.core_netpacket_interface->receive
                     (buf, cmd_size, (uint16_t)pkt_client_id);
            }
            else
            {
               bool broadcast = (pkt_client_id == RETRO_NETPACKET_BROADCAST);
               uint16_t incoming_client_id =
                     (uint16_t)(connection - netplay->connections + 1);

               /* check if this is a packet for the host
                * pkt_client_id designates the recipient of the packet */
               if ((broadcast || pkt_client_id == 0)
                     && networking_driver_st.core_netpacket_interface->receive)
                  networking_driver_st.core_netpacket_interface->receive
                     (buf, cmd_size, incoming_client_id);

               if (broadcast)
               {
                  /* relay to all but incoming client */
                  size_t i, skip = incoming_client_id - 1;
                  for (i = 0; i < netplay->connections_size; i++)
                     if (i != skip)
                         netplay_send_cmd_netpacket(netplay, i,
                            buf, cmd_size, incoming_client_id);
               }
               /* Relay unless target is the host or the incoming client */
               else if (pkt_client_id && pkt_client_id != incoming_client_id)
                  netplay_send_cmd_netpacket(netplay, pkt_client_id-1,
                     buf, cmd_size, incoming_client_id);
            }
            break;
         }

      case NETPLAY_CMD_PLAYER_CHAT:
         {
            char nickname[NETPLAY_NICK_LEN];
            char message[NETPLAY_CHAT_MAX_SIZE];

            /* We do not send the sentinel/null character on chat messages
               and we do not allow empty messages. */
            if (netplay->is_server)
            {
               /* If server, we only receive the message,
                  without the nickname portion. */
               if (!cmd_size || cmd_size >= sizeof(message))
               {
                  RARCH_ERR("[Netplay] NETPLAY_CMD_PLAYER_CHAT with incorrect payload size.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               strlcpy(nickname, connection->nick, sizeof(nickname));
            }
            else
            {
               /* If client, we receive both the nickname and
                  the message from the server. */
               if (     cmd_size <= sizeof(nickname)
                     || cmd_size >= sizeof(nickname) + sizeof(message))
               {
                  RARCH_ERR("[Netplay] NETPLAY_CMD_PLAYER_CHAT with incorrect payload size.\n");
                  return netplay_cmd_nak(netplay, connection);
               }

               RECV(nickname, sizeof(nickname))
                  return false;
               STRING_SAFE(nickname, sizeof(nickname));
               cmd_size -= sizeof(nickname);
            }

            RECV(message, cmd_size)
               return false;
            message[recvd] = '\0';

            if (!netplay_handle_chat(netplay, connection, nickname, message))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_PLAYER_CHAT with invalid message or from an invalid peer.\n");
               return netplay_cmd_nak(netplay, connection);
            }
         }
         break;

      case NETPLAY_CMD_PING_REQUEST:
         netplay_answer_ping(netplay, connection);

         /* If we are the server,
            we should request our own ping after answering. */
         if (netplay->is_server)
            netplay_request_ping(netplay, connection);
         break;

      case NETPLAY_CMD_PING_RESPONSE:
         {
            /* Only process ping responses if we requested them. */
            if (connection->flags & NETPLAY_CONN_FLAG_PING_REQUESTED)
            {
               connection->ping   = (int32_t)
                  ((cpu_features_get_time_usec() - connection->ping_timer)
                     / 1000);
               connection->flags &= ~NETPLAY_CONN_FLAG_PING_REQUESTED;
            }
         }
         break;

      case NETPLAY_CMD_SETTING_ALLOW_PAUSING:
         {
            uint32_t allow_pausing;

            if (netplay->is_server)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_SETTING_ALLOW_PAUSING from client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(allow_pausing))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_SETTING_ALLOW_PAUSING with incorrect payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(&allow_pausing, sizeof(allow_pausing))
               return false;
            allow_pausing = ntohl(allow_pausing);

            if (allow_pausing != 0 && allow_pausing != 1)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_SETTING_ALLOW_PAUSING with incorrect setting.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            netplay->allow_pausing = allow_pausing;
         }
         break;

      case NETPLAY_CMD_SETTING_INPUT_LATENCY_FRAMES:
         {
            int32_t frames[2];

            if (netplay->is_server)
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_SETTING_INPUT_LATENCY_FRAMES from client.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            if (cmd_size != sizeof(frames))
            {
               RARCH_ERR("[Netplay] NETPLAY_CMD_SETTING_INPUT_LATENCY_FRAMES with incorrect payload size.\n");
               return netplay_cmd_nak(netplay, connection);
            }

            RECV(frames, sizeof(frames))
               return false;
            netplay->input_latency_frames_min = ntohl(frames[0]);
            netplay->input_latency_frames_max = ntohl(frames[1]);
         }
         break;

      default:
         {
            unsigned char buf[1024];

            while (cmd_size)
            {
               RECV(buf, (cmd_size > sizeof(buf)) ? sizeof(buf) : cmd_size)
                  return false;
               cmd_size -= recvd;
            }

            RARCH_WARN("[Netplay] %s\n",
               msg_hash_to_str(MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED));
         }
         break;
   }

   netplay_recv_flush(&connection->recv_packet_buffer);

   if (had_input)
      *had_input = true;

   return true;

shrt:
   /* No more data, reset and try again */
   netplay_recv_reset(&connection->recv_packet_buffer);
   return true;
}

#undef RECV

/**
 * netplay_poll_net_input
 *
 * Poll input from the network
 */
static void netplay_poll_net_input(netplay_t *netplay)
{
   size_t i;
   bool had_input;
   struct netplay_connection *connection;

   do
   {
      had_input = false;

      /* Read input from each connection. */
      for (i = 0; i < netplay->connections_size; i++)
      {
         connection = &netplay->connections[i];
         if (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
         {
            if (!netplay_get_cmd(netplay, connection, &had_input))
               netplay_hangup(netplay, connection);
         }
      }
   } while (had_input);
}

/**
 * netplay_handle_slaves
 *
 * Handle any slave connections
 */
static void netplay_handle_slaves(netplay_t *netplay)
{
   struct delta_frame *frame = &netplay->buffer[netplay->self_ptr];
   size_t i;
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (   (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
          && (connection->mode == NETPLAY_CONNECTION_SLAVE))
      {
         uint32_t client_num = (uint32_t)(i + 1);

         /* This is a slave connection. First, should we do anything at all? If
          * we've already "read" this data, then we can just ignore it */
         if (netplay->read_frame_count[client_num] > netplay->self_frame_count)
            continue;

         /* Alright, we have to send something. Do we need to generate it first? */
         if (!frame->have_real[client_num])
         {
            uint32_t device;
            uint32_t devices           = netplay->client_devices[client_num];
            /* Copy the previous frame's data */
            struct delta_frame *oframe = &netplay->buffer[PREV_PTR(netplay->self_ptr)];
            for (device = 0; device < MAX_INPUT_DEVICES; device++)
            {
               netplay_input_state_t istate_out, istate_in;
               if (!(devices & (1<<device)))
                  continue;
               istate_in = oframe->real_input[device];
               while (istate_in && istate_in->client_num != client_num)
                  istate_in = istate_in->next;

               /* Start with blank input */
               if (!istate_in)
                  netplay_input_state_for(&frame->real_input[device],
                        client_num,
                        netplay_expected_input_size(netplay, 1 << device), true,
                        false);
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
static void netplay_announce_nat_traversal(netplay_t *netplay,
      uint16_t ext_port)
{
   net_driver_state_t *net_st = &networking_driver_st;

   /* Start announcing immediately. */
   netplay->next_announce = cpu_features_get_time_usec();

   if (net_st->nat_traversal_request.status == NAT_TRAVERSAL_STATUS_OPENED)
   {
      struct sockaddr_in *addr = &net_st->nat_traversal_request.request.addr;

      netplay->ext_tcp_port = ext_port;

      if (!ipv4_is_cgnat_address(addr) && !ipv4_is_lan_address(addr))
      {
         char msg[512];
         char host[256], port[6];
         size_t _len = strlcpy(msg, msg_hash_to_str(MSG_PUBLIC_ADDRESS), sizeof(msg));

         if (!getnameinfo_retro((struct sockaddr*)addr, sizeof(*addr),
               host, sizeof(host), port, sizeof(port),
               NI_NUMERICHOST | NI_NUMERICSERV))
            _len += snprintf(msg + _len, sizeof(msg) - _len, ": %s:%s", host, port);

         RARCH_LOG("[Netplay] %s\n", msg);
         runloop_msg_queue_push(msg, _len, 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
      else
      {
         const char *msg = msg_hash_to_str(MSG_PRIVATE_OR_SHARED_ADDRESS);
         RARCH_WARN("[Netplay] %s\n", msg);
         runloop_msg_queue_push(msg, strlen(msg), 1, 600, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }
   else
   {
      const char *msg = msg_hash_to_str(MSG_UPNP_FAILED);
      RARCH_ERR("[Netplay] %s\n", msg);
      runloop_msg_queue_push(msg, strlen(msg), 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

/**
 * netplay_init_nat_traversal
 *
 * Initialize the NAT traversal library and try to open a port
 */
static void netplay_init_nat_traversal(netplay_t *netplay)
{
   net_driver_state_t *net_st = &networking_driver_st;

   task_push_netplay_nat_traversal(&net_st->nat_traversal_request,
      netplay->tcp_port);
}

static void netplay_deinit_nat_traversal(void)
{
   net_driver_state_t *net_st = &networking_driver_st;
   task_push_netplay_nat_close(&net_st->nat_traversal_request);
}

static int init_tcp_connection(netplay_t *netplay, const struct addrinfo *addr,
      bool is_server, bool is_mitm)
{
   char msg[512];
   char host[256], port[6];
   const char *_msg = NULL;
   int fd           = socket(addr->ai_family, addr->ai_socktype,
      addr->ai_protocol);

   if (fd < 0)
      return -1;

   SET_TCP_NODELAY(fd)
   SET_FD_CLOEXEC(fd)

   if (!is_server)
   {
      if (socket_connect_with_timeout(fd, (void*)addr, 10000))
      {
         /* If we are connecting to a tunnel server,
            we must also send our session linking request. */
         if (!netplay->mitm_session_id.magic ||
               socket_send_all_blocking_with_timeout(fd,
                  &netplay->mitm_session_id, sizeof(netplay->mitm_session_id),
                  5000, true))
            return fd;
      }

      if (!getnameinfo_retro(addr->ai_addr, addr->ai_addrlen,
            host, sizeof(host), port, sizeof(port),
            NI_NUMERICHOST | NI_NUMERICSERV))
      {
         snprintf(msg, sizeof(msg),
            "Failed to connect to host %s on port %s.",
            host, port);
         _msg = msg;
      }
      else
         _msg = "Failed to connect to host.";
   }
   else if (is_mitm)
   {
      if (socket_connect_with_timeout(fd, (void*)addr, 10000))
      {
         mitm_id_t new_session = {0};

         /* To request a new session,
            we send the magic with the rest of the ID zeroed. */
         new_session.magic = htonl(MITM_SESSION_MAGIC);

         /* Tunnel server should provide us with our session ID. */
         if (   socket_send_all_blocking_with_timeout(fd,
                  &new_session, sizeof(new_session), 5000, true)
             && socket_receive_all_blocking_with_timeout(fd,
                  &netplay->mitm_session_id,
                  sizeof(netplay->mitm_session_id),
                  5000))
         {
            if (ntohl(netplay->mitm_session_id.magic) == MITM_SESSION_MAGIC &&
                  memcmp(netplay->mitm_session_id.unique, new_session.unique,
                     sizeof(netplay->mitm_session_id.unique)))
            {
               /* Initialize data for handling tunneled client connections. */
               netplay->mitm_handler = (struct netplay_mitm_handler*)
                  calloc(1, sizeof(*netplay->mitm_handler));
               if (netplay->mitm_handler)
               {
                  size_t i;

                  netplay->mitm_handler->addr = addr;

                  for (i = 0; i < ARRAY_SIZE(netplay->mitm_handler->pending); i++)
                     netplay->mitm_handler->pending[i].fd = -1;

                  return fd;
               }
            }
         }

         _msg = "Failed to create a tunnel session.";
      }
      else
      {
         if (!getnameinfo_retro(addr->ai_addr, addr->ai_addrlen,
               host, sizeof(host), port, sizeof(port),
               NI_NUMERICHOST | NI_NUMERICSERV))
         {
            snprintf(msg, sizeof(msg),
               "Failed to connect to relay server %s on port %s.",
               host, port);
            _msg = msg;
         }
         else
            _msg = "Failed to connect to relay server.";
      }
   }
   else
   {
#if defined(HAVE_INET6) && defined(IPV6_V6ONLY)
      /* Make sure we accept connections on both IPv6 and IPv4. */
      if (addr->ai_family == AF_INET6)
      {
         int on = 0;

         if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
               (const char*)&on, sizeof(on)) < 0)
            RARCH_WARN("[Netplay] Failed to listen on both IPv6 and IPv4.\n");
      }
#endif

      if (socket_bind(fd, (void*)addr))
      {
         if (!listen(fd, 64) && socket_nonblock(fd))
            return fd;
      }
      else
      {
         if (!getnameinfo_retro(addr->ai_addr, addr->ai_addrlen,
               NULL, 0, port, sizeof(port), NI_NUMERICSERV))
         {
            snprintf(msg, sizeof(msg),
               "Failed to bind port %s.",
               port);
            _msg = msg;
         }
         else
            _msg = "Failed to bind port.";
      }
   }

   socket_close(fd);

   if (_msg)
      RARCH_ERR("[Netplay] %s\n", _msg);

   return -1;
}

static bool init_tcp_socket(netplay_t *netplay,
      const char *server, const char *mitm, uint16_t port)
{
   char port_buf[6];
   const struct addrinfo *tmp_info;
   struct addrinfo *addr = NULL;
   struct addrinfo hints = {0};
   bool is_mitm          = !server && mitm;
   int fd                = -1;

   if (!network_init())
      return false;

   if (!server)
   {
      if (!is_mitm)
      {
         hints.ai_flags  = AI_PASSIVE;
#ifdef HAVE_INET6
         /* Default to hosting on IPv6 and IPv4 */
         hints.ai_family = AF_INET6;
#else
         hints.ai_family = AF_INET;
#endif
      }
      else /* IPv4 only for relay servers. */
         hints.ai_family = AF_INET;
   }
   hints.ai_socktype = SOCK_STREAM;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   hints.ai_flags |= AI_NUMERICSERV;

   if (getaddrinfo_retro(is_mitm ? mitm : server, port_buf,
      &hints, &addr))
   {
      if (!server && !is_mitm)
      {
#ifdef HAVE_INET6
try_ipv4:
         /* Didn't work with IPv6, try IPv4 */
         hints.ai_family = AF_INET;
         if (getaddrinfo_retro(server, port_buf, &hints, &addr))
#endif
         {
            RARCH_ERR("[Netplay] Failed to set a hosting address.\n");
            return false;
         }
      }
      else
      {
         RARCH_ERR("[Netplay] Failed to resolve host: %s\n",
            is_mitm ? mitm : server);
         return false;
      }
   }

   if (!addr)
      return false;

   /* If we're serving on IPv6, make sure we accept all connections, including
    * IPv4 */
#ifdef HAVE_INET6
   if (!server && !is_mitm && addr->ai_family == AF_INET6)
   {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr->ai_addr;

#if defined(_MSC_VER) && _MSC_VER <= 1200
      IN6ADDR_SETANY(sin6);
#else
      sin6->sin6_addr           = in6addr_any;
#endif
   }
#endif

   /* If "localhost" is used, it is important to check every possible
    * address for IPv4/IPv6. */
   tmp_info = addr;

   do
   {
      fd = init_tcp_connection(netplay, tmp_info, !server, is_mitm);
      if (fd >= 0)
         break;
   } while ((tmp_info = tmp_info->ai_next));

   if (netplay->mitm_handler && netplay->mitm_handler->addr)
      netplay->mitm_handler->base_addr = addr;
   else
      freeaddrinfo_retro(addr);
   addr = NULL;

   if (fd < 0)
   {
#ifdef HAVE_INET6
      if (!server && !is_mitm && hints.ai_family == AF_INET6)
         goto try_ipv4;
#endif
      RARCH_ERR("[Netplay] Failed to set up netplay sockets.\n");
      return false;
   }

   if (server)
   {
      netplay->connections[0].flags |= NETPLAY_CONN_FLAG_ACTIVE;
      netplay->connections[0].fd     = fd;
   }
   else
   {
      netplay->listen_fd             = fd;
#ifdef HAVE_NETPLAYDISCOVERY_NSNET
      netplay_mdns_publish(netplay);
#endif
   }

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
      if (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
      {
         if (connection->send_packet_buffer.data)
         {
            if (  !netplay_resize_socket_buffer(
                   &connection->send_packet_buffer,
                   packet_buffer_size)
                || !netplay_resize_socket_buffer(
                   &connection->recv_packet_buffer,
                   packet_buffer_size))
               return false;
         }
         else
         {
            if (  !netplay_init_socket_buffer(
                     &connection->send_packet_buffer,
                     packet_buffer_size)
                || !netplay_init_socket_buffer(
                     &connection->recv_packet_buffer,
                     packet_buffer_size))
               return false;
         }
      }
   }

   return true;
}

static void netplay_write_block_header(unsigned char* output, const char* header, size_t len)
{
   memcpy(output, header, 4);
   output[4] = ((len) & 0xFF);
   output[5] = ((len >> 8) & 0xFF);
   output[6] = ((len >> 16) & 0xFF);
   output[7] = ((len >> 24) & 0xFF);
}

static bool netplay_init_serialization(netplay_t *netplay)
{
   size_t i;

   if (netplay->state_size)
      return true;

   if (netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE)
   {
      /* max core netpacket size is 64 kb, hold 2 of them */
      netplay->state_size = 64*1024*2;
   }
   else
   {
      size_t info_size = core_serialize_size_special();
      if (!info_size)
         return false;

      netplay->coremem_size = info_size;

      /* 8-byte identifier, 8-byte block header, content, 8-byte terminator */
      info_size = 8 + 8 + CONTENT_ALIGN_SIZE(info_size) + 8;

#ifdef HAVE_CHEEVOS
      {
         const settings_t* settings = config_get_ptr();
         if (settings->bools.cheevos_enable)
         {
            /* 8-byte flags + content */
            netplay->cheevos_size = 8 + rcheevos_get_serialize_size();
         }
         else
         {
            /* just 8-byte flags */
            netplay->cheevos_size = 8;
         }

         /* 8-byte block header + content */
         info_size += 8 + CONTENT_ALIGN_SIZE(netplay->cheevos_size);
      }
#endif

      netplay->state_size = info_size;
   }

   for (i = 0; i < netplay->buffer_size; i++)
   {
      netplay->buffer[i].state = calloc(1, netplay->state_size);
      if (!netplay->buffer[i].state)
         return false;
   }

   netplay->zbuffer_size    = netplay->state_size * 2;
   netplay->zbuffer         = (uint8_t*)calloc(1, netplay->zbuffer_size);
   if (!netplay->zbuffer)
   {
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
static bool netplay_try_init_serialization(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   if (netplay->state_size)
      return true;

   if (!netplay_init_serialization(netplay))
      return false;

   /* Check if we can actually save. */
   serial_info.data_const = NULL;
   serial_info.data       = netplay->buffer[netplay->run_ptr].state;
   serial_info.size       = netplay->coremem_size;
   if (!core_serialize_special(&serial_info))
      return false;

   /* Once initialized, we no longer exhibit this quirk. */
   netplay->quirks &= ~NETPLAY_QUIRK_INITIALIZATION;

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
   /* Enough to get ahead or behind by MAX_STALL_FRAMES frames,
      plus one for other remote clients,
      plus one to send the stall message. */
   netplay->buffer_size = NETPLAY_MAX_STALL_FRAMES + 2;

   /* If we're the server,
      we need enough to get ahead AND behind by MAX_STALL_FRAMES frame. */
   if (netplay->is_server)
      netplay->buffer_size *= 2;

   netplay->buffer = (struct delta_frame*)calloc(netplay->buffer_size,
      sizeof(*netplay->buffer));
   if (!netplay->buffer)
      return false;

   if (!(netplay->quirks & NETPLAY_QUIRK_INITIALIZATION))
   {
      if (!netplay_init_serialization(netplay))
         return false;
   }

   return netplay_init_socket_buffers(netplay);
}

/**
 * netplay_free
 * @netplay              : pointer to netplay object
 *
 * Frees netplay data.
 */
static void netplay_free(netplay_t *netplay)
{
   size_t i;

   if (netplay->listen_fd >= 0)
      socket_close(netplay->listen_fd);

   if (netplay->mitm_handler)
   {
      for (i = 0; i < ARRAY_SIZE(netplay->mitm_handler->pending); i++)
      {
         int fd = netplay->mitm_handler->pending[i].fd;

         if (fd >= 0)
            socket_close(fd);
      }

      freeaddrinfo_retro(netplay->mitm_handler->base_addr);
      free(netplay->mitm_handler);
   }

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];

      if (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
      {
         socket_close(connection->fd);
         netplay_deinit_socket_buffer(&connection->send_packet_buffer);
         netplay_deinit_socket_buffer(&connection->recv_packet_buffer);
      }
   }

   free(netplay->connections);
   free(netplay->ban_list.list);

   if (netplay->buffer)
   {
      for (i = 0; i < netplay->buffer_size; i++)
         netplay_delta_frame_free(&netplay->buffer[i]);

      free(netplay->buffer);
   }

   free(netplay->zbuffer);

   if (netplay->compress_nil.compression_stream)
      netplay->compress_nil.compression_backend->stream_free(
         netplay->compress_nil.compression_stream);
   if (netplay->compress_nil.decompression_stream)
      netplay->compress_nil.decompression_backend->stream_free(
         netplay->compress_nil.decompression_stream);
   if (netplay->compress_zlib.compression_stream)
      netplay->compress_zlib.compression_backend->stream_free(
         netplay->compress_zlib.compression_stream);
   if (netplay->compress_zlib.decompression_stream)
      netplay->compress_zlib.decompression_backend->stream_free(
         netplay->compress_zlib.decompression_stream);

   free(netplay);
}

/**
 * netplay_new:
 * @server               : IP address of server.
 * @mitm                 : IP address of the MITM/tunnel server.
 * @port                 : Port of server.
 * @mitm_session         : Session id for MITM/tunnel.
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
static netplay_t *netplay_new(const char *server, const char *mitm,
      uint16_t port, const char *mitm_session,
      uint32_t check_frames, const struct retro_callbacks *cb,
      bool nat_traversal, const char *nick, uint32_t quirks,
      enum netplay_modus modus)
{
   netplay_t *netplay        = (netplay_t*)calloc(1, sizeof(*netplay));

   if (!netplay)
      return NULL;

   netplay->is_server        = !server;
   netplay->check_frames     = check_frames;
   netplay->cbs              = *cb;
   netplay->quirks           = quirks;
   netplay->modus            = modus;
   netplay->crcs_valid       = true;
   netplay->listen_fd        = -1;
   netplay->next_announce    = -1;
   netplay->next_ping        = -1;
   netplay->simple_rand_next = 1;

   strlcpy(netplay->nick,
      !string_is_empty(nick) ? nick : RARCH_DEFAULT_NICK,
      sizeof(netplay->nick));

   netplay_key_init(netplay);

   if (netplay->is_server)
   {
      unsigned i;
      settings_t *settings = config_get_ptr();

      netplay->tcp_port     = port;
      netplay->ext_tcp_port = port;

      if (!mitm)
         netplay->nat_traversal = nat_traversal;

      for (i = 0; i < MAX_INPUT_DEVICES; i++)
      {
         uint32_t device = input_config_get_device(i);

         netplay->config_devices[i] = device;

         switch (device & RETRO_DEVICE_MASK)
         {
            case RETRO_DEVICE_KEYBOARD:
               netplay->have_updown_device = true;
            case RETRO_DEVICE_JOYPAD:
            case RETRO_DEVICE_MOUSE:
            case RETRO_DEVICE_LIGHTGUN:
            case RETRO_DEVICE_ANALOG:
            case RETRO_DEVICE_NONE:
               break;
            default:
               RARCH_WARN("[Netplay] Netplay does not support input device %u.\n",
                  i + 1);
               break;
         }
      }

      netplay->allow_pausing =
         settings->bools.netplay_allow_pausing;
      netplay->input_latency_frames_min =
         settings->uints.netplay_input_latency_frames_min;
      if (settings->bools.run_ahead_enabled)
         netplay->input_latency_frames_min -=
            settings->uints.run_ahead_frames;
      netplay->input_latency_frames_max =
         netplay->input_latency_frames_min +
         settings->uints.netplay_input_latency_frames_range;
   }
   else
   {
      netplay->connections       =
         (struct netplay_connection*)calloc(1, sizeof(*netplay->connections));
      if (!netplay->connections)
         goto failure;
      netplay->connections_size  = 1;

      netplay->connections[0].fd = -1;

      if (!string_is_empty(mitm_session))
      {
         int           flen = 0;
         unsigned char *buf =
            unbase64(mitm_session, (int)strlen(mitm_session), &flen);

         if (!buf)
            goto failure;
         if (flen != sizeof(netplay->mitm_session_id.unique))
         {
            free(buf);
            goto failure;
         }

         netplay->mitm_session_id.magic = htonl(MITM_SESSION_MAGIC);
         memcpy(netplay->mitm_session_id.unique, buf, flen);
         free(buf);
      }

      netplay->allow_pausing = true;

      /* Clients get device info from the server. */
   }

   if (     !init_tcp_socket(netplay, server, mitm, port)
         || !netplay_init_buffers(netplay))
      goto failure;

   return netplay;

failure:
   netplay_free(netplay);

   return NULL;
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
   struct compression_transcoder *z, bool is_legacy_data)
{
   uint32_t header[4];
   uint32_t rd, wn;
   size_t i;
   bool has_legacy_connection = false;
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

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
      struct netplay_connection* connection = &netplay->connections[i];
      bool can_send;

      /* if is_legacy_data is false, only send to peers on protocol 7 or higher */
      REQUIRE_PROTOCOL_VERSION(connection, 7)
         can_send = !is_legacy_data;
      else
         can_send = is_legacy_data;

      if (can_send)
      {
         if ( (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
            ||  (connection->mode < NETPLAY_CONNECTION_CONNECTED)
            ||  (connection->compression_supported != cx))
            continue;

         if (  !netplay_send(&connection->send_packet_buffer,
                 connection->fd, header, sizeof(header))
            || !netplay_send(&connection->send_packet_buffer,
                 connection->fd, netplay->zbuffer, wn))
            netplay_hangup(netplay, connection);
      }
      else
      {
         has_legacy_connection = true;
      }
   }

   if (has_legacy_connection && !is_legacy_data)
   {
      /* at least one peer is not on protocol 7 or higher. extract the coremem segment
       * and only send it. */
      const uint8_t* input = netplay_get_savestate_coremem(netplay,
            (const uint8_t*)serial_info->data_const);

      if (input != serial_info->data_const)
      {
         serial_info->data_const = input;
         serial_info->size = netplay->coremem_size;

         netplay_send_savestate(netplay, serial_info, cx, z, true);
      }
   }
}

/**
 * netplay_frontend_paused
 * @netplay              : pointer to netplay object
 * @paused               : true if frontend is paused
 *
 * Inform Netplay of the frontend's pause state (paused or otherwise)
 */
static void netplay_frontend_paused(netplay_t *netplay, bool paused)
{
   size_t i;
   uint32_t paused_ct    = 0;

   /* When a core uses the netpacket interface netplay doesn't control pause.
    * We need this because even if RARCH_NETPLAY_CTL_ALLOW_PAUSE returns false
    * on some platforms the frontend may try to force netplay to pause. */
   if (netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE)
      return;

   netplay->local_paused = paused;

   /* Communicating this is a bit odd: If exactly one other connection is
    * paused, then we must tell them that we're unpaused, as from their
    * perspective we are. If more than one other connection is paused, then our
    * status as proxy means we are NOT unpaused to either of them. */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (connection->flags &
                ((NETPLAY_CONN_FLAG_ACTIVE | NETPLAY_CONN_FLAG_PAUSED)
            ==  (NETPLAY_CONN_FLAG_ACTIVE | NETPLAY_CONN_FLAG_PAUSED)))
         paused_ct++;
   }

   if (paused_ct > 1)
      return;

   /* Send our unpaused status. Must send manually because we must immediately
    * flush the buffer: If we're paused, we won't be polled. */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (     (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (connection->mode >= NETPLAY_CONNECTION_CONNECTED))
      {
         if (paused)
            netplay_send_raw_cmd(netplay, connection, NETPLAY_CMD_PAUSE,
               netplay->nick, sizeof(netplay->nick));
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

/**
 * netplay_core_reset
 * @netplay              : pointer to netplay object
 *
 * Indicate that the core has been reset to netplay peers
 **/
static void netplay_core_reset(netplay_t *netplay)
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
      if (   (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
            || (connection->mode < NETPLAY_CONNECTION_CONNECTED))
         continue;

      if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmd,
               sizeof(cmd)))
         netplay_hangup(netplay, connection);
   }
}

static bool netplay_process_savestate1(retro_ctx_serialize_info_t* serial_info)
{
#ifdef HAVE_CHEEVOS
   const settings_t* settings = config_get_ptr();
#endif
   const uint8_t* input       = serial_info->data_const;
   const uint8_t* stop        = input + serial_info->size;
   bool seen_core             = false;

   input += 8; /* NETPLAY1 */

   while (input < stop)
   {
      size_t block_size     = (input[7] << 24 | input[6] << 16 |  input[5] << 8 | input[4]);
      const uint8_t *marker = input;

      input += 8;

      if (memcmp(marker, NETPLAYSTATE_MEM_BLOCK, 4) == 0)
      {
         retro_ctx_serialize_info_t serial_info;
         serial_info.data_const = (void*)input;
         serial_info.size       = block_size;
         if (!core_unserialize_special(&serial_info))
            return false;

         seen_core = true;
      }
#ifdef HAVE_CHEEVOS
      else if (memcmp(marker, NETPLAYSTATE_CHEEVOS_BLOCK, 4) == 0 && settings->bools.cheevos_enable)
      {
         const bool hardcore_state = (input[0] != 0);
         if (hardcore_state != rcheevos_hardcore_active() && !netplay_is_spectating())
         {
            const char *msg = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST);
            runloop_msg_queue_push(msg, strlen(msg), 0, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            if (!hardcore_state)
            {
               /* use pause_hardcore to ensure player can't re-enabled it on client side */
               rcheevos_pause_hardcore();
            }
            else
            {
               /* if the player had hardcore on, this will reset it to on. otherwise, it'll
                * stay off, even if the server is playing with hardcore restrictions now. we
                * don't want to force hardcore unlocks for a player that doesn't want them. */
               rcheevos_hardcore_enabled_changed();
            }
         }

         if (block_size > 8)
         {
            input += 8;
            rcheevos_set_serialized_data((void*)input);
         }
      }
#endif

      input += CONTENT_ALIGN_SIZE(block_size);
   }

   if (!seen_core)
      return false;

   return true;
}

static bool netplay_process_savestate(netplay_t* netplay, retro_ctx_serialize_info_t* serial_info)
{
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   /* if no NETPLAY marker, it's just raw core data */
   if (memcmp(serial_info->data_const, "NETPLAY", 7) != 0)
   {
      serial_info->size = netplay->coremem_size;
      return core_unserialize_special(serial_info);
   }

   switch (((uint8_t*)serial_info->data_const)[7])
   {
      case 1:
         return netplay_process_savestate1(serial_info);

      default:
         return false;
   }
}

static bool netplay_build_savestate(netplay_t* netplay, retro_ctx_serialize_info_t* serial_info, bool force_capture_achievements)
{
   uint8_t* buffer = (uint8_t*)serial_info->data;
   uint8_t* output = buffer;
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   memcpy(output, "NETPLAY", 7);
   output[7] = NETPLAYSTATE_VERSION;
   output += 8;

   /* important - write the unaligned size - some cores fail if they aren't passed the exact right size. */
   netplay_write_block_header(output, NETPLAYSTATE_MEM_BLOCK, netplay->coremem_size);
   output += 8;

   /* capture the core state */
   serial_info->data = output;
   serial_info->size = netplay->coremem_size;
   if (!core_serialize_special(serial_info))
      return false;

   output += CONTENT_ALIGN_SIZE(netplay->coremem_size);

#ifdef HAVE_CHEEVOS
   if (netplay->is_server || (force_capture_achievements && netplay->cheevos_size > 8))
   {
      const settings_t* settings = config_get_ptr();
      size_t cheevos_size = 8;

      if (settings->bools.cheevos_enable)
      {
         if (netplay->cheevos_size > 8 && rcheevos_get_serialized_data(output + 16))
            cheevos_size = netplay->cheevos_size;
      }

      netplay_write_block_header(output, NETPLAYSTATE_CHEEVOS_BLOCK, cheevos_size);
      output += 8;
      memset(output, 0, 8);
      output[0] = rcheevos_hardcore_active() ? 1 : 0;
      output += cheevos_size;
   }
#endif

   netplay_write_block_header(output, NETPLAYSTATE_END_BLOCK, 0);
   output += 8;

   serial_info->data_const = serial_info->data = buffer;
   serial_info->size = (output - buffer);
   return true;
}

void netplay_load_savestate(netplay_t *netplay,
      retro_ctx_serialize_info_t *serial_info, bool save)
{
   retro_ctx_serialize_info_t tmp_serial_info = {0};
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

   if (!serial_info)
      save = true;

   netplay_force_future(netplay);

   /* Record it in our own buffer. */
   if (save)
   {
      /* TODO/FIXME: This is a critical failure! */
      if (!netplay_delta_frame_ready(netplay,
            &netplay->buffer[netplay->run_ptr], netplay->run_frame_count))
         return;

      if (!serial_info)
      {
         tmp_serial_info.data = netplay->buffer[netplay->run_ptr].state;
         if (!netplay_build_savestate(netplay, &tmp_serial_info, false))
            return;
         serial_info                = &tmp_serial_info;
      }
      else if (serial_info->size <= netplay->state_size)
         memcpy(netplay->buffer[netplay->run_ptr].state,
            serial_info->data_const, serial_info->size);
   }

   /* Don't send it if we're expected to be desynced. */
   if (!netplay->desync)
   {
      /* Send this to every peer. */
      if (netplay->compress_nil.compression_backend)
         netplay_send_savestate(netplay, serial_info, 0,
            &netplay->compress_nil, false);
      if (netplay->compress_zlib.compression_backend)
         netplay_send_savestate(netplay, serial_info, NETPLAY_COMPRESSION_ZLIB,
            &netplay->compress_zlib, false);
   }
}

/**
 * netplay_toggle_play_spectate
 *
 * Toggle between play mode and spectate mode
 */
static void netplay_toggle_play_spectate(netplay_t *netplay)
{
   switch (netplay->self_mode)
   {
      case NETPLAY_CONNECTION_PLAYING:
      case NETPLAY_CONNECTION_SLAVE:
         {
            /* Switch to spectator mode immediately.
               Host switches to spectator on netplay_cmd_mode. */
            if (!netplay->is_server)
            {
               int i;
               uint32_t client_num = netplay->self_client_num;
               uint32_t client_mask = ~(1 << client_num);

               netplay->connected_players &= client_mask;

               netplay->client_devices[netplay->self_client_num] = 0;
               for (i = 0; i < MAX_INPUT_DEVICES; i++)
                  netplay->device_clients[i] &= client_mask;
               netplay->self_devices = 0;

               netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;

               netplay_announce_play_spectate(netplay, NULL,
                  NETPLAY_CONNECTION_SPECTATING, 0, -1, client_num);
            }

            netplay_cmd_mode(netplay, NETPLAY_CONNECTION_SPECTATING);
         }
         break;
      case NETPLAY_CONNECTION_SPECTATING:
         /* Switch only after getting permission */
         netplay_cmd_mode(netplay, NETPLAY_CONNECTION_PLAYING);
         break;
      default:
         break;
   }
}

static int16_t netplay_input_state(netplay_t *netplay,
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
            unsigned key = netplay_key_hton(netplay, id);
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
   NETPLAY_ASSERT_MODUS(NETPLAY_MODUS_INPUT_FRAME_SYNC);

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
               state[1]      = (uint16_t)tmp_x | (((uint16_t)tmp_y) << 16);
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
                              netplay_key_ntoh(netplay, key)) ?
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
      ptr->have_real[netplay->self_client_num]            = true;
      netplay->read_ptr[netplay->self_client_num]         = NEXT_PTR(netplay->self_ptr);
      netplay->read_frame_count[netplay->self_client_num] = netplay->self_frame_count + 1;
   }

   /* And send this input to our peers */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (     (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (connection->mode >= NETPLAY_CONNECTION_CONNECTED))
         netplay_send_cur_input(netplay, &netplay->connections[i]);
   }

   /* Handle any delayed state changes */
   if (netplay->is_server)
      netplay_delayed_state_change(netplay);

   return true;
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
static bool netplay_poll(netplay_t *netplay, bool block_libretro_input)
{
   size_t i;

   /* Use simplified polling loop when a core uses the netpacket interface. */
   if (netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE)
   {
      if (netplay->self_mode == NETPLAY_CONNECTION_NONE)
         return true;

      /* Read netplay input. */
      netplay_poll_net_input(netplay);

      if (networking_driver_st.core_netpacket_interface
            && networking_driver_st.core_netpacket_interface->poll
            && netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
         networking_driver_st.core_netpacket_interface->poll();
      return true;
   }

   if (!get_self_input_state(block_libretro_input, netplay))
      goto catastrophe;

   /* If we're not connected, we're done. */
   if (netplay->self_mode == NETPLAY_CONNECTION_NONE)
      return true;

   netplay_update_unread_ptr(netplay);

   /* Read netplay input. */
   netplay_poll_net_input(netplay);

   /* Resolve and/or simulate the input if we don't have real input. */
   netplay_resolve_input(netplay, netplay->run_ptr, false);

   /* Handle slaves. */
   if (netplay->is_server && netplay->connected_slaves)
      netplay_handle_slaves(netplay);

   netplay_update_unread_ptr(netplay);

   /* Figure out how many frames of input latency we should be using to
      hide network latency. */
   if (netplay->frame_run_time_avg)
   {
      /* FIXME: Using fixed 60fps for this calculation */
      unsigned frames_per_frame    = netplay->frame_run_time_avg ?
         (unsigned)(16666 / netplay->frame_run_time_avg) : 0;
      unsigned frames_ahead        =
         (netplay->run_frame_count > netplay->unread_frame_count) ?
            (unsigned)(netplay->run_frame_count - netplay->unread_frame_count)
            : 0;
      int input_latency_frames_min = (int)netplay->input_latency_frames_min;
      int input_latency_frames_max = (int)netplay->input_latency_frames_max;

      /* Assume we need a couple frames worth of time
         to actually run the current frame. */
      if (frames_per_frame > 2)
         frames_per_frame -= 2;
      else
         frames_per_frame  = 0;

      /* We can't hide this much network latency with replay,
         so hide some with input latency. */
      if (      netplay->input_latency_frames < input_latency_frames_min
            || (frames_per_frame < frames_ahead
            && netplay->input_latency_frames < input_latency_frames_max))
         netplay->input_latency_frames++;
      /* We don't need this much latency (any more). */
      else if (netplay->input_latency_frames > input_latency_frames_max
            || (frames_per_frame > (frames_ahead + 2)
            && netplay->input_latency_frames > input_latency_frames_min))
         netplay->input_latency_frames--;
   }

   /* If we're stalled, consider unstalling. */
   switch (netplay->stall)
   {
      case NETPLAY_STALL_RUNNING_FAST:
         if ((netplay->unread_frame_count + NETPLAY_MAX_STALL_FRAMES - 2) >
               netplay->self_frame_count)
         {
            struct netplay_connection *connection;

            for (i = 0; i < netplay->connections_size; i++)
            {
               connection = &netplay->connections[i];
               if (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
                  connection->stall = NETPLAY_STALL_NONE;
            }

            netplay->stall = NETPLAY_STALL_NONE;
         }
         break;
      case NETPLAY_STALL_SPECTATOR_WAIT:
         if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING ||
               netplay->unread_frame_count > netplay->self_frame_count)
            netplay->stall = NETPLAY_STALL_NONE;
         break;
      case NETPLAY_STALL_INPUT_LATENCY:
         /* Just let it recalculate momentarily. */
         netplay->stall = NETPLAY_STALL_NONE;
         break;
      case NETPLAY_STALL_SERVER_REQUESTED:
         {
            struct netplay_connection *connection = &netplay->connections[0];

            /* See if the stall is done. */
            if (!connection->stall_frame)
            {
               /* Stop stalling! */
               connection->stall = NETPLAY_STALL_NONE;
               netplay->stall    = NETPLAY_STALL_NONE;
            }
            else
               connection->stall_frame--;
         }
         break;
      default:
         /* Not stalling. */
         break;
   }

   /* If we're not stalled, consider stalling. */
   if (netplay->stall == NETPLAY_STALL_NONE)
   {
      switch (netplay->self_mode)
      {
         case NETPLAY_CONNECTION_SPECTATING:
         case NETPLAY_CONNECTION_SLAVE:
            /* If we're a spectator, are we ahead at all? */
            if (!netplay->is_server &&
                  netplay->unread_frame_count <= netplay->self_frame_count)
            {
               netplay->stall      = NETPLAY_STALL_SPECTATOR_WAIT;
               netplay->stall_time = cpu_features_get_time_usec();
            }
            break;
         case NETPLAY_CONNECTION_PLAYING:
            /* Have we not read enough latency frames? */
            if (netplay->connected_players &&
                  (netplay->run_frame_count + netplay->input_latency_frames) >
                     netplay->self_frame_count)
            {
               netplay->stall      = NETPLAY_STALL_INPUT_LATENCY;
               netplay->stall_time = 0;
            }
            break;
         default:
            break;
      }

      /* Are we too far ahead? */
      if (netplay->stall == NETPLAY_STALL_NONE &&
            netplay->self_frame_count >= NETPLAY_MAX_STALL_FRAMES)
      {
         uint32_t min_frame_count = netplay->self_frame_count -
            NETPLAY_MAX_STALL_FRAMES;

         if (netplay->unread_frame_count <= min_frame_count)
         {
            netplay->stall      = NETPLAY_STALL_RUNNING_FAST;
            netplay->stall_time = cpu_features_get_time_usec();

            /* Figure out who to blame. */
            if (netplay->is_server)
            {
               struct netplay_connection *connection;

               for (i = 0; i < netplay->connections_size; i++)
               {
                  connection = &netplay->connections[i];
                  if (     (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
                        ||   (connection->mode != NETPLAY_CONNECTION_PLAYING))
                     continue;

                  if (netplay->read_frame_count[i + 1] <= min_frame_count)
                  {
                     connection->stall = NETPLAY_STALL_RUNNING_FAST;
                     connection->stall_slow++;
                  }
               }
            }
         }
      }
   }

   /* If we're stalling, consider disconnection. */
   if (netplay->stall != NETPLAY_STALL_NONE && netplay->stall_time)
   {
      retro_time_t now = cpu_features_get_time_usec();

      if (!netplay->remote_paused)
      {
         retro_time_t delta = now - netplay->stall_time;

         if (netplay->is_server)
         {
            if (delta >= MAX_SERVER_STALL_TIME_USEC)
            {
               /* Stalled out! */
               struct netplay_connection *connection;

               for (i = 0; i < netplay->connections_size; i++)
               {
                  connection = &netplay->connections[i];
                  if (   (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
                        || (connection->mode != NETPLAY_CONNECTION_PLAYING))
                     continue;

                  if (connection->stall != NETPLAY_STALL_NONE)
                     netplay_hangup(netplay, connection);
               }

               netplay->stall = NETPLAY_STALL_NONE;
            }
         }
         else
         {
            if (delta >= MAX_CLIENT_STALL_TIME_USEC)
               /* Stalled out! */
               goto catastrophe;
         }
      }
      else
         /* Don't stall out while they're paused. */
         netplay->stall_time = now;
   }

   return true;

catastrophe:
   for (i = 0; i < netplay->connections_size; i++)
      netplay_hangup(netplay, &netplay->connections[i]);

   return false;
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
   return netplay->is_server ||
      netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED;
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

   return netplay->is_replay &&
      netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED;
}

bool init_netplay_deferred(const char *server, unsigned port, const char *mitm_session)
{
   net_driver_state_t *net_st = &networking_driver_st;

   if (string_is_empty(server) || !port)
   {
      net_st->flags &= ~NET_DRIVER_ST_FLAG_NETPLAY_CLIENT_DEFERRED;
      return false;
   }

   strlcpy(net_st->server_address_deferred, server,
      sizeof(net_st->server_address_deferred));
   net_st->server_port_deferred    = port;
   strlcpy(net_st->server_session_deferred, mitm_session,
      sizeof(net_st->server_session_deferred));
   net_st->flags |= NET_DRIVER_ST_FLAG_NETPLAY_CLIENT_DEFERRED;

   return true;
}

/**
 * input_poll_net
 * @netplay              : pointer to netplay object
 *
 * Poll the network if necessary.
 */
void input_poll_net(netplay_t *netplay)
{
   if (!netplay_should_skip(netplay))
   {
      input_driver_state_t *input_st = input_state_get_ptr();

      netplay_poll(netplay, input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT);
   }
}

/* Netplay polling callbacks */
void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   net_driver_state_t *net_st  = &networking_driver_st;
   netplay_t          *netplay = net_st->data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.frame_cb(data, width, height, pitch);
}

void audio_sample_net(int16_t left, int16_t right)
{
   net_driver_state_t *net_st  = &networking_driver_st;
   netplay_t          *netplay = net_st->data;
   if (!netplay_should_skip(netplay) && !netplay->stall)
      netplay->cbs.sample_cb(left, right);
}

size_t audio_sample_batch_net(const int16_t *data, size_t frames)
{
   net_driver_state_t *net_st  = &networking_driver_st;
   netplay_t          *netplay = net_st->data;
   if (!netplay_should_skip(netplay) && !netplay->stall)
      return netplay->cbs.sample_batch_cb(data, frames);
   return frames;
}

static void netplay_announce_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   char *buf, *buf_data;
   size_t remaining;
   http_transfer_data_t *data     = (http_transfer_data_t*)task_data;
   net_driver_state_t  *net_st    = &networking_driver_st;
   struct netplay_room *host_room = &net_st->host_room;
   bool first                     = !host_room->id;

   /* Abort if netplay is not initialized. */
   if (!net_st->data)
      return;

   net_st->data->next_announce =
      cpu_features_get_time_usec() + NETPLAY_ANNOUNCE_TIME;

   if (error || !data || !data->data || !data->len || data->status != 200)
   {
      RARCH_ERR("[Netplay] Failed to announce session to the lobby server.");
      return;
   }

   buf = (char*)malloc(data->len);
   if (!buf)
      return;
   memcpy(buf, data->data, data->len);

   buf_data  = buf;
   remaining = data->len;
   do
   {
      char *delim;
      char *key, *value;
      char *lnbreak = (char*)memchr(buf_data, '\n', remaining);
      if (!lnbreak)
         break;
      *lnbreak++ = '\0';

      delim   = (char*)strchr(buf_data, '=');
      if (delim)
      {
         *delim = '\0';
         key    = buf_data;
         value  = delim + 1;

         if (!string_is_empty(key) && !string_is_empty(value))
         {
            if (string_is_equal(key, "id"))
               host_room->id = (int)strtol(value, NULL, 10);
            else if (string_is_equal(key, "username"))
               strlcpy(host_room->nickname, value,
                  sizeof(host_room->nickname));
            else if (string_is_equal(key, "core_name"))
               strlcpy(host_room->corename, value,
                  sizeof(host_room->corename));
            else if (string_is_equal(key, "game_name"))
               strlcpy(host_room->gamename, value,
                  sizeof(host_room->gamename));
            else if (string_is_equal(key, "game_crc"))
               host_room->gamecrc = (int)strtoul(value, NULL, 16);
            else if (string_is_equal(key, "core_version"))
               strlcpy(host_room->coreversion, value,
                  sizeof(host_room->coreversion));
            else if (string_is_equal(key, "ip"))
               strlcpy(host_room->address, value, sizeof(host_room->address));
            else if (string_is_equal(key, "port"))
               host_room->port = (int)strtol(value, NULL, 10);
            else if (string_is_equal(key, "host_method"))
               host_room->host_method = (int)strtol(value, NULL, 10);
            else if (string_is_equal(key, "has_password"))
               host_room->has_password =
                     string_is_equal_case_insensitive(value, "true")
                  || string_is_equal(value, "1");
            else if (string_is_equal(key, "has_spectate_password"))
               host_room->has_spectate_password =
                     string_is_equal_case_insensitive(value, "true")
                  || string_is_equal(value, "1");
            else if (string_is_equal(key, "retroarch_version"))
               strlcpy(host_room->retroarch_version, value,
                  sizeof(host_room->retroarch_version));
            else if (string_is_equal(key, "frontend"))
               strlcpy(host_room->frontend, value,
                  sizeof(host_room->frontend));
            else if (string_is_equal(key, "subsystem_name"))
               strlcpy(host_room->subsystem_name, value,
                  sizeof(host_room->subsystem_name));
            else if (string_is_equal(key, "country"))
               strlcpy(host_room->country, value, sizeof(host_room->country));
            else if (string_is_equal(key, "connectable"))
               host_room->connectable =
                     string_is_equal_case_insensitive(value, "true")
                  || string_is_equal(value, "1");
         }
      }

      remaining -= (size_t)lnbreak - (size_t)buf_data;
      buf_data   = lnbreak;
   } while (remaining);

   free(buf);

   /* Warn only on the first announce. */
   if (!host_room->connectable && first)
   {
      const char *_msg = msg_hash_to_str(MSG_ROOM_NOT_CONNECTABLE);
      RARCH_WARN("[Netplay] %s\n", msg_hash_to_str(MSG_ROOM_NOT_CONNECTABLE));
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

#ifdef HAVE_PRESENCE
   {
      presence_userdata_t userdata;

      userdata.status = PRESENCE_NETPLAY_HOSTING;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
   }
#endif
}

static void netplay_announce(netplay_t *netplay)
{
   char buf[4096];
   char frontend_architecture_tmp[24];
   const frontend_ctx_driver_t *frontend_drv;
   char *username                   = NULL;
   char *corename                   = NULL;
   char *coreversion                = NULL;
   char *gamename                   = NULL;
   char *subsystemname              = NULL;
   char *frontend_ident             = NULL;
   char *mitm_session               = NULL;
   char *mitm_custom_addr           = NULL;
   int mitm_custom_port             = 0;
   int is_mitm                      = 0;
   uint32_t content_crc             = 0;
   net_driver_state_t *net_st       = &networking_driver_st;
   struct netplay_room *host_room   = &net_st->host_room;
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;
   struct string_list *subsystem    = path_get_subsystem_list();
   settings_t *settings             = config_get_ptr();

   net_http_urlencode(&username, netplay->nick);

   strlcpy(buf, system->library_name, sizeof(host_room->corename));
   net_http_urlencode(&corename, buf);
   strlcpy(buf, system->library_version, sizeof(host_room->coreversion));
   net_http_urlencode(&coreversion, buf);

   if (subsystem && subsystem->size > 0)
   {
      unsigned i = 0;
      buf[0]     = '\0';
      for (;;)
      {
         /* TODO/FIXME - is last param OK here */
         strlcat(buf, path_basename(subsystem->elems[i].data),
            sizeof(host_room->gamename));
         if (++i >= subsystem->size)
            break;
         /* TODO/FIXME - is last param OK here */
         strlcat(buf, "|", sizeof(host_room->gamename));
      }

      net_http_urlencode(&gamename, buf);
      strlcpy(buf, path_get(RARCH_PATH_SUBSYSTEM),
         sizeof(host_room->subsystem_name));
      net_http_urlencode(&subsystemname, buf);
   }
   else
   {
      const char *basename = path_basename(path_get(RARCH_PATH_BASENAME));

      if (!string_is_empty(basename))
      {
         strlcpy(buf, basename, sizeof(host_room->gamename));
         net_http_urlencode(&gamename, buf);
      }
      else
         net_http_urlencode(&gamename, "N/A");

      net_http_urlencode(&subsystemname, "N/A");

      content_crc = content_get_crc();
   }

   frontend_drv =
      (const frontend_ctx_driver_t*)frontend_driver_get_cpu_architecture_str(
         frontend_architecture_tmp, sizeof(frontend_architecture_tmp));
   if (frontend_drv)
   {
      snprintf(buf, sizeof(host_room->frontend), "%s %s",
         frontend_drv->ident, frontend_architecture_tmp);
      net_http_urlencode(&frontend_ident, buf);
   }
   else
      net_http_urlencode(&frontend_ident, "N/A");

   if (!string_is_empty(host_room->mitm_session))
   {
      is_mitm = 1;
      net_http_urlencode(&mitm_session, host_room->mitm_session);
   }
   else
      net_http_urlencode(&mitm_session, "");

   if (is_mitm && string_is_equal(host_room->mitm_handle, "custom"))
   {
      net_http_urlencode(&mitm_custom_addr, host_room->mitm_address);
      mitm_custom_port = host_room->mitm_port;
   }
   else
      net_http_urlencode(&mitm_custom_addr, "");

   /* Estimated to a maximum of 3062 bytes. */
   snprintf(buf, sizeof(buf),
      "username=%s&"
      "core_name=%s&"
      "core_version=%s&"
      "game_name=%s&"
      "game_crc=%08lX&"
      "port=%d&"
      "mitm_server=%s&"
      "has_password=%d&"
      "has_spectate_password=%d&"
      "force_mitm=%d&"
      "retroarch_version=%s&"
      "frontend=%s&"
      "subsystem_name=%s&"
      "mitm_session=%s&"
      "mitm_custom_addr=%s&"
      "mitm_custom_port=%d",
      username,
      corename,
      coreversion,
      gamename,
      (unsigned long)content_crc,
      (int)netplay->ext_tcp_port,
      host_room->mitm_handle,
      !string_is_empty(settings->paths.netplay_password) ? 1 : 0,
      !string_is_empty(settings->paths.netplay_spectate_password) ? 1 : 0,
      is_mitm,
      PACKAGE_VERSION,
      frontend_ident,
      subsystemname,
      mitm_session,
      mitm_custom_addr,
      mitm_custom_port);

   if (task_push_http_post_transfer(FILE_PATH_LOBBY_LIBRETRO_URL "add", buf,
         true, NULL, netplay_announce_cb, NULL))
      /* Let the callback set the time for the next announce. */
      netplay->next_announce = -1;
   else
      netplay->next_announce =
         cpu_features_get_time_usec() + NETPLAY_ANNOUNCE_TIME;

   free(username);
   free(corename);
   free(coreversion);
   free(gamename);
   free(subsystemname);
   free(frontend_ident);
   free(mitm_session);
   free(mitm_custom_addr);
}

static void netplay_mitm_query_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   char *buf, *buf_data;
   size_t remaining;
   http_transfer_data_t *data     = (http_transfer_data_t*)task_data;
   net_driver_state_t  *net_st    = &networking_driver_st;
   struct netplay_room *host_room = &net_st->host_room;

   if (error || !data || !data->data || !data->len || data->status != 200)
   {
      RARCH_ERR("[Netplay] Failed to query the lobby server for tunnel information.");
      return;
   }

   buf = (char*)malloc(data->len);
   if (!buf)
      return;
   memcpy(buf, data->data, data->len);

   buf_data  = buf;
   remaining = data->len;
   do
   {
      char *delim;
      char *key, *value;
      char *lnbreak = (char*)memchr(buf_data, '\n', remaining);
      if (!lnbreak)
         break;
      *lnbreak++ = '\0';

      delim   = (char*)strchr(buf_data, '=');
      if (delim)
      {
         *delim = '\0';
         key    = buf_data;
         value  = delim + 1;

         if (!string_is_empty(key) && !string_is_empty(value))
         {
            if (string_is_equal(key, "tunnel_addr"))
               strlcpy(host_room->mitm_address, value,
                  sizeof(host_room->mitm_address));
            else if (string_is_equal(key, "tunnel_port"))
               host_room->mitm_port = (int)strtol(value, NULL, 10);
         }
      }

      remaining -= (size_t)lnbreak - (size_t)buf_data;
      buf_data   = lnbreak;
   } while (remaining);

   free(buf);
}

static bool netplay_mitm_query(const char *handle)
{
   net_driver_state_t  *net_st    = &networking_driver_st;
   struct netplay_room *host_room = &net_st->host_room;

   if (string_is_empty(handle))
      return false;

   /* We don't need to query,
      if we are using a custom relay server. */
   if (string_is_equal(handle, "custom"))
   {
      char addr[256];
      unsigned port             = 0;
      settings_t *settings      = config_get_ptr();
      const char *custom_server = settings->paths.netplay_custom_mitm_server;

      addr[0] = '\0';
      if (!netplay_decode_hostname(custom_server, addr, &port, NULL,
            sizeof(addr)))
         return false;
      if (!port)
         port = RARCH_DEFAULT_PORT;

      strlcpy(host_room->mitm_address, addr, sizeof(host_room->mitm_address));
      host_room->mitm_port = (int)port;
   }
   else
   {
      char query[256];
      size_t _len = strlcpy(query, FILE_PATH_LOBBY_LIBRETRO_URL "tunnel?name=",
            sizeof(query));
      strlcpy(query + _len, handle, sizeof(query) - _len);
      if (!task_push_http_transfer(query, true, NULL,
            netplay_mitm_query_cb, NULL))
         return false;
      /* Make sure we've the tunnel address before continuing. */
      task_queue_wait(NULL, NULL);
   }

   return !string_is_empty(host_room->mitm_address) && host_room->mitm_port;
}

int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   net_driver_state_t *net_st  = &networking_driver_st;
   netplay_t          *netplay = net_st->data;
   if (netplay)
   {
      if (netplay_is_alive(netplay))
         return netplay_input_state(netplay, port, device, idx, id);
      return netplay->cbs.state_cb(port, device, idx, id);
   }
   return 0;
}

/* ^^^ Netplay polling callbacks */

/**
 * netplay_disconnect
 * @netplay              : pointer to netplay object
 *
 * Disconnect netplay.
 *
 * Returns: true (1) if successful. At present, cannot fail.
 **/
static void netplay_disconnect(netplay_t *netplay)
{
   size_t i;

   for (i = 0; i < netplay->connections_size; i++)
      netplay_hangup(netplay, &netplay->connections[i]);

   deinit_netplay();

#ifdef HAVE_PRESENCE
   {
      presence_userdata_t userdata;
      userdata.status = PRESENCE_NETPLAY_NETPLAY_STOPPED;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
   }
#endif
}

/**
 * netplay_pre_frame:
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay.
 * Call this before running retro_run().
 *
 * Returns: true if the frontend is cleared to emulate the frame,
 * false if we're stalled or paused
 **/
static bool netplay_pre_frame(netplay_t *netplay)
{
   /* FIXME: This is an ugly way to learn we're not paused anymore */
   if (netplay->local_paused)
      netplay_frontend_paused(netplay, false);

   /* Are we ready now? */
   if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
      netplay_try_init_serialization(netplay);

   if (!netplay_sync_pre_frame(netplay))
   {
      netplay_disconnect(netplay);
      return true;
   }

   if (netplay->is_server)
   {
      settings_t *settings = config_get_ptr();

/* Vita can't bind to our discovery port;
   do not try to answer discovery queries there. */
#if defined(HAVE_NETPLAYDISCOVERY) && !defined(VITA)
      if (!netplay->mitm_handler)
      {
         net_driver_state_t *net_st = &networking_driver_st;

         /* Advertise our server */
         if (net_st->lan_ad_server_fd >= 0 || init_lan_ad_server_socket())
            netplay_lan_ad_server(netplay);
      }
#endif

      if (settings->bools.netplay_public_announce &&
            netplay->next_announce != -1)
      {
         if (cpu_features_get_time_usec() >= netplay->next_announce)
            netplay_announce(netplay);
      }
   }
   else
   {
      /* If we're disconnected, deinitialize. */
      if (!(netplay->connections[0].flags & NETPLAY_CONN_FLAG_ACTIVE))
      {
         netplay_disconnect(netplay);
         return true;
      }

      if (netplay->next_ping != -1)
      {
         retro_time_t ctime = cpu_features_get_time_usec();

         if (ctime >= netplay->next_ping)
         {
            netplay_request_ping(netplay, &netplay->connections[0]);
            netplay->next_ping = ctime + NETPLAY_PING_TIME;
         }
      }
   }

   if ((netplay->stall || netplay->remote_paused)
         && (!netplay->is_server || netplay->connected_players > 1)
         && netplay->modus == NETPLAY_MODUS_INPUT_FRAME_SYNC)
   {
      /* We may have received data even if we're stalled,
       * so run post-frame sync. */
      netplay_sync_input_post_frame(netplay, true);
      return false;
   }

   return true;
}

/**
 * netplay_post_frame:
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay.
 * We check if we have new input and replay from recorded input.
 * Call this after running retro_run().
 **/
static void netplay_post_frame(netplay_t *netplay)
{
   size_t i;

   /* When a core uses the netpacket interface frames are not synced */
   if (netplay->modus == NETPLAY_MODUS_INPUT_FRAME_SYNC)
   {
      netplay_update_unread_ptr(netplay);
      netplay_sync_input_post_frame(netplay, false);
   }

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];
      if (   (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
          && !netplay_send_flush(
             &connection->send_packet_buffer, connection->fd,
             false))
         netplay_hangup(netplay, connection);
   }

   /* If we're disconnected, deinitialize */
   if (     (!(netplay->is_server))
         && (!(netplay->connections[0].flags & NETPLAY_CONN_FLAG_ACTIVE)))
      netplay_disconnect(netplay);
}

void deinit_netplay(void)
{
   net_driver_state_t *net_st  = &networking_driver_st;
   netplay_t          *netplay = net_st->data;

   if (netplay)
   {
      if (netplay->nat_traversal)
         netplay_deinit_nat_traversal();

      netplay_free(netplay);

#ifdef HAVE_NETPLAYDISCOVERY
      deinit_lan_ad_server_socket();
#ifdef HAVE_NETPLAYDISCOVERY_NSNET
      netplay_mdns_unpublish();
#endif
#endif

      net_st->data              = NULL;
      net_st->flags            &= ~(NET_DRIVER_ST_FLAG_NETPLAY_ENABLED
                                |   NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT);

#if HAVE_RUNAHEAD
      /* Reinitialize preemptive frames if enabled */
      preempt_init(runloop_state_get_ptr());
#endif

      if (net_st->core_netpacket_interface
            && net_st->core_netpacket_interface->stop)
         net_st->core_netpacket_interface->stop();
   }

   free(net_st->client_info);
   net_st->client_info       = NULL;
   net_st->client_info_count = 0;

   core_unset_netplay_callbacks();
}

bool init_netplay(const char *server, unsigned port, const char *mitm_session)
{
   netplay_t *netplay;
   struct retro_callbacks cbs     = {0};
   uint64_t serialization_quirks  = 0;
   uint32_t quirks                = 0;
   settings_t *settings           = config_get_ptr();
   net_driver_state_t *net_st     = &networking_driver_st;
   struct netplay_room *host_room = &net_st->host_room;
   const char *mitm               = NULL;
   enum netplay_modus modus       = NETPLAY_MODUS_INPUT_FRAME_SYNC;

   if (!(net_st->flags & NET_DRIVER_ST_FLAG_NETPLAY_ENABLED))
      return false;

#ifdef HAVE_NETPLAYDISCOVERY
   net_st->lan_ad_server_fd       = -1;
#endif

   serialization_quirks = core_serialization_quirks();

   if (net_st->core_netpacket_interface)
      modus = NETPLAY_MODUS_CORE_PACKET_INTERFACE;

   if ((!core_info_current_supports_netplay()
         || serialization_quirks & (RETRO_SERIALIZATION_QUIRK_INCOMPLETE
                                 | RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION))
         && modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
   {
      const char *_msg = msg_hash_to_str(MSG_NETPLAY_UNSUPPORTED);
      RARCH_ERR("[Netplay] %s\n", msg_hash_to_str(MSG_NETPLAY_UNSUPPORTED));
      runloop_msg_queue_push(_msg, strlen(_msg), 0, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      goto failure;
   }

   core_set_default_callbacks(&cbs);
   if (!core_set_netplay_callbacks())
      goto failure;

   /* Map the core's quirks to our quirks. */
   if (serialization_quirks & RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE)
      quirks |= NETPLAY_QUIRK_INITIALIZATION;
   if (serialization_quirks & RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT)
      quirks |= NETPLAY_QUIRK_ENDIAN_DEPENDENT;
   if (serialization_quirks & RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT)
      quirks |= NETPLAY_QUIRK_PLATFORM_DEPENDENT;

   if (!(net_st->flags & NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT))
   {
      memset(host_room, 0, sizeof(*host_room));
      host_room->connectable = true;

      server = NULL;

      if (settings->bools.netplay_use_mitm_server)
      {
         const char *mitm_handle = settings->arrays.netplay_mitm_server;

         if (netplay_mitm_query(mitm_handle))
         {
            /* We want to cache the MITM server handle in order to
               prevent sending the wrong one to the lobby server if
               we change its config mid-session. */
            strlcpy(host_room->mitm_handle, mitm_handle,
               sizeof(host_room->mitm_handle));

            mitm = host_room->mitm_address;
            port = host_room->mitm_port;
         }
         else
            RARCH_WARN("[Netplay] Failed to get tunnel information. Switching to direct mode.\n");
      }

      if (!port)
         port = RARCH_DEFAULT_PORT;
   }
   else
   {
      if (net_st->flags & NET_DRIVER_ST_FLAG_NETPLAY_CLIENT_DEFERRED)
      {
         server       = net_st->server_address_deferred;
         port         = net_st->server_port_deferred;
         mitm_session = net_st->server_session_deferred;
      }
   }

   net_st->flags &= ~NET_DRIVER_ST_FLAG_NETPLAY_CLIENT_DEFERRED;
   netplay        = netplay_new(
      server, mitm, port, mitm_session,
      settings->ints.netplay_check_frames,
      &cbs,
      settings->bools.netplay_nat_traversal,
#ifdef HAVE_DISCORD
      !string_is_empty(discord_get_own_username()) ? discord_get_own_username()
      :
#endif
      settings->paths.username,
      quirks, modus);
   if (!netplay)
      goto failure;

   net_st->data = netplay;

   if (netplay->is_server)
   {
      const char *_msg;
      if (mitm)
      {
         int  flen = 0;
         char *buf = base64(netplay->mitm_session_id.unique,
            sizeof(netplay->mitm_session_id.unique), &flen);

         if (!buf)
            goto failure;
         strlcpy(host_room->mitm_session, buf,
            sizeof(host_room->mitm_session));
         free(buf);
      }

      if (netplay->nat_traversal)
         /* If performing NAT traversal,
            start announcing after it's done. */
         netplay_init_nat_traversal(netplay);
      else
         /* Start announcing after NETPLAY_ANNOUNCE_AFTER. */
         netplay->next_announce =
            cpu_features_get_time_usec() + NETPLAY_ANNOUNCE_AFTER;

      _msg = msg_hash_to_str(MSG_WAITING_FOR_CLIENT);
      runloop_msg_queue_push(_msg, strlen(_msg), 0, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
      if (!settings->bools.netplay_start_as_spectator)
         netplay_toggle_play_spectate(netplay);
   }
   else
   {
      /* Start our handshake */
      if (!netplay_handshake_init_send(netplay, &netplay->connections[0],
            LOW_NETPLAY_PROTOCOL_VERSION))
         goto failure;

      netplay->connections[0].mode = NETPLAY_CONNECTION_INIT;
      netplay->self_mode           = NETPLAY_CONNECTION_INIT;
   }

   /* Tell a core that uses the netpacket interface that the host is ready */
   if (netplay->is_server && net_st->core_netpacket_interface &&
         net_st->core_netpacket_interface->start)
      net_st->core_netpacket_interface->start(0,
            netplay_netpacket_send_cb, netplay_netpacket_poll_receive_cb);

   return true;

failure:
   net_st->flags &= ~(NET_DRIVER_ST_FLAG_NETPLAY_ENABLED
                    | NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT
                    | NET_DRIVER_ST_FLAG_NETPLAY_CLIENT_DEFERRED);
   deinit_netplay();

   {
      const char *_msg = msg_hash_to_str(MSG_NETPLAY_FAILED);
      RARCH_ERR("[Netplay] %s\n", _msg);
      runloop_msg_queue_push(_msg, strlen(_msg), 0, 180, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   return false;
}

static size_t retrieve_client_info(netplay_t *netplay, netplay_client_info_t *buf)
{
   size_t i, j = 0;

   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];

      /* We only want info from already connected clients. */
      if (     (connection->flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (connection->mode >= NETPLAY_CONNECTION_CONNECTED))
      {
         netplay_client_info_t *info = &buf[j++];

         info->id        = (int)i;
         info->protocol  = connection->netplay_protocol;
         info->mode      = connection->mode;
         info->ping      = connection->ping;
         info->slowdowns = connection->stall_slow;
         info->devices   = netplay->client_devices[i + 1];
         strlcpy(info->name, connection->nick, sizeof(info->name));
      }
   }

   return j;
}

static bool ban_client(netplay_t *netplay,
      struct netplay_connection *connection)
{
   if (netplay->ban_list.size >= netplay->ban_list.allocated)
   {
      if (!netplay->ban_list.size)
      {
         netplay->ban_list.list =
            (netplay_address_t*)malloc(2 * sizeof(*netplay->ban_list.list));
         if (!netplay->ban_list.list)
            return false;
         netplay->ban_list.allocated = 2;
      }
      else
      {
         size_t             new_allocated = netplay->ban_list.allocated + 4;
         netplay_address_t *new_list      = (netplay_address_t*)realloc(
            netplay->ban_list.list, new_allocated * sizeof(*new_list));

         if (!new_list)
            return false;

         netplay->ban_list.allocated = new_allocated;
         netplay->ban_list.list      = new_list;
      }
   }

   memcpy(&netplay->ban_list.list[netplay->ban_list.size++], &connection->addr,
      sizeof(*netplay->ban_list.list));

   return true;
}

static bool kick_client_by_id(netplay_t *netplay, int client_id, bool ban)
{
   struct netplay_connection *connection = NULL;

   /* Make sure the id is valid. */
   if ((size_t)client_id >= netplay->connections_size)
      return false;

   connection = &netplay->connections[client_id];
   /* We can only kick connected clients. */
   if (    (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
         ||  (connection->mode < NETPLAY_CONNECTION_CONNECTED))
      return false;

   if (ban && !ban_client(netplay, connection))
      return false;

   netplay_hangup(netplay, connection);

   return true;
}

static bool kick_client_by_name(netplay_t *netplay, const char *client_name,
      bool ban)
{
   size_t i;

   /* Find the connection with the name we want. */
   for (i = 0; i < netplay->connections_size; i++)
   {
      struct netplay_connection *connection = &netplay->connections[i];

      /* We can only kick connected clients. */
      if (    (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
            ||  (connection->mode < NETPLAY_CONNECTION_CONNECTED))
         continue;

      /* Kick the first client with a matched name. */
      if (string_is_equal(client_name, connection->nick))
      {
         if (ban && !ban_client(netplay, connection))
            return false;

         netplay_hangup(netplay, connection);

         return true;
      }
   }

   return false;
}

static bool kick_client_by_id_and_name(netplay_t *netplay,
      int client_id, const char *client_name, bool ban)
{
   struct netplay_connection *connection = NULL;

   /* Make sure the id is valid. */
   if ((size_t)client_id >= netplay->connections_size)
      return false;

   connection = &netplay->connections[client_id];
   /* We can only kick connected clients. */
   if (     (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE))
         ||   (connection->mode  < NETPLAY_CONNECTION_CONNECTED))
      return false;

   /* Make sure the name matches. */
   if (!string_is_equal(client_name, connection->nick))
      return false;

   if (ban && !ban_client(netplay, connection))
      return false;

   netplay_hangup(netplay, connection);

   return true;
}

static bool netplay_have_any_active_connection(netplay_t *netplay)
{
   size_t i;
   if (!netplay || !netplay->is_server)
      return (netplay && netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED);
   for (i = 0; i < netplay->connections_size; i++)
      if (     (netplay->connections[i].flags & NETPLAY_CONN_FLAG_ACTIVE)
            && (netplay->connections[i].mode >= NETPLAY_CONNECTION_CONNECTED))
         return true;
   return false;
}

bool netplay_is_spectating(void)
{
   /* helper function to check the spectating flag without being blocked by the netplay_driver_ctl guard */
   net_driver_state_t* net_st = &networking_driver_st;
   netplay_t* netplay = net_st->data;
   return (netplay && (netplay->self_mode == NETPLAY_CONNECTION_SPECTATING));
}

void netplay_force_send_savestate(void)
{
   net_driver_state_t* net_st = &networking_driver_st;
   netplay_t* netplay = net_st->data;

   /* Don't sync state when the core packet interface is active where clients
      run fully independent without the full state being synchronized. */
   if (netplay && netplay->is_server
         && netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
      netplay->force_send_savestate = true;
}

bool netplay_reinit_serialization(void)
{
   net_driver_state_t* net_st = &networking_driver_st;
   netplay_t* netplay = net_st->data;
   size_t i;

   if (!netplay)
      return true;

   netplay->state_size = 0;

   /* netplay_init_serialization rebuilds the delta states and zbuffer, but
    * nothing else, so we have to free them directly */
   for (i = 0; i < netplay->buffer_size; i++)
   {
      free(netplay->buffer[i].state);
      netplay->buffer[i].state = NULL;
   }

   if (netplay->zbuffer)
   {
      free(netplay->zbuffer);
      netplay->zbuffer = NULL;
   }

   return netplay_init_serialization(netplay);
}

/**
 * netplay_driver_ctl
 *
 * Frontend access to Netplay functionality
 */
bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   static bool guard          = false;
   net_driver_state_t *net_st = &networking_driver_st;
   netplay_t *netplay         = net_st->data;
   bool ret                   = true;

   if (guard)
      return true;

   guard                      = true;

   switch (state)
   {
      case RARCH_NETPLAY_CTL_ENABLE_SERVER:
         if (netplay)
         {
            ret = false;
            break;
         }
         net_st->flags       |=  NET_DRIVER_ST_FLAG_NETPLAY_ENABLED;
         net_st->flags       &= ~NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT;
         break;

      case RARCH_NETPLAY_CTL_ENABLE_CLIENT:
         if (netplay)
         {
            ret               = false;
            break;
         }
         net_st->flags       |= NET_DRIVER_ST_FLAG_NETPLAY_ENABLED
                              | NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT;
         break;

      case RARCH_NETPLAY_CTL_DISABLE:
         if (netplay)
         {
            ret               = false;
            break;
         }
         net_st->flags       &= ~NET_DRIVER_ST_FLAG_NETPLAY_ENABLED;
#ifdef HAVE_PRESENCE
         {
            presence_userdata_t userdata;
            userdata.status   = PRESENCE_NETPLAY_NETPLAY_STOPPED;
            command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
         }
#endif
         break;

#ifndef HAVE_DYNAMIC
      case RARCH_NETPLAY_CTL_ADD_FORK_ARG:
         if (data && net_st->fork_args.size < sizeof(net_st->fork_args.args))
         {
            size_t new_size   = strlcpy(
               net_st->fork_args.args + net_st->fork_args.size,
               (const char*)data,
               sizeof(net_st->fork_args.args) - net_st->fork_args.size);
            new_size         += 1; /* NULL terminator */
            new_size         += net_st->fork_args.size;
            if (new_size > sizeof(net_st->fork_args.args))
            {
               ret            = false;
               break;
            }
            net_st->fork_args.size = new_size;
         }
         else
            ret               = false;
         break;

      case RARCH_NETPLAY_CTL_GET_FORK_ARGS:
         if (data && net_st->fork_args.size)
         {
            size_t offset     = 0;
            char  *args       = net_st->fork_args.args;
            size_t args_sz    = net_st->fork_args.size;
            char **args_cur   = (char**)data;
            char **args_end   = &args_cur[NETPLAY_FORK_MAX_ARGS - 1];
            for (; offset < args_sz && args_cur != args_end; args_cur++)
            {
               *args_cur      = args + offset;
               offset        += strlen(*args_cur) + 1;
            }
            /* Ensure that the final entry is NULL. */
            *args_cur         = NULL;
         }
         else
            ret               = false;
         break;

      case RARCH_NETPLAY_CTL_CLEAR_FORK_ARGS:
         net_st->fork_args.size  = 0;
         *net_st->fork_args.args = '\0';
         break;
#endif

      case RARCH_NETPLAY_CTL_REFRESH_CLIENT_INFO:
         if (!netplay || !netplay->is_server)
         {
            ret = false;
            break;
         }
         if (!net_st->client_info)
         {
            net_st->client_info = (netplay_client_info_t*)calloc(
               MAX_CLIENTS - 1, sizeof(*net_st->client_info));
            if (!net_st->client_info)
            {
               ret = false;
               break;
            }
         }
         net_st->client_info_count = retrieve_client_info(netplay,
            net_st->client_info);
         break;

      case RARCH_NETPLAY_CTL_IS_ENABLED:
         ret = ((net_st->flags & NET_DRIVER_ST_FLAG_NETPLAY_ENABLED) > 0);
         break;

      case RARCH_NETPLAY_CTL_IS_DATA_INITED:
         ret = (netplay != NULL);
         break;

      case RARCH_NETPLAY_CTL_IS_REPLAYING:
         ret = (netplay && netplay->is_replay);
         break;

      case RARCH_NETPLAY_CTL_IS_SERVER:
         ret =   (net_st->flags & NET_DRIVER_ST_FLAG_NETPLAY_ENABLED)
            && (!(net_st->flags & NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT));
         break;

      case RARCH_NETPLAY_CTL_IS_CONNECTED:
         ret = (     netplay
               && (!(netplay->is_server))
               &&   (netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED));
         break;

      case RARCH_NETPLAY_CTL_IS_SPECTATING:
         ret = (   netplay
               && (netplay->self_mode == NETPLAY_CONNECTION_SPECTATING));
         break;

      case RARCH_NETPLAY_CTL_IS_PLAYING:
         ret = (   netplay
               && (netplay->self_mode == NETPLAY_CONNECTION_PLAYING
               ||  netplay->self_mode == NETPLAY_CONNECTION_SLAVE));
         break;

      case RARCH_NETPLAY_CTL_POST_FRAME:
         if (netplay)
            netplay_post_frame(netplay);
         break;

      case RARCH_NETPLAY_CTL_PRE_FRAME:
         if (netplay)
            ret = netplay_pre_frame(netplay);
         break;

      case RARCH_NETPLAY_CTL_GAME_WATCH:
         if (netplay && netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
            netplay_toggle_play_spectate(netplay);
         else
            ret = false;
         break;

      case RARCH_NETPLAY_CTL_PLAYER_CHAT:
         if (netplay)
            netplay_input_chat(netplay);
         else
            ret = false;
         break;

      case RARCH_NETPLAY_CTL_ALLOW_PAUSE:
         ret = (!netplay || (netplay->allow_pausing &&
                  (netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE ||
                     !netplay_have_any_active_connection(netplay))));
         break;

      case RARCH_NETPLAY_CTL_PAUSE:
         if (netplay && !netplay->local_paused)
            netplay_frontend_paused(netplay, true);

         if (netplay && netplay->modus == NETPLAY_MODUS_CORE_PACKET_INTERFACE)
         {
            /* handle new connections while paused, unpause on connect */
            if (!netplay_sync_pre_frame(netplay))
               netplay_disconnect(netplay);
            else if (netplay_have_any_active_connection(netplay))
               command_event(CMD_EVENT_UNPAUSE, NULL);
         }
         break;

      case RARCH_NETPLAY_CTL_UNPAUSE:
         if (netplay && netplay->local_paused)
            netplay_frontend_paused(netplay, false);
         break;

      case RARCH_NETPLAY_CTL_LOAD_SAVESTATE:
         if (netplay && netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
            netplay_load_savestate(netplay,
               (retro_ctx_serialize_info_t*)data, true);
         break;

      case RARCH_NETPLAY_CTL_RESET:
         if (netplay && netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
            netplay_core_reset(netplay);
         break;

      case RARCH_NETPLAY_CTL_DISCONNECT:
         if (netplay)
            netplay_disconnect(netplay);
         else
            ret = false;
         break;

      case RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL:
         if (netplay)
            netplay_announce_nat_traversal(netplay, (uintptr_t)data);
         break;

      case RARCH_NETPLAY_CTL_DESYNC_PUSH:
         if (netplay)
         {
            if (netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE)
               netplay->desync++;
            else if (netplay_have_any_active_connection(netplay))
               ret = false;
         }
         break;

      case RARCH_NETPLAY_CTL_DESYNC_POP:
         if (netplay && netplay->desync)
         {
            if (!(--netplay->desync))
               netplay_load_savestate(netplay, NULL, true);
         }
         break;

      case RARCH_NETPLAY_CTL_KICK_CLIENT:
         /* Only the server should be able to kick others. */
         if (netplay && netplay->is_server)
         {
            netplay_client_info_t *client = (netplay_client_info_t*)data;
            if (!client)
            {
               ret = false;
               break;
            }
            if (client->id >= 0 && !string_is_empty(client->name))
               ret = kick_client_by_id_and_name(netplay,
                  client->id, client->name, false);
            else if (client->id >= 0)
               ret = kick_client_by_id(netplay, client->id, false);
            else if (!string_is_empty(client->name))
               ret = kick_client_by_name(netplay, client->name, false);
            else
               ret = false;
         }
         else
            ret = false;
         break;

      case RARCH_NETPLAY_CTL_BAN_CLIENT:
         /* Only the server should be able to ban others. */
         if (netplay && netplay->is_server)
         {
            netplay_client_info_t *client = (netplay_client_info_t*)data;
            if (!client)
            {
               ret = false;
               break;
            }
            if (client->id >= 0 && !string_is_empty(client->name))
               ret = kick_client_by_id_and_name(netplay,
                  client->id, client->name, true);
            else if (client->id >= 0)
               ret = kick_client_by_id(netplay, client->id, true);
            else if (!string_is_empty(client->name))
               ret = kick_client_by_name(netplay, client->name, true);
            else
               ret = false;
         }
         else
            ret = false;
         break;

      case RARCH_NETPLAY_CTL_SET_CORE_PACKET_INTERFACE:
         if (!net_st->core_netpacket_interface && data && netplay)
         {
            /* it's too late to enable the interface */
            ret = false;
            break;
         }

         if (net_st->core_netpacket_interface)
         {
            if (data && !memcmp(net_st->core_netpacket_interface,
                  data, sizeof(*net_st->core_netpacket_interface)))
               break; /* interface is not modified */

            free(net_st->core_netpacket_interface);
            net_st->core_netpacket_interface = NULL;
         }

         if (data)
         {
            /* copy passed interface to local state */
            net_st->core_netpacket_interface = (struct retro_netpacket_callback*)
               malloc(sizeof(*net_st->core_netpacket_interface));
            *net_st->core_netpacket_interface = *(struct retro_netpacket_callback*)data;
            /* reset savefile dir as core_netpacket_interface affects it */
            runloop_path_set_redirect(config_get_ptr(),
               dir_get_ptr(RARCH_DIR_SAVEFILE),
               dir_get_ptr(RARCH_DIR_SAVESTATE));
         }
         break;

      case RARCH_NETPLAY_CTL_USE_CORE_PACKET_INTERFACE:
         ret = (net_st->core_netpacket_interface != NULL);
         break;

      case RARCH_NETPLAY_CTL_ALLOW_TIMESKIP:
         ret = (!netplay
                  || netplay->modus != NETPLAY_MODUS_CORE_PACKET_INTERFACE
                  || !netplay_have_any_active_connection(netplay));
         break;

      case RARCH_NETPLAY_CTL_NONE:
      default:
         ret = false;
         break;
   }
   guard = false;

   return ret;
}

/* Netplay Utils */

bool netplay_compatible_version(const char *version)
{
   static const uint64_t min_version = 0x0001000900010000ULL; /* 1.9.1 */
   size_t   version_parts            = 0;
   uint64_t version_value            = 0;
   char     *version_end             = NULL;
   bool     loop                     = true;

   /* Convert the version string to an integer first. */
   do
   {
      uint16_t version_part = (uint16_t)strtoul(version, &version_end, 10);

      if (version_end == version) /* Nothing to convert */
         return false;

      switch (*version_end)
      {
         case '\0': /* End of version string */
            loop = false;
            break;
         case '.':
            version = (const char*)version_end + 1;
            break;
         default: /* Invalid version string */
            return false;
      }

      /* We only want enough bits as to fit into version_value. */
      if (version_parts++ < (sizeof(version_value) / sizeof(version_part)))
         version_value |= (uint64_t)version_part <<
            ((sizeof(version_value) << 3) -
               ((sizeof(version_part) << 3) * version_parts));
   } while (loop);

   return version_value >= min_version;
}

bool netplay_decode_hostname(const char *hostname,
      char *address, unsigned *port, char *session, size_t len)
{
   struct string_list hostname_data;

   if (string_is_empty(hostname))
      return false;
   if (!string_list_initialize(&hostname_data))
      return false;
   if (!string_split_noalloc(&hostname_data, hostname, "|"))
   {
      string_list_deinitialize(&hostname_data);
      return false;
   }

   if (hostname_data.size >= 1 &&
         !string_is_empty(hostname_data.elems[0].data))
   {
      if (address)
         strlcpy(address, hostname_data.elems[0].data, len);
   }
   if (hostname_data.size >= 2 &&
         !string_is_empty(hostname_data.elems[1].data))
   {
      if (port)
      {
         unsigned long int tmp_port = (unsigned long int)
            strtoul(hostname_data.elems[1].data, NULL, 10);

         if (tmp_port && tmp_port <= 65535)
            *port = (unsigned)tmp_port;
      }
   }
   if (hostname_data.size >= 3 &&
         !string_is_empty(hostname_data.elems[2].data))
   {
      if (session)
         strlcpy(session, hostname_data.elems[2].data, len);
   }

   string_list_deinitialize(&hostname_data);

   return true;
}

/**
 * netplay_send_cmd_netpacket
 * @netplay       : pointer to netplay object
 * @conn_idx      : connection index to send cmd to
 * @buf           : packet data pointer
 * @len           : packet data size
 * @pkt_client_id : source id if host sending to client, otherwise recipient id or RETRO_NETPACKET_BROADCAST
 *
 * Send a netpacket command to a connected peer.
 */
static void netplay_send_cmd_netpacket(netplay_t *netplay, size_t conn_idx,
      const void* buf, size_t len, uint16_t pkt_client_id)
{
   struct netplay_connection *connection;
   struct socket_buffer *sbuf;
   uint32_t cmdbuf[3];
   bool need_flush;

   if (conn_idx >= netplay->connections_size) return;
   connection = &netplay->connections[conn_idx];
   if (!(connection->flags & NETPLAY_CONN_FLAG_ACTIVE)) return;
   if (connection->mode != NETPLAY_CONNECTION_PLAYING) return;

   cmdbuf[0] = htonl(NETPLAY_CMD_NETPACKET);
   cmdbuf[1] = htonl(len);
   cmdbuf[2] = htonl(pkt_client_id);

   sbuf = &connection->send_packet_buffer;
   need_flush = (buf_remaining(sbuf) < sizeof(cmdbuf)+len);

   if (     (need_flush && !netplay_send_flush(sbuf, connection->fd, true))
         || (!netplay_send(sbuf, connection->fd, cmdbuf, sizeof(cmdbuf)))
         || (len && !netplay_send(sbuf, connection->fd, buf, len)))
      netplay_hangup(netplay, connection);
}

static void RETRO_CALLCONV netplay_netpacket_send_cb(int flags,
      const void* buf, size_t len, uint16_t client_id)
{
   net_driver_state_t *net_st = &networking_driver_st;
   netplay_t *netplay         = net_st->data;
   if (!netplay) return;

   if (buf && len)
   {
      if (!netplay->is_server)
      {
         /* client always sends packet to host, host will relay it if needed */
         netplay_send_cmd_netpacket(netplay, 0, buf, len, client_id);
      }
      else if (client_id == RETRO_NETPACKET_BROADCAST)
      {
         /* send packet to all clients */
         size_t i;
         for (i = 0; i < netplay->connections_size; i++)
            netplay_send_cmd_netpacket(netplay, i, buf, len, 0);
      }
      else if (client_id)
      {
         /* send packet to specific client */
         netplay_send_cmd_netpacket(netplay, client_id-1, buf, len, 0);
      }
   }

   if (flags & RETRO_NETPACKET_FLUSH_HINT)
   {
      netplay_send_flush_all(netplay, NULL);
   }
}

static void RETRO_CALLCONV netplay_netpacket_poll_receive_cb(void)
{
   net_driver_state_t *net_st = &networking_driver_st;
   netplay_t *netplay         = net_st->data;
   if (!netplay) return;

   netplay_poll_net_input(netplay);
}

/* Netplay Widgets */

#ifdef HAVE_GFX_WIDGETS
static void gfx_widget_netplay_chat_iterate(void *user_data,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path, bool is_threaded)
{
   size_t i;
   net_driver_state_t         *net_st      = &networking_driver_st;
   netplay_t                  *netplay     = net_st->data;
   struct netplay_chat_buffer *chat_buffer = &net_st->chat_buffer;

   if (netplay)
   {
      struct netplay_chat *chat     = &netplay->chat;
      settings_t          *settings = config_get_ptr();
#ifdef HAVE_MENU
      bool menu_open                = menu_state_get_ptr()->flags &
         MENU_ST_FLAG_ALIVE;
#endif
      bool fade_chat                = settings->bools.netplay_fade_chat;

      /* Move the messages to a thread-safe buffer
         before drawing them. */
      chat_buffer->color_name       =
         (uint32_t)settings->uints.netplay_chat_color_name << 8;
      chat_buffer->color_msg        =
         (uint32_t)settings->uints.netplay_chat_color_msg << 8;

      for (i = 0; i < ARRAY_SIZE(chat->messages); i++)
      {
         uint32_t *frames = &chat->messages[i].frames;
         uint8_t  *alpha  = &chat_buffer->messages[i].alpha;

#ifdef HAVE_MENU
         /* Don't show chat while in the menu. */
         if (menu_open)
         {
            *alpha = 0;
            continue;
         }
#endif

         /* If we are not fading, set alpha to max. */
         if (!fade_chat)
            *alpha = 0xFF;
         else if (*frames)
         {
            float alpha_percent = (float)*frames /
               (float)NETPLAY_CHAT_FRAME_TIME;

            *alpha = (uint8_t)float_max(alpha_percent * 255.0f, 1.0f);
            (*frames)--;
         }
         else
         {
            *alpha = 0;
            continue;
         }

         memcpy(chat_buffer->messages[i].nick, chat->messages[i].nick,
            sizeof(chat_buffer->messages[i].nick));
         memcpy(chat_buffer->messages[i].msg, chat->messages[i].msg,
            sizeof(chat_buffer->messages[i].msg));
      }
   }
   /* If we are not in netplay, do nothing. */
   else
   {
      for (i = 0; i < ARRAY_SIZE(chat_buffer->messages); i++)
         chat_buffer->messages[i].alpha = 0;
   }
}

static void gfx_widget_netplay_chat_frame(void *data, void *userdata)
{
   size_t i;
   char formatted_nick[NETPLAY_CHAT_MAX_SIZE];
   char formatted_msg[NETPLAY_CHAT_MAX_SIZE];
   int  formatted_nick_width;
   video_frame_info_t         *video_info   = (video_frame_info_t*)data;
   dispgfx_widget_t           *p_dispwidget = (dispgfx_widget_t*)userdata;
   net_driver_state_t         *net_st       = &networking_driver_st;
   struct netplay_chat_buffer *chat_buffer  = &net_st->chat_buffer;
   gfx_widget_font_data_t     *font         =
      &p_dispwidget->gfx_widget_fonts.regular;
   int line_height                          =
      font->line_height + p_dispwidget->simple_widget_padding / 3.0f;
   int height                               = video_info->height - line_height;
   uint32_t color_name                      = chat_buffer->color_name;
   uint32_t color_msg                       = chat_buffer->color_msg;

   for (i = 0; i < ARRAY_SIZE(chat_buffer->messages); i++)
   {
      size_t formatted_nick_len;
      uint8_t    alpha = chat_buffer->messages[i].alpha;
      const char *nick = chat_buffer->messages[i].nick;
      const char *msg  = chat_buffer->messages[i].msg;

      if (!alpha || string_is_empty(nick) || string_is_empty(msg))
         continue;

      /* Truncate the message, if necessary. */
      formatted_nick_len = (size_t)snprintf(
            formatted_nick, sizeof(formatted_nick),
            "%s: ", nick);
      strlcpy(formatted_msg, msg, sizeof(formatted_msg)
            - formatted_nick_len);
      formatted_nick_width = font_driver_get_message_width(
         font->font, formatted_nick, formatted_nick_len, 1.0f);

      /* Draw the nickname first. */
      gfx_widgets_draw_text(
         font,
         formatted_nick,
         p_dispwidget->simple_widget_padding,
         height,
         video_info->width,
         video_info->height,
         color_name | (uint32_t)alpha,
         TEXT_ALIGN_LEFT,
         true);
      /* Now draw the message. */
      gfx_widgets_draw_text(
         font,
         formatted_msg,
         p_dispwidget->simple_widget_padding + formatted_nick_width,
         height,
         video_info->width,
         video_info->height,
         color_msg | (uint32_t)alpha,
         TEXT_ALIGN_LEFT,
         true);

      /* Move up */
      height -= line_height;
   }
}

static void gfx_widget_netplay_ping_iterate(void *user_data,
      unsigned width, unsigned height, bool fullscreen,
      const char *dir_assets, char *font_path, bool is_threaded)
{
   net_driver_state_t *net_st   = &networking_driver_st;
   netplay_t          *netplay  = net_st->data;
   settings_t         *settings = config_get_ptr();
#ifdef HAVE_MENU
   bool menu_open               = menu_state_get_ptr()->flags &
      MENU_ST_FLAG_ALIVE;
#endif
   bool show_ping               = settings->bools.netplay_ping_show;

   if (!netplay || !show_ping)
   {
      net_st->latest_ping       = -1;
      return;
   }

   /* Don't show the ping counter while in the menu. */
#ifdef HAVE_MENU
   if (menu_open)
   {
      net_st->latest_ping       = -1;
      return;
   }
#endif

   if (!netplay->is_server &&
         netplay->self_mode >= NETPLAY_CONNECTION_CONNECTED)
      net_st->latest_ping       = netplay->connections[0].ping;
   else
      net_st->latest_ping       = -1;
}

static void gfx_widget_netplay_ping_frame(void *data, void *userdata)
{
   net_driver_state_t *net_st = &networking_driver_st;
   int ping                   = net_st->latest_ping;

   if (ping >= 0)
   {
      char ping_str[16];
      size_t ping_len;
      int ping_width, total_width;
      video_frame_info_t     *video_info   = (video_frame_info_t*)data;
      dispgfx_widget_t       *p_dispwidget = (dispgfx_widget_t*)userdata;
      gfx_display_t          *p_disp       =
         (gfx_display_t*)video_info->disp_userdata;
      gfx_widget_font_data_t *font         =
         &p_dispwidget->gfx_widget_fonts.regular;

      /* Limit the ping counter to 999. */
      if (ping > 999)
         ping     = 999;

      ping_len    = (size_t)snprintf(ping_str,
            sizeof(ping_str), "PING: %d", ping);

      ping_width  = font_driver_get_message_width(
         font->font, ping_str, ping_len, 1.0f);
      total_width = ping_width + p_dispwidget->simple_widget_padding * 2;

      gfx_display_set_alpha(p_dispwidget->backdrop_orig, DEFAULT_BACKDROP);
      gfx_display_draw_quad(
         p_disp,
         video_info->userdata,
         video_info->width,
         video_info->height,
         video_info->width - total_width,
         video_info->height - p_dispwidget->simple_widget_height,
         total_width,
         p_dispwidget->simple_widget_height,
         video_info->width,
         video_info->height,
         p_dispwidget->backdrop_orig,
	     NULL);
      gfx_widgets_draw_text(
         font,
         ping_str,
         video_info->width - ping_width - p_dispwidget->simple_widget_padding,
         video_info->height - font->line_centre_offset,
         video_info->width,
         video_info->height,
         0xFFFFFFFF,
         TEXT_ALIGN_LEFT,
         true);
   }
}

const gfx_widget_t gfx_widget_netplay_chat = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   &gfx_widget_netplay_chat_iterate,
   &gfx_widget_netplay_chat_frame
};

const gfx_widget_t gfx_widget_netplay_ping = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   &gfx_widget_netplay_ping_iterate,
   &gfx_widget_netplay_ping_frame
};
#endif
