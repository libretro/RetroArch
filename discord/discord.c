/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2019 - Andrés Suárez
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
#include <string/stdstring.h>
#include <retro_timers.h>

#include <net/net_http.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <features/features_cpu.h>

#include "discord.h"
#include "discord_register.h"

#include "../deps/discord-rpc/include/discord_rpc.h"

#include "../retroarch.h"
#include "../core.h"
#include "../core_info.h"
#include "../paths.h"
#include "../playlist.h"
#include "../verbosity.h"

#include "../msg_hash.h"
#include "../tasks/task_file_transfer.h"

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#include "../../network/netplay/netplay_discovery.h"
#include "../../tasks/tasks_internal.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos-new/cheevos.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_cbs.h"
#endif

#include "../network/net_http_special.h"
#include "../tasks/tasks_internal.h"
#include "../file_path_special.h"


static int64_t start_time         = 0;
static int64_t pause_time         = 0;
static int64_t ellapsed_time      = 0;

static bool discord_ready         = false;
static bool discord_avatar_ready  = false;
static unsigned discord_status    = 0;

/* The discord API specifies these variables:
- userId --------- char[24]   - the userId of the player asking to join
- username ------- char[344]  - the username of the player asking to join
- discriminator -- char[8]    - the discriminator of the player asking to join
- spectateSecret - char[128] - secret used for spectatin matches
- joinSecret     - char[128] - secret used to join matches
- partyId        - char[128] - the party you would be joining
*/

static char user_name[344];
static char self_party_id[128];
static char peer_party_id[128];

static char user_avatar[PATH_MAX_LENGTH];
static bool connecting = false;

static char cdn_url[] = "https://cdn.discordapp.com/avatars";

DiscordRichPresence discord_presence;

char* discord_get_own_username(void)
{
   if (discord_is_ready())
      return user_name;
   return NULL;
}

char* discord_get_own_avatar(void)
{
   if (discord_is_ready())
      return user_avatar;
   return NULL;
}

bool discord_avatar_is_ready(void)
{
   /*To-Do: fix-me, prevent lockups in ozone due to unfinished code*/
   return false;
}

void discord_avatar_set_ready(bool ready)
{
   discord_avatar_ready = ready;
}

bool discord_is_ready(void)
{
   return discord_ready;
}

#ifdef HAVE_MENU
static bool discord_download_avatar(
      const char* user_id, const char* avatar_id)
{
   static char url[PATH_MAX_LENGTH];
   static char url_encoded[PATH_MAX_LENGTH];
   static char full_path[PATH_MAX_LENGTH];

   static char buf[PATH_MAX_LENGTH];

   file_transfer_t *transf = NULL;

   RARCH_LOG("[discord] user avatar id: %s\n", user_id);

   fill_pathname_application_special(buf,
            sizeof(buf),
            APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
   fill_pathname_join(full_path, buf, avatar_id, sizeof(full_path));
   strlcpy(user_avatar, avatar_id, sizeof(user_avatar));

   if (path_is_valid(full_path))
      return true;

   if (string_is_empty(avatar_id))
      return false;

   snprintf(url, sizeof(url), "%s/%s/%s.png", cdn_url, user_id, avatar_id);
   net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));
   snprintf(buf, sizeof(buf), "%s.png", avatar_id);

   transf           = (file_transfer_t*)calloc(1, sizeof(*transf));
   transf->enum_idx = MENU_ENUM_LABEL_CB_DISCORD_AVATAR;
   strlcpy(transf->path, buf, sizeof(transf->path));

   RARCH_LOG("[discord] downloading avatar from: %s\n", url_encoded);
   task_push_http_transfer(url_encoded, true, NULL, cb_generic_download, transf);

   return false;
}
#endif

static void handle_discord_ready(const DiscordUser* connectedUser)
{
   strlcpy(user_name, connectedUser->username, sizeof(user_name));

   RARCH_LOG("[discord] connected to user: %s#%s\n",
      connectedUser->username,
      connectedUser->discriminator);

#ifdef HAVE_MENU
   discord_download_avatar(connectedUser->userId, connectedUser->avatar);
#endif
}

static void handle_discord_disconnected(int errcode, const char* message)
{
   RARCH_LOG("[discord] disconnected (%d: %s)\n", errcode, message);
}

static void handle_discord_error(int errcode, const char* message)
{
   RARCH_LOG("[discord] error (%d: %s)\n", errcode, message);
}

