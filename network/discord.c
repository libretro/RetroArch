/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andr�s Su�rez
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

#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <net/net_http.h>
#include <features/features_cpu.h>
#include <time/rtime.h>

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include "netplay/netplay.h"
#endif

#include "../paths.h"
#include "../file_path_special.h"
#include "../tasks/tasks_internal.h"
#include "../tasks/task_file_transfer.h"

#ifdef HAVE_MENU
#include "../menu/menu_cbs.h"
#include "../menu/menu_driver.h"
#endif

#include "discord.h"

#define CDN_URL "https://cdn.discordapp.com/avatars"

/* Forward declaration */
#if defined(__cplusplus) && !defined(CXX_BUILD)
extern "C"
{
#endif
   void Discord_Register(const char *a, const char *b);
#if defined(__cplusplus) && !defined(CXX_BUILD)
}
#endif

static discord_state_t discord_state_st = {0}; /* int64_t alignment */

discord_state_t *discord_state_get_ptr(void)
{
   return &discord_state_st;
}

bool discord_is_ready(void)
{
   discord_state_t *discord_st = &discord_state_st;
   return discord_st->ready;
}

char *discord_get_own_username(void)
{
   discord_state_t *discord_st = &discord_state_st;
   if (discord_st->ready)
      return discord_st->user_name;
   return NULL;
}

char *discord_get_own_avatar(void)
{
   discord_state_t *discord_st = &discord_state_st;
   if (discord_st->ready)
      return discord_st->user_avatar;
   return NULL;
}

bool discord_avatar_is_ready(void)
{
   return false;
}

void discord_avatar_set_ready(bool ready)
{
   discord_state_t *discord_st = &discord_state_st;
   discord_st->avatar_ready    = ready;
}

#ifdef HAVE_MENU
static bool discord_download_avatar(
      discord_state_t *discord_st,
      const char* user_id, const char* avatar_id)
{
   static char url[PATH_MAX_LENGTH];
   static char url_encoded[PATH_MAX_LENGTH];
   static char full_path[PATH_MAX_LENGTH];
   static char buf[PATH_MAX_LENGTH];
   file_transfer_t     *transf = NULL;

   fill_pathname_application_special(buf,
            sizeof(buf),
            APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
   fill_pathname_join_special(full_path, buf, avatar_id, sizeof(full_path));
   strlcpy(discord_st->user_avatar,
         avatar_id, sizeof(discord_st->user_avatar));

   if (path_is_valid(full_path))
      return true;

   if (string_is_empty(avatar_id))
      return false;

   snprintf(url, sizeof(url), "%s/%s/%s" FILE_PATH_PNG_EXTENSION, CDN_URL, user_id, avatar_id);
   net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));
   snprintf(buf, sizeof(buf), "%s" FILE_PATH_PNG_EXTENSION, avatar_id);

   transf           = (file_transfer_t*)malloc(sizeof(*transf));

   transf->enum_idx  = MENU_ENUM_LABEL_CB_DISCORD_AVATAR;
   strlcpy(transf->path, buf, sizeof(transf->path));
   transf->user_data = NULL;

   task_push_http_transfer_file(url_encoded, true, NULL, cb_generic_download, transf);

   return false;
}
#endif

static void handle_discord_ready(const DiscordUser* connectedUser)
{
   discord_state_t *discord_st = &discord_state_st;
   strlcpy(discord_st->user_name,
         connectedUser->username, sizeof(discord_st->user_name));

#ifdef HAVE_MENU
   discord_download_avatar(discord_st,
         connectedUser->userId, connectedUser->avatar);
#endif
}

static void handle_discord_disconnected(int errcode, const char* message)
{
}

static void handle_discord_error(int errcode, const char* message)
{
}

static void handle_discord_join_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   char hostname[512];
   struct netplay_room *room;
   char *room_data             = NULL;
   http_transfer_data_t *data  = (http_transfer_data_t*)task_data;
   discord_state_t *discord_st = &discord_state_st;

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   if (!(room_data = (char*)malloc(data->len + 1)))
      goto done;
   memcpy(room_data, data->data, data->len);
   room_data[data->len] = '\0';

   netplay_rooms_parse(room_data, strlen(room_data));
   free(room_data);

   if ((room = netplay_room_get(0)))
   {
      if (room->host_method == NETPLAY_HOST_METHOD_MITM)
         snprintf(hostname, sizeof(hostname), "%s|%d|%s",
            room->mitm_address, room->mitm_port, room->mitm_session);
      else
         snprintf(hostname, sizeof(hostname), "%s|%d",
            room->address, room->port);

      discord_st->connecting = true;
      if (discord_st->ready)
         discord_update(PRESENCE_NETPLAY_CLIENT);

      task_push_netplay_crc_scan(room->gamecrc, room->gamename,
         room->subsystem_name, room->corename, hostname);
   }

   netplay_rooms_free();

