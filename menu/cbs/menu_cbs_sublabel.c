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

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../../input/input_remapping.h"

#include "../../retroarch.h"
#include "../../core_option_manager.h"

#ifdef HAVE_CHEEVOS
#include "../../cheevos/cheevos_menu.h"
#endif
#include "../../audio/audio_driver.h"
#include "../../core_info.h"
#include "../../verbosity.h"
#ifdef HAVE_BLUETOOTH
#include "../../bluetooth/bluetooth_driver.h"
#endif
#include "../../misc/cpufreq/cpufreq.h"

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#endif

#include "../../retroarch.h"
#include "../../content.h"
#include "../../dynamic.h"
#include "../../configuration.h"
#ifdef HAVE_NETWORKING
#include "../../core_updater_list.h"
#endif
#ifdef HAVE_CHEATS
#include "../../cheat_manager.h"
#endif
#include "../../tasks/tasks_internal.h"

#include "../../msg_hash_lbl_str.h"
#include "../../playlist.h"
#include "../../runtime_file.h"

#ifndef BIND_ACTION_SUBLABEL
#define BIND_ACTION_SUBLABEL(cbs, name) (cbs)->action_sublabel = (name)
#endif

#define DEFAULT_SUBLABEL_MACRO(func_name, lbl) \
  static int (func_name)(file_list_t *list, unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len) \
{ \
   strlcpy(s, msg_hash_to_str(lbl), len); \
   return 1; \
}

static int menu_action_sublabel_file_browser_core(file_list_t *list, unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len)
{
   core_info_t *core_info = NULL;
   size_t _len =
      strlcpy(s,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES), len);
   s[  _len]   = ':';
   s[++_len]   = ' ';
   s[++_len]   = '\0';

   /* Search for specified core */
   if (
         core_info_find(path, &core_info)
      && core_info->licenses_list)
   {
      unsigned i;
      /* Add license text */
      for (i = 0; i < core_info->licenses_list->size; i++)
      {
         _len += strlcpy(s + _len, core_info->licenses_list->elems[i].data, len - _len);
         if ((i + 1) < core_info->licenses_list->size)
            _len += strlcpy(s + _len, ", ", len - _len);
      }
   }
   else /* No license found - set to N/A */
      strlcpy(s + _len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len - _len);

   return 1;
}

static int menu_action_sublabel_contentless_core(file_list_t *list,
      unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len)
{
   char tmp[64];
   const char *core_path                      = path;
   core_info_t *core_info                     = NULL;
   const contentless_core_info_entry_t *entry = NULL;
   bool display_runtime                       = true;
   settings_t *settings                       = config_get_ptr();
   bool playlist_show_sublabels               = settings->bools.playlist_show_sublabels;
   unsigned playlist_sublabel_runtime_type    = settings->uints.playlist_sublabel_runtime_type;
   bool content_runtime_log                   = settings->bools.content_runtime_log;
   bool content_runtime_log_aggregate         = settings->bools.content_runtime_log_aggregate;
   const char *directory_runtime_log          = settings->paths.directory_runtime_log;
   const char *directory_playlist             = settings->paths.directory_playlist;
   enum playlist_sublabel_last_played_style_type
         playlist_sublabel_last_played_style  =
               (enum playlist_sublabel_last_played_style_type)
                     settings->uints.playlist_sublabel_last_played_style;
   enum playlist_sublabel_last_played_date_separator_type
         menu_timedate_date_separator         =
               (enum playlist_sublabel_last_played_date_separator_type)
                     settings->uints.menu_timedate_date_separator;
#if defined(HAVE_OZONE) || defined(HAVE_MATERIALUI)
   const char *menu_ident                     = menu_driver_ident();
#endif
   if (playlist_show_sublabels)
   {
      /* Search for specified core */
      if (     !core_info_find(core_path, &core_info)
            || !(core_info->flags & CORE_INFO_FLAG_SUPPORTS_NO_GAME))
         return 1;
      /* Get corresponding contentless core info entry */
      menu_contentless_cores_get_info(core_info->core_file_id.str,
            &entry);
      if (!entry)
         return 1;
      /* Determine which info we need to display */
      /* > Runtime info is always omitted when using Ozone
       * > Check if required runtime log is enabled */
      if (   ((playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_PER_CORE)
               && !content_runtime_log)
            || ((playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_AGGREGATE)
               && !content_runtime_log_aggregate)
#ifdef HAVE_OZONE
            || string_is_equal(menu_ident, "ozone")
#endif
         )
         display_runtime = false;
#ifdef HAVE_MATERIALUI
      /* > License info is always displayed unless
       *   we are using GLUI with runtime info enabled */
      if (display_runtime && string_is_equal(menu_ident, "glui"))
         tmp[0  ] = '\0';
      else
#endif
      {
         /* Display licenses */
         strlcpy(s, entry->licenses_str, len);
         tmp[0  ] = '\n';
         tmp[1  ] = '\0';
      }
      if (display_runtime)
      {
         /* Check whether runtime info should be loaded
          * from log file */
         if (entry->runtime.status == CONTENTLESS_CORE_RUNTIME_UNKNOWN)
            runtime_update_contentless_core(
                  core_path,
                  directory_runtime_log,
                  directory_playlist,
                  (playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_PER_CORE),
                  playlist_sublabel_last_played_style,
                  menu_timedate_date_separator);
         /* Check whether runtime info is valid */
         if (entry->runtime.status == CONTENTLESS_CORE_RUNTIME_VALID)
         {
            size_t _len = strlcpy(tmp, entry->runtime.runtime_str, sizeof(tmp));
            if (_len < 64 - 1)
            {
               tmp[_len    ] = '\n';
               tmp[_len + 1] = '\0';
               _len = strlcpy(tmp + _len + 1, entry->runtime.last_played_str, sizeof(tmp) - _len - 1);
            }
            if (*tmp)
            {
               size_t slen = strlen(s);
               strlcpy(s + slen, tmp, len - slen);
            }
         }
      }
   }
   return 0;
}

#ifdef HAVE_CHEEVOS
static int menu_action_sublabel_achievement_pause_menu(file_list_t* list,
      unsigned type, unsigned i, const char* label, const char* path, char* s, size_t len)
{
   if (string_is_equal(path, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE)))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME), len);
   return 1;
}
#endif

#ifdef HAVE_AUDIOMIXER
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_add_to_mixer_and_play,
      MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY)
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_add_to_mixer,
      MENU_ENUM_SUBLABEL_ADD_TO_MIXER)
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_stream_play,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY)
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_stream_play_looped,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED)
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_stream_play_sequential,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL)
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_stream_stop,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP)
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_stream_remove,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE)
DEFAULT_SUBLABEL_MACRO(menu_action_sublabel_setting_audio_mixer_stream_volume,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_accessibility_enabled, MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_accessibility_narrator_speech_speed, MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_load_config,                      MENU_ENUM_SUBLABEL_CONFIGURATIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_current_config,              MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_new_config,                  MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_as_config,                   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_main_config,                 MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_reset_to_default_config,          MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_override_options,             MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_start_streaming,             MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_start_recording,             MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_stop_streaming,             MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_stop_recording,             MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres,             MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_hdr_enable,      MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_hdr_paper_white_nits,      MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_hdr_menu_nits,      MENU_ENUM_SUBLABEL_MENU_HDR_BRIGHTNESS_NITS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_hdr_expand_gamut,      MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_hdr_scanlines,      MENU_ENUM_SUBLABEL_VIDEO_HDR_SCANLINES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_hdr_subpixel_layout,      MENU_ENUM_SUBLABEL_VIDEO_HDR_SUBPIXEL_LAYOUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres_super,       MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres_x_axis_centering,       MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres_porch_adjust,       MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres_vertical_adjust,       MENU_ENUM_SUBLABEL_CRT_SWITCH_VERTICAL_ADJUST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres_use_custom_refresh_rate,       MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres_hires_menu,       MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_driver_settings_list,             MENU_ENUM_SUBLABEL_DRIVER_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_retro_achievements_settings_list, MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_saving_settings_list,          MENU_ENUM_SUBLABEL_SAVING_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_settings_list,      MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_enable,             MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_sync_mode,          MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_destructive,        MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_sync_saves,         MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_sync_configs,       MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_sync_thumbs,        MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_sync_system,        MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_driver,             MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_url,                MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_s3_url,             MENU_ENUM_SUBLABEL_CLOUD_SYNC_S3_URL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_username,           MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_password,           MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_access_key_id,      MENU_ENUM_SUBLABEL_CLOUD_SYNC_ACCESS_KEY_ID)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cloud_sync_secret_access_key,  MENU_ENUM_SUBLABEL_CLOUD_SYNC_SECRET_ACCESS_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_logging_settings_list,         MENU_ENUM_SUBLABEL_LOGGING_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_user_interface_settings_list,  MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_settings_list,  MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_accessibility_settings_list,  MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_mode,  MENU_ENUM_SUBLABEL_AI_SERVICE_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_backend,  MENU_ENUM_SUBLABEL_AI_SERVICE_BACKEND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_pause,  MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_target_lang,  MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_source_lang,  MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_url,  MENU_ENUM_SUBLABEL_AI_SERVICE_URL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ai_service_enable,  MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_power_management_settings_list,  MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_privacy_settings_list,         MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_midi_settings_list,            MENU_ENUM_SUBLABEL_MIDI_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_directory_settings_list,       MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_settings_list,        MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_manager_list,         MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_manager_default_core, MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_manager_reset_cores,  MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_manager_label_display_mode, MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_manager_sort_mode, MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_manager_clean_playlist, MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_manager_refresh_playlist, MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_delete_playlist,               MENU_ENUM_SUBLABEL_DELETE_PLAYLIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_network_settings_list,         MENU_ENUM_SUBLABEL_NETWORK_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_network_on_demand_thumbnails,  MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_user_settings_list,            MENU_ENUM_SUBLABEL_USER_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_recording_settings_list,       MENU_ENUM_SUBLABEL_RECORDING_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_frame_throttle_settings_list,  MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_frame_time_counter_settings_list,  MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_frame_time_counter_auto_reset,                MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_AUTO_RESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_onscreen_display_settings_list,MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_settings_list,            MENU_ENUM_SUBLABEL_CORE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_information_list_list,         MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST)
#if defined(HAVE_CHEEVOS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_achievement_list,              MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_achievement_pause_cancel,      MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_achievement_resume_cancel,     MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_achievement_resume_requires_reload,  MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_achievement_server_unreachable,MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_enable,                MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_test_unofficial,       MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_hardcore_mode_enable,  MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_challenge_indicators,  MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_richpresence_enable,   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_badges_enable,         MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE)
#if defined(HAVE_AUDIOMIXER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_unlock_sound_enable,   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_auto_screenshot,       MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_start_active,          MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_verbose_enable,        MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_appearance_settings,   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_appearance_anchor,     MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_appearance_padding_auto, MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_appearance_padding_h,  MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_appearance_padding_v,  MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_settings,   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_summary,    MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_unlock,     MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_mastery,    MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_account,    MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_lboard_start, MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_lboard_submit, MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_lboard_cancel, MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_lboard_trackers, MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_visibility_progress_tracker, MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER)

#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_views_settings_list,      MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_views_settings_list, MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_views_settings_list, MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_settings_list,            MENU_ENUM_SUBLABEL_MENU_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_appicon_settings_list,         MENU_ENUM_SUBLABEL_APPICON_SETTINGS)
#ifdef _3DS
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_bottom_settings_list,     MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_synchronization_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_fullscreen_mode_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_windowed_mode_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_scaling_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_hdr_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_output_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_crt_switchres_settings_list,           MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_suspend_screensaver_enable,    MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE)
#ifdef HAVE_VIDEO_FILTER
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_filter_enable,           MENU_ENUM_SUBLABEL_VIDEO_FILTER_ENABLE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_scale,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_opacity,          MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_show_decorations, MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_show_menubar,     MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_settings_list,           MENU_ENUM_SUBLABEL_AUDIO_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_output_settings_list,           MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS)
#ifdef HAVE_MICROPHONE
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_settings_list,           MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_synchronization_settings_list,           MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS)
#ifdef HAVE_AUDIOMIXER
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_mixer_settings_list,           MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_settings_list,           MENU_ENUM_SUBLABEL_INPUT_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_menu_settings_list,           MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_fire_settings_list,     MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_haptic_feedback_settings_list,MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_sensor_settings_list,    MENU_ENUM_SUBLABEL_INPUT_SENSOR_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_latency_settings_list,         MENU_ENUM_SUBLABEL_LATENCY_SETTINGS)
#ifdef HAVE_BLUETOOTH
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bluetooth_settings_list,       MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_wifi_settings_list,            MENU_ENUM_SUBLABEL_WIFI_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_help_list,                     MENU_ENUM_SUBLABEL_HELP_LIST)
#ifdef HAVE_LAKKA
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_services_settings_list,        MENU_ENUM_SUBLABEL_SERVICES_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ssh_enable,                    MENU_ENUM_SUBLABEL_SSH_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_samba_enable,                  MENU_ENUM_SUBLABEL_SAMBA_ENABLE )
#ifdef HAVE_BLUETOOTH
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bluetooth_enable,              MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE )
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_localap_enable,                MENU_ENUM_SUBLABEL_LOCALAP_ENABLE )
#ifdef HAVE_RETROFLAG
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_safeshutdown_enable,           MENU_ENUM_SUBLABEL_SAFESHUTDOWN_ENABLE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_timezone,                      MENU_ENUM_SUBLABEL_TIMEZONE)
#endif
#ifdef HAVE_LAKKA_SWITCH
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_switch_options,                MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_switch_oc_enable,              MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_switch_cec_enable,             MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bluetooth_ertm_disable,        MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_user_language,                 MENU_ENUM_SUBLABEL_USER_LANGUAGE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_max_swapchain_images,          MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_waitable_swapchains,           MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_max_frame_latency,             MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_online_updater,                MENU_ENUM_SUBLABEL_ONLINE_UPDATER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_updater_settings,              MENU_ENUM_SUBLABEL_UPDATER_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_fps_show,                      MENU_ENUM_SUBLABEL_FPS_SHOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_fps_update_interval,           MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_framecount_show,               MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_memory_show,                   MENU_ENUM_SUBLABEL_MEMORY_SHOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_memory_update_interval,        MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_time_show,                     MENU_ENUM_SUBLABEL_TIME_SHOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_statistics_show,               MENU_ENUM_SUBLABEL_STATISTICS_SHOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_ping_show,             MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_settings,              MENU_ENUM_SUBLABEL_NETPLAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_retropad_settings,       MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_user_bind_settings,            MENU_ENUM_SUBLABEL_INPUT_USER_BINDS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_user_remap_settings,           MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_hotkey_settings,         MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_enable_hotkey,         MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_menu_toggle,           MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE)
#ifdef HAVE_LAKKA
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_restart_key,           MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY)
#else
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_quit_key,              MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_close_content_key,     MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_reset,                 MENU_ENUM_SUBLABEL_INPUT_META_RESET)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_fast_forward_key,      MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_fast_forward_hold_key, MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_slowmotion_key,        MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_slowmotion_hold_key,   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_rewind,                MENU_ENUM_SUBLABEL_INPUT_META_REWIND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_pause_toggle,          MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_frameadvance,          MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_mute,                  MENU_ENUM_SUBLABEL_INPUT_META_MUTE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_osk,                   MENU_ENUM_SUBLABEL_INPUT_META_OSK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_volume_up,             MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_volume_down,           MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_load_state_key,        MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_save_state_key,        MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_state_slot_plus,       MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_state_slot_minus,      MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_play_replay_key,        MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_record_replay_key,      MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_halt_replay_key,        MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_save_replay_checkpoint_key, MENU_ENUM_SUBLABEL_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_prev_replay_checkpoint_key, MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_next_replay_checkpoint_key, MENU_ENUM_SUBLABEL_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_replay_slot_plus,       MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_replay_slot_minus,      MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_disk_eject_toggle,     MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_disk_next,             MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_disk_prev,             MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_shader_toggle,         MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_shader_hold,           MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_shader_next,           MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_shader_prev,           MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV)
#ifdef HAVE_CHEATS
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_cheat_toggle,          MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_cheat_index_plus,      MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_cheat_index_minus,     MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_screenshot,            MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_recording_toggle,      MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_streaming_toggle,      MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_turbo_fire_toggle,     MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_grab_mouse_toggle,     MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_game_focus_toggle,     MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_fullscreen_toggle_key, MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_ui_companion_toggle,   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_vrr_runloop_toggle,    MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_runahead_toggle,       MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_preempt_toggle,        MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE)
#ifdef HAVE_VIDEO_FILTER
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_video_filter_toggle,   MENU_ENUM_SUBLABEL_INPUT_META_VIDEO_FILTER_TOGGLE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_fps_toggle,            MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_statistics_toggle,     MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_ai_service,            MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_netplay_ping_toggle,   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_netplay_host_toggle,   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_netplay_game_watch,    MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_netplay_player_chat,   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_meta_netplay_fade_chat_toggle, MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_hotkey_block_delay,         MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_hotkey_device_merge,        MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_hotkey_follows_player1,     MENU_ENUM_SUBLABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_device_type,                MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_device_index,               MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_mouse_index,                MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_adc_type,                   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_device_reservation_type,    MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_device_reserved_device_name, MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_bind_all,                   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_save_autoconfig,            MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_bind_defaults,              MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS)
#ifdef HAVE_MATERIALUI
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_icons_enable,        MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_switch_icons,        MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_playlist_icons_enable, MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_landscape_layout_optimization, MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_show_nav_bar,        MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_auto_rotate_nav_bar, MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_dual_thumbnail_list_view_enable, MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_thumbnail_background_enable, MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_add_content_list,              MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_frame_delay,             MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_frame_delay_auto,        MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_frame_time_sample_gated, MENU_ENUM_SUBLABEL_VIDEO_FRAME_TIME_SAMPLE_GATED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_shader_delay,            MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_black_frame_insertion,   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_bfi_dark_frames,         MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_shader_subframes,        MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_scan_subframes,          MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_toggle_gamepad_combo,          MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quit_gamepad_combo,            MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_show_hidden_files,             MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_log_verbosity,                 MENU_ENUM_SUBLABEL_LOG_VERBOSITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_log_to_file,                   MENU_ENUM_SUBLABEL_LOG_TO_FILE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_log_to_file_timestamp,         MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_log_dir,                       MENU_ENUM_SUBLABEL_LOG_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_monitor_index,           MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_refresh_rate_auto,       MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_hard_sync,               MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_hard_sync_frames,        MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_threaded,                MENU_ENUM_SUBLABEL_VIDEO_THREADED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings,                      MENU_ENUM_SUBLABEL_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_config_save_on_exit,           MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_config_save_minimal,           MENU_ENUM_SUBLABEL_CONFIG_SAVE_MINIMAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_save_on_exit,            MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_configuration_settings_list,   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_configurations_list_list,      MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_shared_context,          MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_driver_switch_enable,          MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_latency,                 MENU_ENUM_SUBLABEL_AUDIO_LATENCY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_rate_control_delta,      MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_mute,                    MENU_ENUM_SUBLABEL_AUDIO_MUTE)
#ifdef HAVE_AUDIOMIXER
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_mixer_mute,              MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE)
#endif
#if TARGET_OS_IOS
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_respect_silent_mode,     MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_fastforward_mute,        MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_rewind_mute,             MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_fastforward_speedup,     MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_camera_allow,                  MENU_ENUM_SUBLABEL_CAMERA_ALLOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_location_allow,                MENU_ENUM_SUBLABEL_LOCATION_ALLOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_max_users,               MENU_ENUM_SUBLABEL_INPUT_MAX_USERS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_poll_type_behavior,      MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_bind_timeout,            MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_bind_hold,               MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_volume,                  MENU_ENUM_SUBLABEL_AUDIO_VOLUME)
#ifdef HAVE_AUDIOMIXER
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_mixer_volume,            MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_sync,                    MENU_ENUM_SUBLABEL_AUDIO_SYNC)
#if defined(GEKKO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_mouse_scale,             MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_touch_scale,             MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE)
#ifdef UDEV_TOUCH_SUPPORT
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_touch_vmouse_pointer,    MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_touch_vmouse_mouse,      MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_touch_vmouse_touchpad,   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_touch_vmouse_trackball,  MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_touch_vmouse_gesture,    MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_button_axis_threshold,         MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_analog_deadzone,               MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_analog_sensitivity,            MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sensor_accelerometer_sensitivity,            MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sensor_orientation,            MENU_ENUM_SUBLABEL_INPUT_SENSOR_ORIENTATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sensor_gyroscope_sensitivity,            MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY)

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_enable,            MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_period,            MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_duty_cycle,        MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_mode,              MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_bind,              MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_button,            MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_turbo_allow_dpad,        MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_rumble_gain,             MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vertical_sync,           MENU_ENUM_SUBLABEL_VIDEO_VSYNC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_adaptive_vsync,          MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_scanline_sync,           MENU_ENUM_SUBLABEL_VIDEO_SCANLINE_SYNC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_allow_rotate,             MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_dummy_on_core_shutdown,        MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_option_category_enable,   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_info_cache_enable,        MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_info_savestate_bypass,    MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS)
#ifndef HAVE_DYNAMIC
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_always_reload_core_on_run_content, MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_refresh_rate,            MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_refresh_rate_polled,     MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_enable,                  MENU_ENUM_SUBLABEL_AUDIO_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_enable_menu,             MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_sounds,                   MENU_ENUM_SUBLABEL_MENU_SOUNDS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_max_timing_skew,         MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_pause_nonactive,               MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_pause_on_disconnect,           MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_disable_composition,     MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_smooth,                  MENU_ENUM_SUBLABEL_VIDEO_SMOOTH)
#ifdef HAVE_ODROIDGO2
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_ctx_scaling,             MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING)
#else
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_ctx_scaling,             MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_history_list_enable,           MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_history_size,          MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_favorites_size,        MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_input_unified_controls,   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_input_disable_info_button,     MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_input_disable_search_button,   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_input_disable_left_analog_in_menu,  MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_input_disable_right_analog_in_menu, MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_confirm_quit,                  MENU_ENUM_SUBLABEL_CONFIRM_QUIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_confirm_close,                 MENU_ENUM_SUBLABEL_CONFIRM_CLOSE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_confirm_reset,                 MENU_ENUM_SUBLABEL_CONFIRM_RESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_onscreen_notifications_enable, MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_crop_overscan,           MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_filter,                  MENU_ENUM_SUBLABEL_VIDEO_FILTER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_filter_remove,           MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_nickname,              MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_username,              MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheevos_password,              MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_post_filter_record,      MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_start_core,                    MENU_ENUM_SUBLABEL_START_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_list,                     MENU_ENUM_SUBLABEL_CORE_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_list_unload,              MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_download_core,                 MENU_ENUM_SUBLABEL_DOWNLOAD_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_update_installed_cores,        MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sync_now,                      MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_NOW)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_resolve_keep_local,            MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_resolve_keep_server,           MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER)
#if defined(ANDROID)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_switch_installed_cores_pfd,    MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sideload_core_list,            MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_manager_list,             MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST)
#ifdef HAVE_MIST
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_steam_settings_list,           MENU_ENUM_SUBLABEL_STEAM_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_steam_rich_presence_enable,    MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_steam_rich_presence_format,    MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_manager_steam_list,       MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_core_manager_steam,  MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_load_disc,                     MENU_ENUM_SUBLABEL_LOAD_DISC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_dump_disc,                     MENU_ENUM_SUBLABEL_DUMP_DISC)
#ifdef HAVE_LAKKA
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_eject_disc,                    MENU_ENUM_SUBLABEL_EJECT_DISC)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_list,                  MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_subsystem_settings,            MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_special,               MENU_ENUM_SUBLABEL_LOAD_CONTENT_SPECIAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_load_content_history,          MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_network_information,           MENU_ENUM_SUBLABEL_NETWORK_INFORMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_system_information,            MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION)
#ifdef HAVE_LAKKA
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quit_retroarch,                MENU_ENUM_SUBLABEL_RESTART_RETROARCH)
#else

