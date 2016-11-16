/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_navigation.h"
#include "../../file_path_special.h"

#ifdef HAVE_CHEEVOS
#include "../../cheevos.h"
#endif

#ifndef BIND_ACTION_SUBLABEL
#define BIND_ACTION_SUBLABEL(cbs, name) \
   cbs->action_sublabel = name; \
   cbs->action_sublabel_ident = #name;
#endif

static int action_bind_sublabel_generic(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   return 0;
}

static int action_bind_sublabel_core_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_CORE_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_cheevos_hardcore_mode_enable(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE), len);
   return 0;
}

static int action_bind_sublabel_menu_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_MENU_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_video_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_suspend_screensaver_enable(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE), len);
   return 0;
}

static int action_bind_sublabel_audio_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_AUDIO_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_input_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_wifi_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_WIFI_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_services_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_SERVICES_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_ssh_enable(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_SSH_ENABLE), len);
   return 0;
}

static int action_bind_sublabel_samba_enable(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_SAMBA_ENABLE), len);
   return 0;
}

static int action_bind_sublabel_bluetooth_enable(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE), len);
   return 0;
}

static int action_bind_sublabel_user_language(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_USER_LANGUAGE), len);
   return 0;
}

static int action_bind_sublabel_max_swapchain_images(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES), len);
   return 0;
}

static int action_bind_sublabel_online_updater(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_ONLINE_UPDATER), len);
   return 0;
}

static int action_bind_sublabel_fps_show(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_FPS_SHOW), len);
   return 0;
}

static int action_bind_sublabel_netplay_settings(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{

   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_NETPLAY), len);
   return 0;
}

static int action_bind_sublabel_user_bind_settings(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{

   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_USER_BINDS), len);
   return 0;
}

static int action_bind_sublabel_input_hotkey_settings(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{

   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS), len);
   return 0;
}

static int action_bind_sublabel_add_content_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{

   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST), len);
   return 0;
}

static int action_bind_sublabel_video_frame_delay(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY), len);
   return 0;
}

static int action_bind_sublabel_video_black_frame_insertion(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION), len);
   return 0;
}

static int action_bind_sublabel_systeminfo_cpu_cores(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_CPU_CORES), len);
   return 0;
}

static int action_bind_sublabel_toggle_gamepad_combo(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO), len);
   return 0;
}

static int action_bind_sublabel_show_hidden_files(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES), len);
   return 0;
}

static int action_bind_sublabel_log_verbosity(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_LOG_VERBOSITY), len);
   return 0;
}

static int action_bind_sublabel_video_monitor_index(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX), len);
   return 0;
}

static int action_bind_sublabel_video_refresh_rate_auto(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO), len);
   return 0;
}

static int action_bind_sublabel_video_hard_sync(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC), len);
   return 0;
}

static int action_bind_sublabel_video_hard_sync_frames(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES), len);
   return 0;
}

static int action_bind_sublabel_video_threaded(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_THREADED), len);
   return 0;
}

static int action_bind_sublabel_cheevos_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
#ifdef HAVE_CHEEVOS
   cheevos_ctx_desc_t desc_info;
   unsigned new_id = type - MENU_SETTINGS_CHEEVOS_START;
   desc_info.idx   = new_id;
   desc_info.s     = s;
   desc_info.len   = len;
   cheevos_get_description(&desc_info);

   strlcpy(s, desc_info.s, len);
#endif
   return 0;
}

static int action_bind_sublabel_config_save_on_exit(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{

   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT), len);
   return 0;
}

int menu_cbs_init_bind_sublabel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_generic);

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY:
         case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_entry);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_hardcore_mode_enable);
            break;
         case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_config_save_on_exit);
            break;
         case MENU_ENUM_LABEL_VIDEO_THREADED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_threaded);
            break;
         case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_hard_sync);
            break;
         case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_hard_sync_frames);
            break;
         case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_refresh_rate_auto);
            break;
         case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_monitor_index);
            break;
         case MENU_ENUM_LABEL_LOG_VERBOSITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_log_verbosity);
            break;
         case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_show_hidden_files);
            break;
         case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_toggle_gamepad_combo);
            break;
         case MENU_ENUM_LABEL_CPU_CORES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_systeminfo_cpu_cores);
            break;
         case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_black_frame_insertion);
            break;
         case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_frame_delay);
            break;
         case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_add_content_list);
            break;
         case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_hotkey_settings);
            break;
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
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_user_bind_settings);
            break;

         case MENU_ENUM_LABEL_NETPLAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_settings);
            break;
         case MENU_ENUM_LABEL_ONLINE_UPDATER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_online_updater);
            break;
         case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_max_swapchain_images);
            break;
         case MENU_ENUM_LABEL_FPS_SHOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_fps_show);
            break;
         case MENU_ENUM_LABEL_MENU_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_settings_list);
            break;
         case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_settings_list);
            break;
         case MENU_ENUM_LABEL_CORE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_settings_list);
            break;
         case MENU_ENUM_LABEL_INPUT_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_settings_list);
            break;
         case MENU_ENUM_LABEL_WIFI_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_wifi_settings_list);
            break;
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
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bluetooth_enable);
            break;
         case MENU_ENUM_LABEL_USER_LANGUAGE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_user_language);
            break;
         case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_suspend_screensaver_enable);
            break;
         default:
         case MSG_UNKNOWN:
            return -1;
      }
   }

   return 0;
}
