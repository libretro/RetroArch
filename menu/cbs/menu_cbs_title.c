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
#include "../../core_option_manager.h"
#include "../../core_info.h"

#ifdef HAVE_CHEEVOS
#include "../../cheevos/cheevos.h"
#endif

#ifndef BIND_ACTION_GET_TITLE
#define BIND_ACTION_GET_TITLE(cbs, name) (cbs)->action_get_title = (name)
#endif

#define DEFAULT_TITLE_GENERIC_MACRO(func_name, lbl) \
   static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
  { \
   return action_get_title_generic(s, len, path, msg_hash_to_str(lbl)); \
} \

#define SANITIZE_TO_STRING(s, label, len) \
   { \
      char *pos = NULL; \
      strlcpy(s, label, len); \
      while ((pos = strchr(s, '_'))) \
         *pos = ' '; \
   }

#define DEFAULT_TITLE_MACRO(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   const char *str = msg_hash_to_str(lbl); \
   if (s && !string_is_empty(str)) \
   { \
      SANITIZE_TO_STRING(s, str, len); \
   } \
   return 1; \
}

#define DEFAULT_FILL_TITLE_MACRO(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   const char *title = msg_hash_to_str(lbl); \
   if (!string_is_empty(path) && !string_is_empty(title)) \
      fill_pathname_join_delim(s, title, path, ' ', len); \
   else if (!string_is_empty(title)) \
      strlcpy(s, title, len); \
   return 1; \
}

#define DEFAULT_TITLE_COPY_MACRO(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   strlcpy(s, msg_hash_to_str(lbl), len); \
   return 1; \
}

static void action_get_title_fill_search_filter_default(
      enum msg_hash_enums lbl, char *s, size_t len)
{
   /* Copy label value */
   strlcpy(s, msg_hash_to_str(lbl), len);

   /* Add current search terms */
   menu_entries_search_append_terms_string(s, len);
}

#define DEFAULT_TITLE_SEARCH_FILTER_MACRO(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   action_get_title_fill_search_filter_default(lbl, s, len); \
   return 0; \
}

static void action_get_title_fill_path_search_filter_default(
      const char *path, enum msg_hash_enums lbl, char *s, size_t len)
{
   const char *title = msg_hash_to_str(lbl);

   snprintf(s, len, "%s %s", 
         string_is_empty(title) ? "" : title,
         string_is_empty(path)  ? "" : path
         );

   menu_entries_search_append_terms_string(s, len);
}

#define DEFAULT_FILL_TITLE_SEARCH_FILTER_MACRO(func_name, lbl) \
  static int (func_name)(const char *path, const char *label, unsigned menu_type, char *s, size_t len) \
{ \
   action_get_title_fill_path_search_filter_default(path, lbl, s, len); \
   return 0; \
}

static int action_get_title_action_generic(
      const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   if (s && !string_is_empty(label))
   {
      SANITIZE_TO_STRING(s, label, len);
   }
   return 1;
}

static int action_get_title_remap_port(
      const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   char lbl[128];
   snprintf(lbl, sizeof(lbl),
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS),
         atoi(path) + 1);
   SANITIZE_TO_STRING(s, lbl, len);
   return 1;
}

static int action_get_title_thumbnails(
      const char *path, const char *label, unsigned menu_type,
      char *s, size_t len)
{
   const char *title               = NULL;
   enum msg_hash_enums label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS;
   const char *menu_ident          = menu_driver_ident();
   /* Get label value */
#ifdef HAVE_RGUI
   if (string_is_equal(menu_ident, "rgui"))
      label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI;
#endif
#ifdef HAVE_MATERIALUI
   if (string_is_equal(menu_ident, "glui"))
      label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI;
#endif

   title = msg_hash_to_str(label_value);

   if (s && !string_is_empty(title))
   {
      SANITIZE_TO_STRING(s, title, len);
      return 1;
   }

   return 0;
}

static int action_get_title_left_thumbnails(
      const char *path, const char *label, unsigned menu_type,
      char *s, size_t len)
{
   const char *title               = NULL;
   enum msg_hash_enums label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS;
#if defined(HAVE_RGUI) || defined(HAVE_OZONE) || defined(HAVE_MATERIALUI)
   const char *menu_ident          = menu_driver_ident();
   /* Get label value */
#ifdef HAVE_RGUI
   if (string_is_equal(menu_ident, "rgui"))
      label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI;
#endif
#ifdef HAVE_OZONE
   if (string_is_equal(menu_ident, "ozone"))
      label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE;
#endif
#ifdef HAVE_MATERIALUI
   if (string_is_equal(menu_ident, "glui"))
      label_value = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI;
#endif
#endif

   title = msg_hash_to_str(label_value);

   if (s && !string_is_empty(title))
   {
      SANITIZE_TO_STRING(s, title, len);
      return 1;
   }

   return 0;
}

static int action_get_title_core_options_list(
      const char *path, const char *label, unsigned menu_type,
      char *s, size_t len)
{
   const char *category = path;
   const char *title    = NULL;

   /* If this is an options subcategory, fetch
    * the category description */
   if (!string_is_empty(category))
   {
      core_option_manager_t *coreopts = NULL;

      if (rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
         title = core_option_manager_get_category_desc(
               coreopts, category);
   }

   /* If this isn't a subcategory (or something
    * went wrong...), use top level core options
    * menu label */
   if (string_is_empty(title))
      title = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS);

   if (s && !string_is_empty(title))
   {
      strlcpy(s, title, len);
      return 1;
   }

   return 0;
}