static void handle_discord_join_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   struct netplay_room *room;
   char join_hostname[PATH_MAX_LENGTH];

   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;

   if (!data || err)
      goto finish;

   data->data = (char*)realloc(data->data, data->len + 1);
   data->data[data->len] = '\0';

   netplay_rooms_parse(data->data);
   room = netplay_room_get(0);

   if (room)
   {
      if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
         deinit_netplay();
      netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

      snprintf(join_hostname, sizeof(join_hostname), "%s|%d",
         room->host_method == NETPLAY_HOST_METHOD_MITM 
         ? room->mitm_address : room->address,
         room->host_method == NETPLAY_HOST_METHOD_MITM 
         ? room->mitm_port : room->port);

      RARCH_LOG("[discord] joining lobby at: %s\n", join_hostname);
      task_push_netplay_crc_scan(room->gamecrc,
         room->gamename, join_hostname, room->corename, room->subsystem_name);
      connecting = true;
      discord_update(DISCORD_PRESENCE_NETPLAY_CLIENT, false);
   }

finish:

   if (err)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), err);

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (user_data)
      free(user_data);
}

static void handle_discord_join(const char* secret)
{
   char url [2048] = "http://lobby.libretro.com/";
   static struct string_list *list =  NULL;

   RARCH_LOG("[discord] join secret: (%s)\n", secret);
   list = string_split(secret, "|");

   strlcpy(peer_party_id, list->elems[0].data, sizeof(peer_party_id));
   strlcat(url, peer_party_id, sizeof(url));
   strlcat(url, "/", sizeof(url));

   RARCH_LOG("[discord] querying lobby id: %s at %s\n", peer_party_id, url);
   task_push_http_transfer(url, true, NULL, handle_discord_join_cb, NULL);
}

static void handle_discord_spectate(const char* secret)
{
   RARCH_LOG("[discord] spectate (%s)\n", secret);
}

#ifdef HAVE_MENU
static void handle_discord_join_response(void *ignore, const char *line)
{
   /* To-Do: needs in-game widgets
   if (strstr(line, "yes"))
      Discord_Respond(user_id, DISCORD_REPLY_YES);

#ifdef HAVE_MENU
   menu_input_dialog_end();
   retroarch_menu_running_finished(false);
#endif
*/
}
#endif

static void handle_discord_join_request(const DiscordUser* request)
{
   static char url[PATH_MAX_LENGTH];
   static char url_encoded[PATH_MAX_LENGTH];
   static char filename[PATH_MAX_LENGTH];
   char buf[PATH_MAX_LENGTH];
#ifdef HAVE_MENU
   menu_input_ctx_line_t line;
#endif

   RARCH_LOG("[discord] join request from %s#%s - %s %s\n",
      request->username,
      request->discriminator,
      request->userId,
      request->avatar);

#ifdef HAVE_MENU
   discord_download_avatar(request->userId, request->avatar);
   /* To-Do: needs in-game widgets
      retroarch_menu_running();
      */

   memset(&line, 0, sizeof(line));
   snprintf(buf, sizeof(buf), "%s %s?", msg_hash_to_str(MSG_DISCORD_CONNECTION_REQUEST), request->username);
   line.label         = buf;
   line.label_setting = "no_setting";
   line.cb            = handle_discord_join_response;

   /* To-Do: needs in-game widgets
      To-Do: bespoke dialog, should show while in-game and have a hotkey to accept
      To-Do: show avatar of the user connecting
      if (!menu_input_dialog_start(&line))
      return;
      */
#endif
}

/* TODO/FIXME - replace last parameter with struct type to allow for more
 * arguments to be passed later */
