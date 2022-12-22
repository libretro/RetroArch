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
#include <lists/string_list.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../menu_shader.h"
#endif

#include "../../configuration.h"
#include "../../file_path_special.h"
#include "../../core.h"
#include "../../core_info.h"
#include "../../core_option_manager.h"
#ifdef HAVE_CHEATS
#include "../../cheat_manager.h"
#endif
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../performance_counters.h"
#include "../../playlist.h"
#include "../../manual_content_scan.h"

#include "../../audio/audio_driver.h"
#include "../../input/input_remapping.h"

#include "../../config.def.h"

#ifdef HAVE_BLUETOOTH
#include "../../bluetooth/bluetooth_driver.h"
#endif

#ifdef HAVE_NETWORKING
#include "../../core_updater_list.h"
#endif

#ifndef BIND_ACTION_START
#define BIND_ACTION_START(cbs, name) (cbs)->action_start = (name)
#endif

/* Forward declarations */
int generic_action_ok_command(enum event_command cmd);
int action_ok_push_playlist_manager_settings(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx);
int action_ok_push_core_information_list(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx);

#ifdef HAVE_AUDIOMIXER
static int action_start_audio_mixer_stream_volume(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   unsigned         offset      = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN);

   if (offset >= AUDIO_MIXER_MAX_STREAMS)
      return 0;

   audio_driver_mixer_set_stream_volume(offset, 1.0f);

   return 0;
}
#endif

static int action_start_remap_file_info(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings                  = config_get_ptr();
   const char *directory_input_remapping = settings ?
         settings->paths.directory_input_remapping : NULL;
   rarch_system_info_t *system           = &runloop_state_get_ptr()->system;
   bool refresh                          = false;

   input_remapping_deinit(false);
   input_remapping_set_defaults(false);
   config_load_remap(directory_input_remapping, system);

   /* Refresh menu */
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   return 0;
}

static int action_start_shader_preset(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   bool refresh                = false;
   struct video_shader *shader = menu_shader_get();

   shader->passes = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
#endif
   return 0;
}

static int action_start_shader_preset_prepend(
   const char* path, const char* label,
   unsigned type, size_t idx, size_t entry_idx)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   bool refresh = false;
   struct video_shader* shader = menu_shader_get();

   shader->passes = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
#endif
   return 0;
}

static int action_start_shader_preset_append(
   const char* path, const char* label,
   unsigned type, size_t idx, size_t entry_idx)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   bool refresh = false;
   struct video_shader* shader = menu_shader_get();

   shader->passes = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
#endif
   return 0;
}

static int action_start_video_filter_file_load(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return -1;

   if (!string_is_empty(settings->paths.path_softfilter_plugin))
   {
      bool refresh = false;

      /* Unload video filter */
      settings->paths.path_softfilter_plugin[0] = '\0';
      command_event(CMD_EVENT_REINIT, NULL);

      /* Refresh menu */
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   }

   return 0;
}

static int action_start_audio_dsp_plugin_file_load(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return -1;

   if (!string_is_empty(settings->paths.path_audio_dsp_plugin))
   {
      bool refresh = false;

      /* Unload dsp plugin filter */
      settings->paths.path_audio_dsp_plugin[0] = '\0';
      command_event(CMD_EVENT_DSP_FILTER_INIT, NULL);

      /* Refresh menu */
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   }

   return 0;
}

