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
#ifdef HAVE_RC_CLIENT
   NULL, /* client */
#else
   {0},  /* runtime */
   {0},  /* game */
#endif
   {{0}},/* memory */
#ifdef HAVE_THREADS
   CMD_EVENT_NONE, /* queued_command */
   false, /* game_placard_requested */
#endif
#ifndef HAVE_RC_CLIENT
   "",   /* displayname */
   "",   /* username */
   "",   /* token */
#endif
   "",   /* user_agent_prefix */
   "",   /* user_agent_core */
#ifdef HAVE_MENU
   NULL, /* menuitems */
   0,    /* menuitem_capacity */
   0,    /* menuitem_count */
#endif
#ifdef HAVE_RC_CLIENT
   true, /* hardcore_allowed */
   false,/* hardcore_being_enabled */
#else
 #ifdef HAVE_GFX_WIDGETS
   0,    /* active_lboard_trackers */
   NULL, /* tracker_achievement */
   0.0,  /* tracker_progress */
 #endif
   {RCHEEVOS_LOAD_STATE_NONE, 0, 0 },  /* load_info */
   0,    /* unpaused_frames */
   false,/* hardcore_active */
   false,/* loaded */
 #ifdef HAVE_GFX_WIDGETS
   false,/* assign_new_trackers */
 #endif
#endif
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

#ifndef HAVE_RC_CLIENT

static void rcheevos_achievement_disabled(
      rcheevos_racheevo_t* cheevo, unsigned address)
{
   if (!cheevo)
      return;

   CHEEVOS_ERR(RCHEEVOS_TAG
         "Achievement %u disabled (invalid address %06X): %s\n",
         cheevo->id, address, cheevo->title);
   CHEEVOS_FREE(cheevo->memaddr);
   cheevo->memaddr = NULL;
   cheevo->active |= RCHEEVOS_ACTIVE_UNSUPPORTED;
}

static void rcheevos_lboard_disabled(
      rcheevos_ralboard_t* lboard, unsigned address)
{
   if (!lboard)
      return;

   CHEEVOS_ERR(RCHEEVOS_TAG
         "Leaderboard %u disabled (invalid address %06X): %s\n",
         lboard->id, address, lboard->title);
   CHEEVOS_FREE(lboard->mem);
   lboard->mem = NULL;
}

#endif /* HAVE_RC_CLIENT */

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
#ifdef HAVE_RC_CLIENT
   const rc_client_game_t* game;
#endif
   rarch_system_info_t *sys_info               = &runloop_state_get_ptr()->system;
   rarch_memory_map_t *mmaps                   = &sys_info->mmaps;
   struct retro_memory_descriptor* descriptors;
   unsigned console_id;

#ifdef HAVE_RC_CLIENT
   /* if no game is loaded, fallback to a default mapping (SYSTEM RAM followed by SAVE RAM) */
   game = rc_client_get_game_info(locals->client);
   console_id = game ? game->console_id : 0;
#else
   console_id = locals->game.console_id;
#endif

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

#ifndef HAVE_RC_CLIENT

static uint32_t rcheevos_peek(uint32_t address,
      uint32_t num_bytes, void* ud)
{
   uint32_t avail;
   uint8_t* data = rc_libretro_memory_find_avail(
         &rcheevos_locals.memory, address, &avail);

   if (data && avail >= num_bytes)
   {
      switch (num_bytes)
      {
         case 4:
            return (data[3] << 24) | (data[2] << 16) |
                   (data[1] <<  8) | (data[0]);
         case 3:
            return (data[2] << 16) | (data[1] << 8) | (data[0]);
         case 2:
            return (data[1] << 8)  | (data[0]);
         case 1:
            return data[0];
      }
   }

   return 0;
}

static void rcheevos_activate_achievements(void)
{
   unsigned i;
   int result;
   rcheevos_racheevo_t* achievement = rcheevos_locals.game.achievements;
   settings_t* settings = config_get_ptr();
   const uint8_t active_flag = rcheevos_locals.hardcore_active ? RCHEEVOS_ACTIVE_HARDCORE : RCHEEVOS_ACTIVE_SOFTCORE;

   for (i = 0; i < rcheevos_locals.game.achievement_count;
         i++, achievement++)
   {
      if ((achievement->active & active_flag) != 0)
      {
         result = rc_runtime_activate_achievement(&rcheevos_locals.runtime, achievement->id, achievement->memaddr, NULL, 0);
         if (result != RC_OK)
         {
            char buffer[256];
            buffer[0] = '\0';
            /* TODO/FIXME - localize */
            snprintf(buffer, sizeof(buffer),
               "Could not activate achievement %u \"%s\": %s",
               achievement->id, achievement->title, rc_error_str(result));

            if (settings->bools.cheevos_verbose_enable)
               runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            CHEEVOS_ERR(RCHEEVOS_TAG "%s: mem %s\n", buffer, achievement->memaddr);
            achievement->active &= ~(RCHEEVOS_ACTIVE_HARDCORE | RCHEEVOS_ACTIVE_SOFTCORE);
            achievement->active |= RCHEEVOS_ACTIVE_UNSUPPORTED;

            CHEEVOS_FREE(achievement->memaddr);
            achievement->memaddr = NULL;
         }
      }
   }
}

static rcheevos_racheevo_t* rcheevos_find_cheevo(unsigned id)
{
   rcheevos_racheevo_t* cheevo = rcheevos_locals.game.achievements;
   rcheevos_racheevo_t* stop   = cheevo
      + rcheevos_locals.game.achievement_count;

   for(; cheevo < stop; ++cheevo)
   {
      if (cheevo->id == id)
         return cheevo;
   }

   return NULL;
}

#endif

static bool rcheevos_is_game_loaded(void)
{
#ifdef HAVE_RC_CLIENT
   return rc_client_is_game_loaded(rcheevos_locals.client);
#else
   return rcheevos_locals.loaded;
#endif
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
#ifdef HAVE_RC_CLIENT
   /* don't update spectator mode while a game is loading - it prevents being able to change it later */
   if (rcheevos_is_game_loaded())
   {
      const bool spectating = !rcheevos_is_player_active();
      if (spectating != rc_client_get_spectator_mode_enabled(rcheevos_locals.client))
         rc_client_set_spectator_mode_enabled(rcheevos_locals.client, !rcheevos_is_player_active());
   }
#endif
}

bool rcheevos_is_pause_allowed(void)
{
#ifdef HAVE_RC_CLIENT
   return rc_client_can_pause(rcheevos_locals.client, NULL);
#else
   return (rcheevos_locals.unpaused_frames == 0);
#endif
}

#ifdef HAVE_RC_CLIENT

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

#else /* !HAVE_RC_CLIENT */

void rcheevos_award_achievement(rcheevos_locals_t* locals,
      rcheevos_racheevo_t* cheevo, bool widgets_ready)
{
   const settings_t *settings = config_get_ptr();

   if (!cheevo)
      return;

   /* Deactivates the achievement. */
   rc_runtime_deactivate_achievement(&locals->runtime, cheevo->id);

   cheevo->active &= ~RCHEEVOS_ACTIVE_SOFTCORE;
   if (locals->hardcore_active)
      cheevo->active &= ~RCHEEVOS_ACTIVE_HARDCORE;

   cheevo->unlock_time = cpu_features_get_time_usec();

   if (!rcheevos_is_player_active())
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Not awarding achievement %u, player not active\n",
            cheevo->id);
      return;
   }

   CHEEVOS_LOG(RCHEEVOS_TAG "Awarding achievement %u: %s (%s)\n",
         cheevo->id, cheevo->title, cheevo->description);

   /* Show the on screen message. */
   if (settings->bools.cheevos_visibility_unlock)
   {
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_ready)
         gfx_widgets_push_achievement(msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED), cheevo->title, cheevo->badge);
      else
