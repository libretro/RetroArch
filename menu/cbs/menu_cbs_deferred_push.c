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
#include "../../retroarch.h"
#include "../../verbosity.h"

#ifndef BIND_ACTION_DEFERRED_PUSH
#define BIND_ACTION_DEFERRED_PUSH(cbs, name) \
   cbs->action_deferred_push = name; \
   cbs->action_deferred_push_ident = #name;
#endif

enum
{
   PUSH_ARCHIVE_OPEN_DETECT_CORE = 0,
   PUSH_ARCHIVE_OPEN,
   PUSH_DEFAULT,
   PUSH_DETECT_CORE_LIST
};

static int deferred_push_dlist(menu_displaylist_info_t *info, enum menu_displaylist_ctl_state state)
{
   if (!menu_displaylist_ctl(state, info))
      return menu_cbs_exit();
   menu_displaylist_process(info);
   return 0;
}

static int deferred_push_database_manager_list_deferred(
      menu_displaylist_info_t *info)
{
   if (!string_is_empty(info->path_b))
      free(info->path_b);
   if (!string_is_empty(info->path_c))
      free(info->path_c);

   info->path_b    = strdup(info->path);
   info->path_c    = NULL;

   return deferred_push_dlist(info, DISPLAYLIST_DATABASE_QUERY);
}

#define generic_deferred_push(name, type) \
static int (name)(menu_displaylist_info_t *info) \
{ \
   return deferred_push_dlist(info, type); \
}

static int deferred_push_remappings_port(menu_displaylist_info_t *info)
{
   return deferred_push_dlist(info, DISPLAYLIST_OPTIONS_REMAPPINGS_PORT);
}