/*DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quit_retroarch,                MENU_ENUM_SUBLABEL_QUIT_RETROARCH)*/
static int action_bind_sublabel_quit_retroarch(file_list_t* list,
      unsigned type, unsigned i, const char* label, const char* path, char* s, size_t len)
{
   settings_t *settings = config_get_ptr();
   bool save_on_exit    = settings->bools.config_save_on_exit;
   if (save_on_exit)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_QUIT_RETROARCH), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE), len);
   return 1;
}

#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_restart_retroarch,             MENU_ENUM_SUBLABEL_RESTART_RETROARCH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_widgets,                  MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_load_content_animation, MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_autoconfig,  MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_autoconfig_fails, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS)
#ifdef HAVE_PATCH
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_patch_applied,   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_remap_load,  MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_config_override_load, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_set_initial_disk, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_disk_control, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_save_state, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_fast_forward, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD)
#ifdef HAVE_SCREENSHOTS
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_screenshot,  MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_screenshot_duration,  MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_screenshot_flash, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_refresh_rate, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE)
#ifdef HAVE_NETWORKING
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_netplay_extra, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_when_menu_is_alive, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_width,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_height,           MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_auto_width_max,   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_auto_height_max,  MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_fullscreen_x,            MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_fullscreen_y,            MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_color_red,       MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_color_green,     MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_color_blue,      MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_bgcolor_enable,  MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_bgcolor_red,     MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_bgcolor_green,   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_bgcolor_blue,    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_bgcolor_opacity, MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_force_resolution,        MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_save_window_position,    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_custom_size_enable, MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_pos_x,           MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_message_pos_y,           MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_font_size,               MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_behind_menu,     MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_hide_in_menu,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU)
#if defined(ANDROID)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_hide_when_gamepad_connected_android, MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID)
#else
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_hide_when_gamepad_connected, MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_show_mouse_cursor,       MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_auto_rotate,     MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_auto_scale,      MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_osk_overlay_auto_scale,  MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_dpad_diag_sens,  MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_abxy_diag_sens,  MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_analog_recenter_zone,  MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_collection_list,       MENU_ENUM_SUBLABEL_PLAYLISTS_TAB)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_scale_integer,           MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_scale_integer_axis,      MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_scale_integer_scaling,   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_gpu_screenshot,          MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_rotation,                MENU_ENUM_SUBLABEL_VIDEO_ROTATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_screen_orientation,            MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_force_srgb_enable,       MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_fullscreen,              MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_windowed_fullscreen,     MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_autoswitch_refresh_rate, MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_autoswitch_pal_threshold,MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_gpu_record,              MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_auto_index,          MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_replay_auto_index,             MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_block_sram_overwrite,          MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_fastforward_ratio,             MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_fastforward_frameskip,         MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_vrr_runloop_enable,            MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_throttle_framerate,       MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_slowmotion_ratio,              MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_run_ahead_unsupported,         MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED)
#if (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_runahead_mode,                 MENU_ENUM_SUBLABEL_RUNAHEAD_MODE)
#else
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_runahead_mode,                 MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_run_ahead_hide_warnings,       MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_run_ahead_frames,              MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_preempt_frames,                MENU_ENUM_SUBLABEL_PREEMPT_FRAMES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_block_timeout,           MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT)
#ifdef HAVE_CHEATS
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_notification_show_cheats_applied, MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_apply_after_toggle,      MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_apply_after_load,        MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_idx,                     MENU_ENUM_SUBLABEL_CHEAT_IDX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_big_endian,              MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_start_or_cont,           MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_start_or_restart,        MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_search_exact,            MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_search_lt,               MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_search_gt,               MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_search_eq,               MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQ)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_search_neq,              MENU_ENUM_SUBLABEL_CHEAT_SEARCH_NEQ)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_search_eqplus,           MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_search_eqminus,          MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_repeat_count,            MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_repeat_add_to_address,   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_repeat_add_to_value,     MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_add_matches,             MENU_ENUM_SUBLABEL_CHEAT_ADD_MATCHES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_add_new_top,             MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_add_new_bottom,          MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_reload_cheats,           MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_address_bit_position,    MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_delete_all,              MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_cheat_options,            MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_cheats,        MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheatfile_directory,           MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_apply_changes,           MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_file_load,               MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_file_load_append,        MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cheat_file_save_as,            MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rewind_enable,                 MENU_ENUM_SUBLABEL_REWIND_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rewind_settings,               MENU_ENUM_SUBLABEL_REWIND_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rewind_granularity,            MENU_ENUM_SUBLABEL_REWIND_GRANULARITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rewind_buffer_size,            MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rewind_buffer_size_step,       MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_libretro_log_level,            MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_frontend_log_level,            MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_perfcnt_enable,                MENU_ENUM_SUBLABEL_PERFCNT_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_auto_save,           MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_auto_load,           MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_thumbnail_enable,    MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_file_compression,         MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_file_compression,    MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_max_keep,            MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_autosave_interval,             MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_automatic_interval,  MENU_ENUM_SUBLABEL_SAVESTATE_AUTOMATIC_INTERVAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_replay_max_keep,               MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_replay_checkpoint_interval,    MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_replay_checkpoint_deserialize, MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_DESERIALIZE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_remap_binds_enable,      MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_remap_sort_by_controller_enable,      MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_autodetect_enable,       MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE)
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_nowinkey_enable,         MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE)
#endif
#ifdef ANDROID
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_select_physical_keyboard,   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_android_input_disconnect_workaround, MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_sensors_enable,          MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_auto_mouse_grab,         MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_auto_game_focus,         MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_swap_ok_cancel,          MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_swap_scroll,             MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_all_users_control_menu,  MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_singleclick_playlists,    MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_allow_tabs_back,          MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_pause_libretro,                MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_savestate_resume,         MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_insert_disk_resume,       MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quit_on_close_content,         MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_screensaver_timeout,      MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT)
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_screensaver_animation,       MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_screensaver_animation_speed, MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_remember_selection,       MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_startup_page,             MENU_ENUM_SUBLABEL_MENU_STARTUP_PAGE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_driver,                  MENU_ENUM_SUBLABEL_VIDEO_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_driver,                  MENU_ENUM_SUBLABEL_AUDIO_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_driver,                  MENU_ENUM_SUBLABEL_INPUT_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_joypad_driver,                 MENU_ENUM_SUBLABEL_JOYPAD_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_resampler_driver,        MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_camera_driver,                 MENU_ENUM_SUBLABEL_CAMERA_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_location_driver,               MENU_ENUM_SUBLABEL_LOCATION_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_driver,                   MENU_ENUM_SUBLABEL_MENU_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_record_driver,                 MENU_ENUM_SUBLABEL_RECORD_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_midi_driver,                   MENU_ENUM_SUBLABEL_MIDI_DRIVER)
#ifdef HAVE_BLUETOOTH
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bluetooth_driver,              MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_wifi_driver,                   MENU_ENUM_SUBLABEL_WIFI_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_filter_supported_extensions,   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_filter_by_current_core,        MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_wallpaper,                     MENU_ENUM_SUBLABEL_MENU_WALLPAPER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_dynamic_wallpaper,             MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_device,                  MENU_ENUM_SUBLABEL_AUDIO_DEVICE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_output_rate,             MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_dsp_plugin,              MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_dsp_plugin_remove,       MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_wasapi_exclusive_mode,   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_wasapi_sh_buffer_length, MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH)

#ifdef HAVE_MICROPHONE
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_driver,                  MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_resampler_driver,        MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_resampler_quality,       MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_enable,                  MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_device,                  MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_rate,                    MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_latency,                 MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_wasapi_exclusive_mode,   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_wasapi_float_format,     MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_microphone_wasapi_sh_buffer_length, MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH)
#endif

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_opacity,                 MENU_ENUM_SUBLABEL_OVERLAY_OPACITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_osk_overlay_opacity,             MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_scale_landscape,         MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_aspect_adjust_landscape, MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_x_separation_landscape,  MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_y_separation_landscape,  MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_x_offset_landscape,      MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_y_offset_landscape,      MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_scale_portrait,          MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_aspect_adjust_portrait,  MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_x_separation_portrait,   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_y_separation_portrait,   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_x_offset_portrait,       MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_y_offset_portrait,       MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_enable,                  MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_autoload_preferred,      MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_preset,                  MENU_ENUM_SUBLABEL_OVERLAY_PRESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_osk_overlay_preset,              MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_pointer_enable,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_lightgun_port,              MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_lightgun_trigger_on_touch,  MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_lightgun_trigger_delay,     MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_lightgun_allow_offscreen,   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_lightgun_two_touch_input,   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_lightgun_three_touch_input, MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_lightgun_four_touch_input,  MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_mouse_speed,                MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_mouse_hold_to_drag,         MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_mouse_hold_msec,            MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_mouse_dtap_to_drag,         MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_mouse_dtap_msec,            MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_mouse_swipe_threshold,      MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_mouse_alt_two_touch_input,  MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_public_announce,       MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_ip_address,            MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_tcp_udp_port,          MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_max_connections,       MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_max_ping,              MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_password,              MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_spectate_password,     MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_start_as_spectator,    MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_fade_chat,             MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_chat_color_name,       MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_chat_color_msg,        MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_allow_pausing,         MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_allow_slaves,          MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_require_slaves,        MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_check_frames,          MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_nat_traversal,         MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_stdin_cmd_enable,              MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_mouse_enable,                  MENU_ENUM_SUBLABEL_MOUSE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_pointer_enable,                MENU_ENUM_SUBLABEL_POINTER_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_icon_thumbnails,               MENU_ENUM_SUBLABEL_ICON_THUMBNAILS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_thumbnails,                    MENU_ENUM_SUBLABEL_THUMBNAILS)
#ifdef HAVE_MATERIALUI
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_thumbnails_materialui,         MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_left_thumbnails_materialui,    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_left_thumbnails,               MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS)
#ifdef HAVE_RGUI
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_left_thumbnails_rgui,          MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_thumbnails_rgui,               MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI)
#endif
#ifdef HAVE_OZONE
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_left_thumbnails_ozone,                   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_menu_color_theme,                  MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_padding_factor,                    MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_header_icon,                       MENU_ENUM_SUBLABEL_OZONE_HEADER_ICON)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_header_separator,                  MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_collapse_sidebar,                  MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_show_sidebar,                      MENU_ENUM_SUBLABEL_OZONE_SHOW_SIDEBAR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_scroll_content_metadata,           MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_thumbnail_scale_factor,            MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale,                        MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale_factor_global,          MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale_factor_title,           MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale_factor_sidebar,         MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale_factor_label,           MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale_factor_sublabel,        MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale_factor_time,            MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_font_scale_factor_footer,          MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER)
#endif
#if defined(HAVE_OZONE) || defined(HAVE_XMB)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_truncate_playlist_name,            MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_ozone_sort_after_truncate_playlist_name, MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_thumbnail_upscale_threshold,      MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_thumbnail_background_enable,      MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_timedate_enable,                       MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_timedate_style,                        MENU_ENUM_SUBLABEL_TIMEDATE_STYLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_timedate_date_separator,               MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_battery_level_enable,                  MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_sublabels,                   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_confirm,                     MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIRM)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_navigation_wraparound,                 MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_resampler_quality,               MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_fastpath_s16,                    MENU_ENUM_SUBLABEL_AUDIO_FASTPATH_S16)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_format_negotiation,              MENU_ENUM_SUBLABEL_AUDIO_FORMAT_NEGOTIATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_enable_host,                   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_enable_client,                 MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_disconnect,                    MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_kick,                          MENU_ENUM_SUBLABEL_NETPLAY_KICK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_ban,                           MENU_ENUM_SUBLABEL_NETPLAY_BAN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_file,                             MENU_ENUM_SUBLABEL_SCAN_FILE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_directory,                        MENU_ENUM_SUBLABEL_SCAN_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_method,                           MENU_ENUM_SUBLABEL_SCAN_METHOD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_use_db,                           MENU_ENUM_SUBLABEL_SCAN_USE_DB)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_db_select,                        MENU_ENUM_SUBLABEL_SCAN_DB_SELECT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_target_playlist,                  MENU_ENUM_SUBLABEL_SCAN_TARGET_PLAYLIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_single_file,                      MENU_ENUM_SUBLABEL_SCAN_SINGLE_FILE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_omit_db_ref,                      MENU_ENUM_SUBLABEL_SCAN_OMIT_DB_REF)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_swap_interval,                   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sort_savefiles_enable,                 MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sort_savestates_enable,                MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sort_savefiles_by_content_enable,      MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sort_savestates_by_content_enable,     MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_sort_screenshots_by_content_enable,    MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savefiles_in_content_dir_enable,       MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestates_in_content_dir_enable,      MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_screenshots_in_content_dir_enable,     MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_systemfiles_in_content_dir_enable,     MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_updater_buildbot_url,             MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_show_inputs,             MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_overlay_show_inputs_port,        MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_updater_buildbot_assets_url,      MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_updater_auto_extract_archive,     MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_updater_show_experimental_cores,  MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_updater_auto_backup,              MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_updater_auto_backup_history_size, MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_refresh_rooms,                 MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_refresh_lan,                   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rename_entry,                          MENU_ENUM_SUBLABEL_RENAME_ENTRY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_delete_entry,                          MENU_ENUM_SUBLABEL_DELETE_ENTRY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_information,                           MENU_ENUM_SUBLABEL_INFORMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_run,                                   MENU_ENUM_SUBLABEL_RUN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_add_to_favorites,                      MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_add_to_playlist,                       MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_set_core_association,                  MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_reset_core_association,                MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_download_pl_entry_thumbnails,          MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_goto_favorites,                        MENU_ENUM_SUBLABEL_GOTO_FAVORITES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_goto_images,                           MENU_ENUM_SUBLABEL_GOTO_IMAGES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_goto_music,                            MENU_ENUM_SUBLABEL_GOTO_MUSIC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_goto_video,                            MENU_ENUM_SUBLABEL_GOTO_VIDEO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_goto_explore,                          MENU_ENUM_SUBLABEL_GOTO_EXPLORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_goto_contentless_cores,                MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_filebrowser_settings,             MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_filebrowser_open_uwp_permissions, MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_filebrowser_open_picker,          MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_auto_remaps_enable,                    MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_initial_disk_change_enable,            MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_auto_overrides_enable,                 MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_game_specific_options,                 MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_global_core_options,                   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_enable,                           MENU_ENUM_SUBLABEL_CORE_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_database_manager,                      MENU_ENUM_SUBLABEL_DATABASE_MANAGER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cursor_manager,                        MENU_ENUM_SUBLABEL_CURSOR_MANAGER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_take_screenshot,                       MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_resume_content,                        MENU_ENUM_SUBLABEL_RESUME_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_close_content,                         MENU_ENUM_SUBLABEL_CLOSE_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_list,                        MENU_ENUM_SUBLABEL_SAVESTATE_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_state_slot,                            MENU_ENUM_SUBLABEL_STATE_SLOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_load_state,                            MENU_ENUM_SUBLABEL_LOAD_STATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_state,                            MENU_ENUM_SUBLABEL_SAVE_STATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_undo_load_state,                       MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_undo_save_state,                       MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_record_replay,                         MENU_ENUM_SUBLABEL_RECORD_REPLAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_play_replay,                           MENU_ENUM_SUBLABEL_PLAY_REPLAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_halt_replay,                           MENU_ENUM_SUBLABEL_HALT_REPLAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_replay_slot,                           MENU_ENUM_SUBLABEL_REPLAY_SLOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_accounts_retro_achievements,           MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_accounts_list,                         MENU_ENUM_SUBLABEL_ACCOUNTS_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_restart_content,                       MENU_ENUM_SUBLABEL_RESTART_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_override_file_info,                    MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_override_file_load,                    MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_override_file_save_as,                 MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_override_unload,                       MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_current_config_override_core,     MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_current_config_override_content_dir,
                                                                                   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_save_current_config_override_game,     MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remove_current_config_override_core,   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remove_current_config_override_content_dir,
                                                                                   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remove_current_config_override_game,   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_input_remapping_options,          MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_manager_list,               MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_info,                       MENU_ENUM_SUBLABEL_REMAP_FILE_INFO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_load,                       MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_save_as,                    MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_save_core,                  MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_save_content_dir,           MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_save_game,                  MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_remove_core,                MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_remove_content_dir,         MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_remove_game,                MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_reset,                      MENU_ENUM_SUBLABEL_REMAP_FILE_RESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_remap_file_flush,                      MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_option_override_list,             MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_option_override_info,             MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_options_game_specific_create,     MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_options_game_specific_remove,     MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_options_folder_specific_create,   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_options_folder_specific_remove,   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_options_reset,                    MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_options_flush,                    MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_show_advanced_settings,                MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_threaded_data_runloop_enable,          MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_entry_rename,                 MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_entry_remove,                 MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_system_directory,                      MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rgui_browser_directory,                MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_use_last_start_directory,              MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_suggest_always,                   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_use_builtin_media_player,              MENU_ENUM_SUBLABEL_USE_BUILTIN_PLAYER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_use_builtin_image_viewer,              MENU_ENUM_SUBLABEL_USE_BUILTIN_IMAGE_VIEWER)
DEFAULT_SUBLABEL_MACRO(action_bind_dynamic_wallpapers_directory,                   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_thumbnails_directory,                           MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_rgui_config_directory,                          MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_latency_frames,                  MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_latency_frames_range,            MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_disk_tray_eject,                       MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_disk_tray_insert,                      MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_disk_image_append,                     MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_disk_index,                            MENU_ENUM_SUBLABEL_DISK_INDEX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_disk_options,                          MENU_ENUM_SUBLABEL_DISK_OPTIONS)
#ifdef HAVE_XMB
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_layout,                            MENU_ENUM_SUBLABEL_XMB_LAYOUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_menu_color_theme,                  MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_icon_theme,                        MENU_ENUM_SUBLABEL_XMB_THEME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_ribbon_enable,                    MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_current_menu_icon,                 MENU_ENUM_SUBLABEL_XMB_CURRENT_MENU_ICON)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_entry_icons,                       MENU_ENUM_SUBLABEL_XMB_ENTRY_ICONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_switch_icons,                      MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_shadows_enable,                    MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_xmb_vertical_thumbnails,               MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_alpha_factor,                 MENU_ENUM_SUBLABEL_XMB_ALPHA_FACTOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_vertical_fade_factor,         MENU_ENUM_SUBLABEL_MENU_XMB_VERTICAL_FADE_FACTOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_show_horizontal_list,         MENU_ENUM_SUBLABEL_MENU_XMB_SHOW_HORIZONTAL_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_show_title_header,            MENU_ENUM_SUBLABEL_MENU_XMB_SHOW_TITLE_HEADER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_title_margin,                 MENU_ENUM_SUBLABEL_MENU_XMB_TITLE_MARGIN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_title_margin_horizontal_offset, MENU_ENUM_SUBLABEL_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_font,                         MENU_ENUM_SUBLABEL_XMB_FONT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_thumbnail_scale_factor,       MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_animation_horizontal_higlight,MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_animation_move_up_down,       MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_xmb_animation_opening_main_menu,  MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU)
#endif
#ifdef HAVE_MATERIALUI
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_menu_color_theme,              MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_menu_transition_animation,     MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_menu_thumbnail_view_portrait,  MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_materialui_menu_thumbnail_view_landscape, MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_use_preferred_system_color_theme, MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_scale_factor,                     MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_widget_scale_auto,                MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO)
#if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_widget_scale_factor,              MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR)
#else
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_widget_scale_factor,              MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_widget_scale_factor_windowed,     MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_wallpaper_opacity,                MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_framebuffer_opacity,              MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_horizontal_animation,             MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_ozone_font,                       MENU_ENUM_SUBLABEL_OZONE_FONT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_drivers,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_video,                   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_audio,                   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_input,                   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_latency,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_core,                    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_configuration,           MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_saving,                  MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_logging,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_file_browser,            MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_frame_throttle,          MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_recording,               MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_onscreen_display,        MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_user_interface,          MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_ai_service,              MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_accessibility,           MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_power_management,        MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_achievements,            MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_network,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_playlists,               MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_user,                    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_directory,               MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_settings_show_steam,                   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_take_screenshot,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_resume_content,        MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_restart_content,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_close_content,         MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_savestate_submenu,     MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_save_load_state,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_replay,                MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_undo_save_load_state,  MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_add_to_favorites,      MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_add_to_playlist,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_start_recording,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_start_streaming,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_set_core_association,  MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_reset_core_association,MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_options,               MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_core_options_flush,    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_controls,              MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_shaders,               MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_show_overlays,                 MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_show_rewind,                   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_show_latency,                  MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_save_core_overrides,   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_save_content_dir_overrides, MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_save_game_overrides,   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_information,           MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_download_thumbnails,   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_enable_kiosk_mode,                MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_disable_kiosk_mode,               MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_kiosk_mode_password,              MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_favorites_tab,                    MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_favorites_first,                  MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_images_tab,                       MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_load_core,                   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_load_content,                MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_load_disc,                   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_dump_disc,                   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC)
#if defined(HAVE_CDROM) && defined(HAVE_LAKKA)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_eject_disc,                  MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_information,                 MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_configurations,              MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_help,                        MENU_ENUM_SUBLABEL_MENU_SHOW_HELP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_quit_retroarch,              MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH)
#ifndef HAVE_LAKKA
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_restart_retroarch,           MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_reboot,                      MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_shutdown,                    MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_online_updater,              MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_show_core_updater,                MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_scroll_fast,                      MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_scroll_delay,                     MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_music_tab,                        MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_video_tab,                        MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_netplay_tab,                      MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_settings_tab,                     MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_settings_tab_enable_password,     MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_history_tab,                      MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_import_content_entry,             MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_playlists_tab,                    MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_playlist_tabs,                    MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_explore_tab,                      MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_contentless_cores_tab,            MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_main_menu_enable_settings,             MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rgui_show_start_screen,                MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_assets_directory,                 MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_input_remapping_directory,             MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_directory,                        MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_info_directory,                   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_joypad_autoconfig_directory,           MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlists_directory,                   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_favorites_directory,           MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_history_directory,             MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_image_history_directory,       MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_music_history_directory,       MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_video_history_directory,       MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_runtime_log_directory,                 MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_cache_directory,                       MENU_ENUM_SUBLABEL_CACHE_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_database_directory,                    MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_assets_directory,                      MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savefile_directory,                    MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_savestate_directory,                   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_screenshot_directory,                  MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_directory,                     MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_osk_overlay_directory,                 MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_audio_filter_directory,                MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_filter_directory,                MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_shader_directory,                MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_recording_output_directory,            MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_recording_config_directory,            MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_font_path,                       MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_shaders_enable,                  MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_options,                        MENU_ENUM_SUBLABEL_SHADER_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_apply_changes,                  MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_watch_for_changes,              MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_shader_remember_last_dir,        MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_num_passes,                     MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset,                         MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_prepend,                 MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_append,                  MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_manager,                 MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_MANAGER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_file_info,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_FILE_INFO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_save_reference,          MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_save_current,            MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_save_as,                 MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_save_global,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_save_core,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_save_parent,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_save_game,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_remove_global,           MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_remove_core,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_remove_parent,           MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_remove_game,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_parameters,                     MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_shader_preset_parameters,              MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu,                            MENU_ENUM_SUBLABEL_CONTENT_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_information,                      MENU_ENUM_SUBLABEL_CORE_INFORMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_disc_information,                      MENU_ENUM_SUBLABEL_DISC_INFORMATION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_aspect_ratio,                    MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_aspect_ratio_index,              MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX)
#if defined(DINGUX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_dingux_ipu_keep_aspect,          MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_dingux_ipu_filter_type,          MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE)
#if defined(DINGUX_BETA)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_dingux_refresh_rate,             MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE)
#endif
#if defined(RS90) || defined(MIYOO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_dingux_rs90_softfilter_type,     MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE)
#endif
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_screen_resolution,               MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_use_metal_arg_buffers,     MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_gpu_index,                 MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_custom_height,          MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_custom_width,           MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_custom_x,               MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_custom_y,               MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_bias_x,                 MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_bias_y,                 MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y)
#if defined(RARCH_MOBILE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_bias_portrait_x,        MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_vp_bias_portrait_y,        MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_use_mitm_server,               MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_mitm_server,                   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_netplay_custom_mitm_server,            MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_lock,                             MENU_ENUM_SUBLABEL_CORE_LOCK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_set_standalone_exempt,            MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_delete,                           MENU_ENUM_SUBLABEL_CORE_DELETE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_pause_hardcode_mode,                   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_resume_hardcode_mode,                  MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_midi_input,                            MENU_ENUM_SUBLABEL_MIDI_INPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_midi_output,                           MENU_ENUM_SUBLABEL_MIDI_OUTPUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_midi_volume,                           MENU_ENUM_SUBLABEL_MIDI_VOLUME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_onscreen_overlay_settings_list,        MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_osk_overlay_settings_list,             MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_lightgun_settings_list,        MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_overlay_mouse_settings_list,           MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_onscreen_notifications_settings_list,  MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_onscreen_notifications_views_settings_list, MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS)
#ifdef HAVE_QT
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_show_wimp,                             MENU_ENUM_SUBLABEL_SHOW_WIMP)
#endif
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_discord_allow,                         MENU_ENUM_SUBLABEL_DISCORD_ALLOW)