#endif
      {
         char buffer[256];
         size_t _len = strlcpy(buffer, msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED),
               sizeof(buffer));
         _len += strlcpy(buffer + _len, ": ", sizeof(buffer) - _len);
         _len += strlcpy(buffer + _len, cheevo->title, sizeof(buffer) - _len);
         runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         runloop_msg_queue_push(cheevo->description, 0, 3 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* Start the award task (unofficial achievement
    * unlocks are not submitted). */
   if (!(cheevo->active & RCHEEVOS_ACTIVE_UNOFFICIAL))
      rcheevos_client_award_achievement(cheevo->id);

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
      size_t shotname_len  = sizeof(char) * 8192;
      char *shotname       = (char*)malloc(shotname_len);

      if (shotname)
      {
         video_driver_state_t *video_st  = video_state_get_ptr();;
         snprintf(shotname, shotname_len, "%s/%s-cheevo-%u",
               settings->paths.directory_screenshot,
               path_basename(path_get(RARCH_PATH_BASENAME)),
               cheevo->id);
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

static rcheevos_ralboard_t* rcheevos_find_lboard(unsigned id)
{
   rcheevos_ralboard_t* lboard = rcheevos_locals.game.leaderboards;
   rcheevos_ralboard_t* stop   = lboard
      + rcheevos_locals.game.leaderboard_count;

   for (; lboard < stop; ++lboard)
   {
      if (lboard->id == id)
         return lboard;
   }

   return NULL;
}

#if defined(HAVE_GFX_WIDGETS)
static void rcheevos_assign_leaderboard_tracker_ids(rcheevos_locals_t* locals)
{
   rcheevos_ralboard_t* lboard = locals->game.leaderboards;
   rcheevos_ralboard_t* scan;
   rcheevos_ralboard_t* end = lboard + locals->game.leaderboard_count;
   unsigned tracker_id;
   char buffer[32];

   for (; lboard < end; ++lboard)
   {
      if (lboard->active_tracker_id != 0xFF)
         continue;

      tracker_id = 0;
      if (locals->active_lboard_trackers != 0 && lboard->value_hash != 0)
      {
         scan = locals->game.leaderboards;
         for (; scan < end; ++scan)
         {
            if (scan->active_tracker_id == 0 || scan->active_tracker_id == 0xFF)
               continue;

            /* value_hash match indicates the values have the same definition, but if the leaderboard
             * is tracking hits, it could have a different value depending on when it was started.
             * also require the current value to match. */
            if (scan->value_hash == lboard->value_hash && scan->value == lboard->value)
            {
               tracker_id = scan->active_tracker_id;
               break;
            }
         }
      }

      if (!tracker_id)
      {
         unsigned active_trackers = locals->active_lboard_trackers >> 1;
         tracker_id++;

         while ((active_trackers & 1) != 0)
         {
            tracker_id++;
            active_trackers >>= 1;
         }

         if (tracker_id <= 31)
         {
            locals->active_lboard_trackers |= (1 << tracker_id);

            rc_runtime_format_lboard_value(buffer,
               sizeof(buffer), lboard->value, lboard->format);
            gfx_widgets_set_leaderboard_display(tracker_id, buffer);
         }
      }

      lboard->active_tracker_id = tracker_id;
   }
}

static void rcheevos_hide_leaderboard_tracker(rcheevos_locals_t* locals,
      rcheevos_ralboard_t* lboard)
{
   unsigned i;

   uint8_t tracker_id = lboard->active_tracker_id;
   if (!tracker_id)
      return;

   lboard->active_tracker_id = 0;
   for (i = 0; i < locals->game.leaderboard_count; ++i)
   {
      if (locals->game.leaderboards[i].active_tracker_id == tracker_id)
         return;
   }

   if (tracker_id <= 31)
   {
      locals->active_lboard_trackers &= ~(1 << tracker_id);
      gfx_widgets_set_leaderboard_display(tracker_id, NULL);
   }
}
#endif

static void rcheevos_lboard_submit(rcheevos_locals_t* locals,
      rcheevos_ralboard_t* lboard, int value, bool widgets_ready)
{
   char buffer[256];
   char formatted_value[16];
   const settings_t *settings = config_get_ptr();

#if defined(HAVE_GFX_WIDGETS)
   /* Hide the tracker */
   if (gfx_widgets_ready())
      rcheevos_hide_leaderboard_tracker(locals, lboard);
#endif

   rc_runtime_format_lboard_value(formatted_value,
         sizeof(formatted_value),
         value, lboard->format);

   if (!rcheevos_is_player_active())
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Not submitting %s for leaderboard %u, player not active\n",
            formatted_value, lboard->id);
      return;
   }

   CHEEVOS_LOG(RCHEEVOS_TAG "Submitting %s for leaderboard %u\n",
         formatted_value, lboard->id);

   /* Show the on-screen message. */
   if (settings->bools.cheevos_visibility_lboard_submit)
   {
      snprintf(buffer, sizeof(buffer), msg_hash_to_str(MSG_LEADERBOARD_SUBMISSION),
            formatted_value, lboard->title);
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* Start the submit task */
   rcheevos_client_submit_lboard_entry(lboard->id, value);
}

static void rcheevos_lboard_canceled(rcheevos_ralboard_t * lboard,
      bool widgets_ready)
{
   const settings_t *settings = config_get_ptr();
   if (!lboard)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard %u canceled: %s\n",
         lboard->id, lboard->title);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_ready)
      rcheevos_hide_leaderboard_tracker(&rcheevos_locals, lboard);
