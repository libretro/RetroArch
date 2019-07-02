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
#include "../menu_shader.h"

#include "../../configuration.h"
#include "../../core.h"
#include "../../core_info.h"
#include "../../managers/core_option_manager.h"
#include "../../managers/cheat_manager.h"
#include "../../retroarch.h"
#include "../../performance_counters.h"
#include "../../playlist.h"

#include "../../input/input_driver.h"
#include "../../input/input_remapping.h"

#include "../../config.def.h"

#ifndef BIND_ACTION_START
#define BIND_ACTION_START(cbs, name) \
   cbs->action_start = name; \
   cbs->action_start_ident = #name;
#endif

static int action_start_audio_mixer_stream_volume(unsigned type, const char *label)
{
   unsigned         offset      = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN);

   if (offset >= AUDIO_MIXER_MAX_STREAMS)
      return 0;

   audio_driver_mixer_set_stream_volume(offset, 1.0f);

   return 0;
}

static int action_start_remap_file_load(unsigned type, const char *label)
{
   input_remapping_set_defaults(true);
   return 0;
}

static int action_start_video_filter_file_load(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return -1;

   settings->paths.path_softfilter_plugin[0] = '\0';
   command_event(CMD_EVENT_REINIT, NULL);
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

static int action_start_performance_counters_core(unsigned type, const char *label)
{
   struct retro_perf_counter **counters = retro_get_perf_counter_libretro();
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   return generic_action_start_performance_counters(counters, offset, type, label);
}

static int action_start_performance_counters_frontend(unsigned type,
      const char *label)
{
   struct retro_perf_counter **counters = retro_get_perf_counter_rarch();
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;
   return generic_action_start_performance_counters(counters, offset, type, label);
}

static int action_start_input_desc(unsigned type, const char *label)
{
   settings_t           *settings = config_get_ptr();
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / (RARCH_FIRST_CUSTOM_BIND + 4);
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * (RARCH_FIRST_CUSTOM_BIND + 4));

   (void)label;

   if (inp_desc_button_index_offset < RARCH_FIRST_CUSTOM_BIND)
   {
      const struct retro_keybind *keyptr = &input_config_binds[inp_desc_user]
            [inp_desc_button_index_offset];
      settings->uints.input_remap_ids[inp_desc_user][inp_desc_button_index_offset] = keyptr->id;
   }
   else
      settings->uints.input_remap_ids[inp_desc_user][inp_desc_button_index_offset] =
         inp_desc_button_index_offset - RARCH_FIRST_CUSTOM_BIND;

   return 0;
}

static int action_start_shader_action_parameter(
      unsigned type, const char *label)
{
   video_shader_ctx_t shader_info;
   struct video_shader_parameter *param = NULL;
   unsigned parameter = type - MENU_SETTINGS_SHADER_PARAMETER_0;

   video_shader_driver_get_current_shader(&shader_info);

   if (!shader_info.data)
      return 0;

   param          = &shader_info.data->parameters
      [parameter];
   param->current = param->initial;
   param->current = MIN(MAX(param->minimum, param->current), param->maximum);

   return menu_shader_manager_clear_parameter(parameter);
}

static int action_start_shader_pass(unsigned type, const char *label)
{
   menu_handle_t *menu       = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu->scratchpad.unsigned_var = type - MENU_SETTINGS_SHADER_PASS_0;

   menu_shader_manager_clear_pass_path(menu->scratchpad.unsigned_var);

   return 0;
}

static int action_start_shader_scale_pass(unsigned type, const char *label)
{
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;

   menu_shader_manager_clear_pass_scale(pass);

   return 0;
}

static int action_start_shader_filter_pass(unsigned type, const char *label)
{
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   return menu_shader_manager_clear_pass_filter(pass);
}

static int action_start_netplay_mitm_server(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();
   strlcpy(settings->arrays.netplay_mitm_server, netplay_mitm_server, sizeof(settings->arrays.netplay_mitm_server));
   return 0;
}