static int generic_action_start_performance_counters(struct retro_perf_counter **counters,
      unsigned offset, unsigned type, const char *label)
{
   if (counters[offset])
   {
      counters[offset]->total    = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_performance_counters_core(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   struct retro_perf_counter **counters = retro_get_perf_counter_libretro();
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   return generic_action_start_performance_counters(counters, offset, type, label);
}

static int action_start_performance_counters_frontend(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   struct retro_perf_counter **counters = retro_get_perf_counter_rarch();
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;
   return generic_action_start_performance_counters(counters, offset, type, label);
}

static int action_start_input_desc(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   unsigned user_idx;
   unsigned btn_idx;
   unsigned mapped_port;
   settings_t *settings        = config_get_ptr();
   rarch_system_info_t *system = &runloop_state_get_ptr()->system;

   if (!settings || !system)
      return 0;

   user_idx    = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) / (RARCH_FIRST_CUSTOM_BIND + 8);
   btn_idx     = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) - (RARCH_FIRST_CUSTOM_BIND + 8) * user_idx;
   mapped_port = settings->uints.input_remap_ports[user_idx];

   if ((user_idx >= MAX_USERS) ||
       (mapped_port >= MAX_USERS) ||
       (btn_idx >= RARCH_CUSTOM_BIND_LIST_END))
      return 0;

   /* Check whether core has defined this input */
   if (!string_is_empty(system->input_desc_btn[mapped_port][btn_idx]))
   {
      const struct retro_keybind *keyptr = &input_config_binds[user_idx][btn_idx];
      settings->uints.input_remap_ids[user_idx][btn_idx] = keyptr->id;
   }
   else
      settings->uints.input_remap_ids[user_idx][btn_idx] = RARCH_UNMAPPED;

   return 0;
}

static int action_start_input_desc_kbd(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   unsigned user_idx;
   unsigned btn_idx;

   (void)label;

   if (!settings)
      return 0;

   user_idx = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) / RARCH_ANALOG_BIND_LIST_END;
   btn_idx  = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) - RARCH_ANALOG_BIND_LIST_END * user_idx;

   if ((user_idx >= MAX_USERS) || (btn_idx >= RARCH_CUSTOM_BIND_LIST_END))
      return 0;

   /* By default, inputs are unmapped */
   settings->uints.input_keymapper_ids[user_idx][btn_idx] = RETROK_FIRST;

   return 0;
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static int action_start_shader_action_parameter_generic(
      unsigned type, unsigned offset)
{
   video_shader_ctx_t shader_info;
   struct video_shader_parameter *param = NULL;
   unsigned parameter                   = type - offset;

   video_shader_driver_get_current_shader(&shader_info);

   if (!shader_info.data)
      return 0;

   param          = &shader_info.data->parameters
      [parameter];
   param->current = param->initial;
   param->current = MIN(MAX(param->minimum, param->current), param->maximum);

   return menu_shader_manager_clear_parameter(menu_shader_get(), parameter);
}

static int action_start_shader_action_parameter(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   return action_start_shader_action_parameter_generic(type, MENU_SETTINGS_SHADER_PARAMETER_0);
}

static int action_start_shader_action_preset_parameter(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   return action_start_shader_action_parameter_generic(type, MENU_SETTINGS_SHADER_PRESET_PARAMETER_0);
}

static int action_start_shader_pass(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu       = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   menu->scratchpad.unsigned_var = type - MENU_SETTINGS_SHADER_PASS_0;

   menu_shader_manager_clear_pass_path(menu_shader_get(),
         menu->scratchpad.unsigned_var);

   return 0;
}

static int action_start_shader_scale_pass(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;

   menu_shader_manager_clear_pass_scale(menu_shader_get(), pass);

   return 0;
}

static int action_start_shader_filter_pass(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   return menu_shader_manager_clear_pass_filter(menu_shader_get(), pass);
}
#endif

