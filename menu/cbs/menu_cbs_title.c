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

#include <lists/string_list.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include <compat/strl.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"

#include "../../retroarch.h"
#include "../../configuration.h"
#include "../managers/core_option_manager.h"

#ifndef BIND_ACTION_GET_TITLE
#define BIND_ACTION_GET_TITLE(cbs, name) \
   cbs->action_get_title = name; \
   cbs->action_get_title_ident = #name;
#endif

#define sanitize_to_string(s, label, len) \
   { \
      char *pos = NULL; \
      strlcpy(s, label, len); \
      while((pos = strchr(s, '_'))) \
         *pos = ' '; \
   }

static int action_get_title_action_generic(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   if (s && !string_is_empty(label))
   {
      sanitize_to_string(s, label, len);
   }
   return 1;
}

#define default_title_macro(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   const char *str = msg_hash_to_str(lbl); \
   if (s && !string_is_empty(str)) \
   { \
      sanitize_to_string(s, str, len); \
   } \
   return 1; \
}

#define default_fill_title_macro(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   const char *title = msg_hash_to_str(lbl); \
   if (!string_is_empty(path) && !string_is_empty(title)) \
      fill_pathname_join_delim(s, title, path, ' ', len); \
   else if (!string_is_empty(title)) \
      strlcpy(s, title, len); \
   return 1; \
}

#define default_title_copy_macro(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   strlcpy(s, msg_hash_to_str(lbl), len); \
   return 1; \
}

static int action_get_title_remap_port(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   char lbl[128];
   snprintf(lbl, sizeof(lbl), "Port %d Controls", atoi(path) + 1);
   sanitize_to_string(s, lbl, len);
   return 1;
}

static int action_get_title_thumbnails(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   settings_t *settings            = config_get_ptr();
   const char *title               = NULL;
   enum msg_hash_enums label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS;

   /* Get label value */
   if (string_is_equal(settings->arrays.menu_driver, "rgui"))
      label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI;
   else if (string_is_equal(settings->arrays.menu_driver, "glui"))
      label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI;

   title = msg_hash_to_str(label_value);

   if (s && !string_is_empty(title))
   {
      sanitize_to_string(s, title, len);
      return 1;
   }

   return 0;
}

static int action_get_title_left_thumbnails(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   settings_t *settings            = config_get_ptr();
   const char *title               = NULL;
   enum msg_hash_enums label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS;

   /* Get label value */
   if (string_is_equal(settings->arrays.menu_driver, "rgui"))
      label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI;
   else if (string_is_equal(settings->arrays.menu_driver, "ozone"))
      label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE;
   else if (string_is_equal(settings->arrays.menu_driver, "glui"))
      label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI;

   title = msg_hash_to_str(label_value);

   if (s && !string_is_empty(title))
   {
      sanitize_to_string(s, title, len);
      return 1;
   }

   return 0;
}

static int action_get_title_dropdown_item(const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   /* Sanity check */
   if (string_is_empty(path))
      return 0;

   if (strstr(path, "core_option_"))
   {
      /* This is a core options item */
      struct string_list *tmp_str_list = string_split(path, "_");
      int ret                          = 0;

      if (tmp_str_list && tmp_str_list->size > 0)
      {
         core_option_manager_t *coreopts = NULL;

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            settings_t *settings            = config_get_ptr();
            unsigned menu_index             = string_to_unsigned(tmp_str_list->elems[(unsigned)tmp_str_list->size - 1].data);
            unsigned visible_index          = 0;
            unsigned option_index           = 0;
            bool option_found               = false;
            unsigned i;

            /* Convert menu index to option index */
            if (settings->bools.game_specific_options)
               menu_index--;

            for (i = 0; i < coreopts->size; i++)
            {
               if (core_option_manager_get_visible(coreopts, i))
               {
                  if (visible_index == menu_index)
                  {
                     option_found = true;
                     option_index = i;
                     break;
                  }
                  visible_index++;
               }
            }

            /* If option was found, title == option description */
            if (option_found)
            {
               const char *title = core_option_manager_get_desc(coreopts, option_index);

               if (s && !string_is_empty(title))
               {
                  sanitize_to_string(s, title, len);
                  ret = 1;
               }
            }
         }
      }

      /* Clean up */
      if (tmp_str_list)
         string_list_free(tmp_str_list);

      return ret;
   }
   else
   {
      /* This is a 'normal' drop down list */

      /* In msg_hash.h, msg_hash_enums are generated via
       * the following macro:
       *    #define MENU_LABEL(STR) \
       *       MENU_ENUM_LABEL_##STR, \
       *       MENU_ENUM_SUBLABEL_##STR, \
       *       MENU_ENUM_LABEL_VALUE_##STR
       * to get 'MENU_ENUM_LABEL_VALUE_' from a
       * 'MENU_ENUM_LABEL_', we therefore add 2... */
      enum msg_hash_enums enum_idx = (enum msg_hash_enums)(string_to_unsigned(path) + 2);

      /* Check if enum index is valid
       * Note: This is a very crude check, but better than nothing */
      if ((enum_idx > MSG_UNKNOWN) && (enum_idx < MSG_LAST))
      {
         /* An annoyance: MENU_ENUM_LABEL_THUMBNAILS and
          * MENU_ENUM_LABEL_LEFT_THUMBNAILS require special
          * treatment, since their titles depend upon the
          * current menu driver... */
         if (enum_idx == MENU_ENUM_LABEL_VALUE_THUMBNAILS)
            return action_get_title_thumbnails(path, label, menu_type, s, len);
         else if (enum_idx == MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS)
            return action_get_title_left_thumbnails(path, label, menu_type, s, len);
         else
         {
            const char *title = msg_hash_to_str(enum_idx);

            if (s && !string_is_empty(title))
            {
               sanitize_to_string(s, title, len);
               return 1;
            }
         }
      }
   }

   return 0;
}

