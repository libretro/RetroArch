/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2019-2023 - Brian Weiss
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

#include <string.h>
#include <ctype.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <formats/cdfs.h>
#include <formats/m3u_file.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <retro_timers.h>
#include <net/net_http.h>
#include <libretro.h>
#include <lrc_hash.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef HAVE_CHEATS
#include "../cheat_manager.h"
#endif

#ifdef HAVE_CHD
#include "streams/chd_stream.h"
#endif

#include "cheevos.h"
#include "cheevos_client.h"
#include "cheevos_menu.h"
#include "cheevos_locals.h"

#include "../network/netplay/netplay.h"

#include "../audio/audio_driver.h"
#include "../file_path_special.h"
#include "../paths.h"
#include "../command.h"
#include "../configuration.h"
#include "../performance_counters.h"
#include "../msg_hash.h"
#include "../retroarch.h"
#include "../runtime_file.h"
#include "../core.h"
#include "../core_option_manager.h"

#include "../tasks/tasks_internal.h"

#include "../deps/rcheevos/include/rc_runtime.h"
#include "../deps/rcheevos/include/rc_runtime_types.h"
#include "../deps/rcheevos/include/rc_hash.h"
#include "../deps/rcheevos/src/rc_libretro.h"

/* Define this macro to prevent cheevos from being deactivated when they trigger. */
#undef CHEEVOS_DONT_DEACTIVATE

static rcheevos_locals_t rcheevos_locals =
{
   NULL, /* client */
   {{0}},/* memory */
#ifdef HAVE_THREADS
   CMD_EVENT_NONE, /* queued_command */
   false, /* game_placard_requested */
#endif
   "",   /* user_agent_prefix */
   "",   /* user_agent_core */
#ifdef HAVE_MENU
   NULL, /* menuitems */
   0,    /* menuitem_capacity */
   0,    /* menuitem_count */
#endif
   true, /* hardcore_allowed */
   false,/* hardcore_being_enabled */
   true  /* core_supports */
};

rcheevos_locals_t* get_rcheevos_locals(void)
{
   return &rcheevos_locals;
}

#define CHEEVOS_MB(x)   ((x) * 1024 * 1024)

/*****************************************************************************
Supporting functions.
*****************************************************************************/

#define CMD_CHEEVOS_NON_COMMAND -1
static void rcheevos_show_game_placard(void);

#ifndef CHEEVOS_VERBOSE
void rcheevos_log(const char* fmt, ...)
{
   (void)fmt;
}
#endif

static void rcheevos_handle_log_message(const char* message)
{
   CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", message);
}

static void rcheevos_get_core_memory_info(uint32_t id,
      rc_libretro_core_memory_info_t* info)
{
   retro_ctx_memory_info_t ctx_info;
   if (!info)
      return;

   ctx_info.id = id;
   if (core_get_memory(&ctx_info))
   {
      info->data = (unsigned char*)ctx_info.data;
      info->size = ctx_info.size;
   }
   else
   {
      info->data = NULL;
      info->size = 0;
   }
}

static int rcheevos_init_memory(rcheevos_locals_t* locals)
{
   unsigned i;
   int result;
   struct retro_memory_map mmap;
   const rc_client_game_t* game;
   rarch_system_info_t *sys_info               = &runloop_state_get_ptr()->system;
   rarch_memory_map_t *mmaps                   = &sys_info->mmaps;
   struct retro_memory_descriptor* descriptors;
   unsigned console_id;

   /* if no game is loaded, fallback to a default mapping (SYSTEM RAM followed by SAVE RAM) */
   game = rc_client_get_game_info(locals->client);
   console_id = game ? game->console_id : 0;

   descriptors = (struct retro_memory_descriptor*)malloc(mmaps->num_descriptors * sizeof(*descriptors));
   if (!descriptors)
      return 0;

   mmap.descriptors = &descriptors[0];
   mmap.num_descriptors = mmaps->num_descriptors;

   /* RetroArch wraps the retro_memory_descriptor's
    * in rarch_memory_descriptor_t's, pull them back out */
   for (i = 0; i < mmap.num_descriptors; ++i)
      memcpy(&descriptors[i], &mmaps->descriptors[i].core,
            sizeof(descriptors[0]));

   rc_libretro_init_verbose_message_callback(rcheevos_handle_log_message);
   result = rc_libretro_memory_init(&locals->memory, &mmap,
         rcheevos_get_core_memory_info, console_id);

   free(descriptors);
   return result;
}

uint8_t* rcheevos_patch_address(unsigned address)
{
   /* Memory map was not previously initialized
    * (no achievements for this game?), try now */
   if (rcheevos_locals.memory.count == 0)
      rcheevos_init_memory(&rcheevos_locals);
   return rc_libretro_memory_find(&rcheevos_locals.memory, address);
}

static bool rcheevos_is_game_loaded(void)
{
   return rc_client_is_game_loaded(rcheevos_locals.client);
}

static bool rcheevos_is_player_active(void)
{
   if (netplay_is_spectating())
      return false;

   /* TODO: disallow player slots other than player one unless it's a [Multi] set */

   return true;
}

void rcheevos_spectating_changed(void)
{
   /* don't update spectator mode while a game is loading - it prevents being able to change it later */
   if (rcheevos_is_game_loaded())
   {
      const bool spectating = !rcheevos_is_player_active();
      if (spectating != rc_client_get_spectator_mode_enabled(rcheevos_locals.client))
         rc_client_set_spectator_mode_enabled(rcheevos_locals.client, !rcheevos_is_player_active());
   }
}

bool rcheevos_is_pause_allowed(void)
{
   return rc_client_can_pause(rcheevos_locals.client, NULL);
}

