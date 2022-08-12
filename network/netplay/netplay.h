/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Gregor Richards
 *  Copyright (C) 2021-2022 - Roberto V. Rampim
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

#include "../../msg_hash.h"

#include "../natt.h"

#ifndef HAVE_DYNAMIC
#define NETPLAY_FORK_MAX_ARGS 64
#endif

#define NETPLAY_NICK_LEN         32
#define NETPLAY_HOST_STR_LEN     32
#define NETPLAY_HOST_LONGSTR_LEN 256

#define NETPLAY_MITM_SERVERS 5

#define NETPLAY_CHAT_MAX_MESSAGES   5
#define NETPLAY_CHAT_MAX_SIZE       96
#define NETPLAY_CHAT_FRAME_TIME     900

enum rarch_netplay_ctl_state
{
   RARCH_NETPLAY_CTL_NONE = 0,
   RARCH_NETPLAY_CTL_GAME_WATCH,
   RARCH_NETPLAY_CTL_PLAYER_CHAT,
   RARCH_NETPLAY_CTL_POST_FRAME,
   RARCH_NETPLAY_CTL_PRE_FRAME,
   RARCH_NETPLAY_CTL_ENABLE_SERVER,
   RARCH_NETPLAY_CTL_ENABLE_CLIENT,
   RARCH_NETPLAY_CTL_DISABLE,
#ifndef HAVE_DYNAMIC
   RARCH_NETPLAY_CTL_ADD_FORK_ARG,
   RARCH_NETPLAY_CTL_GET_FORK_ARGS,
   RARCH_NETPLAY_CTL_CLEAR_FORK_ARGS,
#endif
   RARCH_NETPLAY_CTL_REFRESH_CLIENT_INFO,
   RARCH_NETPLAY_CTL_IS_ENABLED,
   RARCH_NETPLAY_CTL_IS_REPLAYING,
   RARCH_NETPLAY_CTL_IS_SERVER,
   RARCH_NETPLAY_CTL_IS_CONNECTED,
   RARCH_NETPLAY_CTL_IS_PLAYING,
   RARCH_NETPLAY_CTL_IS_SPECTATING,
   RARCH_NETPLAY_CTL_IS_DATA_INITED,
   RARCH_NETPLAY_CTL_ALLOW_PAUSE,
   RARCH_NETPLAY_CTL_PAUSE,
   RARCH_NETPLAY_CTL_UNPAUSE,
   RARCH_NETPLAY_CTL_LOAD_SAVESTATE,
   RARCH_NETPLAY_CTL_RESET,
   RARCH_NETPLAY_CTL_DISCONNECT,
   RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL,
   RARCH_NETPLAY_CTL_DESYNC_PUSH,
   RARCH_NETPLAY_CTL_DESYNC_POP,
   RARCH_NETPLAY_CTL_KICK_CLIENT,
   RARCH_NETPLAY_CTL_BAN_CLIENT
};

/* The current status of a connection */
enum rarch_netplay_connection_mode
{
   NETPLAY_CONNECTION_NONE = 0,

   NETPLAY_CONNECTION_DELAYED_DISCONNECT, 
   /* The connection is dead, but data
      is still waiting to be forwarded */

   /* Initialization: */
   NETPLAY_CONNECTION_INIT,         /* Waiting for header */
   NETPLAY_CONNECTION_PRE_NICK,     /* Waiting for nick */
   NETPLAY_CONNECTION_PRE_PASSWORD, /* Waiting for password */
   NETPLAY_CONNECTION_PRE_INFO,     /* Waiting for core/content info */
   NETPLAY_CONNECTION_PRE_SYNC,     /* Waiting for sync */

   /* Ready: */
   NETPLAY_CONNECTION_CONNECTED, /* Modes above this are connected */
   NETPLAY_CONNECTION_SPECTATING, /* Spectator mode */
   NETPLAY_CONNECTION_SLAVE, /* Playing in slave mode */
   NETPLAY_CONNECTION_PLAYING /* Normal ready state */
};

/* Preferences for sharing digital devices */
enum rarch_netplay_share_digital_preference
{
   RARCH_NETPLAY_SHARE_DIGITAL_NO_SHARING = 0,
   RARCH_NETPLAY_SHARE_DIGITAL_NO_PREFERENCE,
   RARCH_NETPLAY_SHARE_DIGITAL_OR,
   RARCH_NETPLAY_SHARE_DIGITAL_XOR,
   RARCH_NETPLAY_SHARE_DIGITAL_VOTE,
   RARCH_NETPLAY_SHARE_DIGITAL_LAST
};

/* Preferences for sharing analog devices */
enum rarch_netplay_share_analog_preference
{
   RARCH_NETPLAY_SHARE_ANALOG_NO_SHARING = 0,
   RARCH_NETPLAY_SHARE_ANALOG_NO_PREFERENCE,
   RARCH_NETPLAY_SHARE_ANALOG_MAX,
   RARCH_NETPLAY_SHARE_ANALOG_AVERAGE,
   RARCH_NETPLAY_SHARE_ANALOG_LAST
};

enum netplay_host_method
{
   NETPLAY_HOST_METHOD_UNKNOWN = 0,
   NETPLAY_HOST_METHOD_MANUAL,
   NETPLAY_HOST_METHOD_UPNP,
   NETPLAY_HOST_METHOD_MITM
};

enum rarch_netplay_discovery_ctl_state
{
   RARCH_NETPLAY_DISCOVERY_CTL_NONE = 0,
   RARCH_NETPLAY_DISCOVERY_CTL_LAN_SEND_QUERY,
   RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES,
   RARCH_NETPLAY_DISCOVERY_CTL_LAN_CLEAR_RESPONSES
};

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
   char server_address_deferred[256];
   char server_session_deferred[32];
   bool netplay_client_deferred;
   /* Only used before init_netplay */
   bool netplay_enabled;
   bool netplay_is_client;
   bool has_set_netplay_mode;
   bool has_set_netplay_ip_address;
   bool has_set_netplay_ip_port;
   bool has_set_netplay_check_frames;
} net_driver_state_t;

net_driver_state_t *networking_state_get_ptr(void);

bool netplay_compatible_version(const char *version);
bool netplay_decode_hostname(const char *hostname,
   char *address, unsigned *port, char *session, size_t len);
bool netplay_is_lan_address(struct sockaddr_in *addr);

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