generic_deferred_push(deferred_push_video_shader_preset_parameters, DISPLAYLIST_SHADER_PARAMETERS_PRESET)
generic_deferred_push(deferred_push_video_shader_parameters,        DISPLAYLIST_SHADER_PARAMETERS)
generic_deferred_push(deferred_push_video_shader_preset_save,       DISPLAYLIST_SHADER_PRESET_SAVE)
generic_deferred_push(deferred_push_video_shader_preset_remove,       DISPLAYLIST_SHADER_PRESET_REMOVE)
generic_deferred_push(deferred_push_settings,                       DISPLAYLIST_SETTINGS_ALL)
generic_deferred_push(deferred_push_shader_options,                 DISPLAYLIST_OPTIONS_SHADERS)
generic_deferred_push(deferred_push_quick_menu_override_options,    DISPLAYLIST_OPTIONS_OVERRIDES)
generic_deferred_push(deferred_push_options,                        DISPLAYLIST_OPTIONS)
generic_deferred_push(deferred_push_netplay,                        DISPLAYLIST_NETPLAY_ROOM_LIST)
generic_deferred_push(deferred_push_netplay_sublist,                DISPLAYLIST_NETPLAY)
generic_deferred_push(deferred_push_content_settings,               DISPLAYLIST_CONTENT_SETTINGS)
generic_deferred_push(deferred_push_add_content_list,               DISPLAYLIST_ADD_CONTENT_LIST)
generic_deferred_push(deferred_push_history_list,                   DISPLAYLIST_HISTORY)
generic_deferred_push(deferred_push_database_manager_list,          DISPLAYLIST_DATABASES)
generic_deferred_push(deferred_push_cursor_manager_list,            DISPLAYLIST_DATABASE_CURSORS)
generic_deferred_push(deferred_push_content_collection_list,        DISPLAYLIST_DATABASE_PLAYLISTS)
generic_deferred_push(deferred_push_configurations_list,            DISPLAYLIST_CONFIGURATIONS_LIST)
generic_deferred_push(deferred_push_load_content_special,           DISPLAYLIST_LOAD_CONTENT_LIST)
generic_deferred_push(deferred_push_load_content_list,              DISPLAYLIST_LOAD_CONTENT_LIST)
generic_deferred_push(deferred_push_dump_disk_list,                 DISPLAYLIST_DUMP_DISC)
generic_deferred_push(deferred_push_cdrom_info_detail_list,         DISPLAYLIST_CDROM_DETAIL_INFO)
generic_deferred_push(deferred_push_load_disk_list,                 DISPLAYLIST_LOAD_DISC)
generic_deferred_push(deferred_push_information_list,               DISPLAYLIST_INFORMATION_LIST)
generic_deferred_push(deferred_push_information,                    DISPLAYLIST_INFORMATION)
generic_deferred_push(deferred_archive_action_detect_core,          DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE)
generic_deferred_push(deferred_archive_action,                      DISPLAYLIST_ARCHIVE_ACTION)
generic_deferred_push(deferred_push_management_options,             DISPLAYLIST_OPTIONS_MANAGEMENT)
generic_deferred_push(deferred_push_core_counters,                  DISPLAYLIST_PERFCOUNTERS_CORE)
generic_deferred_push(deferred_push_frontend_counters,              DISPLAYLIST_PERFCOUNTERS_FRONTEND)
generic_deferred_push(deferred_push_core_cheat_options,             DISPLAYLIST_OPTIONS_CHEATS)
generic_deferred_push(deferred_push_core_input_remapping_options,   DISPLAYLIST_OPTIONS_REMAPPINGS)
generic_deferred_push(deferred_push_core_options,                   DISPLAYLIST_CORE_OPTIONS)
generic_deferred_push(deferred_push_disk_options,                   DISPLAYLIST_OPTIONS_DISK)
generic_deferred_push(deferred_push_browse_url_list,                DISPLAYLIST_BROWSE_URL_LIST)
generic_deferred_push(deferred_push_browse_url_start,               DISPLAYLIST_BROWSE_URL_START)
generic_deferred_push(deferred_push_core_list,                      DISPLAYLIST_CORES)
generic_deferred_push(deferred_push_configurations,                 DISPLAYLIST_CONFIG_FILES)
generic_deferred_push(deferred_push_video_shader_preset,            DISPLAYLIST_SHADER_PRESET)
generic_deferred_push(deferred_push_video_shader_pass,              DISPLAYLIST_SHADER_PASS)
generic_deferred_push(deferred_push_video_filter,                   DISPLAYLIST_VIDEO_FILTERS)
generic_deferred_push(deferred_push_images,                         DISPLAYLIST_IMAGES)
generic_deferred_push(deferred_push_audio_dsp_plugin,               DISPLAYLIST_AUDIO_FILTERS)
generic_deferred_push(deferred_push_cheat_file_load,                DISPLAYLIST_CHEAT_FILES)
generic_deferred_push(deferred_push_cheat_file_load_append,         DISPLAYLIST_CHEAT_FILES)
generic_deferred_push(deferred_push_remap_file_load,                DISPLAYLIST_REMAP_FILES)
generic_deferred_push(deferred_push_record_configfile,              DISPLAYLIST_RECORD_CONFIG_FILES)
generic_deferred_push(deferred_push_stream_configfile,              DISPLAYLIST_STREAM_CONFIG_FILES)
generic_deferred_push(deferred_push_input_overlay,                  DISPLAYLIST_OVERLAYS)
#ifdef HAVE_VIDEO_LAYOUT
generic_deferred_push(deferred_push_video_layout_path,              DISPLAYLIST_VIDEO_LAYOUT_PATH)
#endif
generic_deferred_push(deferred_push_video_font_path,                DISPLAYLIST_FONTS)
generic_deferred_push(deferred_push_xmb_font_path,                  DISPLAYLIST_FONTS)
generic_deferred_push(deferred_push_content_history_path,           DISPLAYLIST_CONTENT_HISTORY)
generic_deferred_push(deferred_push_core_information,               DISPLAYLIST_CORE_INFO)
generic_deferred_push(deferred_push_disc_information,               DISPLAYLIST_DISC_INFO)
generic_deferred_push(deferred_push_system_information,             DISPLAYLIST_SYSTEM_INFO)
generic_deferred_push(deferred_push_network_information,            DISPLAYLIST_NETWORK_INFO)
generic_deferred_push(deferred_push_achievement_list,               DISPLAYLIST_ACHIEVEMENT_LIST)
generic_deferred_push(deferred_push_rdb_collection,                 DISPLAYLIST_PLAYLIST_COLLECTION)
generic_deferred_push(deferred_main_menu_list,                      DISPLAYLIST_MAIN_MENU)
generic_deferred_push(deferred_music_list,                          DISPLAYLIST_MUSIC_LIST)
generic_deferred_push(deferred_user_binds_list,                     DISPLAYLIST_USER_BINDS_LIST)
generic_deferred_push(deferred_push_accounts_list,                  DISPLAYLIST_ACCOUNTS_LIST)
generic_deferred_push(deferred_push_driver_settings_list,           DISPLAYLIST_DRIVER_SETTINGS_LIST)
generic_deferred_push(deferred_push_core_settings_list,             DISPLAYLIST_CORE_SETTINGS_LIST)
generic_deferred_push(deferred_push_video_settings_list,            DISPLAYLIST_VIDEO_SETTINGS_LIST)
generic_deferred_push(deferred_push_video_fullscreen_mode_settings_list,    DISPLAYLIST_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST)
generic_deferred_push(deferred_push_video_windowed_mode_settings_list,    DISPLAYLIST_VIDEO_WINDOWED_MODE_SETTINGS_LIST)
generic_deferred_push(deferred_push_video_synchronization_settings_list,    DISPLAYLIST_VIDEO_SYNCHRONIZATION_SETTINGS_LIST)
generic_deferred_push(deferred_push_video_output_settings_list,    DISPLAYLIST_VIDEO_OUTPUT_SETTINGS_LIST)
generic_deferred_push(deferred_push_video_scaling_settings_list,    DISPLAYLIST_VIDEO_SCALING_SETTINGS_LIST)
generic_deferred_push(deferred_push_crt_switchres_settings_list,    DISPLAYLIST_CRT_SWITCHRES_SETTINGS_LIST)
generic_deferred_push(deferred_push_configuration_settings_list,    DISPLAYLIST_CONFIGURATION_SETTINGS_LIST)
generic_deferred_push(deferred_push_saving_settings_list,           DISPLAYLIST_SAVING_SETTINGS_LIST)
generic_deferred_push(deferred_push_mixer_stream_settings_list,     DISPLAYLIST_MIXER_STREAM_SETTINGS_LIST)
generic_deferred_push(deferred_push_logging_settings_list,          DISPLAYLIST_LOGGING_SETTINGS_LIST)
generic_deferred_push(deferred_push_frame_throttle_settings_list,   DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST)
generic_deferred_push(deferred_push_rewind_settings_list,           DISPLAYLIST_REWIND_SETTINGS_LIST)
generic_deferred_push(deferred_push_frame_time_counter_settings_list,           DISPLAYLIST_FRAME_TIME_COUNTER_SETTINGS_LIST)
generic_deferred_push(deferred_push_cheat_details_settings_list,    DISPLAYLIST_CHEAT_DETAILS_SETTINGS_LIST)
generic_deferred_push(deferred_push_cheat_search_settings_list,     DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST)
generic_deferred_push(deferred_push_onscreen_display_settings_list, DISPLAYLIST_ONSCREEN_DISPLAY_SETTINGS_LIST)
generic_deferred_push(deferred_push_onscreen_notifications_settings_list, DISPLAYLIST_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST)
generic_deferred_push(deferred_push_onscreen_overlay_settings_list, DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST)
#ifdef HAVE_VIDEO_LAYOUT
generic_deferred_push(deferred_push_onscreen_video_layout_settings_list, DISPLAYLIST_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST)
#endif
generic_deferred_push(deferred_push_menu_file_browser_settings_list,DISPLAYLIST_MENU_FILE_BROWSER_SETTINGS_LIST)
generic_deferred_push(deferred_push_menu_views_settings_list,       DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST)
generic_deferred_push(deferred_push_quick_menu_views_settings_list, DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST)
generic_deferred_push(deferred_push_settings_views_settings_list, DISPLAYLIST_SETTINGS_VIEWS_SETTINGS_LIST)
generic_deferred_push(deferred_push_menu_settings_list,             DISPLAYLIST_MENU_SETTINGS_LIST)
generic_deferred_push(deferred_push_user_interface_settings_list,   DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST)
generic_deferred_push(deferred_push_power_management_settings_list, DISPLAYLIST_POWER_MANAGEMENT_SETTINGS_LIST)
generic_deferred_push(deferred_push_retro_achievements_settings_list,DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST)
generic_deferred_push(deferred_push_updater_settings_list,          DISPLAYLIST_UPDATER_SETTINGS_LIST)
generic_deferred_push(deferred_push_wifi_settings_list,             DISPLAYLIST_WIFI_SETTINGS_LIST)
generic_deferred_push(deferred_push_network_settings_list,          DISPLAYLIST_NETWORK_SETTINGS_LIST)
generic_deferred_push(deferred_push_subsystem_settings_list,          DISPLAYLIST_SUBSYSTEM_SETTINGS_LIST)
generic_deferred_push(deferred_push_network_hosting_settings_list,          DISPLAYLIST_NETWORK_HOSTING_SETTINGS_LIST)
generic_deferred_push(deferred_push_lakka_services_list,            DISPLAYLIST_LAKKA_SERVICES_LIST)
generic_deferred_push(deferred_push_user_settings_list,             DISPLAYLIST_USER_SETTINGS_LIST)
generic_deferred_push(deferred_push_directory_settings_list,        DISPLAYLIST_DIRECTORY_SETTINGS_LIST)
generic_deferred_push(deferred_push_privacy_settings_list,          DISPLAYLIST_PRIVACY_SETTINGS_LIST)
generic_deferred_push(deferred_push_midi_settings_list,             DISPLAYLIST_MIDI_SETTINGS_LIST)
generic_deferred_push(deferred_push_audio_settings_list,            DISPLAYLIST_AUDIO_SETTINGS_LIST)
generic_deferred_push(deferred_push_audio_output_settings_list,            DISPLAYLIST_AUDIO_OUTPUT_SETTINGS_LIST)
generic_deferred_push(deferred_push_audio_resampler_settings_list,            DISPLAYLIST_AUDIO_RESAMPLER_SETTINGS_LIST)
generic_deferred_push(deferred_push_audio_synchronization_settings_list,            DISPLAYLIST_AUDIO_SYNCHRONIZATION_SETTINGS_LIST)
generic_deferred_push(deferred_push_audio_mixer_settings_list,      DISPLAYLIST_AUDIO_MIXER_SETTINGS_LIST)
generic_deferred_push(deferred_push_input_settings_list,            DISPLAYLIST_INPUT_SETTINGS_LIST)
generic_deferred_push(deferred_push_input_menu_settings_list,            DISPLAYLIST_INPUT_MENU_SETTINGS_LIST)
generic_deferred_push(deferred_push_input_haptic_feedback_settings_list,            DISPLAYLIST_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST)
generic_deferred_push(deferred_push_ai_service_settings_list,            DISPLAYLIST_AI_SERVICE_SETTINGS_LIST)
generic_deferred_push(deferred_push_accessibility_settings_list,            DISPLAYLIST_ACCESSIBILITY_SETTINGS_LIST)
generic_deferred_push(deferred_push_latency_settings_list,          DISPLAYLIST_LATENCY_SETTINGS_LIST)
generic_deferred_push(deferred_push_recording_settings_list,        DISPLAYLIST_RECORDING_SETTINGS_LIST)
generic_deferred_push(deferred_push_playlist_settings_list,         DISPLAYLIST_PLAYLIST_SETTINGS_LIST)
generic_deferred_push(deferred_push_playlist_manager_list,          DISPLAYLIST_PLAYLIST_MANAGER_LIST)
generic_deferred_push(deferred_push_playlist_manager_settings,      DISPLAYLIST_PLAYLIST_MANAGER_SETTINGS)
generic_deferred_push(deferred_push_input_hotkey_binds_list,        DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST)
generic_deferred_push(deferred_push_accounts_cheevos_list,          DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST)
generic_deferred_push(deferred_push_accounts_twitch_list,           DISPLAYLIST_ACCOUNTS_TWITCH_LIST)
generic_deferred_push(deferred_push_accounts_youtube_list,          DISPLAYLIST_ACCOUNTS_YOUTUBE_LIST)
generic_deferred_push(deferred_push_help,                           DISPLAYLIST_HELP_SCREEN_LIST)
generic_deferred_push(deferred_push_rdb_entry_detail,               DISPLAYLIST_DATABASE_ENTRY)
generic_deferred_push(deferred_push_rpl_entry_actions,              DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS)
generic_deferred_push(deferred_push_core_list_deferred,             DISPLAYLIST_CORES_SUPPORTED)
generic_deferred_push(deferred_push_core_collection_list_deferred,  DISPLAYLIST_CORES_COLLECTION_SUPPORTED)
generic_deferred_push(deferred_push_menu_sounds_list,               DISPLAYLIST_MENU_SOUNDS_LIST)
generic_deferred_push(deferred_push_rgui_theme_preset,              DISPLAYLIST_RGUI_THEME_PRESETS)