#endif

   if (settings->bools.cheevos_visibility_lboard_cancel)
   {
      char buffer[256];
      size_t _len = strlcpy(buffer, msg_hash_to_str(MSG_LEADERBOARD_FAILED),
            sizeof(buffer));
      _len += strlcpy(buffer + _len, ": ",  sizeof(buffer) - _len);
      strlcpy(buffer + _len, lboard->title, sizeof(buffer) - _len);
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_lboard_started(
      rcheevos_ralboard_t * lboard, int value,
      bool widgets_ready)
{
   const settings_t *settings = config_get_ptr();
   char buffer[256];
   if (!lboard)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard %u started: %s\n",
         lboard->id, lboard->title);

   if (!rcheevos_is_player_active())
      return;

#if defined(HAVE_GFX_WIDGETS)
   lboard->value = value;

   if (settings->bools.cheevos_visibility_lboard_trackers)
   {
      /* mark the leaderboard as needing a tracker assigned so we can check for merging later */
      lboard->active_tracker_id = 0xFF;
      rcheevos_locals.assign_new_trackers = true;
   }
#endif

   if (settings->bools.cheevos_visibility_lboard_start)
   {
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
static void rcheevos_lboard_updated(
      rcheevos_ralboard_t* lboard, int value,
      bool widgets_ready)
{
   const settings_t *settings = config_get_ptr();
   if (!lboard)
      return;

   lboard->value = value;

   if (widgets_ready && settings->bools.cheevos_visibility_lboard_trackers &&
      lboard->active_tracker_id && lboard->active_tracker_id <= 31)
   {
      char buffer[32];
      rc_runtime_format_lboard_value(buffer,
            sizeof(buffer), value, lboard->format);
      gfx_widgets_set_leaderboard_display(lboard->active_tracker_id,
         rcheevos_is_player_active() ? buffer : NULL);
   }
}

static void rcheevos_challenge_started(
      rcheevos_racheevo_t* cheevo, int value,
      bool widgets_ready)
{
   settings_t* settings = config_get_ptr();
   if (     cheevo
         && widgets_ready
         && settings->bools.cheevos_challenge_indicators
         && rcheevos_is_player_active())
      gfx_widgets_set_challenge_display(cheevo->id, cheevo->badge);
}

static void rcheevos_challenge_ended(
      rcheevos_racheevo_t* cheevo, int value,
      bool widgets_ready)
{
   if (cheevo && widgets_ready)
      gfx_widgets_set_challenge_display(cheevo->id, NULL);
}

static void rcheevos_progress_updated(rcheevos_locals_t* locals,
      rcheevos_racheevo_t* cheevo, int value,
      bool widgets_ready)
{
   settings_t* settings = config_get_ptr();

   if (     cheevo
         && widgets_ready
         && settings->bools.cheevos_visibility_progress_tracker
         && rcheevos_is_player_active())
   {
      unsigned measured_value, measured_target;
      if (rc_runtime_get_achievement_measured(&locals->runtime, cheevo->id, &measured_value, &measured_target))
      {
         const float progress = ((float)measured_value / (float)measured_target);
         if (progress > locals->tracker_progress)
         {
            locals->tracker_progress = progress;
            locals->tracker_achievement = cheevo;
         }
      }
   }
}

#endif

int rcheevos_get_richpresence(char *s, size_t len)
{
   if (rcheevos_is_player_active())
   {
      int ret = rc_runtime_get_richpresence(
            &rcheevos_locals.runtime, s, (unsigned)len,
            &rcheevos_peek, NULL, NULL);

      if (ret <= 0 && rcheevos_locals.game.title)
      {
         /* TODO/FIXME - localize */
         size_t _len = strlcpy(s, "Playing ", len);
         strlcpy(s + _len, rcheevos_locals.game.title, len - _len);
      }
      return ret;
   }
   if (rcheevos_locals.game.title)
   {
      /* TODO/FIXME - localize */
      size_t _len = strlcpy(s, "Spectating ", len);
      return (int)strlcpy(s + _len, rcheevos_locals.game.title, len - _len);
   }
   return 0;
}

#if defined(HAVE_GFX_WIDGETS)
static void rcheevos_hide_leaderboard_trackers(void)
{
   unsigned i = 0;
   unsigned trackers = rcheevos_locals.active_lboard_trackers;
   if (trackers == 0)
      return;

   do
   {
      if ((trackers & 1) != 0)
         gfx_widgets_set_leaderboard_display(i, NULL);

      i++;
      trackers >>= 1;
   } while (trackers != 0);

   for (i = 0; i < rcheevos_locals.game.leaderboard_count; i++)
      rcheevos_locals.game.leaderboards[i].active_tracker_id = 0;
}
#endif

#endif /* HAVE_RC_CLIENT */

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

#ifdef HAVE_RC_CLIENT
   rc_client_reset(rcheevos_locals.client);
#else
   rc_runtime_reset(&rcheevos_locals.runtime);
#endif

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
#ifdef HAVE_RC_CLIENT
   /* normal hardcore check */
   if (rcheevos_locals.client && rc_client_get_hardcore_enabled(rcheevos_locals.client))
      return true;

   /* if we're trying to enable hardcore, pretend it's on so the caller can decide to disable
    * it (by calling rcheevos_pause_hardcore) before we actually turn it on. */
   return rcheevos_locals.hardcore_being_enabled;
#else
   return rcheevos_locals.hardcore_active;
#endif
}

void rcheevos_pause_hardcore(void)
{
#ifdef HAVE_RC_CLIENT
   rcheevos_locals.hardcore_allowed = false;
#endif

   if (rcheevos_hardcore_active())
      rcheevos_toggle_hardcore_paused();
}

#if defined(HAVE_THREADS) && !defined(HAVE_RC_CLIENT)
static bool rcheevos_timer_check(void* userdata)
{
   retro_time_t stop_time = *(retro_time_t*)userdata;
   retro_time_t now       = cpu_features_get_time_usec();
   return (now < stop_time);
}
#endif

bool rcheevos_unload(void)
{
   settings_t* settings  = config_get_ptr();
   const bool was_loaded = rcheevos_is_game_loaded();

#ifdef HAVE_GFX_WIDGETS
   rcheevos_hide_widgets(gfx_widgets_ready());
   gfx_widget_set_cheevos_set_loading(false);
#endif

#ifdef HAVE_RC_CLIENT
   rc_client_unload_game(rcheevos_locals.client);
#else
   /* Immediately mark the game as unloaded
      so the ping thread will terminate normally */
   rcheevos_locals.game.id         = -1;
   rcheevos_locals.game.console_id = 0;
   rcheevos_locals.game.hash       = NULL;

 #ifdef HAVE_THREADS
   if (rcheevos_locals.load_info.state < RCHEEVOS_LOAD_STATE_DONE &&
       rcheevos_locals.load_info.state != RCHEEVOS_LOAD_STATE_NONE)
   {
      /* allow up to 5 seconds for pending tasks to run */
      retro_time_t stop_time = cpu_features_get_time_usec() + 5000000;

      rcheevos_locals.load_info.state = RCHEEVOS_LOAD_STATE_ABORTED;
      CHEEVOS_LOG(RCHEEVOS_TAG "Asked the load tasks to terminate\n");

      /* Wait for pending tasks to run */
      task_queue_wait(rcheevos_timer_check, &stop_time);
      /* Clean up after completed tasks */
      task_queue_check();
   }
 #endif
#endif

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
   rcheevos_locals.game_placard_requested = false;
#endif

   if (rcheevos_locals.memory.count > 0)
      rc_libretro_memory_destroy(&rcheevos_locals.memory);

   if (was_loaded)
   {
#ifndef HAVE_RC_CLIENT
      unsigned count = 0;
#endif

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

#ifndef HAVE_RC_CLIENT
      count = rcheevos_locals.game.achievement_count;
      rcheevos_locals.game.achievement_count = 0;
      if (rcheevos_locals.game.achievements)
      {
         rcheevos_racheevo_t* achievement = rcheevos_locals.game.achievements;
         rcheevos_racheevo_t* end = achievement + count;
         while (achievement < end)
         {
            CHEEVOS_FREE(achievement->title);
            CHEEVOS_FREE(achievement->description);
            CHEEVOS_FREE(achievement->badge);
            CHEEVOS_FREE(achievement->memaddr);

            ++achievement;
         }

         CHEEVOS_FREE(rcheevos_locals.game.achievements);
         rcheevos_locals.game.achievements      = NULL;
      }

      count = rcheevos_locals.game.leaderboard_count;
      rcheevos_locals.game.leaderboard_count = 0;
      if (rcheevos_locals.game.leaderboards)
      {
         rcheevos_ralboard_t* lboard = rcheevos_locals.game.leaderboards;
         rcheevos_ralboard_t* end = lboard + count;
         while (lboard < end)
         {
            CHEEVOS_FREE(lboard->title);
            CHEEVOS_FREE(lboard->description);
            CHEEVOS_FREE(lboard->mem);

            ++lboard;
         }

         CHEEVOS_FREE(rcheevos_locals.game.leaderboards);
         rcheevos_locals.game.leaderboards      = NULL;
      }

      if (rcheevos_locals.game.title)
      {
         CHEEVOS_FREE(rcheevos_locals.game.title);
         rcheevos_locals.game.title             = NULL;
      }

      rcheevos_locals.loaded                    = false;
      rcheevos_locals.hardcore_active           = false;

      rc_libretro_hash_set_destroy(&rcheevos_locals.game.hashes);
#endif
   }

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
#endif

   if (!settings->arrays.cheevos_token[0])
   {
#ifdef HAVE_RC_CLIENT
      /* If the config-level token has been cleared, we need to re-login on
       * loading the next game. Easiest way to do that is to destroy the client */
      rc_client_t* client = rcheevos_locals.client;
      rcheevos_locals.client = NULL;

      rc_client_destroy(client);
#else
      /* If the config-level token has been cleared,
       * we need to re-login on loading the next game */
      rcheevos_locals.token[0] = '\0';
#endif
   }

#ifndef HAVE_RC_CLIENT
   rc_runtime_destroy(&rcheevos_locals.runtime);
   rcheevos_locals.load_info.state = RCHEEVOS_LOAD_STATE_NONE;
#endif

   return true;
}

#ifndef HAVE_RC_CLIENT

static void rcheevos_toggle_hardcore_achievements(
      rcheevos_locals_t *locals)
{
   const unsigned active_mask  =
      RCHEEVOS_ACTIVE_SOFTCORE | RCHEEVOS_ACTIVE_HARDCORE | RCHEEVOS_ACTIVE_UNSUPPORTED;
   rcheevos_racheevo_t* cheevo = locals->game.achievements;
   rcheevos_racheevo_t* stop   = cheevo + locals->game.achievement_count;

   while (cheevo < stop)
   {
      if ((cheevo->active & active_mask) == RCHEEVOS_ACTIVE_HARDCORE)
      {
         /* player has unlocked achievement in non-hardcore,
          * but has not unlocked in hardcore. Toggle state */
         if (locals->hardcore_active)
         {
            rc_runtime_activate_achievement(&locals->runtime, cheevo->id, cheevo->memaddr, NULL, 0);
            CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u activated: %s\n", cheevo->id, cheevo->title);
         }
         else
         {
            rc_runtime_deactivate_achievement(&locals->runtime, cheevo->id);
            CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u deactivated: %s\n", cheevo->id, cheevo->title);
         }
      }

      ++cheevo;
   }
}

static void rcheevos_activate_leaderboards(void)
{
   unsigned i;
   int result;
   rcheevos_ralboard_t* leaderboard = rcheevos_locals.game.leaderboards;
   const settings_t *settings       = config_get_ptr();

   for (i = 0; i < rcheevos_locals.game.leaderboard_count;
         ++i, ++leaderboard)
   {
      if (!leaderboard->mem)
         continue;

      result = rc_runtime_activate_lboard(
            &rcheevos_locals.runtime, leaderboard->id,
            leaderboard->mem, NULL, 0);
      if (result != RC_OK)
      {
         char buffer[256];
         buffer[0] = '\0';
         /* TODO/FIXME - localize */
         snprintf(buffer, sizeof(buffer),
            "Could not activate leaderboard %u \"%s\": %s",
            leaderboard->id, leaderboard->title, rc_error_str(result));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s: mem %s\n", buffer, leaderboard->mem);

         CHEEVOS_FREE(leaderboard->mem);
         leaderboard->mem = NULL;
      }
   }
}

static void rcheevos_deactivate_leaderboards(void)
{
   rcheevos_ralboard_t* lboard = rcheevos_locals.game.leaderboards;
   rcheevos_ralboard_t* stop   = lboard +
      rcheevos_locals.game.leaderboard_count;

#if defined(HAVE_GFX_WIDGETS)
   /* Hide any visible trackers */
   rcheevos_hide_leaderboard_trackers();
#endif

   for (; lboard < stop; ++lboard)
   {
      if (lboard->mem)
      {
         rc_runtime_deactivate_lboard(&rcheevos_locals.runtime,
               lboard->id);
      }
   }
}

void rcheevos_leaderboard_trackers_visibility_changed(void)
{
   const settings_t* settings           = config_get_ptr();

   if (rcheevos_locals.loaded)
   {
#if defined(HAVE_GFX_WIDGETS)
      if (!settings->bools.cheevos_visibility_lboard_trackers)
      {
         /* Hide any visible trackers */
         rcheevos_hide_leaderboard_trackers();
      }
      else
      {
         unsigned i;
         rc_runtime_lboard_t* lboard = rcheevos_locals.runtime.lboards;
         for (i = 0; i < rcheevos_locals.runtime.lboard_count; ++i, ++lboard)
         {
            if (!lboard->lboard)
               continue;

            if (lboard->lboard->state == RC_LBOARD_STATE_STARTED)
            {
               rcheevos_ralboard_t* ralboard = rcheevos_find_lboard(lboard->id);
               if (ralboard && !ralboard->active_tracker_id)
               {
                  /* mark the leaderboard as needing a tracker assigned so we can check for merging later */
                  ralboard->active_tracker_id = 0xFF;
                  rcheevos_locals.assign_new_trackers = true;
               }
            }
            else
            {
               rcheevos_ralboard_t* ralboard = rcheevos_find_lboard(lboard->id);
               if (ralboard && ralboard->active_tracker_id)
                  rcheevos_hide_leaderboard_tracker(&rcheevos_locals, ralboard);
            }
         }
      }
#endif
   }
}

#else /* HAVE_RC_CLIENT */

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

#endif /* HAVE_RC_CLIENT */

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
#ifdef HAVE_RC_CLIENT
      locals->hardcore_being_enabled = true;
      locals->hardcore_allowed = true;
#else
      /* Activate hardcore */
      locals->hardcore_active = true;
#endif

      /* If one or more invalid settings is enabled, abort*/
      rcheevos_validate_config_settings();
#ifdef HAVE_RC_CLIENT
      if (!locals->hardcore_allowed)
      {
         locals->hardcore_being_enabled = false;
         return;
      }
#else
      if (!locals->hardcore_active)
         return;
#endif

#ifdef HAVE_CHEATS
      /* If one or more emulator managed cheats is active, abort */
      cheat_manager_apply_cheats();
 #ifdef HAVE_RC_CLIENT
      if (!locals->hardcore_allowed)
      {
         locals->hardcore_being_enabled = false;
         return;
      }
#else
      if (!locals->hardcore_active)
         return;
 #endif
#endif

      if (rcheevos_is_game_loaded())
      {
         const char* msg = msg_hash_to_str(
               MSG_CHEEVOS_HARDCORE_MODE_ENABLE);
         CHEEVOS_LOG("%s\n", msg);
         runloop_msg_queue_push(msg, 0, 3 * 60, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         rcheevos_enforce_hardcore_settings();

#ifndef HAVE_RC_CLIENT
         /* Reactivate leaderboards */
         rcheevos_activate_leaderboards();

         /* reset the game */
         command_event(CMD_EVENT_RESET, NULL);
#endif
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

#ifdef HAVE_RC_CLIENT
      locals->hardcore_being_enabled = false;
      rc_client_set_hardcore_enabled(locals->client, 1);
#endif
   }
   else
   {
      /* pause hardcore */
#ifdef HAVE_RC_CLIENT
      rc_client_set_hardcore_enabled(locals->client, 0);
#else
      locals->hardcore_active = false;

      if (locals->loaded)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Hardcore paused\n");

         /* deactivate leaderboards */
         rcheevos_deactivate_leaderboards();
      }
#endif

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

#ifndef HAVE_RC_CLIENT
   if (locals->loaded)
      rcheevos_toggle_hardcore_achievements(locals);
#endif
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

#ifdef HAVE_RC_CLIENT
   {
      const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals.client);
      console_id = game ? game->console_id : 0;
   }
#else
   console_id = rcheevos_locals.game.console_id;
#endif

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

#ifndef HAVE_RC_CLIENT

static void rcheevos_runtime_event_handler(
      const rc_runtime_event_t* runtime_event)
{
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_ready = gfx_widgets_ready();
#else
   bool widgets_ready = false;
#endif

   switch (runtime_event->type)
   {
#if defined(HAVE_GFX_WIDGETS)
      case RC_RUNTIME_EVENT_LBOARD_UPDATED:
         rcheevos_lboard_updated(
               rcheevos_find_lboard(runtime_event->id),
               runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED:
         rcheevos_challenge_started(
               rcheevos_find_cheevo(runtime_event->id),
               runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED:
         rcheevos_challenge_ended(
               rcheevos_find_cheevo(runtime_event->id),
               runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_PROGRESS_UPDATED:
         rcheevos_progress_updated(&rcheevos_locals,
               rcheevos_find_cheevo(runtime_event->id),
               runtime_event->value, widgets_ready);
         break;
#endif

      case RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED:
         rcheevos_award_achievement(
               &rcheevos_locals,
               rcheevos_find_cheevo(runtime_event->id), widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_STARTED:
         rcheevos_lboard_started(
               rcheevos_find_lboard(runtime_event->id),
               runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_CANCELED:
         rcheevos_lboard_canceled(
               rcheevos_find_lboard(runtime_event->id),
               widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_TRIGGERED:
         rcheevos_lboard_submit(
               &rcheevos_locals,
               rcheevos_find_lboard(runtime_event->id),
               runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED:
         rcheevos_achievement_disabled(
               rcheevos_find_cheevo(runtime_event->id),
               runtime_event->value);
         break;

      case RC_RUNTIME_EVENT_LBOARD_DISABLED:
         rcheevos_lboard_disabled(
               rcheevos_find_lboard(runtime_event->id),
               runtime_event->value);
         break;

      default:
         break;
   }
}

static int rcheevos_runtime_address_validator(uint32_t address)
{
   return rc_libretro_memory_find(
            &rcheevos_locals.memory, address) != NULL;
}

static void rcheevos_validate_memrefs(rcheevos_locals_t* locals)
{
   if (!rcheevos_init_memory(locals))
   {
      const settings_t* settings = config_get_ptr();
      /* some cores (like Mupen64-Plus) don't expose the memory until the
       * first call to retro_run. in that case, there will be a total_size
       * of memory reported by the core, but init will return false, as
       * all of the pointers were null. if we're still loading the game,
       * just reset the memory count and we'll re-evaluate in
       * rcheevos_test()
       */
      if (!locals->loaded)
      {
         /* If no memory was exposed, report the error now
          * instead of waiting */
         if (locals->memory.total_size != 0)
         {
            locals->memory.count = 0;
            return;
         }
      }

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

   rc_runtime_validate_addresses(&locals->runtime,
         rcheevos_runtime_event_handler,
         rcheevos_runtime_address_validator);
}

#endif /* HAVE_RC_CLIENT */

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

#ifdef HAVE_RC_CLIENT
   if (rcheevos_locals.memory.count != 0)
      rc_client_do_frame(rcheevos_locals.client);
   else
      rc_client_idle(rcheevos_locals.client);
#else
   if (!rcheevos_locals.loaded)
      return;

   /* We were unable to initialize memory earlier, try now */
   if (rcheevos_locals.memory.count == 0)
   {
      rcheevos_validate_memrefs(&rcheevos_locals);

      /* rcheevos_validate_memrefs may decide the core doesn't support achievements and
       * disable them. if so, bail. */
      if (!rcheevos_locals.loaded)
         return;
   }

   rc_runtime_do_frame(&rcheevos_locals.runtime,
         &rcheevos_runtime_event_handler, rcheevos_peek, NULL, 0);

 #ifdef HAVE_GFX_WIDGETS
   if (rcheevos_locals.assign_new_trackers)
   {
      if (gfx_widgets_ready())
         rcheevos_assign_leaderboard_tracker_ids(&rcheevos_locals);

      rcheevos_locals.assign_new_trackers = false;
   }

   if (rcheevos_locals.tracker_achievement != NULL)
   {
      char buffer[32] = "";
      if (rc_runtime_format_achievement_measured(&rcheevos_locals.runtime,
            rcheevos_locals.tracker_achievement->id, buffer, sizeof(buffer)))
      {
         gfx_widget_set_achievement_progress(rcheevos_locals.tracker_achievement->badge, buffer);
      }

      rcheevos_locals.tracker_achievement = NULL;
      rcheevos_locals.tracker_progress = 0.0;
   }
 #endif

   /* We processed a frame - if there's a pause delay in effect, process it */
   if (rcheevos_locals.unpaused_frames > 0)
      rcheevos_locals.unpaused_frames--;

#endif /* HAVE_RC_CLIENT */
}

void rcheevos_idle(void)
{
#ifdef HAVE_RC_CLIENT
   rc_client_idle(rcheevos_locals.client);
#endif
}

size_t rcheevos_get_serialize_size(void)
{
#ifdef HAVE_RC_CLIENT
   return rc_client_progress_size(rcheevos_locals.client);
#else
   if (!rcheevos_locals.loaded)
      return 0;
   return rc_runtime_progress_size(&rcheevos_locals.runtime, NULL);
#endif
}

bool rcheevos_get_serialized_data(void* buffer)
{
#ifdef HAVE_RC_CLIENT
   return (rc_client_serialize_progress(rcheevos_locals.client, (uint8_t*)buffer) == RC_OK);
#else
   if (!rcheevos_locals.loaded)
      return false;
   return (rc_runtime_serialize_progress(
            buffer, &rcheevos_locals.runtime, NULL) == RC_OK);
#endif
}

bool rcheevos_set_serialized_data(void* buffer)
{
   if (rcheevos_is_game_loaded() && buffer)
   {
#ifdef HAVE_RC_CLIENT
      const int result = rc_client_deserialize_progress(
         rcheevos_locals.client, (const uint8_t*)buffer);
#else
      const int result = rc_runtime_deserialize_progress(
         &rcheevos_locals.runtime, (const unsigned char*)buffer, NULL);

 #if defined(HAVE_GFX_WIDGETS)
      if (gfx_widgets_ready() && rcheevos_is_player_active())
      {
         settings_t* settings = config_get_ptr();

         if (settings->bools.cheevos_visibility_lboard_trackers)
            rcheevos_leaderboard_trackers_visibility_changed();

         if (settings->bools.cheevos_challenge_indicators)
         {
            unsigned i;
            rc_runtime_trigger_t* cheevo = rcheevos_locals.runtime.triggers;
            for (i = 0; i < rcheevos_locals.runtime.trigger_count; ++i, ++cheevo)
            {
               if (!cheevo->trigger)
                  continue;

               if (cheevo->trigger->state == RC_TRIGGER_STATE_PRIMED)
               {
                  rcheevos_racheevo_t* racheevo = rcheevos_find_cheevo(cheevo->id);
                  if (racheevo != NULL)
                     gfx_widgets_set_challenge_display(racheevo->id, racheevo->badge);
               }
               else
               {
                  gfx_widgets_set_challenge_display(cheevo->id, NULL);
               }
            }
         }

         if (settings->bools.cheevos_visibility_progress_tracker)
            gfx_widget_set_achievement_progress(NULL, NULL);
      }
 #endif
#endif /* HAVE_RC_CLIENT */

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
#ifdef HAVE_RC_CLIENT
   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals.client);
   return game ? game->hash : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
#else
   return (rcheevos_locals.game.hash != NULL) ?
      rcheevos_locals.game.hash :
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
#endif
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

#ifdef HAVE_RC_CLIENT

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

#else /* !HAVE_RC_CLIENT */

void rcheevos_show_mastery_placard(void)
{
   char title[256];
   const settings_t* settings = config_get_ptr();

   if (rcheevos_locals.game.mastery_placard_shown)
      return;

   rcheevos_locals.game.mastery_placard_shown = true;

   snprintf(title, sizeof(title),
      msg_hash_to_str(rcheevos_locals.hardcore_active
         ? MSG_CHEEVOS_MASTERED_GAME
         : MSG_CHEEVOS_COMPLETED_GAME),
      rcheevos_locals.game.title);
   title[sizeof(title) - 1] = '\0';
   CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", title);

   if (settings->bools.cheevos_visibility_mastery)
   {
#if defined (HAVE_GFX_WIDGETS)
      if (gfx_widgets_ready())
      {
         char msg[128];
         const bool content_runtime_log      = settings->bools.content_runtime_log;
         const bool content_runtime_log_aggr = settings->bools.content_runtime_log_aggregate;
         size_t len = strlcpy(msg, rcheevos_locals.displayname, sizeof(msg));

         if (len < sizeof(msg) - 12 &&
            (content_runtime_log || content_runtime_log_aggr))
         {
            const char* content_path   = path_get(RARCH_PATH_CONTENT);
            const char* core_path      = path_get(RARCH_PATH_CORE);
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

               len += snprintf(msg + len, sizeof(msg) - len, " | ");
               runtime_log_get_runtime_str(runtime_log, msg + len, sizeof(msg) - len);
               msg[sizeof(msg) - 1] = '\0';

               free(runtime_log);
            }
         }

         gfx_widgets_push_achievement(title, msg, rcheevos_locals.game.badge_name);
      }
      else
#endif
         runloop_msg_queue_push(title, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_show_game_placard(void)
{
   char msg[256];
   const settings_t* settings        = config_get_ptr();
   const rcheevos_racheevo_t* cheevo = rcheevos_locals.game.achievements;
   const rcheevos_racheevo_t* end    = cheevo
      + rcheevos_locals.game.achievement_count;
   int number_of_active              = 0;
   int number_of_unsupported         = 0;
   int number_of_core                = 0;
   int mode                          = RCHEEVOS_ACTIVE_SOFTCORE;

   if (rcheevos_locals.game.id < 0) /* make sure there's actually a game loaded */
      return;

   if (rcheevos_locals.hardcore_active)
      mode = RCHEEVOS_ACTIVE_HARDCORE;

   for (; cheevo < end; cheevo++)
   {
      if (cheevo->active & RCHEEVOS_ACTIVE_UNOFFICIAL)
         continue;

      number_of_core++;
      if (cheevo->active & RCHEEVOS_ACTIVE_UNSUPPORTED)
         number_of_unsupported++;
      else if (cheevo->active & mode)
         number_of_active++;
   }

   /* TODO/FIXME - localize strings */
   if (number_of_core == 0)
      strlcpy(msg, "This game has no achievements.", sizeof(msg));
   else if (!number_of_unsupported)
   {
      if (settings->bools.cheevos_start_active)
         snprintf(msg, sizeof(msg),
            "All %d achievements activated for this session.",
            number_of_core);
      else
         snprintf(msg, sizeof(msg),
            "You have %d of %d achievements unlocked.",
            number_of_core - number_of_active, number_of_core);
   }
   else
   {
      if (settings->bools.cheevos_start_active)
         snprintf(msg, sizeof(msg),
            "All %d achievements activated for this session (%d unsupported).",
            number_of_core, number_of_unsupported);
      else
         snprintf(msg, sizeof(msg),
            "You have %d of %d achievements unlocked (%d unsupported).",
            number_of_core - number_of_active - number_of_unsupported,
            number_of_core, number_of_unsupported);
   }

   msg[sizeof(msg) - 1] = 0;
   CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg);

   if (settings->uints.cheevos_visibility_summary == RCHEEVOS_SUMMARY_ALLGAMES ||
       (number_of_core > 0 && settings->uints.cheevos_visibility_summary == RCHEEVOS_SUMMARY_HASCHEEVOS))
   {
#if defined (HAVE_GFX_WIDGETS)
      if (gfx_widgets_ready())
         gfx_widgets_push_achievement(rcheevos_locals.game.title, msg, rcheevos_locals.game.badge_name);
      else
#endif
         runloop_msg_queue_push(msg, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_end_load(void)
{
#ifdef HAVE_GFX_WIDGETS
   rcheevos_ralboard_t* lboard = rcheevos_locals.game.leaderboards;
   rcheevos_ralboard_t* stop = lboard + rcheevos_locals.game.leaderboard_count;
   const char* ptr;
   unsigned hash;

   for (; lboard < stop; ++lboard)
   {
      lboard->value_hash = 0;
      lboard->active_tracker_id = 0;

      ptr = lboard->mem;
      if (!ptr)
         continue;

      ptr = strstr(ptr, "VAL:");
      if (!ptr)
         continue;
      ptr += 4;

      /* calculate the DJB2 hash of the VAL portion of the string*/
      hash = 5381;
      while (*ptr && (ptr[0] != ':' || ptr[1] != ':'))
         hash = (hash << 5) + hash + *ptr++;

      lboard->value_hash = hash;
   }
#endif

   CHEEVOS_LOG(RCHEEVOS_TAG "Load finished\n");
   rcheevos_locals.load_info.state = RCHEEVOS_LOAD_STATE_DONE;
}

static void rcheevos_fetch_badges_callback(void* userdata)
{
   rcheevos_end_load();
}

static void rcheevos_fetch_badges(void)
{
   /* this function manages the
    * RCHEEVOS_LOAD_STATE_FETCHING_BADGES state */
   rcheevos_client_fetch_badges(rcheevos_fetch_badges_callback, NULL);
}

static void rcheevos_start_session_async(retro_task_t* task)
{
   const bool needs_runtime =
      (  rcheevos_locals.game.achievement_count > 0
      || rcheevos_locals.game.leaderboard_count > 0
      || rcheevos_locals.runtime.richpresence);

   if (rcheevos_load_aborted())
      return;

   /* We don't have to wait for this to complete
    * to proceed to the next loading state */
   rcheevos_client_start_session(rcheevos_locals.game.id);

   rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_STARTING_SESSION);

   if (needs_runtime)
   {
      /* activate the achievements and leaderboards
       * (rich presence has already been activated) */
      rcheevos_activate_achievements();

      if (rcheevos_locals.hardcore_active)
         rcheevos_activate_leaderboards();

      /* disable any unsupported achievements */
      rcheevos_validate_memrefs(&rcheevos_locals);

      /* Let the runtime start processing the achievements */
      rcheevos_locals.loaded = true;
   }

#if HAVE_REWIND
   if (!rcheevos_locals.hardcore_active)
   {
      /* Re-enable rewind. If rcheevos_locals.loaded is true,
       * additional space will be allocated for the achievement
       * state data */
      const settings_t* settings = config_get_ptr();
      if (settings->bools.rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* Have to "schedule" this. CMD_EVENT_REWIND_INIT should
             * only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_INIT;
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_INIT, NULL);
      }
   }
#endif

   /* If there's nothing for the runtime to process,
    * disable hardcore. */
   if (!needs_runtime)
      rcheevos_pause_hardcore();
   /* hardcore is active. we're going to start processing
    * achievements. make sure restrictions are enforced */
   else if (rcheevos_locals.hardcore_active)
      runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_SLOWMOTION;

   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);

   if (rcheevos_end_load_state() == 0)
      rcheevos_fetch_badges();
}

static void rcheevos_start_session_finish(retro_task_t* task, void* data, void* userdata, const char* error)
{
   (void)task;
   (void)data;
   (void)userdata;
   (void)error;

   /* this must be called on the main thread */
   rcheevos_show_game_placard();
}

static void rcheevos_start_session(void)
{
   retro_task_t* task;

   if (rcheevos_load_aborted())
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Load aborted before starting session\n");
      return;
   }

   /* re-validate the config settings now that we know
    * which console_id is active */
   rcheevos_validate_config_settings();

   task           = task_init();
   task->handler  = rcheevos_start_session_async;
   task->callback = rcheevos_start_session_finish;
   task_queue_push(task);
}

static void rcheevos_initialize_runtime_callback(void* userdata)
{
   rcheevos_start_session();
}

static void rcheevos_fetch_game_data(void)
{
   if (rcheevos_load_aborted())
   {
      rcheevos_locals.game.hash = NULL;
      rcheevos_pause_hardcore();
      return;
   }

   if (rcheevos_locals.game.id <= 0)
   {
      const settings_t* settings = config_get_ptr();
      if (settings->bools.cheevos_verbose_enable)
         runloop_msg_queue_push(
            "RetroAchievements: Game could not be identified.",
            0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      CHEEVOS_LOG(RCHEEVOS_TAG "Game could not be identified\n");
      if (rcheevos_locals.load_info.hashes_tried > 1)
         rcheevos_locals.game.hash = NULL;

      rcheevos_locals.load_info.state = RCHEEVOS_LOAD_STATE_UNKNOWN_GAME;
      rcheevos_pause_hardcore();
      return;
   }

   if (!rcheevos_locals.token[0])
   {
      rcheevos_locals.load_info.state = RCHEEVOS_LOAD_STATE_LOGIN_FAILED;
      rcheevos_pause_hardcore();
      return;
   }

   /* fetch the game data and the user unlocks */
   rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_FETCHING_GAME_DATA);

#if HAVE_REWIND
   if (!rcheevos_locals.hardcore_active)
   {
      /* deactivate rewind while we activate the achievements */
      const settings_t* settings = config_get_ptr();
      if (settings->bools.rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* have to "schedule" this. CMD_EVENT_REWIND_DEINIT should only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_DEINIT;

            /* wait for rewind to be disabled */
            while (rcheevos_locals.queued_command != CMD_EVENT_NONE)
               retro_sleep(1);
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
      }
   }
#endif

   rcheevos_client_initialize_runtime(rcheevos_locals.game.id, rcheevos_initialize_runtime_callback, NULL);

   if (rcheevos_end_load_state() == 0)
      rcheevos_start_session();
}

struct rcheevos_identify_game_data
{
   struct rc_hash_iterator iterator;
   char* path;
   uint8_t* datacopy;
   char hash[33];
};

static void rcheevos_identify_game_callback(void* userdata)
{
   struct rcheevos_identify_game_data* data =
      (struct rcheevos_identify_game_data*)userdata;

   rcheevos_locals.load_info.hashes_tried++;

   if (rcheevos_locals.game.id == 0)
   {
      /* previous hash didn't match, try the next one */
      char new_hash[33];
      int found_new_hash;
      while ((found_new_hash = rc_hash_iterate(new_hash, &data->iterator)) != 0)
      {
         if (!rc_libretro_hash_set_get_game_id(&rcheevos_locals.game.hashes, new_hash))
            break;

         CHEEVOS_LOG(RCHEEVOS_TAG "Ignoring [%s]. Already tried.\n", new_hash);
      }

      if (found_new_hash)
      {
         memcpy(data->hash, new_hash, sizeof(data->hash));
         rcheevos_client_identify_game(data->hash,
               rcheevos_identify_game_callback, data);
         return;
      }
   }

   rc_libretro_hash_set_add(&rcheevos_locals.game.hashes,
      data->path, rcheevos_locals.game.id, data->hash);
   rcheevos_locals.game.hash =
      rc_libretro_hash_set_get_hash(&rcheevos_locals.game.hashes, data->path);

   if (data->iterator.path && strcmp(data->iterator.path, data->path) != 0)
   {
      rc_libretro_hash_set_add(&rcheevos_locals.game.hashes,
         data->iterator.path, rcheevos_locals.game.id, data->hash);
      rcheevos_locals.game.hash =
         rc_libretro_hash_set_get_hash(&rcheevos_locals.game.hashes, data->iterator.path);
   }

   /* no more hashes generated, free the iterator data */
   rc_hash_destroy_iterator(&data->iterator);
   if (data->datacopy)
      free(data->datacopy);
   if (data->path)
      free(data->path);
   free(data);

   /* hash resolution complete, proceed to fetching game data */
   if (rcheevos_end_load_state() == 0)
      rcheevos_fetch_game_data();
}

static int rcheevos_get_image_path(uint32_t index, char* buffer, size_t buffer_size)
{
   rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;
   if (!sys_info->disk_control.cb.get_image_path)
      return 0;
   return sys_info->disk_control.cb.get_image_path(index, buffer, buffer_size);
}

static bool rcheevos_identify_game(const struct retro_game_info* info)
{
   struct rcheevos_identify_game_data* data;
   struct rc_hash_filereader filereader;
   size_t len;
#ifndef DEBUG
   settings_t* settings = config_get_ptr();
#endif

   data = (struct rcheevos_identify_game_data*)
         calloc(1, sizeof(struct rcheevos_identify_game_data));
   if (!data)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "allocation failed\n");
      return false;
   }

   /* provide hooks for reading files */
   memset(&filereader, 0, sizeof(filereader));
   filereader.open = rc_hash_handle_file_open;
   filereader.seek = rc_hash_handle_file_seek;
   filereader.tell = rc_hash_handle_file_tell;
   filereader.read = rc_hash_handle_file_read;
   filereader.close = rc_hash_handle_file_close;
   rc_hash_init_custom_filereader(&filereader);

   rc_hash_init_error_message_callback(rcheevos_handle_log_message);

#ifndef DEBUG
   /* in DEBUG mode, always initialize the verbose message handler */
   if (settings->bools.cheevos_verbose_enable)
#endif
   {
      rc_hash_init_verbose_message_callback(rcheevos_handle_log_message);
   }

   rc_hash_reset_cdreader_hooks();

   /* fetch the first hash */
   rc_hash_initialize_iterator(&data->iterator,
         info->path, (uint8_t*)info->data, info->size);
   if (!rc_hash_iterate(data->hash, &data->iterator))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "no hashes generated\n");
      rc_hash_destroy_iterator(&data->iterator);
      free(data);
      return false;
   }

   rc_libretro_hash_set_init(&rcheevos_locals.game.hashes, info->path, rcheevos_get_image_path);
   data->path = strdup(info->path);

   if (data->iterator.consoles[data->iterator.index] != 0)
   {
      /* multiple potential matches, clone the data for the next attempt */
      if (info->data)
      {
         len = info->size;
         if (len > CHEEVOS_MB(64))
            len = CHEEVOS_MB(64);

         data->datacopy = (uint8_t*)malloc(len);
         if (!data->datacopy)
         {
            CHEEVOS_LOG(RCHEEVOS_TAG "allocation failed\n");
            rc_hash_destroy_iterator(&data->iterator);
            free(data);
            return false;
         }

         memcpy(data->datacopy, info->data, len);
         data->iterator.buffer = data->datacopy;
      }
   }

   rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_IDENTIFYING_GAME);
   rcheevos_client_identify_game(data->hash,
         rcheevos_identify_game_callback, data);
   return true;
}

static void rcheevos_login_callback(void* userdata)
{
   if (rcheevos_locals.token[0])
   {
      const settings_t* settings = config_get_ptr();
      if (settings->bools.cheevos_visibility_account)
      {
         char msg[256];
         msg[0] = '\0';
         /* TODO/FIXME - localize */
         snprintf(msg, sizeof(msg),
            "RetroAchievements: Logged in as \"%s\".",
            rcheevos_locals.displayname);
         runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   if (rcheevos_end_load_state() == 0)
      rcheevos_fetch_game_data();
}

/* Increment the outstanding requests counter and set the load state */
void rcheevos_begin_load_state(enum rcheevos_load_state state)
{
#ifdef HAVE_THREADS
   slock_lock(rcheevos_locals.load_info.request_lock);
#endif
   ++rcheevos_locals.load_info.outstanding_requests;
   rcheevos_locals.load_info.state = state;
#ifdef HAVE_THREADS
   slock_unlock(rcheevos_locals.load_info.request_lock);
#endif
}

/* Decrement and return the outstanding requests counter.
 * If non-zero, requests are still outstanding */
int rcheevos_end_load_state(void)
{
   int requests = 0;

#ifdef HAVE_THREADS
   slock_lock(rcheevos_locals.load_info.request_lock);
#endif
   if (rcheevos_locals.load_info.outstanding_requests > 0)
      --rcheevos_locals.load_info.outstanding_requests;
   requests = rcheevos_locals.load_info.outstanding_requests;
#ifdef HAVE_THREADS
   slock_unlock(rcheevos_locals.load_info.request_lock);
#endif

   return requests;
}

bool rcheevos_load_aborted(void)
{
   switch (rcheevos_locals.load_info.state)
   {
      /* Unload has been called */
      case RCHEEVOS_LOAD_STATE_ABORTED:
      /* Unload quit waiting and ran to completion */
      case RCHEEVOS_LOAD_STATE_NONE:
      /* Login/resolve hash failed after several attempts */
      case RCHEEVOS_LOAD_STATE_NETWORK_ERROR:
         return true;
      default:
         break;
   }
   return false;
}

#endif /* HAVE_RC_CLIENT */

bool rcheevos_load(const void *data)
{
   const struct retro_game_info *info = (const struct retro_game_info*)data;
   settings_t *settings               = config_get_ptr();
   bool cheevos_enable                = settings
      && settings->bools.cheevos_enable;

#ifndef HAVE_RC_CLIENT
   memset(&rcheevos_locals.load_info, 0,
         sizeof(rcheevos_locals.load_info));

   rcheevos_locals.loaded             = false;
   rcheevos_locals.game.id            = -1;
   rcheevos_locals.game.console_id    = 0;
   rcheevos_locals.game.mastery_placard_shown = false;
 #ifdef HAVE_GFX_WIDGETS
   rcheevos_locals.tracker_progress   = 0.0;
 #endif
   rc_runtime_init(&rcheevos_locals.runtime);
#endif /* HAVE_RC_CLIENT */

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
#endif

   /* If achievements are not enabled, or the core doesn't
    * support achievements, disable hardcore and bail */
   if (!cheevos_enable || !rcheevos_locals.core_supports || !data)
   {
#ifndef HAVE_RC_CLIENT
      rcheevos_locals.game.id = 0;
#endif
      rcheevos_pause_hardcore();
      return false;
   }

   if (string_is_empty(settings->arrays.cheevos_username))
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Cannot login (no username)\n");
      runloop_msg_queue_push("Missing RetroAchievements account information.", 0, 5 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
#ifndef HAVE_RC_CLIENT
      rcheevos_locals.game.id = 0;
#endif
      rcheevos_pause_hardcore();
      return false;
   }

#ifdef HAVE_RC_CLIENT
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
#ifndef HAVE_RC_CLIENT
   else
   {
#if HAVE_REWIND
      /* deactivate rewind while we activate the achievements */
      const settings_t* settings = config_get_ptr();
      if (settings->bools.rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* have to "schedule" this. CMD_EVENT_REWIND_DEINIT should only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_DEINIT;

            /* wait for rewind to be disabled */
            while (rcheevos_locals.queued_command != CMD_EVENT_NONE)
               retro_sleep(1);
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
      }
#endif
   }
#endif

   /* provide hooks for reading files */
   rc_hash_reset_cdreader_hooks();

#if defined(HAVE_GFX_WIDGETS)
   if (settings->bools.cheevos_verbose_enable)
      gfx_widget_set_cheevos_set_loading(true);
#endif

   rc_client_begin_identify_and_load_game(rcheevos_locals.client, RC_CONSOLE_UNKNOWN,
      info->path, (const uint8_t*)info->data, info->size, rcheevos_client_load_game_callback, NULL);

#else /* !HAVE_RC_CLIENT */
 #ifdef HAVE_THREADS
   if (!rcheevos_locals.load_info.request_lock)
      rcheevos_locals.load_info.request_lock = slock_new();
 #endif
   rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_IDENTIFYING_GAME);

   /* reset hardcore mode and leaderboard settings based on configs */
   rcheevos_hardcore_enabled_changed();
   CHEEVOS_LOG(RCHEEVOS_TAG "Load started, hardcore %sactive\n", rcheevos_hardcore_active() ? "" : "not ");

   rcheevos_validate_config_settings();

   /* Refresh the user agent in case it's not set or has changed */
   rcheevos_client_initialize();
   rcheevos_get_user_agent(&rcheevos_locals,
      rcheevos_locals.user_agent_core,
      sizeof(rcheevos_locals.user_agent_core));

   /* === ACHIEVEMENT INITIALIZATION PROCESS ===

      1. RCHEEVOS_LOAD_STATE_IDENTIFYING_GAME
         a. iterate possible hashes to identify game [rcheevos_identify_game]
            i. if game not found, display "no achievements for this game" and abort [rcheevos_identify_game_callback]
         b. Login
            i. if already logged in, skip this step
            ii. start login request [rcheevos_client_login_with_password/rcheevos_client_login_with_token]
            iii. complete login, store user/token [rcheevos_login_callback]
      2. RCHEEVOS_LOAD_STATE_FETCHING_GAME_DATA [rcheevos_client_initialize_runtime]
         a. begin game data request [rc_api_init_fetch_game_data_request]
         b. fetch user unlocks
            i. if encore mode, skip this step
            ii. begin user unlocks hardcore request [rc_api_init_fetch_user_unlocks_request]
            iii. begin user unlocks softcore request [rc_api_init_fetch_user_unlocks_request]
      3. RCHEEVOS_LOAD_STATE_STARTING_SESSION [rcheevos_initialize_runtime_callback]
         a. activate achievements [rcheevos_activate_achievements]
         b. schedule rich presence periodic update [rcheevos_client_start_session]
         c. start session on server [rcheevos_client_start_session]
         d. show title card [rcheevos_show_game_placard]
      4. RCHEEVOS_LOAD_STATE_FETCHING_BADGES
         a. download from server [rcheevos_client_fetch_badges]
      5. RCHEEVOS_LOAD_STATE_DONE

    */

   /* Identify the game and log the user in.
    * These will run asynchronously. */
   if (!rcheevos_identify_game(info))
   {
      /* No hashes could be generated for the game,
       * disable hardcore and bail */
      rcheevos_locals.game.id = 0;
      rcheevos_end_load_state();
      rcheevos_pause_hardcore();
      return false;
   }

   if (!rcheevos_locals.token[0])
   {
      rcheevos_begin_load_state(RCHEEVOS_LOAD_STATE_IDENTIFYING_GAME);
      if (!string_is_empty(settings->arrays.cheevos_token))
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Attempting to login %s (with token)\n",
               settings->arrays.cheevos_username);
         rcheevos_client_login_with_token(
               settings->arrays.cheevos_username,
               settings->arrays.cheevos_token,
               rcheevos_login_callback, NULL);
      }
      else if (!string_is_empty(settings->arrays.cheevos_password))
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Attempting to login %s (with password)\n",
               settings->arrays.cheevos_username);
         rcheevos_client_login_with_password(
               settings->arrays.cheevos_username,
               settings->arrays.cheevos_password,
               rcheevos_login_callback, NULL);
      }
      else
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Cannot login %s (no password or token)\n",
               settings->arrays.cheevos_username);
         runloop_msg_queue_push("No password provided for RetroAchievements account", 0, 5 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
         rcheevos_unload();
         return false;
      }
   }

   if (rcheevos_end_load_state() == 0)
      rcheevos_fetch_game_data();
#endif /* HAVE_RC_CLIENT */

   return true;
}