#ifdef HAVE_AUDIOMIXER
static int action_get_title_mixer_stream_actions(const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   unsigned         offset      = (menu_type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_BEGIN);

   snprintf(s, len, "Mixer Stream #%d: %s", offset + 1, audio_driver_mixer_get_stream_name(offset));
   return 0;
}
#endif

static int action_get_title_deferred_playlist_list(const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   const char *playlist_file = NULL;

   if (string_is_empty(path))
      return 0;

   playlist_file = path_basename(path);

   if (string_is_empty(playlist_file))
      return 0;

   if (string_is_equal_noncase(path_get_extension(playlist_file),
            "lpl"))
   {
      /* Handle content history */
      if (string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_HISTORY)))
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB), len);
      /* Handle favourites */
      else if (string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_FAVORITES)))
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB), len);
      /* Handle collection playlists */
      else
      {
         char playlist_name[PATH_MAX_LENGTH];

         playlist_name[0] = '\0';

         strlcpy(playlist_name, playlist_file, sizeof(playlist_name));
         path_remove_extension(playlist_name);

         strlcpy(s, playlist_name, len);
      }
   }
   /* This should never happen, but if it does just set
    * the label to the file name (it's better than nothing...) */
   else
      strlcpy(s, playlist_file, len);

   return 0;
}

static int action_get_title_dropdown_playlist_right_thumbnail_mode_item(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   return action_get_title_thumbnails(path, label, menu_type, s, len);
}

static int action_get_title_dropdown_playlist_left_thumbnail_mode_item(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   return action_get_title_left_thumbnails(path, label, menu_type, s, len);
}