#ifdef HAVE_NETWORKING
generic_deferred_push(deferred_push_thumbnails_updater_list,        DISPLAYLIST_THUMBNAILS_UPDATER)
generic_deferred_push(deferred_push_pl_thumbnails_updater_list,     DISPLAYLIST_PL_THUMBNAILS_UPDATER)
generic_deferred_push(deferred_push_core_updater_list,              DISPLAYLIST_CORES_UPDATER)
generic_deferred_push(deferred_push_core_content_list,              DISPLAYLIST_CORE_CONTENT)
generic_deferred_push(deferred_push_core_content_dirs_list,         DISPLAYLIST_CORE_CONTENT_DIRS)
generic_deferred_push(deferred_push_core_content_dirs_subdir_list,  DISPLAYLIST_CORE_CONTENT_DIRS_SUBDIR)
generic_deferred_push(deferred_push_lakka_list,                     DISPLAYLIST_LAKKA)
#endif

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
generic_deferred_push(deferred_push_switch_cpu_profile,             DISPLAYLIST_SWITCH_CPU_PROFILE)
#endif

#ifdef HAVE_LAKKA_SWITCH
generic_deferred_push(deferred_push_switch_gpu_profile,             DISPLAYLIST_SWITCH_GPU_PROFILE)
generic_deferred_push(deferred_push_switch_backlight_control,       DISPLAYLIST_SWITCH_BACKLIGHT_CONTROL)
#endif

