/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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
#include <net/net_http.h>
#include <libretro.h>

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
#include "cheevos_locals.h"
#include "cheevos_parser.h"

#include "../file_path_special.h"
#include "../paths.h"
#include "../command.h"
#include "../configuration.h"
#include "../performance_counters.h"
#include "../msg_hash.h"
#include "../retroarch.h"
#include "../core.h"
#include "../core_option_manager.h"

#include "../tasks/tasks_internal.h"

#include "../deps/rcheevos/include/rc_runtime.h"
#include "../deps/rcheevos/include/rc_url.h"
#include "../deps/rcheevos/include/rc_hash.h"
#include "../deps/rcheevos/src/rcheevos/rc_libretro.h"

/* Define this macro to prevent cheevos from being deactivated. */
#undef CHEEVOS_DONT_DEACTIVATE

/* Define this macro to load a JSON file from disk instead of downloading
 * from retroachievements.org. */
#undef CHEEVOS_JSON_OVERRIDE

/* Define this macro with a string to save the JSON file to disk with
 * that name. */
#undef CHEEVOS_SAVE_JSON

/* Define this macro to log downloaded badge images. */
#undef CHEEVOS_LOG_BADGES

/* Define this macro to capture how long it takes to generate a hash */
#undef CHEEVOS_TIME_HASH

static rcheevos_locals_t rcheevos_locals =
{
   {0},  /* runtime */
   {0},  /* patchdata */
   {{0}},/* memory */
   NULL, /* task */
#ifdef HAVE_THREADS
   NULL, /* task_lock */
   CMD_EVENT_NONE, /* queued_command */
#endif
   "",   /* username */
   "",   /* token */
   "N/A",/* hash */
   "",   /* user_agent_prefix */
   "",   /* user_agent_core */
#ifdef HAVE_MENU
   NULL, /* menuitems */
   0,    /* menuitem_capacity */
   0,    /* menuitem_count */
#endif
   false,/* hardcore_active */
   false,/* loaded */
   true, /* core_supports */
   false,/* network_error */
   false,/* leaderboards_enabled */
   false,/* leaderboard_notifications */
   false /* leaderboard_trackers */
};

rcheevos_locals_t* get_rcheevos_locals(void)
{
   return &rcheevos_locals;
}

#ifdef HAVE_THREADS
#define CHEEVOS_LOCK(l)   do { slock_lock(l); } while (0)
#define CHEEVOS_UNLOCK(l) do { slock_unlock(l); } while (0)
#else
#define CHEEVOS_LOCK(l)
#define CHEEVOS_UNLOCK(l)
#endif

#define CHEEVOS_MB(x)   ((x) * 1024 * 1024)

/* Forward declaration */
static void rcheevos_validate_memrefs(rcheevos_locals_t* locals);

/*****************************************************************************
Supporting functions.
*****************************************************************************/

#ifndef CHEEVOS_VERBOSE
void rcheevos_log(const char *fmt, ...)
{
   (void)fmt;
}
#endif


static void rcheevos_achievement_disabled(rcheevos_racheevo_t* cheevo, unsigned address)
{
   if (!cheevo)
      return;

   CHEEVOS_ERR(RCHEEVOS_TAG "Achievement %u disabled (invalid address %06X): %s\n",
               cheevo->id, address, cheevo->title);
   CHEEVOS_FREE(cheevo->memaddr);
   cheevo->memaddr = NULL;
}

static void rcheevos_lboard_disabled(rcheevos_ralboard_t* lboard, unsigned address)
{
   if (!lboard)
      return;

   CHEEVOS_ERR(RCHEEVOS_TAG "Leaderboard %u disabled (invalid address %06X): %s\n",
               lboard->id, address, lboard->title);
   CHEEVOS_FREE(lboard->mem);
   lboard->mem = NULL;
}

static void rcheevos_handle_log_message(const char* message)
{
   CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", message);
}

static void rcheevos_get_core_memory_info(unsigned id, rc_libretro_core_memory_info_t* info)
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
   rarch_system_info_t* system                 = &runloop_state_get_ptr()->system;
   rarch_memory_map_t* mmaps                   = &system->mmaps;
   struct retro_memory_descriptor *descriptors = (struct retro_memory_descriptor*)malloc(mmaps->num_descriptors * sizeof(*descriptors));
   if (!descriptors)
      return 0;

   mmap.descriptors = &descriptors[0];
   mmap.num_descriptors = mmaps->num_descriptors;

   /* RetroArch wraps the retro_memory_descriptor's in rarch_memory_descriptor_t's, pull them back out */
   for (i = 0; i < mmap.num_descriptors; ++i)
      memcpy(&descriptors[i], &mmaps->descriptors[i].core, sizeof(descriptors[0]));

   rc_libretro_init_verbose_message_callback(rcheevos_handle_log_message);
   result = rc_libretro_memory_init(&locals->memory, &mmap,
         rcheevos_get_core_memory_info, locals->patchdata.console_id);

   free(descriptors);
   return result;
}

uint8_t* rcheevos_patch_address(unsigned address)
{
   if (rcheevos_locals.memory.count == 0)
   {
      /* memory map was not previously initialized (no achievements for this game?) try now */
      rcheevos_init_memory(&rcheevos_locals);
   }

   return rc_libretro_memory_find(&rcheevos_locals.memory, address);
}

static unsigned rcheevos_peek(unsigned address, unsigned num_bytes, void* ud)
{
   uint8_t* data = rc_libretro_memory_find(&rcheevos_locals.memory, address);
   if (data)
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

static void rcheevos_activate_achievements(rcheevos_locals_t *locals,
      rcheevos_racheevo_t* cheevo, unsigned count, unsigned flags)
{
   int res;
   unsigned i;
   char buffer[256];
   settings_t *settings = config_get_ptr();

   for (i = 0; i < count; i++, cheevo++)
   {
      res = rc_runtime_activate_achievement(&locals->runtime, cheevo->id,
            cheevo->memaddr, NULL, 0);

      if (res < 0)
      {
         snprintf(buffer, sizeof(buffer),
               "Could not activate achievement %d \"%s\": %s",
               cheevo->id, cheevo->title, rc_error_str(res));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s: mem %s\n", buffer, cheevo->memaddr);
         CHEEVOS_FREE(cheevo->memaddr);
         cheevo->memaddr = NULL;
         continue;
      }

      cheevo->active = 
           RCHEEVOS_ACTIVE_SOFTCORE 
         | RCHEEVOS_ACTIVE_HARDCORE 
         | flags;
   }
}

static int rcheevos_parse(rcheevos_locals_t *locals, const char* json)
{
   char buffer[256];
   unsigned j                  = 0;
   unsigned count              = 0;
   settings_t *settings        = NULL;
   rcheevos_ralboard_t* lboard = NULL;
   int res                     = rcheevos_get_patchdata(
         json, &locals->patchdata);

   if (res != 0)
   {
      char *ptr = NULL;
      strcpy_literal(buffer, "Error retrieving achievement data: ");
      ptr       = buffer + strlen(buffer);

      /* Extract the Error field from the JSON. 
       * If not found, remove the colon from the message. */
      if (rcheevos_get_json_error(json, ptr,
               sizeof(buffer) - (ptr - buffer)) == -1)
         ptr[-2] = '\0';

      runloop_msg_queue_push(buffer, 0, 5 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

      RARCH_ERR(RCHEEVOS_TAG "%s", buffer);
      return -1;
   }

   if (   locals->patchdata.core_count       == 0
       && locals->patchdata.unofficial_count == 0
       && locals->patchdata.lboard_count     == 0
       && (!locals->patchdata.richpresence_script ||
           !*locals->patchdata.richpresence_script))
   {
      rcheevos_free_patchdata(&locals->patchdata);
      return 0;
   }

   settings        = config_get_ptr();

   if (!rcheevos_init_memory(locals))
   {
      /* some cores (like Mupen64-Plus) don't expose the 
       * memory until the first call to retro_run.
       * in that case, there will be a total_size of 
       * memory reported by the core, but init will return
       * false, as all of the pointers were null.
       */

      /* reset the memory count and we'll re-evaluate in rcheevos_test() */
      if (locals->memory.total_size != 0)
         locals->memory.count = 0;
      else
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "No memory exposed by core.\n");
         rcheevos_locals.core_supports = false;

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
               0, 4 * 60, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

         goto error;
      }
   }

   /* Initialize. */
   rcheevos_activate_achievements(locals, locals->patchdata.core,
         locals->patchdata.core_count, 0);

   if (settings->bools.cheevos_test_unofficial)
      rcheevos_activate_achievements(locals, locals->patchdata.unofficial,
            locals->patchdata.unofficial_count, RCHEEVOS_ACTIVE_UNOFFICIAL);

   if (locals->hardcore_active && locals->leaderboards_enabled)
   {
      lboard = locals->patchdata.lboards;
      count  = locals->patchdata.lboard_count;

      for (j = 0; j < count; j++, lboard++)
      {
         res = rc_runtime_activate_lboard(&locals->runtime, lboard->id,
               lboard->mem, NULL, 0);

         if (res < 0)
         {
            snprintf(buffer, sizeof(buffer),
                  "Could not activate leaderboard %d \"%s\": %s",
                  lboard->id, lboard->title, rc_error_str(res));

            if (settings->bools.cheevos_verbose_enable)
               runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            CHEEVOS_ERR(RCHEEVOS_TAG "%s mem: %s\n", buffer, lboard->mem);
            CHEEVOS_FREE(lboard->mem);
            lboard->mem = NULL;
            continue;
         }
      }
   }

   res = RC_MISSING_DISPLAY_STRING;
   if (      locals->patchdata.richpresence_script 
         && *locals->patchdata.richpresence_script)
   {
      res = rc_runtime_activate_richpresence(&locals->runtime,
            locals->patchdata.richpresence_script, NULL, 0);

      if (res < 0)
      {
         snprintf(buffer, sizeof(buffer),
               "Could not activate rich presence: %s",
               rc_error_str(res));

         if (settings->bools.cheevos_verbose_enable)
            runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         CHEEVOS_ERR(RCHEEVOS_TAG "%s\n", buffer);
      }
   }

   rcheevos_client_start_session(locals->patchdata.game_id);

   /* validate the memrefs */
   if (rcheevos_locals.memory.count != 0)
      rcheevos_validate_memrefs(&rcheevos_locals);

   return 0;

error:
   rcheevos_free_patchdata(&locals->patchdata);
   rc_libretro_memory_destroy(&locals->memory);
   return -1;
}