default_title_macro(action_get_quick_menu_override_options,     MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS)
default_title_macro(action_get_user_accounts_cheevos_list,      MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS)
default_title_macro(action_get_user_accounts_youtube_list,      MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE)
default_title_macro(action_get_user_accounts_twitch_list,       MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH)
default_title_macro(action_get_download_core_content_list,      MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT)
default_title_macro(action_get_user_accounts_list,              MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST)
default_title_macro(action_get_core_information_list,           MENU_ENUM_LABEL_VALUE_CORE_INFORMATION)
default_title_macro(action_get_core_list,                       MENU_ENUM_LABEL_VALUE_CORE_LIST)
default_title_macro(action_get_online_updater_list,             MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER)
default_title_macro(action_get_netplay_list,                    MENU_ENUM_LABEL_VALUE_NETPLAY)
default_title_macro(action_get_online_thumbnails_updater_list,  MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST)
default_title_macro(action_get_online_pl_thumbnails_updater_list, MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST)
default_title_macro(action_get_core_updater_list,               MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST)
default_title_macro(action_get_add_content_list,                MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST)
default_title_macro(action_get_configurations_list,             MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST)
default_title_macro(action_get_core_options_list,               MENU_ENUM_LABEL_VALUE_CORE_OPTIONS)
default_title_macro(action_get_load_recent_list,                MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY)
default_title_macro(action_get_quick_menu_list,                 MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS)
default_title_macro(action_get_input_remapping_options_list,    MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS)
default_title_macro(action_get_shader_options_list,             MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS)
default_title_macro(action_get_disk_options_list,               MENU_ENUM_LABEL_VALUE_DISK_OPTIONS)
default_title_macro(action_get_frontend_counters_list,          MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS)
default_title_macro(action_get_core_counters_list,              MENU_ENUM_LABEL_VALUE_CORE_COUNTERS)
default_title_macro(action_get_recording_settings_list,         MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS)
default_title_macro(action_get_playlist_settings_list,          MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS)
default_title_macro(action_get_playlist_manager_list,           MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST)
default_title_macro(action_get_input_hotkey_binds_settings_list,MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS)
default_title_macro(action_get_driver_settings_list,            MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS)
default_title_macro(action_get_core_settings_list,              MENU_ENUM_LABEL_VALUE_CORE_SETTINGS)
default_title_macro(action_get_video_settings_list,             MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS)
default_title_macro(action_get_video_fullscreen_mode_settings_list,     MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS)
default_title_macro(action_get_video_windowed_mode_settings_list,     MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS)
default_title_macro(action_get_video_scaling_settings_list,     MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS)
default_title_macro(action_get_video_output_settings_list,      MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS)
default_title_macro(action_get_video_synchronization_settings_list,      MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS)
default_title_macro(action_get_input_menu_settings_list,      MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS)
default_title_macro(action_get_input_haptic_feedback_settings_list,      MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS)
default_title_macro(action_get_crt_switchres_settings_list,     MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS)
default_title_macro(action_get_configuration_settings_list,     MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS)
default_title_macro(action_get_load_disc_list,                  MENU_ENUM_LABEL_VALUE_LOAD_DISC)
default_title_macro(action_get_dump_disc_list,                  MENU_ENUM_LABEL_VALUE_DUMP_DISC)
default_title_macro(action_get_saving_settings_list,            MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS)
default_title_macro(action_get_logging_settings_list,           MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS)
default_title_macro(action_get_frame_throttle_settings_list,    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS)
default_title_macro(action_get_frame_time_counter_settings_list,    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS)
default_title_macro(action_get_rewind_settings_list,            MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS)
default_title_macro(action_get_cheat_details_settings_list,     MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS)
default_title_macro(action_get_cheat_search_settings_list,      MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS)
default_title_macro(action_get_onscreen_display_settings_list,  MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS)
default_title_macro(action_get_onscreen_notifications_settings_list,  MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS)
default_title_macro(action_get_onscreen_overlay_settings_list,  MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS)
#ifdef HAVE_VIDEO_LAYOUT
default_title_macro(action_get_onscreen_video_layout_settings_list, MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS)
#endif
default_title_macro(action_get_menu_views_settings_list,        MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS)
default_title_macro(action_get_settings_views_settings_list,  MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS)
default_title_macro(action_get_quick_menu_views_settings_list,  MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS)
default_title_macro(action_get_menu_settings_list,              MENU_ENUM_LABEL_VALUE_MENU_SETTINGS)
default_title_macro(action_get_user_interface_settings_list,    MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS)
default_title_macro(action_get_ai_service_settings_list,    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS)
default_title_macro(action_get_accessibility_settings_list,    MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS)
default_title_macro(action_get_power_management_settings_list,  MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS)
default_title_macro(action_get_menu_sounds_list,                MENU_ENUM_LABEL_VALUE_MENU_SOUNDS)
default_title_macro(action_get_menu_file_browser_settings_list, MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS)
default_title_macro(action_get_retro_achievements_settings_list,MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS)
default_title_macro(action_get_wifi_settings_list,              MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS)
default_title_macro(action_get_network_hosting_settings_list,           MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS)
default_title_macro(action_get_subsystem_settings_list,           MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS)
default_title_macro(action_get_network_settings_list,           MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS)
default_title_macro(action_get_netplay_lan_scan_settings_list,  MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS)
#ifdef HAVE_LAKKA
default_title_macro(action_get_lakka_services_list,             MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES)
#endif
default_title_macro(action_get_user_settings_list,              MENU_ENUM_LABEL_VALUE_USER_SETTINGS)
default_title_macro(action_get_directory_settings_list,         MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS)
default_title_macro(action_get_privacy_settings_list,           MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS)
default_title_macro(action_get_midi_settings_list,              MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS)
default_title_macro(action_get_updater_settings_list,           MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS)
default_title_macro(action_get_audio_settings_list,             MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS)
default_title_macro(action_get_audio_resampler_settings_list,             MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS)
default_title_macro(action_get_audio_output_settings_list,             MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS)
default_title_macro(action_get_audio_synchronization_settings_list,             MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS)
#ifdef HAVE_AUDIOMIXER
default_title_macro(action_get_audio_mixer_settings_list,       MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS)
#endif
default_title_macro(action_get_input_settings_list,             MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS)
default_title_macro(action_get_latency_settings_list,           MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS)
default_title_macro(action_get_core_cheat_options_list,         MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS)
default_title_macro(action_get_load_content_list,               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST)
default_title_macro(action_get_load_content_special,            MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_SPECIAL)
default_title_macro(action_get_cursor_manager_list,             MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER)
default_title_macro(action_get_database_manager_list,           MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER)
default_title_macro(action_get_system_information_list,         MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION)
default_title_macro(action_get_disc_information_list,        MENU_ENUM_LABEL_VALUE_DISC_INFORMATION)
default_title_macro(action_get_network_information_list,        MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION)
default_title_macro(action_get_settings_list,                   MENU_ENUM_LABEL_VALUE_SETTINGS)
default_title_macro(action_get_title_information_list,          MENU_ENUM_LABEL_VALUE_INFORMATION_LIST)
default_title_macro(action_get_title_information,               MENU_ENUM_LABEL_VALUE_INFORMATION)
default_title_macro(action_get_title_goto_favorites,            MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES)
default_title_macro(action_get_title_goto_image,                MENU_ENUM_LABEL_VALUE_GOTO_IMAGES)
default_title_macro(action_get_title_goto_music,                MENU_ENUM_LABEL_VALUE_GOTO_MUSIC)
default_title_macro(action_get_title_goto_video,                MENU_ENUM_LABEL_VALUE_GOTO_VIDEO)
default_title_macro(action_get_title_collection,                MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
default_title_macro(action_get_title_deferred_core_list,        MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES)
default_title_macro(action_get_title_dropdown_resolution_item,  MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION)
default_title_macro(action_get_title_dropdown_video_shader_num_pass_item,  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES)
default_title_macro(action_get_title_dropdown_video_shader_parameter_item,  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS)
default_title_macro(action_get_title_dropdown_video_shader_preset_parameter_item,  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS)
default_title_macro(action_get_title_dropdown_playlist_default_core_item, MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE)
default_title_macro(action_get_title_dropdown_playlist_label_display_mode_item, MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE)
default_title_macro(action_get_title_manual_content_scan_list,  MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST)
default_title_macro(action_get_title_dropdown_manual_content_scan_system_name_item, MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME)
default_title_macro(action_get_title_dropdown_manual_content_scan_core_name_item, MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME)
default_title_macro(action_get_title_dropdown_disk_index, MENU_ENUM_LABEL_VALUE_DISK_INDEX)

default_fill_title_macro(action_get_title_disk_image_append,    MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND)
default_fill_title_macro(action_get_title_cheat_file_load,      MENU_ENUM_LABEL_VALUE_CHEAT_FILE)
default_fill_title_macro(action_get_title_cheat_file_load_append, MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND)
default_fill_title_macro(action_get_title_remap_file_load,      MENU_ENUM_LABEL_VALUE_REMAP_FILE)
default_fill_title_macro(action_get_title_overlay,              MENU_ENUM_LABEL_VALUE_OVERLAY)
default_fill_title_macro(action_get_title_video_filter,         MENU_ENUM_LABEL_VALUE_VIDEO_FILTER)
default_fill_title_macro(action_get_title_cheat_directory,      MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH)
default_fill_title_macro(action_get_title_core_directory,       MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
default_fill_title_macro(action_get_title_core_info_directory,  MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH)
default_fill_title_macro(action_get_title_audio_filter,         MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR)
default_fill_title_macro(action_get_title_video_shader_preset,  MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO)
default_fill_title_macro(action_get_title_configurations,       MENU_ENUM_LABEL_VALUE_CONFIG)
default_fill_title_macro(action_get_title_content_database_directory,   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY)
default_fill_title_macro(action_get_title_savestate_directory,          MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY)
default_fill_title_macro(action_get_title_dynamic_wallpapers_directory, MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY)
default_fill_title_macro(action_get_title_core_assets_directory, MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR)
default_fill_title_macro(action_get_title_config_directory,      MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY)
default_fill_title_macro(action_get_title_thumbnail_directory,    MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY)
default_fill_title_macro(action_get_title_input_remapping_directory,    MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY)
default_fill_title_macro(action_get_title_autoconfig_directory,  MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR )
default_fill_title_macro(action_get_title_playlist_directory,    MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY)
default_fill_title_macro(action_get_title_runtime_log_directory, MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY)
default_fill_title_macro(action_get_title_browser_directory,     MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY)
default_fill_title_macro(action_get_title_content_directory,     MENU_ENUM_LABEL_VALUE_CONTENT_DIR)
default_fill_title_macro(action_get_title_screenshot_directory,  MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY)
default_fill_title_macro(action_get_title_cursor_directory,      MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY)
default_fill_title_macro(action_get_title_onscreen_overlay_keyboard_directory, MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY)
default_fill_title_macro(action_get_title_recording_config_directory, MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY)
default_fill_title_macro(action_get_title_recording_output_directory, MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY)
default_fill_title_macro(action_get_title_video_shader_directory, MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR)
default_fill_title_macro(action_get_title_audio_filter_directory, MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR)
default_fill_title_macro(action_get_title_video_filter_directory, MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR)
default_fill_title_macro(action_get_title_savefile_directory,     MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY)
default_fill_title_macro(action_get_title_overlay_directory,      MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY)
#ifdef HAVE_VIDEO_LAYOUT
default_fill_title_macro(action_get_title_video_layout_directory, MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY)
#endif
default_fill_title_macro(action_get_title_system_directory,       MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY)
default_fill_title_macro(action_get_title_assets_directory,       MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY)
default_fill_title_macro(action_get_title_extraction_directory,   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY)
default_fill_title_macro(action_get_title_menu,                   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS)
default_fill_title_macro(action_get_title_font_path,              MENU_ENUM_LABEL_VALUE_XMB_FONT)
default_fill_title_macro(action_get_title_log_dir,                MENU_ENUM_LABEL_VALUE_LOG_DIR)
default_fill_title_macro(action_get_title_manual_content_scan_dir, MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR)

default_title_copy_macro(action_get_title_help,                   MENU_ENUM_LABEL_VALUE_HELP_LIST)
default_title_copy_macro(action_get_title_input_settings,         MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS)
default_title_copy_macro(action_get_title_cheevos_list,           MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST)
default_title_copy_macro(action_get_title_video_shader_parameters,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS)
default_title_copy_macro(action_get_title_video_shader_preset_parameters,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS)
default_title_copy_macro(action_get_title_video_shader_preset_save,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE)
default_title_copy_macro(action_get_title_video_shader_preset_remove,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE)
default_title_copy_macro(action_get_title_video_shader_preset_save_list,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE)

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
default_title_macro(action_get_title_switch_cpu_profile,          MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE)
#endif

#ifdef HAVE_LAKKA_SWITCH
default_title_macro(action_get_title_switch_gpu_profile,          MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE)
default_title_macro(action_get_title_switch_backlight_control,    MENU_ENUM_LABEL_VALUE_SWITCH_BACKLIGHT_CONTROL)
#endif

static int action_get_title_generic(char *s, size_t len, const char *path,
      const char *text)
{
   struct string_list *list_path    = NULL;

   if (!string_is_empty(path))
      list_path = string_split(path, "|");

   if (list_path)
   {
      char elem0_path[255];

      elem0_path[0] = '\0';

      if (list_path->size > 0)
         strlcpy(elem0_path, list_path->elems[0].data, sizeof(elem0_path));
      string_list_free(list_path);
      strlcpy(s, text, len);

      if (!string_is_empty(elem0_path))
      {
         strlcat(s, "- ", len);
         strlcat(s, path_basename(elem0_path), len);
      }
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);

   return 0;
}

#define default_title_generic_macro(func_name, lbl) \
   static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
  { \
   return action_get_title_generic(s, len, path, msg_hash_to_str(lbl)); \
} \

default_title_generic_macro(action_get_title_deferred_database_manager_list,MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION)
default_title_generic_macro(action_get_title_deferred_cursor_manager_list,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST)
default_title_generic_macro(action_get_title_list_rdb_entry_developer,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER)
default_title_generic_macro(action_get_title_list_rdb_entry_publisher,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER)
default_title_generic_macro(action_get_title_list_rdb_entry_origin,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN)
default_title_generic_macro(action_get_title_list_rdb_entry_franchise,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE)
default_title_generic_macro(action_get_title_list_rdb_entry_edge_magazine_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING)
default_title_generic_macro(action_get_title_list_rdb_entry_edge_magazine_issue,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE)
default_title_generic_macro(action_get_title_list_rdb_entry_releasedate_by_month,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH)
default_title_generic_macro(action_get_title_list_rdb_entry_releasedate_by_year,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR)
default_title_generic_macro(action_get_title_list_rdb_entry_esrb_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING)
default_title_generic_macro(action_get_title_list_rdb_entry_elspa_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING)
default_title_generic_macro(action_get_title_list_rdb_entry_pegi_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING)
default_title_generic_macro(action_get_title_list_rdb_entry_cero_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING)
default_title_generic_macro(action_get_title_list_rdb_entry_bbfc_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING)
default_title_generic_macro(action_get_title_list_rdb_entry_max_users,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS)
default_title_generic_macro(action_get_title_list_rdb_entry_database_info,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO)

static int action_get_sideload_core_list(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST), len);
   strlcat(s, " ", len);
   if (!string_is_empty(path))
      strlcat(s, path, len);
   return 0;
}

