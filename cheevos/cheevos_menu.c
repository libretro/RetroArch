/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019-2026 - Brian Weiss
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
#include <compat/strl.h>

#include "cheevos_locals.h"
#include "cheevos_client.h"

#include "../gfx/gfx_display.h"
#include "../file_path_special.h"
#include "../msg_hash.h"

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

#if HAVE_MENU

enum rcheevos_menu_type
{
   RCHEEVOS_MENU_ACHIEVEMENT,
   RCHEEVOS_MENU_HEADER,
   RCHEEVOS_MENU_USER,
   RCHEEVOS_MENU_RICHPRESENCE,
   RCHEEVOS_MENU_INFO,
   RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS,
   RCHEEVOS_MENU_TOGGLE_HARDCORE,
   RCHEEVOS_MENU_ACTION
};

static void rcheevos_menu_update_badge(rcheevos_menuitem_t* menuitem, bool download_if_missing);

static size_t rcheevos_menu_get_achievement_state(const rcheevos_menuitem_t* menuitem, char* s, size_t len)
{
   const rc_client_achievement_t* cheevo = menuitem->source.achievement.achievement;
   if (cheevo)
   {
      size_t _len;
      if (cheevo->state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
         _len = strlcpy(s, msg_hash_to_str((enum msg_hash_enums)menuitem->state_label_idx), len);
      else
      {
         const char* missable = (cheevo->type == RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE) ? "[m] " : "";
         _len = strlcpy(s, missable, len);
         _len += strlcpy(s + _len, msg_hash_to_str((enum msg_hash_enums)menuitem->state_label_idx), len - _len);
         if (cheevo->measured_progress[0])
         {
            _len += strlcpy_lit(s + _len, " - ", len - _len);
            _len += strlcpy(s + _len, cheevo->measured_progress, len - _len);
         }
      }
      return _len;
   }

   s[0] = '\0';
   return 0;
}

static size_t rcheevos_menu_get_subset_achievement_state(const rcheevos_locals_t* rcheevos_locals, const rcheevos_menuitem_t* menuitem, char* s, size_t len)
{
   rc_client_user_game_summary_t user_summary;
   rc_client_get_user_subset_summary(rcheevos_locals->client, menuitem->subset_id, &user_summary);
   return snprintf(s, len, "%u/%u", user_summary.num_unlocked_achievements, user_summary.num_core_achievements);
}

size_t rcheevos_menu_get_state(unsigned menu_offset, char *s, size_t len)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (!s)
      return 0;

   if (menu_offset < rcheevos_locals->menuitem_count)
   {
      const rcheevos_menuitem_t* menuitem   = &rcheevos_locals->menuitems[menu_offset];
      switch (menuitem->type)
      {
      case RCHEEVOS_MENU_ACHIEVEMENT:
         return rcheevos_menu_get_achievement_state(menuitem, s, len);

      case RCHEEVOS_MENU_HEADER:
         /* state_label_idx for header is the header text, not the state text */
         break;

      case RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS:
         return rcheevos_menu_get_subset_achievement_state(rcheevos_locals, menuitem, s, len);

      default:
         if (menuitem->state_label_idx)
            return strlcpy(s, msg_hash_to_str((enum msg_hash_enums)menuitem->state_label_idx), len);

         break;
      }
   }

   s[0] = '\0';
   return 0;
}

size_t rcheevos_menu_get_sublabel(unsigned menu_offset, char* s, size_t len)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (!s)
      return 0;

   if (menu_offset < rcheevos_locals->menuitem_count && s)
   {
      const rcheevos_menuitem_t* menuitem = &rcheevos_locals->menuitems[menu_offset];
      switch (menuitem->type)
      {
      case RCHEEVOS_MENU_ACHIEVEMENT:
         if (menuitem->source.achievement.achievement)
            return strlcpy(s, menuitem->source.achievement.achievement->description, len);
         break;

      case RCHEEVOS_MENU_USER:
         return rc_client_get_rich_presence_message(rcheevos_locals->client, s, len);

      case RCHEEVOS_MENU_TOGGLE_HARDCORE:
         return strlcpy(s, msg_hash_to_str(rcheevos_hardcore_active() ? MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE : MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME), len);

      case RCHEEVOS_MENU_ACTION:
         return strlcpy(s, msg_hash_to_str((enum msg_hash_enums)menuitem->source.action.sublabel), len);

      default:
         break;
      }
   }

   s[0] = '\0';
   return 0;
}

