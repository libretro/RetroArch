/*  RetroArch - A frontend for libretro.
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

#include <features/features_cpu.h>
#include <retro_assert.h>

#include "cheevos_locals.h"
#include "cheevos_client.h"

#include "../gfx/gfx_display.h"
#include "../file_path_special.h"

#include "cheevos.h"

#include "../deps/rcheevos/include/rc_runtime_types.h"
#include "../deps/rcheevos/include/rc_api_runtime.h"
#include "../deps/rcheevos/src/rc_client_internal.h"

#if HAVE_MENU

#include "../menu/menu_driver.h"
#include "../menu/menu_entries.h"

#endif

#include <features/features_cpu.h>
#include <retro_assert.h>

 /* if menu_badge_grayscale is set to a value other than 1 or 0, it's a counter for the number of
  * frames since the last time we checked for the file. When the counter reaches this value, we'll
  * check for the file again. */
#define MENU_BADGE_RETRY_RELOAD_FRAMES 64

#ifdef HAVE_RC_CLIENT

#if HAVE_MENU

bool rcheevos_menu_get_state(unsigned menu_offset, char* buffer, size_t buffer_size)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (menu_offset < rcheevos_locals->menuitem_count)
   {
      const rcheevos_menuitem_t* menuitem = &rcheevos_locals->menuitems[menu_offset];
      const rc_client_achievement_t* cheevo = menuitem->achievement;
      if (cheevo)
      {
         if (cheevo->state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
            strlcpy(buffer, msg_hash_to_str(menuitem->state_label_idx), buffer_size);
         else
         {
            const char* missable = cheevo->type == RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE ? "[m] " : "";
            size_t _len = strlcpy(buffer, missable, buffer_size);
            _len += strlcpy(buffer + _len, msg_hash_to_str(menuitem->state_label_idx), buffer_size - _len);
            if (cheevo->measured_progress[0])
            {
               _len += strlcpy(buffer + _len, " - ", buffer_size - _len);
               strlcpy(buffer + _len, cheevo->measured_progress, buffer_size - _len);
            }
         }
         return true;
      }
   }

   if (buffer)
      buffer[0] = '\0';

   return false;
}

bool rcheevos_menu_get_sublabel(unsigned menu_offset, char* buffer, size_t buffer_size)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (menu_offset < rcheevos_locals->menuitem_count && buffer)
   {
      const rcheevos_menuitem_t* menuitem = &rcheevos_locals->menuitems[menu_offset];
      if (menuitem->achievement)
      {
         strlcpy(buffer, menuitem->achievement->description, buffer_size);
         return true;
      }
   }

   if (buffer)
      buffer[0] = '\0';

   return false;
}

void rcheevos_menu_reset_badges(void)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rcheevos_menuitem_t* menuitem = rcheevos_locals->menuitems;
   rcheevos_menuitem_t* stop = menuitem + rcheevos_locals->menuitem_count;

   while (menuitem < stop)
   {
      if (menuitem->menu_badge_texture)
      {
         video_driver_texture_unload(&menuitem->menu_badge_texture);
         menuitem->menu_badge_texture = 0;
         menuitem->menu_badge_grayscale = MENU_BADGE_RETRY_RELOAD_FRAMES;
      }
      ++menuitem;
   }
}

static rcheevos_menuitem_t* rcheevos_menu_allocate(
   rcheevos_locals_t* rcheevos_locals)
{
   rcheevos_menuitem_t* menuitem;

   if (rcheevos_locals->menuitem_count == rcheevos_locals->menuitem_capacity)
   {
      if (rcheevos_locals->menuitems)
      {
         rcheevos_menuitem_t* new_menuitems;
         rcheevos_locals->menuitem_capacity += 32;
         new_menuitems = (rcheevos_menuitem_t*)realloc(rcheevos_locals->menuitems,
            rcheevos_locals->menuitem_capacity * sizeof(rcheevos_menuitem_t));

         if (new_menuitems)
            rcheevos_locals->menuitems = new_menuitems;
         else
         {
            /* realloc failed */
            CHEEVOS_ERR(RCHEEVOS_TAG " could not allocate space for %u menu items\n",
               rcheevos_locals->menuitem_capacity);
            rcheevos_locals->menuitem_capacity -= 32;
            return NULL;
         }
      }
      else
      {
         rcheevos_locals->menuitem_capacity = 64;
         rcheevos_locals->menuitems = (rcheevos_menuitem_t*)
            malloc(rcheevos_locals->menuitem_capacity * sizeof(rcheevos_menuitem_t));

         if (!rcheevos_locals->menuitems)
         {
            /* malloc failed */
            CHEEVOS_ERR(RCHEEVOS_TAG " could not allocate space for %u menu items\n",
               rcheevos_locals->menuitem_capacity);
            rcheevos_locals->menuitem_capacity = 0;
            return NULL;
         }
      }
   }

   menuitem = &rcheevos_locals->menuitems[rcheevos_locals->menuitem_count++];
   memset(menuitem, 0, sizeof(*menuitem));
   return menuitem;
}

