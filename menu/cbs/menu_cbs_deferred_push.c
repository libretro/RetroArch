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

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../../msg_hash.h"

#include "../../database_info.h"

#include "../../cores/internal_cores.h"

#include "../../configuration.h"
#include "../../core.h"
#include "../../core_info.h"
#include "../../verbosity.h"

enum
{
   PUSH_ARCHIVE_OPEN_DETECT_CORE = 0,
   PUSH_ARCHIVE_OPEN,
   PUSH_DEFAULT,
   PUSH_DETECT_CORE_LIST
};

#ifndef BIND_ACTION_DEFERRED_PUSH
#define BIND_ACTION_DEFERRED_PUSH(cbs, name) (cbs)->action_deferred_push = (name)
#endif

#define GENERIC_DEFERRED_PUSH(name, type) \
static int (name)(menu_displaylist_info_t *info) \
{ \
   settings_t *settings = config_get_ptr(); \
   return deferred_push_dlist(info, type, settings); \
}

#define GENERIC_DEFERRED_CURSOR_MANAGER(name, type) \
static int (name)(menu_displaylist_info_t *info) \
{ \
   return deferred_push_cursor_manager_list_generic(info, type); \
}

#define GENERIC_DEFERRED_PUSH_GENERAL(name, a, b) \
static int (name)(menu_displaylist_info_t *info) \
{ \
   return general_push(info, a, b); \
}

static int deferred_push_dlist(
      menu_displaylist_info_t *info,
      enum menu_displaylist_ctl_state state,
      settings_t *settings)
{
   if (!menu_displaylist_ctl(state, info, settings))
      return menu_cbs_exit();
   menu_displaylist_process(info);
   return 0;
}

static int deferred_push_database_manager_list_deferred(
      menu_displaylist_info_t *info)
{
   settings_t *settings = config_get_ptr();
   if (!string_is_empty(info->path_b))
      free(info->path_b);
   if (!string_is_empty(info->path_c))
      free(info->path_c);

   info->path_b    = strdup(info->path);
   info->path_c    = NULL;

   return deferred_push_dlist(info, DISPLAYLIST_DATABASE_QUERY, settings);
}