static int action_start_netplay_mitm_server(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   configuration_set_string(settings,
         settings->arrays.netplay_mitm_server,
         DEFAULT_NETPLAY_MITM_SERVER);
   return 0;
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static int action_start_shader_watch_for_changes(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   settings->bools.video_shader_watch_files = DEFAULT_VIDEO_SHADER_WATCH_FILES;
   return 0;
}

static int action_start_shader_num_passes(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   return menu_shader_manager_clear_num_passes(menu_shader_get());
}
#endif

#ifdef HAVE_CHEATS
static int action_start_cheat_num_passes(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   if (cheat_manager_get_size())
   {
      bool refresh                = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      cheat_manager_realloc(0, CHEAT_HANDLER_TYPE_EMU);
   }

   return 0;
}
#endif

static int action_start_core_setting(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   unsigned core_idx               = type - MENU_SETTINGS_CORE_OPTION_START;
   core_option_manager_t *coreopts = NULL;

   if (retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
      core_option_manager_set_default(coreopts, core_idx, true);

   return 0;
}

static int action_start_playlist_association(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   /* Set default core path + name to DETECT */
   playlist_set_default_core_path(playlist, FILE_PATH_DETECT);
   playlist_set_default_core_name(playlist, FILE_PATH_DETECT);
   playlist_write_file(playlist);

   return 0;
}

static int action_start_playlist_label_display_mode(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   /* Set label display mode to the default */
   playlist_set_label_display_mode(playlist, LABEL_DISPLAY_MODE_DEFAULT);
   playlist_write_file(playlist);

   return 0;
}

static int action_start_playlist_right_thumbnail_mode(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   /* Set thumbnail_mode to default value */
   playlist_set_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_RIGHT, PLAYLIST_THUMBNAIL_MODE_DEFAULT);
   playlist_write_file(playlist);

   return 0;
}

static int action_start_playlist_left_thumbnail_mode(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   /* Set thumbnail_mode to default value */
   playlist_set_thumbnail_mode(playlist, PLAYLIST_THUMBNAIL_LEFT, PLAYLIST_THUMBNAIL_MODE_DEFAULT);
   playlist_write_file(playlist);

   return 0;
}

static int action_start_state_slot(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings      = config_get_ptr();

   settings->ints.state_slot = 0;

   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH, NULL);
   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE, NULL);

   return 0;
}

static int action_start_menu_wallpaper(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings       = config_get_ptr();
   struct menu_state *menu_st = menu_state_get_ptr();

   settings->paths.path_menu_wallpaper[0] = '\0';

   /* Reset wallpaper by menu context reset */
   if (menu_st->driver_ctx && menu_st->driver_ctx->context_reset)
      menu_st->driver_ctx->context_reset(menu_st->userdata,
            video_driver_is_threaded());

   return 0;
}

static int action_start_playlist_sort_mode(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   /* Set sort mode to the default */
   playlist_set_sort_mode(playlist, PLAYLIST_SORT_MODE_DEFAULT);
   playlist_write_file(playlist);

   return 0;
}

static int action_start_manual_content_scan_dir(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   /* Reset content directory */
   manual_content_scan_set_menu_content_dir("");
   return 0;
}

static int action_start_manual_content_scan_system_name(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   /* Reset system name */
   manual_content_scan_set_menu_system_name(
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR, "");
   return 0;
}

static int action_start_manual_content_scan_core_name(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   /* Reset core name */
   manual_content_scan_set_menu_core_name(
         MANUAL_CONTENT_SCAN_CORE_DETECT, "");
   return 0;
}

static int action_start_video_resolution(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
#if defined(GEKKO) || defined(PS2) || !defined(__PSL1GHT__) && !defined(__PS3__)
   unsigned width = 0, height = 0;
   char desc[64] = {0};
   global_t *global = global_get_ptr();

   /*  Reset the resolution id to zero */
   global->console.screen.resolutions.current.id = 0;

   if (video_driver_get_video_output_size(&width, &height, desc, sizeof(desc)))
   {
      char msg[PATH_MAX_LENGTH];

      msg[0] = '\0';

#if defined(_WIN32) || !defined(__PSL1GHT__) && !defined(__PS3__)
      generic_action_ok_command(CMD_EVENT_REINIT);
#endif
      video_driver_set_video_mode(width, height, true);
#ifdef GEKKO
      if (width == 0 || height == 0)
         strlcpy(msg, "Resetting to: DEFAULT", sizeof(msg));
      else
#endif
      {
         if (!string_is_empty(desc))
            snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_SCREEN_RESOLUTION_RESETTING_DESC), 
               width, height, desc);
         else
            snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC), 
               width, height);
      }

      runloop_msg_queue_push(msg, 1, 100, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
#endif

   return 0;
}

static int action_start_load_core(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   int ret                     = generic_action_ok_command(
         CMD_EVENT_UNLOAD_CORE);
   bool refresh                = false;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return ret;
}