static void rcheevos_menu_append_header(rcheevos_locals_t* rcheevos_locals,
   enum msg_hash_enums label, uint32_t subset_id)
{
   rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals);
   if (menuitem)
   {
      menuitem->state_label_idx = label;
      menuitem->subset_id = subset_id;
   }
}

static void rcheevos_menu_update_badge(rcheevos_menuitem_t* menuitem, bool download_if_missing)
{
   const char* badge_name = "00000";
   bool badge_grayscale = false;

   if (menuitem->achievement)
      badge_name = menuitem->achievement->badge_name;

   switch (menuitem->state_label_idx)
   {
   case MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY:
   case MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY:
   case MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY:
   case MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY:
   case MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY:
      badge_grayscale = true;
      break;

   default:
      badge_grayscale = false;
      break;
   }

   if (!menuitem->menu_badge_texture || menuitem->menu_badge_grayscale != badge_grayscale)
   {
      uintptr_t new_badge_texture =
         rcheevos_get_badge_texture(badge_name, badge_grayscale, download_if_missing);

      if (new_badge_texture)
      {
         if (menuitem->menu_badge_texture)
            video_driver_texture_unload(&menuitem->menu_badge_texture);

         menuitem->menu_badge_texture = new_badge_texture;
         menuitem->menu_badge_grayscale = badge_grayscale;
      }
      /* menu_badge_grayscale is overloaded such
       * that any value greater than 1 indicates
       * the server default image is being used */
      else if (menuitem->menu_badge_grayscale < 2)
      {
         if (menuitem->menu_badge_texture)
            video_driver_texture_unload(&menuitem->menu_badge_texture);

         /* requested badge is not available, check for server default */
         menuitem->menu_badge_texture =
            rcheevos_get_badge_texture("00000", false, false);

         if (menuitem->menu_badge_texture)
            menuitem->menu_badge_grayscale = 2;
      }
   }
}

uintptr_t rcheevos_menu_get_badge_texture(unsigned menu_offset)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (menu_offset < rcheevos_locals->menuitem_count)
   {
      rcheevos_menuitem_t* menuitem = &rcheevos_locals->menuitems[menu_offset];

      /* if we're using the placeholder badge, check to see if the real badge
         * has become available (do this roughly once a second) */
      if (menuitem->menu_badge_grayscale >= 2)
      {
         if (++menuitem->menu_badge_grayscale >= MENU_BADGE_RETRY_RELOAD_FRAMES)
         {
            menuitem->menu_badge_grayscale = 2;
            rcheevos_menu_update_badge(menuitem, false);
         }
      }

      return menuitem->menu_badge_texture;
   }

   return 0;
}

void rcheevos_menu_populate_hardcore_pause_submenu(void* data)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   menu_displaylist_info_t* info = (menu_displaylist_info_t*)data;
   const settings_t* settings = config_get_ptr();
   const bool cheevos_hardcore_mode_enable = settings->bools.cheevos_hardcore_mode_enable;

   if (cheevos_hardcore_mode_enable && rc_client_get_game_info(rcheevos_locals->client))
   {
      if (rc_client_get_hardcore_enabled(rcheevos_locals->client))
      {
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL),
            MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL,
            MENU_SETTING_ACTION_CLOSE, 0, 0, NULL);
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE),
            MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE,
            MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0, NULL);
      }
      else
      {
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL),
            MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL,
            MENU_SETTING_ACTION_CLOSE, 0, 0, NULL);
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_RESUME),
            MENU_ENUM_LABEL_ACHIEVEMENT_RESUME,
            MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0, NULL);
      }
   }
}