size_t rcheevos_menu_get_submenu_title(char* s, size_t len)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (s) {
      switch (rcheevos_locals->menuitem_submenu_type)
      {
      case RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS:
      {
         const rc_client_subset_t* subset = rc_client_get_subset_info(rcheevos_locals->client, rcheevos_locals->menuitem_submenu_id);
         if (subset)
            return snprintf(s, len, "%s - %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST), subset->title);
      }

      case RCHEEVOS_MENU_TOGGLE_HARDCORE:
         return strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE), len);
      }

      s[0] = '\0';
   }

   return 0;
}

size_t rcheevos_menu_get_help_text(unsigned menu_offset, char *s, size_t len)
{
   const rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   if (!s)
      return 0;

   if (menu_offset < rcheevos_locals->menuitem_count && s)
   {
      const rcheevos_menuitem_t* menuitem = &rcheevos_locals->menuitems[menu_offset];
      switch (menuitem->type)
      {
      case RCHEEVOS_MENU_ACHIEVEMENT:
         if (menuitem->source.achievement.achievement)
            return strlcpy(s, menuitem->source.achievement.achievement->description, len);
         break;

      default:
         break;
      }
   }

   s[0] = '\0';
   return 0;
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
   rcheevos_locals_t* rcheevos_locals, uint8_t type)
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
   menuitem->type = type;
   return menuitem;
}

static void rcheevos_menu_append_header(rcheevos_locals_t* rcheevos_locals,
   enum msg_hash_enums label, uint32_t subset_id)
{
   rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals, RCHEEVOS_MENU_HEADER);
   if (menuitem)
   {
      menuitem->state_label_idx = label;
      menuitem->subset_id = subset_id;
   }
}

static void rcheevos_menu_append_action(rcheevos_locals_t* rcheevos_locals,
   enum msg_hash_enums type, enum msg_hash_enums label, enum msg_hash_enums sublabel, enum menu_settings_type action)
{
   rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals, RCHEEVOS_MENU_ACTION);
   if (menuitem)
   {
      menuitem->source.action.type = type;
      menuitem->source.action.label = label;
      menuitem->source.action.sublabel = sublabel;
      menuitem->source.action.action = action;
   }
}

static void rcheevos_menu_append_user_item(rcheevos_locals_t* rcheevos_locals)
{
   rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals, RCHEEVOS_MENU_USER);
   if (menuitem)
   {
      menuitem->state_label_idx = rcheevos_hardcore_active()
         ? MSG_CHEEVOS_HARDCORE_MODE : MSG_CHEEVOS_CASUAL_MODE;

      rcheevos_menu_update_badge(menuitem, true);
   }
}

static void rcheevos_menu_append_warnings(rcheevos_locals_t* rcheevos_locals, const rc_client_game_t* game)
{
   if (!rcheevos_locals->core_supports)
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
         MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
         MENU_ENUM_LABEL_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
         (enum menu_settings_type)FILE_TYPE_NONE);
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

      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
         msg,
         MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
         (enum menu_settings_type)FILE_TYPE_NONE);
   }
   else if (!game->id)
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
         MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
         MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
         (enum menu_settings_type)FILE_TYPE_NONE);
   }
   else if (rcheevos_locals->has_unsupported_achievements)
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MSG_CHEEVOS_UNSUPPORTED_WARNING,
         MSG_CHEEVOS_UNSUPPORTED_WARNING,
         MSG_CHEEVOS_UNSUPPORTED_WARNING,
         (enum menu_settings_type)FILE_TYPE_NONE);
   }

   if (rcheevos_locals->client && rcheevos_locals->client->state.disconnect)
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
         MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
         MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
         MENU_INFO_ACHIEVEMENTS_SERVER_UNREACHABLE);
   }
}