generic_deferred_push(deferred_push_manual_content_scan_list,       DISPLAYLIST_MANUAL_CONTENT_SCAN_LIST)
generic_deferred_push(deferred_push_manual_content_scan_dat_file,   DISPLAYLIST_MANUAL_CONTENT_SCAN_DAT_FILES)

generic_deferred_push(deferred_push_file_browser_select_sideload_core, DISPLAYLIST_FILE_BROWSER_SELECT_SIDELOAD_CORE)

static int deferred_push_cursor_manager_list_deferred(
      menu_displaylist_info_t *info)
{
   char rdb_path[PATH_MAX_LENGTH];
   int ret                        = -1;
   char *query                    = NULL;
   char *rdb                      = NULL;
   const char *path               = info->path;
   config_file_t *conf            = NULL;
   
   if (!(conf = config_file_new_from_path_to_string(path)))
      return -1;

   if (!config_get_string(conf, "query", &query))
      goto end;

   if (!config_get_string(conf, "rdb", &rdb))
      goto end;

   rdb_path[0] = '\0';

   {
      settings_t *settings           = config_get_ptr();
      if (settings)
         fill_pathname_join(rdb_path,
               settings->paths.path_content_database,
               rdb, sizeof(rdb_path));
   }

   if (!string_is_empty(info->path_b))
      free(info->path_b);

   if (!string_is_empty(info->path_c))
      free(info->path_c);

   info->path_b    = strdup(info->path);

   if (!string_is_empty(info->path))
      free(info->path);

   info->path_c    = strdup(query);
   info->path      = strdup(rdb_path);

   ret             = deferred_push_dlist(info, DISPLAYLIST_DATABASE_QUERY);

end:
   config_file_free(conf);
   free(rdb);
   free(query);
   return ret;
}

