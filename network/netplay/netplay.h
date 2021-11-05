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
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <net/net_compat.h>
#include <net/net_ifinfo.h>
#include <retro_miscellaneous.h>

#include "../../core.h"

#define NETPLAY_HOST_STR_LEN 32
#define NETPLAY_HOST_LONGSTR_LEN 256

enum rarch_netplay_ctl_state
{
   RARCH_NETPLAY_CTL_NONE = 0,
   RARCH_NETPLAY_CTL_GAME_WATCH,
   RARCH_NETPLAY_CTL_POST_FRAME,
   RARCH_NETPLAY_CTL_PRE_FRAME,
   RARCH_NETPLAY_CTL_ENABLE_SERVER,
   RARCH_NETPLAY_CTL_ENABLE_CLIENT,
   RARCH_NETPLAY_CTL_DISABLE,
   RARCH_NETPLAY_CTL_IS_ENABLED,
   RARCH_NETPLAY_CTL_IS_REPLAYING,
   RARCH_NETPLAY_CTL_IS_SERVER,
   RARCH_NETPLAY_CTL_IS_CONNECTED,
   RARCH_NETPLAY_CTL_IS_DATA_INITED,
   RARCH_NETPLAY_CTL_PAUSE,
   RARCH_NETPLAY_CTL_UNPAUSE,
   RARCH_NETPLAY_CTL_LOAD_SAVESTATE,
   RARCH_NETPLAY_CTL_RESET,
   RARCH_NETPLAY_CTL_DISCONNECT,
   RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL,
   RARCH_NETPLAY_CTL_DESYNC_PUSH,
   RARCH_NETPLAY_CTL_DESYNC_POP
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

typedef struct mitm_server
{
   const char *name;
   const char *description;
} mitm_server_t;

static const mitm_server_t netplay_mitm_server_list[] = {
   { "nyc", "New York City, USA" },
   { "madrid", "Madrid, Spain" },
   { "montreal", "Montreal, Canada" },
   { "saopaulo", "Sao Paulo, Brazil" },
};

struct netplay_room
{
   struct netplay_room *next;
   int id;
   int  port;
   int  mitm_port;
   int  gamecrc;
   int  timestamp;
   int  host_method;
   char country           [3];
   char retroarch_version [33];
   char nickname          [33];
   char subsystem_name    [256];
   char corename          [256];
   char frontend          [256];
   char coreversion       [256];
   char gamename          [256];
   char address           [256];
   char mitm_address      [256];
   bool has_password;
   bool has_spectate_password;
   bool lan;
   bool fixed;
};

struct netplay_host
{
   struct sockaddr addr;
   socklen_t addrlen;
   int  content_crc;
   int  port;
   char address[NETPLAY_HOST_STR_LEN];
   char nick[NETPLAY_HOST_STR_LEN];
   char frontend[NETPLAY_HOST_STR_LEN];
   char core[NETPLAY_HOST_STR_LEN];
   char core_version[NETPLAY_HOST_STR_LEN];
   char retroarch_version[NETPLAY_HOST_STR_LEN];
   char content[NETPLAY_HOST_LONGSTR_LEN];
   char subsystem_name[NETPLAY_HOST_LONGSTR_LEN];
};

struct netplay_host_list
{
   struct netplay_host *hosts;
   size_t size;
};

typedef struct
{
   netplay_t *data; /* Used while Netplay is running */
   struct netplay_room host_room; /* ptr alignment */
   netplay_t *handshake_password;
   struct netplay_room *room_list;
   /* List of discovered hosts */
   struct netplay_host_list discovered_hosts;
#ifdef HAVE_NETPLAYDISCOVERY
   size_t discovered_hosts_allocated;
#endif
   int room_count;
   int reannounce;
   unsigned server_port_deferred;
   /* Packet buffer for advertisement and responses */
   struct ad_packet ad_packet_buffer; /* uint32_t alignment */
   uint16_t mapping[RETROK_LAST];
   char server_address_deferred[512];
   /* Only used before init_netplay */
   bool netplay_enabled;
   bool netplay_is_client;
   /* Used to avoid recursive netplay calls */
   bool in_netplay;
   bool netplay_client_deferred;
   bool is_mitm;
   bool has_set_netplay_mode;
   bool has_set_netplay_ip_address;
   bool has_set_netplay_ip_port;
   bool has_set_netplay_stateless_mode;
   bool has_set_netplay_check_frames;
} net_driver_state_t;

net_driver_state_t *networking_state_get_ptr(void);

bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data);

int netplay_rooms_parse(const char *buf);

struct netplay_room* netplay_room_get(int index);

int netplay_rooms_get_count(void);

void netplay_rooms_free(void);

/**
* netplay_frontend_paused
 * @netplay              : pointer to netplay object
 * @paused               : true if frontend is paused
 *
 * Inform Netplay of the frontend's pause state (paused or otherwise)
 */
void netplay_frontend_paused(netplay_t *netplay, bool paused);

/**
 * netplay_toggle_play_spectate
 *
 * Toggle between play mode and spectate mode
 */
void netplay_toggle_play_spectate(netplay_t *netplay);

/**
 * netplay_load_savestate
 * @netplay              : pointer to netplay object
 * @serial_info          : the savestate being loaded, NULL means
 *                         "load it yourself"
 * @save                 : Whether to save the provided serial_info
 *                         into the frame buffer
 *
 * Inform Netplay of a savestate load and send it to the other side
 **/
void netplay_load_savestate(netplay_t *netplay,
      retro_ctx_serialize_info_t *serial_info, bool save);

/**
 * netplay_core_reset
 * @netplay              : pointer to netplay object
 *
 * Indicate that the core has been reset to netplay peers
 **/
void netplay_core_reset(netplay_t *netplay);

int16_t netplay_input_state(netplay_t *netplay,
      unsigned port, unsigned device,
      unsigned idx, unsigned id);

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
bool netplay_poll(
      bool block_libretro_input,
      void *settings_data,
      netplay_t *netplay);

/**
 * netplay_is_alive:
 * @netplay              : pointer to netplay object
 *
 * Checks if input port/index is controlled by netplay or not.
 *
 * Returns: true (1) if alive, otherwise false (0).
 **/
bool netplay_is_alive(netplay_t *netplay);

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
bool netplay_should_skip(netplay_t *netplay);

/**
 * netplay_post_frame:
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay.
 * We check if we have new input and replay from recorded input.
 * Call this after running retro_run().
 **/
void netplay_post_frame(netplay_t *netplay);

void deinit_netplay(void);

/**
 * init_netplay
 * @direct_host          : Host to connect to directly, if applicable (client only)
 * @server               : server address to connect to (client only)
 * @port                 : TCP port to host on/connect to
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool init_netplay(void *direct_host, const char *server, unsigned port);

bool init_netplay_deferred(const char* server, unsigned port);

void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch);
void audio_sample_net(int16_t left, int16_t right);
size_t audio_sample_batch_net(const int16_t *data, size_t frames);
int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id);

#ifdef HAVE_NETPLAYDISCOVERY
/** Initialize Netplay discovery */
bool init_netplay_discovery(void);

/** Deinitialize and free Netplay discovery */
void deinit_netplay_discovery(void);

/** Discovery control */
bool netplay_discovery_driver_ctl(
      enum rarch_netplay_discovery_ctl_state state, void *data);
#endif

#endif