static int action_get_title_dropdown_item(
      const char *path, const char *label, unsigned menu_type,
      char *s, size_t len)
{
   /* Sanity check */
   if (string_is_empty(path))
      return 0;

   if (string_starts_with_size(path, "core_option_",
         STRLEN_CONST("core_option_")))
   {
      /* This is a core options item */
      struct string_list tmp_str_list = {0};
      core_option_manager_t *coreopts = NULL;
      int ret                         = 0;

      string_list_initialize(&tmp_str_list);
      string_split_noalloc(&tmp_str_list, path, "_");

      if (tmp_str_list.size > 0)
      {
         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            unsigned option_index = string_to_unsigned(
                  tmp_str_list.elems[(unsigned)tmp_str_list.size - 1].data);
            const char *title     = core_option_manager_get_desc(
                  coreopts, option_index, true);

            if (s && !string_is_empty(title))
            {
               strlcpy(s, title, len);
               ret = 1;
            }
         }
      }

      /* Clean up */
      string_list_deinitialize(&tmp_str_list);

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
      enum msg_hash_enums enum_idx = (enum msg_hash_enums)
         (string_to_unsigned(path) + 2);

      /* Check if enum index is valid
       * Note: This is a very crude check, but better than nothing */
      if ((enum_idx > MSG_UNKNOWN) && (enum_idx < MSG_LAST))
      {
         /* An annoyance: MENU_ENUM_LABEL_THUMBNAILS and
          * MENU_ENUM_LABEL_LEFT_THUMBNAILS require special
          * treatment, since their titles depend upon the
          * current menu driver... */
         switch (enum_idx)
         {
            case MENU_ENUM_LABEL_VALUE_THUMBNAILS:
               return action_get_title_thumbnails(
                     path, label, menu_type, s, len);
            case MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS:
               return action_get_title_left_thumbnails(
                     path, label, menu_type, s, len);
            default:
               {
                  /* Submenu label exceptions */
                  /* Device Type */
                  if ((enum_idx >= MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE) &&
                      (enum_idx <= MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE_LAST))
                     enum_idx = MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE;

                  /* Analog to Digital Type */
                  if ((enum_idx >= MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE) &&
                      (enum_idx <= MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE_LAST))
                     enum_idx = MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE;

                  /* Device Index */
                  if ((enum_idx >= MENU_ENUM_LABEL_INPUT_DEVICE_INDEX) &&
                      (enum_idx <= MENU_ENUM_LABEL_INPUT_DEVICE_INDEX_LAST))
                     enum_idx = MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX;

                  /* Mouse Index */
                  if ((enum_idx >= MENU_ENUM_LABEL_INPUT_MOUSE_INDEX) &&
                      (enum_idx <= MENU_ENUM_LABEL_INPUT_MOUSE_INDEX_LAST))
                     enum_idx = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX;

                  /* Mapped Port (virtual -> 'physical' port mapping) */
                  if ((enum_idx >= MENU_ENUM_LABEL_INPUT_REMAP_PORT) &&
                      (enum_idx <= MENU_ENUM_LABEL_INPUT_REMAP_PORT_LAST))
                     enum_idx = MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT;

                  {
                     const char *title = msg_hash_to_str(enum_idx);

                     if (s && !string_is_empty(title))
                     {
                        SANITIZE_TO_STRING(s, title, len);
                        return 1;
                     }
                  }
               }
               break;
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

   playlist_file = path_basename_nocompression(path);

   if (string_is_empty(playlist_file))
      return 0;

   if (string_is_equal_noncase(path_get_extension(playlist_file),
            "lpl"))
   {
      /* Handle content history */
      if (string_is_equal(playlist_file, FILE_PATH_CONTENT_HISTORY))
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB), len);
      /* Handle favourites */
      else if (string_is_equal(playlist_file, FILE_PATH_CONTENT_FAVORITES))
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

   /* Add current search terms */
   menu_entries_search_append_terms_string(s, len);

   return 0;
}

static int action_get_title_deferred_core_backup_list(
      const char *core_path, const char *prefix, char *s, size_t len)
{
   core_info_t *core_info = NULL;

   if (string_is_empty(core_path) || string_is_empty(prefix))
      return 0;

   /* Search for specified core
    * > If core is found, add display name */
   if (core_info_find(core_path, &core_info) &&
       core_info->display_name)
      snprintf(s, len, "%s: %s", prefix,
            core_info->display_name);
   else
   {
      /* > If not, use core file name */
      const char *core_filename = path_basename_nocompression(core_path);
      if (!string_is_empty(core_filename))
         snprintf(s, len, "%s: %s", prefix,
               core_filename);
      else
         snprintf(s, len, "%s: ", prefix);
   }

   return 1;
}

static int action_get_title_deferred_core_restore_backup_list(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   return action_get_title_deferred_core_backup_list(path,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST),
         s, len);
}

static int action_get_title_deferred_core_delete_backup_list(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   return action_get_title_deferred_core_backup_list(path,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST),
         s, len);
}

static int action_get_core_information_list(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   core_info_t *core_info = NULL;

   /* Check whether we are parsing information for a
    * core updater/manager entry or the currently loaded core */
   if ((menu_type == FILE_TYPE_DOWNLOAD_CORE) ||
       (menu_type == MENU_SETTING_ACTION_CORE_MANAGER_OPTIONS))
   {
      core_info_t *core_info_menu = NULL;

      if (string_is_empty(path))
         goto error;

      /* Core updater/manager entry - search for
       * corresponding core info */
      if (core_info_find(path, &core_info_menu))
         core_info = core_info_menu;
   }
   else
      core_info_get_current_core(&core_info);

   if (!core_info || string_is_empty(core_info->display_name))
      goto error;

   /* Copy display name */
   strlcpy(s, core_info->display_name, len);
   return 1;

error:
   /* An unknown error has occurred - just set the
    * title to the legacy 'Core Information' string */
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION), len);
   return 0;
}

static int action_get_title_dropdown_input_description_common(
      const char *input_name, unsigned port, char *s, size_t len)
{
   const char *input_label_ptr = input_name;
   char input_label[256];

   input_label[0] = '\0';

   if (!string_is_empty(input_label_ptr))
   {
      /* Strip off 'Auto:' prefix, if required */
      if (string_starts_with_size(input_label_ptr, "Auto:",
            STRLEN_CONST("Auto:")))
         input_label_ptr += STRLEN_CONST("Auto:");

      strlcpy(input_label, input_label_ptr,
            sizeof(input_label));

      string_trim_whitespace_left(input_label);
   }

   /* Sanity check */
   if (string_is_empty(input_label))
      strlcpy(input_label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
            sizeof(input_label));

   /* Build title string */
   snprintf(s, len, "%s %u - %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT),
         port + 1,
         input_label);

   return 1;
}

