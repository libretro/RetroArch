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

static const char *menu_hash_to_str_german(uint32_t hash)
{
   switch (hash)
   {
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_french(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Charger un fichier de Remap";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Enregistrer un fichier de Remap sous";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Ratio personnalise";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Choisir ce dossier>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Lancer le contenu";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Options de disques";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Options du Core";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Options de triche";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturer l ecran";
      case MENU_LABEL_VALUE_RESUME:
         return "Reprendre";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Numero du disque";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Compteurs du Frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Ajouter une image de disque";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Etat du lecteur de disque";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Playlist vide.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Pad d'informations disponibles.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Pas d'options disponibles.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Aucun Core disponible.";
      case MENU_VALUE_NO_CORE:
         return "Aucun Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Gestion de base de donnee";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Gestion de curseur";
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Réglage de capture video";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Reglages";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Quitter RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Aide";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Sauver nouvelle configuration";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Redemarrer le contenu";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Mise a jour des Cores";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informations du systeme";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Options";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informations sur le Core";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Dossier non trouve.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Pas d elements.";
      case MENU_LABEL_CORE_LIST:
         return "Charger un Core";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Charger un contenu";
      case MENU_LABEL_VALUE_UNLOAD_CORE:
         return "Unload Core";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Gestion avancee";
      case MENU_LABEL_VALUE_PERFORMANCE_COUNTERS:
         return "Compteurs de performance";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Sauvegarder un etat";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Charger un etat";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Reprendre";
      case MENU_LABEL_DRIVER_SETTINGS:
         return "Reglage des pilotes";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Impossible de lire l'archive.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Zoom de l'Overlay";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Preset";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Audio Latency (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Audio Device";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Prereglage d'Overlay Clavier";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Transparence de l'Overlay";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Fond d'ecran";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Fond d'ecran dynamique";
      case MENU_LABEL_VALUE_BOXART:
         return "Afficher les vignettes";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Core Input Remapping Options";
      case MENU_LABEL_VALUE_VIDEO_OPTIONS:
         return "Options video";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Options de Shaders";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No shader parameters.";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtre video";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Audio DSP Plugin";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Telechargement de: ";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
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
      case MENU_LABEL_VIDEO_CROP_OVERSCAN:
         return "video_crop_overscan";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Crop Overscan (Reload)";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Black Frame Insertion";
      case MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         return "video_black_frame_insertion";
      case MENU_LABEL_VIDEO_HARD_SYNC_FRAMES:
         return "video_hard_sync_frames";
      case MENU_LABEL_SORT_SAVEFILES_ENABLE:
         return "sort_savefiles_enable";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sort Saves In Folders";
      case MENU_LABEL_SORT_SAVESTATES_ENABLE:
         return "sort_savestates_enable";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sort Savestates In Folders";
      case MENU_LABEL_VIDEO_FULLSCREEN:
         return "video_fullscreen";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Use Fullscreen Mode";
      case MENU_LABEL_PERFCNT_ENABLE:
         return "perfcnt_enable";
      case MENU_LABEL_VIDEO_SCALE:
         return "video_scale";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Windowed Scale";
      case MENU_LABEL_VIDEO_SCALE_INTEGER:
         return "video_scale_integer";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Integer Scale";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Performance Counters";
      case MENU_LABEL_LIBRETRO_LOG_LEVEL:
         return "libretro_log_level";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Core Logging Level";
      case MENU_LABEL_LOG_VERBOSITY:
         return "log_verbosity";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Logging Verbosity";
      case MENU_LABEL_SAVESTATE_AUTO_SAVE:
         return "savestate_auto_save";
      case MENU_LABEL_SAVESTATE_AUTO_LOAD:
         return "savestate_auto_load";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Auto Load State";
      case MENU_LABEL_SAVESTATE_AUTO_INDEX:
         return "savestate_auto_index";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Save State Auto Index";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Auto Save State";
      case MENU_LABEL_AUTOSAVE_INTERVAL:
         return "autosave_interval";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "SaveRAM Autosave Interval";
      case MENU_LABEL_BLOCK_SRAM_OVERWRITE:
         return "block_sram_overwrite";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Don't overwrite SaveRAM on loading savestate";
      case MENU_LABEL_VIDEO_SHARED_CONTEXT:
         return "video_shared_context";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "HW Shared Context Enable";
      case MENU_LABEL_RESTART_RETROARCH:
         return "restart_retroarch";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Restart RetroArch";
      case MENU_LABEL_NETPLAY_NICKNAME:
         return "netplay_nickname";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Username";
      case MENU_LABEL_USER_LANGUAGE:
         return "user_language";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Language";
      case MENU_LABEL_CAMERA_ALLOW:
         return "camera_allow";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Allow Camera";
      case MENU_LABEL_LOCATION_ALLOW:
         return "location_allow";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Allow Location";
      case MENU_LABEL_PAUSE_LIBRETRO:
         return "menu_pause_libretro";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pause when menu activated";
      case MENU_LABEL_INPUT_OSK_OVERLAY_ENABLE:
         return "input_osk_overlay_enable";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Display Keyboard Overlay";
      case MENU_LABEL_INPUT_OVERLAY_ENABLE:
         return "input_overlay_enable";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Display Overlay";
      case MENU_LABEL_VIDEO_MONITOR_INDEX:
         return "video_monitor_index";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Monitor Index";
      case MENU_LABEL_VIDEO_FRAME_DELAY:
         return "video_frame_delay";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Frame Delay";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Duty Cycle";
      case MENU_LABEL_INPUT_DUTY_CYCLE:
         return "input_duty_cycle";
      case MENU_LABEL_INPUT_TURBO_PERIOD:
         return "input_turbo_period";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Turbo Period";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Input Axis Threshold";
      case MENU_LABEL_INPUT_AXIS_THRESHOLD:
         return "input_axis_threshold";
      case MENU_LABEL_INPUT_REMAP_BINDS_ENABLE:
         return "input_remap_binds_enable";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Remap Binds Enable";
      case MENU_LABEL_INPUT_MAX_USERS:
         return "input_max_users";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Max Users";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Autoconfig Enable";
      case MENU_LABEL_INPUT_AUTODETECT_ENABLE:
         return "input_autodetect_enable";
      case MENU_LABEL_AUDIO_OUTPUT_RATE:
         return "audio_output_rate";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Audio Output Rate (KHz)";
      case MENU_LABEL_AUDIO_MAX_TIMING_SKEW:
         return "audio_max_timing_skew";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Audio Maximum Timing Skew";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Cheat Passes";
      case MENU_LABEL_CHEAT_APPLY_CHANGES:
         return "cheat_apply_changes";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Save Core Remap File";
      case MENU_LABEL_REMAP_FILE_SAVE_CORE:
         return "remap_file_save_core";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Save Game Remap File";
      case MENU_LABEL_REMAP_FILE_SAVE_GAME:
         return "remap_file_save_game";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Apply Cheat Changes";
      case MENU_LABEL_CHEAT_NUM_PASSES:
         return "cheat_num_passes";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Apply Shader Changes";
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         return "shader_apply_changes";
      case MENU_LABEL_COLLECTION:
         return "collection";
      case MENU_LABEL_REWIND_ENABLE:
         return "rewind_enable";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Rewind Enable";
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
         return "content_collection_list";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Load Content (Collection)";
      case MENU_LABEL_DETECT_CORE_LIST:
         return "detect_core_list";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Load Content (Detect Core)";
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         return "history_list";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Load Content (History)";
      case MENU_LABEL_AUDIO_ENABLE:
         return "audio_enable";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Audio Enable";
      case MENU_LABEL_FPS_SHOW:
         return "fps_show";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Display Framerate";
      case MENU_LABEL_AUDIO_MUTE:
         return "audio_mute_enable";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Audio Mute";
      case MENU_LABEL_VIDEO_SHADER_PASS:
         return "video_shader_pass";
      case MENU_LABEL_AUDIO_VOLUME:
         return "audio_volume";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Audio Volume Level (dB)";
      case MENU_LABEL_AUDIO_SYNC:
         return "audio_sync";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Audio Sync Enable";
      case MENU_LABEL_AUDIO_RATE_CONTROL_DELTA:
         return "audio_rate_control_delta";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Audio Rate Control Delta";
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         return "video_shader_filter_pass";
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         return "video_shader_scale_pass";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Shader Passes";
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         return "video_shader_num_passes";
      case MENU_LABEL_RDB_ENTRY_DESCRIPTION:
         return "rdb_entry_description";
      case MENU_LABEL_RDB_ENTRY_ORIGIN:
         return "rdb_entry_origin";
      case MENU_LABEL_RDB_ENTRY_PUBLISHER:
         return "rdb_entry_publisher";
      case MENU_LABEL_RDB_ENTRY_DEVELOPER:
         return "rdb_entry_developer";
      case MENU_LABEL_RDB_ENTRY_FRANCHISE:
         return "rdb_entry_franchise";
      case MENU_LABEL_RDB_ENTRY_MAX_USERS:
         return "rdb_entry_max_users";
      case MENU_LABEL_RDB_ENTRY_NAME:
         return "rdb_entry_name";
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "rdb_entry_edge_magazine_rating";
      case MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "rdb_entry_famitsu_magazine_rating";
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         return "rdb_entry_edge_magazine_issue";
      case MENU_LABEL_RDB_ENTRY_RELEASE_MONTH:
         return "rdb_entry_releasemonth";
      case MENU_LABEL_RDB_ENTRY_RELEASE_YEAR:
         return "rdb_entry_releaseyear";
      case MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
         return "rdb_entry_enhancement_hw";
      case MENU_LABEL_RDB_ENTRY_SHA1:
         return "rdb_entry_sha1";
      case MENU_LABEL_RDB_ENTRY_CRC32:
         return "rdb_entry_crc32";
      case MENU_LABEL_RDB_ENTRY_MD5:
         return "rdb_entry_md5";
      case MENU_LABEL_RDB_ENTRY_BBFC_RATING:
         return "rdb_entry_bbfc_rating";
      case MENU_LABEL_RDB_ENTRY_ESRB_RATING:
         return "rdb_entry_esrb_rating";
      case MENU_LABEL_RDB_ENTRY_ELSPA_RATING:
         return "rdb_entry_elspa_rating";
      case MENU_LABEL_RDB_ENTRY_PEGI_RATING:
         return "rdb_entry_pegi_rating";
      case MENU_LABEL_RDB_ENTRY_CERO_RATING:
         return "rdb_entry_cero_rating";
      case MENU_LABEL_RDB_ENTRY_ANALOG:
         return "rdb_entry_analog";
      case MENU_LABEL_CONFIGURATIONS:
         return "configurations";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Configuration Files";
      case MENU_LABEL_LOAD_OPEN_ZIP:
         return "load_open_zip";
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
      case MENU_LABEL_BOXART:
         return "menu_boxart_enable";
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
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         return "video_shader_parameters";
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         return "video_shader_preset_parameters";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Shader Preset Save As";
      case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         return "video_shader_preset_save_as";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No shader parameters.";
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         return "video_shader_preset";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Load Shader Preset";
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
      case RETRO_LANGUAGE_GERMAN:
         ret = menu_hash_to_str_german(hash);
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
