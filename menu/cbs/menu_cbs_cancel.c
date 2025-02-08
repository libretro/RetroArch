/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../../audio/audio_driver.h"
#include "../../configuration.h"
#include "../../msg_hash.h"
#ifdef HAVE_CHEATS
#include "../../cheat_manager.h"
#endif

#ifndef BIND_ACTION_CANCEL
#define BIND_ACTION_CANCEL(cbs, name) (cbs)->action_cancel = (name)
#endif

/* Clicks the back button */
int action_cancel_pop_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   size_t new_selection_ptr;
   struct menu_state *menu_st             = menu_state_get_ptr();
   size_t selection                       = menu_st->selection_ptr;
   const char *menu_label                 = NULL;
   unsigned menu_type                     = MENU_SETTINGS_NONE;
   menu_search_terms_t *menu_search_terms = menu_entries_search_get_terms();
#ifdef HAVE_AUDIOMIXER
   settings_t *settings                   = config_get_ptr();
   bool audio_enable_menu                 = settings->bools.audio_enable_menu;
   bool audio_enable_menu_cancel          = settings->bools.audio_enable_menu_cancel;
   if (audio_enable_menu && audio_enable_menu_cancel)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
#endif

   menu_entries_get_last_stack(NULL, &menu_label, &menu_type, NULL, NULL);

   /* Check whether search terms have been set
    * > If so, check whether this is a menu list
    *   with 'search filter' support
    * > If so, remove the last search term */
   if (   menu_search_terms
       && menu_driver_search_filter_enabled(menu_label, menu_type)
       && menu_entries_search_pop())
   {
      /* Reset navigation pointer */
      menu_st->selection_ptr      = 0;
      if (menu_st->driver_ctx->navigation_set)
         menu_st->driver_ctx->navigation_set(menu_st->userdata, false);
      /* Refresh menu */
      menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                      | MENU_ST_FLAG_PREVENT_POPULATE;
      return 0;
   }

   if (!string_is_empty(menu_label))
   {
      if (
            string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)
               )
         || string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_WALLPAPER)
               )
         )
         filebrowser_clear_type();
   }

   new_selection_ptr      = menu_st->selection_ptr;
   menu_entries_pop_stack(&new_selection_ptr, 0, 1);
   menu_st->selection_ptr = new_selection_ptr;

   if (menu_st->driver_ctx)
   {
      if (menu_st->driver_ctx->update_savestate_thumbnail_path)
         menu_st->driver_ctx->update_savestate_thumbnail_path(
               menu_st->userdata, (unsigned)selection);
      if (menu_st->driver_ctx->update_savestate_thumbnail_image)
         menu_st->driver_ctx->update_savestate_thumbnail_image(menu_st->userdata);
   }

   return 0;
}

static int action_cancel_contentless_core(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_state_get_ptr()->contentless_core_ptr = 0;
   menu_contentless_cores_flush_runtime();
   return action_cancel_pop_default(path, label, type, idx) ;
}

#ifdef HAVE_CHEATS
static int action_cancel_cheat_details(const char *path,
      const char *label, unsigned type, size_t idx)
{
   cheat_manager_copy_working_to_idx(cheat_manager_state.working_cheat.idx) ;
   return action_cancel_pop_default(path, label, type, idx) ;
}
#endif

static const char* find_core_updater_list_flush_target()
{
   struct menu_state* menu_st = menu_state_get_ptr();
   menu_list_t* list = menu_st->entries.list;
   file_list_t const * const menu_list = MENU_LIST_GET(list, 0);
   const size_t menu_stack_size = MENU_LIST_GET_STACK_SIZE(list, 0);
   const char *candidate_label;
   int all_targets_hashes[] = {
      MENU_ENUM_LABEL_ONLINE_UPDATER,
      MENU_ENUM_LABEL_CORE_LIST,
      MENU_ENUM_LABEL_DEFERRED_CORE_LIST,
      MSG_UNKNOWN,
   };

   size_t i;
   int target_idx;
   /* Iterate from the top of the stack to the bottom. If we hit zero we hit the bottom of the stack, can choose as last resort. */
   for(i = menu_stack_size - 1; i > 0; i--)
   {
      candidate_label = menu_list->list[i].label;
      target_idx = 0;
      while (all_targets_hashes[target_idx] != MSG_UNKNOWN)
      {
         if (string_is_equal(candidate_label, msg_hash_to_str(all_targets_hashes[target_idx++]))) return candidate_label;
      }
   }
   return msg_hash_to_str(all_targets_hashes[0]);
}