static int action_get_title_dropdown_input_description(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   unsigned port = (menu_type - MENU_SETTINGS_INPUT_DESC_BEGIN) /
         (RARCH_FIRST_CUSTOM_BIND + 8);

   return action_get_title_dropdown_input_description_common(
      path, port, s, len);
}

static int action_get_title_dropdown_input_description_kbd(
      const char *path, const char *label, unsigned menu_type, char *s, size_t len)
{
   unsigned port = (menu_type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) /
         RARCH_FIRST_CUSTOM_BIND;

   return action_get_title_dropdown_input_description_common(
      path, port, s, len);
}

#ifdef HAVE_CHEEVOS
static int action_get_title_achievement_pause_menu(
      const char* path, const char* label, unsigned menu_type, char* s, size_t len)
{
   if (rcheevos_hardcore_active())
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME), len);
   return 1;
}
#endif

DEFAULT_TITLE_MACRO(action_get_quick_menu_override_options,     MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS)
DEFAULT_TITLE_MACRO(action_get_user_accounts_cheevos_list,      MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS)
DEFAULT_TITLE_MACRO(action_get_user_accounts_youtube_list,      MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE)
DEFAULT_TITLE_MACRO(action_get_user_accounts_twitch_list,       MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH)
DEFAULT_TITLE_MACRO(action_get_user_accounts_facebook_list,     MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK)
DEFAULT_TITLE_MACRO(action_get_download_core_content_list,      MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT)
DEFAULT_TITLE_MACRO(action_get_user_accounts_list,              MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST)
DEFAULT_TITLE_MACRO(action_get_core_list,                       MENU_ENUM_LABEL_VALUE_CORE_LIST)
DEFAULT_TITLE_MACRO(action_get_online_updater_list,             MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER)
DEFAULT_TITLE_MACRO(action_get_netplay_list,                    MENU_ENUM_LABEL_VALUE_NETPLAY)
DEFAULT_TITLE_MACRO(action_get_online_thumbnails_updater_list,  MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST)
DEFAULT_TITLE_MACRO(action_get_online_pl_thumbnails_updater_list, MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST)
DEFAULT_TITLE_MACRO(action_get_add_content_list,                MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST)
DEFAULT_TITLE_MACRO(action_get_configurations_list,             MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST)
DEFAULT_TITLE_MACRO(action_get_core_option_override_list,       MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST)
DEFAULT_TITLE_MACRO(action_get_quick_menu_list,                 MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_input_remapping_options_list,    MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS)
DEFAULT_TITLE_MACRO(action_get_shader_options_list,             MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS)
DEFAULT_TITLE_MACRO(action_get_disk_options_list,               MENU_ENUM_LABEL_VALUE_DISK_OPTIONS)
DEFAULT_TITLE_MACRO(action_get_frontend_counters_list,          MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS)
DEFAULT_TITLE_MACRO(action_get_core_counters_list,              MENU_ENUM_LABEL_VALUE_CORE_COUNTERS)
DEFAULT_TITLE_MACRO(action_get_recording_settings_list,         MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_playlist_settings_list,          MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_playlist_manager_list,           MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST)
DEFAULT_TITLE_MACRO(action_get_input_hotkey_binds_settings_list,MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS)
DEFAULT_TITLE_MACRO(action_get_driver_settings_list,            MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_core_settings_list,              MENU_ENUM_LABEL_VALUE_CORE_SETTINGS)

DEFAULT_TITLE_MACRO(action_get_video_settings_list,                 MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_video_fullscreen_mode_settings_list, MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_video_windowed_mode_settings_list,   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_video_hdr_settings_list,             MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_video_scaling_settings_list,         MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_video_output_settings_list,          MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_video_synchronization_settings_list, MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS)

DEFAULT_TITLE_MACRO(action_get_input_menu_settings_list,            MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_input_turbo_fire_settings_list,      MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_input_haptic_feedback_settings_list, MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS)

DEFAULT_TITLE_MACRO(action_get_crt_switchres_settings_list,     MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_configuration_settings_list,     MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_load_disc_list,                  MENU_ENUM_LABEL_VALUE_LOAD_DISC)
DEFAULT_TITLE_MACRO(action_get_dump_disc_list,                  MENU_ENUM_LABEL_VALUE_DUMP_DISC)
DEFAULT_TITLE_MACRO(action_get_saving_settings_list,            MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_logging_settings_list,           MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_frame_throttle_settings_list,    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_frame_time_counter_settings_list, MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_rewind_settings_list,            MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_cheat_details_settings_list,     MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_cheat_search_settings_list,      MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_onscreen_display_settings_list,  MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_onscreen_notifications_settings_list, MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_onscreen_notifications_views_settings_list, MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_onscreen_overlay_settings_list,  MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS)
#ifdef HAVE_VIDEO_LAYOUT
DEFAULT_TITLE_MACRO(action_get_onscreen_video_layout_settings_list, MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS)
#endif
DEFAULT_TITLE_MACRO(action_get_menu_views_settings_list,        MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_settings_views_settings_list,    MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_quick_menu_views_settings_list,  MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_menu_settings_list,              MENU_ENUM_LABEL_VALUE_MENU_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_user_interface_settings_list,    MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_ai_service_settings_list,        MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_accessibility_settings_list,     MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_power_management_settings_list,  MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_cpu_perfpower_settings_list,     MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER)
DEFAULT_TITLE_MACRO(action_get_cpu_policy_entry_list,           MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY)
DEFAULT_TITLE_MACRO(action_get_menu_sounds_list,                MENU_ENUM_LABEL_VALUE_MENU_SOUNDS)
DEFAULT_TITLE_MACRO(action_get_menu_file_browser_settings_list, MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_retro_achievements_settings_list,MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_bluetooth_settings_list,         MENU_ENUM_LABEL_VALUE_BLUETOOTH_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_wifi_networks_list,              MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS)
DEFAULT_TITLE_MACRO(action_get_wifi_settings_list,              MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_network_hosting_settings_list,   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_subsystem_settings_list,         MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_network_settings_list,           MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_netplay_lan_scan_settings_list,  MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS)
#ifdef HAVE_LAKKA
DEFAULT_TITLE_MACRO(action_get_lakka_services_list,             MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES)
#endif
DEFAULT_TITLE_MACRO(action_get_user_settings_list,              MENU_ENUM_LABEL_VALUE_USER_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_directory_settings_list,         MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_privacy_settings_list,           MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_midi_settings_list,              MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_updater_settings_list,           MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS)