#if defined(HAVE_LIBNX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_switch_cpu_profile,             MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE)
#endif

#ifndef HAVE_LAKKA
#ifdef __linux__
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_gamemode_enable,                MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX)
#else
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_gamemode_enable,                MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE)
#endif
#endif /*HAVE_LAKKA*/

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_brightness_control,             MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL)

#ifdef _3DS
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_new3ds_speedup_enable,          MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_3ds_display_mode,         MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_3ds_lcd_bottom,           MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bottom_assets_directory,        MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bottom_font_enable,             MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bottom_font_color_red,          MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bottom_font_color_green,        MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bottom_font_color_blue,         MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bottom_font_color_opacity,      MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_bottom_font_scale,              MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE)
#endif

#if defined (WIIU)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_wiiu_prefer_drc,          MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC)
#endif

#if defined(GEKKO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_overscan_correction_top,    MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_overscan_correction_bottom, MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM)
#endif

#if defined(HAVE_WINDOW_OFFSET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_offset_x,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_video_window_offset_y,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y)
#endif

DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_show_sublabels,                       MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_show_history_icons,                   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_show_entry_idx,                       MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_border_filler_enable,                MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_border_filler_thickness_enable,      MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_background_filler_thickness_enable,  MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_linear_filter,                            MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_aspect_ratio_lock,                   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rgui_menu_color_theme,                         MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rgui_menu_theme_preset,                        MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_transparency,                        MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_shadows,                             MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_particle_effect,                     MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_particle_effect_speed,               MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_particle_effect_screensaver,         MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_inline_thumbnails,                   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_swap_thumbnails,                     MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_thumbnail_downscaler,                MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_thumbnail_delay,                     MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_runtime_log,                           MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_content_runtime_log_aggregate,                 MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_without_core_match,                       MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_scan_serial_and_crc,                           MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_sublabel_runtime_type,                MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_sublabel_last_played_style,           MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_internal_upscale_level,              MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_aspect_ratio,                        MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_ticker_type,                              MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_ticker_speed,                             MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_ticker_smooth,                            MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_show_inline_core_name,                MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_sort_alphabetical,                    MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_fuzzy_archive_match,                  MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_use_old_format,                       MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_compression,                          MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_portable_paths,                       MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_use_filename,                         MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_playlist_allow_non_png,                        MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_full_width_layout,                   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_extended_ascii,                      MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_menu_rgui_switch_icons,                        MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_pl_thumbnails_updater_list,                    MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_download_core_system_files,                    MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_download_core_content,                         MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_rdb_entry_detail,                              MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_list,                      MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_dir,                       MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_system_name,               MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_system_name_custom,        MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_core_name,                 MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_file_exts,                 MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_search_recursively,        MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_search_archives,           MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_dat_file,                  MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_dat_file_filter,           MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_overwrite,                 MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_validate_entries,          MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_manual_content_scan_start,                     MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_create_backup,                            MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_restore_backup_list,                      MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_delete_backup_list,                       MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST)

#ifdef HAVE_GAME_AI
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_game_ai_menu_option,                       MENU_ENUM_SUBLABEL_GAME_AI_MENU_OPTION)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_quick_menu_show_game_ai,                       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_core_game_ai_options,            MENU_ENUM_SUBLABEL_CORE_GAME_AI_OPTIONS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_game_ai_override_p1,            MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_game_ai_override_p2,            MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_game_ai_show_debug,            MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG)
#endif

#ifdef HAVE_SMBCLIENT
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_settings,                         MENU_ENUM_SUBLABEL_SMB_CLIENT_SETTINGS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_enable,                           MENU_ENUM_SUBLABEL_SMB_CLIENT_ENABLE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_auth_mode,                        MENU_ENUM_SUBLABEL_SMB_CLIENT_AUTH_MODE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_server,                           MENU_ENUM_SUBLABEL_SMB_CLIENT_SERVER)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_share,                            MENU_ENUM_SUBLABEL_SMB_CLIENT_SHARE)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_subdir,                           MENU_ENUM_SUBLABEL_SMB_CLIENT_SUBDIR)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_username,                         MENU_ENUM_SUBLABEL_SMB_CLIENT_USERNAME)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_password,                         MENU_ENUM_SUBLABEL_SMB_CLIENT_PASSWORD)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_workgroup,                        MENU_ENUM_SUBLABEL_SMB_CLIENT_WORKGROUP)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_num_contexts,                     MENU_ENUM_SUBLABEL_SMB_CLIENT_NUM_CONTEXTS)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_timeout,                          MENU_ENUM_SUBLABEL_SMB_CLIENT_TIMEOUT)
DEFAULT_SUBLABEL_MACRO(action_bind_sublabel_smb_client_browse,                           MENU_ENUM_SUBLABEL_SMB_CLIENT_BROWSE)
#endif

static int action_bind_sublabel_systeminfo_controller_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   char tmp[NAME_MAX_LENGTH];
   unsigned controller;
   const char *val_port_dev_name =
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME);

   for (controller = 0; controller < MAX_USERS; controller++)
   {
      if (input_config_get_device_autoconfigured(controller))
      {
            snprintf(tmp, sizeof(tmp),
               val_port_dev_name,
               controller + 1,
               input_config_get_device_name(controller));

            if (string_is_equal(path, tmp))
               break;
      }
   }

   snprintf(s, len,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO),
           input_config_get_device_display_name(controller)
         ? input_config_get_device_display_name(controller)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
           input_config_get_device_display_name(controller)
         ? input_config_get_device_config_name(controller)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
           input_config_get_device_vid(controller),
           input_config_get_device_pid(controller));

   return 0;
}

static int action_bind_sublabel_core_info_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   if (!list || (!list->list[i].label || !*list->list[i].label))
      return 0;
   {
      int pos = string_find_index_substring_string(list->list[i].label, "(md5)");
      if (pos >= 0)
         strlcpy(s, list->list[i].label + pos, len);
      else
         strlcpy(s, list->list[i].label, len);
   }
   return 0;
}

#ifdef HAVE_BLUETOOTH
static int action_bind_sublabel_bluetooth_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   driver_bluetooth_device_get_sublabel(s, i, len);
   return 0;
}
#endif

#ifdef HAVE_LAKKA
static int action_bind_sublabel_cpu_policy_entry_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   /* Displays info about the Policy entry */
   cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(false);
   if (drivers)
   {
      int idx     = atoi(path);
      size_t _len = strlcpy(s, drivers[idx]->scaling_governor, len);
      snprintf(s + _len, len - _len, " | Freq: %u MHz\n",
            drivers[idx]->current_frequency / 1000);
      return 0;
   }

   return -1;
}
static int action_bind_sublabel_cpu_perf_mode(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   /* Displays info about the mode selected */
   enum cpu_scaling_mode mode = get_cpu_scaling_mode(NULL);
   strlcpy(s, msg_hash_to_str(
      MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF + (int)mode), len);
   return 0;
}
#endif

#ifdef HAVE_CHEEVOS
static int action_bind_sublabel_cheevos_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   unsigned offset = type - MENU_SETTINGS_CHEEVOS_START;
   rcheevos_menu_get_sublabel(offset, s, len);
   return 0;
}
#endif

static int action_bind_sublabel_subsystem_add(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   const struct retro_subsystem_info *subsystem = NULL;
   runloop_state_t *runloop_st                  = runloop_state_get_ptr();
   rarch_system_info_t *sys_info                = &runloop_st->system;

   /* Core fully loaded, use the subsystem data */
   if (sys_info->subsystem.data)
      subsystem = sys_info->subsystem.data + (type - MENU_SETTINGS_SUBSYSTEM_ADD);
   /* Core not loaded completely, use the data we peeked on load core */
   else
      subsystem = runloop_st->subsystem_data + (type - MENU_SETTINGS_SUBSYSTEM_ADD);

   if (subsystem && runloop_st->subsystem_current_count > 0)
   {
      if (content_get_subsystem_rom_id() < subsystem->num_roms)
         snprintf(s, len,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO),
               (content_get_subsystem() == (int)type - MENU_SETTINGS_SUBSYSTEM_ADD)
             ? subsystem->roms[content_get_subsystem_rom_id()].desc
             : subsystem->roms[0].desc);
   }

   return 0;
}

static int action_bind_sublabel_subsystem_load(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   unsigned j = 0;
   size_t _len = 0;
   char buf[PATH_MAX_LENGTH];
   buf[0] = '\0';
   for (j = 0; j < content_get_subsystem_rom_id(); j++)
   {
      _len += strlcpy(buf + _len, path_basename(content_get_subsystem_rom(j)), sizeof(buf) - _len);
      if (j != content_get_subsystem_rom_id() - 1)
         _len += strlcpy(buf + _len, "\n", sizeof(buf) - _len);
   }
   if (*buf)
      strlcpy(s, buf, len);
   return 0;
}

#ifdef HAVE_AUDIOMIXER
static int action_bind_sublabel_audio_mixer_stream(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   size_t _len;
   char msg[64];
   unsigned              offset = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN);
   audio_mixer_stream_t *stream = audio_driver_mixer_get_stream(offset);

   if (!stream)
      return -1;

   switch (stream->state)
   {
      case AUDIO_STREAM_STATE_NONE:
         strlcpy(msg,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE),
               sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_STOPPED:
         strlcpy(msg,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED),
               sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_PLAYING:
         strlcpy(msg,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING),
               sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
         strlcpy(msg,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED),
               sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         strlcpy(msg,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL),
               sizeof(msg));
         break;
   }

   _len = strlcpy(s, msg, len);
   snprintf(s + _len, len - _len, " | %s: %.2f dB",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME),
         stream->volume);
   return 0;
}
#endif

static int action_bind_sublabel_input_remap_port(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   size_t _len;
   unsigned user_idx     = 0;
   unsigned display_port = 0;
   menu_entry_t entry;

   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_LABEL_ENABLED;
   menu_entry_get(&entry, 0, i, NULL, false);

   /* We need the actual frontend port index.
    * This is difficult to obtain here - the only
    * way to get it is to parse the entry label
    * (input_remap_port_p<port_index+1>).
    *
    * The label format string is "input_remap_port_p%u"
    * (MENU_ENUM_LABEL_INPUT_REMAP_PORT_STR). Match the literal
    * prefix and then parse the trailing %u with strtoul to avoid
    * the per-call strlen()/malloc() overhead some libcs incur on
    * sscanf. */
   {
      static const char prefix[] = "input_remap_port_p";
      const char       *p;
      char             *endp;
      unsigned long     v;

      if (   !*entry.label
          || !string_starts_with_size(entry.label, prefix,
                STRLEN_CONST(prefix)))
         return 0;
      p = entry.label + STRLEN_CONST(prefix);
      v = strtoul(p, &endp, 10);
      if (endp == p)
         return 0;
      display_port = (unsigned)v;
      if (display_port >= MAX_USERS + 1)
         return 0;
   }

   _len = snprintf(s, len,
         msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT),
         display_port);

   if (display_port)
      user_idx = display_port - 1;

   /* Remove trailing dot */
   snprintf(s + _len - 1, len - _len - 1, ": %s",
           input_config_get_device_display_name(user_idx)
         ? input_config_get_device_display_name(user_idx)
         : (input_config_get_device_name(user_idx)
         ? input_config_get_device_name(user_idx)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)));

   /* We can safely cache the sublabel here, since
    * frontend port index cannot change while the
    * current menu is displayed */
   return 1;
}

#ifdef HAVE_CHEATS
static int action_bind_sublabel_cheat_desc(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   size_t _len          = 0;
   unsigned cheat_index = (type - MENU_SETTINGS_CHEAT_BEGIN);

   if (cheat_manager_state.cheats)
   {
      if (cheat_manager_state.cheats[cheat_index].handler == CHEAT_HANDLER_TYPE_EMU)
      {
         const char *code = cheat_manager_get_code(cheat_index);
         _len += strlcpy(s + _len,
               (code && *code)
                  ? code
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               len - _len);
      }
      else
         _len += snprintf(s + _len, len - _len, "%08X",
               cheat_manager_state.cheats[cheat_index].address);
   }

   return 0;
}
#endif

#ifdef HAVE_NETWORKING
static int action_bind_sublabel_netplay_room(file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   size_t _len;
   struct netplay_room *room;
   net_driver_state_t *net_st = networking_state_get_ptr();
   unsigned room_index        = type - MENU_SETTINGS_NETPLAY_ROOMS_START;
   if (room_index >= (unsigned)net_st->room_count)
      return -1;
   room  = &net_st->room_list[room_index];
   _len  = strlcpy(s, msg_hash_to_str(MSG_PROGRAM), len);
   _len += snprintf(s + _len, len - _len,
      ": %s (%s)\n"
      "%s: %s (%s)\n"
      "%s: %s ",
      (*room->retroarch_version)
      ? room->retroarch_version
      : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
      (*room->frontend
      && !string_is_equal_case_insensitive(room->frontend, "N/A"))
            ? room->frontend
            : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME),
      room->corename, room->coreversion,
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT),
      (*room->gamename
       && !string_is_equal_case_insensitive(room->gamename, "N/A"))
            ? room->gamename
            : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));

   if (     !*room->subsystem_name
         || string_is_equal_case_insensitive(room->subsystem_name, "N/A"))
      _len += snprintf(s + _len, len - _len, "(%08lX)\n",
            (unsigned long)(unsigned)room->gamecrc);
   else
   {
      _len += strlcpy(s + _len, "(", len - _len);
      _len += strlcpy(s + _len, room->subsystem_name, len - _len);
      _len += strlcpy(s + _len, ")\n", len - _len);
   }

   if (room->spectator_count > 0)
      _len += snprintf(s + _len, len - _len,
         msg_hash_to_str(MSG_NETPLAY_SPECTATORS_INFO),
         room->player_count, room->spectator_count);
   else if (room->player_count >= 0)
      _len += snprintf(s + _len, len - _len,
         msg_hash_to_str(MSG_NETPLAY_PLAYERS_INFO),
         room->player_count);
   return 0;
}

static int action_bind_sublabel_netplay_kick_client(file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   size_t _len;
   char buf[NAME_MAX_LENGTH];
   netplay_client_info_t *client;
   const char         *status = NULL;
   size_t             idx     = list->list[i].entry_idx;
   net_driver_state_t *net_st = networking_state_get_ptr();

   if (idx >= net_st->client_info_count)
      return -1;

   client = &net_st->client_info[idx];

   switch (client->mode)
   {
      case NETPLAY_CONNECTION_SLAVE:
      case NETPLAY_CONNECTION_PLAYING:
         status = msg_hash_to_str(MSG_NETPLAY_STATUS_PLAYING);
         break;
      case NETPLAY_CONNECTION_SPECTATING:
         status = msg_hash_to_str(MSG_NETPLAY_STATUS_SPECTATING);
         break;
      default:
         break;
   }

   s[0] = '\0';

   if (status)
   {
      _len        = strlcpy(buf, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STATUS),
            sizeof(buf) - 3);
      if (_len > sizeof(buf) - 4)
         _len = sizeof(buf) - 4;
      buf[  _len] = ':';
      buf[++_len] = ' ';
      buf[++_len] = '\0';
      strlcpy(buf + _len, status, sizeof(buf) - _len);
      _len        = strlen(buf);
      buf[  _len] = '\n';
      buf[++_len] = '\0';
      strlcpy(s, buf, len);
   }

   if (client->devices)
   {
      _len = snprintf(buf, sizeof(buf), "%s:",
         msg_hash_to_str(MSG_NETPLAY_CLIENT_DEVICES));

      if (_len > 0 && _len < sizeof(buf) - STRLEN_CONST(" 16\n"))
      {
         uint32_t device;
         char *buf_written = buf + _len;

         for (device = 0; device < (sizeof(client->devices) << 3); device++)
         {
            if (client->devices & (1 << device))
            {
               int dev_len = snprintf(buf_written,
                     sizeof(buf) - _len,
                     " %u,", (unsigned)(device + 1));

               if (dev_len <= 0)
               {
                  _len = -1;
                  break;
               }

               _len += dev_len;
               if (_len >= sizeof(buf) - 1)
                  break;

               buf_written += dev_len;
            }
         }

         if (_len > 0)
         {
            buf_written = strrchr(buf, ',');
            if (buf_written)
            {
               *buf_written++ = '\n';
               *buf_written   = '\0';

               _len = strlen(s);
               strlcpy(s + _len, buf, len - _len);
            }
         }
      }
   }

   snprintf(buf, sizeof(buf), "%s: %s\n",
      msg_hash_to_str(MSG_NETPLAY_CHAT_SUPPORTED),
      msg_hash_to_str((client->protocol >= 6) ?
         MENU_ENUM_LABEL_VALUE_YES : MENU_ENUM_LABEL_VALUE_NO));
   _len = strlen(s);
   if (_len < len - 1)
      strlcpy(s + _len, buf, len - _len);

   _len = strlen(s);
   if (_len < len - 1)
      _len += snprintf(s + _len, len - _len, "%s: %lu",
         msg_hash_to_str(MSG_NETPLAY_SLOWDOWNS_CAUSED),
         (unsigned long)client->slowdowns);

   if (client->ping >= 0 && _len < len - 1)
      snprintf(s + _len, len - _len,
            "\nPing: %u ms", (unsigned)client->ping);

   return 0;
}
#endif

static int action_bind_sublabel_playlist_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   size_t _len;
   struct menu_state    *menu_st             = menu_state_get_ptr();
   menu_list_t *menu_list                    = menu_st->entries.list;
   size_t list_size                          = MENU_LIST_GET_SELECTION(menu_list, 0)->size;
   playlist_t *playlist                      = NULL;
   const struct playlist_entry *entry        = NULL;
   size_t playlist_index                     = i;
#ifdef HAVE_OZONE
   const char *menu_ident                    = (menu_st->driver_ctx && menu_st->driver_ctx->ident) ? menu_st->driver_ctx->ident : NULL;
#endif
   settings_t *settings                      = config_get_ptr();
   bool playlist_show_sublabels              = settings->bools.playlist_show_sublabels;
   unsigned playlist_sublabel_runtime_type   = settings->uints.playlist_sublabel_runtime_type;
   bool content_runtime_log                  = settings->bools.content_runtime_log;
   bool content_runtime_log_aggregate        = settings->bools.content_runtime_log_aggregate;

   if (!playlist_show_sublabels)
      return 0;
#ifdef HAVE_OZONE
   if (string_is_equal(menu_ident, "ozone"))
      return 0;
#endif

   /* Get playlist index corresponding
    * to the current entry */
   if (!list || (i >= list_size))
      return 0;

   playlist_index = list->list[i].entry_idx;

   /* Get current playlist */
   playlist = playlist_get_cached();

   if (!playlist)
      return 0;

   if (playlist_index >= playlist_get_size(playlist))
      return 0;

   /* Read playlist entry */
   playlist_get_index(playlist, playlist_index, &entry);

   /* Only add sublabel if a core is currently assigned
    * > Both core name and core path must be valid */
   if (     (!entry->core_name || !*entry->core_name)
         || string_is_equal(entry->core_name, "DETECT")
         || (!entry->core_path || !*entry->core_path)
         || string_is_equal(entry->core_path, "DETECT"))
      return 0;

   /* Add core name */
   _len      = strlcpy(s,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE), len);
   s[  _len] =  ' ';
   s[++_len] =  '\0';
   _len     += strlcpy(s + _len, entry->core_name, len - _len);

   /* Get runtime info *if* required runtime log is enabled
    * *and* this is a valid playlist type */
   if (   ((playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_PER_CORE)
         && !content_runtime_log)
         || ((playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_AGGREGATE)
         && !content_runtime_log_aggregate))
      return 0;

   /* Note: This looks heavy, but each string_is_equal() call will
    * return almost immediately */
   if (   !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY))
       && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))
       && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST))
       && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB))
       && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST))
       && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)))
      return 0;

   /* Check whether runtime info should be loaded from log file */
   if (entry->runtime_status == PLAYLIST_RUNTIME_UNKNOWN)
      runtime_update_playlist(playlist, playlist_index);

   /* Check whether runtime info is valid */
   if (entry->runtime_status == PLAYLIST_RUNTIME_VALID)
   {
      size_t n = 0;
      char tmp[128];

      /* Runtime/last played strings are now cached in the
       * playlist, so we can add both in one go */
      tmp[  n] = '\n';
      tmp[++n] = '\0';
      n       += strlcpy(tmp + n, entry->runtime_str, sizeof(tmp) - n);

      if (n < 128 - 1)
      {
         tmp[  n] = '\n';
         tmp[++n] = '\0';
         strlcpy(tmp + n, entry->last_played_str, sizeof(tmp) - n);
      }

      if (*tmp)
         strlcpy(s + _len, tmp, len - _len);
   }

   return 0;
}