GENERIC_DEFERRED_PUSH(deferred_push_remappings_port,                DISPLAYLIST_OPTIONS_REMAPPINGS_PORT)
GENERIC_DEFERRED_PUSH(deferred_push_video_shader_preset_parameters, DISPLAYLIST_SHADER_PARAMETERS_PRESET)
GENERIC_DEFERRED_PUSH(deferred_push_video_shader_parameters,        DISPLAYLIST_SHADER_PARAMETERS)
GENERIC_DEFERRED_PUSH(deferred_push_video_shader_preset_save,       DISPLAYLIST_SHADER_PRESET_SAVE)
GENERIC_DEFERRED_PUSH(deferred_push_video_shader_preset_remove,       DISPLAYLIST_SHADER_PRESET_REMOVE)
GENERIC_DEFERRED_PUSH(deferred_push_settings,                       DISPLAYLIST_SETTINGS_ALL)
GENERIC_DEFERRED_PUSH(deferred_push_shader_options,                 DISPLAYLIST_OPTIONS_SHADERS)
GENERIC_DEFERRED_PUSH(deferred_push_quick_menu_override_options,    DISPLAYLIST_OPTIONS_OVERRIDES)
GENERIC_DEFERRED_PUSH(deferred_push_options,                        DISPLAYLIST_OPTIONS)
GENERIC_DEFERRED_PUSH(deferred_push_netplay,                        DISPLAYLIST_NETPLAY_ROOM_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_netplay_sublist,                DISPLAYLIST_NETPLAY)
GENERIC_DEFERRED_PUSH(deferred_push_content_settings,               DISPLAYLIST_CONTENT_SETTINGS)
GENERIC_DEFERRED_PUSH(deferred_push_add_content_list,               DISPLAYLIST_ADD_CONTENT_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_history_list,                   DISPLAYLIST_HISTORY)
GENERIC_DEFERRED_PUSH(deferred_push_database_manager_list,          DISPLAYLIST_DATABASES)
GENERIC_DEFERRED_PUSH(deferred_push_cursor_manager_list,            DISPLAYLIST_DATABASE_CURSORS)
GENERIC_DEFERRED_PUSH(deferred_push_content_collection_list,        DISPLAYLIST_DATABASE_PLAYLISTS)
GENERIC_DEFERRED_PUSH(deferred_push_configurations_list,            DISPLAYLIST_CONFIGURATIONS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_load_content_special,           DISPLAYLIST_LOAD_CONTENT_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_load_content_list,              DISPLAYLIST_LOAD_CONTENT_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_dump_disk_list,                 DISPLAYLIST_DUMP_DISC)
#ifdef HAVE_LAKKA
GENERIC_DEFERRED_PUSH(deferred_push_eject_disc,                     DISPLAYLIST_EJECT_DISC)
#endif
GENERIC_DEFERRED_PUSH(deferred_push_cdrom_info_detail_list,         DISPLAYLIST_CDROM_DETAIL_INFO)
GENERIC_DEFERRED_PUSH(deferred_push_load_disk_list,                 DISPLAYLIST_LOAD_DISC)
GENERIC_DEFERRED_PUSH(deferred_push_information_list,               DISPLAYLIST_INFORMATION_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_information,                    DISPLAYLIST_INFORMATION)
GENERIC_DEFERRED_PUSH(deferred_archive_action_detect_core,          DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE)
GENERIC_DEFERRED_PUSH(deferred_archive_action,                      DISPLAYLIST_ARCHIVE_ACTION)
GENERIC_DEFERRED_PUSH(deferred_push_management_options,             DISPLAYLIST_OPTIONS_MANAGEMENT)
GENERIC_DEFERRED_PUSH(deferred_push_core_counters,                  DISPLAYLIST_PERFCOUNTERS_CORE)
GENERIC_DEFERRED_PUSH(deferred_push_frontend_counters,              DISPLAYLIST_PERFCOUNTERS_FRONTEND)
GENERIC_DEFERRED_PUSH(deferred_push_core_cheat_options,             DISPLAYLIST_OPTIONS_CHEATS)
GENERIC_DEFERRED_PUSH(deferred_push_core_input_remapping_options,   DISPLAYLIST_OPTIONS_REMAPPINGS)
GENERIC_DEFERRED_PUSH(deferred_push_remap_file_manager,             DISPLAYLIST_REMAP_FILE_MANAGER)
GENERIC_DEFERRED_PUSH(deferred_push_savestate_list,                 DISPLAYLIST_SAVESTATE_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_core_options,                   DISPLAYLIST_CORE_OPTIONS)
GENERIC_DEFERRED_PUSH(deferred_push_core_option_override_list,      DISPLAYLIST_CORE_OPTION_OVERRIDE_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_disk_options,                   DISPLAYLIST_OPTIONS_DISK)
GENERIC_DEFERRED_PUSH(deferred_push_browse_url_list,                DISPLAYLIST_BROWSE_URL_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_browse_url_start,               DISPLAYLIST_BROWSE_URL_START)
GENERIC_DEFERRED_PUSH(deferred_push_core_list,                      DISPLAYLIST_CORES)
GENERIC_DEFERRED_PUSH(deferred_push_configurations,                 DISPLAYLIST_CONFIG_FILES)
GENERIC_DEFERRED_PUSH(deferred_push_video_shader_preset,            DISPLAYLIST_SHADER_PRESET)
GENERIC_DEFERRED_PUSH(deferred_push_video_shader_pass,              DISPLAYLIST_SHADER_PASS)
GENERIC_DEFERRED_PUSH(deferred_push_video_filter,                   DISPLAYLIST_VIDEO_FILTERS)
GENERIC_DEFERRED_PUSH(deferred_push_images,                         DISPLAYLIST_IMAGES)
GENERIC_DEFERRED_PUSH(deferred_push_audio_dsp_plugin,               DISPLAYLIST_AUDIO_FILTERS)
GENERIC_DEFERRED_PUSH(deferred_push_cheat_file_load,                DISPLAYLIST_CHEAT_FILES)
GENERIC_DEFERRED_PUSH(deferred_push_cheat_file_load_append,         DISPLAYLIST_CHEAT_FILES)
GENERIC_DEFERRED_PUSH(deferred_push_remap_file_load,                DISPLAYLIST_REMAP_FILES)
GENERIC_DEFERRED_PUSH(deferred_push_record_configfile,              DISPLAYLIST_RECORD_CONFIG_FILES)
GENERIC_DEFERRED_PUSH(deferred_push_stream_configfile,              DISPLAYLIST_STREAM_CONFIG_FILES)
GENERIC_DEFERRED_PUSH(deferred_push_input_overlay,                  DISPLAYLIST_OVERLAYS)
#ifdef HAVE_VIDEO_LAYOUT
GENERIC_DEFERRED_PUSH(deferred_push_video_layout_path,              DISPLAYLIST_VIDEO_LAYOUT_PATH)
#endif
GENERIC_DEFERRED_PUSH(deferred_push_video_font_path,                DISPLAYLIST_VIDEO_FONTS)
GENERIC_DEFERRED_PUSH(deferred_push_xmb_font_path,                  DISPLAYLIST_FONTS)
GENERIC_DEFERRED_PUSH(deferred_push_content_history_path,           DISPLAYLIST_CONTENT_HISTORY)
GENERIC_DEFERRED_PUSH(deferred_push_disc_information,               DISPLAYLIST_DISC_INFO)
GENERIC_DEFERRED_PUSH(deferred_push_system_information,             DISPLAYLIST_SYSTEM_INFO)
GENERIC_DEFERRED_PUSH(deferred_push_network_information,            DISPLAYLIST_NETWORK_INFO)
GENERIC_DEFERRED_PUSH(deferred_push_achievement_pause_menu,         DISPLAYLIST_ACHIEVEMENT_PAUSE_MENU)
GENERIC_DEFERRED_PUSH(deferred_push_achievement_list,               DISPLAYLIST_ACHIEVEMENT_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_rdb_collection,                 DISPLAYLIST_PLAYLIST_COLLECTION)
GENERIC_DEFERRED_PUSH(deferred_main_menu_list,                      DISPLAYLIST_MAIN_MENU)
GENERIC_DEFERRED_PUSH(deferred_music_list,                          DISPLAYLIST_MUSIC_LIST)
GENERIC_DEFERRED_PUSH(deferred_user_binds_list,                     DISPLAYLIST_USER_BINDS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_accounts_list,                  DISPLAYLIST_ACCOUNTS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_driver_settings_list,           DISPLAYLIST_DRIVER_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_core_settings_list,             DISPLAYLIST_CORE_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_core_information_list,          DISPLAYLIST_CORE_INFO)
GENERIC_DEFERRED_PUSH(deferred_push_video_settings_list,            DISPLAYLIST_VIDEO_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_video_fullscreen_mode_settings_list,    DISPLAYLIST_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_video_windowed_mode_settings_list,    DISPLAYLIST_VIDEO_WINDOWED_MODE_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_video_synchronization_settings_list,    DISPLAYLIST_VIDEO_SYNCHRONIZATION_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_video_output_settings_list,    DISPLAYLIST_VIDEO_OUTPUT_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_video_scaling_settings_list,    DISPLAYLIST_VIDEO_SCALING_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_video_hdr_settings_list,        DISPLAYLIST_VIDEO_HDR_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_crt_switchres_settings_list,    DISPLAYLIST_CRT_SWITCHRES_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_configuration_settings_list,    DISPLAYLIST_CONFIGURATION_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_saving_settings_list,           DISPLAYLIST_SAVING_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_mixer_stream_settings_list,     DISPLAYLIST_MIXER_STREAM_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_logging_settings_list,          DISPLAYLIST_LOGGING_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_frame_throttle_settings_list,   DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_rewind_settings_list,           DISPLAYLIST_REWIND_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_frame_time_counter_settings_list,           DISPLAYLIST_FRAME_TIME_COUNTER_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_cheat_details_settings_list,    DISPLAYLIST_CHEAT_DETAILS_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_cheat_search_settings_list,     DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_onscreen_display_settings_list, DISPLAYLIST_ONSCREEN_DISPLAY_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_onscreen_notifications_settings_list, DISPLAYLIST_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_onscreen_notifications_views_settings_list, DISPLAYLIST_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST)
#if defined(HAVE_OVERLAY)
GENERIC_DEFERRED_PUSH(deferred_push_onscreen_overlay_settings_list, DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST)
#endif
#ifdef HAVE_VIDEO_LAYOUT
GENERIC_DEFERRED_PUSH(deferred_push_onscreen_video_layout_settings_list, DISPLAYLIST_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST)
#endif
GENERIC_DEFERRED_PUSH(deferred_push_menu_file_browser_settings_list,DISPLAYLIST_MENU_FILE_BROWSER_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_menu_views_settings_list,       DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_quick_menu_views_settings_list, DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_settings_views_settings_list, DISPLAYLIST_SETTINGS_VIEWS_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_menu_settings_list,             DISPLAYLIST_MENU_SETTINGS_LIST)
#ifdef _3DS
GENERIC_DEFERRED_PUSH(deferred_push_menu_bottom_settings_list,      DISPLAYLIST_MENU_BOTTOM_SETTINGS_LIST)
#endif
GENERIC_DEFERRED_PUSH(deferred_push_user_interface_settings_list,   DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_power_management_settings_list, DISPLAYLIST_POWER_MANAGEMENT_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_retro_achievements_settings_list,DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_updater_settings_list,          DISPLAYLIST_UPDATER_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_bluetooth_settings_list,        DISPLAYLIST_BLUETOOTH_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_wifi_settings_list,             DISPLAYLIST_WIFI_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_wifi_networks_list,             DISPLAYLIST_WIFI_NETWORKS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_network_settings_list,          DISPLAYLIST_NETWORK_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_subsystem_settings_list,        DISPLAYLIST_SUBSYSTEM_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_network_hosting_settings_list,  DISPLAYLIST_NETWORK_HOSTING_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_netplay_kick_list,              DISPLAYLIST_NETPLAY_KICK_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_netplay_ban_list,               DISPLAYLIST_NETPLAY_BAN_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_netplay_lobby_filters_list,     DISPLAYLIST_NETPLAY_LOBBY_FILTERS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_lakka_services_list,            DISPLAYLIST_LAKKA_SERVICES_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_user_settings_list,             DISPLAYLIST_USER_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_directory_settings_list,        DISPLAYLIST_DIRECTORY_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_privacy_settings_list,          DISPLAYLIST_PRIVACY_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_midi_settings_list,             DISPLAYLIST_MIDI_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_audio_settings_list,            DISPLAYLIST_AUDIO_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_audio_output_settings_list,            DISPLAYLIST_AUDIO_OUTPUT_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_audio_resampler_settings_list,            DISPLAYLIST_AUDIO_RESAMPLER_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_audio_synchronization_settings_list,            DISPLAYLIST_AUDIO_SYNCHRONIZATION_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_audio_mixer_settings_list,      DISPLAYLIST_AUDIO_MIXER_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_input_settings_list,            DISPLAYLIST_INPUT_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_input_menu_settings_list,            DISPLAYLIST_INPUT_MENU_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_input_turbo_fire_settings_list,      DISPLAYLIST_INPUT_TURBO_FIRE_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_input_haptic_feedback_settings_list, DISPLAYLIST_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_ai_service_settings_list,            DISPLAYLIST_AI_SERVICE_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_accessibility_settings_list,         DISPLAYLIST_ACCESSIBILITY_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_latency_settings_list,          DISPLAYLIST_LATENCY_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_recording_settings_list,        DISPLAYLIST_RECORDING_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_playlist_settings_list,         DISPLAYLIST_PLAYLIST_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_playlist_manager_list,          DISPLAYLIST_PLAYLIST_MANAGER_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_playlist_manager_settings,      DISPLAYLIST_PLAYLIST_MANAGER_SETTINGS)
GENERIC_DEFERRED_PUSH(deferred_push_input_hotkey_binds_list,        DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_accounts_cheevos_list,          DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_accounts_twitch_list,           DISPLAYLIST_ACCOUNTS_TWITCH_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_accounts_youtube_list,          DISPLAYLIST_ACCOUNTS_YOUTUBE_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_accounts_facebook_list,         DISPLAYLIST_ACCOUNTS_FACEBOOK_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_help,                           DISPLAYLIST_HELP_SCREEN_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_rdb_entry_detail,               DISPLAYLIST_DATABASE_ENTRY)
GENERIC_DEFERRED_PUSH(deferred_push_rpl_entry_actions,              DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS)
GENERIC_DEFERRED_PUSH(deferred_push_core_list_deferred,             DISPLAYLIST_CORES_SUPPORTED)
GENERIC_DEFERRED_PUSH(deferred_push_core_collection_list_deferred,  DISPLAYLIST_CORES_COLLECTION_SUPPORTED)
GENERIC_DEFERRED_PUSH(deferred_push_menu_sounds_list,               DISPLAYLIST_MENU_SOUNDS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_rgui_theme_preset,              DISPLAYLIST_RGUI_THEME_PRESETS)