DEFAULT_TITLE_MACRO(action_get_audio_settings_list,                 MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_audio_resampler_settings_list,       MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_audio_output_settings_list,          MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_audio_synchronization_settings_list, MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS)
#ifdef HAVE_AUDIOMIXER
DEFAULT_TITLE_MACRO(action_get_audio_mixer_settings_list,           MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS)
#endif
DEFAULT_TITLE_MACRO(action_get_input_settings_list,             MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_latency_settings_list,           MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_load_content_list,               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST)
DEFAULT_TITLE_MACRO(action_get_load_content_special,            MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_SPECIAL)
DEFAULT_TITLE_MACRO(action_get_cursor_manager_list,             MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER)
DEFAULT_TITLE_MACRO(action_get_database_manager_list,           MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER)
DEFAULT_TITLE_MACRO(action_get_system_information_list,         MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION)
DEFAULT_TITLE_MACRO(action_get_disc_information_list,           MENU_ENUM_LABEL_VALUE_DISC_INFORMATION)
DEFAULT_TITLE_MACRO(action_get_network_information_list,        MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION)
DEFAULT_TITLE_MACRO(action_get_settings_list,                   MENU_ENUM_LABEL_VALUE_SETTINGS)
DEFAULT_TITLE_MACRO(action_get_title_information_list,          MENU_ENUM_LABEL_VALUE_INFORMATION_LIST)
DEFAULT_TITLE_MACRO(action_get_title_information,               MENU_ENUM_LABEL_VALUE_INFORMATION)
DEFAULT_TITLE_MACRO(action_get_title_collection,                MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
DEFAULT_TITLE_MACRO(action_get_title_deferred_core_list,        MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_resolution_item,  MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_video_shader_num_pass_item,  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_video_shader_parameter_item,  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_video_shader_preset_parameter_item,  MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_playlist_default_core_item, MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_playlist_label_display_mode_item, MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_playlist_sort_mode_item, MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE)
DEFAULT_TITLE_MACRO(action_get_title_manual_content_scan_list,  MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_manual_content_scan_system_name_item, MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_manual_content_scan_core_name_item, MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME)
DEFAULT_TITLE_MACRO(action_get_title_dropdown_disk_index, MENU_ENUM_LABEL_VALUE_DISK_INDEX)

DEFAULT_FILL_TITLE_MACRO(action_get_title_disk_image_append,    MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND)
DEFAULT_FILL_TITLE_MACRO(action_get_title_remap_file_load,      MENU_ENUM_LABEL_VALUE_REMAP_FILE)
DEFAULT_FILL_TITLE_MACRO(action_get_title_video_filter,         MENU_ENUM_LABEL_VALUE_VIDEO_FILTER)
DEFAULT_FILL_TITLE_MACRO(action_get_title_cheat_directory,      MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH)
DEFAULT_FILL_TITLE_MACRO(action_get_title_core_directory,       MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
DEFAULT_FILL_TITLE_MACRO(action_get_title_core_info_directory,  MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH)
DEFAULT_FILL_TITLE_MACRO(action_get_title_audio_filter,         MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN)
DEFAULT_FILL_TITLE_MACRO(action_get_title_configurations,       MENU_ENUM_LABEL_VALUE_CONFIG)
DEFAULT_FILL_TITLE_MACRO(action_get_title_content_database_directory,   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_savestate_directory,          MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_dynamic_wallpapers_directory, MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_core_assets_directory, MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR)
DEFAULT_FILL_TITLE_MACRO(action_get_title_config_directory,      MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_thumbnail_directory,    MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_input_remapping_directory,    MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_autoconfig_directory,  MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR )
DEFAULT_FILL_TITLE_MACRO(action_get_title_playlist_directory,    MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_content_favorites_directory,  MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_content_history_directory,    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_content_image_history_directory,   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_content_music_history_directory,   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_content_video_history_directory,   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_runtime_log_directory, MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_browser_directory,     MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_use_last_start_directory,     MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_content_directory,     MENU_ENUM_LABEL_VALUE_CONTENT_DIR)
DEFAULT_FILL_TITLE_MACRO(action_get_title_screenshot_directory,  MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_cursor_directory,      MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_onscreen_overlay_keyboard_directory, MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_recording_config_directory, MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_recording_output_directory, MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_video_shader_directory, MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR)
DEFAULT_FILL_TITLE_MACRO(action_get_title_audio_filter_directory, MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR)
DEFAULT_FILL_TITLE_MACRO(action_get_title_video_filter_directory, MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR)
DEFAULT_FILL_TITLE_MACRO(action_get_title_savefile_directory,     MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_overlay_directory,      MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY)
#ifdef HAVE_VIDEO_LAYOUT
DEFAULT_FILL_TITLE_MACRO(action_get_title_video_layout_directory, MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY)
#endif
DEFAULT_FILL_TITLE_MACRO(action_get_title_system_directory,       MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_assets_directory,       MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_extraction_directory,   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY)
DEFAULT_FILL_TITLE_MACRO(action_get_title_menu,                   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS)
DEFAULT_FILL_TITLE_MACRO(action_get_title_video_font_path,        MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH)
DEFAULT_FILL_TITLE_MACRO(action_get_title_xmb_font,               MENU_ENUM_LABEL_VALUE_XMB_FONT)
DEFAULT_FILL_TITLE_MACRO(action_get_title_log_dir,                MENU_ENUM_LABEL_VALUE_LOG_DIR)
DEFAULT_FILL_TITLE_MACRO(action_get_title_manual_content_scan_dir, MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR)

DEFAULT_TITLE_COPY_MACRO(action_get_title_help,                   MENU_ENUM_LABEL_VALUE_HELP_LIST)
DEFAULT_TITLE_COPY_MACRO(action_get_title_input_settings,         MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS)
#ifdef HAVE_CHEEVOS
DEFAULT_TITLE_COPY_MACRO(action_get_title_cheevos_list,           MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST)
#endif
DEFAULT_TITLE_COPY_MACRO(action_get_title_video_shader_parameters,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS)
DEFAULT_TITLE_COPY_MACRO(action_get_title_video_shader_preset_parameters,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS)
DEFAULT_TITLE_COPY_MACRO(action_get_title_video_shader_preset_save,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE)
DEFAULT_TITLE_COPY_MACRO(action_get_title_video_shader_preset_remove,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE)
DEFAULT_TITLE_COPY_MACRO(action_get_title_video_shader_preset_save_list,MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE)

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
DEFAULT_TITLE_MACRO(action_get_title_switch_cpu_profile,          MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE)
#endif

#ifdef HAVE_LAKKA_SWITCH
DEFAULT_TITLE_MACRO(action_get_title_switch_gpu_profile,          MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE)
#endif

DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_title_deferred_history_list,   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY)
DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_title_deferred_favorites_list, MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES)
DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_title_deferred_images_list,    MENU_ENUM_LABEL_VALUE_GOTO_IMAGES)
DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_title_deferred_music_list,     MENU_ENUM_LABEL_VALUE_GOTO_MUSIC)
DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_title_deferred_video_list,     MENU_ENUM_LABEL_VALUE_GOTO_VIDEO)
DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_core_updater_list,             MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST)
DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_core_manager_list,             MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST)
DEFAULT_TITLE_SEARCH_FILTER_MACRO(action_get_core_cheat_options_list,       MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS)