static int action_bind_sublabel_core_options(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   const char *category = path;
   const char *info     = NULL;

   /* If this is an options subcategory, fetch
    * the category info string */
   if (category && *category)
   {
      core_option_manager_t *coreopts = NULL;

      if (retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
         info = core_option_manager_get_category_info(
               coreopts, category);
   }

   /* If this isn't a subcategory (or something
    * went wrong...), use top level core options
    * menu sublabel */
   if (!info || !*info)
      info = msg_hash_to_str(MENU_ENUM_SUBLABEL_CORE_OPTIONS);

   if (info && *info)
   {
      strlcpy(s, info, len);
      return 1;
   }

   return 0;
}

static int action_bind_sublabel_core_option(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   core_option_manager_t *opt = NULL;
   if (retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &opt))
   {
      const char *info = core_option_manager_get_info(opt,
            type - MENU_SETTINGS_CORE_OPTION_START, true);
      if (info && *info)
         strlcpy(s, info, len);
   }
   return 0;
}

#ifdef HAVE_NETWORKING
static int action_bind_sublabel_core_updater_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   core_updater_list_t *core_list         = core_updater_list_get_cached();
   const core_updater_list_entry_t *entry = NULL;
   size_t _len =
      strlcpy(s,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES), len);
   s[  _len]   = ':';
   s[++_len]   = ' ';
   s[++_len]   = '\0';

   /* Search for specified core */
   if (   core_list
       && core_updater_list_get_filename(core_list, path, &entry)
       && entry->licenses_list)
   {
      unsigned i;
      /* Add license text */
      for (i = 0; i < entry->licenses_list->size; i++)
      {
         _len += strlcpy(s + _len, entry->licenses_list->elems[i].data, len - _len);
         if ((i + 1) < entry->licenses_list->size)
            _len += strlcpy(s + _len, ", ", len - _len);
      }
   }
   else /* No license found - set to N/A */
      strlcpy(s + _len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len - _len);

   return 1;
}
#endif

static int action_bind_sublabel_core_backup_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   /* crc is entered as 'alt' text */
   const char *crc = list->list[i].alt
      ? list->list[i].alt
      : list->list[i].path;
   size_t _len = 0;
   strlcpy_append(s, len, &_len,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_BACKUP_CRC));
   /* Add CRC string */
   if (!crc || !*crc)
      strlcpy_append(s, len, &_len, "00000000");
   else
      strlcpy_append(s, len, &_len, crc);
   return 1;
}

static int action_bind_sublabel_generic(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   return 0;
}

int menu_cbs_init_bind_sublabel(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, size_t lbl_len,
      unsigned type, size_t idx)
{
   unsigned i;
   typedef struct info_range_list
   {
      unsigned min;
      unsigned max;
      int (*cb)(file_list_t *list,
            unsigned type, unsigned i,
            const char *label, const char *path,
            char *s, size_t len);
   } info_range_list_t;

   static const info_range_list_t info_list[] = {
#ifdef HAVE_AUDIOMIXER
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_END,
         menu_action_sublabel_setting_audio_mixer_stream_play
      },
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_END,
         menu_action_sublabel_setting_audio_mixer_stream_play_looped
      },
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_END,
         menu_action_sublabel_setting_audio_mixer_stream_play_sequential
      },
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_END,
         menu_action_sublabel_setting_audio_mixer_stream_remove
      },
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_END,
         menu_action_sublabel_setting_audio_mixer_stream_stop
      },
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END,
         menu_action_sublabel_setting_audio_mixer_stream_volume
      },
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_END,
         action_bind_sublabel_audio_mixer_stream
      },
#endif
#ifdef HAVE_CHEATS
      {
         MENU_SETTINGS_CHEAT_BEGIN,
         MENU_SETTINGS_CHEAT_END,
         action_bind_sublabel_cheat_desc
      },
#endif
      {
         MENU_SETTINGS_CORE_OPTION_START,
         MENU_SETTINGS_CHEEVOS_START - 1,
         action_bind_sublabel_core_option
      },
      {
         MENU_SETTINGS_REMAPPING_PORT_BEGIN,
         MENU_SETTINGS_REMAPPING_PORT_END,
         action_bind_sublabel_user_remap_settings
      },

   };

   if (!cbs)
      return -1;

   BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_generic);

   for (i = 0; i < ARRAY_SIZE(info_list); i++)
   {
      if (type >= info_list[i].min && type <= info_list[i].max)
      {
         BIND_ACTION_SUBLABEL(cbs, info_list[i].cb);
         return 0;
      }
   }

   if (type == MENU_SETTINGS_INPUT_LIBRETRO_DEVICE)
   {
      BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_device_type);
      return 0;
   }
   else if (type == MENU_SETTINGS_INPUT_INPUT_REMAP_PORT)
   {
      BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_remap_port);
      return 0;
   }

   /* Hotkey binds require special handling */
   if ((cbs->enum_idx >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN) &&
       (cbs->enum_idx <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END))
   {
      unsigned bind_index = (unsigned)(cbs->enum_idx -
            MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN);

      if (bind_index < RARCH_BIND_LIST_END)
      {
         switch (input_config_bind_map_get_retro_key(bind_index))
         {
            case RARCH_ENABLE_HOTKEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_enable_hotkey);
               return 0;
            case RARCH_MENU_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_menu_toggle);
               return 0;
            case RARCH_QUIT_KEY:
#ifdef HAVE_LAKKA
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_restart_key);
#else
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_quit_key);
#endif
               return 0;
            case RARCH_CLOSE_CONTENT_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_close_content_key);
               return 0;
            case RARCH_RESET:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_reset);
               return 0;

            case RARCH_FAST_FORWARD_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_fast_forward_key);
               return 0;
            case RARCH_FAST_FORWARD_HOLD_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_fast_forward_hold_key);
               return 0;
            case RARCH_SLOWMOTION_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_slowmotion_key);
               return 0;
            case RARCH_SLOWMOTION_HOLD_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_slowmotion_hold_key);
               return 0;
            case RARCH_REWIND:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_rewind);
               return 0;
            case RARCH_PAUSE_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_pause_toggle);
               return 0;
            case RARCH_FRAMEADVANCE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_frameadvance);
               return 0;

            case RARCH_MUTE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_mute);
               return 0;
            case RARCH_VOLUME_UP:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_volume_up);
               return 0;
            case RARCH_VOLUME_DOWN:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_volume_down);
               return 0;

            case RARCH_LOAD_STATE_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_load_state_key);
               return 0;
            case RARCH_SAVE_STATE_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_save_state_key);
               return 0;
            case RARCH_STATE_SLOT_PLUS:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_state_slot_plus);
               return 0;
            case RARCH_STATE_SLOT_MINUS:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_state_slot_minus);
               return 0;

            case RARCH_PLAY_REPLAY_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_play_replay_key);
               return 0;
            case RARCH_RECORD_REPLAY_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_record_replay_key);
               return 0;
            case RARCH_HALT_REPLAY_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_halt_replay_key);
               return 0;
            case RARCH_SAVE_REPLAY_CHECKPOINT_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_save_replay_checkpoint_key);
               return 0;
            case RARCH_PREV_REPLAY_CHECKPOINT_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_prev_replay_checkpoint_key);
               return 0;
            case RARCH_NEXT_REPLAY_CHECKPOINT_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_next_replay_checkpoint_key);
               return 0;
            case RARCH_REPLAY_SLOT_PLUS:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_replay_slot_plus);
               return 0;
            case RARCH_REPLAY_SLOT_MINUS:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_replay_slot_minus);
               return 0;

            case RARCH_DISK_EJECT_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_disk_eject_toggle);
               return 0;
            case RARCH_DISK_NEXT:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_disk_next);
               return 0;
            case RARCH_DISK_PREV:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_disk_prev);
               return 0;

            case RARCH_SHADER_NEXT:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_shader_next);
               return 0;
            case RARCH_SHADER_PREV:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_shader_prev);
               return 0;
            case RARCH_SHADER_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_shader_toggle);
               return 0;
            case RARCH_SHADER_HOLD:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_shader_hold);
               return 0;

            case RARCH_CHEAT_TOGGLE:
#ifdef HAVE_CHEATS
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_cheat_toggle);
#endif
               return 0;
            case RARCH_CHEAT_INDEX_PLUS:
#ifdef HAVE_CHEATS
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_cheat_index_plus);
#endif
               return 0;
            case RARCH_CHEAT_INDEX_MINUS:
#ifdef HAVE_CHEATS
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_cheat_index_minus);
#endif
               return 0;

            case RARCH_SCREENSHOT:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_screenshot);
               return 0;
            case RARCH_RECORDING_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_recording_toggle);
               return 0;
            case RARCH_STREAMING_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_streaming_toggle);
               return 0;

            case RARCH_TURBO_FIRE_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_turbo_fire_toggle);
               return 0;
            case RARCH_GRAB_MOUSE_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_grab_mouse_toggle);
               return 0;
            case RARCH_GAME_FOCUS_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_game_focus_toggle);
               return 0;
            case RARCH_FULLSCREEN_TOGGLE_KEY:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_fullscreen_toggle_key);
               return 0;
            case RARCH_UI_COMPANION_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_ui_companion_toggle);
               return 0;

            case RARCH_VRR_RUNLOOP_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_vrr_runloop_toggle);
               return 0;
            case RARCH_RUNAHEAD_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_runahead_toggle);
               return 0;
            case RARCH_PREEMPT_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_preempt_toggle);
               return 0;
#ifdef HAVE_VIDEO_FILTER
            case RARCH_VIDEO_FILTER_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_video_filter_toggle);
               return 0;