static void rcheevos_show_mastery_placard(void)
{
   const settings_t* settings = config_get_ptr();
   if (settings->bools.cheevos_visibility_mastery)
   {
      const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals.client);
      char title[256];

      snprintf(title, sizeof(title),
         msg_hash_to_str(rc_client_get_hardcore_enabled(rcheevos_locals.client)
            ? MSG_CHEEVOS_MASTERED_GAME
            : MSG_CHEEVOS_COMPLETED_GAME),
         game->title);
      title[sizeof(title) - 1] = '\0';

#if defined (HAVE_GFX_WIDGETS)
      if (gfx_widgets_ready())
      {
         char msg[128];
         char badge_name[32];
         const char* displayname = rc_client_get_user_info(rcheevos_locals.client)->display_name;
         const bool content_runtime_log = settings->bools.content_runtime_log;
         const bool content_runtime_log_aggr = settings->bools.content_runtime_log_aggregate;
         size_t len = strlcpy(msg, displayname, sizeof(msg));

         if (len < sizeof(msg) - 12
            && (content_runtime_log || content_runtime_log_aggr))
         {
            const char* content_path = path_get(RARCH_PATH_CONTENT);
            const char* core_path = path_get(RARCH_PATH_CORE);
            runtime_log_t* runtime_log = runtime_log_init(
               content_path, core_path,
               settings->paths.directory_runtime_log,
               settings->paths.directory_playlist,
               !content_runtime_log_aggr);

            if (runtime_log)
            {
               const runloop_state_t* runloop_state = runloop_state_get_ptr();
               runtime_log_add_runtime_usec(runtime_log,
                  runloop_state->core_runtime_usec);

               len += strlcpy(msg + len, " | ", sizeof(msg) - len);
               runtime_log_get_runtime_str(runtime_log, msg + len, sizeof(msg) - len);
               msg[sizeof(msg) - 1] = '\0';

               free(runtime_log);
            }
         }

         len = strlcpy(badge_name, "i", sizeof(badge_name));
         strlcpy(badge_name + len, game->badge_name, sizeof(badge_name) - len);
         gfx_widgets_push_achievement(title, msg, badge_name);
      }
      else
#endif
         runloop_msg_queue_push(title, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_award_achievement(const rc_client_achievement_t* cheevo)
{
   const settings_t* settings = config_get_ptr();

   if (!cheevo)
      return;

   /* Show the on screen message. */
   if (settings->bools.cheevos_visibility_unlock)
   {
#if defined(HAVE_GFX_WIDGETS)
      if (gfx_widgets_ready())
      {
         char title[128], subtitle[96];
         float rarity = rc_client_get_hardcore_enabled(rcheevos_locals.client) ?
            cheevo->rarity_hardcore : cheevo->rarity;

         if (rarity >= 10.0)
            snprintf(title, sizeof(title), "%s - %0.2f%%",
               msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED), rarity);
         else if (rarity > 0.0)
            snprintf(title, sizeof(title), "%s - %0.2f%%",
               msg_hash_to_str(MSG_RARE_ACHIEVEMENT_UNLOCKED), rarity);
         else
            strlcpy(title,
               msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED), sizeof(title));

         snprintf(subtitle, sizeof(subtitle), "%s (%d)", cheevo->title, cheevo->points);

         gfx_widgets_push_achievement(title, subtitle, cheevo->badge_name);
      }
      else
#endif
      {
         char buffer[256];
         size_t _len = strlcpy(buffer, msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED),
               sizeof(buffer));
         _len += strlcpy(buffer + _len, ": ", sizeof(buffer) - _len);
         strlcpy(buffer + _len, cheevo->title, sizeof(buffer) - _len);
         runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         runloop_msg_queue_push(cheevo->description, 0, 3 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

#ifdef HAVE_AUDIOMIXER
   /* Play the unlock sound */
   if (settings->bools.cheevos_unlock_sound_enable)
      audio_driver_mixer_play_menu_sound(
         AUDIO_MIXER_SYSTEM_SLOT_ACHIEVEMENT_UNLOCK);
#endif

#ifdef HAVE_SCREENSHOTS
   /* Take a screenshot of the achievement. */
   if (settings->bools.cheevos_auto_screenshot)
   {
      size_t shotname_len = sizeof(char) * 8192;
      char* shotname = (char*)malloc(shotname_len);

      if (shotname)
      {
         video_driver_state_t* video_st = video_state_get_ptr();;
         snprintf(shotname, shotname_len, "%s/%s-cheevo-%u",
            settings->paths.directory_screenshot,
            path_basename(path_get(RARCH_PATH_BASENAME)),
            (unsigned)cheevo->id);
         shotname[shotname_len - 1] = '\0';

         if (take_screenshot(settings->paths.directory_screenshot,
            shotname,
            true,
            video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID),
            false,
            true))
            CHEEVOS_LOG(RCHEEVOS_TAG
               "Captured screenshot for achievement %u\n",
               cheevo->id);
         else
            CHEEVOS_LOG(RCHEEVOS_TAG
               "Failed to capture screenshot for achievement %u\n",
               cheevo->id);

         free(shotname);
      }
   }
#endif
}

static void rcheevos_lboard_submitted(const rc_client_leaderboard_t* lboard, const rc_client_leaderboard_scoreboard_t* scoreboard)
{
   const settings_t* settings = config_get_ptr();
   if (lboard && settings->bools.cheevos_visibility_lboard_submit)
   {
      char buffer[256];
      if (scoreboard)
      {
         char addendum[64];
         const size_t len = snprintf(buffer, sizeof(buffer), msg_hash_to_str(MSG_LEADERBOARD_SUBMISSION),
            scoreboard->submitted_score, lboard->title);
         if (strcmp(scoreboard->best_score, scoreboard->submitted_score) == 0)
            snprintf(addendum, sizeof(addendum), msg_hash_to_str(MSG_LEADERBOARD_RANK), scoreboard->new_rank);
         else
            snprintf(addendum, sizeof(addendum), msg_hash_to_str(MSG_LEADERBOARD_BEST), scoreboard->best_score);
         snprintf(buffer + len, sizeof(buffer) - len, " (%s)", addendum);
      }
      else
      {
         snprintf(buffer, sizeof(buffer), msg_hash_to_str(MSG_LEADERBOARD_SUBMISSION),
            lboard->tracker_value, lboard->title);
      }
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_lboard_canceled(const rc_client_leaderboard_t* lboard)
{
   const settings_t* settings = config_get_ptr();
   if (lboard && settings->bools.cheevos_visibility_lboard_cancel)
   {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%s: %s",
         msg_hash_to_str(MSG_LEADERBOARD_FAILED), lboard->title);
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_lboard_started(const rc_client_leaderboard_t* lboard)
{
   const settings_t* settings = config_get_ptr();
   if (settings->bools.cheevos_visibility_lboard_start)
   {
      char buffer[256];
      size_t _len = strlcpy(buffer, msg_hash_to_str(MSG_LEADERBOARD_STARTED),
            sizeof(buffer));
      _len += strlcpy(buffer + _len, ": ", sizeof(buffer) - _len);
      _len += strlcpy(buffer + _len, lboard->title, sizeof(buffer) - _len);
      if (lboard->description && *lboard->description)
         snprintf(buffer + _len, sizeof(buffer) - _len, "- %s",
            lboard->description);

      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

#if defined(HAVE_GFX_WIDGETS)
static void rcheevos_lboard_update_tracker(const rc_client_leaderboard_tracker_t* tracker)
{
   const settings_t* settings = config_get_ptr();
   if (tracker && gfx_widgets_ready() && settings->bools.cheevos_visibility_lboard_trackers)
      gfx_widgets_set_leaderboard_display(tracker->id, tracker->display);
}

static void rcheevos_lboard_hide_tracker(const rc_client_leaderboard_tracker_t* tracker)
{
   const settings_t* settings = config_get_ptr();
   if (tracker && gfx_widgets_ready() && settings->bools.cheevos_visibility_lboard_trackers)
      gfx_widgets_set_leaderboard_display(tracker->id, NULL);
}

static void rcheevos_challenge_started(const rc_client_achievement_t* cheevo)
{
   settings_t* settings = config_get_ptr();
   if (cheevo && gfx_widgets_ready() && settings->bools.cheevos_challenge_indicators)
      gfx_widgets_set_challenge_display(cheevo->id, cheevo->badge_name);
}

static void rcheevos_challenge_ended(const rc_client_achievement_t* cheevo)
{
   if (cheevo && gfx_widgets_ready())
      gfx_widgets_set_challenge_display(cheevo->id, NULL);
}

static void rcheevos_progress_updated(rcheevos_locals_t* locals,
   const rc_client_achievement_t* cheevo)
{
   settings_t* settings = config_get_ptr();
   if (cheevo && gfx_widgets_ready() && settings->bools.cheevos_visibility_progress_tracker)
      gfx_widget_set_achievement_progress(cheevo->badge_name, cheevo->measured_progress);
}

static void rcheevos_progress_hide(rcheevos_locals_t* locals)
{
   settings_t* settings = config_get_ptr();
   if (gfx_widgets_ready() && settings->bools.cheevos_visibility_progress_tracker)
      gfx_widget_set_achievement_progress(NULL, NULL);
}

#endif

static void rcheevos_client_log_message(const char* message, const rc_client_t* client)
{
   CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", message);
}

static void rcheevos_server_error(const char* api_name, const char* message)
{
   char buffer[256];
   size_t _len = strlcpy(buffer, api_name, sizeof(buffer));
   _len += strlcpy(buffer + _len, " failed: ", sizeof(buffer) - _len);
   _len += strlcpy(buffer + _len, message, sizeof(buffer) - _len);
   runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
      MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
}

static void rcheevos_server_disconnected(void)
{
   CHEEVOS_LOG(RCHEEVOS_TAG "Unable to communicate with RetroAchievements server\n");

   /* always show message - even with widget. it helps the user understand what the widget is for */
   {
      const char* message = msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED);
      runloop_msg_queue_push(message, 0, 3 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
   }

#if defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_ready())
      gfx_widget_set_cheevos_disconnect(true);
#endif
}

static void rcheevos_server_reconnected(void)
{
   CHEEVOS_LOG(RCHEEVOS_TAG "All pending requests synced to RetroAchievements server\n");

   {
      const char* message = msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED);
      runloop_msg_queue_push(message, 0, 3 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_SUCCESS);
   }

#if defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_ready())
      gfx_widget_set_cheevos_disconnect(false);
#endif
}

static void rcheevos_client_event_handler(const rc_client_event_t* event, rc_client_t* client)
{
   switch (event->type)
   {
#ifdef HAVE_GFX_WIDGETS
   case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
      rcheevos_lboard_update_tracker(event->leaderboard_tracker);
      break;
   case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
      rcheevos_challenge_started(event->achievement);
      break;
   case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
      rcheevos_challenge_ended(event->achievement);
      break;
   case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
      rcheevos_progress_updated(&rcheevos_locals, event->achievement);
      break;
   case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
      rcheevos_progress_updated(&rcheevos_locals, event->achievement);
      break;
   case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
      rcheevos_progress_hide(&rcheevos_locals);
      break;
   case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
      rcheevos_lboard_update_tracker(event->leaderboard_tracker);
      break;
   case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
      rcheevos_lboard_hide_tracker(event->leaderboard_tracker);
      break;
#endif
   case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
      rcheevos_award_achievement(event->achievement);
      break;
   case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
      rcheevos_lboard_started(event->leaderboard);
      break;
   case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
      rcheevos_lboard_canceled(event->leaderboard);
      break;
   case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
      /* don't notify on submission - report new rank/best score after submission via SCOREBOARD event */
      break;
   case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
      rcheevos_lboard_submitted(event->leaderboard, event->leaderboard_scoreboard);
      break;
   case RC_CLIENT_EVENT_RESET:
      command_event(CMD_EVENT_RESET, NULL); /* reset the game */
      break;
   case RC_CLIENT_EVENT_GAME_COMPLETED:
      rcheevos_show_mastery_placard();
      break;
   case RC_CLIENT_EVENT_SERVER_ERROR:
      rcheevos_server_error(event->server_error->api, event->server_error->error_message);
      break;
   case RC_CLIENT_EVENT_DISCONNECTED:
      rcheevos_server_disconnected();
      break;
   case RC_CLIENT_EVENT_RECONNECTED:
      rcheevos_server_reconnected();
      break;
   default:
#ifndef NDEBUG
      CHEEVOS_LOG(RCHEEVOS_TAG "Unsupported rc_client event %u\n", event->type);
#endif
      break;
   }
}

int rcheevos_get_richpresence(char* s, size_t len)
{
   if (!rcheevos_is_player_active())
   {
      if (!rcheevos_is_game_loaded())
         return 0;

      return snprintf(s, len, msg_hash_to_str(MSG_CHEEVOS_RICH_PRESENCE_SPECTATING),
                      rc_client_get_game_info(rcheevos_locals.client)->title);
   }

   return (int)rc_client_get_rich_presence_message(rcheevos_locals.client, s, (size_t)len);
}

int rcheevos_get_game_badge_url(char* s, size_t len)
{
   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals.client);
   if (!game || !game->id || !game->badge_name || !game->badge_name[0])
      return 0;

   return (rc_client_game_get_image_url(game, s, len) == RC_OK);
}

#ifdef HAVE_GFX_WIDGETS

static void rcheevos_hide_widgets(bool widgets_ready)
{
   /* Hide any visible trackers */
   if (widgets_ready)
   {
      gfx_widgets_clear_leaderboard_displays();
      gfx_widgets_clear_challenge_displays();
      gfx_widget_set_achievement_progress(NULL, NULL);
   }
}

#endif

void rcheevos_reset_game(bool widgets_ready)
{
#if defined(HAVE_GFX_WIDGETS)
   /* Hide any visible trackers */
   rcheevos_hide_widgets(widgets_ready);
#endif

   rc_client_reset(rcheevos_locals.client);

   /* Some cores reallocate memory on reset,
    * make sure we update our pointers */
   if (rcheevos_locals.memory.total_size > 0)
      rcheevos_init_memory(&rcheevos_locals);
}

void rcheevos_refresh_memory(void)
{
   if (rcheevos_locals.memory.total_size > 0)
      rcheevos_init_memory(&rcheevos_locals);
}

bool rcheevos_hardcore_active(void)
{
   /* normal hardcore check */
   if (rcheevos_locals.client && rc_client_get_hardcore_enabled(rcheevos_locals.client))
      return true;

   /* if we're trying to enable hardcore, pretend it's on so the caller can decide to disable
    * it (by calling rcheevos_pause_hardcore) before we actually turn it on. */
   return rcheevos_locals.hardcore_being_enabled;
}

void rcheevos_pause_hardcore(void)
{
   rcheevos_locals.hardcore_allowed = false;

   if (rcheevos_hardcore_active())
      rcheevos_toggle_hardcore_paused();
}

bool rcheevos_unload(void)
{
   settings_t* settings  = config_get_ptr();
   const bool was_loaded = rcheevos_is_game_loaded();

#ifdef HAVE_GFX_WIDGETS
   rcheevos_hide_widgets(gfx_widgets_ready());
   gfx_widget_set_cheevos_set_loading(false);
#endif

   rc_client_unload_game(rcheevos_locals.client);

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
   rcheevos_locals.game_placard_requested = false;
#endif

   if (rcheevos_locals.memory.count > 0)
      rc_libretro_memory_destroy(&rcheevos_locals.memory);

   if (was_loaded)
   {
#ifdef HAVE_MENU
      rcheevos_menu_reset_badges();

      if (rcheevos_locals.menuitems)
      {
         CHEEVOS_FREE(rcheevos_locals.menuitems);
         rcheevos_locals.menuitems              = NULL;
         rcheevos_locals.menuitem_capacity      =
            rcheevos_locals.menuitem_count      = 0;
      }
#endif
   }

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
#endif

   if (!settings->arrays.cheevos_token[0])
   {
      /* If the config-level token has been cleared, we need to re-login on
       * loading the next game. Easiest way to do that is to destroy the client */
      rc_client_t* client = rcheevos_locals.client;
      rcheevos_locals.client = NULL;

      rc_client_destroy(client);
   }

   return true;
}

void rcheevos_leaderboard_trackers_visibility_changed(void)
{
#if defined(HAVE_GFX_WIDGETS)
   const settings_t* settings = config_get_ptr();
   if (!settings->bools.cheevos_visibility_lboard_trackers)
   {
      /* Hide any visible trackers */
      gfx_widgets_clear_leaderboard_displays();
   }
   else
   {
      /* No way to immediately request trackers be reshown, but they
       * will reappear the next time they're updated */
   }
#endif
}

static void rcheevos_enforce_hardcore_settings(void)
{
   /* disable slowdown */
   runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_SLOWMOTION;
}

static void rcheevos_toggle_hardcore_active(rcheevos_locals_t* locals)
{
   settings_t* settings = config_get_ptr();
   bool rewind_enable   = settings->bools.rewind_enable;
   const bool was_enabled = rcheevos_hardcore_active();

   if (!was_enabled)
   {
      locals->hardcore_being_enabled = true;
      locals->hardcore_allowed = true;

      /* If one or more invalid settings is enabled, abort*/
      rcheevos_validate_config_settings();
      if (!locals->hardcore_allowed)
      {
         locals->hardcore_being_enabled = false;
         return;
      }

#ifdef HAVE_CHEATS
      /* If one or more emulator managed cheats is active, abort */
      cheat_manager_apply_cheats();
      if (!locals->hardcore_allowed)
      {
         locals->hardcore_being_enabled = false;
         return;
      }
#endif

      if (rcheevos_is_game_loaded())
      {
         const char* msg = msg_hash_to_str(
               MSG_CHEEVOS_HARDCORE_MODE_ENABLE);
         CHEEVOS_LOG("%s\n", msg);
         runloop_msg_queue_push(msg, 0, 3 * 60, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         rcheevos_enforce_hardcore_settings();
      }

      /* deinit rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* have to "schedule" this.
             * CMD_EVENT_REWIND_DEINIT should
             * only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_DEINIT;
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
      }

      locals->hardcore_being_enabled = false;
      rc_client_set_hardcore_enabled(locals->client, 1);
   }
   else
   {
      /* pause hardcore */
      rc_client_set_hardcore_enabled(locals->client, 0);

      /* re-init rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* have to "schedule" this.
             * CMD_EVENT_REWIND_INIT should
             * only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_INIT;
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_INIT, NULL);
      }
   }
}

void rcheevos_toggle_hardcore_paused(void)
{
   settings_t* settings = config_get_ptr();
   /* if hardcore mode is not enabled, we can't toggle whether its active */
   if (settings->bools.cheevos_hardcore_mode_enable)
      rcheevos_toggle_hardcore_active(&rcheevos_locals);
}

void rcheevos_hardcore_enabled_changed(void)
{
   /* called whenever a setting that could potentially affect hardcore enabledness changes
    * (i.e. cheevos_enable, hardcore_mode_enable) to synchronize the internal state to the configs.
    * also called when a game is first loaded to synchronize the internal state to the configs. */
   const settings_t* settings = config_get_ptr();
   const bool enabled         = settings
      && settings->bools.cheevos_enable
      && settings->bools.cheevos_hardcore_mode_enable;
   const bool was_enabled = rcheevos_hardcore_active();

   if (enabled != was_enabled)
   {
      rcheevos_toggle_hardcore_active(&rcheevos_locals);
   }
   else if (was_enabled && rcheevos_is_game_loaded())
   {
      /* hardcore enabledness didn't change, but hardcore is active, so make
       * sure to enforce the restrictions. */
      rcheevos_enforce_hardcore_settings();
   }
}

void rcheevos_validate_config_settings(void)
{
   int i;
   const rc_disallowed_setting_t
      *disallowed_settings           = NULL;
   core_option_manager_t* coreopts   = NULL;
   struct retro_system_info *sysinfo =
      &runloop_state_get_ptr()->system.info;
   const settings_t* settings = config_get_ptr();
   unsigned console_id;

   if (!rcheevos_hardcore_active())
      return;

   /* this adds a sleep to every frame. if the value is high enough that a
    * single frame takes more than 1/60th of a second to evaluate, render,
    * and sleep, then the real framerate is less than 60fps. with vsync on,
    * it'll wait for the next vsync event, effectively halfing the fps. the
    * auto setting should achieve the most accurate frame rate anyway, so
    * disallow any manual values */
   if (!settings->bools.video_frame_delay_auto && settings->uints.video_frame_delay != 0) {
      const char* error = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY);
      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg_hash_to_str_us(MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY));
      rcheevos_pause_hardcore();

      runloop_msg_queue_push(error, 0, 4 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      return;
   }

   /* this specifies how many vsync events should occur for each rendered
    * frame. if vsync is on for a 60Hz monitor and swap_interval is 2 (only
    * update every other vsync), only 30fps will be generated. for a 144Hz
    * monitor, a value of 2 will generate 72fps, which is still faster than
    * the expected 60fps, so the user should really be using auto (0).
    * allow 1 even though that could be potentially abused on monitors
    * running at less than 60Hz because 1 is the default value - many users
    * wouldn't know how to change it to auto. */
   if (settings->uints.video_swap_interval > 1) {
      const char* error = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL);
      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg_hash_to_str_us(MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL));
      rcheevos_pause_hardcore();

      runloop_msg_queue_push(error, 0, 4 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      return;
   }

   /* this causes N blank frames to be rendered between real frames, thus
    * can slow down the actual number of rendered frames per second. */
   if (settings->uints.video_black_frame_insertion > 0) {
      const char* error = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION);
      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg_hash_to_str_us(MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION));
      rcheevos_pause_hardcore();

      runloop_msg_queue_push(error, 0, 4 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      return;
   }

   /* this causes N dupe frames to be rendered between real frames, for
      the purposes of shaders that update faster than content. Thus
    * can slow down the actual number of rendered frames per second. */
   if (settings->uints.video_shader_subframes > 1) {
      const char* error = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_PAUSED_SHADER_SUBFRAMES);
      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg_hash_to_str_us(MSG_CHEEVOS_HARDCORE_PAUSED_SHADER_SUBFRAMES));
      rcheevos_pause_hardcore();

      runloop_msg_queue_push(error, 0, 4 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      return;
   }

   if (!sysinfo->library_name)
      return;

   disallowed_settings = rc_libretro_get_disallowed_settings(sysinfo->library_name);
   if (disallowed_settings && retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
   {
      for (i = 0; i < (int)coreopts->size; i++)
      {
         const char* key = coreopts->opts[i].key;
         const char* val = core_option_manager_get_val(coreopts, i);
         if (!rc_libretro_is_setting_allowed(disallowed_settings, key, val))
         {
            char buffer[128];
            snprintf(buffer, sizeof(buffer),
                     msg_hash_to_str_us(MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED), key, val);
            CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", buffer);
            snprintf(buffer, sizeof(buffer),
                     msg_hash_to_str(MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED), key, val);
            rcheevos_pause_hardcore();

            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
            return;
         }
      }
   }

   {
      const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals.client);
      console_id = game ? game->console_id : 0;
   }

   if (console_id && !rc_libretro_is_system_allowed(sysinfo->library_name, console_id))
   {
      char buffer[256];
      snprintf(buffer, sizeof(buffer),
            msg_hash_to_str(MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE),
            rc_console_name(console_id), sysinfo->library_name);
      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", buffer);
      rcheevos_pause_hardcore();

      runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      return;
   }
}

/*****************************************************************************
Test all the achievements (call once per frame).
*****************************************************************************/
void rcheevos_test(void)
{
#ifdef HAVE_THREADS
   if (rcheevos_locals.queued_command != CMD_EVENT_NONE)
   {
      if ((int)rcheevos_locals.queued_command != CMD_CHEEVOS_NON_COMMAND)
         command_event(rcheevos_locals.queued_command, NULL);

      rcheevos_locals.queued_command = CMD_EVENT_NONE;

      if (rcheevos_locals.game_placard_requested)
      {
         rcheevos_locals.game_placard_requested = false;
         rcheevos_show_game_placard();
      }
   }
#endif

   if (rcheevos_locals.memory.count != 0)
      rc_client_do_frame(rcheevos_locals.client);
   else
      rc_client_idle(rcheevos_locals.client);
}

void rcheevos_idle(void)
{
   rc_client_idle(rcheevos_locals.client);
}

size_t rcheevos_get_serialize_size(void)
{
   return rc_client_progress_size(rcheevos_locals.client);
}

bool rcheevos_get_serialized_data(void* buffer)
{
   return (rc_client_serialize_progress(rcheevos_locals.client, (uint8_t*)buffer) == RC_OK);
}

bool rcheevos_set_serialized_data(void* buffer)
{
   if (rcheevos_is_game_loaded() && buffer)
   {
      const int result = rc_client_deserialize_progress(
         rcheevos_locals.client, (const uint8_t*)buffer);

      return (result == RC_OK);
   }

   return false;
}

void rcheevos_set_support_cheevos(bool state)
{
   rcheevos_locals.core_supports = state;
}

bool rcheevos_get_support_cheevos(void)
{
   return rcheevos_locals.core_supports;
}

const char* rcheevos_get_hash(void)
{
   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals.client);
   return game ? game->hash : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
}

/* hooks for rc_hash library */

static void* rc_hash_handle_file_open(const char* path)
{
   return intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void rc_hash_handle_file_seek(
      void* file_handle, int64_t offset, int origin)
{
   intfstream_seek((intfstream_t*)file_handle, offset, origin);
}

static int64_t rc_hash_handle_file_tell(void* file_handle)
{
   return intfstream_tell((intfstream_t*)file_handle);
}

static size_t rc_hash_handle_file_read(
      void* file_handle, void* buffer, size_t requested_bytes)
{
   return intfstream_read((intfstream_t*)file_handle,
         buffer, requested_bytes);
}

static void rc_hash_handle_file_close(void* file_handle)
{
   intfstream_close((intfstream_t*)file_handle);
   CHEEVOS_FREE(file_handle);
}

#ifdef HAVE_CHD
static void* rc_hash_handle_chd_open_track(
      const char* path, uint32_t track)
{
   cdfs_track_t* cdfs_track;

   switch (track)
   {
      case RC_HASH_CDTRACK_FIRST_DATA:
         cdfs_track = cdfs_open_data_track(path);
         break;

      case RC_HASH_CDTRACK_LAST:
         cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_LAST);
         break;

      case RC_HASH_CDTRACK_LARGEST:
         cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_PRIMARY);
         break;

      default:
         cdfs_track = cdfs_open_track(path, track);
         break;
   }

   if (cdfs_track)
   {
      cdfs_file_t* file = (cdfs_file_t*)malloc(sizeof(cdfs_file_t));
      if (cdfs_open_file(file, cdfs_track, NULL))
         return file; /* ASSERT: file owns cdfs_track now */

      CHEEVOS_FREE(file);
      cdfs_close_track(cdfs_track); /* ASSERT: this free()s cdfs_track */
   }

   return NULL;
}