static int action_get_title_default(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SELECT_FILE), len);
   strlcat(s, " ", len);
   if (!string_is_empty(path))
      strlcat(s, path, len);
   return 0;
}

static int action_get_title_group_settings(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   unsigned i;
   typedef struct title_info_list 
   {
      enum msg_hash_enums type;
      enum msg_hash_enums val;
   } title_info_list_t;

   title_info_list_t info_list[] = {
      {MENU_ENUM_LABEL_MAIN_MENU,         MENU_ENUM_LABEL_VALUE_MAIN_MENU}       ,
      {MENU_ENUM_LABEL_HISTORY_TAB,       MENU_ENUM_LABEL_VALUE_HISTORY_TAB}     ,
      {MENU_ENUM_LABEL_FAVORITES_TAB,     MENU_ENUM_LABEL_VALUE_FAVORITES_TAB}   ,
      {MENU_ENUM_LABEL_IMAGES_TAB,        MENU_ENUM_LABEL_VALUE_IMAGES_TAB   }   ,
      {MENU_ENUM_LABEL_MUSIC_TAB,         MENU_ENUM_LABEL_VALUE_MUSIC_TAB    }   ,
      {MENU_ENUM_LABEL_VIDEO_TAB,         MENU_ENUM_LABEL_VALUE_VIDEO_TAB    }   ,
      {MENU_ENUM_LABEL_SETTINGS_TAB,      MENU_ENUM_LABEL_VALUE_SETTINGS_TAB }   ,
      {MENU_ENUM_LABEL_PLAYLISTS_TAB,     MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB}   ,
      {MENU_ENUM_LABEL_ADD_TAB,           MENU_ENUM_LABEL_VALUE_ADD_TAB      }   ,
      {MENU_ENUM_LABEL_NETPLAY_TAB,       MENU_ENUM_LABEL_VALUE_NETPLAY_TAB  }   ,
      {MENU_ENUM_LABEL_HORIZONTAL_MENU,   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU },
   };

   for (i = 0; i < ARRAY_SIZE(info_list); i++)
   {
      if (string_is_equal(label, msg_hash_to_str(info_list[i].type)))
      {
         strlcpy(s, msg_hash_to_str(info_list[i].val), len);
         return 0;
      }
   }

   {
      char elem0[255];
      char elem1[255];
      struct string_list *list_label = string_split(label, "|");

      elem0[0] = elem1[0] = '\0';

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

      strlcpy(s, elem0, len);

      if (!string_is_empty(elem1))
      {
         strlcat(s, " - ", len);
         strlcat(s, elem1, len);
      }
   }

   return 0;
}