#endif
            case RARCH_FPS_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_fps_toggle);
               return 0;
            case RARCH_STATISTICS_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_statistics_toggle);
               return 0;
            case RARCH_AI_SERVICE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_ai_service);
               return 0;

            case RARCH_NETPLAY_PING_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_netplay_ping_toggle);
               return 0;
            case RARCH_NETPLAY_HOST_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_netplay_host_toggle);
               return 0;
            case RARCH_NETPLAY_GAME_WATCH:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_netplay_game_watch);
               return 0;
            case RARCH_NETPLAY_PLAYER_CHAT:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_netplay_player_chat);
               return 0;
            case RARCH_NETPLAY_FADE_CHAT_TOGGLE:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_netplay_fade_chat_toggle);
               return 0;
            case RARCH_OSK:
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_osk);
               return 0;
            default:
               break;
         }
      }
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
         typedef struct sublabel_enum_map
   {
      uint32_t label;
      int (*cb)(file_list_t *list,
            unsigned type, unsigned i,
            const char *label, const char *path,
            char *s, size_t len);
   } sublabel_enum_map_t;

   /* The former thousand-case switch as data: one row per label,
    * platform guards carried inside; const so the whole table lives
    * in read-only, link-resolved storage. Cases with surrounding
    * guard scope or extra logic stay in the switch below. */
   static const sublabel_enum_map_t sublabel_map[] = {
      { MENU_ENUM_LABEL_FILE_BROWSER_CORE, menu_action_sublabel_file_browser_core },
      { MENU_ENUM_LABEL_CORE_MANAGER_ENTRY, menu_action_sublabel_file_browser_core },
      { MENU_ENUM_LABEL_CONTENTLESS_CORE, menu_action_sublabel_contentless_core },
      { MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING, action_bind_sublabel_quick_menu_start_streaming },
      { MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING, action_bind_sublabel_quick_menu_start_recording },
      { MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING, action_bind_sublabel_quick_menu_stop_streaming },
      { MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING, action_bind_sublabel_quick_menu_stop_recording },
      { MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS, action_bind_sublabel_quick_menu_override_options },
      { MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION, action_bind_sublabel_crt_switchres },
      { MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER, action_bind_sublabel_crt_switchres_super },
      { MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING, action_bind_sublabel_crt_switchres_x_axis_centering },
      { MENU_ENUM_LABEL_CRT_SWITCH_PORCH_ADJUST, action_bind_sublabel_crt_switchres_porch_adjust },
      { MENU_ENUM_LABEL_CRT_SWITCH_VERTICAL_ADJUST, action_bind_sublabel_crt_switchres_vertical_adjust },
      { MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE, action_bind_sublabel_crt_switchres_use_custom_refresh_rate },
      { MENU_ENUM_LABEL_CRT_SWITCH_HIRES_MENU, action_bind_sublabel_crt_switchres_hires_menu },
      { MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY, action_bind_sublabel_audio_resampler_quality },
      { MENU_ENUM_LABEL_AUDIO_FASTPATH_S16, action_bind_sublabel_audio_fastpath_s16 },
      { MENU_ENUM_LABEL_AUDIO_FORMAT_NEGOTIATION, action_bind_sublabel_audio_format_negotiation },
      { MENU_ENUM_LABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE, action_bind_sublabel_menu_thumbnail_background_enable },
      { MENU_ENUM_LABEL_SCREEN_RESOLUTION, action_bind_sublabel_screen_resolution },
      { MENU_ENUM_LABEL_VIDEO_USE_METAL_ARG_BUFFERS, action_bind_sublabel_video_use_metal_arg_buffers },
      { MENU_ENUM_LABEL_VIDEO_GPU_INDEX, action_bind_sublabel_video_gpu_index },
      { MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT, action_bind_sublabel_video_vp_custom_height },
      { MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH, action_bind_sublabel_video_vp_custom_width },
      { MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X, action_bind_sublabel_video_vp_custom_x },
      { MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y, action_bind_sublabel_video_vp_custom_y },
      { MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO, action_bind_sublabel_video_aspect_ratio },
      { MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X, action_bind_sublabel_video_vp_bias_x },
      { MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y, action_bind_sublabel_video_vp_bias_y },
      { MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, action_bind_sublabel_video_aspect_ratio_index },
      { MENU_ENUM_LABEL_CORE_INFORMATION, action_bind_sublabel_core_information },
      { MENU_ENUM_LABEL_DISC_INFORMATION, action_bind_sublabel_disc_information },
      { MENU_ENUM_LABEL_CONTENT_SETTINGS, action_bind_sublabel_quick_menu },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS, action_bind_sublabel_shader_parameters },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS, action_bind_sublabel_shader_preset_parameters },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_MANAGER, action_bind_sublabel_shader_preset_manager },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_FILE_INFO, action_bind_sublabel_shader_preset_file_info },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE, action_bind_sublabel_shader_preset_save_reference },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT, action_bind_sublabel_shader_preset_save_current },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS, action_bind_sublabel_shader_preset_save_as },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL, action_bind_sublabel_shader_preset_save_global },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE, action_bind_sublabel_shader_preset_save_core },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT, action_bind_sublabel_shader_preset_save_parent },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME, action_bind_sublabel_shader_preset_save_game },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL, action_bind_sublabel_shader_preset_remove_global },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE, action_bind_sublabel_shader_preset_remove_core },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT, action_bind_sublabel_shader_preset_remove_parent },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME, action_bind_sublabel_shader_preset_remove_game },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET, action_bind_sublabel_shader_preset },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND, action_bind_sublabel_shader_preset_prepend },
      { MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND, action_bind_sublabel_shader_preset_append },
      { MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES, action_bind_sublabel_shader_num_passes },
      { MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE, action_bind_sublabel_video_shaders_enable },
      { MENU_ENUM_LABEL_SHADER_APPLY_CHANGES, action_bind_sublabel_shader_apply_changes },
      { MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES, action_bind_sublabel_shader_watch_for_changes },
      { MENU_ENUM_LABEL_VIDEO_SHADER_REMEMBER_LAST_DIR, action_bind_sublabel_video_shader_remember_last_dir },
      { MENU_ENUM_LABEL_VIDEO_FONT_PATH, action_bind_sublabel_video_font_path },
      { MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY, action_bind_sublabel_recording_config_directory },
      { MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY, action_bind_sublabel_recording_output_directory },
      { MENU_ENUM_LABEL_VIDEO_SHADER_DIR, action_bind_sublabel_video_shader_directory },
      { MENU_ENUM_LABEL_AUDIO_FILTER_DIR, action_bind_sublabel_audio_filter_directory },
      { MENU_ENUM_LABEL_VIDEO_FILTER_DIR, action_bind_sublabel_video_filter_directory },
      { MENU_ENUM_LABEL_OVERLAY_DIRECTORY, action_bind_sublabel_overlay_directory },
      { MENU_ENUM_LABEL_OSK_OVERLAY_DIRECTORY, action_bind_sublabel_osk_overlay_directory },
      { MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY, action_bind_sublabel_screenshot_directory },
      { MENU_ENUM_LABEL_SAVEFILE_DIRECTORY, action_bind_sublabel_savefile_directory },
      { MENU_ENUM_LABEL_SAVESTATE_DIRECTORY, action_bind_sublabel_savestate_directory },
      { MENU_ENUM_LABEL_ASSETS_DIRECTORY, action_bind_sublabel_assets_directory },
      { MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY, action_bind_sublabel_database_directory },
      { MENU_ENUM_LABEL_CACHE_DIRECTORY, action_bind_sublabel_cache_directory },
      { MENU_ENUM_LABEL_PLAYLIST_DIRECTORY, action_bind_sublabel_playlists_directory },
      { MENU_ENUM_LABEL_CONTENT_FAVORITES_DIRECTORY, action_bind_sublabel_content_favorites_directory },
      { MENU_ENUM_LABEL_CONTENT_HISTORY_DIRECTORY, action_bind_sublabel_content_history_directory },
      { MENU_ENUM_LABEL_CONTENT_IMAGE_HISTORY_DIRECTORY, action_bind_sublabel_content_image_history_directory },
      { MENU_ENUM_LABEL_CONTENT_MUSIC_HISTORY_DIRECTORY, action_bind_sublabel_content_music_history_directory },
      { MENU_ENUM_LABEL_CONTENT_VIDEO_HISTORY_DIRECTORY, action_bind_sublabel_content_video_history_directory },
      { MENU_ENUM_LABEL_RUNTIME_LOG_DIRECTORY, action_bind_sublabel_runtime_log_directory },
      { MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR, action_bind_sublabel_joypad_autoconfig_directory },
      { MENU_ENUM_LABEL_LIBRETRO_INFO_PATH, action_bind_sublabel_core_info_directory },
      { MENU_ENUM_LABEL_LIBRETRO_DIR_PATH, action_bind_sublabel_core_directory },
      { MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY, action_bind_sublabel_core_assets_directory },
      { MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY, action_bind_sublabel_input_remapping_directory },
      { MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN, action_bind_sublabel_rgui_show_start_screen },
      { MENU_ENUM_LABEL_CONTENT_SHOW_ADD_ENTRY, action_bind_sublabel_menu_import_content_entry },
      { MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLISTS, action_bind_sublabel_menu_playlists_tab },
      { MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLIST_TABS, action_bind_sublabel_menu_playlist_tabs },
      { MENU_ENUM_LABEL_CONTENT_SHOW_EXPLORE, action_bind_sublabel_menu_explore_tab },
      { MENU_ENUM_LABEL_CONTENT_SHOW_CONTENTLESS_CORES, action_bind_sublabel_menu_contentless_cores_tab },
      { MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS, action_bind_sublabel_main_menu_enable_settings },
      { MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY, action_bind_sublabel_menu_history_tab },
      { MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS, action_bind_sublabel_menu_settings_tab },
      { MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS_PASSWORD, action_bind_sublabel_menu_settings_tab_enable_password },
      { MENU_ENUM_LABEL_GOTO_IMAGES, action_bind_sublabel_goto_images },
      { MENU_ENUM_LABEL_GOTO_MUSIC, action_bind_sublabel_goto_music },
      { MENU_ENUM_LABEL_GOTO_VIDEO, action_bind_sublabel_goto_video },
      { MENU_ENUM_LABEL_GOTO_EXPLORE, action_bind_sublabel_goto_explore },
      { MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES, action_bind_sublabel_goto_contentless_cores },
      { MENU_ENUM_LABEL_GOTO_FAVORITES, action_bind_sublabel_goto_favorites },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_DRIVERS, action_bind_sublabel_settings_show_drivers },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_VIDEO, action_bind_sublabel_settings_show_video },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_AUDIO, action_bind_sublabel_settings_show_audio },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_INPUT, action_bind_sublabel_settings_show_input },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_LATENCY, action_bind_sublabel_settings_show_latency },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_CORE, action_bind_sublabel_settings_show_core },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_CONFIGURATION, action_bind_sublabel_settings_show_configuration },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_SAVING, action_bind_sublabel_settings_show_saving },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_LOGGING, action_bind_sublabel_settings_show_logging },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_FILE_BROWSER, action_bind_sublabel_settings_show_file_browser },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_FRAME_THROTTLE, action_bind_sublabel_settings_show_frame_throttle },
      { MENU_ENUM_LABEL_FRAME_TIME_COUNTER_AUTO_RESET, action_bind_sublabel_frame_time_counter_auto_reset },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_RECORDING, action_bind_sublabel_settings_show_recording },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY, action_bind_sublabel_settings_show_onscreen_display },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_USER_INTERFACE, action_bind_sublabel_settings_show_user_interface },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_AI_SERVICE, action_bind_sublabel_settings_show_ai_service },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_ACCESSIBILITY, action_bind_sublabel_settings_show_accessibility },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_POWER_MANAGEMENT, action_bind_sublabel_settings_show_power_management },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_ACHIEVEMENTS, action_bind_sublabel_settings_show_achievements },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_NETWORK, action_bind_sublabel_settings_show_network },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_PLAYLISTS, action_bind_sublabel_settings_show_playlists },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_USER, action_bind_sublabel_settings_show_user },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_DIRECTORY, action_bind_sublabel_settings_show_directory },
      { MENU_ENUM_LABEL_SETTINGS_SHOW_STEAM, action_bind_sublabel_settings_show_steam },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESUME_CONTENT, action_bind_sublabel_quick_menu_show_resume_content },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESTART_CONTENT, action_bind_sublabel_quick_menu_show_restart_content },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_CLOSE_CONTENT, action_bind_sublabel_quick_menu_show_close_content },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT, action_bind_sublabel_quick_menu_show_take_screenshot },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU, action_bind_sublabel_quick_menu_show_savestate_submenu },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE, action_bind_sublabel_quick_menu_show_save_load_state },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_REPLAY, action_bind_sublabel_quick_menu_show_replay },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE, action_bind_sublabel_quick_menu_show_undo_save_load_state },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES, action_bind_sublabel_quick_menu_show_add_to_favorites },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST, action_bind_sublabel_quick_menu_show_add_to_playlist },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_RECORDING, action_bind_sublabel_quick_menu_show_start_recording },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_STREAMING, action_bind_sublabel_quick_menu_show_start_streaming },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION, action_bind_sublabel_quick_menu_show_set_core_association },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION, action_bind_sublabel_quick_menu_show_reset_core_association },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS, action_bind_sublabel_quick_menu_show_options },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH, action_bind_sublabel_quick_menu_show_core_options_flush },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS, action_bind_sublabel_quick_menu_show_controls },
      { MENU_ENUM_LABEL_CONTENT_SHOW_LATENCY, action_bind_sublabel_content_show_latency },
      { MENU_ENUM_LABEL_CONTENT_SHOW_REWIND, action_bind_sublabel_content_show_rewind },
      { MENU_ENUM_LABEL_CONTENT_SHOW_OVERLAYS, action_bind_sublabel_content_show_overlays },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS, action_bind_sublabel_quick_menu_show_shaders },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES, action_bind_sublabel_quick_menu_show_save_core_overrides },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES, action_bind_sublabel_quick_menu_show_save_content_dir_overrides },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES, action_bind_sublabel_quick_menu_show_save_game_overrides },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_INFORMATION, action_bind_sublabel_quick_menu_show_information },
      { MENU_ENUM_LABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS, action_bind_sublabel_quick_menu_show_download_thumbnails },
      { MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE, action_bind_sublabel_menu_enable_kiosk_mode },
      { MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE, action_bind_sublabel_menu_disable_kiosk_mode },
      { MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD, action_bind_sublabel_menu_kiosk_mode_password },
      { MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES, action_bind_sublabel_menu_favorites_tab },
      { MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES_FIRST, action_bind_sublabel_menu_favorites_first },
      { MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES, action_bind_sublabel_menu_images_tab },
      { MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC, action_bind_sublabel_menu_music_tab },
      { MENU_ENUM_LABEL_MENU_SHOW_LOAD_CORE, action_bind_sublabel_menu_show_load_core },
      { MENU_ENUM_LABEL_LOAD_DISC, action_bind_sublabel_load_disc },
      { MENU_ENUM_LABEL_DUMP_DISC, action_bind_sublabel_dump_disc },
      { MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT, action_bind_sublabel_menu_show_load_content },
      { MENU_ENUM_LABEL_MENU_SHOW_LOAD_DISC, action_bind_sublabel_menu_show_load_disc },
      { MENU_ENUM_LABEL_MENU_SHOW_DUMP_DISC, action_bind_sublabel_menu_show_dump_disc },
      { MENU_ENUM_LABEL_MENU_SHOW_INFORMATION, action_bind_sublabel_menu_show_information },
      { MENU_ENUM_LABEL_MENU_SHOW_CONFIGURATIONS, action_bind_sublabel_menu_show_configurations },
      { MENU_ENUM_LABEL_MENU_SHOW_HELP, action_bind_sublabel_menu_show_help },
      { MENU_ENUM_LABEL_MENU_SHOW_QUIT_RETROARCH, action_bind_sublabel_menu_show_quit_retroarch },
      { MENU_ENUM_LABEL_MENU_SHOW_REBOOT, action_bind_sublabel_menu_show_reboot },
      { MENU_ENUM_LABEL_MENU_SHOW_SHUTDOWN, action_bind_sublabel_menu_show_shutdown },
      { MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER, action_bind_sublabel_menu_show_online_updater },
      { MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER, action_bind_sublabel_menu_show_core_updater },
      { MENU_ENUM_LABEL_MENU_SCROLL_FAST, action_bind_sublabel_menu_scroll_fast },
      { MENU_ENUM_LABEL_MENU_SCROLL_DELAY, action_bind_sublabel_menu_scroll_delay },
      { MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY, action_bind_sublabel_menu_netplay_tab },
      { MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO, action_bind_sublabel_menu_video_tab },
      { MENU_ENUM_LABEL_OZONE_FONT, action_bind_sublabel_menu_ozone_font },
      { MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY, action_bind_sublabel_menu_framebuffer_opacity },
      { MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION, action_bind_sublabel_menu_horizontal_animation },
      { MENU_ENUM_LABEL_MENU_SCALE_FACTOR, action_bind_sublabel_menu_scale_factor },
      { MENU_ENUM_LABEL_MENU_WIDGET_SCALE_AUTO, action_bind_sublabel_menu_widget_scale_auto },
      { MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR, action_bind_sublabel_menu_widget_scale_factor },
      { MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY, action_bind_sublabel_menu_wallpaper_opacity },
      { MENU_ENUM_LABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME, action_bind_sublabel_menu_use_preferred_system_color_theme },
      { MENU_ENUM_LABEL_DISK_IMAGE_APPEND, action_bind_sublabel_disk_image_append },
      { MENU_ENUM_LABEL_SUBSYSTEM_ADD, action_bind_sublabel_subsystem_add },
      { MENU_ENUM_LABEL_SUBSYSTEM_LOAD, action_bind_sublabel_subsystem_load },
      { MENU_ENUM_LABEL_DISK_TRAY_EJECT, action_bind_sublabel_disk_tray_eject },
      { MENU_ENUM_LABEL_DISK_TRAY_INSERT, action_bind_sublabel_disk_tray_insert },
      { MENU_ENUM_LABEL_DISK_INDEX, action_bind_sublabel_disk_index },
      { MENU_ENUM_LABEL_DISK_OPTIONS, action_bind_sublabel_disk_options },
      { MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE, action_bind_sublabel_input_latency_frames_range },
      { MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN, action_bind_sublabel_input_latency_frames },
      { MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY, action_bind_rgui_config_directory },
      { MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY, action_bind_thumbnails_directory },
      { MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY, action_bind_dynamic_wallpapers_directory },
      { MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY, action_bind_sublabel_rgui_browser_directory },
      { MENU_ENUM_LABEL_SYSTEM_DIRECTORY, action_bind_sublabel_system_directory },
      { MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME, action_bind_sublabel_playlist_entry_rename },
      { MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE, action_bind_sublabel_playlist_entry_remove },
      { MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE, action_bind_sublabel_threaded_data_runloop_enable },
      { MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS, action_bind_sublabel_show_advanced_settings },
      { MENU_ENUM_LABEL_SAVESTATE_LIST, action_bind_sublabel_savestate_list },
      { MENU_ENUM_LABEL_STATE_SLOT_RUN, action_bind_sublabel_load_state },
      { MENU_ENUM_LABEL_CORE_OPTIONS, action_bind_sublabel_core_options },
      { MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_LIST, action_bind_sublabel_core_option_override_list },
      { MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_INFO, action_bind_sublabel_core_option_override_info },
      { MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE, action_bind_sublabel_core_options_game_specific_create },
      { MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE, action_bind_sublabel_core_options_game_specific_remove },
      { MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE, action_bind_sublabel_core_options_folder_specific_create },
      { MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE, action_bind_sublabel_core_options_folder_specific_remove },
      { MENU_ENUM_LABEL_CORE_OPTIONS_RESET, action_bind_sublabel_core_options_reset },
      { MENU_ENUM_LABEL_CORE_OPTIONS_FLUSH, action_bind_sublabel_core_options_flush },
      { MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS, action_bind_sublabel_core_input_remapping_options },
      { MENU_ENUM_LABEL_REMAP_FILE_MANAGER_LIST, action_bind_sublabel_remap_file_manager_list },
      { MENU_ENUM_LABEL_REMAP_FILE_INFO, action_bind_sublabel_remap_file_info },
      { MENU_ENUM_LABEL_REMAP_FILE_LOAD, action_bind_sublabel_remap_file_load },
      { MENU_ENUM_LABEL_REMAP_FILE_SAVE_AS, action_bind_sublabel_remap_file_save_as },
      { MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME, action_bind_sublabel_remap_file_save_game },
      { MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR, action_bind_sublabel_remap_file_save_content_dir },
      { MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE, action_bind_sublabel_remap_file_save_core },
      { MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME, action_bind_sublabel_remap_file_remove_game },
      { MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR, action_bind_sublabel_remap_file_remove_content_dir },
      { MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE, action_bind_sublabel_remap_file_remove_core },
      { MENU_ENUM_LABEL_REMAP_FILE_RESET, action_bind_sublabel_remap_file_reset },
      { MENU_ENUM_LABEL_REMAP_FILE_FLUSH, action_bind_sublabel_remap_file_flush },
      { MENU_ENUM_LABEL_OVERRIDE_FILE_INFO, action_bind_sublabel_override_file_info },
      { MENU_ENUM_LABEL_OVERRIDE_FILE_LOAD, action_bind_sublabel_override_file_load },
      { MENU_ENUM_LABEL_OVERRIDE_FILE_SAVE_AS, action_bind_sublabel_override_file_save_as },
      { MENU_ENUM_LABEL_OVERRIDE_UNLOAD, action_bind_sublabel_override_unload },
      { MENU_ENUM_LABEL_SHADER_OPTIONS, action_bind_sublabel_shader_options },
      { MENU_ENUM_LABEL_CONFIGURATIONS, action_bind_sublabel_load_config },
      { MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG, action_bind_sublabel_save_current_config },
      { MENU_ENUM_LABEL_SAVE_NEW_CONFIG, action_bind_sublabel_save_new_config },
      { MENU_ENUM_LABEL_SAVE_AS_CONFIG, action_bind_sublabel_save_as_config },
      { MENU_ENUM_LABEL_SAVE_MAIN_CONFIG, action_bind_sublabel_save_main_config },
      { MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG, action_bind_sublabel_reset_to_default_config },
      { MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME, action_bind_sublabel_save_current_config_override_game },
      { MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR, action_bind_sublabel_save_current_config_override_content_dir },
      { MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE, action_bind_sublabel_save_current_config_override_core },
      { MENU_ENUM_LABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME, action_bind_sublabel_remove_current_config_override_game },
      { MENU_ENUM_LABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR, action_bind_sublabel_remove_current_config_override_content_dir },
      { MENU_ENUM_LABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE, action_bind_sublabel_remove_current_config_override_core },
      { MENU_ENUM_LABEL_RESTART_CONTENT, action_bind_sublabel_restart_content },
      { MENU_ENUM_LABEL_ACCOUNTS_LIST, action_bind_sublabel_accounts_list },
      { MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS, action_bind_sublabel_accounts_retro_achievements },
      { MENU_ENUM_LABEL_UNDO_SAVE_STATE, action_bind_sublabel_undo_save_state },
      { MENU_ENUM_LABEL_UNDO_LOAD_STATE, action_bind_sublabel_undo_load_state },
      { MENU_ENUM_LABEL_STATE_SLOT, action_bind_sublabel_state_slot },
      { MENU_ENUM_LABEL_REPLAY_SLOT, action_bind_sublabel_replay_slot },
      { MENU_ENUM_LABEL_RESUME_CONTENT, action_bind_sublabel_resume_content },
      { MENU_ENUM_LABEL_SAVE_STATE, action_bind_sublabel_save_state },
      { MENU_ENUM_LABEL_LOAD_STATE, action_bind_sublabel_load_state },
      { MENU_ENUM_LABEL_HALT_REPLAY, action_bind_sublabel_halt_replay },
      { MENU_ENUM_LABEL_RECORD_REPLAY, action_bind_sublabel_record_replay },
      { MENU_ENUM_LABEL_PLAY_REPLAY, action_bind_sublabel_play_replay },
      { MENU_ENUM_LABEL_CLOSE_CONTENT, action_bind_sublabel_close_content },
      { MENU_ENUM_LABEL_TAKE_SCREENSHOT, action_bind_sublabel_take_screenshot },
      { MENU_ENUM_LABEL_CURSOR_MANAGER, action_bind_sublabel_cursor_manager },
      { MENU_ENUM_LABEL_CURSOR_MANAGER_LIST, action_bind_sublabel_cursor_manager },
      { MENU_ENUM_LABEL_DATABASE_MANAGER, action_bind_sublabel_database_manager },
      { MENU_ENUM_LABEL_DATABASE_MANAGER_LIST, action_bind_sublabel_database_manager },
      { MENU_ENUM_LABEL_CORE_ENABLE, action_bind_sublabel_core_enable },
      { MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS, action_bind_sublabel_game_specific_options },
      { MENU_ENUM_LABEL_GLOBAL_CORE_OPTIONS, action_bind_sublabel_global_core_options },
      { MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE, action_bind_sublabel_auto_overrides_enable },
      { MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE, action_bind_sublabel_auto_remaps_enable },
      { MENU_ENUM_LABEL_INITIAL_DISK_CHANGE_ENABLE, action_bind_sublabel_initial_disk_change_enable },
      { MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS, action_bind_sublabel_menu_filebrowser_settings },
      { MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS, action_bind_sublabel_menu_filebrowser_open_uwp_permissions },
      { MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER, action_bind_sublabel_menu_filebrowser_open_picker },
      { MENU_ENUM_LABEL_ADD_TO_FAVORITES, action_bind_sublabel_add_to_favorites },
      { MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST, action_bind_sublabel_add_to_favorites },
      { MENU_ENUM_LABEL_ADD_TO_PLAYLIST, action_bind_sublabel_add_to_playlist },
      { MENU_ENUM_LABEL_ADD_TO_PLAYLIST_QUICKMENU, action_bind_sublabel_add_to_playlist },
      { MENU_ENUM_LABEL_SET_CORE_ASSOCIATION, action_bind_sublabel_set_core_association },
      { MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION, action_bind_sublabel_reset_core_association },
      { MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS, action_bind_sublabel_download_pl_entry_thumbnails },
      { MENU_ENUM_LABEL_RUN, action_bind_sublabel_run },
      { MENU_ENUM_LABEL_INFORMATION, action_bind_sublabel_information },
      { MENU_ENUM_LABEL_RENAME_ENTRY, action_bind_sublabel_rename_entry },
      { MENU_ENUM_LABEL_DELETE_ENTRY, action_bind_sublabel_delete_entry },
      { MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS, action_bind_sublabel_netplay_refresh_rooms },
      { MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN, action_bind_sublabel_netplay_refresh_lan },
      { MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE, action_bind_sublabel_core_updater_auto_extract_archive },
      { MENU_ENUM_LABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES, action_bind_sublabel_core_updater_show_experimental_cores },
      { MENU_ENUM_LABEL_CORE_UPDATER_AUTO_BACKUP, action_bind_sublabel_core_updater_auto_backup },
      { MENU_ENUM_LABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE, action_bind_sublabel_core_updater_auto_backup_history_size },
      { MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL, action_bind_sublabel_core_updater_buildbot_url },
      { MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL, action_bind_sublabel_core_updater_buildbot_assets_url },
      { MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE, action_bind_sublabel_sort_savefiles_enable },
      { MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE, action_bind_sublabel_sort_savestates_enable },
      { MENU_ENUM_LABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE, action_bind_sublabel_sort_savefiles_by_content_enable },
      { MENU_ENUM_LABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE, action_bind_sublabel_sort_savestates_by_content_enable },
      { MENU_ENUM_LABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE, action_bind_sublabel_sort_screenshots_by_content_enable },
      { MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE, action_bind_sublabel_savefiles_in_content_dir_enable },
      { MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE, action_bind_sublabel_savestates_in_content_dir_enable },
      { MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE, action_bind_sublabel_screenshots_in_content_dir_enable },
      { MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE, action_bind_sublabel_systemfiles_in_content_dir_enable },
      { MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL, action_bind_sublabel_video_swap_interval },
      { MENU_ENUM_LABEL_SCAN_FILE, action_bind_sublabel_scan_file },
      { MENU_ENUM_LABEL_SCAN_DIRECTORY, action_bind_sublabel_scan_directory },
      { MENU_ENUM_LABEL_SCAN_METHOD, action_bind_sublabel_scan_method },
      { MENU_ENUM_LABEL_SCAN_USE_DB, action_bind_sublabel_scan_use_db },
      { MENU_ENUM_LABEL_SCAN_DB_SELECT, action_bind_sublabel_scan_db_select },
      { MENU_ENUM_LABEL_SCAN_TARGET_PLAYLIST, action_bind_sublabel_scan_target_playlist },
      { MENU_ENUM_LABEL_SCAN_SINGLE_FILE, action_bind_sublabel_scan_single_file },
      { MENU_ENUM_LABEL_SCAN_OMIT_DB_REF, action_bind_sublabel_scan_omit_db_ref },
      { MENU_ENUM_LABEL_NETPLAY_KICK, action_bind_sublabel_netplay_kick },
      { MENU_ENUM_LABEL_NETPLAY_BAN, action_bind_sublabel_netplay_ban },
      { MENU_ENUM_LABEL_NETPLAY_DISCONNECT, action_bind_sublabel_netplay_disconnect },
      { MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT, action_bind_sublabel_netplay_enable_client },
      { MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST, action_bind_sublabel_netplay_enable_host },
      { MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND, action_bind_sublabel_navigation_wraparound },
      { MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE, action_bind_sublabel_battery_level_enable },
      { MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS, action_bind_sublabel_menu_show_sublabels },
      { MENU_ENUM_LABEL_MENU_SHOW_CONFIRM, action_bind_sublabel_menu_show_confirm },
      { MENU_ENUM_LABEL_TIMEDATE_ENABLE, action_bind_sublabel_timedate_enable },
      { MENU_ENUM_LABEL_TIMEDATE_STYLE, action_bind_sublabel_timedate_style },
      { MENU_ENUM_LABEL_TIMEDATE_DATE_SEPARATOR, action_bind_sublabel_timedate_date_separator },
      { MENU_ENUM_LABEL_ICON_THUMBNAILS, action_bind_sublabel_icon_thumbnails },
      { MENU_ENUM_LABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD, action_bind_sublabel_menu_thumbnail_upscale_threshold },
      { MENU_ENUM_LABEL_MOUSE_ENABLE, action_bind_sublabel_mouse_enable },
      { MENU_ENUM_LABEL_POINTER_ENABLE, action_bind_sublabel_pointer_enable },
      { MENU_ENUM_LABEL_STDIN_CMD_ENABLE, action_bind_sublabel_stdin_cmd_enable },
      { MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE, action_bind_sublabel_netplay_public_announce },
      { MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL, action_bind_sublabel_netplay_nat_traversal },
      { MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES, action_bind_sublabel_netplay_check_frames },
      { MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR, action_bind_sublabel_netplay_start_as_spectator },
      { MENU_ENUM_LABEL_NETPLAY_FADE_CHAT, action_bind_sublabel_netplay_fade_chat },
      { MENU_ENUM_LABEL_NETPLAY_CHAT_COLOR_NAME, action_bind_sublabel_netplay_chat_color_name },
      { MENU_ENUM_LABEL_NETPLAY_CHAT_COLOR_MSG, action_bind_sublabel_netplay_chat_color_msg },
      { MENU_ENUM_LABEL_NETPLAY_ALLOW_PAUSING, action_bind_sublabel_netplay_allow_pausing },
      { MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES, action_bind_sublabel_netplay_allow_slaves },
      { MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES, action_bind_sublabel_netplay_require_slaves },
      { MENU_ENUM_LABEL_NETPLAY_PASSWORD, action_bind_sublabel_netplay_password },
      { MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD, action_bind_sublabel_netplay_spectate_password },
      { MENU_ENUM_LABEL_NETPLAY_MAX_PING, action_bind_sublabel_netplay_max_ping },
      { MENU_ENUM_LABEL_NETPLAY_MAX_CONNECTIONS, action_bind_sublabel_netplay_max_connections },
      { MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT, action_bind_sublabel_netplay_tcp_udp_port },
      { MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS, action_bind_sublabel_netplay_ip_address },
      { MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED, action_bind_sublabel_overlay_autoload_preferred },
      { MENU_ENUM_LABEL_OVERLAY_PRESET, action_bind_sublabel_overlay_preset },
      { MENU_ENUM_LABEL_OSK_OVERLAY_PRESET, action_bind_sublabel_osk_overlay_preset },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE, action_bind_sublabel_overlay_enable },
      { MENU_ENUM_LABEL_OVERLAY_OPACITY, action_bind_sublabel_overlay_opacity },
      { MENU_ENUM_LABEL_OSK_OVERLAY_OPACITY, action_bind_sublabel_osk_overlay_opacity },
      { MENU_ENUM_LABEL_OVERLAY_SCALE_LANDSCAPE, action_bind_sublabel_overlay_scale_landscape },
      { MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE, action_bind_sublabel_overlay_aspect_adjust_landscape },
      { MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_LANDSCAPE, action_bind_sublabel_overlay_x_separation_landscape },
      { MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_LANDSCAPE, action_bind_sublabel_overlay_y_separation_landscape },
      { MENU_ENUM_LABEL_OVERLAY_X_OFFSET_LANDSCAPE, action_bind_sublabel_overlay_x_offset_landscape },
      { MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_LANDSCAPE, action_bind_sublabel_overlay_y_offset_landscape },
      { MENU_ENUM_LABEL_OVERLAY_SCALE_PORTRAIT, action_bind_sublabel_overlay_scale_portrait },
      { MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT, action_bind_sublabel_overlay_aspect_adjust_portrait },
      { MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_PORTRAIT, action_bind_sublabel_overlay_x_separation_portrait },
      { MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_PORTRAIT, action_bind_sublabel_overlay_y_separation_portrait },
      { MENU_ENUM_LABEL_OVERLAY_X_OFFSET_PORTRAIT, action_bind_sublabel_overlay_x_offset_portrait },
      { MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_PORTRAIT, action_bind_sublabel_overlay_y_offset_portrait },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_POINTER_ENABLE, action_bind_sublabel_input_overlay_pointer_enable },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_LIGHTGUN_PORT, action_bind_sublabel_input_overlay_lightgun_port },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH, action_bind_sublabel_input_overlay_lightgun_trigger_on_touch },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY, action_bind_sublabel_input_overlay_lightgun_trigger_delay },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN, action_bind_sublabel_input_overlay_lightgun_allow_offscreen },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT, action_bind_sublabel_input_overlay_lightgun_two_touch_input },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT, action_bind_sublabel_input_overlay_lightgun_three_touch_input },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT, action_bind_sublabel_input_overlay_lightgun_four_touch_input },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_MOUSE_SPEED, action_bind_sublabel_input_overlay_mouse_speed },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG, action_bind_sublabel_input_overlay_mouse_hold_to_drag },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC, action_bind_sublabel_input_overlay_mouse_hold_msec },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG, action_bind_sublabel_input_overlay_mouse_dtap_to_drag },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC, action_bind_sublabel_input_overlay_mouse_dtap_msec },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD, action_bind_sublabel_input_overlay_mouse_swipe_threshold },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT, action_bind_sublabel_input_overlay_mouse_alt_two_touch_input },
      { MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN, action_bind_sublabel_audio_dsp_plugin },
      { MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN_REMOVE, action_bind_sublabel_audio_dsp_plugin_remove },
      { MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE, action_bind_sublabel_audio_output_rate },
      { MENU_ENUM_LABEL_AUDIO_DEVICE, action_bind_sublabel_audio_device },
      { MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE, action_bind_sublabel_audio_wasapi_exclusive_mode },
      { MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH, action_bind_sublabel_audio_wasapi_sh_buffer_length },
      { MENU_ENUM_LABEL_MENU_WALLPAPER, action_bind_sublabel_wallpaper },
      { MENU_ENUM_LABEL_DYNAMIC_WALLPAPER, action_bind_sublabel_dynamic_wallpaper },
      { MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE, action_bind_sublabel_filter_supported_extensions },
      { MENU_ENUM_LABEL_FILTER_BY_CURRENT_CORE, action_bind_sublabel_filter_by_current_core },
      { MENU_ENUM_LABEL_WIFI_DRIVER, action_bind_sublabel_wifi_driver },
      { MENU_ENUM_LABEL_RECORD_DRIVER, action_bind_sublabel_record_driver },
      { MENU_ENUM_LABEL_MIDI_DRIVER, action_bind_sublabel_midi_driver },
      { MENU_ENUM_LABEL_MENU_DRIVER, action_bind_sublabel_menu_driver },
      { MENU_ENUM_LABEL_LOCATION_DRIVER, action_bind_sublabel_location_driver },
      { MENU_ENUM_LABEL_CAMERA_DRIVER, action_bind_sublabel_camera_driver },
      { MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER, action_bind_sublabel_audio_resampler_driver },
      { MENU_ENUM_LABEL_JOYPAD_DRIVER, action_bind_sublabel_joypad_driver },
      { MENU_ENUM_LABEL_INPUT_DRIVER, action_bind_sublabel_input_driver },
      { MENU_ENUM_LABEL_AUDIO_DRIVER, action_bind_sublabel_audio_driver },
      { MENU_ENUM_LABEL_VIDEO_DRIVER, action_bind_sublabel_video_driver },
      { MENU_ENUM_LABEL_PAUSE_LIBRETRO, action_bind_sublabel_pause_libretro },
      { MENU_ENUM_LABEL_MENU_SAVESTATE_RESUME, action_bind_sublabel_menu_savestate_resume },
      { MENU_ENUM_LABEL_MENU_INSERT_DISK_RESUME, action_bind_sublabel_menu_insert_disk_resume },
      { MENU_ENUM_LABEL_QUIT_ON_CLOSE_CONTENT, action_bind_sublabel_quit_on_close_content },
      { MENU_ENUM_LABEL_MENU_SCREENSAVER_TIMEOUT, action_bind_sublabel_menu_screensaver_timeout },
      { MENU_ENUM_LABEL_MENU_REMEMBER_SELECTION, action_bind_sublabel_menu_remember_selection },
      { MENU_ENUM_LABEL_MENU_STARTUP_PAGE, action_bind_sublabel_menu_startup_page },
      { MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL, action_bind_sublabel_input_swap_ok_cancel },
      { MENU_ENUM_LABEL_MENU_INPUT_SWAP_SCROLL, action_bind_sublabel_input_swap_scroll },
      { MENU_ENUM_LABEL_MENU_SINGLECLICK_PLAYLISTS, action_bind_sublabel_menu_singleclick_playlists },
      { MENU_ENUM_LABEL_MENU_ALLOW_TABS_BACK, action_bind_sublabel_menu_allow_tabs_back },
      { MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU, action_bind_sublabel_input_all_users_control_menu },
      { MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE, action_bind_sublabel_input_autodetect_enable },
      { MENU_ENUM_LABEL_INPUT_SENSORS_ENABLE, action_bind_sublabel_input_sensors_enable },
      { MENU_ENUM_LABEL_INPUT_AUTO_MOUSE_GRAB, action_bind_sublabel_input_auto_mouse_grab },
      { MENU_ENUM_LABEL_INPUT_AUTO_GAME_FOCUS, action_bind_sublabel_input_auto_game_focus },
      { MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE, action_bind_sublabel_input_remap_binds_enable },
      { MENU_ENUM_LABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE, action_bind_sublabel_input_remap_sort_by_controller_enable },
      { MENU_ENUM_LABEL_AUTOSAVE_INTERVAL, action_bind_sublabel_autosave_interval },
      { MENU_ENUM_LABEL_SAVESTATE_AUTOMATIC_INTERVAL, action_bind_sublabel_savestate_automatic_interval },
      { MENU_ENUM_LABEL_REPLAY_CHECKPOINT_INTERVAL, action_bind_sublabel_replay_checkpoint_interval },
      { MENU_ENUM_LABEL_REPLAY_CHECKPOINT_DESERIALIZE, action_bind_sublabel_replay_checkpoint_deserialize },
      { MENU_ENUM_LABEL_SAVESTATE_MAX_KEEP, action_bind_sublabel_savestate_max_keep },
      { MENU_ENUM_LABEL_REPLAY_MAX_KEEP, action_bind_sublabel_replay_max_keep },
      { MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE, action_bind_sublabel_savestate_thumbnail_enable },
      { MENU_ENUM_LABEL_SAVE_FILE_COMPRESSION, action_bind_sublabel_save_file_compression },
      { MENU_ENUM_LABEL_SAVESTATE_FILE_COMPRESSION, action_bind_sublabel_savestate_file_compression },
      { MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE, action_bind_sublabel_savestate_auto_save },
      { MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD, action_bind_sublabel_savestate_auto_load },
      { MENU_ENUM_LABEL_PERFCNT_ENABLE, action_bind_sublabel_perfcnt_enable },
      { MENU_ENUM_LABEL_FRONTEND_LOG_LEVEL, action_bind_sublabel_frontend_log_level },
      { MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL, action_bind_sublabel_libretro_log_level },
      { MENU_ENUM_LABEL_REWIND_SETTINGS, action_bind_sublabel_rewind_settings },
      { MENU_ENUM_LABEL_REWIND_ENABLE, action_bind_sublabel_rewind_enable },
      { MENU_ENUM_LABEL_REWIND_GRANULARITY, action_bind_sublabel_rewind_granularity },
      { MENU_ENUM_LABEL_REWIND_BUFFER_SIZE, action_bind_sublabel_rewind_buffer_size },
      { MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP, action_bind_sublabel_rewind_buffer_size_step },
      { MENU_ENUM_LABEL_SLOWMOTION_RATIO, action_bind_sublabel_slowmotion_ratio },
      { MENU_ENUM_LABEL_RUN_AHEAD_UNSUPPORTED, action_bind_sublabel_run_ahead_unsupported },
      { MENU_ENUM_LABEL_RUNAHEAD_MODE, action_bind_sublabel_runahead_mode },
      { MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS, action_bind_sublabel_run_ahead_hide_warnings },
      { MENU_ENUM_LABEL_RUN_AHEAD_FRAMES, action_bind_sublabel_run_ahead_frames },
      { MENU_ENUM_LABEL_PREEMPT_FRAMES, action_bind_sublabel_preempt_frames },
      { MENU_ENUM_LABEL_INPUT_BLOCK_TIMEOUT, action_bind_sublabel_input_block_timeout },
      { MENU_ENUM_LABEL_FASTFORWARD_RATIO, action_bind_sublabel_fastforward_ratio },
      { MENU_ENUM_LABEL_FASTFORWARD_FRAMESKIP, action_bind_sublabel_fastforward_frameskip },
      { MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE, action_bind_sublabel_vrr_runloop_enable },
      { MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE, action_bind_sublabel_menu_throttle_framerate },
      { MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE, action_bind_sublabel_block_sram_overwrite },
      { MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX, action_bind_sublabel_savestate_auto_index },
      { MENU_ENUM_LABEL_REPLAY_AUTO_INDEX, action_bind_sublabel_replay_auto_index },
      { MENU_ENUM_LABEL_VIDEO_GPU_RECORD, action_bind_sublabel_video_gpu_record },
      { MENU_ENUM_LABEL_VIDEO_FULLSCREEN, action_bind_sublabel_video_fullscreen },
      { MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN, action_bind_sublabel_video_windowed_fullscreen },
      { MENU_ENUM_LABEL_VIDEO_AUTOSWITCH_REFRESH_RATE, action_bind_sublabel_video_autoswitch_refresh_rate },
      { MENU_ENUM_LABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD, action_bind_sublabel_video_autoswitch_pal_threshold },
      { MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE, action_bind_sublabel_video_force_srgb_enable },
      { MENU_ENUM_LABEL_VIDEO_ROTATION, action_bind_sublabel_video_rotation },
      { MENU_ENUM_LABEL_SCREEN_ORIENTATION, action_bind_sublabel_screen_orientation },
      { MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT, action_bind_sublabel_video_gpu_screenshot },
      { MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER, action_bind_sublabel_video_scale_integer },
      { MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER_AXIS, action_bind_sublabel_video_scale_integer_axis },
      { MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER_SCALING, action_bind_sublabel_video_scale_integer_scaling },
      { MENU_ENUM_LABEL_PLAYLISTS_TAB, action_bind_sublabel_content_collection_list },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_BEHIND_MENU, action_bind_sublabel_input_overlay_behind_menu },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU, action_bind_sublabel_input_overlay_hide_in_menu },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS, action_bind_sublabel_input_overlay_show_inputs },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT, action_bind_sublabel_input_overlay_show_inputs_port },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR, action_bind_sublabel_input_overlay_show_mouse_cursor },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_ROTATE, action_bind_sublabel_input_overlay_auto_rotate },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_SCALE, action_bind_sublabel_input_overlay_auto_scale },
      { MENU_ENUM_LABEL_INPUT_OSK_OVERLAY_AUTO_SCALE, action_bind_sublabel_input_osk_overlay_auto_scale },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY, action_bind_sublabel_input_overlay_dpad_diag_sens },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY, action_bind_sublabel_input_overlay_abxy_diag_sens },
      { MENU_ENUM_LABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE, action_bind_sublabel_input_overlay_analog_recenter_zone },
      { MENU_ENUM_LABEL_VIDEO_FONT_SIZE, action_bind_sublabel_video_font_size },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X, action_bind_sublabel_video_message_pos_x },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y, action_bind_sublabel_video_message_pos_y },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED, action_bind_sublabel_video_message_color_red },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN, action_bind_sublabel_video_message_color_green },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE, action_bind_sublabel_video_message_color_blue },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE, action_bind_sublabel_video_message_bgcolor_enable },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED, action_bind_sublabel_video_message_bgcolor_red },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN, action_bind_sublabel_video_message_bgcolor_green },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE, action_bind_sublabel_video_message_bgcolor_blue },
      { MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY, action_bind_sublabel_video_message_bgcolor_opacity },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH, action_bind_sublabel_video_window_width },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT, action_bind_sublabel_video_window_height },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX, action_bind_sublabel_video_window_auto_width_max },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX, action_bind_sublabel_video_window_auto_height_max },
      { MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X, action_bind_sublabel_video_fullscreen_x },
      { MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y, action_bind_sublabel_video_fullscreen_y },
      { MENU_ENUM_LABEL_VIDEO_FORCE_RESOLUTION, action_bind_sublabel_video_force_resolution },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION, action_bind_sublabel_video_save_window_position },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE, action_bind_sublabel_video_window_custom_size_enable },
      { MENU_ENUM_LABEL_QUIT_RETROARCH, action_bind_sublabel_quit_retroarch },
      { MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE, action_bind_sublabel_menu_widgets },
      { MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION, action_bind_sublabel_menu_show_load_content_animation },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_AUTOCONFIG, action_bind_sublabel_notification_show_autoconfig },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS, action_bind_sublabel_notification_show_autoconfig_fails },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_REMAP_LOAD, action_bind_sublabel_notification_show_remap_load },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD, action_bind_sublabel_notification_show_config_override_load },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK, action_bind_sublabel_notification_show_set_initial_disk },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_DISK_CONTROL, action_bind_sublabel_notification_show_disk_control },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_SAVE_STATE, action_bind_sublabel_notification_show_save_state },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_FAST_FORWARD, action_bind_sublabel_notification_show_fast_forward },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_REFRESH_RATE, action_bind_sublabel_notification_show_refresh_rate },
      { MENU_ENUM_LABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE, action_bind_sublabel_notification_show_when_menu_is_alive },
      { MENU_ENUM_LABEL_RESTART_RETROARCH, action_bind_sublabel_restart_retroarch },
      { MENU_ENUM_LABEL_NETWORK_INFORMATION, action_bind_sublabel_network_information },
      { MENU_ENUM_LABEL_SYSTEM_INFORMATION, action_bind_sublabel_system_information },
      { MENU_ENUM_LABEL_LOAD_CONTENT_LIST, action_bind_sublabel_content_list },
      { MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS, action_bind_sublabel_subsystem_settings },
      { MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL, action_bind_sublabel_content_special },
      { MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY, action_bind_sublabel_load_content_history },
      { MENU_ENUM_LABEL_START_CORE, action_bind_sublabel_start_core },
      { MENU_ENUM_LABEL_CORE_LIST, action_bind_sublabel_core_list },
      { MENU_ENUM_LABEL_CORE_LIST_UNLOAD, action_bind_sublabel_core_list_unload },
      { MENU_ENUM_LABEL_SIDELOAD_CORE_LIST, action_bind_sublabel_sideload_core_list },
      { MENU_ENUM_LABEL_CORE_UPDATER_LIST, action_bind_sublabel_download_core },
      { MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES, action_bind_sublabel_update_installed_cores },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SYNC_NOW, action_bind_sublabel_sync_now },
      { MENU_ENUM_LABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL, action_bind_sublabel_resolve_keep_local },
      { MENU_ENUM_LABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER, action_bind_sublabel_resolve_keep_server },
      { MENU_ENUM_LABEL_CORE_MANAGER_LIST, action_bind_sublabel_core_manager_list },
      { MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD, action_bind_sublabel_video_post_filter_record },
      { MENU_ENUM_LABEL_NETPLAY_NICKNAME, action_bind_sublabel_netplay_nickname },
      { MENU_ENUM_LABEL_CHEEVOS_USERNAME, action_bind_sublabel_cheevos_username },
      { MENU_ENUM_LABEL_CHEEVOS_PASSWORD, action_bind_sublabel_cheevos_password },
      { MENU_ENUM_LABEL_VIDEO_FILTER, action_bind_sublabel_video_filter },
      { MENU_ENUM_LABEL_VIDEO_FILTER_REMOVE, action_bind_sublabel_video_filter_remove },
      { MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN, action_bind_sublabel_video_crop_overscan },
      { MENU_ENUM_LABEL_VIDEO_SMOOTH, action_bind_sublabel_video_smooth },
      { MENU_ENUM_LABEL_VIDEO_CTX_SCALING, action_bind_sublabel_video_ctx_scaling },
      { MENU_ENUM_LABEL_VIDEO_FONT_ENABLE, action_bind_sublabel_onscreen_notifications_enable },
      { MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS, action_bind_sublabel_menu_input_unified_controls },
      { MENU_ENUM_LABEL_INPUT_DISABLE_INFO_BUTTON, action_bind_sublabel_menu_input_disable_info_button },
      { MENU_ENUM_LABEL_INPUT_DISABLE_SEARCH_BUTTON, action_bind_sublabel_menu_input_disable_search_button },
      { MENU_ENUM_LABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU, action_bind_sublabel_menu_input_disable_left_analog_in_menu },
      { MENU_ENUM_LABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU, action_bind_sublabel_menu_input_disable_right_analog_in_menu },
      { MENU_ENUM_LABEL_CONFIRM_QUIT, action_bind_sublabel_confirm_quit },
      { MENU_ENUM_LABEL_CONFIRM_CLOSE, action_bind_sublabel_confirm_close },
      { MENU_ENUM_LABEL_CONFIRM_RESET, action_bind_sublabel_confirm_reset },
      { MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW, action_bind_sublabel_audio_max_timing_skew },
      { MENU_ENUM_LABEL_AUDIO_ENABLE, action_bind_sublabel_audio_enable },
      { MENU_ENUM_LABEL_AUDIO_ENABLE_MENU, action_bind_sublabel_audio_enable_menu },
      { MENU_ENUM_LABEL_MENU_SOUNDS, action_bind_sublabel_menu_sounds },
      { MENU_ENUM_LABEL_VIDEO_REFRESH_RATE, action_bind_sublabel_video_refresh_rate },
      { MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN, action_bind_sublabel_dummy_on_core_shutdown },
      { MENU_ENUM_LABEL_CORE_OPTION_CATEGORY_ENABLE, action_bind_sublabel_core_option_category_enable },
      { MENU_ENUM_LABEL_CORE_INFO_CACHE_ENABLE, action_bind_sublabel_core_info_cache_enable },
      { MENU_ENUM_LABEL_CORE_INFO_SAVESTATE_BYPASS, action_bind_sublabel_core_info_savestate_bypass },
      { MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE, action_bind_sublabel_core_allow_rotate },
      { MENU_ENUM_LABEL_VIDEO_VSYNC, action_bind_sublabel_video_vertical_sync },
      { MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC, action_bind_sublabel_video_adaptive_vsync },
      { MENU_ENUM_LABEL_VIDEO_SCANLINE_SYNC, action_bind_sublabel_video_scanline_sync },
      { MENU_ENUM_LABEL_INPUT_TURBO_ENABLE, action_bind_sublabel_input_turbo_enable },
      { MENU_ENUM_LABEL_INPUT_TURBO_DUTY_CYCLE, action_bind_sublabel_input_turbo_duty_cycle },
      { MENU_ENUM_LABEL_INPUT_TURBO_PERIOD, action_bind_sublabel_input_turbo_period },
      { MENU_ENUM_LABEL_INPUT_TURBO_MODE, action_bind_sublabel_input_turbo_mode },
      { MENU_ENUM_LABEL_INPUT_TURBO_BIND, action_bind_sublabel_input_turbo_bind },
      { MENU_ENUM_LABEL_INPUT_TURBO_BUTTON, action_bind_sublabel_input_turbo_button },
      { MENU_ENUM_LABEL_INPUT_TURBO_ALLOW_DPAD, action_bind_sublabel_input_turbo_allow_dpad },
      { MENU_ENUM_LABEL_INPUT_RUMBLE_GAIN, action_bind_sublabel_input_rumble_gain },
      { MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT, action_bind_sublabel_input_bind_timeout },
      { MENU_ENUM_LABEL_INPUT_BIND_HOLD, action_bind_sublabel_input_bind_hold },
      { MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD, action_bind_sublabel_button_axis_threshold },
      { MENU_ENUM_LABEL_INPUT_ANALOG_DEADZONE, action_bind_sublabel_analog_deadzone },
      { MENU_ENUM_LABEL_INPUT_ANALOG_SENSITIVITY, action_bind_sublabel_analog_sensitivity },
      { MENU_ENUM_LABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY, action_bind_sublabel_sensor_accelerometer_sensitivity },
      { MENU_ENUM_LABEL_INPUT_SENSOR_ORIENTATION, action_bind_sublabel_sensor_orientation },
      { MENU_ENUM_LABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY, action_bind_sublabel_sensor_gyroscope_sensitivity },
      { MENU_ENUM_LABEL_INPUT_TOUCH_SCALE, action_bind_sublabel_input_touch_scale },
      { MENU_ENUM_LABEL_AUDIO_SYNC, action_bind_sublabel_audio_sync },
      { MENU_ENUM_LABEL_AUDIO_VOLUME, action_bind_sublabel_audio_volume },
      { MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR, action_bind_sublabel_input_poll_type_behavior },
      { MENU_ENUM_LABEL_INPUT_MAX_USERS, action_bind_sublabel_input_max_users },
      { MENU_ENUM_LABEL_LOCATION_ALLOW, action_bind_sublabel_location_allow },
      { MENU_ENUM_LABEL_CAMERA_ALLOW, action_bind_sublabel_camera_allow },
      { MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA, action_bind_sublabel_audio_rate_control_delta },
      { MENU_ENUM_LABEL_AUDIO_MUTE, action_bind_sublabel_audio_mute },
      { MENU_ENUM_LABEL_AUDIO_REWIND_MUTE, action_bind_sublabel_audio_rewind_mute },
      { MENU_ENUM_LABEL_AUDIO_FASTFORWARD_MUTE, action_bind_sublabel_audio_fastforward_mute },
      { MENU_ENUM_LABEL_AUDIO_FASTFORWARD_SPEEDUP, action_bind_sublabel_audio_fastforward_speedup },
      { MENU_ENUM_LABEL_AUDIO_LATENCY, action_bind_sublabel_audio_latency },
      { MENU_ENUM_LABEL_DRIVER_SWITCH_ENABLE, action_bind_sublabel_driver_switch_enable },
      { MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT, action_bind_sublabel_video_shared_context },
      { MENU_ENUM_LABEL_SETTINGS, action_bind_sublabel_settings },
      { MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT, action_bind_sublabel_config_save_on_exit },
      { MENU_ENUM_LABEL_CONFIG_SAVE_MINIMAL, action_bind_sublabel_config_save_minimal },
      { MENU_ENUM_LABEL_REMAP_SAVE_ON_EXIT, action_bind_sublabel_remap_save_on_exit },
      { MENU_ENUM_LABEL_CONFIGURATION_SETTINGS, action_bind_sublabel_configuration_settings_list },
      { MENU_ENUM_LABEL_CONFIGURATIONS_LIST, action_bind_sublabel_configurations_list_list },
      { MENU_ENUM_LABEL_VIDEO_THREADED, action_bind_sublabel_video_threaded },
      { MENU_ENUM_LABEL_VIDEO_HARD_SYNC, action_bind_sublabel_video_hard_sync },
      { MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES, action_bind_sublabel_video_hard_sync_frames },
      { MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO, action_bind_sublabel_video_refresh_rate_auto },
      { MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED, action_bind_sublabel_video_refresh_rate_polled },
      { MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX, action_bind_sublabel_video_monitor_index },
      { MENU_ENUM_LABEL_LOG_VERBOSITY, action_bind_sublabel_log_verbosity },
      { MENU_ENUM_LABEL_LOG_TO_FILE, action_bind_sublabel_log_to_file },
      { MENU_ENUM_LABEL_LOG_TO_FILE_TIMESTAMP, action_bind_sublabel_log_to_file_timestamp },
      { MENU_ENUM_LABEL_LOG_DIR, action_bind_sublabel_log_dir },
      { MENU_ENUM_LABEL_SHOW_HIDDEN_FILES, action_bind_sublabel_show_hidden_files },
      { MENU_ENUM_LABEL_USE_LAST_START_DIRECTORY, action_bind_sublabel_use_last_start_directory },
      { MENU_ENUM_LABEL_CORE_SUGGEST_ALWAYS, action_bind_sublabel_core_suggest_always },
      { MENU_ENUM_LABEL_USE_BUILTIN_PLAYER, action_bind_sublabel_use_builtin_media_player },
      { MENU_ENUM_LABEL_USE_BUILTIN_IMAGE_VIEWER, action_bind_sublabel_use_builtin_image_viewer },
      { MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO, action_bind_sublabel_toggle_gamepad_combo },
      { MENU_ENUM_LABEL_INPUT_QUIT_GAMEPAD_COMBO, action_bind_sublabel_quit_gamepad_combo },
      { MENU_ENUM_LABEL_CORE_INFO_ENTRY, action_bind_sublabel_core_info_entry },
      { MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY, action_bind_sublabel_systeminfo_controller_entry },
      { MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION, action_bind_sublabel_video_black_frame_insertion },
      { MENU_ENUM_LABEL_VIDEO_BFI_DARK_FRAMES, action_bind_sublabel_video_bfi_dark_frames },
      { MENU_ENUM_LABEL_VIDEO_SHADER_SUBFRAMES, action_bind_sublabel_video_shader_subframes },
      { MENU_ENUM_LABEL_VIDEO_SCAN_SUBFRAMES, action_bind_sublabel_video_scan_subframes },
      { MENU_ENUM_LABEL_VIDEO_FRAME_DELAY, action_bind_sublabel_video_frame_delay },
      { MENU_ENUM_LABEL_VIDEO_FRAME_DELAY_AUTO, action_bind_sublabel_video_frame_delay_auto },
      { MENU_ENUM_LABEL_VIDEO_FRAME_TIME_SAMPLE_GATED, action_bind_sublabel_video_frame_time_sample_gated },
      { MENU_ENUM_LABEL_VIDEO_SHADER_DELAY, action_bind_sublabel_video_shader_delay },
      { MENU_ENUM_LABEL_ADD_CONTENT_LIST, action_bind_sublabel_add_content_list },
      { MENU_ENUM_LABEL_INPUT_RETROPAD_BINDS, action_bind_sublabel_input_retropad_settings },
      { MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS, action_bind_sublabel_input_hotkey_settings },
      { MENU_ENUM_LABEL_INPUT_HOTKEY_BLOCK_DELAY, action_bind_sublabel_input_hotkey_block_delay },
      { MENU_ENUM_LABEL_INPUT_HOTKEY_DEVICE_MERGE, action_bind_sublabel_input_hotkey_device_merge },
      { MENU_ENUM_LABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1, action_bind_sublabel_input_hotkey_follows_player1 },
      { MENU_ENUM_LABEL_INPUT_USER_1_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_2_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_3_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_4_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_5_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_6_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_7_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_8_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_9_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_10_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_11_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_12_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_13_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_14_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_15_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INPUT_USER_16_BINDS, action_bind_sublabel_user_bind_settings },
      { MENU_ENUM_LABEL_INFORMATION_LIST, action_bind_sublabel_information_list_list },
      { MENU_ENUM_LABEL_NETPLAY, action_bind_sublabel_netplay_settings },
      { MENU_ENUM_LABEL_ONLINE_UPDATER, action_bind_sublabel_online_updater },
      { MENU_ENUM_LABEL_UPDATER_SETTINGS, action_bind_sublabel_updater_settings },
      { MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES, action_bind_sublabel_max_swapchain_images },
      { MENU_ENUM_LABEL_VIDEO_WAITABLE_SWAPCHAINS, action_bind_sublabel_waitable_swapchains },
      { MENU_ENUM_LABEL_VIDEO_MAX_FRAME_LATENCY, action_bind_sublabel_max_frame_latency },
      { MENU_ENUM_LABEL_NETPLAY_PING_SHOW, action_bind_sublabel_netplay_ping_show },
      { MENU_ENUM_LABEL_STATISTICS_SHOW, action_bind_sublabel_statistics_show },
      { MENU_ENUM_LABEL_FPS_SHOW, action_bind_sublabel_fps_show },
      { MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL, action_bind_sublabel_fps_update_interval },
      { MENU_ENUM_LABEL_FRAMECOUNT_SHOW, action_bind_sublabel_framecount_show },
      { MENU_ENUM_LABEL_MEMORY_SHOW, action_bind_sublabel_memory_show },
      { MENU_ENUM_LABEL_MEMORY_UPDATE_INTERVAL, action_bind_sublabel_memory_update_interval },
      { MENU_ENUM_LABEL_TIME_SHOW, action_bind_sublabel_time_show },
      { MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS, action_bind_sublabel_menu_views_settings_list },
      { MENU_ENUM_LABEL_SETTINGS_VIEWS_SETTINGS, action_bind_sublabel_settings_views_settings_list },
      { MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS, action_bind_sublabel_quick_menu_views_settings_list },
      { MENU_ENUM_LABEL_MENU_SETTINGS, action_bind_sublabel_menu_settings_list },
      { MENU_ENUM_LABEL_APPICON_SETTINGS, action_bind_sublabel_appicon_settings_list },
      { MENU_ENUM_LABEL_VIDEO_SETTINGS, action_bind_sublabel_video_settings_list },
      { MENU_ENUM_LABEL_VIDEO_SYNCHRONIZATION_SETTINGS, action_bind_sublabel_video_synchronization_settings_list },
      { MENU_ENUM_LABEL_VIDEO_FULLSCREEN_MODE_SETTINGS, action_bind_sublabel_video_fullscreen_mode_settings_list },
      { MENU_ENUM_LABEL_VIDEO_WINDOWED_MODE_SETTINGS, action_bind_sublabel_video_windowed_mode_settings_list },
      { MENU_ENUM_LABEL_VIDEO_SCALING_SETTINGS, action_bind_sublabel_video_scaling_settings_list },
      { MENU_ENUM_LABEL_VIDEO_HDR_SETTINGS, action_bind_sublabel_video_hdr_settings_list },
      { MENU_ENUM_LABEL_VIDEO_HDR_ENABLE, action_bind_sublabel_hdr_enable },
      { MENU_ENUM_LABEL_VIDEO_HDR_PAPER_WHITE_NITS, action_bind_sublabel_hdr_paper_white_nits },
      { MENU_ENUM_LABEL_MENU_HDR_BRIGHTNESS_NITS, action_bind_sublabel_hdr_menu_nits },
      { MENU_ENUM_LABEL_VIDEO_HDR_EXPAND_GAMUT, action_bind_sublabel_hdr_expand_gamut },
      { MENU_ENUM_LABEL_VIDEO_HDR_SCANLINES, action_bind_sublabel_hdr_scanlines },
      { MENU_ENUM_LABEL_VIDEO_HDR_SUBPIXEL_LAYOUT, action_bind_sublabel_hdr_subpixel_layout },
      { MENU_ENUM_LABEL_VIDEO_OUTPUT_SETTINGS, action_bind_sublabel_video_output_settings_list },
      { MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS, action_bind_sublabel_crt_switchres_settings_list },
      { MENU_ENUM_LABEL_AUDIO_SETTINGS, action_bind_sublabel_audio_settings_list },
      { MENU_ENUM_LABEL_AUDIO_SYNCHRONIZATION_SETTINGS, action_bind_sublabel_audio_synchronization_settings_list },
      { MENU_ENUM_LABEL_AUDIO_OUTPUT_SETTINGS, action_bind_sublabel_audio_output_settings_list },
      { MENU_ENUM_LABEL_LATENCY_SETTINGS, action_bind_sublabel_latency_settings_list },
      { MENU_ENUM_LABEL_RECORDING_SETTINGS, action_bind_sublabel_recording_settings_list },
      { MENU_ENUM_LABEL_CORE_SETTINGS, action_bind_sublabel_core_settings_list },
      { MENU_ENUM_LABEL_DRIVER_SETTINGS, action_bind_sublabel_driver_settings_list },
      { MENU_ENUM_LABEL_SAVING_SETTINGS, action_bind_sublabel_saving_settings_list },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SETTINGS, action_bind_sublabel_cloud_sync_settings_list },
      { MENU_ENUM_LABEL_CLOUD_SYNC_ENABLE, action_bind_sublabel_cloud_sync_enable },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SYNC_MODE, action_bind_sublabel_cloud_sync_sync_mode },
      { MENU_ENUM_LABEL_CLOUD_SYNC_DESTRUCTIVE, action_bind_sublabel_cloud_sync_destructive },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SYNC_SAVES, action_bind_sublabel_cloud_sync_sync_saves },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SYNC_CONFIGS, action_bind_sublabel_cloud_sync_sync_configs },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SYNC_THUMBS, action_bind_sublabel_cloud_sync_sync_thumbs },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SYNC_SYSTEM, action_bind_sublabel_cloud_sync_sync_system },
      { MENU_ENUM_LABEL_CLOUD_SYNC_DRIVER, action_bind_sublabel_cloud_sync_driver },
      { MENU_ENUM_LABEL_CLOUD_SYNC_URL, action_bind_sublabel_cloud_sync_url },
      { MENU_ENUM_LABEL_CLOUD_SYNC_S3_URL, action_bind_sublabel_cloud_sync_s3_url },
      { MENU_ENUM_LABEL_CLOUD_SYNC_USERNAME, action_bind_sublabel_cloud_sync_username },
      { MENU_ENUM_LABEL_CLOUD_SYNC_PASSWORD, action_bind_sublabel_cloud_sync_password },
      { MENU_ENUM_LABEL_CLOUD_SYNC_ACCESS_KEY_ID, action_bind_sublabel_cloud_sync_access_key_id },
      { MENU_ENUM_LABEL_CLOUD_SYNC_SECRET_ACCESS_KEY, action_bind_sublabel_cloud_sync_secret_access_key },
      { MENU_ENUM_LABEL_LOGGING_SETTINGS, action_bind_sublabel_logging_settings_list },
      { MENU_ENUM_LABEL_PLAYLIST_SETTINGS, action_bind_sublabel_playlist_settings_list },
      { MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST, action_bind_sublabel_playlist_manager_list },
      { MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE, action_bind_sublabel_playlist_manager_default_core },
      { MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES, action_bind_sublabel_playlist_manager_reset_cores },
      { MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE, action_bind_sublabel_playlist_manager_label_display_mode },
      { MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE, action_bind_sublabel_playlist_manager_sort_mode },
      { MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST, action_bind_sublabel_playlist_manager_clean_playlist },
      { MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST, action_bind_sublabel_playlist_manager_refresh_playlist },
      { MENU_ENUM_LABEL_DELETE_PLAYLIST, action_bind_sublabel_delete_playlist },
      { MENU_ENUM_LABEL_AI_SERVICE_URL, action_bind_sublabel_ai_service_url },
      { MENU_ENUM_LABEL_AI_SERVICE_TARGET_LANG, action_bind_sublabel_ai_service_target_lang },
      { MENU_ENUM_LABEL_AI_SERVICE_SOURCE_LANG, action_bind_sublabel_ai_service_source_lang },
      { MENU_ENUM_LABEL_AI_SERVICE_MODE, action_bind_sublabel_ai_service_mode },
      { MENU_ENUM_LABEL_AI_SERVICE_BACKEND, action_bind_sublabel_ai_service_backend },
      { MENU_ENUM_LABEL_AI_SERVICE_PAUSE, action_bind_sublabel_ai_service_pause },
      { MENU_ENUM_LABEL_AI_SERVICE_ENABLE, action_bind_sublabel_ai_service_enable },
      { MENU_ENUM_LABEL_AI_SERVICE_SETTINGS, action_bind_sublabel_ai_service_settings_list },
      { MENU_ENUM_LABEL_ACCESSIBILITY_ENABLED, action_bind_sublabel_accessibility_enabled },
      { MENU_ENUM_LABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED, action_bind_sublabel_accessibility_narrator_speech_speed },
      { MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS, action_bind_sublabel_accessibility_settings_list },
      { MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS, action_bind_sublabel_user_interface_settings_list },
      { MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS, action_bind_sublabel_power_management_settings_list },
      { MENU_ENUM_LABEL_PRIVACY_SETTINGS, action_bind_sublabel_privacy_settings_list },
      { MENU_ENUM_LABEL_MIDI_SETTINGS, action_bind_sublabel_midi_settings_list },
      { MENU_ENUM_LABEL_DIRECTORY_SETTINGS, action_bind_sublabel_directory_settings_list },
      { MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS, action_bind_sublabel_frame_throttle_settings_list },
      { MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS, action_bind_sublabel_frame_time_counter_settings_list },
      { MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS, action_bind_sublabel_onscreen_display_settings_list },
      { MENU_ENUM_LABEL_NETWORK_SETTINGS, action_bind_sublabel_network_settings_list },
      { MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS, action_bind_sublabel_network_on_demand_thumbnails },
      { MENU_ENUM_LABEL_USER_SETTINGS, action_bind_sublabel_user_settings_list },
      { MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS, action_bind_sublabel_retro_achievements_settings_list },
      { MENU_ENUM_LABEL_INPUT_SETTINGS, action_bind_sublabel_input_settings_list },
      { MENU_ENUM_LABEL_INPUT_MENU_SETTINGS, action_bind_sublabel_input_menu_settings_list },
      { MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS, action_bind_sublabel_input_turbo_fire_settings_list },
      { MENU_ENUM_LABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS, action_bind_sublabel_input_haptic_feedback_settings_list },
      { MENU_ENUM_LABEL_INPUT_SENSOR_SETTINGS, action_bind_sublabel_input_sensor_settings_list },
      { MENU_ENUM_LABEL_WIFI_SETTINGS, action_bind_sublabel_wifi_settings_list },
      { MENU_ENUM_LABEL_HELP_LIST, action_bind_sublabel_help_list },
      { MENU_ENUM_LABEL_USER_LANGUAGE, action_bind_sublabel_user_language },
      { MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE, action_bind_sublabel_suspend_screensaver_enable },
      { MENU_ENUM_LABEL_VIDEO_SCALE, action_bind_sublabel_video_window_scale },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY, action_bind_sublabel_video_window_opacity },
      { MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS, action_bind_sublabel_video_window_show_decorations },
      { MENU_ENUM_LABEL_UI_MENUBAR_ENABLE, action_bind_sublabel_video_window_show_menubar },
      { MENU_ENUM_LABEL_PAUSE_NONACTIVE, action_bind_sublabel_pause_nonactive },
      { MENU_ENUM_LABEL_PAUSE_ON_DISCONNECT, action_bind_sublabel_pause_on_disconnect },
      { MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION, action_bind_sublabel_video_disable_composition },
      { MENU_ENUM_LABEL_HISTORY_LIST_ENABLE, action_bind_sublabel_history_list_enable },
      { MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE, action_bind_sublabel_content_history_size },
      { MENU_ENUM_LABEL_CONTENT_FAVORITES_SIZE, action_bind_sublabel_content_favorites_size },
      { MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER, action_bind_sublabel_netplay_use_mitm_server },
      { MENU_ENUM_LABEL_NETPLAY_MITM_SERVER, action_bind_sublabel_netplay_mitm_server },
      { MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER, action_bind_sublabel_netplay_custom_mitm_server },
      { MENU_ENUM_LABEL_CORE_LOCK, action_bind_sublabel_core_lock },
      { MENU_ENUM_LABEL_CORE_SET_STANDALONE_EXEMPT, action_bind_sublabel_core_set_standalone_exempt },
      { MENU_ENUM_LABEL_CORE_DELETE, action_bind_sublabel_core_delete },
      { MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE, action_bind_sublabel_pause_hardcode_mode },
      { MENU_ENUM_LABEL_ACHIEVEMENT_RESUME, action_bind_sublabel_resume_hardcode_mode },
      { MENU_ENUM_LABEL_MIDI_INPUT, action_bind_sublabel_midi_input },
      { MENU_ENUM_LABEL_MIDI_OUTPUT, action_bind_sublabel_midi_output },
      { MENU_ENUM_LABEL_MIDI_VOLUME, action_bind_sublabel_midi_volume },
      { MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS, action_bind_sublabel_onscreen_overlay_settings_list },
      { MENU_ENUM_LABEL_OSK_OVERLAY_SETTINGS, action_bind_sublabel_osk_overlay_settings_list },
      { MENU_ENUM_LABEL_OVERLAY_LIGHTGUN_SETTINGS, action_bind_sublabel_overlay_lightgun_settings_list },
      { MENU_ENUM_LABEL_OVERLAY_MOUSE_SETTINGS, action_bind_sublabel_overlay_mouse_settings_list },
      { MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS, action_bind_sublabel_onscreen_notifications_settings_list },
      { MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS, action_bind_sublabel_onscreen_notifications_views_settings_list },
      { MENU_ENUM_LABEL_BRIGHTNESS_CONTROL, action_bind_sublabel_brightness_control },
      { MENU_ENUM_LABEL_DISCORD_ALLOW, action_bind_sublabel_discord_allow },
      { MENU_ENUM_LABEL_PLAYLIST_ENTRY, action_bind_sublabel_playlist_entry },
      { MENU_ENUM_LABEL_PLAYLIST_SHOW_SUBLABELS, action_bind_sublabel_playlist_show_sublabels },
      { MENU_ENUM_LABEL_PLAYLIST_SHOW_HISTORY_ICONS, action_bind_sublabel_playlist_show_history_icons },
      { MENU_ENUM_LABEL_PLAYLIST_SHOW_ENTRY_IDX, action_bind_sublabel_playlist_show_entry_idx },
      { MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_ENABLE, action_bind_sublabel_menu_rgui_border_filler_enable },
      { MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE, action_bind_sublabel_menu_rgui_border_filler_thickness_enable },
      { MENU_ENUM_LABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE, action_bind_sublabel_menu_rgui_background_filler_thickness_enable },
      { MENU_ENUM_LABEL_MENU_LINEAR_FILTER, action_bind_sublabel_menu_linear_filter },
      { MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO_LOCK, action_bind_sublabel_menu_rgui_aspect_ratio_lock },
      { MENU_ENUM_LABEL_RGUI_MENU_COLOR_THEME, action_bind_sublabel_rgui_menu_color_theme },
      { MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET, action_bind_sublabel_rgui_menu_theme_preset },
      { MENU_ENUM_LABEL_MENU_RGUI_TRANSPARENCY, action_bind_sublabel_menu_rgui_transparency },
      { MENU_ENUM_LABEL_MENU_RGUI_SHADOWS, action_bind_sublabel_menu_rgui_shadows },
      { MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT, action_bind_sublabel_menu_rgui_particle_effect },
      { MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED, action_bind_sublabel_menu_rgui_particle_effect_speed },
      { MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER, action_bind_sublabel_menu_rgui_particle_effect_screensaver },
      { MENU_ENUM_LABEL_MENU_RGUI_INLINE_THUMBNAILS, action_bind_sublabel_menu_rgui_inline_thumbnails },
      { MENU_ENUM_LABEL_MENU_RGUI_SWAP_THUMBNAILS, action_bind_sublabel_menu_rgui_swap_thumbnails },
      { MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER, action_bind_sublabel_menu_rgui_thumbnail_downscaler },
      { MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DELAY, action_bind_sublabel_menu_rgui_thumbnail_delay },
      { MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG, action_bind_sublabel_content_runtime_log },
      { MENU_ENUM_LABEL_SCAN_WITHOUT_CORE_MATCH, action_bind_sublabel_scan_without_core_match },
      { MENU_ENUM_LABEL_SCAN_SERIAL_AND_CRC, action_bind_sublabel_scan_serial_and_crc },
      { MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE, action_bind_sublabel_content_runtime_log_aggregate },
      { MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE, action_bind_sublabel_playlist_sublabel_runtime_type },
      { MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE, action_bind_sublabel_playlist_sublabel_last_played_style },
      { MENU_ENUM_LABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL, action_bind_sublabel_menu_rgui_internal_upscale_level },
      { MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO, action_bind_sublabel_menu_rgui_aspect_ratio },
      { MENU_ENUM_LABEL_MENU_TICKER_TYPE, action_bind_sublabel_menu_ticker_type },
      { MENU_ENUM_LABEL_MENU_TICKER_SPEED, action_bind_sublabel_menu_ticker_speed },
      { MENU_ENUM_LABEL_MENU_TICKER_SMOOTH, action_bind_sublabel_menu_ticker_smooth },
      { MENU_ENUM_LABEL_PLAYLIST_SHOW_INLINE_CORE_NAME, action_bind_sublabel_playlist_show_inline_core_name },
      { MENU_ENUM_LABEL_PLAYLIST_SORT_ALPHABETICAL, action_bind_sublabel_playlist_sort_alphabetical },
      { MENU_ENUM_LABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH, action_bind_sublabel_playlist_fuzzy_archive_match },
      { MENU_ENUM_LABEL_PLAYLIST_PORTABLE_PATHS, action_bind_sublabel_playlist_portable_paths },
      { MENU_ENUM_LABEL_PLAYLIST_USE_FILENAME, action_bind_sublabel_playlist_use_filename },
      { MENU_ENUM_LABEL_PLAYLIST_ALLOW_NON_PNG, action_bind_sublabel_playlist_allow_non_png },
      { MENU_ENUM_LABEL_PLAYLIST_USE_OLD_FORMAT, action_bind_sublabel_playlist_use_old_format },
      { MENU_ENUM_LABEL_PLAYLIST_COMPRESSION, action_bind_sublabel_playlist_compression },
      { MENU_ENUM_LABEL_MENU_RGUI_FULL_WIDTH_LAYOUT, action_bind_sublabel_menu_rgui_full_width_layout },
      { MENU_ENUM_LABEL_MENU_RGUI_EXTENDED_ASCII, action_bind_sublabel_menu_rgui_extended_ascii },
      { MENU_ENUM_LABEL_MENU_RGUI_SWITCH_ICONS, action_bind_sublabel_menu_rgui_switch_icons },
      { MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST, action_bind_sublabel_pl_thumbnails_updater_list },
      { MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES, action_bind_sublabel_download_core_system_files },
      { MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS, action_bind_sublabel_download_core_content },
      { MENU_ENUM_LABEL_RDB_ENTRY_DETAIL, action_bind_sublabel_rdb_entry_detail },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST, action_bind_sublabel_manual_content_scan_list },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR, action_bind_sublabel_manual_content_scan_dir },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME, action_bind_sublabel_manual_content_scan_system_name },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, action_bind_sublabel_manual_content_scan_system_name_custom },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME, action_bind_sublabel_manual_content_scan_core_name },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_FILE_EXTS, action_bind_sublabel_manual_content_scan_file_exts },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY, action_bind_sublabel_manual_content_scan_search_recursively },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES, action_bind_sublabel_manual_content_scan_search_archives },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE, action_bind_sublabel_manual_content_scan_dat_file },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER, action_bind_sublabel_manual_content_scan_dat_file_filter },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_OVERWRITE, action_bind_sublabel_manual_content_scan_overwrite },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES, action_bind_sublabel_manual_content_scan_validate_entries },
      { MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_START, action_bind_sublabel_manual_content_scan_start },
      { MENU_ENUM_LABEL_CORE_CREATE_BACKUP, action_bind_sublabel_core_create_backup },
      { MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_LIST, action_bind_sublabel_core_restore_backup_list },
      { MENU_ENUM_LABEL_CORE_DELETE_BACKUP_LIST, action_bind_sublabel_core_delete_backup_list },
      { MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_ENTRY, action_bind_sublabel_core_backup_entry },
      { MENU_ENUM_LABEL_CORE_DELETE_BACKUP_ENTRY, action_bind_sublabel_core_backup_entry },
   };
   {
      uint32_t key = (uint32_t)cbs->enum_idx;
      unsigned m;
      for (m = 0; m < (unsigned)ARRAY_SIZE(sublabel_map); m++)
      {
         if (sublabel_map[m].label == key)
         {
            BIND_ACTION_SUBLABEL(cbs, sublabel_map[m].cb);
            return 0;
         }
      }
   }

   switch (cbs->enum_idx)
   {

#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_CORE_UPDATER_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_updater_entry);
            break;
#endif
         case MENU_ENUM_LABEL_ADD_TO_MIXER:
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, menu_action_sublabel_setting_audio_mixer_add_to_mixer);
#endif
            break;
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY:
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, menu_action_sublabel_setting_audio_mixer_add_to_mixer_and_play);
#endif
            break;