static rcheevos_racheevo_t* rcheevos_find_cheevo(unsigned id)
{
   unsigned i;
   rcheevos_racheevo_t* cheevo;

   cheevo = rcheevos_locals.patchdata.core;
   for (i = 0; i < rcheevos_locals.patchdata.core_count; i++, cheevo++)
   {
      if (cheevo->id == id)
         return cheevo;
   }

   cheevo = rcheevos_locals.patchdata.unofficial;
   for (i = 0; i < rcheevos_locals.patchdata.unofficial_count; i++, cheevo++)
   {
      if (cheevo->id == id)
         return cheevo;
   }

   return NULL;
}

void rcheevos_award_achievement(rcheevos_locals_t* locals,
      rcheevos_racheevo_t* cheevo, bool widgets_ready)
{
   const settings_t *settings = config_get_ptr();

   if (!cheevo)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Awarding achievement %u: %s (%s)\n",
         cheevo->id, cheevo->title, cheevo->description);

   /* Deactivates the acheivement. */
   rc_runtime_deactivate_achievement(&locals->runtime, cheevo->id);

   cheevo->active &= ~RCHEEVOS_ACTIVE_SOFTCORE;
   if (locals->hardcore_active)
      cheevo->active &= ~RCHEEVOS_ACTIVE_HARDCORE;

   cheevo->unlock_time = cpu_features_get_time_usec();

   /* Show the on screen message. */
#if defined(HAVE_GFX_WIDGETS)
   if (widgets_ready)
   {
      gfx_widgets_push_achievement(cheevo->title, cheevo->badge);
   }
   else
#endif
   {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%s: %s", 
            msg_hash_to_str(MSG_ACHIEVEMENT_UNLOCKED), cheevo->title);
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      runloop_msg_queue_push(cheevo->description, 0, 3 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* Start the award task (unofficial achievement unlocks are not submitted). */
   if (!(cheevo->active & RCHEEVOS_ACTIVE_UNOFFICIAL))
      rcheevos_client_award_achievement(cheevo->id);

   /* play the unlock sound */
#ifdef HAVE_AUDIOMIXER
   if (settings->bools.cheevos_unlock_sound_enable)
      audio_driver_mixer_play_menu_sound(
            AUDIO_MIXER_SYSTEM_SLOT_ACHIEVEMENT_UNLOCK);
#endif

   /* Take a screenshot of the achievement. */
#ifdef HAVE_SCREENSHOTS
   if (settings->bools.cheevos_auto_screenshot)
   {
      size_t shotname_len  = sizeof(char) * 8192;
      char *shotname       = (char*)malloc(shotname_len);

      if (shotname)
      {
         snprintf(shotname, shotname_len, "%s/%s-cheevo-%u",
               settings->paths.directory_screenshot,
               path_basename(path_get(RARCH_PATH_BASENAME)),
               cheevo->id);
         shotname[shotname_len - 1] = '\0';

         if (take_screenshot(settings->paths.directory_screenshot,
                  shotname, true,
                  video_driver_cached_frame_has_valid_framebuffer(),
                  false, true))
            CHEEVOS_LOG(RCHEEVOS_TAG "Captured screenshot for achievement %u\n",
                  cheevo->id);
         else
            CHEEVOS_LOG(RCHEEVOS_TAG "Failed to capture screenshot for achievement %u\n",
                  cheevo->id);

         free(shotname);
      }
   }
#endif
}

static rcheevos_ralboard_t* rcheevos_find_lboard(unsigned id)
{
   rcheevos_ralboard_t* lboard = rcheevos_locals.patchdata.lboards;
   unsigned i;

   for (i = 0; i < rcheevos_locals.patchdata.lboard_count; ++i, ++lboard)
   {
      if (lboard->id == id)
         return lboard;
   }

   return NULL;
}

static void rcheevos_lboard_submit(rcheevos_locals_t* locals,
      rcheevos_ralboard_t* lboard, int value, bool widgets_ready)
{
   char buffer[256];
   char formatted_value[16];

   rc_runtime_format_lboard_value(formatted_value, sizeof(formatted_value),
         value, lboard->format);
   CHEEVOS_LOG(RCHEEVOS_TAG "Submitting %s for leaderboard %u\n",
         formatted_value, lboard->id);

   /* Show the on-screen message (regardless of notifications setting). */
   snprintf(buffer, sizeof(buffer), "Submitted %s for %s",
         formatted_value, lboard->title);
   runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#if defined(HAVE_GFX_WIDGETS)
   /* Hide the tracker */
   if (gfx_widgets_ready())
      gfx_widgets_set_leaderboard_display(lboard->id, NULL);
#endif

   /* Start the submit task */
   rcheevos_client_submit_lboard_entry(lboard->id, value);
}

static void rcheevos_lboard_canceled(rcheevos_ralboard_t * lboard,
      bool widgets_ready)
{
   char buffer[256];
   if (!lboard)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard %u canceled: %s\n",
         lboard->id, lboard->title);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_ready)
      gfx_widgets_set_leaderboard_display(lboard->id, NULL);