static int action_get_title_input_binds_list(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   unsigned val = (((unsigned)path[0]) - 49) + 1;
   snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS), val);
   return 0;
}

static int menu_cbs_init_bind_title_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash)
{
   unsigned i;
   typedef struct title_info_list 
   {
      enum msg_hash_enums type;
      int (*cb)(const char *path, const char *label,
            unsigned type, char *s, size_t len);
   } title_info_list_t;

   title_info_list_t info_list[] = {
      {MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST,                 action_get_title_remap_port},
      {MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST,                   action_get_core_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_DUMP_DISC_LIST,                       action_get_dump_disc_list},
      {MENU_ENUM_LABEL_DEFERRED_LOAD_DISC_LIST,                       action_get_load_disc_list},
      {MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST,          action_get_configuration_settings_list },
      {MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST,                 action_get_saving_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST,                action_get_logging_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_FRAME_TIME_COUNTER_SETTINGS_LIST,     action_get_frame_time_counter_settings_list },
      {MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST,         action_get_frame_throttle_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST,                 action_get_rewind_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CHEAT_DETAILS_SETTINGS_LIST,          action_get_cheat_details_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CHEAT_SEARCH_SETTINGS_LIST,           action_get_cheat_search_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST,       action_get_onscreen_display_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST, action_get_onscreen_notifications_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST,       action_get_onscreen_overlay_settings_list},
#ifdef HAVE_VIDEO_LAYOUT
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST,  action_get_onscreen_video_layout_settings_list},
#endif
      {MENU_ENUM_LABEL_DEFERRED_MENU_VIEWS_SETTINGS_LIST,             action_get_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SETTINGS_VIEWS_SETTINGS_LIST,         action_get_settings_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_VIEWS_SETTINGS_LIST,       action_get_quick_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST,                   action_get_menu_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST,         action_get_user_interface_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AI_SERVICE_SETTINGS_LIST,             action_get_ai_service_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCESSIBILITY_SETTINGS_LIST,             action_get_accessibility_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_POWER_MANAGEMENT_SETTINGS_LIST,       action_get_power_management_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_SOUNDS_LIST,                     action_get_menu_sounds_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST,      action_get_menu_file_browser_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST,     action_get_retro_achievements_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST,                   action_get_wifi_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST,                action_get_updater_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETWORK_HOSTING_SETTINGS_LIST,                action_get_network_hosting_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SUBSYSTEM_SETTINGS_LIST,                action_get_subsystem_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST,                action_get_network_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETPLAY_LAN_SCAN_SETTINGS_LIST,       action_get_netplay_lan_scan_settings_list},
#ifdef HAVE_LAKKA
      {MENU_ENUM_LABEL_DEFERRED_LAKKA_SERVICES_LIST,                  action_get_lakka_services_list},
#endif
      {MENU_ENUM_LABEL_DEFERRED_USER_SETTINGS_LIST,                   action_get_user_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST,              action_get_directory_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_PRIVACY_SETTINGS_LIST,                action_get_privacy_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_MIDI_SETTINGS_LIST,                   action_get_midi_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST,               action_get_download_core_content_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST,        action_get_download_core_content_list},
      {MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST,                       action_get_title_goto_favorites},
      {MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST,                          action_get_title_goto_image},
      {MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST,                           action_get_title_goto_music},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST,                           action_get_title_goto_video},
      {MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST,                 action_get_driver_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST,                  action_get_audio_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_RESAMPLER_SETTINGS_LIST,                  action_get_audio_resampler_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_OUTPUT_SETTINGS_LIST,                  action_get_audio_output_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SYNCHRONIZATION_SETTINGS_LIST,                  action_get_audio_synchronization_settings_list},