#ifdef HAVE_MICROPHONE
         case MENU_ENUM_LABEL_MICROPHONE_RESAMPLER_QUALITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_resampler_quality);
            break;
#endif
         case MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_icons_enable);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_SWITCH_ICONS:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_switch_icons);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_playlist_icons_enable);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_landscape_layout_optimization);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_SHOW_NAV_BAR:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_show_nav_bar);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_auto_rotate_nav_bar);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_dual_thumbnail_list_view_enable);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_thumbnail_background_enable);
#endif
            break;
#if defined(RARCH_MOBILE)
         case MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_vp_bias_portrait_x);
            break;
         case MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_vp_bias_portrait_y);
            break;
#endif
#if defined(DINGUX)
         case MENU_ENUM_LABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_dingux_ipu_keep_aspect);
            break;
         case MENU_ENUM_LABEL_VIDEO_DINGUX_IPU_FILTER_TYPE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_dingux_ipu_filter_type);
            break;
#if defined(DINGUX_BETA)
         case MENU_ENUM_LABEL_VIDEO_DINGUX_REFRESH_RATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_dingux_refresh_rate);
            break;
#endif
#if defined(RS90) || defined(MIYOO)
         case MENU_ENUM_LABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_dingux_rs90_softfilter_type);
            break;