static size_t rc_hash_handle_chd_read_sector(
      void* track_handle, uint32_t sector,
      void* buffer, size_t requested_bytes)
{
   cdfs_file_t* file = (cdfs_file_t*)track_handle;
   uint32_t track_sectors = cdfs_get_num_sectors(file);

   sector -= cdfs_get_first_sector(file);
   if (sector >= track_sectors)
      return 0;

   cdfs_seek_sector(file, sector);
   return cdfs_read_file(file, buffer, requested_bytes);
}

static uint32_t rc_hash_handle_chd_first_track_sector(
   void* track_handle)
{
   cdfs_file_t* file = (cdfs_file_t*)track_handle;
   return cdfs_get_first_sector(file);
}

static void rc_hash_handle_chd_close_track(void* track_handle)
{
   cdfs_file_t* file = (cdfs_file_t*)track_handle;
   if (file)
   {
      cdfs_close_track(file->track);
      cdfs_close_file(file); /* ASSERT: this does not free() file */
      CHEEVOS_FREE(file);
   }
}

#endif

static void rc_hash_reset_cdreader_hooks(void);

static void* rc_hash_handle_cd_open_track(
      const char* path, uint32_t track)
{
   struct rc_hash_filereader filereader;
   struct rc_hash_cdreader cdreader;

   memset(&filereader, 0, sizeof(filereader));
   filereader.open = rc_hash_handle_file_open;
   filereader.seek = rc_hash_handle_file_seek;
   filereader.tell = rc_hash_handle_file_tell;
   filereader.read = rc_hash_handle_file_read;
   filereader.close = rc_hash_handle_file_close;
   rc_hash_init_custom_filereader(&filereader);

   if (string_is_equal_noncase(path_get_extension(path), "chd"))
   {
#ifdef HAVE_CHD
      /* special handlers for CHD file */
      memset(&cdreader, 0, sizeof(cdreader));
      cdreader.open_track = rc_hash_handle_cd_open_track;
      cdreader.read_sector = rc_hash_handle_chd_read_sector;
      cdreader.close_track = rc_hash_handle_chd_close_track;
      cdreader.first_track_sector = rc_hash_handle_chd_first_track_sector;
      rc_hash_init_custom_cdreader(&cdreader);

      return rc_hash_handle_chd_open_track(path, track);
#else
      CHEEVOS_LOG(RCHEEVOS_TAG "Cannot generate hash from CHD without HAVE_CHD compile flag\n");
      return NULL;
#endif
   }
   else
   {
      /* not a CHD file, use the default handlers */
      rc_hash_get_default_cdreader(&cdreader);
      rc_hash_reset_cdreader_hooks();
      return cdreader.open_track(path, track);
   }
}