#ifdef HAVE_NETWORKING
GENERIC_DEFERRED_PUSH(deferred_push_thumbnails_updater_list,        DISPLAYLIST_THUMBNAILS_UPDATER)
GENERIC_DEFERRED_PUSH(deferred_push_pl_thumbnails_updater_list,     DISPLAYLIST_PL_THUMBNAILS_UPDATER)
GENERIC_DEFERRED_PUSH(deferred_push_core_updater_list,              DISPLAYLIST_CORES_UPDATER)
GENERIC_DEFERRED_PUSH(deferred_push_core_content_list,              DISPLAYLIST_CORE_CONTENT)
GENERIC_DEFERRED_PUSH(deferred_push_core_content_dirs_list,         DISPLAYLIST_CORE_CONTENT_DIRS)
GENERIC_DEFERRED_PUSH(deferred_push_core_content_dirs_subdir_list,  DISPLAYLIST_CORE_CONTENT_DIRS_SUBDIR)
GENERIC_DEFERRED_PUSH(deferred_push_core_system_files_list,         DISPLAYLIST_CORE_SYSTEM_FILES)
GENERIC_DEFERRED_PUSH(deferred_push_lakka_list,                     DISPLAYLIST_LAKKA)
#endif

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
GENERIC_DEFERRED_PUSH(deferred_push_switch_cpu_profile,             DISPLAYLIST_SWITCH_CPU_PROFILE)
#endif

#ifdef HAVE_LAKKA_SWITCH
GENERIC_DEFERRED_PUSH(deferred_push_switch_gpu_profile,             DISPLAYLIST_SWITCH_GPU_PROFILE)
#endif

#if defined(HAVE_LAKKA)
GENERIC_DEFERRED_PUSH(deferred_push_cpu_perfpower,                  DISPLAYLIST_CPU_PERFPOWER_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_cpu_policy,                     DISPLAYLIST_CPU_POLICY_LIST)
#endif

GENERIC_DEFERRED_PUSH(deferred_push_manual_content_scan_list,       DISPLAYLIST_MANUAL_CONTENT_SCAN_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_manual_content_scan_dat_file,   DISPLAYLIST_MANUAL_CONTENT_SCAN_DAT_FILES)

GENERIC_DEFERRED_PUSH(deferred_push_core_restore_backup_list,       DISPLAYLIST_CORE_RESTORE_BACKUP_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_core_delete_backup_list,        DISPLAYLIST_CORE_DELETE_BACKUP_LIST)

GENERIC_DEFERRED_PUSH(deferred_push_core_manager_list,              DISPLAYLIST_CORE_MANAGER_LIST)

#ifdef HAVE_MIST
GENERIC_DEFERRED_PUSH(deferred_push_steam_settings_list,            DISPLAYLIST_STEAM_SETTINGS_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_core_manager_steam_list,        DISPLAYLIST_CORE_MANAGER_STEAM_LIST)
GENERIC_DEFERRED_PUSH(deferred_push_core_information_steam_list,    DISPLAYLIST_CORE_INFORMATION_STEAM_LIST)
#endif

GENERIC_DEFERRED_PUSH(deferred_push_file_browser_select_sideload_core, DISPLAYLIST_FILE_BROWSER_SELECT_SIDELOAD_CORE)

static int deferred_push_cursor_manager_list_deferred(
      menu_displaylist_info_t *info)
{
   char rdb_path[PATH_MAX_LENGTH];
   const char *path               = info->path;
   settings_t *settings           = NULL;
   config_file_t *conf            = NULL;
   struct config_entry_list 
      *query_entry                = NULL;
   struct config_entry_list 
      *rdb_entry                  = NULL;
   
   if (!(conf = config_file_new_from_path_to_string(path)))
      return -1;
   
   query_entry                    = config_get_entry(conf, "query");
   rdb_entry                      = config_get_entry(conf, "rdb");

   if (     
            !query_entry
         ||  (string_is_empty(query_entry->value))
         || !rdb_entry
         ||  (string_is_empty(rdb_entry->value))
      )
   {
      config_file_free(conf);
      return -1;
   }

   settings = config_get_ptr();
   
   fill_pathname_join_special(rdb_path,
         settings->paths.path_content_database,
         rdb_entry->value, sizeof(rdb_path));
   
   if (!string_is_empty(info->path_b))
      free(info->path_b);

   if (!string_is_empty(info->path_c))
      free(info->path_c);

   info->path_b    = strdup(info->path);

   if (!string_is_empty(info->path))
      free(info->path);

   info->path_c    = strdup(query_entry->value);
   info->path      = strdup(rdb_path);
   
   config_file_free(conf);

   return deferred_push_dlist(info, DISPLAYLIST_DATABASE_QUERY, settings);
}