void rcheevos_menu_populate(void* data)
{
   menu_displaylist_info_t* info = (menu_displaylist_info_t*)data;
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals->client);
   const settings_t* settings = config_get_ptr();

   rc_client_achievement_list_t* list = rc_client_create_achievement_list(rcheevos_locals->client,
      RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
      RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
   uint32_t i, j;

   rcheevos_menu_reset_badges();
   rcheevos_locals->menuitem_count = 0;

   if (rcheevos_locals->client->state.disconnect)
   {
      menu_entries_append(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE),
         msg_hash_to_str(MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE),
         MENU_ENUM_LABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
         MENU_INFO_ACHIEVEMENTS_SERVER_UNREACHABLE, 0, 0, NULL);
   }

   if (game && game->id != 0)
   {
      /* first menu item is the Pause/Resume Hardcore option (unless hardcore is completely disabled) */
      if (settings->bools.cheevos_enable && settings->bools.cheevos_hardcore_mode_enable)
      {
         if (rc_client_get_hardcore_enabled(rcheevos_locals->client))
            menu_entries_append(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU,
               MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0, NULL);
         else
            menu_entries_append(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU,
               MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0, NULL);
      }
   }

   for (i = 0; i < list->num_buckets; i++)
   {
      if (list->num_buckets > 1)
      {
         enum msg_hash_enums label;
         switch (list->buckets[i].bucket_type)
         {
         case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED:
            label = MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED:
            label = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED:
            label = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL:
            label = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
            label = MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
            label = MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE:
            label = MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY;
            break;
         default:
            continue;
         }
         rcheevos_menu_append_header(rcheevos_locals, label, list->buckets[i].subset_id);
      }

      for (j = 0; j < list->buckets[i].num_achievements; j++)
      {
         rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals);
         if (!menuitem)
            break;

         menuitem->achievement = list->buckets[i].achievements[j];

         switch (list->buckets[i].bucket_type)
         {
         case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
         case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED:
            if (menuitem->achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE)
               menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE;
            else
               menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED:
            menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY;
            break;
         case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL:
            menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY;
            break;
         default:
            menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY;
            break;
         }

         rcheevos_menu_update_badge(menuitem, true);
      }
   }

   rc_client_destroy_achievement_list(list);

   if (rcheevos_locals->menuitem_count > 0)
   {
      char buffer[128];
      unsigned idx = 0;
      /* convert to menu entries */
      rcheevos_menuitem_t* menuitem = rcheevos_locals->menuitems;
      rcheevos_menuitem_t* stop = menuitem +
         rcheevos_locals->menuitem_count;

      do
      {
         if (menuitem->achievement)
         {
            menu_entries_append(info->list, menuitem->achievement->title,
               menuitem->achievement->description,
               MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + idx, 0, 0, NULL);
         }
         else
         {
            if (menuitem->subset_id)
            {
               const rc_client_subset_t* subset =
                  rc_client_get_subset_info(rcheevos_locals->client, menuitem->subset_id);

               snprintf(buffer, sizeof(buffer), "----- %s - %s -----",
                  subset ? subset->title : "Unknown Subset",
                  msg_hash_to_str(menuitem->state_label_idx));
            }
            else
               snprintf(buffer, sizeof(buffer), "----- %s -----",
                  msg_hash_to_str(menuitem->state_label_idx));

            menu_entries_append(info->list, buffer, "",
               MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
               MENU_SETTINGS_CHEEVOS_START + idx, 0, 0, NULL);
         }

         ++idx;
         ++menuitem;
      } while (menuitem != stop);
   }
   else
   {
      /* no achievements found */
      if (!rcheevos_locals->core_supports)
      {
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
            MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
            FILE_TYPE_NONE, 0, 0, NULL);
      }
      else if (!game)
      {
         int state = rc_client_get_load_game_state(rcheevos_locals->client);
         enum msg_hash_enums msg = MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME;
         switch (state)
         {
         case RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME:
            msg = MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME;
            break;
         case RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN:
            msg = MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN;
            break;
         case RC_CLIENT_LOAD_GAME_STATE_FETCHING_GAME_DATA:
            msg = MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA;
            break;
         case RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION:
            msg = MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION;
            break;
         case RC_CLIENT_LOAD_GAME_STATE_NONE:
            if (!rc_client_get_user_info(rcheevos_locals->client))
               msg = MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN;
            break;
         }

         menu_entries_append(info->list,
            msg_hash_to_str(msg),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0, NULL);
      }
      else if (!game->id)
      {
         char buffer[128];
         snprintf(buffer, sizeof(buffer), "%s (%s)",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME), game->hash);

         menu_entries_append(info->list,
            buffer,
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0, NULL);
      }
      else if (!rc_client_get_user_info(rcheevos_locals->client))
      {
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN),
            msg_hash_to_str(MENU_ENUM_LABEL_NOT_LOGGED_IN),
            MENU_ENUM_LABEL_NOT_LOGGED_IN,
            FILE_TYPE_NONE, 0, 0, NULL);
      }
      else
      {
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0, NULL);
      }
   }
}

#endif /* HAVE_MENU */

