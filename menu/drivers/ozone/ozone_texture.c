/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
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

#include "ozone.h"
#include "ozone_texture.h"

#include <streams/file_stream.h>
#include <file/file_path.h>

#include "../../menu_driver.h"

#include "../../../cheevos/badges.h"

menu_texture_item ozone_entries_icon_get_texture(ozone_handle_t *ozone,
      enum msg_hash_enums enum_idx, unsigned type, bool active)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CORE_OPTIONS:
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE];
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
      case MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS:
      case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
      case MENU_ENUM_LABEL_DISK_INDEX:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS];
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_SAVE_STATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_LOAD_STATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT];
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_RENAME_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RENAME];
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_FAVORITES:
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];


      /* Menu collection submenus*/
      case MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ZIP];
      case MENU_ENUM_LABEL_GOTO_FAVORITES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE];
      case MENU_ENUM_LABEL_GOTO_IMAGES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_GOTO_VIDEO:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
      case MENU_ENUM_LABEL_GOTO_MUSIC:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MUSIC];

      /* Menu icons */
      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU];
      case MENU_ENUM_LABEL_START_CORE:
      case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
      case MENU_ENUM_LABEL_CORE_LIST:
      case MENU_ENUM_LABEL_CORE_SETTINGS:
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SCAN_FILE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case MENU_ENUM_LABEL_ONLINE_UPDATER:
      case MENU_ENUM_LABEL_UPDATER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UPDATER];
      case MENU_ENUM_LABEL_UPDATE_LAKKA:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU];
      case MENU_ENUM_LABEL_UPDATE_CHEATS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY];
      case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
      case MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_INFORMATION:
      case MENU_ENUM_LABEL_INFORMATION_LIST:
      case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
      case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INFO];
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
      case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CURSOR];
      case MENU_ENUM_LABEL_HELP_LIST:
      case MENU_ENUM_LABEL_HELP_CONTROLS:
      case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
      case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
      case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
      case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
      case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_HELP];
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_EXIT];
      /* Settings icons*/
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS];
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_VIDEO];
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_AUDIO];
      case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MIXER];
      case MENU_ENUM_LABEL_INPUT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
      case MENU_ENUM_LABEL_INPUT_USER_1_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_2_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_3_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_4_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_5_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_6_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_7_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_8_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_9_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_10_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_11_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_12_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_13_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_14_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_15_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_16_BINDS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
      case MENU_ENUM_LABEL_LATENCY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LATENCY];
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOG];
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP];
      case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
      case MENU_ENUM_LABEL_RECORDING_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RECORD];
      case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_STREAM];
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING:
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING:
      case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_CORE_DELETE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OSD];
      case MENU_ENUM_LABEL_SHOW_WIMP:
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UI];
#ifdef HAVE_LAKKA_SWITCH
      case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
      case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
#endif
      case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS];
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_NETWORK];
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST];
      case MENU_ENUM_LABEL_USER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_USER];
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY];

      case MENU_ENUM_LABEL_REWIND_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_REWIND];
      case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE];
      case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS];
#ifdef HAVE_NETWORKING
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM];
      case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
#endif
      case MENU_ENUM_LABEL_REBOOT:
      case MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG:
      case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_SHUTDOWN:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN];
      case MENU_ENUM_LABEL_CONFIGURATIONS:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK];
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_ADD];
      default:
            break;
   }

   switch(type)
   {
      case FILE_TYPE_DIRECTORY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FOLDER];
      case FILE_TYPE_PLAIN:
      case FILE_TYPE_IN_CARCHIVE:
      case FILE_TYPE_RPL_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_FILE];
      case FILE_TYPE_SHADER:
      case FILE_TYPE_SHADER_PRESET:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case FILE_TYPE_CARCHIVE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ZIP];
      case FILE_TYPE_MUSIC:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MUSIC];
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case FILE_TYPE_MOVIE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case FILE_TYPE_RDB:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RDB];
      case FILE_TYPE_CURSOR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CURSOR];
      case FILE_TYPE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
      case MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_SETTING_ACTION_CLOSE:
      case MENU_SETTING_ACTION_DELETE_ENTRY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_SAVESTATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE];
      case MENU_SETTING_ACTION_LOADSTATE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case FILE_TYPE_RDB_ENTRY:
      case MENU_SETTING_ACTION_CORE_INFORMATION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO];
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS];
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS];
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS];
      case MENU_SETTING_ACTION_SCREENSHOT:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT];
      case MENU_SETTING_ACTION_RESET:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
      case MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PAUSE];
      case MENU_SETTING_GROUP:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
#ifdef HAVE_LAKKA_SWITCH
      case MENU_SET_SWITCH_BRIGHTNESS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_BRIGHTNESS];
