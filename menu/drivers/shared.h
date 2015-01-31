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

#include "../../settings_data.h"
#include <string/string_list.h>
#include <time.h>

static INLINE void get_title(const char *label, const char *dir,
      unsigned menu_type, char *title, size_t sizeof_title)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   char elem0_path[PATH_MAX_LENGTH], elem1_path[PATH_MAX_LENGTH];
   struct string_list *list_label = string_split(label, "|");
   struct string_list *list_path  = string_split(dir, "|");

   if (list_label && list_label->size > 0)
   {
      strlcpy(elem0, list_label->elems[0].data, sizeof(elem0));
      if (list_label->size > 1)
         strlcpy(elem1, list_label->elems[1].data, sizeof(elem1));
      string_list_free(list_label);
   }

   if (list_path && list_path->size > 0)
   {
      strlcpy(elem0_path, list_path->elems[0].data, sizeof(elem0_path));
      if (list_path->size > 1)
         strlcpy(elem1_path, list_path->elems[1].data, sizeof(elem1_path));
      string_list_free(list_path);
   }

#if 0
   RARCH_LOG("label %s, elem0 %s, elem1 %s\n", label, elem0, elem1);
#endif
   if (!strcmp(label, "core_list"))
      snprintf(title, sizeof_title, "CORE SELECTION %s", dir);
   else if (!strcmp(label, "core_manager_list"))
      snprintf(title, sizeof_title, "CORE UPDATER %s", dir);
   else if (!strcmp(label, "database_manager_list"))
      snprintf(title, sizeof_title, "DATABASE SELECTION %s", dir);
   else if (!strcmp(label, "cursor_manager_list"))
      snprintf(title, sizeof_title, "DATABASE CURSOR SELECTION %s", dir);
   else if (!strcmp(label, "deferred_cursor_manager_list"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST %s", dir);
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
   else if (!strcmp(elem0, "Video Options"))
   {
      strlcpy(title, "VIDEO OPTIONS", sizeof_title);
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
   else if (!strcmp(elem0, "Input Options") ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
   {
      strlcpy(title, "INPUT OPTIONS", sizeof_title);
      if (strstr(elem1, "User"))
         strlcat(title, " - USER", sizeof_title);
      else if (!strcmp(elem1, "Meta Keys"))
         strlcat(title, " - META KEYS", sizeof_title);
      else if (!strcmp(elem1, "Turbo/Deadzone"))
         strlcat(title, " - TURBO / DEADZONE", sizeof_title);
      else if (!strcmp(elem1, "Joypad Mapping"))
         strlcat(title, " - JOYPAD MAPPING", sizeof_title);
      else if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      else if (!strcmp(elem1, "Miscellaneous"))
         strlcat(title, " - MISCELLANEOUS", sizeof_title);
   }
   else if (!strcmp(elem0, "Overlay Options"))
   {
      strlcpy(title, "OVERLAY OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Menu Options"))
   {
      strlcpy(title, "MENU OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      else if (!strcmp(elem1, "Navigation"))
         strlcat(title, " - NAVIGATION", sizeof_title);
      else if (!strcmp(elem1, "Settings View"))
         strlcat(title, " - SETTINGS VIEW", sizeof_title);
      else if (!strcmp(elem1, "Browser"))
         strlcat(title, " - BROWSER", sizeof_title);
   }
   else if (!strcmp(elem0, "Onscreen Keyboard Overlay Options"))
   {
      strlcpy(title, "ONSCREEN KEYBOARD OVERLAY OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Patch Options"))
   {
      strlcpy(title, "PATCH OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "UI Options"))
   {
      strlcpy(title, "UI OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Playlist Options"))
   {
      strlcpy(title, "PLAYLIST OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      if (!strcmp(elem1, "History"))
         strlcat(title, " - HISTORY", sizeof_title);
   }
   else if (!strcmp(elem0, "Network Options"))
   {
      strlcpy(title, "NETWORK OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      if (!strcmp(elem1, "Netplay"))
         strlcat(title, " - NETPLAY", sizeof_title);
      if (!strcmp(elem1, "Miscellaneous"))
         strlcat(title, " - MISCELLANEOUS", sizeof_title);
   }
   else if (!strcmp(elem0, "Core Manager Options"))
   {
      strlcpy(title, "CORE UPDATER OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "User Options"))
   {
      strlcpy(title, "USER OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Path Options"))
   {
      strlcpy(title, "PATH OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
      if (!strcmp(elem1, "Paths"))
         strlcat(title, " - PATHS", sizeof_title);
   }
   else if (!strcmp(label, "settings"))
      strlcpy(title, "SETTINGS", sizeof_title);
   else if (!strcmp(elem0, "Driver Options"))
   {
      strlcpy(title, "DRIVER OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(label, "performance_counters"))
      strlcpy(title, "PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(label, "frontend_counters"))
      strlcpy(title, "FRONTEND PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(label, "core_counters"))
      strlcpy(title, "CORE PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(elem0, "Shader Options"))
   {
      strlcpy(title, "SHADER OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Archive Options"))
   {
      strlcpy(title, "ARCHIVE OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(label, "video_shader_parameters"))
      strlcpy(title, "SHADER PARAMETERS (CURRENT)", sizeof_title);
   else if (!strcmp(label, "video_shader_preset_parameters"))
      strlcpy(title, "SHADER PARAMETERS (MENU PRESET)", sizeof_title);
   else if (!strcmp(elem0, "Font Options"))
   {
      strlcpy(title, "FONT OPTIONS", sizeof_title);
      if (!strcmp(elem1, "Messages"))
         strlcat(title, " - MESSAGES", sizeof_title);
   }
   else if (!strcmp(elem0, "General Options"))
   {
      strlcpy(title, "GENERAL OPTIONS", sizeof_title);
      if (!strcmp(elem1, "State"))
         strlcat(title, " - STATE", sizeof_title);
   }
   else if (!strcmp(elem0, "Audio Options"))
   {
      strlcpy(title, "AUDIO OPTIONS", sizeof_title);
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
   else if (!strcmp(label, "core_cheat_options"))
      strlcpy(title, "CORE CHEAT OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_input_remapping_options"))
      strlcpy(title, "CORE INPUT REMAPPING OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_information"))
      strlcpy(title, "CORE INFO", sizeof_title);
   else if (!strcmp(elem0, "Privacy Options"))
   {
      strlcpy(title, "PRIVACY OPTIONS", sizeof_title);
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
      if (driver.menu->defer_core)
         snprintf(title, sizeof_title, "CONTENT %s", dir);
      else
      {
         const char *core_name = g_extern.menu.info.library_name;
         if (!core_name)
            core_name = g_extern.system.info.library_name;
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

static INLINE void disp_set_label(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';
   *w = 19;

   if (!strcmp(label, "performance_counters"))
      *w = 28;

   if (!strcmp(label, "history_list"))
      *w = 6;

   if (type == MENU_FILE_CORE)
   {
      strlcpy(type_str, "(CORE)", type_str_size);
      menu_list_get_alt_at_offset(list, i, &path);
      *w = 6;
   }
   else if (type == MENU_FILE_PLAIN)
   {
      strlcpy(type_str, "(FILE)", type_str_size);
      *w = 6;
   }
   else if (type == MENU_FILE_USE_DIRECTORY)
   {
      *type_str = '\0';
      *w = 0;
   }
   else if (type == MENU_FILE_DIRECTORY)
   {
      strlcpy(type_str, "(DIR)", type_str_size);
      *w = 5;
   }
   else if (type == MENU_FILE_CARCHIVE)
   {
      strlcpy(type_str, "(COMP)", type_str_size);
      *w = 6;
   }
   else if (type == MENU_FILE_IN_CARCHIVE)
   {
      strlcpy(type_str, "(CFILE)", type_str_size);
      *w = 7;
   }
   else if (type == MENU_FILE_FONT)
   {
      strlcpy(type_str, "(FONT)", type_str_size);
      *w = 7;
   }
   else if (type == MENU_FILE_SHADER_PRESET)
   {
      strlcpy(type_str, "(PRESET)", type_str_size);
      *w = 8;
   }
   else if (type == MENU_FILE_SHADER)
   {
      strlcpy(type_str, "(SHADER)", type_str_size);
      *w = 8;
   }
   else if (
         type == MENU_FILE_VIDEOFILTER ||
         type == MENU_FILE_AUDIOFILTER)
   {
      strlcpy(type_str, "(FILTER)", type_str_size);
      *w = 8;
   }
   else if (type == MENU_FILE_CONFIG)
   {
      strlcpy(type_str, "(CONFIG)", type_str_size);
      *w = 8;
   }
   else if (type == MENU_FILE_OVERLAY)
   {
      strlcpy(type_str, "(OVERLAY)", type_str_size);
      *w = 9;
   }
   else if (type >= MENU_SETTINGS_CORE_OPTION_START)
      strlcpy(
            type_str,
            core_option_get_val(g_extern.system.core_options,
               type - MENU_SETTINGS_CORE_OPTION_START),
            type_str_size);
   else
      setting_data_get_label(list, type_str,
            type_str_size, w, type, label, entry_label, i);

   strlcpy(path_buf, path, path_buf_size);
}

#endif