#ifdef HAVE_LIBRETRODB
static int deferred_push_cursor_manager_list_generic(
      menu_displaylist_info_t *info, enum database_query_type type)
{
   char query[256];
   int ret                       = -1;
   const char *path              = info->path;
   struct string_list str_list   = {0};
   settings_t *settings          = config_get_ptr();
   
   if (!path)
      goto end;

   string_list_initialize(&str_list);
   string_split_noalloc(&str_list, path, "|");

   database_info_build_query_enum(query, sizeof(query), type,
         str_list.elems[0].data);

   if (string_is_empty(query))
      goto end;

   if (!string_is_empty(info->path_b))
      free(info->path_b);
   if (!string_is_empty(info->path_c))
      free(info->path_c);
   if (!string_is_empty(info->path))
      free(info->path);

   info->path   = strdup(str_list.elems[1].data);
   info->path_b = strdup(str_list.elems[0].data);
   info->path_c = strdup(query);

   ret = deferred_push_dlist(info, DISPLAYLIST_DATABASE_QUERY, settings);

end:
   string_list_deinitialize(&str_list);
   return ret;
}

GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_max_users, DATABASE_QUERY_ENTRY_MAX_USERS)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_famitsu_magazine_rating, DATABASE_QUERY_ENTRY_FAMITSU_MAGAZINE_RATING)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_rating, DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_RATING)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_issue, DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_ISSUE)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_elspa_rating, DATABASE_QUERY_ENTRY_ELSPA_RATING)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_cero_rating, DATABASE_QUERY_ENTRY_CERO_RATING)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_pegi_rating, DATABASE_QUERY_ENTRY_PEGI_RATING)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_bbfc_rating, DATABASE_QUERY_ENTRY_BBFC_RATING)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_esrb_rating, DATABASE_QUERY_ENTRY_ESRB_RATING)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_enhancement_hw, DATABASE_QUERY_ENTRY_ENHANCEMENT_HW)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_franchise, DATABASE_QUERY_ENTRY_FRANCHISE)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_publisher, DATABASE_QUERY_ENTRY_PUBLISHER)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_developer, DATABASE_QUERY_ENTRY_DEVELOPER)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_origin, DATABASE_QUERY_ENTRY_ORIGIN)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_releasemonth, DATABASE_QUERY_ENTRY_RELEASEDATE_MONTH)
GENERIC_DEFERRED_CURSOR_MANAGER(deferred_push_cursor_manager_list_deferred_query_rdb_entry_releaseyear, DATABASE_QUERY_ENTRY_RELEASEDATE_YEAR)
#endif

static int general_push(menu_displaylist_info_t *info,
      unsigned id, enum menu_displaylist_ctl_state state)
{
   char newstring2[PATH_MAX_LENGTH];
   core_info_list_t           *list           = NULL;
   settings_t                  *settings      = config_get_ptr();
   menu_handle_t                  *menu       = menu_state_get_ptr()->driver_data;
   bool 
      multimedia_builtin_mediaplayer_enable   = settings->bools.multimedia_builtin_mediaplayer_enable;
   bool multimedia_builtin_imageviewer_enable = settings->bools.multimedia_builtin_imageviewer_enable;
   bool filter_by_current_core                = settings->bools.filter_by_current_core;

   if (!menu)
      return menu_cbs_exit();

   core_info_get_list(&list);

   switch (id)
   {
      case PUSH_DEFAULT:
      case PUSH_DETECT_CORE_LIST:
         break;
      default:
         {
            char tmp_str[PATH_MAX_LENGTH];
            char tmp_str2[PATH_MAX_LENGTH];
            fill_pathname_join_special(tmp_str, menu->scratch2_buf,
                  menu->scratch_buf, sizeof(tmp_str));
            fill_pathname_join_special(tmp_str2, menu->scratch2_buf,
                  menu->scratch_buf, sizeof(tmp_str2));

            if (!string_is_empty(info->path))
               free(info->path);
            if (!string_is_empty(info->label))
               free(info->label);

            info->path  = strdup(tmp_str);
            info->label = strdup(tmp_str2);
         }
         break;
   }

   info->type_default = FILE_TYPE_PLAIN;

   switch (id)
   {
      case PUSH_ARCHIVE_OPEN_DETECT_CORE:
      case PUSH_ARCHIVE_OPEN:
      case PUSH_DEFAULT:
         info->setting      = menu_setting_find_enum(info->enum_idx);
         break;
      default:
         break;
   }

   newstring2[0]                  = '\0';

   switch (id)
   {
      case PUSH_ARCHIVE_OPEN:
         {
            struct retro_system_info *system = 
               &runloop_state_get_ptr()->system.info;
            if (system)
               if (!string_is_empty(system->valid_extensions))
                  strlcpy(newstring2, system->valid_extensions,
                        sizeof(newstring2));
         }
         break;
      case PUSH_DEFAULT:
         {
            const char *valid_extensions     = NULL;
            struct retro_system_info *system = NULL;

            if (menu_setting_get_browser_selection_type(info->setting) 
                  != ST_DIR)
            {
               system = &runloop_state_get_ptr()->system.info;

               if (system && !string_is_empty(system->valid_extensions))
                  valid_extensions = system->valid_extensions;
            }

            if (!valid_extensions)
               valid_extensions = info->exts;

            if (!string_is_empty(valid_extensions))
            {
               struct string_list str_list3 = {0};

               string_list_initialize(&str_list3);
               string_split_noalloc(&str_list3, valid_extensions, "|");

#ifdef HAVE_IBXM
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  string_list_append(&str_list3, "s3m", attr);
                  string_list_append(&str_list3, "mod", attr);
                  string_list_append(&str_list3, "xm", attr);
               }
#endif
               string_list_join_concat(newstring2, sizeof(newstring2),
                     &str_list3, "|");
               string_list_deinitialize(&str_list3);
            }
         }
         break;
      case PUSH_ARCHIVE_OPEN_DETECT_CORE:
      case PUSH_DETECT_CORE_LIST:
         {
            union string_list_elem_attr attr;
            char newstring[PATH_MAX_LENGTH];
            struct string_list str_list2     = {0};
            struct retro_system_info *system = 
               &runloop_state_get_ptr()->system.info;

            attr.i                           = 0;

            string_list_initialize(&str_list2);

            if (system)
            {
               if (!string_is_empty(system->valid_extensions))
               {
                  unsigned x;
                  struct string_list  str_list    = {0};

                  string_list_initialize(&str_list);
                  string_split_noalloc(&str_list,
                        system->valid_extensions, "|");

                  for (x = 0; x < str_list.size; x++)
                  {
                     const char *elem = str_list.elems[x].data;
                     string_list_append(&str_list2, elem, attr);
                  }

                  string_list_deinitialize(&str_list);
               }
            }

            if (!filter_by_current_core)
            {
               if (list && !string_is_empty(list->all_ext))
               {
                  unsigned x;
                  struct string_list str_list  = {0};
                  string_list_initialize(&str_list);

                  string_split_noalloc(&str_list, 
                        list->all_ext, "|");

                  for (x = 0; x < str_list.size; x++)
                  {
                     if (!string_list_find_elem(&str_list2,
                              str_list.elems[x].data))
                     {
                        const char *elem = str_list.elems[x].data;
                        string_list_append(&str_list2, elem, attr);
                     }
                  }

                  string_list_deinitialize(&str_list);
               }
            }

            string_list_join_concat(newstring, sizeof(newstring),
                  &str_list2, "|");

            {
               struct string_list  str_list3  = {0};
               string_list_initialize(&str_list3);
               string_split_noalloc(&str_list3, newstring, "|");

#ifdef HAVE_IBXM
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  string_list_append(&str_list3, "s3m", attr);
                  string_list_append(&str_list3, "mod", attr);
                  string_list_append(&str_list3, "xm", attr);
               }
#endif
               string_list_join_concat(newstring2, sizeof(newstring2),
                     &str_list3, "|");
               string_list_deinitialize(&str_list3);
            }
            string_list_deinitialize(&str_list2);
         }
         break;
   }

   if (multimedia_builtin_mediaplayer_enable ||
         multimedia_builtin_imageviewer_enable)
   {
      struct retro_system_info sysinfo = {0};

      (void)sysinfo;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      if (multimedia_builtin_mediaplayer_enable)
      {
#if defined(HAVE_FFMPEG)
         libretro_ffmpeg_retro_get_system_info(&sysinfo);
#elif defined(HAVE_MPV)
         libretro_mpv_retro_get_system_info(&sysinfo);
#endif
         strlcat(newstring2, "|", sizeof(newstring2));
         strlcat(newstring2, sysinfo.valid_extensions, sizeof(newstring2));
      }
#endif
#ifdef HAVE_IMAGEVIEWER
      if (multimedia_builtin_imageviewer_enable)
      {
         libretro_imageviewer_retro_get_system_info(&sysinfo);
         strlcat(newstring2, "|", sizeof(newstring2));
         strlcat(newstring2, sysinfo.valid_extensions,
               sizeof(newstring2));
      }
#endif
   }

   if (!string_is_empty(newstring2))
   {
      if (info->exts)
         free(info->exts);
      info->exts = strdup(newstring2);
   }

   return deferred_push_dlist(info, state, settings);
}

GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_detect_core_list, PUSH_DETECT_CORE_LIST, DISPLAYLIST_CORES_DETECTED)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_archive_open_detect_core, PUSH_ARCHIVE_OPEN_DETECT_CORE, DISPLAYLIST_DEFAULT)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_archive_open, PUSH_ARCHIVE_OPEN, DISPLAYLIST_DEFAULT)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_default, PUSH_DEFAULT, DISPLAYLIST_DEFAULT)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_favorites_list, PUSH_DEFAULT, DISPLAYLIST_FAVORITES)

GENERIC_DEFERRED_PUSH_GENERAL(deferred_playlist_list, PUSH_DEFAULT, DISPLAYLIST_PLAYLIST)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_music_history_list, PUSH_DEFAULT, DISPLAYLIST_MUSIC_HISTORY)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_image_history_list, PUSH_DEFAULT, DISPLAYLIST_IMAGES_HISTORY)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_video_history_list, PUSH_DEFAULT, DISPLAYLIST_VIDEO_HISTORY)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_explore_list, PUSH_DEFAULT, DISPLAYLIST_EXPLORE)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_contentless_cores_list, PUSH_DEFAULT, DISPLAYLIST_CONTENTLESS_CORES)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_special, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_SPECIAL)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_resolution, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_RESOLUTION)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_video_shader_num_passes, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_NUM_PASSES)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_shader_parameter, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_PARAMETER)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_shader_preset_parameter, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_PRESET_PARAMETER)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_playlist_default_core, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_DEFAULT_CORE)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_playlist_label_display_mode, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LABEL_DISPLAY_MODE)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_playlist_right_thumbnail_mode, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_playlist_left_thumbnail_mode, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_playlist_sort_mode, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_SORT_MODE)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_manual_content_scan_system_name, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_manual_content_scan_core_name, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_CORE_NAME)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_disk_index, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_DISK_INDEX)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_input_device_type, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_INPUT_DEVICE_TYPE)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_input_device_index, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_INPUT_DEVICE_INDEX)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_input_description, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_INPUT_DESCRIPTION)
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_input_description_kbd, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_INPUT_DESCRIPTION_KBD)
#ifdef HAVE_NETWORKING
GENERIC_DEFERRED_PUSH_GENERAL(deferred_push_dropdown_box_list_netplay_mitm_server, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_NETPLAY_MITM_SERVER)
#endif

static int menu_cbs_init_bind_deferred_push_compare_label(
      menu_file_list_cbs_t *cbs,
      const char *label)
{
   unsigned i;
   typedef struct deferred_info_list 
   {
      enum msg_hash_enums type;
      int (*cb)(menu_displaylist_info_t *info);
   } deferred_info_list_t;

   const deferred_info_list_t info_list[] = {
      {MENU_ENUM_LABEL_DEFERRED_DUMP_DISC_LIST, deferred_push_dump_disk_list},
#ifdef HAVE_LAKKA
      {MENU_ENUM_LABEL_DEFERRED_EJECT_DISC, deferred_push_eject_disc},
#endif
      {MENU_ENUM_LABEL_DEFERRED_LOAD_DISC_LIST, deferred_push_load_disk_list},
      {MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST, deferred_push_favorites_list},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST, deferred_push_dropdown_box_list},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL, deferred_push_dropdown_box_list_special},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION, deferred_push_dropdown_box_list_resolution},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES, deferred_push_dropdown_box_list_video_shader_num_passes},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PARAMETER, deferred_push_dropdown_box_list_shader_parameter},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PRESET_PARAMETER, deferred_push_dropdown_box_list_shader_preset_parameter},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE, deferred_push_dropdown_box_list_playlist_default_core},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE, deferred_push_dropdown_box_list_playlist_label_display_mode},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE, deferred_push_dropdown_box_list_playlist_right_thumbnail_mode},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE, deferred_push_dropdown_box_list_playlist_left_thumbnail_mode},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE, deferred_push_dropdown_box_list_playlist_sort_mode},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX, deferred_push_dropdown_box_list_disk_index},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE, deferred_push_dropdown_box_list_input_device_type},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DEVICE_INDEX, deferred_push_dropdown_box_list_input_device_index},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION, deferred_push_dropdown_box_list_input_description},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD, deferred_push_dropdown_box_list_input_description_kbd},
#ifdef HAVE_NETWORKING
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER, deferred_push_dropdown_box_list_netplay_mitm_server},
#endif
      {MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_LIST, deferred_push_browse_url_list},
      {MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_START, deferred_push_browse_url_start},
      {MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST, deferred_push_core_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_INFORMATION_LIST, deferred_push_core_information_list},
      {MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST, deferred_push_configuration_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST, deferred_push_saving_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MIXER_STREAM_SETTINGS_LIST, deferred_push_mixer_stream_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST, deferred_push_logging_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST, deferred_push_frame_throttle_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_FRAME_TIME_COUNTER_SETTINGS_LIST, deferred_push_frame_time_counter_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST, deferred_push_rewind_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CHEAT_DETAILS_SETTINGS_LIST, deferred_push_cheat_details_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CHEAT_SEARCH_SETTINGS_LIST, deferred_push_cheat_search_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST, deferred_push_onscreen_display_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST, deferred_push_onscreen_notifications_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST, deferred_push_onscreen_notifications_views_settings_list},