done:
   free(user_data);
}

static void handle_discord_join(const char *secret)
{
   int room_id;
   char url[512];

   if ((room_id = (int)strtol(secret, NULL, 10)))
   {
      discord_state_t *discord_st = &discord_state_st;
      snprintf(discord_st->peer_party_id,
            sizeof(discord_st->peer_party_id),
            "%d", room_id);

      strlcpy(url, FILE_PATH_LOBBY_LIBRETRO_URL, sizeof(url));
      strlcat(url, discord_st->peer_party_id, sizeof(url));

      task_push_http_transfer(url, true, NULL, handle_discord_join_cb, NULL);
   }
}

static void handle_discord_spectate(const char *secret)
{
}

#if 0
#ifdef HAVE_MENU
static void handle_discord_join_response(void *ignore, const char *line)
{
   /* TODO/FIXME: needs in-game widgets */
   if (strstr(line, "yes"))
      Discord_Respond(user_id, DISCORD_REPLY_YES);

   menu_input_dialog_end();
   retroarch_menu_running_finished(false);
}
#endif
#endif

static void handle_discord_join_request(const DiscordUser *request)
{
#ifdef HAVE_MENU
#if 0
   char buf[PATH_MAX_LENGTH];
   menu_input_ctx_line_t line = {0};
#endif
   discord_state_t *discord_st = &discord_state_st;

   discord_download_avatar(discord_st, request->userId, request->avatar);

#if 0
   /* TODO/FIXME: Needs in-game widgets */
   retroarch_menu_running();

   snprintf(buf, sizeof(buf), "%s %s?",
      msg_hash_to_str(MSG_DISCORD_CONNECTION_REQUEST), request->username);
   line.label         = buf;
   line.label_setting = "no_setting";
   line.cb            = handle_discord_join_response;

   /* TODO/FIXME: needs in-game widgets
    * TODO/FIXME: bespoke dialog, should show while in-game
    * and have a hotkey to accept
    * TODO/FIXME: show avatar of the user connecting
    */
   if (!menu_input_dialog_start(&line))
      return;
#endif
#endif
}

void discord_update(enum presence presence)
{
   discord_state_t *discord_st = &discord_state_st;
#ifdef HAVE_CHEEVOS
   char cheevos_richpresence[256];
#endif

   if (presence == discord_st->status)
      return;

   if (!discord_st->connecting
         &&
         (   presence == PRESENCE_NONE
          || presence == PRESENCE_MENU))
   {
      memset(&discord_st->presence,
            0, sizeof(discord_st->presence));
      discord_st->peer_party_id[0] = '\0';
   }

   switch (presence)
   {
      case PRESENCE_MENU:
         discord_st->presence.details        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU);
         discord_st->presence.largeImageKey  = "base";
         discord_st->presence.largeImageText = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_NO_CORE);
         discord_st->presence.instance       = 0;
         break;
      case PRESENCE_GAME_PAUSED:
         discord_st->presence.smallImageKey  = "paused";
         discord_st->presence.smallImageText = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED);
         discord_st->presence.details        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED);
         discord_st->pause_time              = time(0);
         discord_st->elapsed_time            = difftime(discord_st->pause_time,
               discord_st->start_time);
         discord_st->presence.startTimestamp = discord_st->pause_time;
         break;
      case PRESENCE_GAME:
         {
            core_info_t      *core_info     = NULL;
            core_info_get_current_core(&core_info);

            if (core_info)
            {
               const char *system_id               =
                    core_info->system_id
                  ? core_info->system_id
                  : "core";
               const char *label                   = NULL;
               const struct playlist_entry *entry  = NULL;
               playlist_t *current_playlist        = playlist_get_cached();

               if (current_playlist)
               {
                  playlist_get_index_by_path(
                        current_playlist,
                        path_get(RARCH_PATH_CONTENT),
                        &entry);

                  if (entry && !string_is_empty(entry->label))
                     label = entry->label;
               }

               if (!label)
                  label = path_basename(path_get(RARCH_PATH_BASENAME));
               discord_st->presence.largeImageKey = system_id;

               if (core_info->display_name)
                  discord_st->presence.largeImageText =
                     core_info->display_name;

               discord_st->start_time              = time(0);
               if (discord_st->pause_time != 0)
                  discord_st->start_time           = time(0) -
                     discord_st->elapsed_time;

               discord_st->pause_time              = 0;
               discord_st->elapsed_time            = 0;

               discord_st->presence.smallImageKey  = "playing";
               discord_st->presence.smallImageText = msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING);
               discord_st->presence.startTimestamp = discord_st->start_time;

#ifdef HAVE_CHEEVOS
               if (rcheevos_get_richpresence(cheevos_richpresence, sizeof(cheevos_richpresence)) > 0)
                  discord_st->presence.details     = cheevos_richpresence;
               else
#endif
                   discord_st->presence.details    = msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME);

               discord_st->presence.state          = label;
               discord_st->presence.instance       = 0;

               if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
               {
                  discord_st->peer_party_id[0]     = '\0';
                  discord_st->connecting           = false;
                  discord_st->presence.partyId     = NULL;
                  discord_st->presence.partyMax    = 0;
                  discord_st->presence.partySize   = 0;
                  discord_st->presence.joinSecret  = (const char*)'\0';
               }
            }
         }
         break;
      case PRESENCE_NETPLAY_HOSTING:
         {
            char join_secret[128];
            struct netplay_room *room   = &networking_state_get_ptr()->host_room;
            if (room->id == 0)
               return;

            snprintf(discord_st->self_party_id,
                  sizeof(discord_st->self_party_id), "%d", room->id);
            snprintf(join_secret,
                  sizeof(join_secret), "%d|%" PRId64,
                  room->id, cpu_features_get_time_usec());

            discord_st->presence.joinSecret     = strdup(join_secret);
#if 0
            discord_st->presence.spectateSecret = "SPECSPECSPEC";
#endif
            discord_st->presence.partyId        = strdup(discord_st->self_party_id);
            discord_st->presence.partyMax       = 2;
            discord_st->presence.partySize      = 1;
         }
         break;
      case PRESENCE_NETPLAY_CLIENT:
         discord_st->presence.partyId    = strdup(discord_st->peer_party_id);
         break;
      case PRESENCE_NETPLAY_NETPLAY_STOPPED:
         {
            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
            !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
            {
               discord_st->peer_party_id[0]     = '\0';
               discord_st->connecting           = false;
               discord_st->presence.partyId     = NULL;
               discord_st->presence.partyMax    = 0;
               discord_st->presence.partySize   = 0;
               discord_st->presence.joinSecret  = (const char*)'\0';
            }
         }
         break;