#endif

   if (rcheevos_locals.leaderboard_notifications)
   {
      snprintf(buffer, sizeof(buffer),
            "Leaderboard attempt failed: %s", lboard->title);
      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

static void rcheevos_lboard_started(rcheevos_ralboard_t * lboard, int value,
      bool widgets_ready)
{
   char buffer[256];
   if (!lboard)
      return;

   CHEEVOS_LOG(RCHEEVOS_TAG "Leaderboard %u started: %s\n",
         lboard->id, lboard->title);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_ready && rcheevos_locals.leaderboard_trackers)
   {
      rc_runtime_format_lboard_value(buffer, sizeof(buffer), value, lboard->format);
      gfx_widgets_set_leaderboard_display(lboard->id, buffer);
   }
#endif

   if (rcheevos_locals.leaderboard_notifications)
   {
      if (lboard->description && *lboard->description)
         snprintf(buffer, sizeof(buffer),
               "Leaderboard attempt started: %s - %s",
               lboard->title, lboard->description);
      else
         snprintf(buffer, sizeof(buffer),
               "Leaderboard attempt started: %s",
               lboard->title);

      runloop_msg_queue_push(buffer, 0, 2 * 60, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

#if defined(HAVE_GFX_WIDGETS)
static void rcheevos_lboard_updated(rcheevos_ralboard_t* lboard, int value,
      bool widgets_ready)
{
   if (!lboard)
      return;

   if (widgets_ready && rcheevos_locals.leaderboard_trackers)
   {
      char buffer[32];
      rc_runtime_format_lboard_value(buffer, sizeof(buffer), value, lboard->format);
      gfx_widgets_set_leaderboard_display(lboard->id, buffer);
   }
}

static void rcheevos_challenge_started(rcheevos_racheevo_t* cheevo, int value,
      bool widgets_ready)
{
   settings_t* settings = config_get_ptr();
   if (cheevo && widgets_ready && settings->bools.cheevos_challenge_indicators)
      gfx_widgets_set_challenge_display(cheevo->id, cheevo->badge);
}

static void rcheevos_challenge_ended(rcheevos_racheevo_t* cheevo, int value,
      bool widgets_ready)
{
   if (cheevo && widgets_ready)
      gfx_widgets_set_challenge_display(cheevo->id, NULL);
}

#endif

int rcheevos_get_richpresence(char buffer[], int buffer_size)
{
   int ret = rc_runtime_get_richpresence(&rcheevos_locals.runtime, buffer, buffer_size, &rcheevos_peek, NULL, NULL);

   if (ret <= 0 && rcheevos_locals.patchdata.title)
      ret = snprintf(buffer, buffer_size, "Playing %s", rcheevos_locals.patchdata.title);

   return ret;
}

void rcheevos_reset_game(bool widgets_ready)
{
#if defined(HAVE_GFX_WIDGETS)
   /* Hide any visible trackers */
   if (widgets_ready)
   {
      rcheevos_ralboard_t* lboard;
      rcheevos_racheevo_t* cheevo;
      unsigned i;

      lboard = rcheevos_locals.patchdata.lboards;
      for (i = 0; i < rcheevos_locals.patchdata.lboard_count; ++i, ++lboard)
         gfx_widgets_set_leaderboard_display(lboard->id, NULL);

      cheevo = rcheevos_locals.patchdata.core;
      for (i = 0; i < rcheevos_locals.patchdata.core_count; ++i, ++cheevo)
         gfx_widgets_set_challenge_display(cheevo->id, NULL);

      cheevo = rcheevos_locals.patchdata.unofficial;
      for (i = 0; i < rcheevos_locals.patchdata.unofficial_count; ++i, ++cheevo)
         gfx_widgets_set_challenge_display(cheevo->id, NULL);
   }
#endif

   rc_runtime_reset(&rcheevos_locals.runtime);

   /* Some cores reallocate memory on reset, 
    * make sure we update our pointers */
   if (rcheevos_locals.memory.total_size > 0)
      rcheevos_init_memory(&rcheevos_locals);
}

bool rcheevos_hardcore_active(void)
{
   return rcheevos_locals.hardcore_active;
}

void rcheevos_pause_hardcore(void)
{
   if (rcheevos_locals.hardcore_active)
      rcheevos_toggle_hardcore_paused();
}

bool rcheevos_unload(void)
{
   bool running          = false;
   settings_t* settings  = config_get_ptr();

   CHEEVOS_LOCK(rcheevos_locals.task_lock);
   running               = rcheevos_locals.task != NULL;
   CHEEVOS_UNLOCK(rcheevos_locals.task_lock);

   if (running)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Asked the load thread to terminate\n");
      task_queue_cancel_task(rcheevos_locals.task);

#ifdef HAVE_THREADS
      do
      {
         CHEEVOS_LOCK(rcheevos_locals.task_lock);
         running = rcheevos_locals.task != NULL;
         CHEEVOS_UNLOCK(rcheevos_locals.task_lock);
      } while(running);
#endif
   }

   if (rcheevos_locals.memory.count > 0)
      rc_libretro_memory_destroy(&rcheevos_locals.memory);

   if (rcheevos_locals.loaded)
   {
#ifdef HAVE_MENU
      rcheevos_menu_reset_badges();

      if (rcheevos_locals.menuitems)
      {
         CHEEVOS_FREE(rcheevos_locals.menuitems);
         rcheevos_locals.menuitems = NULL;
         rcheevos_locals.menuitem_capacity = rcheevos_locals.menuitem_count = 0;
      }
#endif
      rcheevos_free_patchdata(&rcheevos_locals.patchdata);

      rcheevos_locals.loaded                    = false;
      rcheevos_locals.hardcore_active           = false;
   }

#ifdef HAVE_THREADS
   rcheevos_locals.queued_command = CMD_EVENT_NONE;
#endif

   rc_runtime_destroy(&rcheevos_locals.runtime);

   /* If the config-level token has been cleared, 
    * we need to re-login on loading the next game */
   if (!settings->arrays.cheevos_token[0])
      rcheevos_locals.token[0]                  = '\0';

   return true;
}

static void rcheevos_toggle_hardcore_achievements(rcheevos_locals_t *locals, 
      rcheevos_racheevo_t* cheevo, unsigned count)
{
   const unsigned active_mask = 
      RCHEEVOS_ACTIVE_SOFTCORE | RCHEEVOS_ACTIVE_HARDCORE;

   while (count--)
   {
      if (cheevo->memaddr && (cheevo->active & active_mask) 
            == RCHEEVOS_ACTIVE_HARDCORE)
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

static void rcheevos_activate_leaderboards(rcheevos_locals_t* locals)
{
   rcheevos_ralboard_t* lboard = locals->patchdata.lboards;
   unsigned i;

   for (i = 0; i < locals->patchdata.lboard_count; ++i, ++lboard)
   {
      if (lboard->mem)
         rc_runtime_activate_lboard(&locals->runtime, lboard->id,
               lboard->mem, NULL, 0);
   }
}

static void rcheevos_deactivate_leaderboards(rcheevos_locals_t* locals)
{
   rcheevos_ralboard_t* lboard = locals->patchdata.lboards;
   unsigned i;

   for (i = 0; i < locals->patchdata.lboard_count; ++i, ++lboard)
   {
      if (lboard->mem)
      {
         rc_runtime_deactivate_lboard(&locals->runtime, lboard->id);

#if defined(HAVE_GFX_WIDGETS)
         /* Hide any visible trackers */
         gfx_widgets_set_leaderboard_display(lboard->id, NULL);
#endif
      }
   }
}

void rcheevos_leaderboards_enabled_changed(void)
{
   const settings_t* settings           = config_get_ptr();
   const bool leaderboards_enabled      = rcheevos_locals.leaderboards_enabled;
   const bool leaderboard_trackers      = rcheevos_locals.leaderboard_trackers;

   rcheevos_locals.leaderboards_enabled = rcheevos_locals.hardcore_active;

   if (string_is_equal(settings->arrays.cheevos_leaderboards_enable, "true"))
   {
      rcheevos_locals.leaderboard_notifications = true;
      rcheevos_locals.leaderboard_trackers = true;
   }
#if defined(HAVE_GFX_WIDGETS)
   else if (string_is_equal(
            settings->arrays.cheevos_leaderboards_enable, "trackers"))
   {
      rcheevos_locals.leaderboard_notifications = false;
      rcheevos_locals.leaderboard_trackers      = true;
   }
   else if (string_is_equal(
            settings->arrays.cheevos_leaderboards_enable, "notifications"))
   {
      rcheevos_locals.leaderboard_notifications = true;
      rcheevos_locals.leaderboard_trackers      = false;
   }
#endif
   else
   {
      rcheevos_locals.leaderboards_enabled      = false;
      rcheevos_locals.leaderboard_notifications = false;
      rcheevos_locals.leaderboard_trackers      = false;
   }

   if (rcheevos_locals.loaded)
   {
      if (leaderboards_enabled != rcheevos_locals.leaderboards_enabled)
      {
         if (rcheevos_locals.leaderboards_enabled)
            rcheevos_activate_leaderboards(&rcheevos_locals);
         else
            rcheevos_deactivate_leaderboards(&rcheevos_locals);
      }

#if defined(HAVE_GFX_WIDGETS)
      if (!rcheevos_locals.leaderboard_trackers && leaderboard_trackers)
      {
         /* Hide any visible trackers */
         unsigned i;
         rcheevos_ralboard_t* lboard = rcheevos_locals.patchdata.lboards;

         for (i = 0; i < rcheevos_locals.patchdata.lboard_count; ++i, ++lboard)
         {
            if (lboard->mem)
               gfx_widgets_set_leaderboard_display(lboard->id, NULL);
         }
      }
#endif
   }
}

static void rcheevos_toggle_hardcore_active(rcheevos_locals_t* locals)
{
   settings_t* settings = config_get_ptr();
   bool rewind_enable   = settings->bools.rewind_enable;

   if (!locals->hardcore_active)
   {
      /* Activate hardcore */
      locals->hardcore_active = true;

      /* If one or more invalid settings is enabled, abort*/
      rcheevos_validate_config_settings();
      if (!locals->hardcore_active)
         return;

#ifdef HAVE_CHEATS
      /* If one or more emulator managed cheats is active, abort */
      cheat_manager_apply_cheats();
      if (!locals->hardcore_active)
         return;
#endif

      if (locals->loaded)
      {
         const char* msg = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_ENABLE);
         CHEEVOS_LOG("%s\n", msg);
         runloop_msg_queue_push(msg, 0, 3 * 60, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         /* Reactivate leaderboards */
         if (locals->leaderboards_enabled)
            rcheevos_activate_leaderboards(locals);

         /* reset the game */
         command_event(CMD_EVENT_RESET, NULL);
      }

      /* deinit rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         /* have to "schedule" this. 
          * CMD_EVENT_REWIND_DEINIT should 
          * only be called on the main thread */
         rcheevos_locals.queued_command = CMD_EVENT_REWIND_DEINIT;
#else
         command_event(CMD_EVENT_REWIND_DEINIT, NULL);
#endif
      }
   }
   else
   {
      /* pause hardcore */
      locals->hardcore_active = false;

      if (locals->loaded)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Hardcore paused\n");

         /* deactivate leaderboards */
         rcheevos_deactivate_leaderboards(locals);
      }

      /* re-init rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         /* have to "schedule" this. 
          * CMD_EVENT_REWIND_INIT should 
          * only be called on the main thread */
         rcheevos_locals.queued_command = CMD_EVENT_REWIND_INIT;
#else
         command_event(CMD_EVENT_REWIND_INIT, NULL);
#endif
      }
   }

   if (locals->loaded)
   {
      rcheevos_toggle_hardcore_achievements(locals,
            locals->patchdata.core, locals->patchdata.core_count);
      if (settings->bools.cheevos_test_unofficial)
         rcheevos_toggle_hardcore_achievements(locals,
               locals->patchdata.unofficial,
               locals->patchdata.unofficial_count);
   }
}

void rcheevos_toggle_hardcore_paused(void)
{
   settings_t* settings = config_get_ptr();
   /* if hardcore mode is not enabled, we can't toggle it */
   if (settings->bools.cheevos_hardcore_mode_enable)
      rcheevos_toggle_hardcore_active(&rcheevos_locals);
}

void rcheevos_hardcore_enabled_changed(void)
{
   const settings_t* settings = config_get_ptr();
   const bool enabled         = settings 
      && settings->bools.cheevos_enable 
      && settings->bools.cheevos_hardcore_mode_enable;

   if (enabled != rcheevos_locals.hardcore_active)
   {
      rcheevos_toggle_hardcore_active(&rcheevos_locals);

      /* update leaderboard state flags */
      rcheevos_leaderboards_enabled_changed();
   }
}

void rcheevos_validate_config_settings(void)
{
   int i;
   const rc_disallowed_setting_t 
      *disallowed_settings          = NULL;
   core_option_manager_t* coreopts  = NULL;
   struct retro_system_info *system = 
      &runloop_state_get_ptr()->system.info;

   if (!system->library_name || !rcheevos_locals.hardcore_active)
      return;

   if (!(disallowed_settings = rc_libretro_get_disallowed_settings(system->library_name)))
      return;

   if (!rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
      return;

   for (i = 0; i < (int)coreopts->size; i++)
   {
      const char* key = coreopts->opts[i].key;
      const char* val = core_option_manager_get_val(coreopts, i);
      if (!rc_libretro_is_setting_allowed(disallowed_settings, key, val))
      {
         char buffer[256];
         snprintf(buffer, sizeof(buffer), "Hardcore paused. Setting not allowed: %s=%s", key, val);
         CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", buffer);
         rcheevos_pause_hardcore();

         runloop_msg_queue_push(buffer, 0, 4 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);

         break;
      }
   }
}

static void rcheevos_runtime_event_handler(const rc_runtime_event_t* runtime_event)
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
         rcheevos_lboard_updated(rcheevos_find_lboard(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED:
         rcheevos_challenge_started(rcheevos_find_cheevo(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED:
         rcheevos_challenge_ended(rcheevos_find_cheevo(runtime_event->id), runtime_event->value, widgets_ready);
         break;
#endif

      case RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED:
         rcheevos_award_achievement(&rcheevos_locals, rcheevos_find_cheevo(runtime_event->id), widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_STARTED:
         rcheevos_lboard_started(rcheevos_find_lboard(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_CANCELED:
         rcheevos_lboard_canceled(rcheevos_find_lboard(runtime_event->id),
               widgets_ready);
         break;

      case RC_RUNTIME_EVENT_LBOARD_TRIGGERED:
         rcheevos_lboard_submit(&rcheevos_locals, rcheevos_find_lboard(runtime_event->id), runtime_event->value, widgets_ready);
         break;

      case RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED:
         rcheevos_achievement_disabled(rcheevos_find_cheevo(runtime_event->id), runtime_event->value);
         break;

      case RC_RUNTIME_EVENT_LBOARD_DISABLED:
         rcheevos_lboard_disabled(rcheevos_find_lboard(runtime_event->id), runtime_event->value);
         break;

      default:
         break;
   }
}

static int rcheevos_runtime_address_validator(unsigned address)
{
   return (rc_libretro_memory_find(&rcheevos_locals.memory, address) != NULL);
}

static void rcheevos_validate_memrefs(rcheevos_locals_t* locals)
{
   rc_runtime_validate_addresses(&locals->runtime,
         rcheevos_runtime_event_handler, rcheevos_runtime_address_validator);
}

/*****************************************************************************
Test all the achievements (call once per frame).
*****************************************************************************/
void rcheevos_test(void)
{
#ifdef HAVE_THREADS
   if (rcheevos_locals.queued_command != CMD_EVENT_NONE)
   {
      command_event(rcheevos_locals.queued_command, NULL);
      rcheevos_locals.queued_command = CMD_EVENT_NONE;
   }
#endif

   if (!rcheevos_locals.loaded)
      return;

   if (rcheevos_locals.memory.count == 0)
   {
      /* we were unable to initialize memory earlier, try now */
      if (!rcheevos_init_memory(&rcheevos_locals))
      {
         const settings_t* settings    = config_get_ptr();
         rcheevos_locals.core_supports = false;

         CHEEVOS_ERR(RCHEEVOS_TAG "No memory exposed by core\n");

         if (settings && settings->bools.cheevos_verbose_enable)
         {
            runloop_msg_queue_push(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
               0, 4 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
         }

         rcheevos_unload();
         rcheevos_pause_hardcore();
         return;
      }

      rcheevos_validate_memrefs(&rcheevos_locals);
   }

   rc_runtime_do_frame(&rcheevos_locals.runtime, &rcheevos_runtime_event_handler, rcheevos_peek, NULL, 0);
}

size_t rcheevos_get_serialize_size(void)
{
   if (!rcheevos_locals.loaded)
      return 0;
   return rc_runtime_progress_size(&rcheevos_locals.runtime, NULL);
}

bool rcheevos_get_serialized_data(void* buffer)
{
   if (!rcheevos_locals.loaded)
      return false;
   return (rc_runtime_serialize_progress(buffer, &rcheevos_locals.runtime, NULL) == RC_OK);
}

bool rcheevos_set_serialized_data(void* buffer)
{
   if (rcheevos_locals.loaded)
   {
      if (buffer && rc_runtime_deserialize_progress(&rcheevos_locals.runtime, (const unsigned char*)buffer, NULL) == RC_OK)
         return true;

      rc_runtime_reset(&rcheevos_locals.runtime);
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
   return rcheevos_locals.hash;
}

static void rcheevos_unlock_cb(unsigned id, void* userdata)
{
   rcheevos_racheevo_t* cheevo = rcheevos_find_cheevo(id);
   if (cheevo)
   {
      unsigned mode = *(unsigned*)userdata;
#ifndef CHEEVOS_DONT_DEACTIVATE
      cheevo->active &= ~mode;
#endif

      if ((rcheevos_locals.hardcore_active && mode == RCHEEVOS_ACTIVE_HARDCORE) ||
            (!rcheevos_locals.hardcore_active && mode == RCHEEVOS_ACTIVE_SOFTCORE))
      {
         rc_runtime_deactivate_achievement(&rcheevos_locals.runtime, cheevo->id);
         CHEEVOS_LOG(RCHEEVOS_TAG "Achievement %u deactivated: %s\n", id, cheevo->title);
      }
   }
}

#include "coro.h"

/* Uncomment the following two lines to debug rcheevos_iterate, this will
 * disable the coroutine yielding.
 *
 * The code is very easy to understand. It's meant to be like BASIC:
 * CORO_GOTO will jump execution to another label, CORO_GOSUB will
 * call another label, and CORO_RET will return from a CORO_GOSUB.
 *
 * This coroutine code is inspired in a very old pure C implementation
 * that runs everywhere:
 *
 * https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 */
/*#undef CORO_YIELD
#define CORO_YIELD()*/

typedef struct
{
   /* variables used in the co-routine */
   char badge_name[16];
   char url[256];
   char badge_basepath[PATH_MAX_LENGTH];
   char badge_fullpath[PATH_MAX_LENGTH];
   char hash[33];
   unsigned gameid;
   unsigned i;
   unsigned j;
   unsigned k;
   size_t len;
   retro_time_t t0;
   void *data;
   char *json;
   const char *path;
   rcheevos_racheevo_t *cheevo;
   const rcheevos_racheevo_t *cheevo_end;
   settings_t *settings;
   struct http_connection_t *conn;
   struct http_t *http;
   struct rc_hash_iterator iterator;

   /* co-routine required fields */
   CORO_FIELDS
} rcheevos_coro_t;

enum
{
   /* Negative values because CORO_SUB generates positive values */
   RCHEEVOS_GET_GAMEID   = -1,
   RCHEEVOS_GET_CHEEVOS  = -2,
   RCHEEVOS_GET_BADGES   = -3,
   RCHEEVOS_LOGIN        = -4,
   RCHEEVOS_HTTP_GET     = -5,
   RCHEEVOS_DEACTIVATE   = -6,
   RCHEEVOS_PLAYING      = -7,
   RCHEEVOS_DELAY        = -8
};

static int rcheevos_iterate(rcheevos_coro_t* coro)
{
   char buffer[2048];
   bool ret;
#ifdef CHEEVOS_TIME_HASH
   retro_time_t start;
#endif

   CORO_ENTER();

      coro->settings = config_get_ptr();

      /* Bail out if cheevos are disabled.
         * But set the above anyways,
         * command_read_ram needs it. */
      if (!coro->settings->bools.cheevos_enable)
         CORO_STOP();

      /* reset the network error flag */
      rcheevos_locals.network_error = false;
      /* reset the identified game id */
      rcheevos_locals.patchdata.game_id = 0;

      /* iterate over the possible hashes for the file being loaded */
      rc_hash_initialize_iterator(&coro->iterator, coro->path, (uint8_t*)coro->data, coro->len);
#ifdef CHEEVOS_TIME_HASH
      start = cpu_features_get_time_usec();
#endif
      while (rc_hash_iterate(coro->hash, &coro->iterator))
      {
#ifdef CHEEVOS_TIME_HASH
         CHEEVOS_LOG(RCHEEVOS_TAG "hash generated in %ums\n", (cpu_features_get_time_usec() - start) / 1000);
#endif
         CORO_GOSUB(RCHEEVOS_GET_GAMEID);
         if (coro->gameid != 0)
            break;

#ifdef CHEEVOS_TIME_HASH
         start = cpu_features_get_time_usec();
#endif
      }
      rc_hash_destroy_iterator(&coro->iterator);

      /* if no match was found, bail */
      if (coro->gameid == 0)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "this game doesn't feature achievements\n");
         strlcpy(rcheevos_locals.hash, "N/A", sizeof(rcheevos_locals.hash));
         rcheevos_pause_hardcore();
         CORO_STOP();
      }

      /* capture the identified game id in case we bail before fetching the patch data (not logged in) */
      rcheevos_locals.patchdata.game_id = coro->gameid;

#ifdef CHEEVOS_JSON_OVERRIDE
      {
         size_t size = 0;
         FILE *file  = fopen(CHEEVOS_JSON_OVERRIDE, "rb");

         fseek(file, 0, SEEK_END);
         size = ftell(file);
         fseek(file, 0, SEEK_SET);

         coro->json = (char*)malloc(size + 1);
         fread((void*)coro->json, 1, size, file);

         fclose(file);
         coro->json[size] = 0;
      }
#else
      CORO_GOSUB(RCHEEVOS_GET_CHEEVOS);

      if (!coro->json)
      {
         runloop_msg_queue_push("Error loading achievements.", 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         CHEEVOS_ERR(RCHEEVOS_TAG "error loading achievements\n");
         CORO_STOP();
      }
#endif

#ifdef CHEEVOS_SAVE_JSON
      {
         FILE *file = fopen(CHEEVOS_SAVE_JSON, "w");
         fwrite((void*)coro->json, 1, strlen(coro->json), file);
         fclose(file);
      }
#endif

#if HAVE_REWIND
      if (!rcheevos_locals.hardcore_active)
      {
         /* deactivate rewind while we activate the achievements */
         if (coro->settings->bools.rewind_enable)
         {
#ifdef HAVE_THREADS
            /* have to "schedule" this. CMD_EVENT_REWIND_DEINIT should only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_DEINIT;

            /* wait for rewind to be disabled */
            while (rcheevos_locals.queued_command != CMD_EVENT_NONE)
            {
               CORO_YIELD();
            }
#else
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
#endif
         }
      }
#endif

      ret = rcheevos_parse(&rcheevos_locals, coro->json);
      CHEEVOS_FREE(coro->json);

      if (ret == 0)
      {
         if (     rcheevos_locals.patchdata.core_count       == 0
               && rcheevos_locals.patchdata.unofficial_count == 0
               && rcheevos_locals.patchdata.lboard_count     == 0
            )
         {
            runloop_msg_queue_push(
                  "This game has no achievements.",
                  0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            if (rcheevos_locals.patchdata.richpresence_script &&
               *rcheevos_locals.patchdata.richpresence_script)
            {
               rcheevos_locals.loaded = true;
            }
            else
            {
               rcheevos_pause_hardcore();
            }
         }
         else
         {
            rcheevos_locals.loaded = true;
         }
      }

#if HAVE_REWIND
      if (!rcheevos_locals.hardcore_active)
      {
         /* re-enable rewind. if rcheevos_locals.loaded is true, additional space will be allocated
          * for the achievement state data */
         if (coro->settings->bools.rewind_enable)
         {
#ifdef HAVE_THREADS
            /* have to "schedule" this. CMD_EVENT_REWIND_INIT should only be called on the main thread */
            rcheevos_locals.queued_command = CMD_EVENT_REWIND_INIT;
#else
            command_event(CMD_EVENT_REWIND_INIT, NULL);
#endif
         }
      }
#endif

      if (!rcheevos_locals.loaded)
      {
         /* parse failure or no achievements - nothing more to do */
         CORO_STOP();
      }

      /*
         * Inputs:  CHEEVOS_VAR_GAMEID
         * Outputs:
         */
      if (!coro->settings->bools.cheevos_start_active)
         CORO_GOSUB(RCHEEVOS_DEACTIVATE);

      /*
         * Inputs:  CHEEVOS_VAR_GAMEID
         * Outputs:
         */
      CORO_GOSUB(RCHEEVOS_PLAYING);

      if (coro->settings->bools.cheevos_verbose_enable && rcheevos_locals.patchdata.core_count > 0)
      {
         char msg[256];
         int mode                        = RCHEEVOS_ACTIVE_SOFTCORE;
         const rcheevos_racheevo_t* cheevo = rcheevos_locals.patchdata.core;
         const rcheevos_racheevo_t* end    = cheevo + rcheevos_locals.patchdata.core_count;
         int number_of_unlocked          = rcheevos_locals.patchdata.core_count;
         int number_of_unsupported       = 0;

         if (rcheevos_locals.hardcore_active)
            mode = RCHEEVOS_ACTIVE_HARDCORE;

         for (; cheevo < end; cheevo++)
         {
            if (!cheevo->memaddr)
               number_of_unsupported++;
            else if (cheevo->active & mode)
               number_of_unlocked--;
         }

         if (!number_of_unsupported)
         {
            if (coro->settings->bools.cheevos_start_active)
            {
               snprintf(msg, sizeof(msg),
                  "All %d achievements activated for this session.",
                  rcheevos_locals.patchdata.core_count);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg);
            }
            else
            {
               snprintf(msg, sizeof(msg),
                  "You have %d of %d achievements unlocked.",
                  number_of_unlocked, rcheevos_locals.patchdata.core_count);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", &msg[9]);
            }
         }
         else
         {
            if (coro->settings->bools.cheevos_start_active)
            {
               snprintf(msg, sizeof(msg),
                  "All %d achievements activated for this session (%d unsupported).",
                  rcheevos_locals.patchdata.core_count,
                  number_of_unsupported);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", msg);
            }
            else
            {
               snprintf(msg, sizeof(msg),
                     "You have %d of %d achievements unlocked (%d unsupported).",
                     number_of_unlocked - number_of_unsupported,
                     rcheevos_locals.patchdata.core_count,
                     number_of_unsupported);
               CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", &msg[9]);
            }
         }

         msg[sizeof(msg) - 1] = 0;
         runloop_msg_queue_push(msg, 0, 3 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      CORO_GOSUB(RCHEEVOS_GET_BADGES);
      CORO_STOP();


   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  coro->hash
    * Outputs coro->gameid
    *************************************************************************/
   CORO_SUB(RCHEEVOS_GET_GAMEID)

      {
         int size;

         CHEEVOS_LOG(RCHEEVOS_TAG "checking %s\n", coro->hash);
         memcpy(rcheevos_locals.hash, coro->hash, sizeof(coro->hash));

         size = rc_url_get_gameid(coro->url, sizeof(coro->url), rcheevos_locals.hash);
         if (size < 0)
         {
            CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
            CORO_RET();
         }

         rcheevos_log_url("rc_url_get_gameid", coro->url);
         CORO_GOSUB(RCHEEVOS_HTTP_GET);

         if (!coro->json)
            CORO_RET();

         coro->gameid = chevos_get_gameid(coro->json);

         CHEEVOS_FREE(coro->json);
         CHEEVOS_LOG(RCHEEVOS_TAG "got game id %u\n", coro->gameid);
         CORO_RET();
      }


   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
   CORO_SUB(RCHEEVOS_GET_CHEEVOS)
   {
      int ret;

      CORO_GOSUB(RCHEEVOS_LOGIN);

      ret = rc_url_get_patch(coro->url, sizeof(coro->url), rcheevos_locals.username, rcheevos_locals.token, coro->gameid);

      if (ret < 0)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
         CORO_STOP();
      }

      rcheevos_log_url("rc_url_get_patch", coro->url);
      CORO_GOSUB(RCHEEVOS_HTTP_GET);

      if (!coro->json)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "error getting achievements for game id %u\n", coro->gameid);
         CORO_STOP();
      }

      CHEEVOS_LOG(RCHEEVOS_TAG "got achievements for game id %u\n", coro->gameid);
      CORO_RET();
   }


   /**************************************************************************
    * Info    Gets the achievements from Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
   CORO_SUB(RCHEEVOS_GET_BADGES)

   /* we always want badges if display widgets are enabled */
#if !defined(HAVE_GFX_WIDGETS)
   {
      settings_t *settings = config_get_ptr();
      if (!(
               string_is_equal(settings->arrays.menu_driver, "xmb") ||
               string_is_equal(settings->arrays.menu_driver, "ozone")
           ) ||
            !settings->bools.cheevos_badges_enable)
         CORO_RET();
   }
#endif

      /* make sure the directory exists */
      coro->badge_fullpath[0] = '\0';
      fill_pathname_application_special(coro->badge_fullpath,
         sizeof(coro->badge_fullpath),
         APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

      if (!path_is_directory(coro->badge_fullpath))
         path_mkdir(coro->badge_fullpath);

      /* fetch the placeholder image */
      strlcpy(coro->badge_name, "00000" FILE_PATH_PNG_EXTENSION,
            sizeof(coro->badge_name));
      fill_pathname_join(coro->badge_fullpath, coro->badge_fullpath,
         coro->badge_name, sizeof(coro->badge_fullpath));

      if (!path_is_valid(coro->badge_fullpath))
      {
#ifdef CHEEVOS_LOG_BADGES
         CHEEVOS_LOG(RCHEEVOS_TAG "downloading badge %s\n",
            coro->badge_fullpath);
#endif
         snprintf(coro->url, sizeof(coro->url),
            FILE_PATH_RETROACHIEVEMENTS_URL "/Badge/%s", coro->badge_name);

         CORO_GOSUB(RCHEEVOS_HTTP_GET);

         if (coro->json)
         {
            if (!filestream_write_file(coro->badge_fullpath, coro->json, coro->k))
               CHEEVOS_ERR(RCHEEVOS_TAG "Error writing badge %s\n", coro->badge_fullpath);

            CHEEVOS_FREE(coro->json);
            coro->json = NULL;
         }
      }

      /* fetch the game images */
      for (coro->i = 0; coro->i < 2; coro->i++)
      {
         if (coro->i == 0)
         {
            coro->cheevo     = rcheevos_locals.patchdata.core;
            coro->cheevo_end = coro->cheevo + rcheevos_locals.patchdata.core_count;
         }
         else
         {
            coro->cheevo     = rcheevos_locals.patchdata.unofficial;
            coro->cheevo_end = coro->cheevo + rcheevos_locals.patchdata.unofficial_count;
         }

         for (; coro->cheevo < coro->cheevo_end; coro->cheevo++)
         {
            if (!coro->cheevo->badge || !coro->cheevo->badge[0])
               continue;

            for (coro->j = 0 ; coro->j < 2; coro->j++)
            {
               CORO_YIELD();

               if (coro->j == 0)
                  snprintf(coro->badge_name,
                        sizeof(coro->badge_name),
                        "%s" FILE_PATH_PNG_EXTENSION,
                        coro->cheevo->badge);
               else
                  snprintf(coro->badge_name,
                        sizeof(coro->badge_name),
                        "%s_lock" FILE_PATH_PNG_EXTENSION,
                        coro->cheevo->badge);

               coro->badge_fullpath[0] = '\0';
               fill_pathname_application_special(coro->badge_fullpath,
                     sizeof(coro->badge_fullpath),
                     APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

               fill_pathname_join(
                     coro->badge_fullpath,
                     coro->badge_fullpath,
                     coro->badge_name,
                     sizeof(coro->badge_fullpath));

               if (!path_is_valid(coro->badge_fullpath))
               {
#ifdef CHEEVOS_LOG_BADGES
                  CHEEVOS_LOG(
                        RCHEEVOS_TAG "downloading badge %s\n",
                        coro->badge_fullpath);
#endif
                  snprintf(coro->url,
                        sizeof(coro->url),
                        FILE_PATH_RETROACHIEVEMENTS_URL "/Badge/%s",
                        coro->badge_name);

                  CORO_GOSUB(RCHEEVOS_HTTP_GET);

                  if (coro->json)
                  {
                     if (!filestream_write_file(coro->badge_fullpath,
                              coro->json, coro->k))
                        CHEEVOS_ERR(RCHEEVOS_TAG "Error writing badge %s\n", coro->badge_fullpath);

                     CHEEVOS_FREE(coro->json);
                     coro->json = NULL;
                  }
               }
            }
         }
      }

      CORO_RET();


   /**************************************************************************
    * Info Logs in the user at Retro Achievements
    *************************************************************************/
   CORO_SUB(RCHEEVOS_LOGIN)
   {
      int ret;
      char tok[256];

      if (rcheevos_locals.token[0])
         CORO_RET();

      if (string_is_empty(coro->settings->arrays.cheevos_username))
      {
         runloop_msg_queue_push(
               "Missing RetroAchievements account information.",
               0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         runloop_msg_queue_push(
               "Please fill in your account information in Settings.",
               0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         CHEEVOS_ERR(RCHEEVOS_TAG "login info not informed\n");
         CORO_STOP();
      }

      if (string_is_empty(coro->settings->arrays.cheevos_token))
      {
         ret = rc_url_login_with_password(coro->url, sizeof(coro->url),
               coro->settings->arrays.cheevos_username,
               coro->settings->arrays.cheevos_password);

         if (ret == RC_OK)
         {
            CHEEVOS_LOG(RCHEEVOS_TAG "attempting to login %s (with password)\n", coro->settings->arrays.cheevos_username);
            rcheevos_log_url("rc_url_login_with_password", coro->url);
         }
      }
      else
      {
         ret = rc_url_login_with_token(coro->url, sizeof(coro->url),
               coro->settings->arrays.cheevos_username,
               coro->settings->arrays.cheevos_token);

         if (ret == RC_OK)
         {
            CHEEVOS_LOG(RCHEEVOS_TAG "attempting to login %s (with token)\n", coro->settings->arrays.cheevos_username);
            rcheevos_log_url("rc_url_login_with_token", coro->url);
         }
      }

      if (ret < 0)
      {
         CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
         CORO_STOP();
      }

      CORO_GOSUB(RCHEEVOS_HTTP_GET);

      if (!coro->json)
      {
         runloop_msg_queue_push("RetroAchievements: Error contacting server.", 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         CHEEVOS_ERR(RCHEEVOS_TAG "error getting user token\n");

         CORO_STOP();
      }

      ret = rcheevos_get_token(coro->json,
            rcheevos_locals.username, sizeof(rcheevos_locals.username),
            tok, sizeof(tok));

      if (ret != 0)
      {
         char msg[512];
         snprintf(msg, sizeof(msg),
               "RetroAchievements: %s",
               tok);
         runloop_msg_queue_push(msg, 0, 5 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         *coro->settings->arrays.cheevos_token = 0;
         CHEEVOS_ERR(RCHEEVOS_TAG "login error: %s\n", tok);

         CHEEVOS_FREE(coro->json);
         CORO_STOP();
      }

      CHEEVOS_FREE(coro->json);

      if (coro->settings->bools.cheevos_verbose_enable)
      {
         char msg[256];
         snprintf(msg, sizeof(msg),
               "RetroAchievements: Logged in as \"%s\".",
               rcheevos_locals.username);
         msg[sizeof(msg) - 1] = 0;
         runloop_msg_queue_push(msg, 0, 2 * 60, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      CHEEVOS_LOG(RCHEEVOS_TAG "%s logged in successfully\n", rcheevos_locals.username);
      strlcpy(rcheevos_locals.token, tok,
            sizeof(rcheevos_locals.token));

      /* Save token to config and clear pass on success */
      strlcpy(coro->settings->arrays.cheevos_token, tok,
            sizeof(coro->settings->arrays.cheevos_token));

      *coro->settings->arrays.cheevos_password = 0;
      CORO_RET();
   }


   /**************************************************************************
    * Info    Pauses execution for five seconds
    *************************************************************************/
   CORO_SUB(RCHEEVOS_DELAY)

      {
         retro_time_t t1;
         coro->t0         = cpu_features_get_time_usec();

         do
         {
            CORO_YIELD();
            t1 = cpu_features_get_time_usec();
         } while((t1 - coro->t0) < 3000000);
      }

      CORO_RET();


   /**************************************************************************
    * Info    Makes a HTTP GET request
    * Inputs  CHEEVOS_VAR_URL
    * Outputs CHEEVOS_VAR_JSON
    *************************************************************************/
   CORO_SUB(RCHEEVOS_HTTP_GET)

      for (coro->k = 0; coro->k < 5; coro->k++)
      {
         if (coro->k != 0)
            CHEEVOS_LOG(RCHEEVOS_TAG "Retrying HTTP request: %u of 5\n", coro->k + 1);

         coro->json       = NULL;
         coro->conn       = net_http_connection_new(
               coro->url, "GET", NULL);

         if (!coro->conn)
         {
            CORO_GOSUB(RCHEEVOS_DELAY);
            continue;
         }

         /* Don't bother with timeouts here, it's just a string scan. */
         while (!net_http_connection_iterate(coro->conn)) {}

         /* Error finishing the connection descriptor. */
         if (!net_http_connection_done(coro->conn))
         {
            net_http_connection_free(coro->conn);
            continue;
         }

         rcheevos_get_user_agent(&rcheevos_locals,
               buffer, sizeof(buffer));
         net_http_connection_set_user_agent(coro->conn, buffer);

         coro->http = net_http_new(coro->conn);

         /* Error connecting to the endpoint. */
         if (!coro->http)
         {
            net_http_connection_free(coro->conn);
            CORO_GOSUB(RCHEEVOS_DELAY);
            continue;
         }

         while (!net_http_update(coro->http, NULL, NULL))
            CORO_YIELD();

         {
            size_t length;
            uint8_t *data = net_http_data(coro->http,
                  &length, false);

            if (data)
            {
               coro->json = (char*)malloc(length + 1);

               if (coro->json)
               {
                  memcpy((void*)coro->json, (void*)data, length);
                  CHEEVOS_FREE(data);
                  coro->json[length] = 0;
               }

               coro->k = (unsigned)length;
               net_http_delete(coro->http);
               net_http_connection_free(coro->conn);
               CORO_RET();
            }
         }

         net_http_delete(coro->http);
         net_http_connection_free(coro->conn);
      }

      CHEEVOS_LOG(RCHEEVOS_TAG "Couldn't connect to server after 5 tries\n");
      rcheevos_locals.network_error = true;
      CORO_RET();


   /**************************************************************************
    * Info    Deactivates the achievements already awarded
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs
    *************************************************************************/
   CORO_SUB(RCHEEVOS_DEACTIVATE)

      CORO_GOSUB(RCHEEVOS_LOGIN);
      {
         int ret;
         unsigned mode;

         /* Two calls - one for softcore and one for hardcore */
         for (coro->i = 0; coro->i < 2; coro->i++)
         {
            ret = rc_url_get_unlock_list(coro->url, sizeof(coro->url),
                  rcheevos_locals.username,
                  rcheevos_locals.token, coro->gameid, coro->i);

            if (ret < 0)
            {
               CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
               CORO_STOP();
            }

            rcheevos_log_url("rc_url_get_unlock_list", coro->url);
            CORO_GOSUB(RCHEEVOS_HTTP_GET);

            if (coro->json)
            {
               mode = coro->i == 0 ? RCHEEVOS_ACTIVE_SOFTCORE : RCHEEVOS_ACTIVE_HARDCORE;
               rcheevos_deactivate_unlocks(coro->json, rcheevos_unlock_cb, &mode);
               CHEEVOS_FREE(coro->json);
            }
            else
               CHEEVOS_ERR(RCHEEVOS_TAG "error retrieving list of unlocked achievements in softcore mode\n");
         }
      }

      CORO_RET();


   /**************************************************************************
    * Info    Posts the "playing" activity to Retro Achievements
    * Inputs  CHEEVOS_VAR_GAMEID
    * Outputs
    *************************************************************************/
   CORO_SUB(RCHEEVOS_PLAYING)

      {
         int ret = rc_url_post_playing(coro->url, sizeof(coro->url),
            rcheevos_locals.username,
            rcheevos_locals.token, coro->gameid);

         if (ret < 0)
         {
            CHEEVOS_ERR(RCHEEVOS_TAG "buffer too small to create URL\n");
            CORO_STOP();
         }
      }

      rcheevos_log_url("rc_url_post_playing", coro->url);

      CORO_GOSUB(RCHEEVOS_HTTP_GET);

      if (coro->json)
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Posted playing activity\n");
         CHEEVOS_FREE(coro->json);
      }
      else
         CHEEVOS_ERR(RCHEEVOS_TAG "error posting playing activity\n");

      CORO_RET();

   CORO_LEAVE();
}

static void rcheevos_task_handler(retro_task_t *task)
{
   rcheevos_coro_t *coro = (rcheevos_coro_t*)task->state;

   if (!coro)
      return;

   if (!rcheevos_iterate(coro) || task_get_cancelled(task))
   {
      task_set_finished(task, true);

      CHEEVOS_LOCK(rcheevos_locals.task_lock);
      rcheevos_locals.task = NULL;
      CHEEVOS_UNLOCK(rcheevos_locals.task_lock);

      if (task_get_cancelled(task))
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Load task cancelled\n");
      }
      else
      {
         CHEEVOS_LOG(RCHEEVOS_TAG "Load task finished\n");
      }

      CHEEVOS_FREE(coro->data);
      CHEEVOS_FREE(coro->path);
      CHEEVOS_FREE(coro);
   }
}

/* hooks for rc_hash library */

static void* rc_hash_handle_file_open(const char* path)
{
   return intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void rc_hash_handle_file_seek(void* file_handle, int64_t offset, int origin)
{
   intfstream_seek((intfstream_t*)file_handle, offset, origin);
}

static int64_t rc_hash_handle_file_tell(void* file_handle)
{
   return intfstream_tell((intfstream_t*)file_handle);
}

static size_t rc_hash_handle_file_read(void* file_handle, void* buffer, size_t requested_bytes)
{
   return intfstream_read((intfstream_t*)file_handle, buffer, requested_bytes);
}

static void rc_hash_handle_file_close(void* file_handle)
{
   intfstream_close((intfstream_t*)file_handle);
   CHEEVOS_FREE(file_handle);
}

static void* rc_hash_handle_cd_open_track(const char* path, uint32_t track)
{
   cdfs_track_t* cdfs_track;

   switch (track)
   {
      case RC_HASH_CDTRACK_FIRST_DATA:
         cdfs_track = cdfs_open_data_track(path);
         break;

      case RC_HASH_CDTRACK_LAST:
#ifdef HAVE_CHD
         if (string_is_equal_noncase(path_get_extension(path), "chd"))
         {
            cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_LAST);
            break;
         }
#endif
         CHEEVOS_LOG(RCHEEVOS_TAG "Last track only supported for CHD\n");
         cdfs_track = NULL;
         break;

      case RC_HASH_CDTRACK_LARGEST:
#ifdef HAVE_CHD
         if (string_is_equal_noncase(path_get_extension(path), "chd"))
         {
            cdfs_track = cdfs_open_track(path, CHDSTREAM_TRACK_PRIMARY);
            break;
         }
#endif
         CHEEVOS_LOG(RCHEEVOS_TAG "Largest track only supported for CHD, using first data track\n");
         cdfs_track = cdfs_open_data_track(path);
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

static size_t rc_hash_handle_cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
   cdfs_file_t* file = (cdfs_file_t*)track_handle;

   cdfs_seek_sector(file, sector);
   return cdfs_read_file(file, buffer, requested_bytes);
}

static void rc_hash_handle_cd_close_track(void* track_handle)
{
   cdfs_file_t* file = (cdfs_file_t*)track_handle;
   if (file)
   {
      cdfs_close_track(file->track);
      cdfs_close_file(file); /* ASSERT: this does not free() file */
      CHEEVOS_FREE(file);
   }
}

/* end hooks */

bool rcheevos_load(const void *data)
{
   retro_task_t *task                 = NULL;
   const struct retro_game_info *info = NULL;
   rcheevos_coro_t *coro              = NULL;
   settings_t *settings               = config_get_ptr();
   bool cheevos_enable                = settings && settings->bools.cheevos_enable;
   struct rc_hash_filereader filereader;
   struct rc_hash_cdreader cdreader;

   rcheevos_locals.loaded             = false;
#ifdef HAVE_THREADS
   rcheevos_locals.queued_command     = CMD_EVENT_NONE;
#endif
   rc_runtime_init(&rcheevos_locals.runtime);

   if (!cheevos_enable || !rcheevos_locals.core_supports || !data)
   {
      rcheevos_pause_hardcore();
      return false;
   }

   /* reset hardcore mode and leaderboard settings based on configs */
   rcheevos_hardcore_enabled_changed();
   rcheevos_validate_config_settings();
   rcheevos_leaderboards_enabled_changed();

   coro = (rcheevos_coro_t*)calloc(1, sizeof(*coro));

   if (!coro)
      return false;

   /* provide hooks for reading files */
   memset(&filereader, 0, sizeof(filereader));
   filereader.open = rc_hash_handle_file_open;
   filereader.seek = rc_hash_handle_file_seek;
   filereader.tell = rc_hash_handle_file_tell;
   filereader.read = rc_hash_handle_file_read;
   filereader.close = rc_hash_handle_file_close;
   rc_hash_init_custom_filereader(&filereader);

   memset(&cdreader, 0, sizeof(cdreader));
   cdreader.open_track = rc_hash_handle_cd_open_track;
   cdreader.read_sector = rc_hash_handle_cd_read_sector;
   cdreader.close_track = rc_hash_handle_cd_close_track;
   rc_hash_init_custom_cdreader(&cdreader);

   rc_hash_init_error_message_callback(rcheevos_handle_log_message);

#ifndef DEBUG /* in DEBUG mode, always initialize the verbose message handler */
   if (settings->bools.cheevos_verbose_enable)
#endif
   {
      rc_hash_init_verbose_message_callback(rcheevos_handle_log_message);
   }

   task = task_init();
   if (!task)
   {
      CHEEVOS_FREE(coro);
      return false;
   }

   CORO_SETUP();

   info = (const struct retro_game_info*)data;
   coro->path = strdup(info->path);

   if (info->data)
   {
      coro->len = info->size;

      /* size limit */
      if (coro->len > CHEEVOS_MB(64))
         coro->len = CHEEVOS_MB(64);

      coro->data = malloc(coro->len);

      if (!coro->data)
      {
         CHEEVOS_FREE(task);
         CHEEVOS_FREE(coro);
         return false;
      }

      memcpy(coro->data, info->data, coro->len);
   }
   else
   {
      coro->data       = NULL;
   }

   task->handler   = rcheevos_task_handler;
   task->state     = (void*)coro;
   task->mute      = true;
   task->callback  = NULL;
   task->user_data = NULL;
   task->progress  = 0;
   task->title     = NULL;

#ifdef HAVE_THREADS
   if (!rcheevos_locals.task_lock)
      rcheevos_locals.task_lock = slock_new();
#endif

   CHEEVOS_LOCK(rcheevos_locals.task_lock);
   rcheevos_locals.task = task;
   CHEEVOS_UNLOCK(rcheevos_locals.task_lock);

   task_queue_push(task);
   return true;
}
