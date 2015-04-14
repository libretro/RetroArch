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

#ifndef _DISP_SHARED_H
#define _DISP_SHARED_H

#include "../../settings.h"
#include <string/string_list.h>
#include <time.h>

static INLINE void get_title(const char *label, const char *dir,
      unsigned menu_type, char *title, size_t sizeof_title)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   char elem0_path[PATH_MAX_LENGTH], elem1_path[PATH_MAX_LENGTH];
   struct string_list *list_label = string_split(label, "|");
   struct string_list *list_path  = string_split(dir, "|");

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

#if 0
   RARCH_LOG("label %s, elem0 %s, elem1 %s\n", label, elem0, elem1);
#endif
   if (!strcmp(label, "core_list"))
      snprintf(title, sizeof_title, "CORE SELECTION %s", dir);
   else if (!strcmp(label, "core_updater_list"))
      snprintf(title, sizeof_title, "CORE UPDATER %s", dir);
   else if (!strcmp(label, "deferred_database_manager_list"))
      snprintf(title, sizeof_title, "DATABASE SELECTION - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
   else if (!strcmp(label, "database_manager_list"))
      snprintf(title, sizeof_title, "DATABASE SELECTION");
   else if (!strcmp(label, "cursor_manager_list"))
      snprintf(title, sizeof_title, "DATABASE CURSOR SELECTION");
   else if (!strcmp(label, "deferred_cursor_manager_list"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: DEVELOPER - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: PUBLISHER - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: ORIGIN - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: FRANCHISE - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE ISSUE - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY MONTH - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY YEAR - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: ESRB RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: ELSPA RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: PEGI RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: CERO RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: BBFC RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: MAX USERS - %s)", elem0_path);
   else if (!strcmp(elem0, "deferred_rdb_entry_detail"))
      snprintf(title, sizeof_title, "DATABASE INFO: %s", elem1);
   else if (!strcmp(label, "deferred_core_list"))
      snprintf(title, sizeof_title, "DETECTED CORES %s", dir);
   else if (!strcmp(label, "configurations"))
      snprintf(title, sizeof_title, "CONFIG %s", dir);
   else if (!strcmp(label, "disk_image_append"))
      snprintf(title, sizeof_title, "DISK APPEND %s", dir);
   else if (!strcmp(elem0, "Recording Settings"))
      strlcpy(title, "RECORDING SETTINGS", sizeof_title);
   else if (!strcmp(elem0, "Video Settings"))
   {
      strlcpy(title, "VIDEO SETTINGS", sizeof_title);
      if (!strcmp(elem1, "Monitor"))
         strlcat(title, " - MONITOR", sizeof_title);
      else if (!strcmp(elem1, "Aspect"))
         strlcat(title, " - ASPECT", sizeof_title);
      else if (!strcmp(elem1, "Scaling"))
         strlcat(title, " - SCALING", sizeof_title);
      else if (!strcmp(elem1, "Synchronization"))
         strlcat(title, " - SYNCHRONIZATION", sizeof_title);
      else if (!strcmp(elem1, "Miscellaneous"))
         strlcat(title, " - MISCELLANEOUS", sizeof_title);
      else if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Input Settings") ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
   {
      strlcpy(title, "INPUT SETTINGS", sizeof_title);
      if (strstr(elem1, "User"))
         strlcat(title, " - USER", sizeof_title);
      else if (!strcmp(elem1, "Meta Keys"))
         strlcat(title, " - META KEYS", sizeof_title);
      else if (!strcmp(elem1, "Turbo/Deadzone"))
         strlcat(title, " - TURBO / DEADZONE", sizeof_title);
      else if (!strcmp(elem1, "Input Device Mapping"))
         strlcat(title, " - INPUT DEVICE MAPPING", sizeof_title);
      else if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      else if (!strcmp(elem1, "Miscellaneous"))
         strlcat(title, " - MISCELLANEOUS", sizeof_title);
   }
   else if (!strcmp(elem0, "Overlay Settings"))
   {
      strlcpy(title, "OVERLAY SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Menu Settings"))
   {
      strlcpy(title, "MENU SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      else if (!strcmp(elem1, "Navigation"))
         strlcat(title, " - NAVIGATION", sizeof_title);
      else if (!strcmp(elem1, "Settings View"))
         strlcat(title, " - SETTINGS VIEW", sizeof_title);
      else if (!strcmp(elem1, "Browser"))
         strlcat(title, " - BROWSER", sizeof_title);
   }
   else if (!strcmp(elem0, "Onscreen Keyboard Overlay Settings"))
   {
      strlcpy(title, "ONSCREEN KEYBOARD OVERLAY SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Patch Settings"))
   {
      strlcpy(title, "PATCH SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "UI Settings"))
   {
      strlcpy(title, "UI SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Playlist Settings"))
   {
      strlcpy(title, "PLAYLIST SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      if (!strcmp(elem1, "History"))
         strlcat(title, " - HISTORY", sizeof_title);
   }
   else if (!strcmp(elem0, "Network Settings"))
   {
      strlcpy(title, "NETWORK SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      if (!strcmp(elem1, "Netplay"))
         strlcat(title, " - NETPLAY", sizeof_title);
      if (!strcmp(elem1, "Miscellaneous"))
         strlcat(title, " - MISCELLANEOUS", sizeof_title);
   }
   else if (!strcmp(elem0, "Core Updater Settings"))
   {
      strlcpy(title, "CORE UPDATER SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "User Settings"))
   {
      strlcpy(title, "USER SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Path Settings"))
   {
      strlcpy(title, "PATH SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      if (!strcmp(elem1, "Paths"))
         strlcat(title, " - PATHS", sizeof_title);
   }
   else if (!strcmp(label, "management"))
      strlcpy(title, "MANAGEMENT", sizeof_title);
   else if (!strcmp(label, "options"))
      strlcpy(title, "OPTIONS", sizeof_title);
   else if (!strcmp(label, "settings"))
      strlcpy(title, "SETTINGS", sizeof_title);
   else if (!strcmp(elem0, "Driver Settings"))
   {
      strlcpy(title, "DRIVER SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(label, "performance_counters"))
      strlcpy(title, "PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(label, "frontend_counters"))
      strlcpy(title, "FRONTEND PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(label, "core_counters"))
      strlcpy(title, "CORE PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(elem0, "Shader Settings"))
   {
      strlcpy(title, "SHADER SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Archive Settings"))
   {
      strlcpy(title, "ARCHIVE SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(label, "video_shader_parameters"))
      strlcpy(title, "SHADER PARAMETERS (CURRENT)", sizeof_title);
   else if (!strcmp(label, "video_shader_preset_parameters"))
      strlcpy(title, "SHADER PARAMETERS (MENU PRESET)", sizeof_title);
   else if (!strcmp(elem0, "Font Settings"))
   {
      strlcpy(title, "FONT SETTINGS", sizeof_title);
      if (!strcmp(elem1, "Messages"))
         strlcat(title, " - MESSAGES", sizeof_title);
   }
   else if (!strcmp(elem0, "General Settings"))
   {
      strlcpy(title, "GENERAL SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Audio Settings"))
   {
      strlcpy(title, "AUDIO SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      else if (!strcmp(elem1, "Synchronization"))
         strlcat(title, " - SYNCHRONIZATION", sizeof_title);
      else if (!strcmp(elem1, "Miscellaneous"))
         strlcat(title, " - MISCELLANEOUS", sizeof_title);
   }
   else if (!strcmp(label, "disk_options"))
      strlcpy(title, "DISK OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_options"))
      strlcpy(title, "CORE OPTIONS", sizeof_title);
   else if (!strcmp(label, "shader_options"))
      strlcpy(title, "SHADER OPTIONS", sizeof_title);
   else if (!strcmp(label, "video_options"))
      strlcpy(title, "VIDEO OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_cheat_options"))
      strlcpy(title, "CORE CHEAT OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_input_remapping_options"))
      strlcpy(title, "CORE INPUT REMAPPING OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_information"))
      strlcpy(title, "CORE INFO", sizeof_title);
   else if (!strcmp(elem0, "Privacy Settings"))
   {
      strlcpy(title, "PRIVACY SETTINGS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(label, "video_shader_pass"))
      snprintf(title, sizeof_title, "SHADER %s", dir);
   else if (!strcmp(label, "video_shader_preset"))
      snprintf(title, sizeof_title, "SHADER PRESET %s", dir);
   else if (!strcmp(label, "cheat_file_load"))
      snprintf(title, sizeof_title, "CHEAT FILE %s", dir);
   else if (!strcmp(label, "remap_file_load"))
      snprintf(title, sizeof_title, "REMAP FILE %s", dir);
   else if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT ||
         !strcmp(label, "custom_viewport_2") ||
         !strcmp(label, "help") ||
         menu_type == MENU_SETTINGS)
      snprintf(title, sizeof_title, "MENU %s", dir);
   else if (!strcmp(label, "history_list"))
      strlcpy(title, "LOAD HISTORY", sizeof_title);
   else if (!strcmp(label, "info_screen"))
      strlcpy(title, "INFO", sizeof_title);
   else if (!strcmp(label, "input_overlay"))
      snprintf(title, sizeof_title, "OVERLAY %s", dir);
   else if (!strcmp(label, "video_font_path"))
      snprintf(title, sizeof_title, "FONT %s", dir);
   else if (!strcmp(label, "video_filter"))
      snprintf(title, sizeof_title, "FILTER %s", dir);
   else if (!strcmp(label, "audio_dsp_plugin"))
      snprintf(title, sizeof_title, "DSP FILTER %s", dir);
   else if (!strcmp(label, "rgui_browser_directory"))
      snprintf(title, sizeof_title, "BROWSER DIR %s", dir);
   else if (!strcmp(label, "playlist_directory"))
      snprintf(title, sizeof_title, "PLAYLIST DIR %s", dir);
   else if (!strcmp(label, "content_directory"))
      snprintf(title, sizeof_title, "CONTENT DIR %s", dir);
   else if (!strcmp(label, "screenshot_directory"))
      snprintf(title, sizeof_title, "SCREENSHOT DIR %s", dir);
   else if (!strcmp(label, "video_shader_dir"))
      snprintf(title, sizeof_title, "SHADER DIR %s", dir);
   else if (!strcmp(label, "video_filter_dir"))
      snprintf(title, sizeof_title, "FILTER DIR %s", dir);
   else if (!strcmp(label, "audio_filter_dir"))
      snprintf(title, sizeof_title, "DSP FILTER DIR %s", dir);
   else if (!strcmp(label, "savestate_directory"))
      snprintf(title, sizeof_title, "SAVESTATE DIR %s", dir);
   else if (!strcmp(label, "libretro_dir_path"))
      snprintf(title, sizeof_title, "LIBRETRO DIR %s", dir);
   else if (!strcmp(label, "libretro_info_path"))
      snprintf(title, sizeof_title, "LIBRETRO INFO DIR %s", dir);
   else if (!strcmp(label, "rgui_config_directory"))
      snprintf(title, sizeof_title, "CONFIG DIR %s", dir);
   else if (!strcmp(label, "savefile_directory"))
      snprintf(title, sizeof_title, "SAVEFILE DIR %s", dir);
   else if (!strcmp(label, "overlay_directory"))
      snprintf(title, sizeof_title, "OVERLAY DIR %s", dir);
   else if (!strcmp(label, "system_directory"))
      snprintf(title, sizeof_title, "SYSTEM DIR %s", dir);
   else if (!strcmp(label, "assets_directory"))
      snprintf(title, sizeof_title, "ASSETS DIR %s", dir);
   else if (!strcmp(label, "extraction_directory"))
      snprintf(title, sizeof_title, "EXTRACTION DIR %s", dir);
   else if (!strcmp(label, "joypad_autoconfig_dir"))
      snprintf(title, sizeof_title, "AUTOCONFIG DIR %s", dir);
   else
   {
      driver_t *driver = driver_get_ptr();

      if (driver->menu->defer_core)
         snprintf(title, sizeof_title, "CONTENT %s", dir);
      else
      {
         global_t *global      = global_get_ptr();
         const char *core_name = global->menu.info.library_name;

         if (!core_name)
            core_name = global->system.info.library_name;
         if (!core_name)
            core_name = "No Core";
         snprintf(title, sizeof_title, "CONTENT (%s) %s", core_name, dir);
      }
   }
}

static INLINE void disp_timedate_set_label(char *label, size_t label_size,
      unsigned time_mode)
{
   time_t time_;
   time(&time_);

   switch (time_mode)
   {
      case 0: /* Date and time */
         strftime(label, label_size, "%Y-%m-%d %H:%M:%S", localtime(&time_));
         break;
      case 1: /* Date */
         strftime(label, label_size, "%Y-%m-%d", localtime(&time_));
         break;
      case 2: /* Time */
         strftime(label, label_size, "%H:%M:%S", localtime(&time_));
         break;
      case 3: /* Time (hours-minutes) */
         strftime(label, label_size, "%H:%M", localtime(&time_));
         break;
   }
}
#endif