#ifdef HAVE_BLUETOOTH
static int action_start_bluetooth(const char *path, const char *label,
         unsigned menu_type, size_t idx, size_t entry_idx)
{
   driver_bluetooth_remove_device((unsigned)idx);

   return 0;
}
#endif

#ifdef HAVE_NETWORKING
static int action_start_core_updater_entry(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   core_updater_list_t *core_list         = core_updater_list_get_cached();
   const core_updater_list_entry_t *entry = NULL;

   /* If specified core is installed, go to core
    * information menu */
   if (core_list &&
       core_updater_list_get_filename(core_list, path, &entry) &&
       !string_is_empty(entry->local_core_path) &&
       path_is_valid(entry->local_core_path))
      return action_ok_push_core_information_list(
            entry->local_core_path, label, type, idx, entry_idx);

   /* Otherwise do nothing */
   return 0;
}
#endif

static int action_start_core_lock(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   const char *core_path = path;
   bool refresh          = false;
   int ret               = 0;

   if (string_is_empty(core_path))
      return -1;

   /* Core should be unlocked by default
    * > If it is currently unlocked, do nothing */
   if (!core_info_get_core_lock(core_path, true))
      return ret;

   /* ...Otherwise, attempt to unlock it */
   if (!core_info_set_core_lock(core_path, false))
   {
      const char *core_name  = NULL;
      core_info_t *core_info = NULL;
      char msg[PATH_MAX_LENGTH];

      /* Need to fetch core name for error message */

      /* If core is found, use display name */
      if (core_info_find(core_path, &core_info) &&
          core_info->display_name)
         core_name = core_info->display_name;
      /* If not, use core file name */
      else
         core_name = path_basename_nocompression(core_path);

      /* Build error message */
      strlcpy(msg, msg_hash_to_str(MSG_CORE_UNLOCK_FAILED), sizeof(msg));

      if (!string_is_empty(core_name))
         strlcat(msg, core_name, sizeof(msg));

      /* Generate log + notification */
      RARCH_ERR("%s\n", msg);

      runloop_msg_queue_push(
         msg,
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      ret = -1;
   }

   /* Whenever lock status is changed, menu must be
    * refreshed - do this even in the event of an error,
    * since we don't want to leave the menu in an
    * undefined state */
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   return ret;
}

static int action_start_core_set_standalone_exempt(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   const char *core_path = path;
   int ret               = 0;

   if (string_is_empty(core_path))
      return -1;

   /* Core should not be exempt by default
    * > If it is currently 'not exempt', do nothing */
   if (!core_info_get_core_standalone_exempt(core_path))
      return ret;

   /* ...Otherwise, attempt to unset the exempt flag */
   if (!core_info_set_core_standalone_exempt(core_path, false))
   {
      const char *core_name  = NULL;
      core_info_t *core_info = NULL;
      char msg[PATH_MAX_LENGTH];

      /* Need to fetch core name for error message */

      /* If core is found, use display name */
      if (core_info_find(core_path, &core_info) &&
          core_info->display_name)
         core_name = core_info->display_name;
      /* If not, use core file name */
      else
         core_name = path_basename_nocompression(core_path);

      /* Build error message */
      strlcpy(msg,
            msg_hash_to_str(MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED),
            sizeof(msg));

      if (!string_is_empty(core_name))
         strlcat(msg, core_name, sizeof(msg));

      /* Generate log + notification */
      RARCH_ERR("%s\n", msg);

      runloop_msg_queue_push(
         msg,
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      ret = -1;
   }

   return ret;
}

static int action_start_lookup_setting(
      const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   return menu_setting_set(type, MENU_ACTION_START, false);
}

static int menu_cbs_init_bind_start_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CORE_LIST:
            BIND_ACTION_START(cbs, action_start_load_core);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_START(cbs, action_start_shader_preset);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND:
            BIND_ACTION_START(cbs, action_start_shader_preset_append);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND:
            BIND_ACTION_START(cbs, action_start_shader_preset_prepend);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_INFO:
            BIND_ACTION_START(cbs, action_start_remap_file_info);
            break;
         case MENU_ENUM_LABEL_VIDEO_FILTER:
            BIND_ACTION_START(cbs, action_start_video_filter_file_load);
            break;
         case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_START(cbs, action_start_audio_dsp_plugin_file_load);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_START(cbs, action_start_shader_pass);
#endif
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_START(cbs, action_start_shader_scale_pass);
#endif
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_START(cbs, action_start_shader_filter_pass);
#endif
            break;
         case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_START(cbs, action_start_shader_watch_for_changes);
#endif
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_START(cbs, action_start_shader_num_passes);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
#ifdef HAVE_CHEATS
            BIND_ACTION_START(cbs, action_start_cheat_num_passes);
#endif
            break;
         case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
            BIND_ACTION_START(cbs, action_start_video_resolution);
            break;
         case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
            BIND_ACTION_START(cbs, action_start_netplay_mitm_server);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE:
            BIND_ACTION_START(cbs, action_start_playlist_association);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE:
            BIND_ACTION_START(cbs, action_start_playlist_label_display_mode);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE:
            BIND_ACTION_START(cbs, action_start_playlist_right_thumbnail_mode);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE:
            BIND_ACTION_START(cbs, action_start_playlist_left_thumbnail_mode);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE:
            BIND_ACTION_START(cbs, action_start_playlist_sort_mode);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR:
            BIND_ACTION_START(cbs, action_start_manual_content_scan_dir);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
            BIND_ACTION_START(cbs, action_start_manual_content_scan_system_name);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME:
            BIND_ACTION_START(cbs, action_start_manual_content_scan_core_name);
            break;
#ifdef HAVE_BLUETOOTH
         case MENU_ENUM_LABEL_CONNECT_BLUETOOTH:
            BIND_ACTION_START(cbs, action_start_bluetooth);
            break;
#endif
         case MENU_ENUM_LABEL_STATE_SLOT:
            BIND_ACTION_START(cbs, action_start_state_slot);
            break;
         case MENU_ENUM_LABEL_MENU_WALLPAPER:
            BIND_ACTION_START(cbs, action_start_menu_wallpaper);
            break;
         default:
            return -1;
      }
   }
   else
      return -1;

   return 0;
}

