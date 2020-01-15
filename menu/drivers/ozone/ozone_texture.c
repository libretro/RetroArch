/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include "../../../cheevos-new/badges.h"
#include "../../../verbosity.h"

menu_texture_item ozone_entries_icon_get_texture(ozone_handle_t *ozone,
      enum msg_hash_enums enum_idx, unsigned type, bool active)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_LOAD_DISC:
      case MENU_ENUM_LABEL_DUMP_DISC:
      case MENU_ENUM_LABEL_DISC_INFORMATION:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISC];
      case MENU_ENUM_LABEL_CORE_OPTIONS:
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE];
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
      case MENU_ENUM_LABEL_DISK_TRAY_EJECT:
      case MENU_ENUM_LABEL_DISK_TRAY_INSERT:
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
      case MENU_ENUM_LABEL_PLAYLISTS_TAB:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU];
      case MENU_ENUM_LABEL_START_CORE:
      case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
      case MENU_ENUM_LABEL_CORE_LIST:
      case MENU_ENUM_LABEL_SIDELOAD_CORE_LIST:
      case MENU_ENUM_LABEL_CORE_SETTINGS:
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
      case MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
      case MENU_ENUM_LABEL_SET_CORE_ASSOCIATION:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CORE];
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS:
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
      case MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST:
      case MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
#ifdef HAVE_VIDEO_LAYOUT
      case MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS:
#endif
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
      case MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_HELP];
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
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
      case MENU_ENUM_LABEL_START_NET_RETROPAD:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
      case MENU_ENUM_LABEL_LATENCY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LATENCY];
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOG];
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
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
      case MENU_ENUM_LABEL_DELETE_PLAYLIST:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_OSD];
      case MENU_ENUM_LABEL_SHOW_WIMP:
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_UI];
#ifdef HAVE_LAKKA_SWITCH
      case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
#endif
      case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_POWER];
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS];
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
      case MENU_ENUM_LABEL_NETWORK_INFO_ENTRY:
      case MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_NETWORK];
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST];
      case MENU_ENUM_LABEL_USER_SETTINGS:
            return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_USER];
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
      case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST:
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
      case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
      case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS:
      case MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST:
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
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK];
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_ADD];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_TOGGLE];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_COG];
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_MOVIE];
      default:
            break;
   }

   switch(type)
   {
      case MENU_SET_CDROM_INFO:
      case MENU_SET_CDROM_LIST:
      case MENU_SET_LOAD_CDROM_LIST:
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DISC];
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
         return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RUN];
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
         /* This part is only utilized by Input User # Binds */
         unsigned input_id;
         if (type < MENU_SETTINGS_INPUT_DESC_BEGIN)
         {
            input_id = MENU_SETTINGS_INPUT_BEGIN;
            if ( type == input_id + 1)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_ADC];
            if ( type == input_id + 2)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS];
            if ( type == input_id + 3)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BIND_ALL];
            if ( type == input_id + 4)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_RELOAD];
            if ( type == input_id + 5)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_SAVING];
            if ( type == input_id + 6)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_MOUSE];
            if ((type > (input_id + 30)) && (type < (input_id + 42)))
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LGUN];
            if ( type == input_id + 42)
               return ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_INPUT_TURBO];
            /* align to use the same code of Quickmenu controls*/
            input_id = input_id + 7;
         }
         else
         {
            /* Quickmenu controls repeats the same icons for all users*/
            input_id = MENU_SETTINGS_INPUT_DESC_BEGIN;
            while (type > (input_id + 23))
            {
               input_id = (input_id + 24) ;
            }
         }
         /* This is utilized for both Input Binds and Quickmenu controls*/
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