static void rc_hash_reset_cdreader_hooks(void)
{
   struct rc_hash_cdreader cdreader;
   rc_hash_get_default_cdreader(&cdreader);
   cdreader.open_track = rc_hash_handle_cd_open_track;
   rc_hash_init_custom_cdreader(&cdreader);
}

/* end hooks */

static void rcheevos_show_game_placard(void)
{
   size_t len;
   char msg[256];
   rc_client_user_game_summary_t summary;
   const settings_t* settings   = config_get_ptr();
   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals.client);
   if (!game) /* Make sure there's actually a game loaded */
      return;

   rc_client_get_user_game_summary(rcheevos_locals.client, &summary);

   if (summary.num_core_achievements == 0)
   {
      if (summary.num_unofficial_achievements == 0)
         len = snprintf(msg, sizeof(msg), "%s", msg_hash_to_str(MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS));
      else
         len = snprintf(msg, sizeof(msg),
                        msg_hash_to_str(MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED),
                        (int)summary.num_unofficial_achievements);
   }
   else if (rc_client_get_encore_mode_enabled(rcheevos_locals.client))
      len = snprintf(msg, sizeof(msg),
                     msg_hash_to_str(MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED),
                     (int)summary.num_core_achievements);
   else
      len = snprintf(msg, sizeof(msg),
                     msg_hash_to_str(MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED),
                     (int)summary.num_unlocked_achievements,
                     (int)summary.num_core_achievements);

   if (summary.num_unsupported_achievements)
   {
      if (len < sizeof(msg) - 4)
      {
         msg[len++] = ' ';
         msg[len++] = '(';

         len       += snprintf(&msg[len], sizeof(msg) - len,
               msg_hash_to_str(MSG_CHEEVOS_UNSUPPORTED_COUNT),
               (int)summary.num_unsupported_achievements);

         if (len < sizeof(msg) - 1)
         {
            msg[len++] = ')';
            msg[len]   = '\0';
         }
      }
   }

   msg[sizeof(msg) - 1] = 0;
   CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg);

   if (   settings->uints.cheevos_visibility_summary == RCHEEVOS_SUMMARY_ALLGAMES
      || (settings->uints.cheevos_visibility_summary == RCHEEVOS_SUMMARY_HASCHEEVOS
      && (summary.num_core_achievements || summary.num_unofficial_achievements)))
   {
#if defined (HAVE_GFX_WIDGETS)
      if (gfx_widgets_ready())
      {
         char badge_name[32];
         size_t _len = strlcpy(badge_name, "i", sizeof(badge_name));
         _len       += strlcpy(badge_name + _len, game->badge_name,
               sizeof(badge_name) - _len);
         gfx_widgets_push_achievement(game->title, msg, badge_name);
      }
      else
#endif
         runloop_msg_queue_push(msg, 0, 3 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static uint32_t rcheevos_client_read_memory(uint32_t address,
   uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
   return rc_libretro_memory_read(&rcheevos_locals.memory, address, buffer, num_bytes);
}

static uint32_t rcheevos_client_read_memory_dummy(uint32_t address,
   uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
   /* pretend the memory exists */
   memset(buffer, 0, num_bytes);
   return num_bytes;
}

static uint32_t rcheevos_client_read_memory_unavailable(uint32_t address,
   uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
   return 0;
}

static uint32_t rcheevos_client_read_memory_uninitialized(uint32_t address,
   uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
   /* we can't initialize the memory until we know which console the game
    * is associated to. This happens internally to the load game sequence,
    * so we have to intercept the first attempt to read memory.
    */
   if (rcheevos_init_memory(&rcheevos_locals))
   {
      rc_client_set_read_memory_function(client, rcheevos_client_read_memory);
      return rcheevos_client_read_memory(address, buffer, num_bytes, client);
   }

   /* some cores (like Mupen64-Plus) don't expose the memory until the
    * first call to retro_run. in that case, there will be a total_size
    * of memory reported by the core, but init will return false, as all
    * of the pointers were null. if we're still loading the game, return
    * dummy memory and we'll re-evaluate in rcheevos_client_load_game_callback().
    */
   if (!rcheevos_is_game_loaded())
   {
      rc_client_set_read_memory_function(client, rcheevos_client_read_memory_dummy);
      return rcheevos_client_read_memory_dummy(address, buffer, num_bytes, client);
   }

   /* game loaded, but no memory available */
   rc_client_set_read_memory_function(client, rcheevos_client_read_memory_unavailable);
   return rcheevos_client_read_memory_unavailable(address, buffer, num_bytes, client);
}

static void rcheevos_client_login_callback(int result,
   const char* error_message, rc_client_t* client, void* userdata)
{
   const rc_client_user_t* user;

   if (result != RC_OK)
   {
      char msg[256];
      size_t _len = strlcpy(msg, "RetroAchievements login failed: ",
            sizeof(msg));
      _len += strlcpy(msg + _len, error_message, sizeof(msg) - _len);
      CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg);
      runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return;
   }

   user = rc_client_get_user_info(client);
   if (!user)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Login failed without error\n");
   }
   else
   {
      settings_t* settings = config_get_ptr();

      if (user->token[0])
      {
         /* store the token, clear the password */
         strlcpy(settings->arrays.cheevos_token, user->token,
            sizeof(settings->arrays.cheevos_token));
         settings->arrays.cheevos_password[0] = '\0';
      }
      else
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Login did not return token\n");
      }

      /* show notification (if enabled) */
      if (settings->bools.cheevos_visibility_account)
      {
         char msg[128];
         snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_CHEEVOS_LOGGED_IN_AS_USER),
            user->display_name);
         runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }
}