#ifdef HAVE_CHEEVOS
      case PRESENCE_RETROACHIEVEMENTS:
         if (discord_st->pause_time)
            return;

         if (rcheevos_get_richpresence(cheevos_richpresence, sizeof(cheevos_richpresence)) > 0)
            discord_st->presence.details = cheevos_richpresence;
         presence = PRESENCE_GAME;
         break;
#endif
      case PRESENCE_SHUTDOWN:
            discord_st->presence.partyId    = NULL;
            discord_st->presence.partyMax   = 0;
            discord_st->presence.partySize  = 0;
            discord_st->presence.joinSecret = (const char*)'\0';
            discord_st->connecting          = false;
      default:
         break;
   }

   Discord_UpdatePresence(&discord_st->presence);
#ifdef DISCORD_DISABLE_IO_THREAD
   Discord_UpdateConnection();
#endif
   discord_st->status = presence;
}

void discord_init(const char *discord_app_id, char *args)
{
   DiscordEventHandlers handlers;
#ifdef _WIN32
   char full_path[PATH_MAX_LENGTH];
#endif
   char command[PATH_MAX_LENGTH];
   discord_state_t *discord_st = &discord_state_st;

   discord_st->start_time      = time(0);

   handlers.ready              = handle_discord_ready;
   handlers.disconnected       = handle_discord_disconnected;
   handlers.errored            = handle_discord_error;
   handlers.joinGame           = handle_discord_join;
   handlers.spectateGame       = handle_discord_spectate;
   handlers.joinRequest        = handle_discord_join_request;

   Discord_Initialize(discord_app_id, &handlers, 0, NULL);
#ifdef DISCORD_DISABLE_IO_THREAD
   Discord_UpdateConnection();
#endif

#ifdef _WIN32
   fill_pathname_application_path(full_path, sizeof(full_path));
   if (strstr(args, full_path))
      strlcpy(command, args, sizeof(command));
   else
   {
      path_basedir(full_path);
      strlcpy(command, full_path, sizeof(command));
      strlcat(command, args,      sizeof(command));
   }
#else
   command[0] = 's';
   command[1] = 'h';
   command[2] = ' ';
   command[3] = '-';
   command[4] = 'c';
   command[5] = ' ';
   command[6] = '\0';
   strlcat(command, args, sizeof(command));
#endif
   Discord_Register(discord_app_id, command);
#ifdef DISCORD_DISABLE_IO_THREAD
   Discord_UpdateConnection();
#endif
   discord_st->ready = true;
}
