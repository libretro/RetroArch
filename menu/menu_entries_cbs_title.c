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

#include <string/string_list.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include "menu.h"
#include "menu_entries_cbs.h"

#include <rhash.h>

#define MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST                              0x7c0b704fU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST                                0x45446638U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER            0xcbd89be5U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER            0x125e594dU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN               0x4ebaa767U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE            0x77f9eff2U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING 0x1c7f8a43U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE  0xaaeebde7U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH         0x2b36ce66U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR          0x9c7c6e91U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING          0x68eba20fU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING         0x8bf6ab18U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING          0x5fc77328U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING          0x24f6172cU
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING          0x0a8e67f0U
#define MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS            0xbfcba816U
#define MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL                                   0xc35416c0U
#define MENU_LABEL_DEFERRED_CORE_LIST                                          0xf157d289U
#define MENU_LABEL_CONFIGURATIONS                                              0x3e930a50U
#define MENU_LABEL_DISK_IMAGE_APPEND                                           0x5af7d709U
#define MENU_LABEL_PERFORMANCE_COUNTERS                                        0xd8ab5049U
#define MENU_LABEL_CORE_LIST                                                   0xa8c3bfc9U
#define MENU_LABEL_MANAGEMENT                                                  0xb8137ec2U
#define MENU_LABEL_OPTIONS                                                     0x71f05091U
#define MENU_LABEL_SETTINGS                                                    0x1304dc16U
#define MENU_LABEL_FRONTEND_COUNTERS                                           0xe5696877U
#define MENU_LABEL_CORE_COUNTERS                                               0x64cc83e0U
#define MENU_LABEL_HISTORY_LIST                                                0x60d82032U
#define MENU_LABEL_INFO_SCREEN                                                 0xd97853d0U
#define MENU_LABEL_SYSTEM_INFORMATION                                          0x206ebf0fU
#define MENU_LABEL_CORE_INFORMATION                                            0xb638e0d3U
#define MENU_LABEL_VIDEO_SHADER_PARAMETERS                                     0x9895c3e5U
#define MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS                              0xd18158d7U
#define MENU_LABEL_DISK_OPTIONS                                                0xc61ab5fbU
#define MENU_LABEL_CORE_OPTIONS                                                0xf65e60f9U
#define MENU_LABEL_SHADER_OPTIONS                                              0x1f7d2fc7U
#define MENU_LABEL_VIDEO_OPTIONS                                               0x6390c4e7U
#define MENU_LABEL_CORE_CHEAT_OPTIONS                                          0x9293171dU
#define MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS                                0x7836a8caU
#define MENU_LABEL_DATABASE_MANAGER_LIST                                       0x7f853d8fU
#define MENU_LABEL_CURSOR_MANAGER_LIST                                         0xa969e378U
#define MENU_LABEL_DEFERRED_CORE_UPDATER_LIST                                  0xbd4d493dU
#define MENU_LABEL_VIDEO_SHADER_PASS                                           0x4fa31028U
#define MENU_LABEL_VIDEO_SHADER_PRESET                                         0xc5d3bae4U
#define MENU_LABEL_CHEAT_FILE_LOAD                                             0x57336148U
#define MENU_LABEL_REMAP_FILE_LOAD                                             0x9c2799b8U
#define MENU_LABEL_CUSTOM_VIEWPORT_2                                           0x76c30170U
#define MENU_LABEL_HELP                                                        0x7c97d2eeU
#define MENU_LABEL_INPUT_OVERLAY                                               0x24e24796U
#define MENU_LABEL_VIDEO_FONT_PATH                                             0xd0de729eU
#define MENU_LABEL_VIDEO_FILTER                                                0x1c0eb741U
#define MENU_LABEL_AUDIO_DSP_PLUGIN                                            0x4a69572bU
#define MENU_LABEL_RGUI_BROWSER_DIRECTORY                                      0xa86cba73U
#define MENU_LABEL_PLAYLIST_DIRECTORY                                          0x6361820bU
#define MENU_LABEL_CONTENT_DIRECTORY                                           0x7738dc14U
#define MENU_LABEL_SCREENSHOT_DIRECTORY                                        0x552612d7U
#define MENU_LABEL_VIDEO_SHADER_DIR                                            0x30f53b10U
#define MENU_LABEL_VIDEO_FILTER_DIR                                            0x67603f1fU
#define MENU_LABEL_AUDIO_FILTER_DIR                                            0x4bd96ebaU
#define MENU_LABEL_SAVESTATE_DIRECTORY                                         0x90551289U
#define MENU_LABEL_LIBRETRO_DIR_PATH                                           0x1af1eb72U
#define MENU_LABEL_LIBRETRO_INFO_PATH                                          0xe552b25fU
#define MENU_LABEL_RGUI_CONFIG_DIRECTORY                                       0x0cb3e005U
#define MENU_LABEL_SAVEFILE_DIRECTORY                                          0x92773488U
#define MENU_LABEL_OVERLAY_DIRECTORY                                           0xc4ed3d1bU
#define MENU_LABEL_SYSTEM_DIRECTORY                                            0x35a6fb9eU
#define MENU_LABEL_ASSETS_DIRECTORY                                            0xde1ae8ecU
#define MENU_LABEL_EXTRACTION_DIRECTORY                                        0x33b55ffaU
#define MENU_LABEL_JOYPAD_AUTOCONFIG_DIR                                       0x2f4822d8U
#define MENU_LABEL_DRIVER_SETTINGS                                             0x81cd2d62U
#define MENU_LABEL_CORE_SETTINGS                                               0x06795dffU
#define MENU_LABEL_CONFIGURATION_SETTINGS                                      0x5a1558ceU
#define MENU_LABEL_LOGGING_SETTINGS                                            0x902c003dU
#define MENU_LABEL_SAVING_SETTINGS                                             0x32fea87eU
#define MENU_LABEL_REWIND_SETTINGS                                             0xbff7775fU
#define MENU_LABEL_VIDEO_SETTINGS                                              0x9dd23badU
#define MENU_LABEL_RECORDING_SETTINGS                                          0x1a80b313U
#define MENU_LABEL_FRAME_THROTTLE_SETTINGS                                     0x573b8837U
#define MENU_LABEL_SHADER_SETTINGS                                             0xd6657e8dU
#define MENU_LABEL_FONT_SETTINGS                                               0x1bc2266dU
#define MENU_LABEL_AUDIO_SETTINGS                                              0x8f74c888U
#define MENU_LABEL_INPUT_SETTINGS                                              0xddd30846U
#define MENU_LABEL_OVERLAY_SETTINGS                                            0x34377f98U
#define MENU_LABEL_ONSCREEN_KEYBOARD_OVERLAY_SETTINGS                          0xa6de9ba6U
#define MENU_LABEL_MENU_SETTINGS                                               0x61e4544bU
#define MENU_LABEL_UI_SETTINGS                                                 0xf8da6ef4U
#define MENU_LABEL_PATCH_SETTINGS                                              0xa78b0986U
#define MENU_LABEL_PLAYLIST_SETTINGS                                           0x4d276288U
#define MENU_LABEL_CORE_UPDATER_SETTINGS                                       0x124ad454U
#define MENU_LABEL_NETWORK_SETTINGS                                            0x8b50d180U
#define MENU_LABEL_ARCHIVE_SETTINGS                                            0x78e85398U
#define MENU_LABEL_USER_SETTINGS                                               0xcdc9a8f5U
#define MENU_LABEL_DIRECTORY_SETTINGS                                          0xb817bd2bU
#define MENU_LABEL_PRIVACY_SETTINGS                                            0xce106254U