#ifdef HAVE_RC_CLIENT

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

#else /* !HAVE_RC_CLIENT */

struct rcheevos_identify_changed_disc_data
{
   int real_game_id;
   char* path;
   char hash[33];
};

static void rcheevos_identify_game_disc_callback(void* userdata)
{
   struct rcheevos_identify_changed_disc_data* changed_disc_data =
      (struct rcheevos_identify_changed_disc_data*)userdata;

   /* rcheevos_locals.game.id has the game id for the new hash, swap it with the old game id */
   const int hash_game_id = rcheevos_locals.game.id;
   rcheevos_locals.game.id = changed_disc_data->real_game_id;

   /* rcheevos_client_identify_game will update rcheevos_locals.game.id */
   if (rcheevos_locals.game.id == hash_game_id)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Hash valid for current game\n");
   }
   else if (hash_game_id != 0)
   {
      /* when changing discs, if the disc is recognized but belongs to another game, allow it.
       * this allows loading known game discs for games that leverage user-provided discs. */
      CHEEVOS_LOG(RCHEEVOS_TAG "Hash identified for game %d\n", hash_game_id);
   }
   else
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Disc not recognized\n");
      if (rcheevos_hardcore_active())
      {
         /* don't allow unknown game discs in hardcore.
            * assume it's a modified version of the base game. */
         runloop_msg_queue_push("Hardcore paused. Game disc unrecognized.", 0, 5 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
         rcheevos_pause_hardcore();
      }
   }

   /* disc is valid, add it to the known disk list */
   rc_libretro_hash_set_add(&rcheevos_locals.game.hashes,
      changed_disc_data->path, hash_game_id, changed_disc_data->hash);

   rcheevos_locals.game.hash =
      rc_libretro_hash_set_get_hash(&rcheevos_locals.game.hashes, changed_disc_data->hash);

   free(changed_disc_data->path);
   free(changed_disc_data);
}