#if defined(HAVE_OVERLAY)
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST, deferred_push_onscreen_overlay_settings_list},
#endif
#ifdef HAVE_VIDEO_LAYOUT
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST, deferred_push_onscreen_video_layout_settings_list},
#endif
      {MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST, deferred_push_menu_file_browser_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_VIEWS_SETTINGS_LIST, deferred_push_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SETTINGS_VIEWS_SETTINGS_LIST, deferred_push_settings_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_VIEWS_SETTINGS_LIST, deferred_push_quick_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST, deferred_push_menu_settings_list},
#ifdef _3DS
      {MENU_ENUM_LABEL_DEFERRED_MENU_BOTTOM_SETTINGS_LIST, deferred_push_menu_bottom_settings_list},
#endif
      {MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST, deferred_push_user_interface_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_POWER_MANAGEMENT_SETTINGS_LIST, deferred_push_power_management_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_SOUNDS_LIST, deferred_push_menu_sounds_list},
      {MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST, deferred_push_retro_achievements_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST, deferred_push_updater_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST, deferred_push_network_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SUBSYSTEM_SETTINGS_LIST, deferred_push_subsystem_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETWORK_HOSTING_SETTINGS_LIST, deferred_push_network_hosting_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETPLAY_KICK_LIST, deferred_push_netplay_kick_list},
      {MENU_ENUM_LABEL_DEFERRED_NETPLAY_BAN_LIST, deferred_push_netplay_ban_list},
      {MENU_ENUM_LABEL_DEFERRED_NETPLAY_LOBBY_FILTERS_LIST, deferred_push_netplay_lobby_filters_list},
      {MENU_ENUM_LABEL_DEFERRED_BLUETOOTH_SETTINGS_LIST, deferred_push_bluetooth_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST, deferred_push_wifi_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_WIFI_NETWORKS_LIST, deferred_push_wifi_networks_list},
      {MENU_ENUM_LABEL_DEFERRED_LAKKA_SERVICES_LIST, deferred_push_lakka_services_list},
      {MENU_ENUM_LABEL_DEFERRED_USER_SETTINGS_LIST, deferred_push_user_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST, deferred_push_directory_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_PRIVACY_SETTINGS_LIST, deferred_push_privacy_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MIDI_SETTINGS_LIST, deferred_push_midi_settings_list},
#ifdef HAVE_NETWORKING
      {MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST, deferred_push_core_content_dirs_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST, deferred_push_core_content_dirs_subdir_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST, deferred_push_core_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST, deferred_push_thumbnails_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_PL_THUMBNAILS_UPDATER_LIST, deferred_push_pl_thumbnails_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST, deferred_push_core_content_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_SYSTEM_FILES_LIST, deferred_push_core_system_files_list},
#endif
      {MENU_ENUM_LABEL_DEFERRED_MUSIC, deferred_music_list},
      {MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST, deferred_music_history_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST, deferred_playlist_list},
      {MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST, deferred_image_history_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST, deferred_video_history_list},
      {MENU_ENUM_LABEL_DEFERRED_EXPLORE_LIST, deferred_explore_list},
      {MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST, deferred_contentless_cores_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST, deferred_push_input_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_MENU_SETTINGS_LIST, deferred_push_input_menu_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_TURBO_FIRE_SETTINGS_LIST, deferred_push_input_turbo_fire_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST, deferred_push_input_haptic_feedback_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AI_SERVICE_SETTINGS_LIST, deferred_push_ai_service_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCESSIBILITY_SETTINGS_LIST, deferred_push_accessibility_settings_list},
      {MENU_ENUM_LABEL_DISC_INFORMATION, deferred_push_disc_information},
      {MENU_ENUM_LABEL_SYSTEM_INFORMATION, deferred_push_system_information},
      {MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS, deferred_push_rpl_entry_actions},
      {MENU_ENUM_LABEL_DEFERRED_NETPLAY, deferred_push_netplay_sublist},
      {MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST, deferred_push_driver_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST, deferred_push_video_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST, deferred_push_video_fullscreen_mode_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_WINDOWED_MODE_SETTINGS_LIST, deferred_push_video_windowed_mode_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SYNCHRONIZATION_SETTINGS_LIST, deferred_push_video_synchronization_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_OUTPUT_SETTINGS_LIST, deferred_push_video_output_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SCALING_SETTINGS_LIST, deferred_push_video_scaling_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_HDR_SETTINGS_LIST, deferred_push_video_hdr_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CRT_SWITCHRES_SETTINGS_LIST, deferred_push_crt_switchres_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST, deferred_push_audio_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SYNCHRONIZATION_SETTINGS_LIST, deferred_push_audio_synchronization_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_OUTPUT_SETTINGS_LIST, deferred_push_audio_output_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_RESAMPLER_SETTINGS_LIST, deferred_push_audio_resampler_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_MIXER_SETTINGS_LIST, deferred_push_audio_mixer_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_LATENCY_SETTINGS_LIST, deferred_push_latency_settings_list},
#ifdef HAVE_LAKKA_SWITCH
      {MENU_ENUM_LABEL_SWITCH_GPU_PROFILE, deferred_push_switch_gpu_profile},
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      {MENU_ENUM_LABEL_SWITCH_CPU_PROFILE, deferred_push_switch_cpu_profile},
#endif
#if defined(HAVE_LAKKA)
      {MENU_ENUM_LABEL_DEFERRED_CPU_PERFPOWER_LIST, deferred_push_cpu_perfpower},
      {MENU_ENUM_LABEL_DEFERRED_CPU_POLICY_ENTRY, deferred_push_cpu_policy},