#ifdef HAVE_AUDIOMIXER
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_MIXER_SETTINGS_LIST,            action_get_audio_mixer_settings_list},
#endif
      {MENU_ENUM_LABEL_DEFERRED_LATENCY_SETTINGS_LIST,                action_get_latency_settings_list},
      {MENU_ENUM_LABEL_SYSTEM_INFORMATION,                            action_get_system_information_list},
      {MENU_ENUM_LABEL_DISC_INFORMATION,                              action_get_disc_information_list},
      {MENU_ENUM_LABEL_NETWORK_INFORMATION,                           action_get_network_information_list},
      {MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_OVERRIDE_OPTIONS,          action_get_quick_menu_override_options},
      {MENU_ENUM_LABEL_DEFERRED_CRT_SWITCHRES_SETTINGS_LIST,          action_get_crt_switchres_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_TWITCH_LIST,                 action_get_user_accounts_twitch_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_YOUTUBE_LIST,                action_get_user_accounts_youtube_list},
      {MENU_ENUM_LABEL_ONLINE_UPDATER,                                action_get_online_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST,              action_get_recording_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SCALING_SETTINGS_LIST,              action_get_video_scaling_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_OUTPUT_SETTINGS_LIST,              action_get_video_output_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SYNCHRONIZATION_SETTINGS_LIST,              action_get_video_synchronization_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_MENU_SETTINGS_LIST,              action_get_input_menu_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST,              action_get_input_haptic_feedback_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_WINDOWED_MODE_SETTINGS_LIST,              action_get_video_windowed_mode_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST,              action_get_video_fullscreen_mode_settings_list},
      {MENU_ENUM_LABEL_SIDELOAD_CORE_LIST, action_get_sideload_core_list},
   };

   if (cbs->setting)
   {
      const char *parent_group   = cbs->setting->parent_group;

      if (string_is_equal(parent_group, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))
            && setting_get_type(cbs->setting) == ST_GROUP)
      {
         BIND_ACTION_GET_TITLE(cbs, action_get_title_group_settings);
         return 0;
      }
   }

   for (i = 0; i < ARRAY_SIZE(info_list); i++)
   {
      if (string_is_equal(label, msg_hash_to_str(info_list[i].type)))
      {
         BIND_ACTION_GET_TITLE(cbs, info_list[i].cb);
         return 0;
      }
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_database_manager_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_cursor_manager_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_developer);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_publisher);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_origin);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_franchise);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_edge_magazine_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_edge_magazine_issue);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_releasedate_by_month);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_releasedate_by_year);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_esrb_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_elspa_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_pegi_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_cero_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_bbfc_rating);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_max_users);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_LIST:
         case MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_core_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_configurations);
            break;
         case MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_autoconfig_directory);
            break;
         case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_extraction_directory);
            break;
         case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_system_directory);
            break;
         case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_assets_directory);
            break;
         case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_savefile_directory);
            break;
         case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_overlay_directory);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_layout_directory);
            break;
#endif
         case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_browser_directory);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_playlist_directory);
            break;
         case MENU_ENUM_LABEL_RUNTIME_LOG_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_runtime_log_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_directory);
            break;
         case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_screenshot_directory);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_directory);
            break;
         case MENU_ENUM_LABEL_VIDEO_FILTER_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_filter_directory);
            break;
         case MENU_ENUM_LABEL_AUDIO_FILTER_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_audio_filter_directory);
            break;
         case MENU_ENUM_LABEL_CURSOR_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cursor_directory);
            break;
         case MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_recording_config_directory);
            break;
         case MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_recording_output_directory);
            break;
         case MENU_ENUM_LABEL_OSK_OVERLAY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_onscreen_overlay_keyboard_directory);
            break;
         case MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_input_remapping_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_database_directory);
            break;
         case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_savestate_directory);
            break;
         case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_dynamic_wallpapers_directory);
            break;
         case MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_core_assets_directory);
            break;
         case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_thumbnail_directory);
            break;
         case MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_config_directory);
            break;
         case MENU_ENUM_LABEL_LOG_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_log_dir);
            break;
         case MENU_ENUM_LABEL_INFORMATION_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_information_list);
            break;
         case MENU_ENUM_LABEL_INFORMATION:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_information);
            break;
         case MENU_ENUM_LABEL_SETTINGS:
            BIND_ACTION_GET_TITLE(cbs, action_get_settings_list);
            break;
         case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_database_manager_list);
            break;
         case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_cursor_manager_list);
            break;
         case MENU_ENUM_LABEL_CORE_INFORMATION:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_information_list);
            break;
         case MENU_ENUM_LABEL_CORE_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_list);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL:
            BIND_ACTION_GET_TITLE(cbs, action_get_load_content_special);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_load_content_list);
            break;
         case MENU_ENUM_LABEL_NETPLAY:
            BIND_ACTION_GET_TITLE(cbs, action_get_netplay_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_online_thumbnails_updater_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_PL_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_online_pl_thumbnails_updater_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_updater_list);
            break;
         case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_add_content_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_configurations_list);
            break;
         case MENU_ENUM_LABEL_CORE_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_options_list);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_load_recent_list);
            break;
         case MENU_ENUM_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_GET_TITLE(cbs, action_get_quick_menu_list);
            break;
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_input_remapping_options_list);
            break;
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_cheat_options_list);
            break;
         case MENU_ENUM_LABEL_SHADER_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_shader_options_list);
            break;
         case MENU_ENUM_LABEL_DISK_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_disk_options_list);
            break;
         case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_frontend_counters_list);
            break;
         case MENU_ENUM_LABEL_CORE_COUNTERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_counters_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_input_binds_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_input_hotkey_binds_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_video_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_configuration_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_logging_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_saving_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_frame_throttle_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_rewind_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CHEAT_DETAILS_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_cheat_details_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CHEAT_SEARCH_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_cheat_search_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_onscreen_display_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_onscreen_overlay_settings_list);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_DEFERRED_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_onscreen_video_layout_settings_list);
            break;
