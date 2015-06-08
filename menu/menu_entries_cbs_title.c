/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <string/string_list.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include <rhash.h>

#include "menu.h"
#include "menu_entries_cbs.h"

static INLINE void replace_chars(char *str, char c1, char c2)
{
   char *pos;
   while((pos = strchr(str, c1)))
      *pos = c2;
}

static INLINE void sanitize_to_string(char *s, const char *label, size_t len)
{
   char new_label[PATH_MAX_LENGTH];
   strlcpy(new_label, label, sizeof(new_label));
   strlcpy(s, string_to_upper(new_label), len);
   replace_chars(s, '_', ' ');
}

static int action_get_title_default(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   uint32_t label_hash = 0;
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   char elem0_path[PATH_MAX_LENGTH], elem1_path[PATH_MAX_LENGTH];
   struct string_list *list_label = string_split(label, "|");
   struct string_list *list_path  = string_split(path, "|");
   driver_t               *driver = driver_get_ptr();

   *elem0 = *elem1 = *elem0_path = *elem1_path = 0;

   if (list_label)
   {
      if (list_label->size > 0)
      {
         strlcpy(elem0, list_label->elems[0].data, sizeof(elem0));
         if (list_label->size > 1)
            strlcpy(elem1, list_label->elems[1].data, sizeof(elem1));
      }
      string_list_free(list_label);
   }

   if (list_path)
   {
      if (list_path->size > 0)
      {
         strlcpy(elem0_path, list_path->elems[0].data, sizeof(elem0_path));
         if (list_path->size > 1)
            strlcpy(elem1_path, list_path->elems[1].data, sizeof(elem1_path));
      }
      string_list_free(list_path);
   }

   label_hash = djb2_calculate(label);

#if 0
   RARCH_LOG("label %s, elem0 %s, elem1 %s\n", label, elem0, elem1);
#endif

   switch (label_hash)
   {
      case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
         snprintf(s, len, "DATABASE SELECTION - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
         snprintf(s, len, "DATABASE CURSOR LIST - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: DEVELOPER - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: PUBLISHER - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: ORIGIN - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: FRANCHISE - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE RATING - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE ISSUE - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY MONTH - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY YEAR - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: ESRB RATING - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: ELSPA RATING - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: PEGI RATING - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: CERO RATING - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: BBFC RATING - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: MAX USERS - %s)", elem0_path);
         break;
      case MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
         snprintf(s, len, "DATABASE INFO: %s", elem1);
         break;
      case MENU_LABEL_DEFERRED_CORE_LIST:
         snprintf(s, len, "DETECTED CORES %s", path);
         break;
      case MENU_LABEL_CONFIGURATIONS:
         snprintf(s, len, "CONFIG %s", path);
         break;
      case MENU_LABEL_DISK_IMAGE_APPEND:
         snprintf(s, len, "DISK APPEND %s", path);
         break;
      case MENU_LABEL_PERFORMANCE_COUNTERS:
      case MENU_LABEL_CORE_LIST:
      case MENU_LABEL_MANAGEMENT:
      case MENU_LABEL_OPTIONS:
      case MENU_LABEL_SETTINGS:
      case MENU_LABEL_FRONTEND_COUNTERS:
      case MENU_LABEL_CORE_COUNTERS:
      case MENU_LABEL_HISTORY_LIST:
      case MENU_LABEL_INFO_SCREEN:
      case MENU_LABEL_SYSTEM_INFORMATION:
      case MENU_LABEL_CORE_INFORMATION:
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
      case MENU_LABEL_DISK_OPTIONS:
      case MENU_LABEL_CORE_OPTIONS:
      case MENU_LABEL_SHADER_OPTIONS:
      case MENU_LABEL_VIDEO_OPTIONS:
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
      case MENU_LABEL_DATABASE_MANAGER_LIST:
      case MENU_LABEL_CURSOR_MANAGER_LIST:
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
         sanitize_to_string(s, label, len);
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         snprintf(s, len, "SHADER %s", path);
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         snprintf(s, len, "SHADER PRESET %s", path);
         break;
      case MENU_LABEL_CHEAT_FILE_LOAD:
         snprintf(s, len, "CHEAT FILE %s", path);
         break;
      case MENU_LABEL_REMAP_FILE_LOAD:
         snprintf(s, len, "REMAP FILE %s", path);
         break;
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
         snprintf(s, len, "CONTENT COLLECTION LIST");
         break;
      case MENU_LABEL_CUSTOM_VIEWPORT_2:
      case MENU_LABEL_HELP:
         snprintf(s, len, "MENU %s", path);
         break;
      case MENU_LABEL_INPUT_OVERLAY:
         snprintf(s, len, "OVERLAY %s", path);
         break;
      case MENU_LABEL_VIDEO_FONT_PATH:
         snprintf(s, len, "FONT %s", path);
         break;
      case MENU_LABEL_VIDEO_FILTER:
         snprintf(s, len, "FILTER %s", path);
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len, "DSP FILTER %s", path);
         break;
      case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
         snprintf(s, len, "BROWSER DIR %s", path);
         break;
      case MENU_LABEL_PLAYLIST_DIRECTORY:
         snprintf(s, len, "PLAYLIST DIR %s", path);
         break;
      case MENU_LABEL_CONTENT_DIRECTORY:
         snprintf(s, len, "CONTENT DIR %s", path);
         break;
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len, "SCREENSHOT DIR %s", path);
         break;
      case MENU_LABEL_VIDEO_SHADER_DIR:
         snprintf(s, len, "SHADER DIR %s", path);
         break;
      case MENU_LABEL_VIDEO_FILTER_DIR:
         snprintf(s, len, "VIDEO FILTER DIR %s", path);
         break;
      case MENU_LABEL_AUDIO_FILTER_DIR:
         snprintf(s, len, "DSP FILTER DIR %s", path);
         break;
      case MENU_LABEL_CURSOR_DIRECTORY:
         snprintf(s, len, "CURSOR DIR %s", path);
         break;
      case MENU_LABEL_RECORDING_CONFIG_DIRECTORY:
         snprintf(s, len, "RECORDING CONFIG DIR %s", path);
         break;
      case MENU_LABEL_RECORDING_OUTPUT_DIRECTORY:
         snprintf(s, len, "RECORDING OUTPUT DIR %s", path);
         break;
      case MENU_LABEL_OSK_OVERLAY_DIRECTORY:
         snprintf(s, len, "OSK OVERLAY DIR %s", path);
         break;
      case MENU_LABEL_CHEAT_DATABASE_PATH:
         snprintf(s, len, "CHEAT DIR %s", path);
         break;
      case MENU_LABEL_INPUT_REMAPPING_DIRECTORY:
         snprintf(s, len, "INPUT REMAPPING DIR %s", path);
         break;
      case MENU_LABEL_CONTENT_DATABASE_DIRECTORY:
         snprintf(s, len, "DATABASE DIR %s", path);
         break;
      case MENU_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len, "SAVESTATE DIR %s", path);
         break;
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len, "DYNAMIC WALLPAPERS DIR %s", path);
         break;
      case MENU_LABEL_CORE_ASSETS_DIRECTORY:
         snprintf(s, len, "CORE ASSETS DIR %s", path);
         break;
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len, "CORE DIR %s", path);
         break;
      case MENU_LABEL_LIBRETRO_INFO_PATH:
         snprintf(s, len, "CORE INFO DIR %s", path);
         break;
      case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
         snprintf(s, len, "CONFIG DIR %s", path);
         break;
      case MENU_LABEL_SAVEFILE_DIRECTORY:
         snprintf(s, len, "SAVEFILE DIR %s", path);
         break;
      case MENU_LABEL_OVERLAY_DIRECTORY:
         snprintf(s, len, "OVERLAY DIR %s", path);
         break;
      case MENU_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len, "SYSTEM DIR %s", path);
         break;
      case MENU_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len, "ASSETS DIR %s", path);
         break;
      case MENU_LABEL_EXTRACTION_DIRECTORY:
         snprintf(s, len, "EXTRACTION DIR %s", path);
         break;
      case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
         snprintf(s, len, "AUTOCONFIG DIR %s", path);
         break;
      default:
         switch (menu_type)
         {
            case MENU_SETTINGS_CUSTOM_VIEWPORT:
            case MENU_SETTINGS:
               snprintf(s, len, "MENU %s", path);
               break;
            case MENU_SETTINGS_CUSTOM_BIND:
            case MENU_SETTINGS_CUSTOM_BIND_KEYBOARD:
               strlcpy(s, "INPUT SETTINGS", len);
               if (elem1[0] != '\0')
               {
                  strlcat(s, " - ", len);
                  strlcat(s, string_to_upper(elem1), len);
               }
               break;
            default:
               if (driver->menu->defer_core)
                  snprintf(s, len, "CONTENT %s", path);
               else
               {
                  global_t *global      = global_get_ptr();
                  const char *core_name = global->menu.info.library_name;

                  if (!core_name)
                     core_name = global->system.info.library_name;
                  if (!core_name)
                     core_name = "No Core";
                  snprintf(s, len, "CONTENT (%s) %s", core_name, path);
               }
               break;
         }
         break;
   }
   
   return 0;
}

static int action_get_title_group_settings(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   struct string_list *list_label = string_split(label, "|");

   if (list_label)
   {
      if (list_label->size > 0)
      {
         strlcpy(elem0, list_label->elems[0].data, sizeof(elem0));
         if (list_label->size > 1)
            strlcpy(elem1, list_label->elems[1].data, sizeof(elem1));
      }
      string_list_free(list_label);
   }

   strlcpy(s, string_to_upper(elem0), len);
   if (elem1[0] != '\0')
   {
      strlcat(s, " - ", len);
      strlcat(s, string_to_upper(elem1), len);
   }
   return 0;
}

int menu_entries_cbs_init_bind_title(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   driver_t               *driver = driver_get_ptr();
   rarch_setting_t *setting = NULL;

   if (!cbs)
      return -1;

   cbs->action_get_title = action_get_title_default;

#if 0
   RARCH_LOG("label %s, elem0 %s, elem1 %s\n", label, elem0, elem1);
#endif

   setting = menu_setting_find(label);

   (void)setting;
   if (setting)
   {
      if (!strcmp(setting->parent_group, "Main Menu") && setting->type == ST_GROUP)
      {
         cbs->action_get_title = action_get_title_group_settings;
         return 0;
      }
   }

   return -1;
}