DEFAULT_FILL_TITLE_SEARCH_FILTER_MACRO(action_get_title_video_shader_preset,    MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO)
DEFAULT_FILL_TITLE_SEARCH_FILTER_MACRO(action_get_title_cheat_file_load,        MENU_ENUM_LABEL_VALUE_CHEAT_FILE)
DEFAULT_FILL_TITLE_SEARCH_FILTER_MACRO(action_get_title_cheat_file_load_append, MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND)
DEFAULT_FILL_TITLE_SEARCH_FILTER_MACRO(action_get_title_overlay,                MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET)

static int action_get_title_generic(char *s, size_t len,
      const char *path, const char *text)
{
   if (!string_is_empty(path))
   {
      struct string_list list_path = {0};
      string_list_initialize(&list_path);
      if (string_split_noalloc(&list_path, path, "|"))
      {
         char elem0_path[255];
         elem0_path[0] = '\0';

         if (list_path.size > 0)
            strlcpy(elem0_path, list_path.elems[0].data,
                  sizeof(elem0_path));
         string_list_deinitialize(&list_path);

         if (!string_is_empty(elem0_path))
            snprintf(s, len, "%s- %s",
                  text,
                  path_basename(elem0_path));
         else
            strlcpy(s, text, len);
         return 0;
      }
   }

   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);

   return 0;
}

DEFAULT_TITLE_GENERIC_MACRO(action_get_title_deferred_database_manager_list,MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_deferred_cursor_manager_list,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_developer,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_publisher,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_origin,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_franchise,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_edge_magazine_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_edge_magazine_issue,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_releasedate_by_month,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_releasedate_by_year,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_esrb_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_elspa_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_pegi_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_cero_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_bbfc_rating,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_max_users,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS)
DEFAULT_TITLE_GENERIC_MACRO(action_get_title_list_rdb_entry_database_info,MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO)

static int action_get_sideload_core_list(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len,
         "%s %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST),
         string_is_empty(path) ? "" : path
         );
   return 0;
}

static int action_get_title_default(const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   if (!string_is_empty(path))
      snprintf(s, len, "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SELECT_FILE),
            path);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SELECT_FILE), len);

   menu_entries_search_append_terms_string(s, len);

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
      bool is_playlist_tab;
   } title_info_list_t;

   /* Note: MENU_ENUM_LABEL_HORIZONTAL_MENU *is* a playlist
    * tab, but its actual title is set elsewhere - so treat
    * it as a generic top-level item */
   title_info_list_t info_list[] = {
      {MENU_ENUM_LABEL_MAIN_MENU,         MENU_ENUM_LABEL_VALUE_MAIN_MENU,       false },
      {MENU_ENUM_LABEL_HISTORY_TAB,       MENU_ENUM_LABEL_VALUE_HISTORY_TAB,     true  },
      {MENU_ENUM_LABEL_FAVORITES_TAB,     MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,   true  },
      {MENU_ENUM_LABEL_IMAGES_TAB,        MENU_ENUM_LABEL_VALUE_IMAGES_TAB,      true  },
      {MENU_ENUM_LABEL_MUSIC_TAB,         MENU_ENUM_LABEL_VALUE_MUSIC_TAB,       true  },
      {MENU_ENUM_LABEL_VIDEO_TAB,         MENU_ENUM_LABEL_VALUE_VIDEO_TAB,       true  },
      {MENU_ENUM_LABEL_SETTINGS_TAB,      MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,    false },
      {MENU_ENUM_LABEL_PLAYLISTS_TAB,     MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,   false },
      {MENU_ENUM_LABEL_ADD_TAB,           MENU_ENUM_LABEL_VALUE_ADD_TAB,         false },
      {MENU_ENUM_LABEL_EXPLORE_TAB,       MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,     false },
      {MENU_ENUM_LABEL_NETPLAY_TAB,       MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,     false },
      {MENU_ENUM_LABEL_HORIZONTAL_MENU,   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU, false },
   };

   for (i = 0; i < ARRAY_SIZE(info_list); i++)
   {
      if (string_is_equal(label, msg_hash_to_str(info_list[i].type)))
      {
         if (info_list[i].is_playlist_tab)
            action_get_title_fill_search_filter_default(
                  info_list[i].val, s, len);
         else
            strlcpy(s, msg_hash_to_str(info_list[i].val), len);
         return 0;
      }
   }

   {
      char elem0[255];
      char elem1[255];
      struct string_list list_label = {0};
      
      elem0[0] = elem1[0] = '\0';

      string_list_initialize(&list_label);
      string_split_noalloc(&list_label, label, "|");

      if (list_label.size > 0)
      {
         strlcpy(elem0, list_label.elems[0].data, sizeof(elem0));
         if (list_label.size > 1)
            strlcpy(elem1, list_label.elems[1].data, sizeof(elem1));
      }
      string_list_deinitialize(&list_label);

      if (!string_is_empty(elem1))
         snprintf(s, len, "%s - %s", elem0, elem1);
      else
         strlcpy(s, elem0, len);
   }

   return 0;
}