#endif
         case MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_input_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_recording_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_playlist_settings_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_playlist_manager_list);
            break;
         case MENU_ENUM_LABEL_MANAGEMENT:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_action_generic);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_action_generic);
            break;
         case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_disk_image_append);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_preset);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_file_load);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_file_load_append);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_remap_file_load);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_cheevos_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_TWITCH_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_twitch_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_YOUTUBE_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_youtube_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST:
         case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_download_core_content_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_list);
            break;
         case MENU_ENUM_LABEL_HELP_LIST:
         case MENU_ENUM_LABEL_HELP:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_help);
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_overlay);
            break;
         case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
         case MENU_ENUM_LABEL_XMB_FONT:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_font_path);
            break;
         case MENU_ENUM_LABEL_VIDEO_FILTER:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_filter);
            break;
         case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_audio_filter);
            break;
         case MENU_ENUM_LABEL_CHEAT_DATABASE_PATH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_directory);
            break;
         case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_core_directory);
            break;
         case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_core_info_directory);
            break;
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
         case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_switch_cpu_profile);
            break;
#endif
#ifdef HAVE_LAKKA_SWITCH
         case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_switch_gpu_profile);
            break;
         case MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_switch_backlight_control);
            break;
#endif
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_manual_content_scan_list);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_manual_content_scan_dir);
            break;
         default:
            return -1;
      }
   }
   else
   {
      switch (label_hash)
      {
         case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_database_manager_list);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_cursor_manager_list);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_developer);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_publisher);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_origin);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_franchise);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_edge_magazine_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_edge_magazine_issue);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_releasedate_by_month);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_releasedate_by_year);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_esrb_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_elspa_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_pegi_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_cero_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_bbfc_rating);
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_max_users);
            break;
         case MENU_LABEL_DEFERRED_CORE_LIST:
         case MENU_LABEL_DEFERRED_CORE_LIST_SET:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_core_list);
            break;
         case MENU_LABEL_CONFIGURATIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_configurations);
            break;
         case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_autoconfig_directory);
            break;
         case MENU_LABEL_CACHE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_extraction_directory);
            break;
         case MENU_LABEL_SYSTEM_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_system_directory);
            break;
         case MENU_LABEL_ASSETS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_assets_directory);
            break;
         case MENU_LABEL_SAVEFILE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_savefile_directory);
            break;
         case MENU_LABEL_OVERLAY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_overlay_directory);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_LABEL_VIDEO_LAYOUT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_layout_directory);
            break;
#endif
         case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_browser_directory);
            break;
         case MENU_LABEL_PLAYLIST_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_playlist_directory);
            break;
         case MENU_LABEL_RUNTIME_LOG_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_runtime_log_directory);
            break;
         case MENU_LABEL_CONTENT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_directory);
            break;
         case MENU_LABEL_SCREENSHOT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_screenshot_directory);
            break;
         case MENU_LABEL_VIDEO_SHADER_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_directory);
            break;
         case MENU_LABEL_VIDEO_FILTER_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_filter_directory);
            break;
         case MENU_LABEL_AUDIO_FILTER_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_audio_filter_directory);
            break;
         case MENU_LABEL_CURSOR_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cursor_directory);
            break;
         case MENU_LABEL_RECORDING_CONFIG_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_recording_config_directory);
            break;
         case MENU_LABEL_RECORDING_OUTPUT_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_recording_output_directory);
            break;
         case MENU_LABEL_OSK_OVERLAY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_onscreen_overlay_keyboard_directory);
            break;
         case MENU_LABEL_INPUT_REMAPPING_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_input_remapping_directory);
            break;
         case MENU_LABEL_CONTENT_DATABASE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_database_directory);
            break;
         case MENU_LABEL_SAVESTATE_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_savestate_directory);
            break;
         case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_dynamic_wallpapers_directory);
            break;
         case MENU_LABEL_CORE_ASSETS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_core_assets_directory);
            break;
         case MENU_LABEL_THUMBNAILS_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_thumbnail_directory);
            break;
         case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_config_directory);
            break;
         case MENU_LABEL_LOG_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_log_dir);
            break;
         case MENU_LABEL_INFORMATION_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_information_list);
            break;
         case MENU_LABEL_INFORMATION:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_information);
            break;
         case MENU_LABEL_SETTINGS:
            BIND_ACTION_GET_TITLE(cbs, action_get_settings_list);
            break;
         case MENU_LABEL_DATABASE_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_database_manager_list);
            break;
         case MENU_LABEL_CURSOR_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_cursor_manager_list);
            break;
         case MENU_LABEL_CORE_INFORMATION:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_information_list);
            break;
         case MENU_LABEL_CORE_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_list);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL:
            BIND_ACTION_GET_TITLE(cbs, action_get_load_content_special);
            break;
         case MENU_LABEL_LOAD_CONTENT_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_load_content_list);
            break;
         case MENU_LABEL_NETPLAY:
            BIND_ACTION_GET_TITLE(cbs, action_get_netplay_list);
            break;
         case MENU_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_online_thumbnails_updater_list);
            break;
         case MENU_LABEL_DEFERRED_PL_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_online_pl_thumbnails_updater_list);
            break;
         case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_updater_list);
            break;
         case MENU_LABEL_DEFERRED_CONFIGURATIONS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_configurations_list);
            break;
         case MENU_LABEL_ADD_CONTENT_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_add_content_list);
            break;
         case MENU_LABEL_CORE_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_options_list);
            break;
         case MENU_LABEL_LOAD_CONTENT_HISTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_load_recent_list);
            break;
         case MENU_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_GET_TITLE(cbs, action_get_quick_menu_list);
            break;
         case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_input_remapping_options_list);
            break;
         case MENU_LABEL_CORE_CHEAT_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_cheat_options_list);
            break;
         case MENU_LABEL_SHADER_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_shader_options_list);
            break;
         case MENU_LABEL_DISK_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_disk_options_list);
            break;
         case MENU_LABEL_FRONTEND_COUNTERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_frontend_counters_list);
            break;
         case MENU_LABEL_CORE_COUNTERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_counters_list);
            break;
         case MENU_LABEL_DEFERRED_USER_BINDS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_input_binds_list);
            break;
         case MENU_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_input_hotkey_binds_settings_list);
            break;
         case MENU_LABEL_DEFERRED_VIDEO_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_video_settings_list);
            break;
         case MENU_LABEL_DEFERRED_INPUT_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_input_settings_list);
            break;
         case MENU_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_playlist_settings_list);
            break;
         case MENU_LABEL_DEFERRED_PLAYLIST_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_playlist_manager_list);
            break;
         case MENU_LABEL_ACHIEVEMENT_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cheevos_list);
            break;
         case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_parameters);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_preset_parameters);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_preset_save);
            break;
         case MENU_LABEL_MANAGEMENT:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_action_generic);
            break;
         case MENU_LABEL_DISK_IMAGE_APPEND:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_disk_image_append);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_preset);
            break;
         case MENU_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_file_load);
            break;
         case MENU_LABEL_CHEAT_FILE_LOAD_APPEND:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_file_load_append);
            break;
         case MENU_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_remap_file_load);
            break;
         case MENU_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_cheevos_list);
            break;
         case MENU_LABEL_DEFERRED_CORE_CONTENT_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_download_core_content_list);
            break;
         case MENU_LABEL_DEFERRED_ACCOUNTS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_list);
            break;
         case MENU_LABEL_HELP_LIST:
         case MENU_LABEL_HELP:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_help);
            break;
         case MENU_LABEL_INPUT_OVERLAY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_overlay);
            break;
         case MENU_LABEL_VIDEO_FONT_PATH:
         case MENU_LABEL_XMB_FONT:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_font_path);
            break;
         case MENU_LABEL_VIDEO_FILTER:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_filter);
            break;
         case MENU_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_audio_filter);
            break;
         case MENU_LABEL_CHEAT_DATABASE_PATH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_directory);
            break;
         case MENU_LABEL_LIBRETRO_DIR_PATH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_core_directory);
            break;
         case MENU_LABEL_LIBRETRO_INFO_PATH:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_core_info_directory);
            break;
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
         case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_switch_cpu_profile);
            break;