static void rcheevos_menu_update_badge(rcheevos_menuitem_t* menuitem, bool download_if_missing)
{
   char badge_name_buffer[32];
   const char* badge_name = "00000";
   bool badge_grayscale = false;

   switch (menuitem->type)
   {
   case RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS:
   {
      const rc_client_subset_t* subset = rc_client_get_subset_info(get_rcheevos_locals()->client, menuitem->subset_id);
      if (subset) {
         snprintf(badge_name_buffer, sizeof(badge_name_buffer), "i%s", subset->badge_name);
         badge_name = badge_name_buffer;
      }
      break;
   }

   case RCHEEVOS_MENU_USER:
   {
      const rc_client_user_t* user = rc_client_get_user_info(get_rcheevos_locals()->client);
      if (user) {
         snprintf(badge_name_buffer, sizeof(badge_name_buffer), "u%u", rc_djb2(user->username));
         badge_name = badge_name_buffer;
      }
      break;
   }

   case RCHEEVOS_MENU_ACHIEVEMENT:
      if (menuitem->source.achievement.achievement)
         badge_name = menuitem->source.achievement.achievement->badge_name;

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
      break;

   default:
      return;
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
      switch (menuitem->type)
      {
      case RCHEEVOS_MENU_ACHIEVEMENT:
      case RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS:
      case RCHEEVOS_MENU_USER:
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
         break;

      default:
         return 0;
      }

      return menuitem->menu_badge_texture;
   }

   return 0;
}

void rcheevos_menu_update_badge_references(const char* badge_name)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   unsigned i;
   char unlocked_badge_name[8];
   const size_t badge_name_len = strlen(badge_name);
   if (badge_name_len > 6 && badge_name_len < sizeof(unlocked_badge_name) + 5 &&
       strcmp(&badge_name[badge_name_len - 5], "_lock") == 0)
   {
      memcpy(unlocked_badge_name, badge_name, badge_name_len - 5);
      unlocked_badge_name[badge_name_len - 5] = '\0';
      badge_name = unlocked_badge_name;
   }

   for (i = 0; i < rcheevos_locals->menuitem_count; ++i)
   {
      rcheevos_menuitem_t* menuitem = &rcheevos_locals->menuitems[i];
      if (menuitem->type != RCHEEVOS_MENU_ACHIEVEMENT)
         continue;

      if (menuitem->menu_badge_grayscale >= 2 && /* using placeholder */
          strncmp(menuitem->source.achievement.achievement->badge_name, badge_name, badge_name_len) == 0)
      {
          rcheevos_menu_update_badge(menuitem, false);
      }
   }
}

static bool rcheevos_menu_achievement_in_list(const rc_client_achievement_t* achievement, rc_client_achievement_list_t* list, uint32_t subset_id)
{
   const rc_client_achievement_bucket_t* bucket = list->buckets;
   const rc_client_achievement_bucket_t* bucket_stop = bucket + list->num_buckets;

   const rc_client_achievement_t** scan;
   const rc_client_achievement_t** stop;

   for (; bucket < bucket_stop; ++bucket)
   {
      if (bucket->subset_id != subset_id)
         continue;

      scan = bucket->achievements;
      stop = scan + bucket->num_achievements;
      for (; scan < stop; ++scan)
      {
         if (*scan == achievement)
            return true;
      }
   }

   return false;
}

static bool rcheevos_menu_has_subsets(const rc_client_achievement_list_t* list)
{
   const rc_client_achievement_bucket_t* bucket = list->buckets;
   const rc_client_achievement_bucket_t* bucket_stop = bucket + list->num_buckets;

   uint32_t first_subset_id = 0;

   for (; bucket < bucket_stop; ++bucket)
   {
      if (!bucket->subset_id)
         continue;

      if (!first_subset_id)
         first_subset_id = bucket->subset_id;
      else if (bucket->subset_id != first_subset_id)
         return true;
   }

   return false;
}