#endif
#endif
         case MENU_ENUM_LABEL_CHEAT_DATABASE_PATH:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheatfile_directory);
#endif
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_cheats);
#endif
            break;
#ifdef HAVE_LAKKA
         case MENU_ENUM_LABEL_EJECT_DISC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_eject_disc);
            break;
#endif
#if defined(HAVE_CDROM) && defined(HAVE_LAKKA)
         case MENU_ENUM_LABEL_MENU_SHOW_EJECT_DISC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_eject_disc);
            break;
#endif
#ifndef HAVE_LAKKA
         case MENU_ENUM_LABEL_MENU_SHOW_RESTART_RETROARCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_restart_retroarch);
            break;
#endif
         case MENU_ENUM_LABEL_XMB_FONT:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_font);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_RIBBON_ENABLE:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_ribbon_enable);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_animation_horizontal_higlight);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_animation_move_up_down);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_animation_opening_main_menu);
#endif
            break;
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
         case MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_widget_scale_factor_windowed);
            break;
#endif
         case MENU_ENUM_LABEL_XMB_ALPHA_FACTOR:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_alpha_factor);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_VERTICAL_FADE_FACTOR:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_vertical_fade_factor);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_SHOW_HORIZONTAL_LIST:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_show_horizontal_list);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_SHOW_TITLE_HEADER:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_show_title_header);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_TITLE_MARGIN:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_title_margin);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_title_margin_horizontal_offset);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_MENU_COLOR_THEME:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_menu_color_theme);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_PADDING_FACTOR:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_padding_factor);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_HEADER_ICON:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_header_icon);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_HEADER_SEPARATOR:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_header_separator);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_COLLAPSE_SIDEBAR:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_collapse_sidebar);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_SHOW_SIDEBAR:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_show_sidebar);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_TRUNCATE_PLAYLIST_NAME:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_truncate_playlist_name);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_sort_after_truncate_playlist_name);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_SCROLL_CONTENT_METADATA:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_scroll_content_metadata);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_THUMBNAIL_SCALE_FACTOR:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_thumbnail_scale_factor);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale_factor_global);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE_FACTOR_TITLE:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale_factor_title);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale_factor_sidebar);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE_FACTOR_LABEL:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale_factor_label);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale_factor_sublabel);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE_FACTOR_TIME:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale_factor_time);