static int action_get_title_input_binds_list(
      const char *path, const char *label,
      unsigned menu_type, char *s, size_t len)
{
   unsigned val = (((unsigned)path[0]) - 49) + 1;
   snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS), val);
   return 0;
}

static int menu_cbs_init_bind_title_compare_label(menu_file_list_cbs_t *cbs,
      const char *label)
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
      {MENU_ENUM_LABEL_DEFERRED_CORE_INFORMATION_LIST,                action_get_core_information_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_RESTORE_BACKUP_LIST,             action_get_title_deferred_core_restore_backup_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_DELETE_BACKUP_LIST,              action_get_title_deferred_core_delete_backup_list},
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
      {MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST, action_get_onscreen_notifications_views_settings_list},
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
      {MENU_ENUM_LABEL_DEFERRED_ACCESSIBILITY_SETTINGS_LIST,          action_get_accessibility_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_POWER_MANAGEMENT_SETTINGS_LIST,       action_get_power_management_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CPU_PERFPOWER_LIST,                   action_get_cpu_perfpower_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_CPU_POLICY_ENTRY,                     action_get_cpu_policy_entry_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_SOUNDS_LIST,                     action_get_menu_sounds_list},
      {MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST,      action_get_menu_file_browser_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST,     action_get_retro_achievements_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_BLUETOOTH_SETTINGS_LIST,              action_get_bluetooth_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_WIFI_NETWORKS_LIST,                   action_get_wifi_networks_list},
      {MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST,                   action_get_wifi_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST,                action_get_updater_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_NETWORK_HOSTING_SETTINGS_LIST,        action_get_network_hosting_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_SUBSYSTEM_SETTINGS_LIST,              action_get_subsystem_settings_list},
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
      {MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,                          action_get_title_deferred_history_list},
      {MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST,                       action_get_title_deferred_favorites_list},
      {MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST,                          action_get_title_deferred_images_list},
      {MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST,                           action_get_title_deferred_music_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST,                           action_get_title_deferred_video_list},
      {MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST,                 action_get_driver_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST,                  action_get_audio_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_RESAMPLER_SETTINGS_LIST,        action_get_audio_resampler_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_OUTPUT_SETTINGS_LIST,           action_get_audio_output_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_AUDIO_SYNCHRONIZATION_SETTINGS_LIST,  action_get_audio_synchronization_settings_list},
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
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_FACEBOOK_LIST,               action_get_user_accounts_facebook_list},
      {MENU_ENUM_LABEL_ONLINE_UPDATER,                                action_get_online_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST,              action_get_recording_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SCALING_SETTINGS_LIST,          action_get_video_scaling_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_HDR_SETTINGS_LIST,              action_get_video_hdr_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_OUTPUT_SETTINGS_LIST,           action_get_video_output_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SYNCHRONIZATION_SETTINGS_LIST,  action_get_video_synchronization_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_MENU_SETTINGS_LIST,             action_get_input_menu_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_TURBO_FIRE_SETTINGS_LIST,       action_get_input_turbo_fire_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST,  action_get_input_haptic_feedback_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_WINDOWED_MODE_SETTINGS_LIST,    action_get_video_windowed_mode_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST,  action_get_video_fullscreen_mode_settings_list},
      {MENU_ENUM_LABEL_SIDELOAD_CORE_LIST, action_get_sideload_core_list},
      {MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST, action_get_title_deferred_database_manager_list},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST, action_get_title_deferred_cursor_manager_list},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER, action_get_title_list_rdb_entry_developer},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER, action_get_title_list_rdb_entry_publisher},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN, 
         action_get_title_list_rdb_entry_origin},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE,
            action_get_title_list_rdb_entry_franchise},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING, action_get_title_list_rdb_entry_edge_magazine_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE, action_get_title_list_rdb_entry_edge_magazine_issue},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH,
         action_get_title_list_rdb_entry_releasedate_by_month},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR,
         action_get_title_list_rdb_entry_releasedate_by_year},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING,
            action_get_title_list_rdb_entry_esrb_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING,
         action_get_title_list_rdb_entry_pegi_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING,
         action_get_title_list_rdb_entry_cero_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING,
         action_get_title_list_rdb_entry_bbfc_rating},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS,
         action_get_title_list_rdb_entry_max_users},
      {MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR,
         action_get_title_autoconfig_directory},
      {MENU_ENUM_LABEL_CACHE_DIRECTORY,
         action_get_title_extraction_directory},
      {MENU_ENUM_LABEL_SYSTEM_DIRECTORY,
         action_get_title_system_directory},
      {MENU_ENUM_LABEL_ASSETS_DIRECTORY,
         action_get_title_assets_directory},
      {MENU_ENUM_LABEL_SAVEFILE_DIRECTORY,
         action_get_title_savefile_directory},
      {MENU_ENUM_LABEL_OVERLAY_DIRECTORY,
         action_get_title_overlay_directory},