static int action_cancel_core_list(const char* path,
   const char* label, unsigned type, size_t idx)
{
   /* When we back out of the filtered core list, clear out any filters we used */
   struct menu_state* menu_st = menu_state_get_ptr();
   menu_st->driver_data->deferred_path[0] = '\0';
   menu_st->driver_data->detect_content_path[0] = '\0';
   return action_cancel_pop_default(path, label, type, idx);
}

static int action_cancel_core_content(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label              = NULL;
   size_t online_updater_idx = 0;
   size_t load_core_idx = 0;
   menu_entries_get_last_stack(NULL, &menu_label, NULL, NULL, NULL);

   if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST)))
   {
      menu_search_terms_t *menu_search_terms =
         menu_entries_search_get_terms();

      /* Check whether search terms have been set
       * > If so, remove the last search term */
      if (   menu_search_terms
          && menu_entries_search_pop())
      {
         struct menu_state *menu_st  = menu_state_get_ptr();
         /* Reset navigation pointer */
         menu_st->selection_ptr      = 0;
         if (menu_st->driver_ctx->navigation_set)
            menu_st->driver_ctx->navigation_set(menu_st->userdata, false);
         /* Refresh menu */
         menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                         | MENU_ST_FLAG_PREVENT_POPULATE;
         return 0;
      }


      menu_entries_flush_stack(find_core_updater_list_flush_target(), 0);

   }
   else if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_SYSTEM_FILES_LIST)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ADD_CONTENT_LIST), 0);

   return 0;
}

static int menu_cbs_init_bind_cancel_compare_label(menu_file_list_cbs_t *cbs,
      const char *label)
{
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST)) ||
      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SIDELOAD_CORE_LIST)) ||
      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NO_CORES_AVAILABLE)))
   {
      BIND_ACTION_CANCEL(cbs, action_cancel_core_list);
   }
      
   return -1;
}

static int menu_cbs_init_bind_cancel_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   switch (type)
   {
      case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
      case FILE_TYPE_DOWNLOAD_CORE_SYSTEM_FILES:
      case FILE_TYPE_DOWNLOAD_URL:
      case FILE_TYPE_DOWNLOAD_CORE:
         BIND_ACTION_CANCEL(cbs, action_cancel_core_content);
         return 0;
      case FILE_TYPE_CORE:
         BIND_ACTION_CANCEL(cbs, action_cancel_core_list);
         return 0;
      case MENU_SETTING_ACTION_CONTENTLESS_CORE_RUN:
         BIND_ACTION_CANCEL(cbs, action_cancel_contentless_core);
         return 0;
      default:
         break;
   }

#ifdef HAVE_CHEATS
   switch (cbs->enum_idx)
   {
      case MENU_ENUM_LABEL_CHEAT_IDX:
      case MENU_ENUM_LABEL_CHEAT_STATE:
      case MENU_ENUM_LABEL_CHEAT_DESC:
      case MENU_ENUM_LABEL_CHEAT_HANDLER:
      case MENU_ENUM_LABEL_CHEAT_CODE:
      case MENU_ENUM_LABEL_CHEAT_MEMORY_SEARCH_SIZE:
      case MENU_ENUM_LABEL_CHEAT_TYPE:
      case MENU_ENUM_LABEL_CHEAT_VALUE:
      case MENU_ENUM_LABEL_CHEAT_ADDRESS:
      case MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION:
      case MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT:
      case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS:
      case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE:
      case MENU_ENUM_LABEL_CHEAT_RUMBLE_TYPE:
      case MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE:
      case MENU_ENUM_LABEL_CHEAT_RUMBLE_PORT:
      case MENU_ENUM_LABEL_CHEAT_RUMBLE_PRIMARY_STRENGTH:
      case MENU_ENUM_LABEL_CHEAT_RUMBLE_PRIMARY_DURATION:
      case MENU_ENUM_LABEL_CHEAT_RUMBLE_SECONDARY_STRENGTH:
      case MENU_ENUM_LABEL_CHEAT_RUMBLE_SECONDARY_DURATION:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_COPY_AFTER:
      case MENU_ENUM_LABEL_CHEAT_COPY_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_DELETE:
         {
            BIND_ACTION_CANCEL(cbs, action_cancel_cheat_details);
            break;
         }
      default:
         break;
   }
#endif
   return -1;
}

int menu_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (cbs)
   {
      BIND_ACTION_CANCEL(cbs, action_cancel_pop_default);

      if (menu_cbs_init_bind_cancel_compare_label(cbs, label) == 0)
         return 0;

      if (menu_cbs_init_bind_cancel_compare_type(
               cbs, type) == 0)
         return 0;
   }

   return -1;
}