uintptr_t rcheevos_get_badge_texture(const char* badge, bool locked, bool download_if_missing)
{
   size_t _len;
   char badge_file[24];
   char fullpath[PATH_MAX_LENGTH];
   uintptr_t tex = 0;

   if (!badge || !badge[0])
      return 0;

#ifdef HAVE_THREADS
   /* The OpenGL driver crashes if gfx_display_reset_textures_list is not called on the video thread.
    * If threaded video is enabled, it'll automatically dispatch the request to the video thread.
    * If threaded video is not enabled, just return null. The video thread should assume the image
    * wasn't downloaded and check again in a few frames.
    */
   if (!video_driver_is_threaded() && !task_is_on_main_thread())
      return 0;
#endif

   _len  = strlcpy(badge_file, badge, sizeof(badge_file));
   _len += strlcpy(badge_file + _len, locked ? "_lock" : "", sizeof(badge_file) - _len);
   strlcpy(badge_file + _len, FILE_PATH_PNG_EXTENSION, sizeof(badge_file) - _len);

   fill_pathname_application_special(fullpath, sizeof(fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

   if (!gfx_display_reset_textures_list(badge_file, fullpath,
      &tex, TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
   {
      if (download_if_missing)
      {
         if (badge[0] == 'i')
         {
            /* rcheevos_client_download_game_badge expects a rc_client_game_t, not the badge name.
             * call rc_api_init_fetch_image_request directly */
            rc_api_fetch_image_request_t image_request;
            rc_api_request_t request;
            int result;

            memset(&image_request, 0, sizeof(image_request));
            image_request.image_type = RC_IMAGE_TYPE_GAME;
            image_request.image_name = &badge[1];
            result = rc_api_init_fetch_image_request(&request, &image_request);

            if (result == RC_OK)
               rcheevos_client_download_badge_from_url(request.url, badge);
         }
         else
         {
            rcheevos_client_download_achievement_badge(badge, locked);
         }
      }
      return 0;
   }

   return tex;
}

#else /* !HAVE_RC_CLIENT */

#if HAVE_MENU

enum rcheevos_menuitem_bucket
{
   RCHEEVOS_MENUITEM_BUCKET_UNKNOWN = 0,
   RCHEEVOS_MENUITEM_BUCKET_LOCKED,
   RCHEEVOS_MENUITEM_BUCKET_UNLOCKED,
   RCHEEVOS_MENUITEM_BUCKET_UNSUPPORTED,
   RCHEEVOS_MENUITEM_BUCKET_UNOFFICIAL,
   RCHEEVOS_MENUITEM_BUCKET_RECENTLY_UNLOCKED,
   RCHEEVOS_MENUITEM_BUCKET_ACTIVE_CHALLENGE,
   RCHEEVOS_MENUITEM_BUCKET_ALMOST_THERE
};

static void rcheevos_menu_update_bucket(rcheevos_racheevo_t* cheevo)
{
   cheevo->menu_progress = 0;

   /* Non-active unsupported achievement */
   if (cheevo->active & RCHEEVOS_ACTIVE_UNSUPPORTED)
      cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_UNSUPPORTED;
   /* Non-active unlocked in hardcore achievement */
   else if (!(cheevo->active & RCHEEVOS_ACTIVE_HARDCORE))
      cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_UNLOCKED;
   else
   {
      rc_trigger_t* trigger;
      const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

      if (!rcheevos_locals->hardcore_active && !(cheevo->active & RCHEEVOS_ACTIVE_SOFTCORE))
      {
         /* Non-active unlocked in softcore achievement in softcore mode */
         cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_UNLOCKED;
         return;
      }

      /* Active achievement */
      if (cheevo->active & RCHEEVOS_ACTIVE_UNOFFICIAL)
         cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_UNOFFICIAL;
      else
         cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_LOCKED;

      trigger = rc_runtime_get_achievement(&rcheevos_locals->runtime, cheevo->id);
      if (trigger)
      {
         if (trigger->measured_value && trigger->measured_value != 0xFFFFFFFF && trigger->measured_target)
         {
            const unsigned long clamped_value = (unsigned long)
                  MIN(trigger->measured_value, trigger->measured_target);
            cheevo->menu_progress =
                  (uint8_t)((clamped_value * 100) / trigger->measured_target);
         }

         if (trigger->state == RC_TRIGGER_STATE_PRIMED)
            cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_ACTIVE_CHALLENGE;
         else if (cheevo->menu_progress >= 80)
            cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_ALMOST_THERE;
      }
   }
}

static void rcheevos_menu_update_buckets(void)
{
   const rcheevos_locals_t *rcheevos_locals = get_rcheevos_locals();
   rcheevos_racheevo_t     *cheevo          = rcheevos_locals->game.achievements;
   rcheevos_racheevo_t     *stop            = cheevo +
      rcheevos_locals->game.achievement_count;

   while (cheevo < stop)
   {
      rcheevos_menu_update_bucket(cheevo);
      ++cheevo;
   }
}

bool rcheevos_menu_get_state(unsigned menu_offset, char *buffer, size_t buffer_size)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (menu_offset < rcheevos_locals->menuitem_count)
   {
      const rcheevos_menuitem_t* menuitem = &rcheevos_locals->menuitems[menu_offset];
      const rcheevos_racheevo_t* cheevo   = menuitem->cheevo;
      if (cheevo)
      {
         if (cheevo->menu_progress)
         {
            const int written = snprintf(buffer, buffer_size, "%s - ",
               msg_hash_to_str(menuitem->state_label_idx));
            if (buffer_size - written > 0)
               rc_runtime_format_achievement_measured(&rcheevos_locals->runtime,
                     cheevo->id, buffer + written, buffer_size - written);
         }
         else
            strlcpy(buffer, msg_hash_to_str(menuitem->state_label_idx), buffer_size);

         return true;
      }
   }

   if (buffer)
      buffer[0] = '\0';

   return false;
}

bool rcheevos_menu_get_sublabel(unsigned menu_offset, char *buffer, size_t buffer_size)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (menu_offset < rcheevos_locals->menuitem_count)
   {
      const rcheevos_racheevo_t* cheevo = rcheevos_locals->menuitems[menu_offset].cheevo;
      if (cheevo && buffer)
      {
         strlcpy(buffer, cheevo->description, buffer_size);
         return true;
      }
   }

   if (buffer)
      buffer[0] = '\0';

   return false;
}

void rcheevos_menu_reset_badges(void)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rcheevos_racheevo_t* cheevo              = rcheevos_locals->game.achievements;
   rcheevos_racheevo_t* stop                = cheevo + rcheevos_locals->game.achievement_count;

   while (cheevo < stop)
   {
      if (cheevo->menu_badge_texture)
      {
         video_driver_texture_unload(&cheevo->menu_badge_texture);
         cheevo->menu_badge_texture   = 0;
         cheevo->menu_badge_grayscale = MENU_BADGE_RETRY_RELOAD_FRAMES;
      }
      ++cheevo;
   }
}

static rcheevos_menuitem_t* rcheevos_menu_allocate(
      rcheevos_locals_t* rcheevos_locals, rcheevos_racheevo_t* cheevo)
{
   rcheevos_menuitem_t* menuitem;

   if (rcheevos_locals->menuitem_count == rcheevos_locals->menuitem_capacity)
   {
      if (rcheevos_locals->menuitems)
      {
         rcheevos_menuitem_t *new_menuitems;
         rcheevos_locals->menuitem_capacity += 32;
         new_menuitems = (rcheevos_menuitem_t*)realloc(rcheevos_locals->menuitems,
                       rcheevos_locals->menuitem_capacity * sizeof(rcheevos_menuitem_t));

         if (new_menuitems)
            rcheevos_locals->menuitems = new_menuitems;
         else
         {
            /* realloc failed */
            CHEEVOS_ERR(RCHEEVOS_TAG " could not allocate space for %u menu items\n",
                  rcheevos_locals->menuitem_capacity);
            rcheevos_locals->menuitem_capacity -= 32;
            return NULL;
         }
      }
      else
      {
         rcheevos_locals->menuitem_capacity = 64;
         rcheevos_locals->menuitems = (rcheevos_menuitem_t*)
               malloc(rcheevos_locals->menuitem_capacity * sizeof(rcheevos_menuitem_t));

         if (!rcheevos_locals->menuitems)
         {
            /* malloc failed */
            CHEEVOS_ERR(RCHEEVOS_TAG " could not allocate space for %u menu items\n",
                  rcheevos_locals->menuitem_capacity);
            rcheevos_locals->menuitem_capacity = 0;
            return NULL;
         }
      }
   }

   menuitem = &rcheevos_locals->menuitems[rcheevos_locals->menuitem_count++];
   menuitem->cheevo = cheevo;
   menuitem->state_label_idx = MSG_UNKNOWN;
   return menuitem;
}

static void rcheevos_menu_append_header(rcheevos_locals_t* rcheevos_locals,
      enum msg_hash_enums label)
{
   rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals, NULL);
   if (menuitem)
      menuitem->state_label_idx = label;
}