const char *ozone_entries_icon_texture_path(unsigned id)
{
switch (id)
   {
      case OZONE_ENTRIES_ICONS_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         return "lakka.png";
#else
         return "retroarch.png";
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTINGS:
         return "settings.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_HISTORY:
         return "history.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITES:
         return "favorites.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD_FAVORITE:
         return "add-favorite.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSICS:
         return "musics.png";
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIES:
         return "movies.png";
#endif
#ifdef HAVE_IMAGEVIEWER
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGES:
         return "images.png";
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_SETTING:
         return "setting.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SUBSETTING:
         return "subsetting.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ARROW:
         return "arrow.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RUN:
         return "run.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOSE:
         return "close.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RESUME:
         return "resume.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CLOCK:
         return "clock.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_FULL:
         return "battery-full.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_CHARGING:
         return "battery-charging.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_80:
         return "battery-80.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_60:
         return "battery-60.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_40:
         return "battery-40.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BATTERY_20:
         return "battery-20.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_POINTER:
         return "pointer.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVESTATE:
         return "savestate.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_LOADSTATE:
         return "loadstate.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_UNDO:
         return "undo.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_INFO:
         return "core-infos.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_WIFI:
         return "wifi.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE_OPTIONS:
         return "core-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_REMAPPING_OPTIONS:
         return "core-input-remapping-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CHEAT_OPTIONS:
         return "core-cheat-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DISK_OPTIONS:
         return "core-disk-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SHADER_OPTIONS:
         return "core-shader-options.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENT_LIST:
         return "achievement-list.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SCREENSHOT:
         return "screenshot.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RELOAD:
         return "reload.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RENAME:
         return "rename.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FILE:
         return "file.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FOLDER:
         return "folder.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ZIP:
         return "zip.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MUSIC:
         return "music.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FAVORITE:
         return "favorites-content.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_IMAGE:
         return "image.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MOVIE:
         return "movie.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CORE:
         return "core.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RDB:
         return "database.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CURSOR:
         return "cursor.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_ON:
         return "on.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SWITCH_OFF:
         return "off.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DISC:
         return "disc.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ADD:
         return "add.png";
#ifdef HAVE_NETWORKING
      case OZONE_ENTRIES_ICONS_TEXTURE_NETPLAY:
         return "netplay.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM:
         return "menu_room.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_LAN:
         return "menu_room_lan.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ROOM_RELAY:
         return "menu_room_relay.png";
#endif
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY:
         return "key.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_KEY_HOVER:
         return "key-hover.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE:
         return "dialog-slice.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_ACHIEVEMENTS:
         return "menu_achievements.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_AUDIO:
         return "menu_audio.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_DRIVERS:
         return "menu_drivers.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_EXIT:
         return "menu_exit.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_FRAMESKIP:
         return "menu_frameskip.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_HELP:
         return "menu_help.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INFO:
         return "menu_info.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SETTINGS:
         return "Libretro - Pad.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_LATENCY:
         return "menu_latency.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_NETWORK:
         return "menu_network.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_POWER:
         return "menu_power.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_RECORD:
         return "menu_record.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SAVING:
         return "menu_saving.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_UPDATER:
         return "menu_updater.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_VIDEO:
         return "menu_video.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MIXER:
         return "menu_mixer.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_LOG:
         return "menu_log.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_OSD:
         return "menu_osd.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_UI:
         return "menu_ui.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_USER:
         return "menu_user.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_PRIVACY:
         return "menu_privacy.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_PLAYLIST:
         return "menu_playlist.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_QUICKMENU:
         return "menu_quickmenu.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_REWIND:
         return "menu_rewind.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERLAY:
         return "menu_overlay.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_OVERRIDE:
         return "menu_override.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_NOTIFICATIONS:
         return "menu_notifications.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_STREAM:
         return "menu_stream.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_SHUTDOWN:
         return "menu_shutdown.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_U:
         return "input_DPAD-U.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_D:
         return "input_DPAD-D.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_L:
         return "input_DPAD-L.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_DPAD_R:
         return "input_DPAD-R.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_U:
         return "input_STCK-U.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_D:
         return "input_STCK-D.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_L:
         return "input_STCK-L.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_R:
         return "input_STCK-R.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_STCK_P:
         return "input_STCK-P.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_U:
         return "input_BTN-U.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_D:
         return "input_BTN-D.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_L:
         return "input_BTN-L.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BTN_R:
         return "input_BTN-R.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LB:
         return "input_LB.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RB:
         return "input_RB.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LT:
         return "input_LT.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_RT:
         return "input_RT.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_SELECT:
         return "input_SELECT.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_START:
         return "input_START.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_CHECKMARK:
         return "menu_check.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MENU_ADD:
         return "menu_add.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_BRIGHTNESS:
         return "menu_brightness.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_PAUSE:
         return "menu_pause.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_TOGGLE:
         return "menu_apply_toggle.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_MENU_APPLY_COG:
         return "menu_apply_cog.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_ADC:
         return "input_ADC.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_BIND_ALL:
         return "input_BIND_ALL.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_MOUSE:
         return "input_MOUSE.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_LGUN:
         return "input_LGUN.png";
      case OZONE_ENTRIES_ICONS_TEXTURE_INPUT_TURBO:
         return "input_TURBO.png";
   }
   return NULL;
}

void ozone_unload_theme_textures(ozone_handle_t *ozone)
{
   unsigned i, j;

   for (j = 0; j < ozone_themes_count; j++)
   {
      ozone_theme_t *theme = ozone_themes[j];
      for (i = 0; i < OZONE_THEME_TEXTURE_LAST; i++)
            video_driver_texture_unload(&theme->textures[i]);
   }
}

bool ozone_reset_theme_textures(ozone_handle_t *ozone)
{
   unsigned i, j;
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

         if (!menu_display_reset_textures_list(filename, theme_path, &theme->textures[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
         {
            RARCH_WARN("[OZONE] Asset missing: %s%s%s\n", theme_path, path_default_slash(), filename);
            result = false;
         }
      }
   }

   return result;
}