#ifdef HAVE_VIDEO_LAYOUT
      {MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY,
         action_get_title_video_layout_directory},
#endif
      {MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY,
         action_get_title_browser_directory},
      {MENU_ENUM_LABEL_USE_LAST_START_DIRECTORY,
         action_get_title_use_last_start_directory},
      {MENU_ENUM_LABEL_PLAYLIST_DIRECTORY,
         action_get_title_playlist_directory},
      {MENU_ENUM_LABEL_CONTENT_FAVORITES_DIRECTORY,
         action_get_title_content_favorites_directory},
      {MENU_ENUM_LABEL_CONTENT_HISTORY_DIRECTORY,
         action_get_title_content_history_directory},
      {MENU_ENUM_LABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
         action_get_title_content_image_history_directory},
      {MENU_ENUM_LABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
         action_get_title_content_music_history_directory},
      {MENU_ENUM_LABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
         action_get_title_content_video_history_directory},
      {MENU_ENUM_LABEL_RUNTIME_LOG_DIRECTORY,
         action_get_title_runtime_log_directory},
      {MENU_ENUM_LABEL_CONTENT_DIRECTORY,
         action_get_title_content_directory},
      {MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY,
         action_get_title_screenshot_directory},
      {MENU_ENUM_LABEL_VIDEO_SHADER_DIR,
         action_get_title_video_shader_directory},
      {MENU_ENUM_LABEL_VIDEO_FILTER_DIR,
         action_get_title_video_filter_directory},
      {MENU_ENUM_LABEL_AUDIO_FILTER_DIR,
         action_get_title_audio_filter_directory},
      {MENU_ENUM_LABEL_CURSOR_DIRECTORY,
         action_get_title_cursor_directory},
      {MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY,
         action_get_title_recording_config_directory},
      {MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY,
         action_get_title_recording_output_directory},
      {MENU_ENUM_LABEL_OSK_OVERLAY_DIRECTORY,
         action_get_title_onscreen_overlay_keyboard_directory},
      {MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY,
         action_get_title_input_remapping_directory},
      {MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY,
         action_get_title_content_database_directory},
      {MENU_ENUM_LABEL_SAVESTATE_DIRECTORY,
         action_get_title_savestate_directory},
      {MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
         action_get_title_dynamic_wallpapers_directory},
      {MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY,
         action_get_title_core_assets_directory},
      {MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY,
         action_get_title_thumbnail_directory},
      {MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY,
         action_get_title_config_directory},
      {MENU_ENUM_LABEL_LOG_DIR,
         action_get_title_log_dir},
      {MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
         action_get_load_content_list},
      {MENU_ENUM_LABEL_NETPLAY,
         action_get_netplay_list},
      {MENU_ENUM_LABEL_INFORMATION_LIST,
         action_get_title_information_list},
      {MENU_ENUM_LABEL_INFORMATION,
         action_get_title_information},
      {MENU_ENUM_LABEL_SETTINGS,
         action_get_settings_list},
      {MENU_ENUM_LABEL_DATABASE_MANAGER_LIST,
         action_get_database_manager_list},
      {MENU_ENUM_LABEL_CURSOR_MANAGER_LIST,
         action_get_cursor_manager_list},
      {MENU_ENUM_LABEL_CORE_LIST,
         action_get_core_list},
      {MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL,
         action_get_load_content_special},
      {MENU_ENUM_LABEL_HELP_LIST,
         action_get_title_help},
      {MENU_ENUM_LABEL_HELP,
         action_get_title_help},
      {MENU_ENUM_LABEL_INPUT_OVERLAY,
         action_get_title_overlay},
      {MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST,
         action_get_core_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST,
         action_get_core_manager_list},
      {MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
         action_get_configurations_list},
      {MENU_ENUM_LABEL_ADD_CONTENT_LIST,
         action_get_add_content_list},
      {MENU_ENUM_LABEL_CORE_OPTIONS,
         action_get_title_core_options_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_OPTION_OVERRIDE_LIST,
         action_get_core_option_override_list},
      {MENU_ENUM_LABEL_CONTENT_SETTINGS,
         action_get_quick_menu_list},
      {MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS,
         action_get_input_remapping_options_list},
      {MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS,
         action_get_core_cheat_options_list},
      {MENU_ENUM_LABEL_SHADER_OPTIONS,
         action_get_shader_options_list},
      {MENU_ENUM_LABEL_DISK_OPTIONS,
         action_get_disk_options_list},
      {MENU_ENUM_LABEL_FRONTEND_COUNTERS,
         action_get_frontend_counters_list},
      {MENU_ENUM_LABEL_CORE_COUNTERS,
         action_get_core_counters_list},
      {MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST,
         action_get_online_thumbnails_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_PL_THUMBNAILS_UPDATER_LIST,
         action_get_online_pl_thumbnails_updater_list},
      {MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST,
         action_get_title_input_binds_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST,
         action_get_input_hotkey_binds_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST,
            action_get_video_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST,
         action_get_input_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST,
         action_get_playlist_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_LIST,
         action_get_playlist_manager_list},
#ifdef HAVE_CHEEVOS
      {MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_MENU,
         action_get_title_achievement_pause_menu},
      {MENU_ENUM_LABEL_ACHIEVEMENT_LIST,
         action_get_title_cheevos_list},