static void rcheevos_menu_append_achievements(rcheevos_locals_t* rcheevos_locals, uint32_t subset_id)
{
   rc_client_achievement_list_t* list = rc_client_create_achievement_list(rcheevos_locals->client,
         RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
         RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
   rc_client_achievement_list_t* list2 = NULL;
   rcheevos_menuitem_t* menuitem = NULL;
   bool checked_subsets = false;

   const rc_client_achievement_bucket_t* bucket = list->buckets;
   const rc_client_achievement_bucket_t* bucket_stop = bucket + list->num_buckets;

   const rc_client_achievement_t** achievement;
   const rc_client_achievement_t** achievement_stop;

   for (; bucket < bucket_stop; ++bucket)
   {
      enum msg_hash_enums label = MSG_UNKNOWN;

      if (subset_id == 0)
      {
         switch (bucket->bucket_type)
         {
         case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
         case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
            /* these can be shown at the top level */
            break;

         default:
            /* the rest aren't shown at the top level */
            continue;
         }
      }

      switch (bucket->bucket_type)
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

      achievement = bucket->achievements;
      achievement_stop = achievement + bucket->num_achievements;
      for (; achievement < achievement_stop; ++achievement)
      {
         if (subset_id != 0)
         {
            if (bucket->subset_id == 0)
            {
               if (!checked_subsets)
               {
                  if (rcheevos_menu_has_subsets(list))
                  {
                     list2 = rc_client_create_achievement_list(rcheevos_locals->client,
                        RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
                  }
                  checked_subsets = true;
               }

               if (list2 && !rcheevos_menu_achievement_in_list(*achievement, list2, subset_id))
                  continue;
            }
            else if (bucket->subset_id != subset_id)
            {
               continue;
            }
         }

         if (label != MSG_UNKNOWN)
         {
            rcheevos_menu_append_header(rcheevos_locals, label, 0);
            label = MSG_UNKNOWN;
         }

         menuitem = rcheevos_menu_allocate(rcheevos_locals, RCHEEVOS_MENU_ACHIEVEMENT);
         if (!menuitem)
            break;

         menuitem->source.achievement.achievement = *achievement;

         switch (bucket->bucket_type)
         {
         case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
         case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED:
            if ((*achievement)->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE)
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

         rcheevos_menu_update_badge(menuitem, false);
      }
   }

   rc_client_destroy_achievement_list(list);

   if (list2)
      rc_client_destroy_achievement_list(list2);

   if (subset_id != 0 && rcheevos_locals->menuitem_count == 0)
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
         MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
         MENU_ENUM_LABEL_NO_ACHIEVEMENTS_TO_DISPLAY,
         (enum menu_settings_type)FILE_TYPE_NONE
      );
   }
}

static void rcheevos_menu_append_subsets(rcheevos_locals_t* rcheevos_locals)
{
   rc_client_subset_list_t* subsets = rc_client_create_subset_list(rcheevos_locals->client);
   const rc_client_subset_t** subset = subsets->subsets;
   const rc_client_subset_t** subset_stop = subset + subsets->num_subsets;

   rcheevos_menu_append_header(rcheevos_locals, MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST, 0);

   for (; subset < subset_stop; ++subset)
   {
      rcheevos_menuitem_t* menuitem = rcheevos_menu_allocate(rcheevos_locals, RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS);
      if (!menuitem)
         break;

      menuitem->subset_id = (*subset)->id;
      rcheevos_menu_update_badge(menuitem, true);
   }

   rc_client_destroy_subset_list(subsets);
}

static void rcheevos_menu_append_pause_hardcore(rcheevos_locals_t* rcheevos_locals)
{
   rcheevos_menu_append_header(rcheevos_locals, MSG_CHEEVOS_HARDCORE_MODE, 0);
   rcheevos_menu_allocate(rcheevos_locals, RCHEEVOS_MENU_TOGGLE_HARDCORE);
}

static void rcheevos_menu_build_header(rcheevos_locals_t* rcheevos_locals, const rcheevos_menuitem_t* menuitem, char* buffer, size_t buffer_size)
{
   if (menuitem->subset_id)
   {
      const rc_client_subset_t* subset =
         rc_client_get_subset_info(rcheevos_locals->client, menuitem->subset_id);

      snprintf(buffer, buffer_size, "----- %s - %s -----",
         subset ? subset->title : "Unknown Subset",
         msg_hash_to_str((enum msg_hash_enums)menuitem->state_label_idx));
   }
   else
      snprintf(buffer, buffer_size, "----- %s -----",
         msg_hash_to_str((enum msg_hash_enums)menuitem->state_label_idx));
}

static void rcheevos_menu_build_menu_list(menu_displaylist_info_t* info, rcheevos_locals_t* rcheevos_locals)
{
   char buffer[256];
   unsigned idx = 0;
   const rcheevos_menuitem_t* menuitem = rcheevos_locals->menuitems;
   const rcheevos_menuitem_t* stop = menuitem + rcheevos_locals->menuitem_count;

   do
   {
      const char* label = NULL;
      const char* sublabel = "";
      int icon = MENU_SETTINGS_CHEEVOS_START + idx;
      enum msg_hash_enums type = MENU_ENUM_LABEL_CHEEVOS_MENU_ENTRY;

      switch (menuitem->type)
      {
      case RCHEEVOS_MENU_ACHIEVEMENT:
         if (menuitem->source.achievement.achievement)
         {
            label = menuitem->source.achievement.achievement->title;
            sublabel = menuitem->source.achievement.achievement->description;
         }
         break;

      case RCHEEVOS_MENU_HEADER:
         rcheevos_menu_build_header(rcheevos_locals, menuitem, buffer, sizeof(buffer));
         label = buffer;
         break;

      case RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS:
      {
         const rc_client_subset_t* subset = rc_client_get_subset_info(rcheevos_locals->client, menuitem->subset_id);
         if (subset)
         {
            label = subset->title;
            type = MENU_ENUM_LABEL_CHEEVOS_MENU_SUBMENU;
         }
         break;
      }

      case RCHEEVOS_MENU_USER:
      {
         const rc_client_user_t* user = rc_client_get_user_info(rcheevos_locals->client);
         if (user)
            label = user->display_name;
         else {
            label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN);
            icon = MENU_INFO_ACHIEVEMENTS_SERVER_UNREACHABLE;
         }
         break;
      }

      case RCHEEVOS_MENU_RICHPRESENCE:
         if (rc_client_get_rich_presence_message(rcheevos_locals->client, buffer, sizeof(buffer)))
            label = buffer;
         break;

      case RCHEEVOS_MENU_TOGGLE_HARDCORE:
         label = msg_hash_to_str(rcheevos_hardcore_active() ? MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE : MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME);
         type = MENU_ENUM_LABEL_CHEEVOS_MENU_SUBMENU;
         icon = MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS;
         break;

      case RCHEEVOS_MENU_ACTION:
         label = msg_hash_to_str((enum msg_hash_enums)menuitem->source.action.label);
         sublabel = msg_hash_to_str((enum msg_hash_enums)menuitem->source.action.sublabel);
         type = (enum msg_hash_enums)menuitem->source.action.type;
         icon = menuitem->source.action.action;
         break;
      }

      menu_entries_append(info->list, label, sublabel, type, icon, idx, idx, NULL);

      ++idx;
      ++menuitem;
   } while (menuitem != stop);
}