static void rcheevos_identify_initial_disc_callback(void* userdata)
{
   struct rcheevos_identify_changed_disc_data* changed_disc_data =
      (struct rcheevos_identify_changed_disc_data*)userdata;

   /* rcheevos_client_identify_game will update rcheevos_locals.game.id */
   if (rcheevos_locals.game.id != changed_disc_data->real_game_id)
   {
      if (rcheevos_locals.game.id == 0)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Disc not recognized\n");
         runloop_msg_queue_push("Disabling achievements. Game disc unrecognized.", 0, 5 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      }
      else
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Initial disc for game %d\n", rcheevos_locals.game.id);
         runloop_msg_queue_push("Disabling achievements. Not for loaded game.", 0, 5 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      }

      rcheevos_locals.game.hash = NULL;
      rcheevos_unload();
   }
   else
   {
      /* disc is valid, add it to the known disk list */
      CHEEVOS_LOG(RCHEEVOS_TAG "Hash valid for current game\n");
      rc_libretro_hash_set_add(&rcheevos_locals.game.hashes,
         changed_disc_data->path, rcheevos_locals.game.id, changed_disc_data->hash);

      rcheevos_locals.game.hash =
         rc_libretro_hash_set_get_hash(&rcheevos_locals.game.hashes, changed_disc_data->hash);
   }

   free(changed_disc_data->path);
   free(changed_disc_data);
}