static void rcheevos_finalize_game_load(rc_client_t* client)
{
   rcheevos_client_download_achievement_badges(client);

   if (!rc_client_is_processing_required(client))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "No runtime logic for game, pausing hardcore\n");
      rcheevos_pause_hardcore();
   }
}

static void rcheevos_client_load_game_callback(int result,
   const char* error_message, rc_client_t* client, void* userdata)
{
   const settings_t* settings = config_get_ptr();
   const rc_client_game_t* game = rc_client_get_game_info(client);
   char msg[256];

#if defined(HAVE_GFX_WIDGETS)
   gfx_widget_set_cheevos_set_loading(false);
#endif

   if (result != RC_OK || !game)
   {
      if (result == RC_NO_GAME_LOADED)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Game not recognized, pausing hardcore\n");
         rcheevos_pause_hardcore();

         if (!settings->bools.cheevos_verbose_enable)
            return;

         snprintf(msg, sizeof(msg), "%s", msg_hash_to_str(MSG_CHEEVOS_GAME_NOT_IDENTIFIED));
      }
      else
      {
         if (!error_message)
            error_message = "Unknown error";

         snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_CHEEVOS_GAME_LOAD_FAILED), error_message);
         CHEEVOS_LOG(RCHEEVOS_TAG "Game load failed: %s\n", error_message);
      }

      runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return;
   }

   if (rcheevos_locals.memory.total_size == 0)
   {
      /* make one last attempt to initialize memory */
      if (!rcheevos_init_memory(&rcheevos_locals))
      {
         rcheevos_locals.core_supports = false;

         CHEEVOS_ERR(RCHEEVOS_TAG "No memory exposed by core\n");

         if (settings && settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
               0, 4 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

         rcheevos_unload();
         rcheevos_pause_hardcore();
         return;
      }

      /* have valid memory now. use the real read function */
      rc_client_set_read_memory_function(client, rcheevos_client_read_memory);
   }

#ifdef HAVE_THREADS
   if (!video_driver_is_threaded() && !task_is_on_main_thread())
   {
      /* have to "schedule" this. game image should not be loaded on background thread */
      rcheevos_locals.queued_command = CMD_CHEEVOS_NON_COMMAND;
      rcheevos_locals.game_placard_requested = true;
   }
   else
#endif
      rcheevos_show_game_placard();

   rcheevos_finalize_game_load(client);

   if (rcheevos_hardcore_active())
   {
      /* hardcore is active. we're going to start processing
       * achievements. make sure restrictions are enforced */
      rcheevos_validate_config_settings();
      rcheevos_enforce_hardcore_settings();
   }
   else
   {
#if HAVE_REWIND
      /* Re-enable rewind. Additional space will be allocated for the achievement state data */
      if (settings->bools.rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* Have to "schedule" this. CMD_EVENT_REWIND_REINIT should
             * only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_REINIT;
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_REINIT, NULL);
      }
#endif
   }

   rcheevos_spectating_changed(); /* synchronize spectating state */
}