static void rcheevos_menu_update_badge(rcheevos_racheevo_t* cheevo)
{
   bool badge_grayscale = false;

   switch (cheevo->menu_bucket)
   {
      case RCHEEVOS_MENUITEM_BUCKET_LOCKED:
      case RCHEEVOS_MENUITEM_BUCKET_UNOFFICIAL:
      case RCHEEVOS_MENUITEM_BUCKET_UNSUPPORTED:
      case RCHEEVOS_MENUITEM_BUCKET_ALMOST_THERE:
      case RCHEEVOS_MENUITEM_BUCKET_ACTIVE_CHALLENGE:
         badge_grayscale = true;
         break;
      default:
         break;
   }

   if (!cheevo->menu_badge_texture || cheevo->menu_badge_grayscale != badge_grayscale)
   {
      uintptr_t new_badge_texture =
         rcheevos_get_badge_texture(cheevo->badge, badge_grayscale, false);

      if (new_badge_texture)
      {
         if (cheevo->menu_badge_texture)
            video_driver_texture_unload(&cheevo->menu_badge_texture);

         cheevo->menu_badge_texture   = new_badge_texture;
         cheevo->menu_badge_grayscale = badge_grayscale;
      }
      /* menu_badge_grayscale is overloaded such
       * that any value greater than 1 indicates
       * the server default image is being used */
      else if (cheevo->menu_badge_grayscale < 2)
      {
         if (cheevo->menu_badge_texture)
            video_driver_texture_unload(&cheevo->menu_badge_texture);

         /* requested badge is not available, check for server default */
         cheevo->menu_badge_texture =
            rcheevos_get_badge_texture("00000", false, false);

         if (cheevo->menu_badge_texture)
            cheevo->menu_badge_grayscale = 2;
      }
   }
}