static void rcheevos_validate_initial_disc_handler(retro_task_t* task)
{
   char* new_disc_path = (char*)task->user_data;

   if (rcheevos_locals.game.id == 0)
   {
      /* could not identify game. don't bother identifying initial disc */
   }
   else
   {
      if (rcheevos_locals.game.console_id == 0)
      {
         /* not ready yet. try again in another 500ms */
         task->when = cpu_features_get_time_usec() + 500 * 1000;
         return;
      }

      /* game ready. attempt to validate the initial disc */
      rcheevos_change_disc(new_disc_path, true);
   }

   free(new_disc_path);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

void rcheevos_change_disc(const char* new_disc_path, bool initial_disc)
{
   struct rcheevos_identify_changed_disc_data* data;
   char hash[33];
   int hash_game_id;

   /* no game loaded */
   if (rcheevos_locals.game.id == 0)
      return;

   /* see if we've already identified this file */
   rcheevos_locals.game.hash =
      rc_libretro_hash_set_get_hash(&rcheevos_locals.game.hashes, new_disc_path);
   if (rcheevos_locals.game.hash)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Switched to known hash: %s\n", rcheevos_locals.game.hash);
      return;
   }

   /* don't check the disc until the game is done loading */
   if (rcheevos_locals.game.console_id == 0)
   {
      retro_task_t* task = task_init();
      task->handler = rcheevos_validate_initial_disc_handler;
      task->user_data = strdup(new_disc_path);
      task->progress = -1;
      task->when = cpu_features_get_time_usec() + 500 * 1000; /* 500ms */
      task_queue_push(task);
      return;
   }

   /* attempt to identify the file */
   if (rc_hash_generate_from_file(hash, rcheevos_locals.game.console_id, new_disc_path))
   {
      /* check to see if the hash is already known */
      hash_game_id = rc_libretro_hash_set_get_game_id(&rcheevos_locals.game.hashes, hash);
      if (hash_game_id)
      {
         /* hash identical to some other file - probably the first disc matching the m3u. */
         CHEEVOS_LOG(RCHEEVOS_TAG "Hash valid for current game\n");
      }
   }
   else
   {
      /* when changing discs, if the disc is not supported by the system, allow it. this is
       * primarily for games that support user-provided audio CDs, but does allow using discs
       * from other systems for games that leverage user-provided discs. */
      CHEEVOS_LOG(RCHEEVOS_TAG "No hash generated\n");
      hash_game_id = -1;
      strlcpy(hash, "[NO HASH]", sizeof(hash));
   }

   if (hash_game_id)
   {
      /* we know how to handle this disc. no need to call the server */
      rc_libretro_hash_set_add(&rcheevos_locals.game.hashes, new_disc_path, hash_game_id, hash);
      rcheevos_locals.game.hash =
         rc_libretro_hash_set_get_hash(&rcheevos_locals.game.hashes, new_disc_path);
      return;
   }

   /* call the server to make sure the hash is valid for the loaded game */
   data = (struct rcheevos_identify_changed_disc_data*)
      calloc(1, sizeof(struct rcheevos_identify_changed_disc_data));
   if (data) {
      data->real_game_id = rcheevos_locals.game.id;
      data->path = strdup(new_disc_path);
      memcpy(data->hash, hash, sizeof(data->hash));

      rcheevos_client_identify_game(data->hash,
         initial_disc ? rcheevos_identify_initial_disc_callback :
            rcheevos_identify_game_disc_callback, data);
   }
}

#endif /* HAVE_RC_CLIENT */