void discord_update(enum discord_presence presence, bool fuzzy_archive_match)
{
   core_info_t *core_info = NULL;

   core_info_get_current_core(&core_info);

   if (!discord_ready)
      return;

   if (presence == discord_status)
      return;

   if (!connecting && (presence == DISCORD_PRESENCE_NONE || presence == DISCORD_PRESENCE_MENU))
   {
      memset(&discord_presence, 0, sizeof(discord_presence));
      peer_party_id[0] = '\0';
   }

   switch (presence)
   {
      case DISCORD_PRESENCE_MENU:
         discord_presence.details        = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU);
         discord_presence.largeImageKey  = "base";
         discord_presence.largeImageText = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);
         discord_presence.instance = 0;
         break;
      case DISCORD_PRESENCE_GAME_PAUSED:
         discord_presence.smallImageKey  = "paused";
         discord_presence.smallImageText = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED);
         discord_presence.details        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED);
         pause_time                      = time(0);
         ellapsed_time                   = difftime(time(0), start_time);
         discord_presence.startTimestamp = pause_time;
         break;
      case DISCORD_PRESENCE_GAME:
         if (core_info)
         {
            const char *system_id        = core_info->system_id
               ? core_info->system_id : "core";
            const char *label            = NULL;
            const struct playlist_entry *entry  = NULL;
            playlist_t *current_playlist = playlist_get_cached();

            if (current_playlist)
            {
               playlist_get_index_by_path(
                     current_playlist, path_get(RARCH_PATH_CONTENT), &entry,
                     fuzzy_archive_match);

               if (entry && !string_is_empty(entry->label))
                  label = entry->label;
            }

            if (!label)
               label = path_basename(path_get(RARCH_PATH_BASENAME));
#if 0
            RARCH_LOG("[discord] current core: %s\n", system_id);
            RARCH_LOG("[discord] current content: %s\n", label);
#endif
            discord_presence.largeImageKey = system_id;

            if (core_info->display_name)
               discord_presence.largeImageText = core_info->display_name;

            start_time = time(0);
            if (pause_time != 0)
               start_time = time(0) - ellapsed_time;

            pause_time    = 0;
            ellapsed_time = 0;

            discord_presence.smallImageKey  = "playing";
            discord_presence.smallImageText = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING);
            discord_presence.startTimestamp = start_time;
            discord_presence.details        = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME);

            discord_presence.state          = label;
            discord_presence.instance       = 0;

            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            {
               peer_party_id[0] = '\0';
               discord_presence.partyId    = NULL;
               discord_presence.partyMax   = 0;
               discord_presence.partySize  = 0;
               discord_presence.joinSecret = (const char*)'\0';
               connecting = false;
            }
         }
         break;
      case DISCORD_PRESENCE_NETPLAY_HOSTING:
         {
            char join_secret[128];
            struct netplay_room *room = netplay_get_host_room();
            if (room->id == 0)
               return;

            RARCH_LOG("[discord] netplay room details: id=%d"
                  ", nick=%s IP=%s port=%d\n",
                  room->id, room->nickname,
                  room->host_method == NETPLAY_HOST_METHOD_MITM 
                  ? room->mitm_address : room->address,
                  room->host_method == NETPLAY_HOST_METHOD_MITM 
                  ? room->mitm_port : room->port);


            snprintf(self_party_id,
                  sizeof(self_party_id), "%d", room->id);
            snprintf(join_secret,
                  sizeof(join_secret), "%d|%" PRId64,
                  room->id, cpu_features_get_time_usec());

            discord_presence.joinSecret     = strdup(join_secret);
#if 0
            discord_presence.spectateSecret = "SPECSPECSPEC";
#endif
            discord_presence.partyId        = strdup(self_party_id);
            discord_presence.partyMax       = 2;
            discord_presence.partySize      = 1;

            RARCH_LOG("[discord] join secret: %s\n", join_secret);
            RARCH_LOG("[discord] party id: %s\n", self_party_id);
         }
         break;
      case DISCORD_PRESENCE_NETPLAY_CLIENT:
         RARCH_LOG("[discord] party id: %s\n", peer_party_id);
         discord_presence.partyId    = strdup(peer_party_id);
         break;
      case DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED:
         {
            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
            !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
            {
               peer_party_id[0] = '\0';
               discord_presence.partyId    = NULL;
               discord_presence.partyMax   = 0;
               discord_presence.partySize  = 0;
               discord_presence.joinSecret = (const char*)'\0';
               connecting = false;
            }
         }
         break;
      case DISCORD_PRESENCE_SHUTDOWN:
            discord_presence.partyId    = NULL;
            discord_presence.partyMax   = 0;
            discord_presence.partySize  = 0;
            discord_presence.joinSecret = (const char*)'\0';
            connecting = false;
      default:
         break;
   }

   RARCH_LOG("[discord] updating (%d)\n", presence);

   Discord_UpdatePresence(&discord_presence);
   discord_status = presence;
}

void discord_init(const char *discord_app_id)
{
   char full_path[PATH_MAX_LENGTH];
   char command[PATH_MAX_LENGTH];

   DiscordEventHandlers handlers;

   RARCH_LOG("[discord] initializing ..\n");
   start_time            = time(0);

   memset(&handlers, 0, sizeof(handlers));
   handlers.ready        = handle_discord_ready;
   handlers.disconnected = handle_discord_disconnected;
   handlers.errored      = handle_discord_error;
   handlers.joinGame     = handle_discord_join;
   handlers.spectateGame = handle_discord_spectate;
   handlers.joinRequest  = handle_discord_join_request;

   Discord_Initialize(discord_app_id, &handlers, 0, NULL);

#ifdef _WIN32
   fill_pathname_application_path(full_path, sizeof(full_path));
   if (strstr(get_retroarch_launch_arguments(), full_path))
      strlcpy(command, get_retroarch_launch_arguments(), sizeof(command));
   else
   {
      path_basedir(full_path);
      snprintf(command, sizeof(command), "%s%s",
            full_path, get_retroarch_launch_arguments());
   }
#else
   snprintf(command, sizeof(command), "sh -c %s",
         get_retroarch_launch_arguments());
#endif
   RARCH_LOG("[discord] registering startup command: %s\n", command);
   Discord_Register(discord_app_id, command);
   discord_ready = true;
}

void discord_shutdown(void)
{
   RARCH_LOG("[discord] shutting down ..\n");
   Discord_ClearPresence();
   Discord_Shutdown();
   discord_ready = false;
}