static void rcheevos_menu_append_items(rcheevos_locals_t* rcheevos_locals,
      enum rcheevos_menuitem_bucket bucket)
{
   rcheevos_racheevo_t* cheevo = rcheevos_locals->game.achievements;
   rcheevos_racheevo_t* stop   = cheevo + rcheevos_locals->game.achievement_count;
   const unsigned first_index  = rcheevos_locals->menuitem_count;

   while (cheevo < stop)
   {
      if (cheevo->menu_bucket == bucket)
      {
         rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals, cheevo);
         if (!menuitem)
            return;

         switch (cheevo->menu_bucket)
         {
            case RCHEEVOS_MENUITEM_BUCKET_UNSUPPORTED:
               menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY;
               break;
            case RCHEEVOS_MENUITEM_BUCKET_RECENTLY_UNLOCKED:
            {
               /* insert the item such that the unlock times are descending */
               unsigned entry_index = rcheevos_locals->menuitem_count - 1;
               while (entry_index > first_index)
               {
                  rcheevos_menuitem_t* prev_menuitem = menuitem - 1;
                  if (prev_menuitem->cheevo->unlock_time >= cheevo->unlock_time)
                     break;

                  memcpy(menuitem, prev_menuitem, sizeof(rcheevos_menuitem_t));
                  menuitem = prev_menuitem;
                  --entry_index;
               }

               menuitem->cheevo = cheevo;
            }
            /* fallthrough to RCHEEVOS_MENUITEM_BUCKET_UNLOCKED */

            case RCHEEVOS_MENUITEM_BUCKET_UNLOCKED:
               if (!(cheevo->active & RCHEEVOS_ACTIVE_HARDCORE))
                  menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE;
               else
                  menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY;
               break;
            case RCHEEVOS_MENUITEM_BUCKET_ALMOST_THERE:
            {
               /* insert the item such that the progresses are descending */
               unsigned entry_index = rcheevos_locals->menuitem_count - 1;
               while (entry_index > first_index)
               {
                  rcheevos_menuitem_t* prev_menuitem = menuitem - 1;
                  if (prev_menuitem->cheevo->menu_progress >= cheevo->menu_progress)
                     break;

                  memcpy(menuitem, prev_menuitem, sizeof(rcheevos_menuitem_t));
                  menuitem = prev_menuitem;
                  --entry_index;
               }

               menuitem->cheevo = cheevo;
            }
            /* fallthrough to default */

            default:
               if (cheevo->active & RCHEEVOS_ACTIVE_UNOFFICIAL)
                  menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY;
               else if (!(cheevo->active & RCHEEVOS_ACTIVE_SOFTCORE))
                  menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY;
               else
                  menuitem->state_label_idx = MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY;
               break;
         }

         if (cheevo->badge && cheevo->badge[0])
         {
#ifndef HAVE_GFX_WIDGETS
            const settings_t* settings = config_get_ptr();
            if (settings && settings->bools.cheevos_badges_enable)
#endif
               rcheevos_menu_update_badge(cheevo);
         }
      }

      ++cheevo;
   }
}

uintptr_t rcheevos_menu_get_badge_texture(unsigned menu_offset)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (menu_offset < rcheevos_locals->menuitem_count)
   {
      rcheevos_racheevo_t* cheevo = rcheevos_locals->menuitems[menu_offset].cheevo;
      if (cheevo)
      {
         /* if we're using the placeholder badge, check to see if the real badge
          * has become available (do this roughly once a second) */
         if (cheevo->menu_badge_grayscale >= 2)
         {
            if (++cheevo->menu_badge_grayscale >= MENU_BADGE_RETRY_RELOAD_FRAMES)
            {
               cheevo->menu_badge_grayscale = 2;
               rcheevos_menu_update_badge(cheevo);
            }
         }

         return cheevo->menu_badge_texture;
      }
   }

   return 0;
}