static int menu_cbs_init_bind_start_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_START(cbs, action_start_shader_action_parameter);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_START(cbs, action_start_shader_action_preset_parameter);
   }
   else
#endif
   if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
   {
      BIND_ACTION_START(cbs, action_start_performance_counters_core);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_START(cbs, action_start_input_desc);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
   {
      BIND_ACTION_START(cbs, action_start_input_desc_kbd);
   }
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_PERF_COUNTERS_END)
   {
      BIND_ACTION_START(cbs, action_start_performance_counters_frontend);
   }
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START) &&
            (type < MENU_SETTINGS_CHEEVOS_START))
   {
      BIND_ACTION_START(cbs, action_start_core_setting);
   }
   else
   {
      switch (type)
      {
         case FILE_TYPE_PLAYLIST_COLLECTION:
            BIND_ACTION_START(cbs, action_ok_push_playlist_manager_settings);
            break;
#ifdef HAVE_NETWORKING
         case FILE_TYPE_DOWNLOAD_CORE:
            BIND_ACTION_START(cbs, action_start_core_updater_entry);
            break;
#endif
         case MENU_SETTING_ACTION_CORE_LOCK:
            BIND_ACTION_START(cbs, action_start_core_lock);
            break;
         case MENU_SETTING_ACTION_CORE_SET_STANDALONE_EXEMPT:
            BIND_ACTION_START(cbs, action_start_core_set_standalone_exempt);
            break;
         case MENU_SETTING_ACTION_SAVESTATE:
         case MENU_SETTING_ACTION_LOADSTATE:
            BIND_ACTION_START(cbs, action_start_state_slot);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

#ifdef HAVE_AUDIOMIXER
   if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END)
   {
      BIND_ACTION_START(cbs, action_start_audio_mixer_stream_volume);
      return 0;
   }
#endif

   BIND_ACTION_START(cbs, action_start_lookup_setting);

   if (menu_cbs_init_bind_start_compare_label(cbs) == 0)
      return 0;

   if (menu_cbs_init_bind_start_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