void rcheevos_menu_populate(void* data, bool cheevos_enable,
      bool cheevos_hardcore_mode_enable)
{
   menu_displaylist_info_t* info = (menu_displaylist_info_t*)data;
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals->client);

   /* Clear out the previously selected submenu - we're back at the main menu */
   rcheevos_locals->menuitem_info_type = 0;
   rcheevos_locals->menuitem_submenu_type = 0;
   rcheevos_locals->menuitem_submenu_id = 0;

   /* Rebuild the main menu */
   rcheevos_menu_reset_badges();
   rcheevos_locals->menuitem_count = 0;

   rcheevos_menu_append_user_item(rcheevos_locals);

   if (!game)
   {
      rcheevos_menu_append_warnings(rcheevos_locals, game);
   }
   else
   {
      /* Rich presence will be shown as the user sublabel if sublabels are enabled. */
      const settings_t* settings = config_get_ptr();
      if (!settings->bools.menu_show_sublabels)
         rcheevos_menu_allocate(rcheevos_locals, RCHEEVOS_MENU_RICHPRESENCE);

      if (!rcheevos_locals->badges_loaded && !rcheevos_locals->badges_loading) {
         rcheevos_locals->badges_loading = true;
         rcheevos_client_download_achievement_badges(rcheevos_locals->client);
      }

      rcheevos_menu_append_warnings(rcheevos_locals, game);

      rcheevos_menu_append_achievements(rcheevos_locals, 0);
      rcheevos_menu_append_subsets(rcheevos_locals);

      /* If hardcore is not completed disabled, add a pause hardcore toggle. */
      if (cheevos_enable && cheevos_hardcore_mode_enable)
         rcheevos_menu_append_pause_hardcore(rcheevos_locals);
   }

   /* convert to menu entries */
   if (rcheevos_locals->menuitem_count > 0)
      rcheevos_menu_build_menu_list(info, rcheevos_locals);
}