void rcheevos_menu_populate_hardcore_pause_submenu(void* data)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   menu_displaylist_info_t* info = (menu_displaylist_info_t*)data;
   const settings_t* settings = config_get_ptr();
   const bool cheevos_hardcore_mode_enable = settings->bools.cheevos_hardcore_mode_enable;

   if (cheevos_hardcore_mode_enable && rcheevos_locals->loaded)
   {
      if (rcheevos_locals->hardcore_active)
      {
         menu_entries_append(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL,
               MENU_SETTING_ACTION_CLOSE, 0, 0, NULL);
         menu_entries_append(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE),
               MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE,
               MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0, NULL);
      }
      else
      {
         menu_entries_append(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL),
               MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL,
               MENU_SETTING_ACTION_CLOSE, 0, 0, NULL);
         menu_entries_append(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
               msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_RESUME),
               MENU_ENUM_LABEL_ACHIEVEMENT_RESUME,
               MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0, NULL);
      }
   }
}

void rcheevos_menu_populate(void* data)
{
   menu_displaylist_info_t* info            = (menu_displaylist_info_t*)data;
   rcheevos_locals_t* rcheevos_locals       = get_rcheevos_locals();
   const settings_t* settings               = config_get_ptr();
   unsigned num_locked                      = 0;
   unsigned num_unlocked                    = 0;
   unsigned num_recently_unlocked           = 0;
   unsigned num_unsupported                 = 0;
   unsigned num_active_challenges           = 0;
   unsigned num_almost_there                = 0;

   if (rcheevos_locals->loaded)
   {
      const retro_time_t now                = cpu_features_get_time_usec();
      const retro_time_t recent_unlock_time = now - (10 * 60 * 1000000); /* 10 minutes ago */
      rcheevos_racheevo_t* cheevo           = NULL;
      rcheevos_racheevo_t* stop             = NULL;

      /* first menu item is the Pause/Resume Hardcore option (unless hardcore is disabled) */
      if (settings->bools.cheevos_enable && settings->bools.cheevos_hardcore_mode_enable)
      {
         if (rcheevos_locals->hardcore_active)
            menu_entries_append(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE),
                  msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU),
                  MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU,
                  MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS, 0, 0, NULL);
         else
            menu_entries_append(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME),
                  msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU),
                  MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU,
                  MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS, 0, 0, NULL);
      }

      /* update the bucket for each achievement */
      rcheevos_menu_update_buckets();

      /* count items in each bucket */
      cheevo = rcheevos_locals->game.achievements;
      stop   = cheevo + rcheevos_locals->game.achievement_count;

      while (cheevo < stop)
      {
         switch (cheevo->menu_bucket)
         {
            case RCHEEVOS_MENUITEM_BUCKET_UNLOCKED:
               if (cheevo->unlock_time && cheevo->unlock_time >= recent_unlock_time)
               {
                  cheevo->menu_bucket = RCHEEVOS_MENUITEM_BUCKET_RECENTLY_UNLOCKED;
                  ++num_recently_unlocked;
               }
               else
                  ++num_unlocked;
               break;

            case RCHEEVOS_MENUITEM_BUCKET_LOCKED:
            case RCHEEVOS_MENUITEM_BUCKET_UNOFFICIAL:
               ++num_locked;
               break;

            case RCHEEVOS_MENUITEM_BUCKET_UNSUPPORTED:
               ++num_unsupported;
               break;

            case RCHEEVOS_MENUITEM_BUCKET_ACTIVE_CHALLENGE:
               ++num_active_challenges;
               break;

            case RCHEEVOS_MENUITEM_BUCKET_ALMOST_THERE:
               ++num_almost_there;
               break;
         }

         ++cheevo;
      }

      if (!rcheevos_locals->menuitems)
      {
         /* reserve space for all achievements and up to 6 headers before we need to realloc */
         rcheevos_locals->menuitem_capacity    = rcheevos_locals->game.achievement_count + 6;
         rcheevos_locals->menuitems            = (rcheevos_menuitem_t*)
               malloc(rcheevos_locals->menuitem_capacity * sizeof(rcheevos_menuitem_t));
         if (!rcheevos_locals->menuitems)
            rcheevos_locals->menuitem_capacity = 0;
      }
   }

   /* reset menu */
   rcheevos_locals->menuitem_count = 0;

   /* active challenges */
   if (num_active_challenges)
   {
      rcheevos_menu_append_header(rcheevos_locals,
            MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY);

      rcheevos_menu_append_items(rcheevos_locals,
            RCHEEVOS_MENUITEM_BUCKET_ACTIVE_CHALLENGE);
   }

   /* recently unlocked */
   if (num_recently_unlocked)
   {
      rcheevos_menu_append_header(rcheevos_locals,
            MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY);

      rcheevos_menu_append_items(rcheevos_locals,
            RCHEEVOS_MENUITEM_BUCKET_RECENTLY_UNLOCKED);
   }

   /* almost there */
   if (num_almost_there)
   {
      rcheevos_menu_append_header(rcheevos_locals,
            MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY);

      rcheevos_menu_append_items(rcheevos_locals,
            RCHEEVOS_MENUITEM_BUCKET_ALMOST_THERE);
   }

   /* locked */
   if (num_locked)
   {
      if (rcheevos_locals->menuitem_count > 0)
         rcheevos_menu_append_header(rcheevos_locals,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY);

      rcheevos_menu_append_items(rcheevos_locals,
            RCHEEVOS_MENUITEM_BUCKET_LOCKED);
      rcheevos_menu_append_items(rcheevos_locals,
            RCHEEVOS_MENUITEM_BUCKET_UNOFFICIAL);
   }

   /* unsupported */
   if (num_unsupported)
   {
      if (rcheevos_locals->menuitem_count > 0)
         rcheevos_menu_append_header(rcheevos_locals,
            MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY);

      rcheevos_menu_append_items(rcheevos_locals,
         RCHEEVOS_MENUITEM_BUCKET_UNSUPPORTED);
   }

   /* unlocked */
   if (num_unlocked)
   {
      if (rcheevos_locals->menuitem_count > 0)
         rcheevos_menu_append_header(rcheevos_locals,
               MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY);

      rcheevos_menu_append_items(rcheevos_locals,
            RCHEEVOS_MENUITEM_BUCKET_UNLOCKED);
   }

   if (rcheevos_locals->menuitem_count > 0)
   {
      char buffer[128];
      unsigned idx                  = 0;
      /* convert to menu entries */
      rcheevos_menuitem_t* menuitem = rcheevos_locals->menuitems;
      rcheevos_menuitem_t* stop     = menuitem +
         rcheevos_locals->menuitem_count;

      do
      {
         if (menuitem->cheevo)
            menu_entries_append(info->list, menuitem->cheevo->title,
                  menuitem->cheevo->description,
                  MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
                  MENU_SETTINGS_CHEEVOS_START + idx, 0, 0, NULL);
         else
         {
            snprintf(buffer, sizeof(buffer), "----- %s -----",
                  msg_hash_to_str(menuitem->state_label_idx));

            menu_entries_append(info->list, buffer, "",
                  MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY,
                  MENU_SETTINGS_CHEEVOS_START + idx, 0, 0, NULL);
         }

         ++idx;
         ++menuitem;
      } while (menuitem != stop);
   }
   else
   {
      /* no achievements found */
      if (!rcheevos_locals->core_supports)
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE),
            MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
            FILE_TYPE_NONE, 0, 0, NULL);
      else if (rcheevos_locals->load_info.state == RCHEEVOS_LOAD_STATE_NETWORK_ERROR)
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_ERROR),
            msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_ERROR),
            MENU_ENUM_LABEL_NETWORK_ERROR,
            FILE_TYPE_NONE, 0, 0, NULL);
      else if (!rcheevos_locals->game.id)
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME),
            msg_hash_to_str(MENU_ENUM_LABEL_UNKNOWN_GAME),
            MENU_ENUM_LABEL_UNKNOWN_GAME,
            FILE_TYPE_NONE, 0, 0, NULL);
      else if (!rcheevos_locals->token[0])
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN),
            msg_hash_to_str(MENU_ENUM_LABEL_NOT_LOGGED_IN),
            MENU_ENUM_LABEL_NOT_LOGGED_IN,
            FILE_TYPE_NONE, 0, 0, NULL);
      else
         menu_entries_append(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0, NULL);
   }
}

#endif /* HAVE_MENU */

uintptr_t rcheevos_get_badge_texture(const char *badge, bool locked, bool download_if_missing)
{
   if (badge)
   {
      size_t _len;
      char badge_file[24];
      char fullpath[PATH_MAX_LENGTH];
      uintptr_t tex = 0;

      /* OpenGL driver crashes if gfx_display_reset_textures_list is called on a background thread */
      retro_assert(task_is_on_main_thread());

      _len  = strlcpy(badge_file, badge, sizeof(badge_file));
      _len += strlcpy(badge_file + _len, locked ? "_lock" : "",
            sizeof(badge_file)   - _len);
      strlcpy(badge_file + _len, ".png",
            sizeof(badge_file)   - _len);

      fill_pathname_application_special(fullpath, sizeof(fullpath),
            APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

      if (gfx_display_reset_textures_list(badge_file, fullpath,
               &tex, TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
         return tex;
   }
   return 0;
}

#endif /* HAVE_RC_CLIENT */
