/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
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

#include <stdint.h>
#include <string.h>
#include <rhash.h>

#include "menu_hash.h"

#include "../configuration.h"

static const char *menu_hash_to_str_french(uint32_t hash)
{
   switch (hash)
   {
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_dutch(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core Informatie";
      case MENU_LABEL_CORE_LIST:
         return "Laad Core";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Instellingen";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Opties";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Systeem Informatie";
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_english(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_REWIND_GRANULARITY:
         return "rewind_granularity";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Rewind Granularity";
      case MENU_LABEL_REMAP_FILE_LOAD:
         return "remap_file_load";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Load Remap File";
      case MENU_LABEL_REMAP_FILE_SAVE_AS:
         return "remap_file_save_as";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Save Remap File As";
      case MENU_LABEL_CUSTOM_RATIO:
         return "custom_ratio";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Custom Ratio";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Use this directory>";
      case MENU_LABEL_USE_THIS_DIRECTORY:
         return "use_this_directory";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Start Content";
      case MENU_LABEL_RDB_ENTRY_START_CONTENT:
         return "rdb_entry_start_content";
      case MENU_LABEL_CUSTOM_BIND:
         return "custom_bind";
      case MENU_LABEL_CUSTOM_BIND_ALL:
         return "custom_bind_all";
      case MENU_LABEL_DISK_OPTIONS:
         return "core_disk_options";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Core Disk Options";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Core Options";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Core Cheat Options";
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
         return "core_cheat_options";
      case MENU_LABEL_CORE_OPTIONS:
         return "core_options";
      case MENU_LABEL_DATABASE_MANAGER_LIST:
         return "database_manager_list";
      case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
         return "deferred_database_manager_list";
      case MENU_LABEL_CURSOR_MANAGER_LIST:
         return "cursor_manager_list";
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
         return "deferred_cursor_manager_list";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Cheat File Load";
      case MENU_LABEL_CHEAT_FILE_LOAD:
         return "cheat_file_load";
      case MENU_LABEL_CHEAT_FILE_SAVE_AS:
         return "cheat_file_save_as";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Cheat File Save As";
      case MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
         return "deferred_rdb_entry_detail";
      case MENU_LABEL_FRONTEND_COUNTERS:
         return "frontend_counters";
      case MENU_LABEL_CORE_COUNTERS:
         return "core_counters";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Core Counters";
      case MENU_LABEL_DISK_CYCLE_TRAY_STATUS:
         return "disk_cycle_tray_status";
      case MENU_LABEL_DISK_IMAGE_APPEND:
         return "disk_image_append";
      case MENU_LABEL_DEFERRED_CORE_LIST:
         return "deferred_core_list";
      case MENU_LABEL_DEFERRED_CORE_LIST_SET:
         return "deferred_core_list_set";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Take Screenshot";
      case MENU_LABEL_INFO_SCREEN:
         return "info_screen";
      case MENU_LABEL_VALUE_RESUME:
         return "Resume";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Disk Index";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Frontend Counters";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Disk Image Append";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Disk Cycle Tray Status";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "No playlist entries available.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "No core information available.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "No core options available.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "No cores available.";
      case MENU_VALUE_NO_CORE:
         return "No Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Database Manager";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Cursor Manager";
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Recording Settings";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_SETTINGS:
         return "settings";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Settings";
      case MENU_LABEL_QUIT_RETROARCH:
         return "quit_retroarch";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Quit RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Help";
      case MENU_LABEL_HELP:
         return "help";
      case MENU_LABEL_SAVE_NEW_CONFIG:
         return "save_new_config";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Save New Config";
      case MENU_LABEL_RESTART_CONTENT:
         return "restart_content";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Restart Content";
      case MENU_LABEL_TAKE_SCREENSHOT:
         return "take_screenshot";
      case MENU_LABEL_CORE_UPDATER_LIST:
         return "core_updater_list";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Core Updater";
      case MENU_LABEL_SYSTEM_INFORMATION:
         return "system_information";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "System Information";
      case MENU_LABEL_OPTIONS:
         return "options";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Options";
      case MENU_LABEL_CORE_INFORMATION:
         return "core_information";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core Information";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Directory not found.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "No items.";
      case MENU_LABEL_CORE_LIST:
         return "Load Core";
      case MENU_LABEL_LOAD_CONTENT:
         return "load_content";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Load Content";
      case MENU_LABEL_UNLOAD_CORE:
         return "unload_core";
      case MENU_LABEL_VALUE_UNLOAD_CORE:
         return "Unload Core";
      case MENU_LABEL_MANAGEMENT:
         return "advanced_management";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Advanced Management";
      case MENU_LABEL_PERFORMANCE_COUNTERS:
         return "performance_counters";
      case MENU_LABEL_VALUE_PERFORMANCE_COUNTERS:
         return "Performance Counters";
      case MENU_LABEL_SAVE_STATE:
         return "savestate";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Save State";
      case MENU_LABEL_LOAD_STATE:
         return "loadstate";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Load State";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Resume Content";
      case MENU_LABEL_RESUME_CONTENT:
         return "resume_content";
      case MENU_LABEL_DRIVER_SETTINGS:
         return "Driver Settings";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Unable to read compressed file.";
      case MENU_LABEL_OVERLAY_SCALE:
         return "input_overlay_scale";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Overlay Scale";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Preset";
      case MENU_LABEL_OVERLAY_PRESET:
         return "input_overlay";
      case MENU_LABEL_KEYBOARD_OVERLAY_PRESET:
         return "input_osk_overlay";
      case MENU_LABEL_AUDIO_DEVICE:
         return "audio_device";
      case MENU_LABEL_AUDIO_LATENCY:
         return "audio_latency";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Audio Latency (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Audio Device";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Keyboard Overlay Preset";
      case MENU_LABEL_OVERLAY_OPACITY:
         return "input_overlay_opacity";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Overlay Opacity";
      case MENU_LABEL_MENU_WALLPAPER:
         return "menu_wallpaper";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Menu Wallpaper";
      case MENU_LABEL_DYNAMIC_WALLPAPER:
         return "menu_dynamic_wallpaper_enable";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Dynamic Wallpaper";
      case MENU_LABEL_VALUE_BOXART:
         return "Display Boxart";
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return "core_input_remapping_options";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Core Input Remapping Options";
      case MENU_LABEL_VIDEO_OPTIONS:
         return "video_options";
      case MENU_LABEL_VALUE_VIDEO_OPTIONS:
         return "Video Options";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Shader Options";
      case MENU_LABEL_SHADER_OPTIONS:
         return "shader_options";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No shader parameters.";
      case MENU_LABEL_VIDEO_FILTER:
         return "video_filter";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Video Filter";
      case MENU_LABEL_DEFERRED_VIDEO_FILTER:
         return "deferred_video_filter";
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
         return "deferred_core_updater_list";
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         return "audio_dsp_plugin";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Audio DSP Plugin";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Starting download: ";
      case MENU_VALUE_SECONDS:
         return "seconds";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      default:
         break;
   }

   return "null";
}

const char *menu_hash_to_str(uint32_t hash)
{
   const char *ret = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return "null";

   switch (settings->user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         ret = menu_hash_to_str_french(hash);
         break;
      case RETRO_LANGUAGE_DUTCH:
         ret = menu_hash_to_str_dutch(hash);
         break;
      default:
         break;
   }

   if (ret && strcmp(ret, "null") != 0)
      return ret;

   return menu_hash_to_str_english(hash);
}

uint32_t menu_hash_calculate(const char *s)
{
   return djb2_calculate(s);
}