#endif
      case MENU_INFO_MESSAGE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO];
      case MENU_WIFI:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_WIFI];
#ifdef HAVE_NETWORKING
      case MENU_ROOM:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM];
      case MENU_ROOM_LAN:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN];
      case MENU_ROOM_RELAY:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY];
#endif
      case MENU_SETTING_ACTION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SETTING];
   }

#ifdef HAVE_CHEEVOS
   if (
         (type >= MENU_SETTINGS_CHEEVOS_START) &&
         (type < MENU_SETTINGS_NETPLAY_ROOMS_START)
      )
   {
      int new_id = type - MENU_SETTINGS_CHEEVOS_START;
      if (get_badge_texture(new_id) != 0)
         return get_badge_texture(new_id);
      /* Should be replaced with placeholder badge icon. */
      return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS];
   }
#endif

   if (
         (type >= MENU_SETTINGS_INPUT_BEGIN) &&
         (type <= MENU_SETTINGS_INPUT_DESC_END)
      )
      {
         unsigned input_id;
         if (type < MENU_SETTINGS_INPUT_DESC_BEGIN)
         {
            input_id = MENU_SETTINGS_INPUT_BEGIN;
            if ( type == input_id + 2)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
            if ( type == input_id + 4)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
            if ( type == input_id + 5)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
            input_id = input_id + 7;
         }
         else
         {
            input_id = MENU_SETTINGS_INPUT_DESC_BEGIN;
            while (type > (input_id + 23))
            {
               input_id = (input_id + 24) ;
            }
         }
         if ( type == input_id )
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D];
         if ( type == (input_id + 1))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L];
         if ( type == (input_id + 2))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT];
         if ( type == (input_id + 3))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START];
         if ( type == (input_id + 4))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U];
         if ( type == (input_id + 5))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D];
         if ( type == (input_id + 6))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L];
         if ( type == (input_id + 7))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R];
         if ( type == (input_id + 8))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R];
         if ( type == (input_id + 9))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U];
         if ( type == (input_id + 10))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB];
         if ( type == (input_id + 11))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB];
         if ( type == (input_id + 12))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT];
         if ( type == (input_id + 13))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT];
         if ( type == (input_id + 14))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P];
         if ( type == (input_id + 15))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P];
         if ( type == (input_id + 16))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R];
         if ( type == (input_id + 17))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L];
         if ( type == (input_id + 18))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D];
         if ( type == (input_id + 19))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U];
         if ( type == (input_id + 20))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R];
         if ( type == (input_id + 21))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L];
         if ( type == (input_id + 22))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D];
         if ( type == (input_id + 23))
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U];
      }
   return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING];
}