#ifdef HAVE_LIBRETRODB
static int deferred_push_cursor_manager_list_generic(
      menu_displaylist_info_t *info, enum database_query_type type)
{
   char query[PATH_MAX_LENGTH];
   int ret                       = -1;
   const char *path              = info->path;
   struct string_list *str_list  = path ? string_split(path, "|") : NULL;

   if (!str_list)
      goto end;

   query[0] = '\0';

   database_info_build_query_enum(query, sizeof(query), type, str_list->elems[0].data);

   if (string_is_empty(query))
      goto end;

   if (!string_is_empty(info->path_b))
      free(info->path_b);
   if (!string_is_empty(info->path_c))
      free(info->path_c);
   if (!string_is_empty(info->path))
      free(info->path);

   info->path   = strdup(str_list->elems[1].data);
   info->path_b = strdup(str_list->elems[0].data);
   info->path_c = strdup(query);

   ret = deferred_push_dlist(info, DISPLAYLIST_DATABASE_QUERY);

end:
   string_list_free(str_list);
   return ret;
}

#define generic_deferred_cursor_manager(name, type) \
static int (name)(menu_displaylist_info_t *info) \
{ \
   return deferred_push_cursor_manager_list_generic(info, type); \
}

generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_max_users, DATABASE_QUERY_ENTRY_MAX_USERS)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_famitsu_magazine_rating, DATABASE_QUERY_ENTRY_FAMITSU_MAGAZINE_RATING)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_rating, DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_RATING)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_issue, DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_ISSUE)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_elspa_rating, DATABASE_QUERY_ENTRY_ELSPA_RATING)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_cero_rating, DATABASE_QUERY_ENTRY_CERO_RATING)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_pegi_rating, DATABASE_QUERY_ENTRY_PEGI_RATING)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_bbfc_rating, DATABASE_QUERY_ENTRY_BBFC_RATING)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_esrb_rating, DATABASE_QUERY_ENTRY_ESRB_RATING)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_enhancement_hw, DATABASE_QUERY_ENTRY_ENHANCEMENT_HW)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_franchise, DATABASE_QUERY_ENTRY_FRANCHISE)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_publisher, DATABASE_QUERY_ENTRY_PUBLISHER)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_developer, DATABASE_QUERY_ENTRY_DEVELOPER)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_origin, DATABASE_QUERY_ENTRY_ORIGIN)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_releasemonth, DATABASE_QUERY_ENTRY_RELEASEDATE_MONTH)
generic_deferred_cursor_manager(deferred_push_cursor_manager_list_deferred_query_rdb_entry_releaseyear, DATABASE_QUERY_ENTRY_RELEASEDATE_YEAR)

#endif

#if 0
static int deferred_push_cursor_manager_list_deferred_query_subsearch(
      menu_displaylist_info_t *info)
{
   int ret                       = -1;
#ifdef HAVE_LIBRETRODB
   char query[PATH_MAX_LENGTH];
   struct string_list *str_list  = string_split(info->path, "|");

   query[0] = '\0';

   database_info_build_query(query, sizeof(query),
         info->label, str_list->elems[0].data);

   if (string_is_empty(query))
      goto end;

   if (!string_is_empty(info->path))
      free(info->path);
   if (!string_is_empty(info->path_b))
      free(info->path_b);
   if (!string_is_empty(info->path_c))
      free(info->path_c);
   info->path   = strdup(str_list->elems[1].data);
   info->path_b = strdup(str_list->elems[0].data);
   info->path_c = strdup(query);

   ret = deferred_push_dlist(info, DISPLAYLIST_DATABASE_QUERY);

end:
   string_list_free(str_list);
#endif
   return ret;
}
#endif