#endif
#ifdef HAVE_LAKKA_SWITCH
         case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_switch_gpu_profile);
            break;
         case MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_switch_backlight_control);
            break;
#endif
         case MENU_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_manual_content_scan_list);
            break;
         case MENU_LABEL_MANUAL_CONTENT_SCAN_DIR:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_manual_content_scan_dir);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_title_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
   switch (type)
   {
      case MENU_SETTINGS:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_menu);
         break;
      case MENU_SETTINGS_CUSTOM_BIND:
      case MENU_SETTINGS_CUSTOM_BIND_KEYBOARD:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_input_settings);
         break;
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_disk_options_list);
         break;
      default:
         return -1;
   }

   return 0;
}

int menu_cbs_init_bind_title(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      uint32_t label_hash)
{
   unsigned i;
   typedef struct title_info_list 
   {
      enum msg_hash_enums type;
      int (*cb)(const char *path, const char *label,
            unsigned type, char *s, size_t len);
   } title_info_list_t;

   title_info_list_t info_list[] = {
#ifdef HAVE_AUDIOMIXER
      {MENU_ENUM_LABEL_DEFERRED_MIXER_STREAM_SETTINGS_LIST,                    action_get_title_mixer_stream_actions},
#endif
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_SAVE_LIST,                 action_get_title_video_shader_preset_save_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_REMOVE_LIST,               action_get_title_video_shader_preset_remove},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST,                             action_get_title_dropdown_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL,                     action_get_title_dropdown_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION,                  action_get_title_dropdown_resolution_item   },
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PARAMETER,                  action_get_title_dropdown_video_shader_parameter_item   },
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PRESET_PARAMETER,                  action_get_title_dropdown_video_shader_preset_parameter_item   },
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES,                  action_get_title_dropdown_video_shader_num_pass_item   },
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE,       action_get_title_dropdown_playlist_default_core_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE, action_get_title_dropdown_playlist_label_display_mode_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE, action_get_title_dropdown_playlist_right_thumbnail_mode_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE, action_get_title_dropdown_playlist_left_thumbnail_mode_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME, action_get_title_dropdown_manual_content_scan_system_name_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME, action_get_title_dropdown_manual_content_scan_core_name_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX, action_get_title_dropdown_disk_index},
      {MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS, action_get_quick_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST, action_get_title_deferred_playlist_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_SETTINGS, action_get_title_deferred_playlist_list},
      {MENU_ENUM_LABEL_PLAYLISTS_TAB, action_get_title_collection},
      {MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST, action_get_title_manual_content_scan_list},
   };

   if (!cbs)
      return -1;

   BIND_ACTION_GET_TITLE(cbs, action_get_title_default);

   if (cbs->enum_idx != MENU_ENUM_LABEL_PLAYLIST_ENTRY &&
       menu_cbs_init_bind_title_compare_label(cbs, label, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_title_compare_type(cbs, type) == 0)
      return 0;

   for (i = 0; i < ARRAY_SIZE(info_list); i++)
   {
      if (string_is_equal(label, msg_hash_to_str(info_list[i].type)))
      {
         BIND_ACTION_GET_TITLE(cbs, info_list[i].cb);
         return 0;
      }
   }

   /* MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL requires special
    * treatment, since the label has the format:
    *   <MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL>|<entry_name>
    * i.e. cannot use a normal string_is_equal() */
   if (strstr(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL)))
   {
      BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_database_info);
      return 0;
   }

   return -1;
}