#endif
      {MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST, deferred_push_remappings_port },
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST, deferred_push_accounts_list},
      {MENU_ENUM_LABEL_CORE_LIST, deferred_push_core_list},
      {MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY, deferred_push_history_list},
      {MENU_ENUM_LABEL_SAVESTATE_LIST, deferred_push_savestate_list},
      {MENU_ENUM_LABEL_CORE_OPTIONS, deferred_push_core_options},
      {MENU_ENUM_LABEL_DEFERRED_CORE_OPTION_OVERRIDE_LIST, deferred_push_core_option_override_list},
      {MENU_ENUM_LABEL_NETWORK_INFORMATION, deferred_push_network_information},
      {MENU_ENUM_LABEL_ONLINE_UPDATER, deferred_push_options},
      {MENU_ENUM_LABEL_HELP_LIST, deferred_push_help},
      {MENU_ENUM_LABEL_INFORMATION_LIST, deferred_push_information_list},
      {MENU_ENUM_LABEL_INFORMATION, deferred_push_information},
      {MENU_ENUM_LABEL_SHADER_OPTIONS, deferred_push_shader_options},
      {MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST, deferred_user_binds_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST, deferred_push_input_hotkey_binds_list},
      {MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_OVERRIDE_OPTIONS, deferred_push_quick_menu_override_options},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_YOUTUBE_LIST, deferred_push_accounts_youtube_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_TWITCH_LIST, deferred_push_accounts_twitch_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_FACEBOOK_LIST, deferred_push_accounts_facebook_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_SAVE_LIST, deferred_push_video_shader_preset_save},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_REMOVE_LIST, deferred_push_video_shader_preset_remove},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME, deferred_push_dropdown_box_list_manual_content_scan_system_name},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME, deferred_push_dropdown_box_list_manual_content_scan_core_name},
      {MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST, deferred_push_recording_settings_list},
      {MENU_ENUM_LABEL_COLLECTION, deferred_push_content_collection_list},
      {MENU_ENUM_LABEL_SETTINGS,   deferred_push_settings},
      {MENU_ENUM_LABEL_CONFIGURATIONS_LIST, deferred_push_configurations_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_LIST, deferred_push_playlist_manager_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_SETTINGS, deferred_push_playlist_manager_settings},
      {MENU_ENUM_LABEL_LOAD_CONTENT_LIST, deferred_push_load_content_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST, deferred_push_playlist_settings_list},
      {MENU_ENUM_LABEL_MANAGEMENT, deferred_push_management_options},
      {MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST, deferred_push_database_manager_list_deferred},
      {MENU_ENUM_LABEL_CONFIGURATIONS, deferred_push_configurations},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST, deferred_push_accounts_cheevos_list},
      {MENU_ENUM_LABEL_DATABASE_MANAGER_LIST, deferred_push_database_manager_list},
      {MENU_ENUM_LABEL_CURSOR_MANAGER_LIST, deferred_push_cursor_manager_list},
#ifdef HAVE_LIBRETRODB
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST, deferred_push_cursor_manager_list_deferred},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER, deferred_push_cursor_manager_list_deferred_query_rdb_entry_publisher},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER, deferred_push_cursor_manager_list_deferred_query_rdb_entry_developer},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN, deferred_push_cursor_manager_list_deferred_query_rdb_entry_origin},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE, deferred_push_cursor_manager_list_deferred_query_rdb_entry_franchise},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ENHANCEMENT_HW, deferred_push_cursor_manager_list_deferred_query_rdb_entry_enhancement_hw},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING, deferred_push_cursor_manager_list_deferred_query_rdb_entry_esrb_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING, deferred_push_cursor_manager_list_deferred_query_rdb_entry_bbfc_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING, deferred_push_cursor_manager_list_deferred_query_rdb_entry_elspa_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING, deferred_push_cursor_manager_list_deferred_query_rdb_entry_pegi_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING, deferred_push_cursor_manager_list_deferred_query_rdb_entry_cero_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING, deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE, deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_issue},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FAMITSU_MAGAZINE_RATING, deferred_push_cursor_manager_list_deferred_query_rdb_entry_famitsu_magazine_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS, deferred_push_cursor_manager_list_deferred_query_rdb_entry_max_users},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH, deferred_push_cursor_manager_list_deferred_query_rdb_entry_releasemonth},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR, deferred_push_cursor_manager_list_deferred_query_rdb_entry_releaseyear},
#endif
#ifdef HAVE_VIDEO_LAYOUT
      {MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH, deferred_push_video_layout_path}, 
#endif
      {MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU, deferred_push_achievement_pause_menu},
      {MENU_ENUM_LABEL_ACHIEVEMENT_LIST, deferred_push_achievement_list},
      {MENU_ENUM_LABEL_CORE_COUNTERS, deferred_push_core_counters},
      {MENU_ENUM_LABEL_FRONTEND_COUNTERS, deferred_push_frontend_counters},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS, deferred_push_video_shader_preset_parameters},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS, deferred_push_video_shader_parameters},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE, deferred_push_video_shader_preset_save},
      {MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS, deferred_push_core_cheat_options},
      {MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS, deferred_push_core_input_remapping_options},
      {MENU_ENUM_LABEL_DEFERRED_REMAP_FILE_MANAGER_LIST, deferred_push_remap_file_manager},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET, deferred_push_video_shader_preset},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PASS, deferred_push_video_shader_pass},
      {MENU_ENUM_LABEL_VIDEO_FILTER, deferred_push_video_filter},
      {MENU_ENUM_LABEL_MENU_WALLPAPER, deferred_push_images},
      {MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN, deferred_push_audio_dsp_plugin},
      {MENU_ENUM_LABEL_INPUT_OVERLAY, deferred_push_input_overlay},
      {MENU_ENUM_LABEL_VIDEO_FONT_PATH, deferred_push_video_font_path},
      {MENU_ENUM_LABEL_XMB_FONT, deferred_push_xmb_font_path},
      {MENU_ENUM_LABEL_CHEAT_FILE_LOAD, deferred_push_cheat_file_load},
      {MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND, deferred_push_cheat_file_load_append},
      {MENU_ENUM_LABEL_REMAP_FILE_LOAD, deferred_push_remap_file_load},
      {MENU_ENUM_LABEL_RECORD_CONFIG, deferred_push_record_configfile},
      {MENU_ENUM_LABEL_STREAM_CONFIG, deferred_push_stream_configfile},
      {MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET, deferred_push_rgui_theme_preset},
      {MENU_ENUM_LABEL_NETPLAY, deferred_push_netplay},
      {MENU_ENUM_LABEL_CONTENT_SETTINGS, deferred_push_content_settings},
      {MENU_ENUM_LABEL_ADD_CONTENT_LIST, deferred_push_add_content_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_LIST, deferred_push_core_list_deferred},
      {MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET, deferred_push_core_collection_list_deferred},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_FILTER, deferred_push_video_filter},
      {MENU_ENUM_LABEL_CONTENT_HISTORY_PATH, deferred_push_content_history_path},
      {MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST, deferred_push_detect_core_list},
      {MENU_ENUM_LABEL_FAVORITES, deferred_push_detect_core_list},
      {MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST, deferred_push_manual_content_scan_list},
      {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE, deferred_push_manual_content_scan_dat_file},
      {MENU_ENUM_LABEL_DEFERRED_CORE_RESTORE_BACKUP_LIST, deferred_push_core_restore_backup_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_DELETE_BACKUP_LIST, deferred_push_core_delete_backup_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST, deferred_push_core_manager_list},
#ifdef HAVE_MIST
      {MENU_ENUM_LABEL_DEFERRED_STEAM_SETTINGS_LIST, deferred_push_steam_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_STEAM_LIST, deferred_push_core_manager_steam_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_INFORMATION_STEAM_LIST, deferred_push_core_information_steam_list},
#endif
      {MENU_ENUM_LABEL_SIDELOAD_CORE_LIST, deferred_push_file_browser_select_sideload_core},
      {MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE, deferred_archive_action_detect_core},
      {MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION, deferred_archive_action},
      {MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE, deferred_archive_open_detect_core},
      {MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN, deferred_archive_open},
#ifdef HAVE_NETWORKING
      {MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST, deferred_push_lakka_list},