static int general_push(menu_displaylist_info_t *info,
      unsigned id, enum menu_displaylist_ctl_state state)
{
   char                      *newstring2 = NULL;
   core_info_list_t           *list      = NULL;
   settings_t                  *settings = config_get_ptr();
   menu_handle_t                  *menu  = menu_driver_get_ptr();

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

            tmp_str[0] = '\0';
            tmp_str2[0] = '\0';

            fill_pathname_join(tmp_str, menu->scratch2_buf,
                  menu->scratch_buf, sizeof(tmp_str));
            fill_pathname_join(tmp_str2, menu->scratch2_buf,
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

   newstring2                     = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

   newstring2[0]                  = '\0';

   switch (id)
   {
      case PUSH_ARCHIVE_OPEN:
         {
            struct retro_system_info *system = 
               runloop_get_libretro_system_info();
            if (system)
            {
               if (!string_is_empty(system->valid_extensions))
                  strlcpy(newstring2, system->valid_extensions,
                        PATH_MAX_LENGTH * sizeof(char));
            }
         }
         break;
      case PUSH_DEFAULT:
         {
            bool new_exts_allocated               = false;
            char *new_exts                        = NULL;

            if (menu_setting_get_browser_selection_type(info->setting) == ST_DIR)
            {
            }
            else
            {
               struct retro_system_info *system = 
                  runloop_get_libretro_system_info();
               if (system && !string_is_empty(system->valid_extensions))
               {
                  new_exts           = strdup(system->valid_extensions);
                  new_exts_allocated = true;
               }
            }

            if (!new_exts)
               new_exts = info->exts;

            if (!string_is_empty(new_exts))
            {
               size_t path_size               = PATH_MAX_LENGTH * sizeof(char);
               struct string_list *str_list3  = string_split(new_exts, "|");

#ifdef HAVE_IBXM
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  string_list_append(str_list3, "s3m", attr);
                  string_list_append(str_list3, "mod", attr);
                  string_list_append(str_list3, "xm", attr);
               }
#endif
               string_list_join_concat(newstring2, path_size,
                     str_list3, "|");
               string_list_free(str_list3);

            }

            if (new_exts_allocated)
            {
               free(new_exts);

               if (new_exts == info->exts)
                  info->exts = NULL;
            }
         }
         break;
      case PUSH_ARCHIVE_OPEN_DETECT_CORE:
      case PUSH_DETECT_CORE_LIST:
         {
            union string_list_elem_attr attr;
            size_t path_size                 = PATH_MAX_LENGTH * sizeof(char);
            char *newstring                  = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            struct string_list *str_list2    = string_list_new();
            struct retro_system_info *system = runloop_get_libretro_system_info();

            newstring[0]                     = '\0';
            attr.i                           = 0;

            if (system)
            {
               if (!string_is_empty(system->valid_extensions))
               {
                  unsigned x;
                  struct string_list *str_list    = string_split(system->valid_extensions, "|");

                  for (x = 0; x < str_list->size; x++)
                  {
                     const char *elem = str_list->elems[x].data;
                     string_list_append(str_list2, elem, attr);
                  }

                  string_list_free(str_list);
               }
            }

            if (!settings->bools.filter_by_current_core)
            {
               if (list && !string_is_empty(list->all_ext))
               {
                  unsigned x;
                  struct string_list *str_list    = string_split(list->all_ext, "|");

                  for (x = 0; x < str_list->size; x++)
                  {
                     if (!string_list_find_elem(str_list2, str_list->elems[x].data))
                     {
                        const char *elem = str_list->elems[x].data;
                        string_list_append(str_list2, elem, attr);
                     }
                  }

                  string_list_free(str_list);
               }
            }

            string_list_join_concat(newstring, path_size,
                  str_list2, "|");

            {
               struct string_list *str_list3  = string_split(newstring, "|");

#ifdef HAVE_IBXM
               {
                  union string_list_elem_attr attr;
                  attr.i = 0;
                  string_list_append(str_list3, "s3m", attr);
                  string_list_append(str_list3, "mod", attr);
                  string_list_append(str_list3, "xm", attr);
               }
#endif
               string_list_join_concat(newstring2, path_size,
                     str_list3, "|");
               string_list_free(str_list3);
            }
            free(newstring);
            string_list_free(str_list2);
         }
         break;
   }

   if (settings->bools.multimedia_builtin_mediaplayer_enable ||
         settings->bools.multimedia_builtin_imageviewer_enable)
   {
      struct retro_system_info sysinfo = {0};

      (void)sysinfo;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      if (settings->bools.multimedia_builtin_mediaplayer_enable)
      {
#if defined(HAVE_FFMPEG)
         libretro_ffmpeg_retro_get_system_info(&sysinfo);
#elif defined(HAVE_MPV)
         libretro_mpv_retro_get_system_info(&sysinfo);
#endif
         strlcat(newstring2, "|", PATH_MAX_LENGTH * sizeof(char));
         strlcat(newstring2, sysinfo.valid_extensions,
               PATH_MAX_LENGTH * sizeof(char));
      }
#endif
#ifdef HAVE_IMAGEVIEWER
      if (settings->bools.multimedia_builtin_imageviewer_enable)
      {
         libretro_imageviewer_retro_get_system_info(&sysinfo);
         strlcat(newstring2, "|", PATH_MAX_LENGTH * sizeof(char));
         strlcat(newstring2, sysinfo.valid_extensions,
               PATH_MAX_LENGTH * sizeof(char));
      }
#endif
   }

   if (!string_is_empty(newstring2))
   {
      if (!string_is_empty(info->exts))
         free(info->exts);
      info->exts = strdup(newstring2);
   }
   free(newstring2);

   return deferred_push_dlist(info, state);
}

#define generic_deferred_push_general(name, a, b) \
static int (name)(menu_displaylist_info_t *info) \
{ \
   return general_push(info, a, b); \
}

#define generic_deferred_push_clear_general(name, a, b) \
static int (name)(menu_displaylist_info_t *info) \
{ \
   menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list); \
   return general_push(info, a, b); \
}

generic_deferred_push_general(deferred_push_detect_core_list, PUSH_DETECT_CORE_LIST, DISPLAYLIST_CORES_DETECTED)
generic_deferred_push_general(deferred_archive_open_detect_core, PUSH_ARCHIVE_OPEN_DETECT_CORE, DISPLAYLIST_DEFAULT)
generic_deferred_push_general(deferred_archive_open, PUSH_ARCHIVE_OPEN, DISPLAYLIST_DEFAULT)
generic_deferred_push_general(deferred_push_default, PUSH_DEFAULT, DISPLAYLIST_DEFAULT)
generic_deferred_push_general(deferred_push_favorites_list, PUSH_DEFAULT, DISPLAYLIST_FAVORITES)