static void rcheevos_menu_append_hardcore_toggles(rcheevos_locals_t* rcheevos_locals)
{
   if (rc_client_get_hardcore_enabled(rcheevos_locals->client))
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL,
         MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
         MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
         MENU_SETTING_ACTION_CLOSE);
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE,
         MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
         MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
         MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS);
   }
   else if (rcheevos_locals->hardcore_requires_reload)
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
         MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
         MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
         MENU_SETTING_ACTION_CLOSE);
   }
   else
   {
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL,
         MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
         MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
         MENU_SETTING_ACTION_CLOSE);
      rcheevos_menu_append_action(rcheevos_locals,
         MENU_ENUM_LABEL_ACHIEVEMENT_RESUME,
         MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
         MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
         MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS);
   }
}

void rcheevos_menu_populate_submenu(void* data)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   menu_displaylist_info_t* info = (menu_displaylist_info_t*)data;

   /* RetroArch will call back into this method if the user closes the menu and then
    * reopens it. At that point, the parent menu no longer exists to extract the submenu
    * properties from. So if the info->type value hasn't changed, just use the last
    * captured submenu properties. */
   if (rcheevos_locals->menuitem_info_type != info->type)
   {
      rcheevos_menuitem_t* menuitem = (info->directory_ptr < rcheevos_locals->menuitem_count)
         ? &rcheevos_locals->menuitems[info->directory_ptr] : &rcheevos_locals->menuitems[0];

      rcheevos_locals->menuitem_submenu_type = menuitem->type;
      rcheevos_locals->menuitem_submenu_id = menuitem->subset_id;
      rcheevos_locals->menuitem_info_type = info->type;
   }

   rcheevos_menu_reset_badges();
   rcheevos_locals->menuitem_count = 0;

   switch (rcheevos_locals->menuitem_submenu_type)
   {
   case RCHEEVOS_MENU_SUBSET_ACHIEVEMENTS:
      rcheevos_menu_append_achievements(rcheevos_locals, rcheevos_locals->menuitem_submenu_id);
      break;

   case RCHEEVOS_MENU_TOGGLE_HARDCORE:
      rcheevos_menu_append_hardcore_toggles(rcheevos_locals);
      break;
   }

   if (rcheevos_locals->menuitem_count > 0)
      rcheevos_menu_build_menu_list(info, rcheevos_locals);
}

#endif /* HAVE_MENU */