#endif
   };

   if (!string_is_equal(label, "null"))
   {
      for (i = 0; i < ARRAY_SIZE(info_list); i++)
      {
         if (string_is_equal(label, msg_hash_to_str(info_list[i].type)))
         {
            BIND_ACTION_DEFERRED_PUSH(cbs, info_list[i].cb);
            return 0;
         }
      }

      /* MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL requires special
       * treatment, since the label has the format:
       *   <MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL>|<entry_name>
       * i.e. cannot use a normal string_is_equal() */
      if (string_starts_with(label,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL)))
      {
         BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_rdb_entry_detail);
      }
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_MAIN_MENU:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_main_menu_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_user_binds_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_accounts_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_playlist_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_playlist_manager_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_SETTINGS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_playlist_manager_settings);
            break;
         case MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_recording_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_input_hotkey_binds_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_accounts_cheevos_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_YOUTUBE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_accounts_youtube_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_TWITCH_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_accounts_twitch_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_FACEBOOK_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_accounts_facebook_list);
            break;            
         case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_action_detect_core);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_action);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_open_detect_core);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_open);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_content_list);
#endif
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_content_dirs_list);
#endif
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_content_dirs_subdir_list);
#endif
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_SYSTEM_FILES_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_system_files_list);
#endif
            break;
         case MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_thumbnails_updater_list);
#endif
            break;
         case MENU_ENUM_LABEL_DEFERRED_PL_THUMBNAILS_UPDATER_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_pl_thumbnails_updater_list);
#endif
            break;
         case MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_lakka_list);
#endif
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_history_list);
            break;
         case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_database_manager_list);
            break;
         case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cheat_file_load);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cheat_file_load_append);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_remap_file_load);
            break;
         case MENU_ENUM_LABEL_RECORD_CONFIG:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_record_configfile);
            break;
         case MENU_ENUM_LABEL_STREAM_CONFIG:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_stream_configfile);
            break;
         case MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_rgui_theme_preset);
            break;
         case MENU_ENUM_LABEL_SHADER_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_shader_options);
            break;
         case MENU_ENUM_LABEL_ONLINE_UPDATER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_options);
            break;
         case MENU_ENUM_LABEL_NETPLAY:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_netplay);
            break;
         case MENU_ENUM_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_content_settings);
            break;
         case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_add_content_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_configurations_list);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_load_content_list);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_load_content_special);
            break;
         case MENU_ENUM_LABEL_INFORMATION_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_information_list);
            break;
         case MENU_ENUM_LABEL_INFORMATION:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_information);
            break;
         case MENU_ENUM_LABEL_MANAGEMENT:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_management_options);
            break;
         case MENU_ENUM_LABEL_HELP_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_help);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_list_deferred);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_collection_list_deferred);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_FILTER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_filter);
            break;
         case MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_database_manager_list_deferred);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred);
            break;
#ifdef HAVE_LIBRETRODB
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_publisher);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_developer);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_origin);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_franchise);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ENHANCEMENT_HW:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_enhancement_hw);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_esrb_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_bbfc_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_elspa_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_pegi_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_cero_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_issue);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_famitsu_magazine_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_max_users);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_releasemonth);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_releaseyear);
            break;
#endif
         case MENU_ENUM_LABEL_NETWORK_INFORMATION:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_network_information);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_achievement_list);
            break;
         case MENU_ENUM_LABEL_CORE_COUNTERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_counters);
            break;
         case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_frontend_counters);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_preset_parameters);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_parameters);
            break;
         case MENU_ENUM_LABEL_SETTINGS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_settings);
            break;
         case MENU_ENUM_LABEL_SAVESTATE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_savestate_list);
            break;
         case MENU_ENUM_LABEL_CORE_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_options);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_OPTION_OVERRIDE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_option_override_list);
            break;
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_cheat_options);
            break;
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_input_remapping_options);
            break;
         case MENU_ENUM_LABEL_DEFERRED_REMAP_FILE_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_remap_file_manager);
            break;
         case MENU_ENUM_LABEL_CORE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_list);
            break;
         case MENU_ENUM_LABEL_PLAYLISTS_TAB:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_content_collection_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_configurations);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_preset);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_pass);
            break;
         case MENU_ENUM_LABEL_VIDEO_FILTER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_filter);
            break;
         case MENU_ENUM_LABEL_MENU_WALLPAPER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_images);
            break;
         case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_audio_dsp_plugin);
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_input_overlay);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_layout_path);
            break;
#endif
         case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_font_path);
            break;
         case MENU_ENUM_LABEL_XMB_FONT:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_xmb_font_path);
            break;
         case MENU_ENUM_LABEL_CONTENT_HISTORY_PATH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_content_history_path);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_fullscreen_mode_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_WINDOWED_MODE_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_windowed_mode_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_SYNCHRONIZATION_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_synchronization_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_OUTPUT_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_output_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_HDR_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_hdr_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_SCALING_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_scaling_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CRT_SWITCHRES_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_crt_switchres_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_configuration_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_saving_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_saving_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_frame_throttle_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_FRAME_TIME_COUNTER_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_frame_time_counter_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_rewind_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_onscreen_display_settings_list);
            break;
#if defined(HAVE_OVERLAY)
         case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_onscreen_overlay_settings_list);
            break;
#endif
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_onscreen_video_layout_settings_list);
            break;
#endif
         case MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_audio_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_AUDIO_OUTPUT_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_audio_output_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_AUDIO_RESAMPLER_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_audio_resampler_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_AUDIO_SYNCHRONIZATION_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_audio_synchronization_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_LATENCY_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_latency_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_INFORMATION_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_information_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_DUMP_DISC_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_dump_disk_list);
            break;
#ifdef HAVE_LAKKA
         case MENU_ENUM_LABEL_DEFERRED_EJECT_DISC:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_eject_disc);
            break;
#endif
         case MENU_ENUM_LABEL_DEFERRED_CDROM_INFO_DETAIL_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cdrom_info_detail_list);
            break;
         case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         case MENU_ENUM_LABEL_FAVORITES:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_detect_core_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_manual_content_scan_list);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_manual_content_scan_dat_file);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_RESTORE_BACKUP_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_restore_backup_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_DELETE_BACKUP_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_delete_backup_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_manager_list);
            break;
#ifdef HAVE_MIST
         case MENU_ENUM_LABEL_DEFERRED_STEAM_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_steam_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_STEAM_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_manager_steam_list);
            break;
#endif
         case MENU_ENUM_LABEL_SIDELOAD_CORE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_file_browser_select_sideload_core);
            break;
         default:
            return -1;
      }

      return 0;
   }

   return -1;
}

static int menu_cbs_init_bind_deferred_push_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   if (type == FILE_TYPE_PLAYLIST_COLLECTION)
   {
      BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_rdb_collection);
   }
   else if (type == MENU_SETTING_ACTION_CORE_DISK_OPTIONS)
   {
      BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_disk_options);
   }
   else if (type == MENU_SET_CDROM_INFO)
   {
      BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cdrom_info_detail_list);
      return 0;
   }
   else
      return -1;

   return 0;
}

int menu_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_default);

   if (cbs->enum_idx != MENU_ENUM_LABEL_PLAYLIST_ENTRY &&
       menu_cbs_init_bind_deferred_push_compare_label(cbs, label) == 0)
      return 0;

   if (menu_cbs_init_bind_deferred_push_compare_type(
            cbs, type) == 0)
      return 0;

   return -1;
}