generic_deferred_push_clear_general(deferred_playlist_list, PUSH_DEFAULT, DISPLAYLIST_PLAYLIST)
generic_deferred_push_clear_general(deferred_music_history_list, PUSH_DEFAULT, DISPLAYLIST_MUSIC_HISTORY)
generic_deferred_push_clear_general(deferred_image_history_list, PUSH_DEFAULT, DISPLAYLIST_IMAGES_HISTORY)
generic_deferred_push_clear_general(deferred_video_history_list, PUSH_DEFAULT, DISPLAYLIST_VIDEO_HISTORY)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_special, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_SPECIAL)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_resolution, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_RESOLUTION)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_video_shader_num_passes, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_NUM_PASSES)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_shader_parameter, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_PARAMETER)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_shader_preset_parameter, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_PRESET_PARAMETER)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_playlist_default_core, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_DEFAULT_CORE)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_playlist_label_display_mode, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LABEL_DISPLAY_MODE)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_playlist_right_thumbnail_mode, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_playlist_left_thumbnail_mode, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_manual_content_scan_system_name, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_manual_content_scan_core_name, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_CORE_NAME)
generic_deferred_push_clear_general(deferred_push_dropdown_box_list_disk_index, PUSH_DEFAULT, DISPLAYLIST_DROPDOWN_LIST_DISK_INDEX)

static int menu_cbs_init_bind_deferred_push_compare_label(
      menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash)
{
   unsigned i;
   typedef struct deferred_info_list 
   {
      enum msg_hash_enums type;
      int (*cb)(menu_displaylist_info_t *info);
   } deferred_info_list_t;

   deferred_info_list_t info_list[] = {
      {MENU_ENUM_LABEL_DEFERRED_DUMP_DISC_LIST, deferred_push_dump_disk_list},
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
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX, deferred_push_dropdown_box_list_disk_index},
      {MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_LIST, deferred_push_browse_url_list},
      {MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_START, deferred_push_browse_url_start},
      {MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST, deferred_push_core_settings_list},
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
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST, deferred_push_onscreen_overlay_settings_list},
#ifdef HAVE_VIDEO_LAYOUT
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST, deferred_push_onscreen_video_layout_settings_list},
#endif
      {MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST, deferred_push_menu_file_browser_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_VIEWS_SETTINGS_LIST, deferred_push_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SETTINGS_VIEWS_SETTINGS_LIST, deferred_push_settings_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_VIEWS_SETTINGS_LIST, deferred_push_quick_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST, deferred_push_menu_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST, deferred_push_user_interface_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_POWER_MANAGEMENT_SETTINGS_LIST, deferred_push_power_management_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_SOUNDS_LIST, deferred_push_menu_sounds_list},
      {MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST, deferred_push_retro_achievements_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST, deferred_push_updater_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST, deferred_push_network_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SUBSYSTEM_SETTINGS_LIST, deferred_push_subsystem_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETWORK_HOSTING_SETTINGS_LIST, deferred_push_network_hosting_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST, deferred_push_wifi_settings_list},
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
#endif
      {MENU_ENUM_LABEL_DEFERRED_MUSIC, deferred_music_list},
      {MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST, deferred_music_history_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST, deferred_playlist_list},
      {MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST, deferred_image_history_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST, deferred_video_history_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST, deferred_push_input_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_MENU_SETTINGS_LIST, deferred_push_input_menu_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST, deferred_push_input_haptic_feedback_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AI_SERVICE_SETTINGS_LIST, deferred_push_ai_service_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCESSIBILITY_SETTINGS_LIST, deferred_push_accessibility_settings_list},
      {MENU_ENUM_LABEL_CORE_INFORMATION, deferred_push_core_information},
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
      {MENU_ENUM_LABEL_DEFERRED_CRT_SWITCHRES_SETTINGS_LIST, deferred_push_crt_switchres_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST, deferred_push_audio_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SYNCHRONIZATION_SETTINGS_LIST, deferred_push_audio_synchronization_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_OUTPUT_SETTINGS_LIST, deferred_push_audio_output_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_RESAMPLER_SETTINGS_LIST, deferred_push_audio_resampler_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_MIXER_SETTINGS_LIST, deferred_push_audio_mixer_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_LATENCY_SETTINGS_LIST, deferred_push_latency_settings_list},
#ifdef HAVE_LAKKA_SWITCH
      {MENU_ENUM_LABEL_SWITCH_GPU_PROFILE, deferred_push_switch_gpu_profile},
      {MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL, deferred_push_switch_backlight_control},
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      {MENU_ENUM_LABEL_SWITCH_CPU_PROFILE, deferred_push_switch_cpu_profile},
#endif
      {MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST, deferred_push_remappings_port },
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST, deferred_push_accounts_list},
      {MENU_ENUM_LABEL_CORE_LIST, deferred_push_core_list},
      {MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY, deferred_push_history_list},
      {MENU_ENUM_LABEL_CORE_OPTIONS, deferred_push_core_options},
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
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_SAVE_LIST, deferred_push_video_shader_preset_save},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_REMOVE_LIST, deferred_push_video_shader_preset_remove},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME, deferred_push_dropdown_box_list_manual_content_scan_system_name},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME, deferred_push_dropdown_box_list_manual_content_scan_core_name},
      {MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST, deferred_push_recording_settings_list},
   };

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
   if (strstr(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL)))
   {
      BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_rdb_entry_detail);
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
         case MENU_ENUM_LABEL_CORE_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_options);
            break;
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_cheat_options);
            break;
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_input_remapping_options);
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
         case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_onscreen_overlay_settings_list);
            break;
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
         case MENU_ENUM_LABEL_DEFERRED_DUMP_DISC_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_dump_disk_list);
            break;
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
         case MENU_ENUM_LABEL_SIDELOAD_CORE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_file_browser_select_sideload_core);
            break;
         default:
            return -1;
      }
   }
   else
   {
      switch (label_hash)
      {
         case MENU_LABEL_SETTINGS: /* TODO/FIXME */
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_settings);
            break;
         case MENU_LABEL_DEFERRED_CONFIGURATIONS_LIST: /* TODO/FIXME */
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_configurations_list);
            break;
         case MENU_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_playlist_settings_list);
            break;
         case MENU_LABEL_DEFERRED_PLAYLIST_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_playlist_manager_list);
            break;
         case MENU_LABEL_DEFERRED_PLAYLIST_MANAGER_SETTINGS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_playlist_manager_settings);
            break;
         case MENU_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_accounts_cheevos_list);
            break;
         case MENU_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_action_detect_core);
            break;
         case MENU_LABEL_DEFERRED_ARCHIVE_ACTION:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_action);
            break;
         case MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_open_detect_core);
            break;
         case MENU_LABEL_DEFERRED_ARCHIVE_OPEN:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_archive_open);
            break;
         case MENU_LABEL_DEFERRED_LAKKA_LIST:
#ifdef HAVE_NETWORKING
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_lakka_list);
#endif
            break;
         case MENU_LABEL_DATABASE_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_database_manager_list);
            break;
         case MENU_LABEL_CURSOR_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list);
            break;
         case MENU_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cheat_file_load);
            break;
         case MENU_LABEL_CHEAT_FILE_LOAD_APPEND:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cheat_file_load_append);
            break;
         case MENU_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_remap_file_load);
            break;
         case MENU_LABEL_RECORD_CONFIG:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_record_configfile);
            break;
         case MENU_LABEL_STREAM_CONFIG:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_stream_configfile);
            break;
         case MENU_LABEL_RGUI_MENU_THEME_PRESET:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_rgui_theme_preset);
            break;
         case MENU_LABEL_NETPLAY:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_netplay);
            break;
         case MENU_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_content_settings);
            break;
         case MENU_LABEL_ADD_CONTENT_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_add_content_list);
            break;
         case MENU_LABEL_LOAD_CONTENT_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_load_content_list);
            break;
         case MENU_LABEL_MANAGEMENT:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_management_options);
            break;
         case MENU_LABEL_DEFERRED_CORE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_list_deferred);
            break;
         case MENU_LABEL_DEFERRED_CORE_LIST_SET:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_collection_list_deferred);
            break;
         case MENU_LABEL_DEFERRED_VIDEO_FILTER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_filter);
            break;
         case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_database_manager_list_deferred);
            break;
#ifdef HAVE_LIBRETRODB
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_publisher);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_developer);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_origin);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_franchise);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ENHANCEMENT_HW:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_enhancement_hw);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_esrb_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_bbfc_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_elspa_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_pegi_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_cero_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_edge_magazine_issue);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_famitsu_magazine_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_max_users);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_releasemonth);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_cursor_manager_list_deferred_query_rdb_entry_releaseyear);
            break;
#endif
         case MENU_LABEL_ACHIEVEMENT_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_achievement_list);
            break;
         case MENU_LABEL_CORE_COUNTERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_counters);
            break;
         case MENU_LABEL_FRONTEND_COUNTERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_frontend_counters);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_preset_parameters);
            break;
         case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_parameters);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_preset_save);
            break;
         case MENU_LABEL_CORE_CHEAT_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_cheat_options);
            break;
         case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_core_input_remapping_options);
            break;
         case MENU_LABEL_PLAYLISTS_TAB:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_content_collection_list);
            break;
         case MENU_LABEL_CONFIGURATIONS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_configurations);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_preset);
            break;
         case MENU_LABEL_VIDEO_SHADER_PASS:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_shader_pass);
            break;
         case MENU_LABEL_VIDEO_FILTER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_filter);
            break;
         case MENU_LABEL_MENU_WALLPAPER:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_images);
            break;
         case MENU_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_audio_dsp_plugin);
            break;
         case MENU_LABEL_INPUT_OVERLAY:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_input_overlay);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_LABEL_VIDEO_LAYOUT_PATH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_layout_path);
            break;
#endif
         case MENU_LABEL_VIDEO_FONT_PATH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_video_font_path);
            break;
         case MENU_LABEL_XMB_FONT:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_xmb_font_path);
            break;
         case MENU_LABEL_CONTENT_HISTORY_PATH:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_content_history_path);
            break;
         case MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         case MENU_LABEL_FAVORITES:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_detect_core_list);
            break;
         case MENU_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_manual_content_scan_list);
            break;
         case MENU_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_manual_content_scan_dat_file);
            break;
         case MENU_LABEL_SIDELOAD_CORE_LIST:
            BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_file_browser_select_sideload_core);
            break;
         default:
            return -1;
      }
   }

   return 0;
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
      const char *path, const char *label, unsigned type, size_t idx,
      uint32_t label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_DEFERRED_PUSH(cbs, deferred_push_default);

   if (cbs->enum_idx != MENU_ENUM_LABEL_PLAYLIST_ENTRY &&
       menu_cbs_init_bind_deferred_push_compare_label(cbs, label, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_deferred_push_compare_type(
            cbs, type) == 0)
      return 0;

   return -1;
}