static rc_clock_t rcheevos_client_get_time_millisecs(const rc_client_t* client)
{
   return cpu_features_get_time_usec() / 1000;
}



bool rcheevos_load(const void *data)
{
   const struct retro_game_info *info = (const struct retro_game_info*)data;
   settings_t *settings               = config_get_ptr();
   bool cheevos_enable                = settings
      && settings->bools.cheevos_enable;

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
#endif

   /* If achievements are not enabled, or the core doesn't
    * support achievements, disable hardcore and bail */
   if (!cheevos_enable || !rcheevos_locals.core_supports || !data)
   {
      rcheevos_pause_hardcore();
      return false;
   }

   if (string_is_empty(settings->arrays.cheevos_username))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Cannot login (no username)\n");
      runloop_msg_queue_push("Missing RetroAchievements account information.", 0, 5 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      rcheevos_pause_hardcore();
      return false;
   }

   /* Refresh the user agent in case it's not set or has changed */
   rcheevos_get_user_agent(&rcheevos_locals,
      rcheevos_locals.user_agent_core,
      sizeof(rcheevos_locals.user_agent_core));

   if (rcheevos_locals.client)
   {
      rc_client_unload_game(rcheevos_locals.client);
   }
   else
   {
      rcheevos_locals.client = rc_client_create(rcheevos_client_read_memory, rcheevos_client_server_call);
      rc_client_enable_logging(rcheevos_locals.client, RC_CLIENT_LOG_LEVEL_VERBOSE, rcheevos_client_log_message);
      rc_client_set_event_handler(rcheevos_locals.client, rcheevos_client_event_handler);
      rc_client_set_get_time_millisecs_function(rcheevos_locals.client, rcheevos_client_get_time_millisecs);

      {
         const char* host = settings->arrays.cheevos_custom_host;
         if (!host[0])
         {
#ifdef HAVE_SSL
            host = "https://retroachievements.org";
#else
            host = "http://retroachievements.org";
#endif
         }

         rc_client_set_host(rcheevos_locals.client, host);
      }

      rcheevos_client_download_placeholder_badge();
   }

   rc_client_set_hardcore_enabled(rcheevos_locals.client, settings->bools.cheevos_hardcore_mode_enable);
   rc_client_set_unofficial_enabled(rcheevos_locals.client, settings->bools.cheevos_test_unofficial);
   rc_client_set_encore_mode_enabled(rcheevos_locals.client, settings->bools.cheevos_start_active);
   rc_client_set_spectator_mode_enabled(rcheevos_locals.client, !rcheevos_is_player_active());
   rc_client_set_read_memory_function(rcheevos_locals.client, rcheevos_client_read_memory_uninitialized);

   rcheevos_validate_config_settings();

   CHEEVOS_LOG(RCHEEVOS_TAG "Load started, hardcore %sactive\n", rcheevos_hardcore_active() ? "" : "not ");

   if (!rc_client_get_user_info(rcheevos_locals.client))
   {
      /* user not logged in, do so now */
      if (settings->arrays.cheevos_token[0])
      {
         rc_client_begin_login_with_token(rcheevos_locals.client,
            settings->arrays.cheevos_username, settings->arrays.cheevos_token,
            rcheevos_client_login_callback, NULL);
      }
      else
      {
         rc_client_begin_login_with_password(rcheevos_locals.client,
            settings->arrays.cheevos_username, settings->arrays.cheevos_password,
            rcheevos_client_login_callback, NULL);
      }
   }

   if (rcheevos_hardcore_active())
   {
      rcheevos_enforce_hardcore_settings();
   }

   /* provide hooks for reading files */
   rc_hash_reset_cdreader_hooks();

#if defined(HAVE_GFX_WIDGETS)
   if (settings->bools.cheevos_verbose_enable)
      gfx_widget_set_cheevos_set_loading(true);
#endif

   rc_client_begin_identify_and_load_game(rcheevos_locals.client, RC_CONSOLE_UNKNOWN,
      info->path, (const uint8_t*)info->data, info->size, rcheevos_client_load_game_callback, NULL);



   return true;
}

static void rcheevos_client_change_media_callback(int result,
   const char* error_message, rc_client_t* client, void* userdata)
{
   char msg[256];

   if (result == RC_OK || result == RC_NO_GAME_LOADED)
      return;

   if (result == RC_HARDCORE_DISABLED)
   {
      strlcpy(msg, error_message, sizeof(msg));
      rcheevos_hardcore_enabled_changed();
   }
   else
   {
      if (!error_message)
         error_message = "Unknown error";

      snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_CHEEVOS_CHANGE_MEDIA_FAILED), error_message);
      CHEEVOS_LOG(RCHEEVOS_TAG "Change media failed: %s\n", error_message);
   }

   runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL,
      MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

void rcheevos_change_disc(const char* new_disc_path, bool initial_disc)
{
   if (rcheevos_locals.client)
   {
      rc_client_begin_change_media(rcheevos_locals.client, new_disc_path,
         NULL, 0, rcheevos_client_change_media_callback, NULL);
   }
}
