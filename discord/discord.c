/*  RetroArch - A frontend for libretro.
  *  Copyright (C) 2018 - Andrés Suárez
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

#include <file/file_path.h>

#include "discord.h"

#include "../retroarch.h"
#include "../core.h"
#include "../core_info.h"
#include "../paths.h"
#include "../playlist.h"

#include "../msg_hash.h"

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#include "../../network/netplay/netplay_discovery.h"
#include "../../tasks/tasks_internal.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

static const char* APPLICATION_ID = "475456035851599874";
static int FrustrationLevel       = 0;

static int64_t start_time         = 0;
static int64_t pause_time         = 0;
static int64_t ellapsed_time      = 0;

static bool discord_ready         = false;
static unsigned discord_status    = 0;

struct netplay_room *room;

DiscordRichPresence discord_presence;

static void handle_discord_ready(const DiscordUser* connectedUser)
{
   RARCH_LOG("[Discord] connected to user: %s#%s - avatar id: %s\n",
      connectedUser->username,
      connectedUser->discriminator,
      connectedUser->userId);
}

static void handle_discord_disconnected(int errcode, const char* message)
{
   RARCH_LOG("[Discord] disconnected (%d: %s)\n", errcode, message);
}

static void handle_discord_error(int errcode, const char* message)
{
   RARCH_LOG("[Discord] error (%d: %s)\n", errcode, message);
}

static void handle_discord_join(const char* secret)
{
   RARCH_LOG("[Discord] join (%s)\n", secret);
   static struct string_list *list =  NULL;
   list = string_split(secret, "|");

   char tmp_hostname[32];
   snprintf(tmp_hostname,
      sizeof(tmp_hostname),
      "%s|%s", list->elems[0].data, list->elems[1].data);

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      deinit_netplay();
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

   task_push_netplay_crc_scan(atoi(list->elems[3].data),
      list->elems[2].data,
      tmp_hostname, list->elems[4].data);
}

static void handle_discord_spectate(const char* secret)
{
   RARCH_LOG("[Discord] spectate (%s)\n", secret);
}

static void handle_discord_join_request(const DiscordUser* request)
{
   int response = -1;
   char yn[4];
   RARCH_LOG("[Discord] join request from %s#%s - %s\n",
      request->username,
      request->discriminator,
      request->userId);
}

void discord_update(enum discord_presence presence)
{
   core_info_t *core_info = NULL;

   core_info_get_current_core(&core_info);

   if (!discord_ready)
      return;

   if (presence == discord_status)
      return;

   if (presence == DISCORD_PRESENCE_NONE || presence == DISCORD_PRESENCE_MENU)
      memset(&discord_presence, 0, sizeof(discord_presence));

   switch (presence)
   {
      case DISCORD_PRESENCE_MENU:
         discord_presence.details = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU);
         discord_presence.largeImageKey = "base";
         discord_presence.largeImageText = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);
         discord_presence.instance = 0;
         break;
      case DISCORD_PRESENCE_GAME_PAUSED:
         discord_presence.smallImageKey = "paused";
         discord_presence.smallImageText = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED);
         discord_presence.details = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED);
         pause_time = time(0);
         ellapsed_time = difftime(time(0), start_time);
         discord_presence.startTimestamp = pause_time;
         break;
      case DISCORD_PRESENCE_GAME:
         if (core_info)
         {
            const char *system_id  = core_info->system_id ? core_info->system_id : "core";

            char *label = NULL;
            playlist_t *current_playlist = playlist_get_cached();

            if (current_playlist)
               playlist_get_index_by_path(
                  current_playlist, path_get(RARCH_PATH_CONTENT), NULL, &label, NULL, NULL, NULL, NULL);

            if (!label)
               label = (char *)path_basename(path_get(RARCH_PATH_BASENAME));
#if 0
            RARCH_LOG("[Discord] current core: %s\n", system_id);
            RARCH_LOG("[Discord] current content: %s\n", label);
#endif
            discord_presence.largeImageKey = system_id;

            if (core_info->display_name)
               discord_presence.largeImageText = core_info->display_name;

            start_time = time(0);
            if (pause_time != 0)
               start_time = time(0) - ellapsed_time;

            pause_time = 0;
            ellapsed_time = 0;

            discord_presence.smallImageKey = "playing";
            discord_presence.smallImageText = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING);
            discord_presence.startTimestamp = start_time;
            discord_presence.details = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME);

            discord_presence.state = label;
            discord_presence.instance = 0;
         }
         break;
      case DISCORD_PRESENCE_NETPLAY_HOSTING:
         room = netplay_get_host_room();
         if (room->id == 0)
            return;

         RARCH_LOG("[Discord] netplay room details: id=%d, nick=%s IP=%s port=%d\n",
            room->id, room->nickname,
            room->host_method == NETPLAY_HOST_METHOD_MITM ? room->mitm_address : room->address,
            room->host_method == NETPLAY_HOST_METHOD_MITM ? room->mitm_port : room->port);

         char party_id[128];
         snprintf(party_id, sizeof(party_id), "%d|%s", room->id, room->nickname);
         char join_secret[128];
         snprintf(join_secret, sizeof(join_secret), "%s|%d|%s|%u|%s", 
            room->host_method == NETPLAY_HOST_METHOD_MITM ? room->mitm_address : room->address,
            room->host_method == NETPLAY_HOST_METHOD_MITM ? room->mitm_port : room->port,
            room->gamename, room->gamecrc, room->corename);
         RARCH_LOG("%s\n", join_secret);
         discord_presence.joinSecret = strdup(join_secret);
         discord_presence.spectateSecret = "SPECSPECSPEC";
         discord_presence.partyId = party_id;
         discord_presence.partyMax = 0;
         discord_presence.partySize = 0;
         break;
      case DISCORD_PRESENCE_NETPLAY_HOSTING_STOPPED:
      case DISCORD_PRESENCE_NETPLAY_CLIENT:
      default:
         discord_presence.joinSecret = NULL;
         break;
   }

   RARCH_LOG("[Discord] updating (%d)\n", presence);

   Discord_UpdatePresence(&discord_presence);
   discord_status = presence;
}

void discord_init(void)
{
   DiscordEventHandlers handlers;

   RARCH_LOG("[Discord] initializing ..\n");
   start_time            = time(0);

   memset(&handlers, 0, sizeof(handlers));
   handlers.ready        = handle_discord_ready;
   handlers.disconnected = handle_discord_disconnected;
   handlers.errored      = handle_discord_error;
   handlers.joinGame     = handle_discord_join;
   handlers.spectateGame = handle_discord_spectate;
   handlers.joinRequest  = handle_discord_join_request;

   Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);

   discord_ready         = true;
}

void discord_shutdown(void)
{
   RARCH_LOG("[Discord] shutting down ..\n");
   Discord_ClearPresence();
   Discord_Shutdown();
   discord_ready = false;
}

void discord_run_callbacks()
{
   Discord_RunCallbacks();
}