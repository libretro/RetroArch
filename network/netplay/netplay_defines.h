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

#ifndef __RARCH_NETPLAY_DEFINES_H
#define __RARCH_NETPLAY_DEFINES_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

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

enum net_driver_state_flags
{
   NET_DRIVER_ST_FLAG_NETPLAY_CLIENT_DEFERRED      = (1 << 0),
   /* Only used before init_netplay */
   NET_DRIVER_ST_FLAG_NETPLAY_ENABLED              = (1 << 1),
   NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT            = (1 << 2),
   NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_MODE         = (1 << 3),
   NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_ADDRESS   = (1 << 4),
   NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_PORT      = (1 << 5),
   NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_CHECK_FRAMES = (1 << 6)
};

#endif
