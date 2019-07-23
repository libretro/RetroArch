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
#include "../../configuration.h"
#include "../../msg_hash.h"
#include "../../managers/cheat_manager.h"

#include "../widgets/menu_filebrowser.h"

#ifndef BIND_ACTION_CANCEL
#define BIND_ACTION_CANCEL(cbs, name) \
   cbs->action_cancel = name; \
   cbs->action_cancel_ident = #name;
#endif

/* Clicks the back button */
int action_cancel_pop_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   size_t new_selection_ptr;
   const char *menu_label       = NULL;
#ifdef HAVE_AUDIOMIXER
   settings_t *settings         = config_get_ptr();
   if (settings->bools.audio_enable_menu && settings->bools.audio_enable_menu_cancel)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
#endif

   menu_entries_get_last_stack(NULL, &menu_label, NULL, NULL, NULL);

   if (!string_is_empty(menu_label))
   {
      if (
         string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)
               ) ||
         string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_WALLPAPER)
               )
         )
         filebrowser_clear_type();
   }

   new_selection_ptr = menu_navigation_get_selection();
   menu_entries_pop_stack(&new_selection_ptr, 0, 1);
   menu_navigation_set_selection(new_selection_ptr);

   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH, NULL);
   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE, NULL);

   return 0;
}

static int action_cancel_cheat_details(const char *path,
      const char *label, unsigned type, size_t idx)
{
   cheat_manager_copy_working_to_idx(cheat_manager_state.working_cheat.idx) ;
   return action_cancel_pop_default(path, label, type, idx) ;
}

static int action_cancel_core_content(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label              = NULL;

   menu_entries_get_last_stack(NULL, &menu_label, NULL, NULL, NULL);

   if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST)))
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER), 0);
   else
      menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ADD_CONTENT_LIST), 0);

   return 0;
}

static int menu_cbs_init_bind_cancel_compare_label(menu_file_list_cbs_t *cbs,
      const char *label)
{
   return -1;
}

static int menu_cbs_init_bind_cancel_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   switch (type)
   {
      case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
      case FILE_TYPE_DOWNLOAD_URL:
      case FILE_TYPE_DOWNLOAD_CORE:
         BIND_ACTION_CANCEL(cbs, action_cancel_core_content);
         return 0;
   }

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
         break ;
      }
      default :
      {
         break ;
      }
   }
   return -1;
}

int menu_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_CANCEL(cbs, action_cancel_pop_default);

   if (menu_cbs_init_bind_cancel_compare_label(cbs, label) == 0)
      return 0;

   if (menu_cbs_init_bind_cancel_compare_type(
            cbs, type) == 0)
      return 0;

   return -1;
}
