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

#ifndef __RARCH_NETPLAY_H
#define __RARCH_NETPLAY_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_miscellaneous.h>

#include <net/net_compat.h>

#include "netplay_defines.h"

#include "../../msg_hash.h"

#include "../natt.h"

typedef struct netplay netplay_t;

typedef struct netplay_client_info
{
   uint32_t protocol;
   uint32_t devices;
   uint32_t slowdowns;
   int32_t  ping;
   int      id;
   enum     rarch_netplay_connection_mode mode;
   char     name[NETPLAY_NICK_LEN];
} netplay_client_info_t;

typedef struct mitm_server
{
   const char *name;
   enum msg_hash_enums description;
} mitm_server_t;

#ifndef HAVE_DYNAMIC
struct netplay_fork_args
{
   size_t size;
   char   args[PATH_MAX_LENGTH];
};
#endif

struct netplay_room
{
   struct netplay_room *next;
   int  id;
   int  gamecrc;
   int  port;
   int  mitm_port;
   int  host_method;
   char nickname[NETPLAY_NICK_LEN];
   char frontend[NETPLAY_HOST_STR_LEN];
   char corename[NETPLAY_HOST_STR_LEN];
   char coreversion[NETPLAY_HOST_STR_LEN];
   char retroarch_version[NETPLAY_HOST_STR_LEN];
   char gamename[NETPLAY_HOST_LONGSTR_LEN];
   char subsystem_name[NETPLAY_HOST_LONGSTR_LEN];
   char country[3];
   char address[NETPLAY_HOST_LONGSTR_LEN];
   char mitm_handle[NETPLAY_HOST_STR_LEN];
   char mitm_address[NETPLAY_HOST_LONGSTR_LEN];
   char mitm_session[NETPLAY_HOST_STR_LEN];
   bool has_password;
   bool has_spectate_password;
   bool connectable;
   bool is_retroarch;
   bool lan;
};

struct netplay_rooms
{
   struct netplay_room *head;
   struct netplay_room *cur;
};

struct netplay_host
{
   int  content_crc;
   int  port;
   char address[16];
   char nick[NETPLAY_NICK_LEN];
   char frontend[NETPLAY_HOST_STR_LEN];
   char core[NETPLAY_HOST_STR_LEN];
   char core_version[NETPLAY_HOST_STR_LEN];
   char retroarch_version[NETPLAY_HOST_STR_LEN];
   char content[NETPLAY_HOST_LONGSTR_LEN];
   char subsystem_name[NETPLAY_HOST_LONGSTR_LEN];
   bool has_password;
   bool has_spectate_password;
};

struct netplay_host_list
{
   struct netplay_host *hosts;
   size_t allocated;
   size_t size;
};

struct netplay_chat_buffer
{
   struct
   {
      uint8_t alpha;
      char    nick[NETPLAY_NICK_LEN];
      char    msg[NETPLAY_CHAT_MAX_SIZE];
   } messages[NETPLAY_CHAT_MAX_MESSAGES];
   uint32_t color_name;
   uint32_t color_msg;
};

typedef struct
{
#ifndef HAVE_DYNAMIC
   struct netplay_fork_args fork_args;
#endif
   /* NAT traversal info (if NAT traversal is used and serving) */
   struct nat_traversal_data nat_traversal_request;
#ifdef HAVE_NETPLAYDISCOVERY
   /* List of discovered hosts */
   struct netplay_host_list discovered_hosts;
#endif
   struct netplay_chat_buffer chat_buffer;
   struct netplay_room host_room;
   struct netplay_room *room_list;
   struct netplay_rooms *rooms_data;
   /* Used while Netplay is running */
   netplay_t *data;
   netplay_client_info_t *client_info;
   size_t client_info_count;
#ifdef HAVE_NETPLAYDISCOVERY
   /* LAN discovery sockets */
   int lan_ad_server_fd;
   int lan_ad_client_fd;
#endif
   int room_count;
   int latest_ping;
   unsigned server_port_deferred;
   uint8_t flags;
   char server_address_deferred[256];
   char server_session_deferred[32];
} net_driver_state_t;

net_driver_state_t *networking_state_get_ptr(void);

bool netplay_compatible_version(const char *version);
bool netplay_decode_hostname(const char *hostname,
   char *address, unsigned *port, char *session, size_t len);

int netplay_rooms_parse(const char *buf, size_t len);
int netplay_rooms_get_count(void);
struct netplay_room *netplay_room_get(int index);
void netplay_rooms_free(void);

/**
 * init_netplay
 * @server               : server address to connect to (client only)
 * @port                 : TCP port to host on/connect to
 * @mitm_session         : Session id for MITM/tunnel (client only).
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool init_netplay(const char *server, unsigned port, const char *mitm_session);
bool init_netplay_deferred(const char *server, unsigned port,
   const char *mitm_session);
void deinit_netplay(void);

bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data);

#ifdef HAVE_NETPLAYDISCOVERY
/** Initialize Netplay discovery */
bool init_netplay_discovery(void);
/** Deinitialize and free Netplay discovery */
void deinit_netplay_discovery(void);

/** Discovery control */
bool netplay_discovery_driver_ctl(enum rarch_netplay_discovery_ctl_state state,
   void *data);
#endif

extern const mitm_server_t netplay_mitm_server_list[NETPLAY_MITM_SERVERS];
#endif