static int action_start_shader_watch_for_changes(unsigned type, const char *label)
{
   settings_t *settings = config_get_ptr();
   settings->bools.video_shader_watch_files = DEFAULT_VIDEO_SHADER_WATCH_FILES;
   return 0;
}

static int action_start_shader_num_passes(unsigned type, const char *label)
{
   return menu_shader_manager_clear_num_passes();
}

static int action_start_cheat_num_passes(unsigned type, const char *label)
{
   if (cheat_manager_get_size())
   {
      bool refresh                = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      cheat_manager_realloc(0, CHEAT_HANDLER_TYPE_EMU);
   }

   return 0;
}

static int action_start_core_setting(unsigned type,
      const char *label)
{
   unsigned idx                = type - MENU_SETTINGS_CORE_OPTION_START;
   core_option_manager_t *coreopts = NULL;

   if (rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
      core_option_manager_set_default(coreopts, idx);

   return 0;
}

static int action_start_playlist_association(unsigned type, const char *label)
{
   playlist_t *playlist  = playlist_get_cached();
   bool load_playlist    = false;

   if (!playlist)
      return -1;

   /* Set default core path + name to DETECT */
   playlist_set_default_core_path(playlist, file_path_str(FILE_PATH_DETECT));
   playlist_set_default_core_name(playlist, file_path_str(FILE_PATH_DETECT));
   playlist_write_file(playlist);

   return 0;
}

static int action_start_video_resolution(unsigned type, const char *label)
{
   unsigned width = 0, height = 0;
   global_t *global = global_get_ptr();

   /*  Reset the resolution id to zero */
   global->console.screen.resolutions.current.id = 0;

   if (video_driver_get_video_output_size(&width, &height))
   {
      char msg[PATH_MAX_LENGTH];

      msg[0] = '\0';

      video_driver_set_video_mode(width, height, true);

      strlcpy(msg, "Resetting to: DEFAULT", sizeof(msg));
      runloop_msg_queue_push(msg, 1, 100, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   return 0;
}

static int action_start_lookup_setting(unsigned type, const char *label)
{
   return menu_setting_set(type, MENU_ACTION_START, false);
}

static int menu_cbs_init_bind_start_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_START(cbs, action_start_remap_file_load);
            break;
         case MENU_ENUM_LABEL_VIDEO_FILTER:
            BIND_ACTION_START(cbs, action_start_video_filter_file_load);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            BIND_ACTION_START(cbs, action_start_shader_pass);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
            BIND_ACTION_START(cbs, action_start_shader_scale_pass);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
            BIND_ACTION_START(cbs, action_start_shader_filter_pass);
            break;
         case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            BIND_ACTION_START(cbs, action_start_shader_watch_for_changes);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            BIND_ACTION_START(cbs, action_start_shader_num_passes);
            break;
         case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
            BIND_ACTION_START(cbs, action_start_cheat_num_passes);
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
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_start_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_START(cbs, action_start_shader_action_parameter);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_START(cbs, action_start_shader_action_parameter);
   }
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
   {
      BIND_ACTION_START(cbs, action_start_performance_counters_core);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_START(cbs, action_start_input_desc);
   }
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_PERF_COUNTERS_END)
   {
      BIND_ACTION_START(cbs, action_start_performance_counters_frontend);
   }
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
   {
      BIND_ACTION_START(cbs, action_start_core_setting);
   }
   else if (type == MENU_LABEL_SCREEN_RESOLUTION)
   {
      BIND_ACTION_START(cbs, action_start_video_resolution);
   }
   else
      return -1;

   return 0;
}

int menu_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END)
   {
      BIND_ACTION_START(cbs, action_start_audio_mixer_stream_volume);
      return 0;
   }

   BIND_ACTION_START(cbs, action_start_lookup_setting);

   if (menu_cbs_init_bind_start_compare_label(cbs) == 0)
      return 0;

   if (menu_cbs_init_bind_start_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