static INLINE void replace_chars(char *str, char c1, char c2)
{
   char *pos;
   while((pos = strchr(str, c1)))
      *pos = c2;
}

static INLINE void sanitize_to_string(char *s, const char *label, size_t len)
{
   char new_label[PATH_MAX_LENGTH];
   strlcpy(new_label, label, sizeof(new_label));
   strlcpy(s, string_to_upper(new_label), len);
   replace_chars(s, '_', ' ');
}

static int action_get_title_default(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   uint32_t hash = 0;
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   char elem0_path[PATH_MAX_LENGTH], elem1_path[PATH_MAX_LENGTH];
   struct string_list *list_label = string_split(label, "|");
   struct string_list *list_path  = string_split(path, "|");

   *elem0 = *elem1 = *elem0_path = *elem1_path = 0;

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

   if (list_path)
   {
      if (list_path->size > 0)
      {
         strlcpy(elem0_path, list_path->elems[0].data, sizeof(elem0_path));
         if (list_path->size > 1)
            strlcpy(elem1_path, list_path->elems[1].data, sizeof(elem1_path));
      }
      string_list_free(list_path);
   }

#if 0
   RARCH_LOG("label %s, elem0 %s, elem1 %s\n", label, elem0, elem1);
#endif

   hash = djb2_calculate(label);

   switch (hash)
   {
   case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
      if (!strcmp(label, "deferred_database_manager_list"))
         snprintf(s, len, "DATABASE SELECTION - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
      if (!strcmp(label, "deferred_cursor_manager_list"))
         snprintf(s, len, "DATABASE CURSOR LIST - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: DEVELOPER - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: PUBLISHER - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: ORIGIN - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: FRANCHISE - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE RATING - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE ISSUE - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY MONTH - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY YEAR - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: ESRB RATING - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: ELSPA RATING - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: PEGI RATING - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: CERO RATING - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: BBFC RATING - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
      if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users"))
         snprintf(s, len, "DATABASE CURSOR LIST (FILTER: MAX USERS - %s)", elem0_path);
      break;
   case MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
      if (!strcmp(label, "deferred_rdb_entry_detail"))
         snprintf(s, len, "DATABASE INFO: %s", elem1);
      break;
   case MENU_LABEL_DEFERRED_CORE_LIST:
      if (!strcmp(label, "deferred_core_list"))
         snprintf(s, len, "DETECTED CORES %s", path);
      break;
   case MENU_LABEL_CONFIGURATIONS:
      if (!strcmp(label, "configurations"))
         snprintf(s, len, "CONFIG %s", path);
      break;
   case MENU_LABEL_DISK_IMAGE_APPEND:
      if (!strcmp(label, "disk_image_append"))
         snprintf(s, len, "DISK APPEND %s", path);
      break;
   case MENU_LABEL_PERFORMANCE_COUNTERS:
      if (!strcmp(label, "performance_counters"))
      {
      is_performance_counters:
         sanitize_to_string(s, label, len);
      }
      break;
   case MENU_LABEL_CORE_LIST:
      if (!strcmp(label, "core_list"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_MANAGEMENT:
      if (!strcmp(label, "management"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_OPTIONS:
      if (!strcmp(label, "options"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_SETTINGS:
      if (!strcmp(label, "settings"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_FRONTEND_COUNTERS:
      if (!strcmp(label, "frontend_counters"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_CORE_COUNTERS:
      if (!strcmp(label, "core_counters"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_HISTORY_LIST:
      if (!strcmp(label, "history_list"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_INFO_SCREEN:
      if (!strcmp(label, "info_screen"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_SYSTEM_INFORMATION:
      if (!strcmp(label, "system_information"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_CORE_INFORMATION:
      if (!strcmp(label, "core_information"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      if (!strcmp(label, "video_shader_parameters"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
      if (!strcmp(label, "video_shader_preset_parameters"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_DISK_OPTIONS:
      if (!strcmp(label, "disk_options"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_CORE_OPTIONS:
      if (!strcmp(label, "core_options"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_SHADER_OPTIONS:
      if (!strcmp(label, "shader_options"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_VIDEO_OPTIONS:
      if (!strcmp(label, "video_options"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_CORE_CHEAT_OPTIONS:
      if (!strcmp(label, "core_cheat_options"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
      if (!strcmp(label, "core_input_remapping_options"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_DATABASE_MANAGER_LIST:
      if (!strcmp(label, "database_manager_list"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_CURSOR_MANAGER_LIST:
      if (!strcmp(label, "cursor_manager_list"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
      if (!strcmp(label, "deferred_core_updater_list"))
         goto is_performance_counters;
      break;
   case MENU_LABEL_VIDEO_SHADER_PASS:
      if (!strcmp(label, "video_shader_pass"))
         snprintf(s, len, "SHADER %s", path);
      break;
   case MENU_LABEL_VIDEO_SHADER_PRESET:
      if (!strcmp(label, "video_shader_preset"))
         snprintf(s, len, "SHADER PRESET %s", path);
      break;
   case MENU_LABEL_CHEAT_FILE_LOAD:
      if (!strcmp(label, "cheat_file_load"))
         snprintf(s, len, "CHEAT FILE %s", path);
      break;
   case MENU_LABEL_REMAP_FILE_LOAD:
      if (!strcmp(label, "remap_file_load"))
         snprintf(s, len, "REMAP FILE %s", path);
      break;
   case MENU_LABEL_CUSTOM_VIEWPORT_2:
      if (!strcmp(label, "custom_viewport_2"))
      {
      is_custom_viewport_2:
         snprintf(s, len, "MENU %s", path);
      }
      break;
   case MENU_LABEL_HELP:
      if (!strcmp(label, "help"))
         goto is_custom_viewport_2;
      break;
   case MENU_LABEL_INPUT_OVERLAY:
      if (!strcmp(label, "input_overlay"))
         snprintf(s, len, "OVERLAY %s", path);
      break;
   case MENU_LABEL_VIDEO_FONT_PATH:
      if (!strcmp(label, "video_font_path"))
         snprintf(s, len, "FONT %s", path);
      break;
   case MENU_LABEL_VIDEO_FILTER:
      if (!strcmp(label, "video_filter"))
         snprintf(s, len, "FILTER %s", path);
      break;
   case MENU_LABEL_AUDIO_DSP_PLUGIN:
      if (!strcmp(label, "audio_dsp_plugin"))
         snprintf(s, len, "DSP FILTER %s", path);
      break;
   case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
      if (!strcmp(label, "rgui_browser_directory"))
         snprintf(s, len, "BROWSER DIR %s", path);
      break;
   case MENU_LABEL_PLAYLIST_DIRECTORY:
      if (!strcmp(label, "playlist_directory"))
         snprintf(s, len, "PLAYLIST DIR %s", path);
      break;
   case MENU_LABEL_CONTENT_DIRECTORY:
      if (!strcmp(label, "content_directory"))
         snprintf(s, len, "CONTENT DIR %s", path);
      break;
   case MENU_LABEL_SCREENSHOT_DIRECTORY:
      if (!strcmp(label, "screenshot_directory"))
         snprintf(s, len, "SCREENSHOT DIR %s", path);
      break;
   case MENU_LABEL_VIDEO_SHADER_DIR:
      if (!strcmp(label, "video_shader_dir"))
         snprintf(s, len, "SHADER DIR %s", path);
      break;
   case MENU_LABEL_VIDEO_FILTER_DIR:
      if (!strcmp(label, "video_filter_dir"))
         snprintf(s, len, "FILTER DIR %s", path);
      break;
   case MENU_LABEL_AUDIO_FILTER_DIR:
      if (!strcmp(label, "audio_filter_dir"))
         snprintf(s, len, "DSP FILTER DIR %s", path);
      break;
   case MENU_LABEL_SAVESTATE_DIRECTORY:
      if (!strcmp(label, "savestate_directory"))
         snprintf(s, len, "SAVESTATE DIR %s", path);
      break;
   case MENU_LABEL_LIBRETRO_DIR_PATH:
      if (!strcmp(label, "libretro_dir_path"))
         snprintf(s, len, "LIBRETRO DIR %s", path);
      break;
   case MENU_LABEL_LIBRETRO_INFO_PATH:
      if (!strcmp(label, "libretro_info_path"))
         snprintf(s, len, "LIBRETRO INFO DIR %s", path);
      break;
   case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
      if (!strcmp(label, "rgui_config_directory"))
         snprintf(s, len, "CONFIG DIR %s", path);
      break;
   case MENU_LABEL_SAVEFILE_DIRECTORY:
      if (!strcmp(label, "savefile_directory"))
         snprintf(s, len, "SAVEFILE DIR %s", path);
      break;
   case MENU_LABEL_OVERLAY_DIRECTORY:
      if (!strcmp(label, "overlay_directory"))
         snprintf(s, len, "OVERLAY DIR %s", path);
      break;
   case MENU_LABEL_SYSTEM_DIRECTORY:
      if (!strcmp(label, "system_directory"))
         snprintf(s, len, "SYSTEM DIR %s", path);
      break;
   case MENU_LABEL_ASSETS_DIRECTORY:
      if (!strcmp(label, "assets_directory"))
         snprintf(s, len, "ASSETS DIR %s", path);
      break;
   case MENU_LABEL_EXTRACTION_DIRECTORY:
      if (!strcmp(label, "extraction_directory"))
         snprintf(s, len, "EXTRACTION DIR %s", path);
      break;
   case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
      if (!strcmp(label, "joypad_autoconfig_dir"))
         snprintf(s, len, "AUTOCONFIG DIR %s", path);
      break;
   case MENU_LABEL_DRIVER_SETTINGS:
      if (!strcmp(label, "Driver Settings"))
      {
      is_settings_entry:
         strlcpy(s, string_to_upper(elem0), len);
         if (elem1[0] != '\0')
         {
            strlcat(s, " - ", len);
            strlcat(s, string_to_upper(elem1), len);
         }
      }
      break;
   case MENU_LABEL_CORE_SETTINGS:
      if (!strcmp(label, "Core Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_CONFIGURATION_SETTINGS:
      if (!strcmp(label, "Configuration Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_LOGGING_SETTINGS:
      if (!strcmp(label, "Logging Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_SAVING_SETTINGS:
      if (!strcmp(label, "Saving Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_REWIND_SETTINGS:
      if (!strcmp(label, "Rewind Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_VIDEO_SETTINGS:
      if (!strcmp(label, "Video Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_RECORDING_SETTINGS:
      if (!strcmp(label, "Recording Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_FRAME_THROTTLE_SETTINGS:
      if (!strcmp(label, "Frame Throttle Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_SHADER_SETTINGS:
      if (!strcmp(label, "Shader Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_FONT_SETTINGS:
      if (!strcmp(label, "Font Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_AUDIO_SETTINGS:
      if (!strcmp(label, "Audio Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_INPUT_SETTINGS:
      if (!strcmp(label, "Input Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_OVERLAY_SETTINGS:
      if (!strcmp(label, "Overlay Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_ONSCREEN_KEYBOARD_OVERLAY_SETTINGS:
      if (!strcmp(label, "Onscreen Keyboard Overlay Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_MENU_SETTINGS:
      if (!strcmp(label, "Menu Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_UI_SETTINGS:
      if (!strcmp(label, "UI Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_PATCH_SETTINGS:
      if (!strcmp(label, "Patch Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_PLAYLIST_SETTINGS:
      if (!strcmp(label, "Playlist Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_CORE_UPDATER_SETTINGS:
      if (!strcmp(label, "Core Updater Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_NETWORK_SETTINGS:
      if (!strcmp(label, "Network Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_ARCHIVE_SETTINGS:
      if (!strcmp(label, "Archive Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_USER_SETTINGS:
      if (!strcmp(label, "User Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_DIRECTORY_SETTINGS:
      if (!strcmp(label, "Directory Settings"))
         goto is_settings_entry;
      break;
   case MENU_LABEL_PRIVACY_SETTINGS:
      if (!strcmp(label, "Privacy Settings"))
         goto is_settings_entry;
      break;
   default:
      if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT ||
          menu_type == MENU_SETTINGS)
      {
         goto is_custom_viewport_2;
      }
      else if (menu_type == MENU_SETTINGS_CUSTOM_BIND ||
               menu_type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
      {
         strlcpy(s, "INPUT SETTINGS", len);
         if (elem1[0] != '\0')
         {
            strlcat(s, " - ", len);
            strlcat(s, string_to_upper(elem1), len);
         }
      }
      else
      {
         driver_t *driver = driver_get_ptr();

         if (driver->menu->defer_core)
             snprintf(s, len, "CONTENT %s", path);
         else
         {
            global_t *global      = global_get_ptr();
            const char *core_name = global->menu.info.library_name;

            if (!core_name)
               core_name = global->system.info.library_name;
            if (!core_name)
               core_name = "No Core";
            snprintf(s, len, "CONTENT (%s) %s", core_name, path);
         }
      }
      break;
   }
   
   return 0;
}

void menu_entries_cbs_init_bind_title(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_get_title = action_get_title_default;
}