#endif
            break;
         case MENU_ENUM_LABEL_OZONE_FONT_SCALE_FACTOR_FOOTER:
#ifdef HAVE_OZONE
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_font_scale_factor_footer);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_menu_color_theme);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_menu_color_theme);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_TRANSITION_ANIMATION:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_menu_transition_animation);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_menu_thumbnail_view_portrait);
#endif
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE:
#ifdef HAVE_MATERIALUI
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_menu_thumbnail_view_landscape);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_shadows_enable);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_VERTICAL_THUMBNAILS:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_vertical_thumbnails);
#endif
            break;
         case MENU_ENUM_LABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_thumbnail_scale_factor);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_LAYOUT:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_layout);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_THEME:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_icon_theme);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_CURRENT_MENU_ICON:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_current_menu_icon);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_ENTRY_ICONS:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_entry_icons);
#endif
            break;
         case MENU_ENUM_LABEL_XMB_SWITCH_ICONS:
#ifdef HAVE_XMB
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_switch_icons);
#endif
            break;
         case MENU_ENUM_LABEL_THUMBNAILS:
            {
               const char *menu_ident             = menu_driver_ident();
#ifdef HAVE_RGUI
               if (string_is_equal(menu_ident, "rgui"))
               {
                  BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails_rgui);
               }
               else
#endif
#ifdef HAVE_MATERIALUI
                  if (string_is_equal(menu_ident, "glui"))
                  {
                     BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails_materialui);
                  }
                  else
#endif
                  {
                     BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails);
                  }
            }
            break;
         case MENU_ENUM_LABEL_LEFT_THUMBNAILS:
            {
               const char *menu_ident             = menu_driver_ident();
#ifdef HAVE_RGUI
               if (string_is_equal(menu_ident, "rgui"))
               {
                  BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_rgui);
               }
               else
#endif
#ifdef HAVE_OZONE
                  if (string_is_equal(menu_ident, "ozone"))
                  {
                     BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_ozone);
                  }
                  else
#endif
#ifdef HAVE_MATERIALUI
                     if (string_is_equal(menu_ident, "glui"))
                     {
                        BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_materialui);
                     }
                     else
#endif
                     {
                        BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails);
                     }
            }
            break;
#ifdef HAVE_MICROPHONE
         case MENU_ENUM_LABEL_MICROPHONE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_enable);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_INPUT_RATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_rate);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_DEVICE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_device);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_LATENCY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_latency);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_wasapi_exclusive_mode);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_WASAPI_FLOAT_FORMAT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_wasapi_float_format);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_wasapi_sh_buffer_length);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_RESAMPLER_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_resampler_driver);
            break;
         case MENU_ENUM_LABEL_MICROPHONE_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_driver);
            break;
#endif
         case MENU_ENUM_LABEL_BLUETOOTH_DRIVER:
#ifdef HAVE_BLUETOOTH
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bluetooth_driver);
#endif
            break;
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
         case MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_screensaver_animation);
            break;
         case MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION_SPEED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_screensaver_animation_speed);
            break;
#endif
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
         case MENU_ENUM_LABEL_INPUT_NOWINKEY_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_nowinkey_enable);
            break;
#endif
#ifdef ANDROID
         case MENU_ENUM_LABEL_INPUT_SELECT_PHYSICAL_KEYBOARD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_select_physical_keyboard);
            break;
         case MENU_ENUM_LABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_android_input_disconnect_workaround);
            break;
#endif
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_cheat_options);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_save_as);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_load);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_load_append);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_apply_changes);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_apply_after_toggle);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_IDX:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_idx);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_big_endian);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_start_or_cont);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_start_or_restart);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_exact);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_LT:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_lt);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_GT:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_gt);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EQ:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_eq);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_neq);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_eqplus);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_eqminus);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_repeat_count);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_repeat_add_to_address);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_repeat_add_to_value);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_MATCHES:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_add_matches);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_add_new_top);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_reload_cheats);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_add_new_bottom);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_address_bit_position);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_delete_all);
#endif
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED:
#if defined(ANDROID)
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_overlay_hide_when_gamepad_connected_android);
#else
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_overlay_hide_when_gamepad_connected);
#endif
            break;
         case MENU_ENUM_LABEL_NOTIFICATION_SHOW_CHEATS_APPLIED:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_notification_show_cheats_applied);
#endif
            break;
         case MENU_ENUM_LABEL_NOTIFICATION_SHOW_PATCH_APPLIED:
#ifdef HAVE_PATCH
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_notification_show_patch_applied);
#endif
            break;
#ifdef HAVE_SCREENSHOTS
         case MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_notification_show_screenshot);
            break;
         case MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_notification_show_screenshot_duration);
            break;
         case MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_notification_show_screenshot_flash);
            break;
#endif
#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_notification_show_netplay_extra);
            break;
#endif
#if defined(ANDROID)
         case MENU_ENUM_LABEL_SWITCH_INSTALLED_CORES_PFD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_installed_cores_pfd);
            break;
#endif
#ifdef HAVE_MIST
         case MENU_ENUM_LABEL_STEAM_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_steam_settings_list);
            break;
         case MENU_ENUM_LABEL_STEAM_RICH_PRESENCE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_steam_rich_presence_enable);
            break;
         case MENU_ENUM_LABEL_STEAM_RICH_PRESENCE_FORMAT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_steam_rich_presence_format);
            break;
         case MENU_ENUM_LABEL_CORE_MANAGER_STEAM_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_manager_steam_list);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_CORE_MANAGER_STEAM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_core_manager_steam);
            break;
#endif
#ifndef HAVE_DYNAMIC
         case MENU_ENUM_LABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_always_reload_core_on_run_content);
            break;
#endif
#if defined(GEKKO)
         case MENU_ENUM_LABEL_INPUT_MOUSE_SCALE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_mouse_scale);
            break;
#endif
#ifdef UDEV_TOUCH_SUPPORT
         case MENU_ENUM_LABEL_INPUT_TOUCH_VMOUSE_POINTER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_touch_vmouse_pointer);
            break;
         case MENU_ENUM_LABEL_INPUT_TOUCH_VMOUSE_MOUSE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_touch_vmouse_mouse);
            break;
         case MENU_ENUM_LABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_touch_vmouse_touchpad);
            break;
         case MENU_ENUM_LABEL_INPUT_TOUCH_VMOUSE_TRACKBALL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_touch_vmouse_trackball);
            break;
         case MENU_ENUM_LABEL_INPUT_TOUCH_VMOUSE_GESTURE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_touch_vmouse_gesture);
            break;
#endif
         case MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mixer_volume);
#endif
            break;
         case MENU_ENUM_LABEL_AUDIO_MIXER_MUTE:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mixer_mute);
#endif
            break;
#if TARGET_OS_IOS
         case MENU_ENUM_LABEL_AUDIO_RESPECT_SILENT_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_respect_silent_mode);
            break;
#endif
         case MENU_ENUM_LABEL_CONNECT_BLUETOOTH:
#ifdef HAVE_BLUETOOTH
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bluetooth_list);
#endif
            break;
#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_room);
            break;
         case MENU_ENUM_LABEL_NETPLAY_KICK_CLIENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_kick_client);
            break;
         case MENU_ENUM_LABEL_NETPLAY_BAN_CLIENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_kick_client);
            break;
#endif
#ifdef HAVE_CHEEVOS
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_achievement_list);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU:
            BIND_ACTION_SUBLABEL(cbs, menu_action_sublabel_achievement_pause_menu);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_achievement_pause_cancel);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_achievement_resume_cancel);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_achievement_resume_requires_reload);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_SERVER_UNREACHABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_achievement_server_unreachable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY:
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY_HARDCORE:
         case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY:
         case MENU_ENUM_LABEL_CHEEVOS_UNSUPPORTED_ENTRY:
         case MENU_ENUM_LABEL_CHEEVOS_UNOFFICIAL_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_entry);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_test_unofficial);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_hardcore_mode_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_CHALLENGE_INDICATORS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_challenge_indicators);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_RICHPRESENCE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_richpresence_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_BADGES_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_badges_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCK_SOUND_ENABLE:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_unlock_sound_enable);
#endif
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_verbose_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_AUTO_SCREENSHOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_auto_screenshot);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_START_ACTIVE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_start_active);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_appearance_settings);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_ANCHOR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_appearance_anchor);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_PADDING_AUTO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_appearance_padding_auto);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_PADDING_H:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_appearance_padding_h);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_PADDING_V:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_appearance_padding_v);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_settings);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_SUMMARY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_summary);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_UNLOCK:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_unlock);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_MASTERY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_mastery);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_ACCOUNT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_account);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_progress_tracker);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_LBOARD_START:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_lboard_start);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_lboard_submit);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_lboard_cancel);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_visibility_lboard_trackers);
            break;
#endif
#if defined(HAVE_WINDOW_OFFSET)
         case MENU_ENUM_LABEL_VIDEO_WINDOW_OFFSET_X:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_window_offset_x);
            break;
         case MENU_ENUM_LABEL_VIDEO_WINDOW_OFFSET_Y:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_window_offset_y);
            break;
#endif
#ifdef _3DS
         case MENU_ENUM_LABEL_MENU_BOTTOM_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_bottom_settings_list);
            break;
#endif
#ifdef HAVE_MICROPHONE
         case MENU_ENUM_LABEL_MICROPHONE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_microphone_settings_list);
            break;
#endif
         case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_mixer_settings_list);
#endif
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE:
            {
               const char *menu_ident             = menu_driver_ident();
               /* Uses same sublabels as MENU_ENUM_LABEL_THUMBNAILS */
#ifdef HAVE_RGUI
               if (string_is_equal(menu_ident, "rgui"))
               {
                  BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails_rgui);
               }
               else
#endif
#ifdef HAVE_MATERIALUI
                  if (string_is_equal(menu_ident, "glui"))
                  {
                     BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails_materialui);
                  }
                  else
#endif
                  {
                     BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails);
                  }
            }
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE:
            {
               const char *menu_ident             = menu_driver_ident();
               /* Uses same sublabels as MENU_ENUM_LABEL_LEFT_THUMBNAILS */
#ifdef HAVE_RGUI
               if (string_is_equal(menu_ident, "rgui"))
               {
                  BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_rgui);
               }
               else
#endif
#ifdef HAVE_OZONE
                  if (string_is_equal(menu_ident, "ozone"))
                  {
                     BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_ozone);
                  }
                  else
#endif
#ifdef HAVE_MATERIALUI
                     if (string_is_equal(menu_ident, "glui"))
                     {
                        BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_materialui);
                     }
                     else
#endif
                     {
                        BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails);
                     }
            }
            break;
         case MENU_ENUM_LABEL_BLUETOOTH_SETTINGS:
#ifdef HAVE_BLUETOOTH
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bluetooth_settings_list);
#endif
            break;
#ifdef HAVE_LAKKA
         case MENU_ENUM_LABEL_LAKKA_SERVICES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_services_settings_list);
            break;
         case MENU_ENUM_LABEL_SSH_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ssh_enable);
            break;
         case MENU_ENUM_LABEL_SAMBA_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_samba_enable);
            break;
         case MENU_ENUM_LABEL_BLUETOOTH_ENABLE:
#ifdef HAVE_BLUETOOTH
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bluetooth_enable);
#endif
            break;
         case MENU_ENUM_LABEL_LOCALAP_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_localap_enable);
            break;
#ifdef HAVE_RETROFLAG
         case MENU_ENUM_LABEL_SAFESHUTDOWN_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_safeshutdown_enable);
            break;
#endif
         case MENU_ENUM_LABEL_TIMEZONE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_timezone);
            break;
         case MENU_ENUM_LABEL_CPU_POLICY_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cpu_policy_entry_list);
            break;
         case MENU_ENUM_LABEL_CPU_PERF_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cpu_perf_mode);
            break;
#endif
#ifdef HAVE_LAKKA_SWITCH
         case MENU_ENUM_LABEL_LAKKA_SWITCH_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_options);
            break;
         case MENU_ENUM_LABEL_SWITCH_OC_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_oc_enable);
            break;
         case MENU_ENUM_LABEL_SWITCH_CEC_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_cec_enable);
            break;
         case MENU_ENUM_LABEL_BLUETOOTH_ERTM_DISABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bluetooth_ertm_disable);
            break;
#endif
#ifdef HAVE_VIDEO_FILTER
         case MENU_ENUM_LABEL_VIDEO_FILTER_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_filter_enable);
            break;
#endif
#ifdef HAVE_QT
         case MENU_ENUM_LABEL_SHOW_WIMP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_show_wimp);
            break;
#endif
#if defined(HAVE_LIBNX)
         case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_cpu_profile);
            break;
#endif
#ifndef HAVE_LAKKA
         case MENU_ENUM_LABEL_GAMEMODE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_gamemode_enable);
            break;
#endif /*HAVE_LAKKA*/
#ifdef _3DS
         case MENU_ENUM_LABEL_NEW3DS_SPEEDUP_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_new3ds_speedup_enable);
            break;
         case MENU_ENUM_LABEL_VIDEO_3DS_DISPLAY_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_3ds_display_mode);
            break;
         case MENU_ENUM_LABEL_VIDEO_3DS_LCD_BOTTOM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_3ds_lcd_bottom);
            break;
         case MENU_ENUM_LABEL_BOTTOM_ASSETS_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bottom_assets_directory);
            break;
         case MENU_ENUM_LABEL_BOTTOM_FONT_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bottom_font_enable);
            break;
         case MENU_ENUM_LABEL_BOTTOM_FONT_COLOR_RED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bottom_font_color_red);
            break;
         case MENU_ENUM_LABEL_BOTTOM_FONT_COLOR_GREEN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bottom_font_color_green);
            break;
         case MENU_ENUM_LABEL_BOTTOM_FONT_COLOR_BLUE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bottom_font_color_blue);
            break;
         case MENU_ENUM_LABEL_BOTTOM_FONT_COLOR_OPACITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bottom_font_color_opacity);
            break;
         case MENU_ENUM_LABEL_BOTTOM_FONT_SCALE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bottom_font_scale);
            break;
#endif
#if defined(WIIU)
         case MENU_ENUM_LABEL_VIDEO_WIIU_PREFER_DRC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_wiiu_prefer_drc);
            break;
#endif
#if defined(GEKKO)
         case MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_TOP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_overscan_correction_top);
            break;
         case MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_overscan_correction_bottom);
            break;
#endif
#if defined(HAVE_WINDOW_OFFSET)
         case MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_window_offset_x);
            break;
         case MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_window_offset_y);
            break;
#endif
         case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
#ifdef HAVE_CHEATS
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_apply_after_load);
#endif
            break;
#ifdef HAVE_GAME_AI
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_GAME_AI:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_game_ai);
            break;
         case MENU_ENUM_LABEL_CORE_GAME_AI_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_game_ai_options);
            break;
         case MENU_ENUM_LABEL_GAME_AI_OVERRIDE_P1:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_game_ai_override_p1);
            break;
         case MENU_ENUM_LABEL_GAME_AI_OVERRIDE_P2:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_game_ai_override_p2);
            break;
         case MENU_ENUM_LABEL_GAME_AI_SHOW_DEBUG:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_game_ai_show_debug);
            break;
#endif
#ifdef HAVE_SMBCLIENT
         case MENU_ENUM_LABEL_SMB_CLIENT_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_settings);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_enable);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_AUTH_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_auth_mode);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_SERVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_server);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_SHARE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_share);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_SUBDIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_subdir);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_USERNAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_username);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_PASSWORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_password);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_WORKGROUP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_workgroup);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_NUM_CONTEXTS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_num_contexts);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_TIMEOUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_timeout);
            break;
         case MENU_ENUM_LABEL_SMB_CLIENT_BROWSE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_smb_client_browse);
            break;
#endif
         default:
            return -1;
         }
   }
   else
   {
      /* Per-port entries require string match against label.
       * Some cases may be detected above by "type", but not all. */
      typedef struct info_single_list
      {
         unsigned label_idx;
         int (*cb)(file_list_t *list,
            unsigned type, unsigned i,
            const char *label, const char *path,
            char *s, size_t len);
      } info_single_list_t;

      /* Entries with %u player index placeholder. */
      static const info_single_list_t info_list[] = {
         {
            MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE,
            action_bind_sublabel_input_adc_type
         },
         {
            MENU_ENUM_LABEL_INPUT_DEVICE_RESERVATION_TYPE,
            action_bind_sublabel_input_device_reservation_type
         },
         {
            MENU_ENUM_LABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
            action_bind_sublabel_input_device_reserved_device_name
         },
         {
            MENU_ENUM_LABEL_INPUT_MOUSE_INDEX,
            action_bind_sublabel_input_mouse_index
         },
         {
            MENU_ENUM_LABEL_INPUT_JOYPAD_INDEX,
            action_bind_sublabel_input_device_index
         },
         {
            MENU_ENUM_LABEL_INPUT_BIND_ALL_INDEX,
            action_bind_sublabel_input_bind_all
         },
         {
            MENU_ENUM_LABEL_INPUT_SAVE_AUTOCONFIG_INDEX,
            action_bind_sublabel_input_save_autoconfig
         },
         {
            MENU_ENUM_LABEL_INPUT_BIND_DEFAULTS_INDEX,
            action_bind_sublabel_input_bind_defaults
         },
      };

      const char* idx_placeholder = "%u";
      for (i = 0; i < ARRAY_SIZE(info_list); i++)
      {
         int idxpos      = string_find_index_substring_string(msg_hash_to_str((enum msg_hash_enums)info_list[i].label_idx), idx_placeholder);
         size_t _lbl_len = strlen(msg_hash_to_str((enum msg_hash_enums)info_list[i].label_idx));
         if (   (idxpos > 0)
              && string_starts_with_size(label, msg_hash_to_str((enum msg_hash_enums)info_list[i].label_idx), idxpos)
              && (((size_t)idxpos == _lbl_len - 2)
              || ((size_t)idxpos  <  _lbl_len - 2
              && string_ends_with_size(label,
                  msg_hash_to_str((enum msg_hash_enums)info_list[i].label_idx) + idxpos + 2,
                  lbl_len,
                  _lbl_len - idxpos - 2))))
         {
            BIND_ACTION_SUBLABEL(cbs, info_list[i].cb);
            return 0;
         }
      }
   }

   return 0;
}