const char *ozone_entries_icon_texture_path(ozone_handle_t *ozone, unsigned id)
{
   char icon_fullpath[255];
   char *icon_name         = NULL;

switch (id)
   {
      case OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         icon_name = "lakka.png";
         break;
#else
         icon_name = "retroarch.png";
         break;
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTINGS:
         icon_name = "settings.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_HISTORY:
         icon_name = "history.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITES:
         icon_name = "favorites.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE:
         icon_name = "add-favorite.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSICS:
         icon_name = "musics.png";
         break;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIES:
         icon_name = "movies.png";
         break;
#endif
#ifdef HAVE_IMAGEVIEWER
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGES:
         icon_name = "images.png";
         break;
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTING:
         icon_name = "setting.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING:
         icon_name = "subsetting.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ARROW:
         icon_name = "arrow.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RUN:
         icon_name = "run.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOSE:
         icon_name = "close.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RESUME:
         icon_name = "resume.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOCK:
         icon_name = "clock.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL:
         icon_name = "battery-full.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING:
         icon_name = "battery-charging.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_POINTER:
         icon_name = "pointer.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE:
         icon_name = "savestate.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE:
         icon_name = "loadstate.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_UNDO:
         icon_name = "undo.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO:
         icon_name = "core-infos.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_WIFI:
         icon_name = "wifi.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS:
         icon_name = "core-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS:
         icon_name = "core-input-remapping-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS:
         icon_name = "core-cheat-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS:
         icon_name = "core-disk-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS:
         icon_name = "core-shader-options.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST:
         icon_name = "achievement-list.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT:
         icon_name = "screenshot.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RELOAD:
         icon_name = "reload.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RENAME:
         icon_name = "rename.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FILE:
         icon_name = "file.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FOLDER:
         icon_name = "folder.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ZIP:
         icon_name = "zip.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSIC:
         icon_name = "music.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE:
         icon_name = "favorites-content.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGE:
         icon_name = "image.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIE:
         icon_name = "movie.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE:
         icon_name = "core.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RDB:
         icon_name = "database.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CURSOR:
         icon_name = "cursor.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_ON:
         icon_name = "on.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_OFF:
         icon_name = "off.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD:
         icon_name = "add.png";
         break;
#ifdef HAVE_NETWORKING
      case OZONE_ENTRIES_ICONS_TEXTURE_NETPLAY:
         icon_name = "netplay.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM:
         icon_name = "menu_room.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN:
         icon_name = "menu_room_lan.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY:
         icon_name = "menu_room_relay.png";
         break;
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY:
         icon_name = "key.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY_HOVER:
         icon_name = "key-hover.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE:
         icon_name = "dialog-slice.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS:
         icon_name = "menu_achievements.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_AUDIO:
         icon_name = "menu_audio.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS:
         icon_name = "menu_drivers.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_EXIT:
         icon_name = "menu_exit.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP:
         icon_name = "menu_frameskip.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_HELP:
         icon_name = "menu_help.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INFO:
         icon_name = "menu_info.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS:
         icon_name = "Libretro - Pad.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_LATENCY:
         icon_name = "menu_latency.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_NETWORK:
         icon_name = "menu_network.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_POWER:
         icon_name = "menu_power.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_RECORD:
         icon_name = "menu_record.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVING:
         icon_name = "menu_saving.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_UPDATER:
         icon_name = "menu_updater.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_VIDEO:
         icon_name = "menu_video.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MIXER:
         icon_name = "menu_mixer.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_LOG:
         icon_name = "menu_log.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_OSD:
         icon_name = "menu_osd.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_UI:
         icon_name = "menu_ui.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_USER:
         icon_name = "menu_user.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY:
         icon_name = "menu_privacy.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST:
         icon_name = "menu_playlist.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU:
         icon_name = "menu_quickmenu.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_REWIND:
         icon_name = "menu_rewind.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY:
         icon_name = "menu_overlay.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE:
         icon_name = "menu_override.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS:
         icon_name = "menu_notifications.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_STREAM:
         icon_name = "menu_stream.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN:
         icon_name = "menu_shutdown.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U:
         icon_name = "input_DPAD-U.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D:
         icon_name = "input_DPAD-D.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L:
         icon_name = "input_DPAD-L.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R:
         icon_name = "input_DPAD-R.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U:
         icon_name = "input_STCK-U.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D:
         icon_name = "input_STCK-D.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L:
         icon_name = "input_STCK-L.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R:
         icon_name = "input_STCK-R.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P:
         icon_name = "input_STCK-P.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U:
         icon_name = "input_BTN-U.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D:
         icon_name = "input_BTN-D.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L:
         icon_name = "input_BTN-L.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R:
         icon_name = "input_BTN-R.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB:
         icon_name = "input_LB.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB:
         icon_name = "input_RB.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT:
         icon_name = "input_LT.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT:
         icon_name = "input_RT.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT:
         icon_name = "input_SELECT.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START:
         icon_name = "input_START.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK:
         icon_name = "menu_check.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_MENU_ADD:
         icon_name = "menu_add.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_BRIGHTNESS:
         icon_name = "menu_brightnes.png";
         break;
      case OZONE_ENTRIES_ICONS_TEXTURE_PAUSE:
         icon_name = "menu_pause.png";
         break;
   }

   fill_pathname_join(
      icon_fullpath,
      ozone->icons_path,
      icon_name,
      sizeof(icon_fullpath)
   );

   if (!filestream_exists(icon_fullpath))
   {
      return "subsetting.png";
   }
   else
      return  icon_name;
}

void ozone_unload_theme_textures(ozone_handle_t *ozone)
{
   int i;
   int j;

   for (j = 0; j < ozone_themes_count; j++)
   {
      ozone_theme_t *theme = ozone_themes[j];
      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
            video_driver_texture_unload(&theme->textures[i]);
   }
}

bool ozone_reset_theme_textures(ozone_handle_t *ozone)
{
   int i;
   int j;
   char theme_path[255];
   bool result = true;

   for (j = 0; j < ozone_themes_count; j++)
   {
      ozone_theme_t *theme = ozone_themes[j];

      fill_pathname_join(
         theme_path,
         ozone->png_path,
         theme->name,
         sizeof(theme_path)
      );

      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
      {
         char filename[PATH_MAX_LENGTH];
         strlcpy(filename, OZONE_THEME_TEXTURES_FILES[i], sizeof(filename));
         strlcat(filename, ".png", sizeof(filename));

         if (!menu_display_reset_textures_list(filename, theme_path, &theme->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR))
            result = false;
      }
   }

   return result;
}