#endif
      {MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS,
         action_get_title_video_shader_parameters},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS,
         action_get_title_video_shader_preset_parameters},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE,
         action_get_title_video_shader_preset_save},
      {MENU_ENUM_LABEL_MANAGEMENT,
         action_get_title_action_generic},
      {MENU_ENUM_LABEL_DISK_IMAGE_APPEND,
         action_get_title_disk_image_append},
      {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET,
         action_get_title_video_shader_preset},
      {MENU_ENUM_LABEL_CHEAT_FILE_LOAD,
         action_get_title_cheat_file_load},
      {MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND,
         action_get_title_cheat_file_load_append},
      {MENU_ENUM_LABEL_REMAP_FILE_LOAD,
         action_get_title_remap_file_load},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST,
         action_get_user_accounts_cheevos_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST,
         action_get_download_core_content_list},
      {MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST,
         action_get_user_accounts_list},
      {MENU_ENUM_LABEL_VIDEO_FONT_PATH,
         action_get_title_video_font_path},
      {MENU_ENUM_LABEL_XMB_FONT,
         action_get_title_xmb_font},
      {MENU_ENUM_LABEL_VIDEO_FILTER,
         action_get_title_video_filter},
      {MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN,
         action_get_title_audio_filter},
      {MENU_ENUM_LABEL_CHEAT_DATABASE_PATH,
         action_get_title_cheat_directory},
      {MENU_ENUM_LABEL_LIBRETRO_DIR_PATH,
         action_get_title_core_directory},
      {MENU_ENUM_LABEL_LIBRETRO_INFO_PATH,
         action_get_title_core_info_directory},
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      {MENU_ENUM_LABEL_SWITCH_CPU_PROFILE,
         action_get_title_switch_cpu_profile},
#endif
#ifdef HAVE_LAKKA_SWITCH
      {MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
         action_get_title_switch_gpu_profile},
#endif
      {MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST,
         action_get_title_manual_content_scan_list},
      {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR,
         action_get_title_manual_content_scan_dir},
      {MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING,
         action_get_title_list_rdb_entry_elspa_rating},
      {MENU_ENUM_LABEL_DEFERRED_CORE_LIST,
         action_get_title_deferred_core_list},
      {MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET,
         action_get_title_deferred_core_list},
   };

   if (cbs->setting)
   {
      const char *parent_group   = cbs->setting->parent_group;

      if (string_is_equal(parent_group, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))
            && cbs->setting->type == ST_GROUP)
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
         case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING: /* TODO/FIXME - doesn't work  */
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
         case MENU_ENUM_LABEL_USE_LAST_START_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_use_last_start_directory);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_playlist_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_FAVORITES_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_favorites_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_HISTORY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_history_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_IMAGE_HISTORY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_image_history_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_MUSIC_HISTORY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_music_history_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_VIDEO_HISTORY_DIRECTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_content_video_history_directory);
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
         case MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_manager_list);
            break;
         case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_add_content_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_configurations_list);
            break;
         case MENU_ENUM_LABEL_CORE_OPTIONS:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_core_options_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_OPTION_OVERRIDE_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_option_override_list);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_history_list);
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
         case MENU_ENUM_LABEL_DEFERRED_CORE_INFORMATION_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_core_information_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_RESTORE_BACKUP_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_core_restore_backup_list);
            break;
         case MENU_ENUM_LABEL_DEFERRED_CORE_DELETE_BACKUP_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_core_delete_backup_list);
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
         case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_FACEBOOK_LIST:
            BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_facebook_list);
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
            BIND_ACTION_GET_TITLE(cbs, action_get_title_video_font_path);
            break;
         case MENU_ENUM_LABEL_XMB_FONT:
            BIND_ACTION_GET_TITLE(cbs, action_get_title_xmb_font);
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
      return -1;

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
      const char *path, const char *label, unsigned type, size_t idx)
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
      {MENU_ENUM_LABEL_DEFERRED_MIXER_STREAM_SETTINGS_LIST,                                 action_get_title_mixer_stream_actions},
#endif
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_SAVE_LIST,                              action_get_title_video_shader_preset_save_list},
      {MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_REMOVE_LIST,                            action_get_title_video_shader_preset_remove},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST,                                          action_get_title_dropdown_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL,                                  action_get_title_dropdown_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION,                               action_get_title_dropdown_resolution_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PARAMETER,                   action_get_title_dropdown_video_shader_parameter_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PRESET_PARAMETER,            action_get_title_dropdown_video_shader_preset_parameter_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES,                  action_get_title_dropdown_video_shader_num_pass_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE,                    action_get_title_dropdown_playlist_default_core_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE,              action_get_title_dropdown_playlist_label_display_mode_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE,                       action_get_title_dropdown_playlist_sort_mode_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE,            action_get_title_thumbnails},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE,             action_get_title_left_thumbnails},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME,          action_get_title_dropdown_manual_content_scan_system_name_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME,            action_get_title_dropdown_manual_content_scan_core_name_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX,                               action_get_title_dropdown_disk_index},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE,                        action_get_title_dropdown_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DEVICE_INDEX,                       action_get_title_dropdown_item},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION,                        action_get_title_dropdown_input_description},
      {MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD,                    action_get_title_dropdown_input_description_kbd},
      {MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS,                                          action_get_quick_menu_views_settings_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST,                                              action_get_title_deferred_playlist_list},
      {MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_SETTINGS,                                  action_get_title_deferred_playlist_list},
      {MENU_ENUM_LABEL_PLAYLISTS_TAB,                                                       action_get_title_collection},
      {MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST,                                   action_get_title_manual_content_scan_list},
   };

   if (!cbs)
      return -1;

   BIND_ACTION_GET_TITLE(cbs, action_get_title_default);

   if (cbs->enum_idx != MENU_ENUM_LABEL_PLAYLIST_ENTRY &&
       menu_cbs_init_bind_title_compare_label(cbs, label) == 0)
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
   if (string_starts_with(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL)))
   {
      BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_database_info);
      return 0;
   }

   return -1;
}