static void rcheevos_client_download_user_badge()
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   const rc_client_user_t* user = rc_client_get_user_info(rcheevos_locals->client);
   if (user)
   {
      char badge_name[32];
      snprintf(badge_name, sizeof(badge_name), "u%u", rc_djb2(user->username));

      rcheevos_client_download_badge_from_url(user->avatar_url, badge_name);
   }
}

static void rcheevos_client_download_subset_badge(const char* badge_name)
{
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();

   const rc_client_game_t* game = rc_client_get_game_info(rcheevos_locals->client);
   if (game && strcmp(game->badge_name, &badge_name[1]) == 0)
   {
      rcheevos_client_download_badge_from_url(game->badge_url, badge_name);
   }
   else
   {
      rc_client_subset_list_t* subset_list = rc_client_create_subset_list(rcheevos_locals->client);
      uint32_t i;
      for (i = 0; i < subset_list->num_subsets; ++i)
      {
         if (strcmp(subset_list->subsets[i]->badge_name, &badge_name[1]) == 0)
         {
            rcheevos_client_download_badge_from_url(subset_list->subsets[i]->badge_url, badge_name);
            break;
         }
      }
      rc_client_destroy_subset_list(subset_list);
   }
}

static void rcheevos_client_download_achievement_badge(const char* badge_name, bool locked)
{
   /* have to find the achievement associated to badge_name, then fetch either badge_url
    * or badge_locked_url based on the locked parameter */
   rcheevos_locals_t* rcheevos_locals = get_rcheevos_locals();
   rc_client_achievement_list_t* list = rc_client_create_achievement_list(rcheevos_locals->client,
      RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
      RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
   if (list)
   {
      const char* url = NULL;
      uint32_t i, j;
      for (i = 0; i < list->num_buckets && !url; i++)
      {
         for (j = 0; j < list->buckets[i].num_achievements; j++)
         {
            const rc_client_achievement_t* achievement = list->buckets[i].achievements[j];
            if (achievement && strcmp(achievement->badge_name, badge_name) == 0)
            {
               url = locked ? achievement->badge_locked_url : achievement->badge_url;
               break;
            }
         }
      }

      if (url)
      {
         char locked_badge_name[32];
         if (locked)
         {
            snprintf(locked_badge_name, sizeof(locked_badge_name), "%s_lock", badge_name);
            badge_name = locked_badge_name;
         }

         rcheevos_client_download_badge_from_url(url, badge_name);
      }

      rc_client_destroy_achievement_list(list);
   }
}

static void rcheevos_get_local_badge_filename(char badge_file[], size_t badge_file_size, const char* badge, bool locked)
{
   size_t _len = strlcpy(badge_file, badge, badge_file_size);
   if (locked)
      _len += strlcpy_lit(badge_file + _len, "_lock", badge_file_size - _len);
   strlcpy(badge_file + _len, FILE_PATH_PNG_EXTENSION, badge_file_size - _len);
}

bool rcheevos_is_badge_available(const char* badge, bool locked)
{
   char badge_file[24];
   char fullpath[PATH_MAX_LENGTH];

   rcheevos_get_local_badge_filename(badge_file, sizeof(badge_file), badge, locked);

   fill_pathname_application_special(fullpath, sizeof(fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);
   fill_pathname_join(fullpath, fullpath, badge_file, sizeof(fullpath));

   return path_is_valid(fullpath);
}

uintptr_t rcheevos_get_badge_texture(const char* badge, bool locked, bool download_if_missing)
{
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

   rcheevos_get_local_badge_filename(badge_file, sizeof(badge_file), badge, locked);

   fill_pathname_application_special(fullpath, sizeof(fullpath),
      APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

   if (!gfx_display_reset_textures_list(badge_file, fullpath,
      &tex, gfx_display_texture_filter(), NULL, NULL))
   {
      if (download_if_missing)
      {
         if (badge[0] == 'i')
            rcheevos_client_download_subset_badge(badge);
         else if (badge[0] == 'u')
            rcheevos_client_download_user_badge();
         else
            rcheevos_client_download_achievement_badge(badge, locked);
      }
      return 0;
   }

   return tex;
}


