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

#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_cbs.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../menu_shader.h"
#endif

#include "../../tasks/tasks_internal.h"

#include "../../core.h"
#include "../../core_info.h"
#include "../../configuration.h"
#include "../../file_path_special.h"
#include "../../managers/core_option_manager.h"
#include "../../managers/cheat_manager.h"
#include "../../performance_counters.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../wifi/wifi_driver.h"
#include "../../playlist.h"
#include "../../manual_content_scan.h"

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#endif

#ifndef BIND_ACTION_GET_VALUE
#define BIND_ACTION_GET_VALUE(cbs, name) \
   cbs->action_get_value = name; \
   cbs->action_get_value_ident = #name;
#endif

extern struct key_desc key_descriptors[RARCH_MAX_KEYS];

#ifdef HAVE_AUDIOMIXER
static void menu_action_setting_audio_mixer_stream_name(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned         offset      = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN);

   *w = 19;
   strlcpy(s2, path, len2);

   if (offset >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   strlcpy(s, audio_driver_mixer_get_stream_name(offset), len);
}

static void menu_action_setting_audio_mixer_stream_volume(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned         offset      = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN);

   *w = 19;
   strlcpy(s2, path, len2);

   if (offset >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   snprintf(s, len, "%.2f dB", audio_driver_mixer_get_stream_volume(offset));
}
#endif

static void menu_action_setting_disp_set_label_cheat_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);
   snprintf(s, len, "%u", cheat_manager_get_buf_size());
}

static void menu_action_setting_disp_set_label_cheevos_unsupported_entry(
   file_list_t* list,
   unsigned *w, unsigned type, unsigned i,
   const char *label,
   char *s, size_t len,
   const char *path,
   char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s,
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY), len);
}

static void menu_action_setting_disp_set_label_cheevos_unofficial_entry(
   file_list_t* list,
   unsigned *w, unsigned type, unsigned i,
   const char *label,
   char *s, size_t len,
   const char *path,
   char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s,
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY), len);
}

static void menu_action_setting_disp_set_label_cheevos_locked_entry(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY), len);
}

static void menu_action_setting_disp_set_label_cheevos_unlocked_entry(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY), len);
}

static void menu_action_setting_disp_set_label_cheevos_unlocked_entry_hardcore(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE), len);
}

static void menu_action_setting_disp_set_label_remap_file_load(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global = global_get_ptr();

   *w = 19;
   strlcpy(s2, path, len2);
   if (global && !string_is_empty(global->name.remapfile))
      fill_pathname_base(s, global->name.remapfile,
            len);
}

static void menu_action_setting_disp_set_label_configurations(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);

   if (!path_is_empty(RARCH_PATH_CONFIG))
      fill_pathname_base(s, path_get(RARCH_PATH_CONFIG),
            len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT), len);
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static void menu_action_setting_disp_set_label_shader_filter_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[type - MENU_SETTINGS_SHADER_PASS_FILTER_0] : NULL;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   if (!shader_pass)
      return;

  switch (shader_pass->filter)
  {
     case 0:
        strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE),
              len);
        break;
     case 1:
        strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LINEAR),
              len);
        break;
     case 2:
        strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NEAREST),
              len);
        break;
  }
}
#endif

#ifdef HAVE_NETWORKING
static void menu_action_setting_disp_set_label_netplay_mitm_server(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned j;
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   if (!settings)
      return;

   if (string_is_empty(settings->arrays.netplay_mitm_server))
      return;

   for (j = 0; j < ARRAY_SIZE(netplay_mitm_server_list); j++)
   {
      if (string_is_equal(settings->arrays.netplay_mitm_server,
               netplay_mitm_server_list[j].name))
         strlcpy(s, netplay_mitm_server_list[j].description, len);
   }
}
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static void menu_action_setting_disp_set_label_shader_watch_for_changes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   if (settings)
   {
      if (settings->bools.video_shader_watch_files)
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE), len);
      else
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FALSE), len);
   }
}

static void menu_action_setting_disp_set_label_shader_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   struct video_shader *shader = menu_shader_get();
   unsigned pass_count         = shader ? shader->passes : 0;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);
   snprintf(s, len, "%u", pass_count);
}

static void menu_action_setting_disp_set_label_shader_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[type - MENU_SETTINGS_SHADER_PASS_0] : NULL;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);

   if (!shader_pass)
      return;

   if (!string_is_empty(shader_pass->source.path))
      fill_pathname_base(s, shader_pass->source.path, len);
}

static void menu_action_setting_disp_set_label_shader_default_filter(

      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = 19;

   if (!settings)
      return;

   if (settings->bools.video_smooth)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LINEAR), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NEAREST), len);
}

static void menu_action_setting_disp_set_label_shader_parameter_internal(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2,
      unsigned offset)
{
   video_shader_ctx_t shader_info;
   const struct video_shader_parameter *param = NULL;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   video_shader_driver_get_current_shader(&shader_info);

   if (!shader_info.data)
      return;

   param = &shader_info.data->parameters[type - offset];

   if (!param)
      return;

   snprintf(s, len, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
}

static void menu_action_setting_disp_set_label_shader_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_disp_set_label_shader_parameter_internal(list, w, type, i,
         label, s, len, path, s2, len2, MENU_SETTINGS_SHADER_PARAMETER_0);
}

static void menu_action_setting_disp_set_label_shader_preset_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_disp_set_label_shader_parameter_internal(list, w, type, i,
         label, s, len, path, s2, len2, MENU_SETTINGS_SHADER_PRESET_PARAMETER_0);
}

static void menu_action_setting_disp_set_label_shader_scale_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned pass                         = 0;
   unsigned scale_value                  = 0;
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[type - MENU_SETTINGS_SHADER_PASS_SCALE_0] : NULL;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   (void)pass;
   (void)scale_value;

   if (!shader_pass)
      return;

   scale_value = shader_pass->fbo.scale_x;

   if (!scale_value)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE), len);
   else
      snprintf(s, len, "%ux", scale_value);
}
#endif

static void menu_action_setting_disp_set_label_menu_file_core(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *alt = NULL;
   strlcpy(s, "(CORE)", len);

   menu_entries_get_at_offset(list, i, NULL,
         NULL, NULL, NULL, &alt);

   *w = (unsigned)strlen(s);
   if (alt)
      strlcpy(s2, alt, len2);
}

static void menu_action_setting_disp_set_label_input_desc(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   rarch_system_info_t *system           = runloop_get_system_info();
   settings_t *settings                  = config_get_ptr();
   const char* descriptor                = NULL;
   char buf[256];

   unsigned btn_idx, user_idx, remap_idx;

   if (!settings)
      return;

   user_idx  = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) / (RARCH_FIRST_CUSTOM_BIND + 8);
   btn_idx   = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) - (RARCH_FIRST_CUSTOM_BIND + 8) * user_idx;
   remap_idx =
      settings->uints.input_remap_ids[user_idx][btn_idx];

   if (!system)
      return;

   if (remap_idx != RARCH_UNMAPPED)
      descriptor = system->input_desc_btn[user_idx][remap_idx];

   if (!string_is_empty(descriptor) && remap_idx < RARCH_FIRST_CUSTOM_BIND)
      strlcpy(s, descriptor, len);
   else if (!string_is_empty(descriptor) && remap_idx >= RARCH_FIRST_CUSTOM_BIND && remap_idx % 2 == 0)
   {
      snprintf(buf, sizeof(buf), "%s %c", descriptor, '+');
      strlcpy(s, buf, len);
   }
   else if (!string_is_empty(descriptor) && remap_idx >= RARCH_FIRST_CUSTOM_BIND && remap_idx % 2 != 0)
   {
      snprintf(buf, sizeof(buf), "%s %c", descriptor, '-');
      strlcpy(s, buf, len);
   }
   else
      strlcpy(s, "---", len);

   *w = 19;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_input_desc_kbd(
   file_list_t* list,
   unsigned *w, unsigned type, unsigned i,
   const char *label,
   char *s, size_t len,
   const char *path,
   char *s2, size_t len2)
{
   char desc[PATH_MAX_LENGTH];
   unsigned key_id, btn_idx;
   unsigned remap_id;
   unsigned user_idx = 0;

   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   user_idx = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) / RARCH_FIRST_CUSTOM_BIND;
   btn_idx  = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) - RARCH_FIRST_CUSTOM_BIND * user_idx;
   remap_id =
      settings->uints.input_keymapper_ids[user_idx][btn_idx];

   for (key_id = 0; key_id < RARCH_MAX_KEYS - 1; key_id++)
   {
      if (remap_id == key_descriptors[key_id].key)
         break;
   }

   if (key_descriptors[key_id].key != RETROK_FIRST)
   {
      snprintf(desc, sizeof(desc), "Keyboard %s", key_descriptors[key_id].desc);
      strlcpy(s, desc, len);
   }
   else
      strlcpy(s, "---", len);

   *w = 19;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned cheat_index = type - MENU_SETTINGS_CHEAT_BEGIN;

   if (cheat_index < cheat_manager_get_buf_size())
   {
      if (cheat_manager_state.cheats[cheat_index].handler == CHEAT_HANDLER_TYPE_EMU)
         snprintf(s, len, "(%s) : %s",
               cheat_manager_get_code_state(cheat_index) ?
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON) :
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               cheat_manager_get_code(cheat_index)
               ? cheat_manager_get_code(cheat_index) :
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)
               );
      else
         snprintf(s, len, "(%s) : %08X",
               cheat_manager_get_code_state(cheat_index) ?
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON) :
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               cheat_manager_state.cheats[cheat_index].address
               );
   }
   *w = 19;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_cheat_match(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned int address = 0;
   unsigned int address_mask = 0;
   unsigned int prev_val = 0;
   unsigned int curr_val = 0 ;
   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_VIEW, cheat_manager_state.match_idx, &address, &address_mask, &prev_val, &curr_val);

   snprintf(s, len, "Prev: %u Curr: %u", prev_val, curr_val);
   *w = 19;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_perf_counters_common(
      struct retro_perf_counter **counters,
      unsigned offset, char *s, size_t len
      )
{
   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   snprintf(s, len,
         "%" PRIu64 " ticks, %" PRIu64 " runs.",
         ((uint64_t)counters[offset]->total /
          (uint64_t)counters[offset]->call_cnt),
         (uint64_t)counters[offset]->call_cnt);
}

static void general_disp_set_label_perf_counters(
      struct retro_perf_counter **counters,
      unsigned offset,
      char *s, size_t len,
      char *s2, size_t len2,
      const char *path, unsigned *w
      )
{
   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   menu_action_setting_disp_set_label_perf_counters_common(
         counters, offset, s, len);
   menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);
}

static void menu_action_setting_disp_set_label_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   struct retro_perf_counter **counters = retro_get_perf_counter_rarch();
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;
   general_disp_set_label_perf_counters(counters, offset, s, len,
         s2, len, path, w);
}

static void menu_action_setting_disp_set_label_libretro_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   struct retro_perf_counter **counters = retro_get_perf_counter_libretro();
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;
   general_disp_set_label_perf_counters(counters, offset, s, len,
         s2, len, path, w);
}

static void menu_action_setting_disp_set_label_menu_more(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MORE), len);
   *w = 19;
   if (!string_is_empty(path))
      strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_db_entry(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MORE), len);
   *w = 10;
   if (!string_is_empty(path))
      strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_entry_url(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *representation_label = list->list[i].alt
      ? list->list[i].alt
      : list->list[i].path;
   *s = '\0';
   *w = 8;

   if (!string_is_empty(representation_label))
      strlcpy(s2, representation_label, len2);
   else
      strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_entry(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = 8;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_wifi_is_online(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   strlcpy(s2, path, len2);
   *w = 19;

   if (driver_wifi_ssid_is_online(i))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE), len);
}

static void menu_action_setting_disp_set_label_menu_disk_index(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned images = 0, current                = 0;
   struct retro_disk_control_callback *control = NULL;
   rarch_system_info_t *system                 = runloop_get_system_info();

   if (!system)
      return;

   control = &system->disk_control_cb;

   if (!control)
      return;

   *w = 19;
   *s = '\0';
   strlcpy(s2, path, len2);

   if (!control->get_num_images)
      return;
   if (!control->get_image_index)
      return;

   images  = control->get_num_images();
   current = control->get_image_index();

   if (current >= images)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_DISK), len);
   else
      snprintf(s, len, "%u", current + 1);
}

static void menu_action_setting_disp_set_label_menu_video_resolution(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned width = 0, height = 0;

   *w = 19;
   *s = '\0';

   strlcpy(s2, path, len2);

   if (video_driver_get_video_output_size(&width, &height))
   {
#ifdef GEKKO
      if (width == 0 || height == 0)
         strlcpy(s, "DEFAULT", len);
      else
#endif
         snprintf(s, len, "%ux%u", width, height);
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
}

static void menu_action_setting_generic_disp_set_label(
      unsigned *w, char *s, size_t len,
      const char *path, const char *label,
      char *s2, size_t len2)
{
   *s = '\0';

   if (label)
      strlcpy(s, label, len);
   *w = (unsigned)strlen(s);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_menu_file_plain(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(FILE)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_imageviewer(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(IMAGE)", s2, len2);
}

static void menu_action_setting_disp_set_label_movie(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(MOVIE)", s2, len2);
}

static void menu_action_setting_disp_set_label_music(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(MUSIC)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_use_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, NULL, s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(DIR)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_parent_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, NULL, s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(COMP)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_shader(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(SHADER)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_shader_preset(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(PRESET)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_in_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CFILE)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_overlay(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(OVERLAY)", s2, len2);
}

#ifdef HAVE_VIDEO_LAYOUT
static void menu_action_setting_disp_set_label_menu_file_video_layout(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(Video Layout)", s2, len2);
}
#endif

static void menu_action_setting_disp_set_label_menu_file_config(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CONFIG)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_font(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(FONT)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(FILTER)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_url_core(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *alt = NULL;
   strlcpy(s, "(CORE)", len);

   menu_entries_get_at_offset(list, i, NULL,
         NULL, NULL, NULL, &alt);

   *w = (unsigned)strlen(s);
   if (alt)
      strlcpy(s2, alt, len2);
}

static void menu_action_setting_disp_set_label_menu_file_rdb(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(RDB)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_cursor(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CURSOR)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CHEAT)", s2, len2);
}

static void menu_action_setting_disp_set_label_core_option_create(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = 19;

   strlcpy(s, "", len);

   if (!string_is_empty(path_get(RARCH_PATH_BASENAME)))
      strlcpy(s,  path_basename(path_get(RARCH_PATH_BASENAME)), len);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_playlist_associations(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   playlist_t *playlist  = playlist_get_cached();
   const char *core_name = NULL;

   *s = '\0';
   *w = 19;

   strlcpy(s2, path, len2);

   if (!playlist)
      return;

   core_name = playlist_get_default_core_name(playlist);

   if (!string_is_empty(core_name) &&
       !string_is_equal(core_name, "DETECT"))
      strlcpy(s, core_name, len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
}

static void menu_action_setting_disp_set_label_playlist_label_display_mode(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   enum playlist_label_display_mode label_display_mode;
   playlist_t *playlist  = playlist_get_cached();

   if (!playlist)
      return;

   label_display_mode = playlist_get_label_display_mode(playlist);

   *w = 19;

   strlcpy(s2, path, len2);

   switch (label_display_mode)
   {
      case LABEL_DISPLAY_MODE_REMOVE_PARENTHESES :
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS), len);
         break;
      case LABEL_DISPLAY_MODE_REMOVE_BRACKETS :
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS), len);
         break;
      case LABEL_DISPLAY_MODE_REMOVE_PARENTHESES_AND_BRACKETS :
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS), len);
         break;
      case LABEL_DISPLAY_MODE_KEEP_DISC_INDEX :
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX), len);
         break;
      case LABEL_DISPLAY_MODE_KEEP_REGION :
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION), len);
         break;
      case LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX :
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX), len);
         break;
      default:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT), len);
   }
}

static const char *get_playlist_thumbnail_mode_value(playlist_t *playlist, enum playlist_thumbnail_id thumbnail_id)
{
   enum playlist_thumbnail_mode thumbnail_mode =
         playlist_get_thumbnail_mode(playlist, thumbnail_id);

   switch (thumbnail_mode)
   {
      case PLAYLIST_THUMBNAIL_MODE_OFF:
         return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
      case PLAYLIST_THUMBNAIL_MODE_SCREENSHOTS:
         return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS);
      case PLAYLIST_THUMBNAIL_MODE_TITLE_SCREENS:
         return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS);
      case PLAYLIST_THUMBNAIL_MODE_BOXARTS:
         return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS);
      default:
         /* PLAYLIST_THUMBNAIL_MODE_DEFAULT */
         break;
   }

   return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT);
}

static void menu_action_setting_disp_set_label_playlist_right_thumbnail_mode(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   playlist_t *playlist  = playlist_get_cached();

   *s = '\0';
   *w = 19;

   strlcpy(s2, path, len2);

   if (!playlist)
      return;

   strlcpy(
         s,
         get_playlist_thumbnail_mode_value(playlist, PLAYLIST_THUMBNAIL_RIGHT),
         len);
}

static void menu_action_setting_disp_set_label_playlist_left_thumbnail_mode(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   playlist_t *playlist  = playlist_get_cached();

   *s = '\0';
   *w = 19;

   strlcpy(s2, path, len2);

   if (!playlist)
      return;

   strlcpy(
         s,
         get_playlist_thumbnail_mode_value(playlist, PLAYLIST_THUMBNAIL_LEFT),
         len);
}

static void menu_action_setting_disp_set_label_core_options(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   core_option_manager_t *coreopts = NULL;
   const char *core_opt = NULL;

   *s = '\0';
   *w = 19;

   if (rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
   {
      core_opt = core_option_manager_get_val_label(coreopts,
            type - MENU_SETTINGS_CORE_OPTION_START);

      strlcpy(s, "", len);

      if (core_opt)
      {
         if (string_is_equal(core_opt, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)))
            core_opt = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON);
         else if (string_is_equal(core_opt, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)))
            core_opt = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
         strlcpy(s, core_opt, len);
      }
   }

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_achievement_information(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = 2;

   menu_setting_get_label(list, s,
         len, w, type, label, i);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_manual_content_scan_dir(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *content_dir = NULL;

   *s = '\0';
   *w = 19;

   strlcpy(s2, path, len2);

   if (!manual_content_scan_get_menu_content_dir(&content_dir))
      return;

   strlcpy(s, content_dir, len);
}

static void menu_action_setting_disp_set_label_manual_content_scan_system_name(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *system_name = NULL;

   *s = '\0';
   *w = 19;

   strlcpy(s2, path, len2);

   if (!manual_content_scan_get_menu_system_name(&system_name))
      return;

   strlcpy(s, system_name, len);
}

static void menu_action_setting_disp_set_label_manual_content_scan_core_name(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *core_name = NULL;

   *s = '\0';
   *w = 19;

   strlcpy(s2, path, len2);

   if (!manual_content_scan_get_menu_core_name(&core_name))
      return;

   strlcpy(s, core_name, len);
}

static void menu_action_setting_disp_set_label_no_items(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = 19;

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = 19;

   menu_setting_get_label(list, s,
         len, w, type, label, i);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_setting_bool(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   rarch_setting_t *setting = menu_setting_find(list->list[i].label);

   *s = '\0';
   *w = 19;

   if (setting)
   {
      if (*setting->value.target.boolean)
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
      else
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
   }

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_setting_string(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   rarch_setting_t *setting = menu_setting_find(list->list[i].label);

   *w = 19;

   if (setting->value.target.string)
      strlcpy(s, setting->value.target.string, len);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_setting_path(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   rarch_setting_t *setting = menu_setting_find(list->list[i].label);
   const char *basename     = setting ? path_basename(setting->value.target.string) : NULL;

   *w = 19;

   if (!string_is_empty(basename))
      strlcpy(s, basename, len);

   strlcpy(s2, path, len2);
}

static int menu_cbs_init_bind_get_string_representation_compare_label(
      menu_file_list_cbs_t *cbs)
{
   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_VIDEO_DRIVER:
         case MENU_ENUM_LABEL_AUDIO_DRIVER:
         case MENU_ENUM_LABEL_INPUT_DRIVER:
         case MENU_ENUM_LABEL_JOYPAD_DRIVER:
         case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         case MENU_ENUM_LABEL_RECORD_DRIVER:
         case MENU_ENUM_LABEL_MIDI_DRIVER:
         case MENU_ENUM_LABEL_LOCATION_DRIVER:
         case MENU_ENUM_LABEL_CAMERA_DRIVER:
         case MENU_ENUM_LABEL_WIFI_DRIVER:
         case MENU_ENUM_LABEL_MENU_DRIVER:
            BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label);
            break;
         case MENU_ENUM_LABEL_CONNECT_WIFI:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_wifi_is_online);
            break;
         case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_cheat_num_passes);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_remap_file_load);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_shader_filter_pass);
#endif
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_shader_scale_pass);
#endif
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_shader_num_passes);
#endif
            break;
         case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_shader_watch_for_changes);
#endif
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_shader_pass);
#endif
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_shader_default_filter);
#endif
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_configurations);
            break;
         case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_menu_video_resolution);
            break;
         case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         case MENU_ENUM_LABEL_FAVORITES:
         case MENU_ENUM_LABEL_CORE_OPTIONS:
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         case MENU_ENUM_LABEL_SHADER_OPTIONS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
         case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
         case MENU_ENUM_LABEL_CORE_COUNTERS:
         case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
         case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         case MENU_ENUM_LABEL_RESTART_CONTENT:
         case MENU_ENUM_LABEL_CLOSE_CONTENT:
         case MENU_ENUM_LABEL_RESUME_CONTENT:
         case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         case MENU_ENUM_LABEL_CORE_INFORMATION:
         case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         case MENU_ENUM_LABEL_SAVE_STATE:
         case MENU_ENUM_LABEL_LOAD_STATE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_menu_more);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_playlist_associations);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_playlist_label_display_mode);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_playlist_right_thumbnail_mode);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_playlist_left_thumbnail_mode);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_manual_content_scan_dir);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_manual_content_scan_system_name);
            break;
         case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_manual_content_scan_core_name);
            break;
         default:
            return - 1;
      }
   }
   else
   {
      return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_get_string_representation_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   unsigned i;
   typedef struct info_range_list
   {
      unsigned min;
      unsigned max;
      void (*cb)(file_list_t* list,
            unsigned *w, unsigned type, unsigned i,
            const char *label, char *s, size_t len,
            const char *path,
            char *path_buf, size_t path_buf_size);
   } info_range_list_t;

   info_range_list_t info_list[] = {
#ifdef HAVE_AUDIOMIXER
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN, 
         MENU_SETTINGS_AUDIO_MIXER_STREAM_END,
         menu_action_setting_audio_mixer_stream_name
      },
      {
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN,
         MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END,
         menu_action_setting_audio_mixer_stream_volume
      },
#endif
      {
         MENU_SETTINGS_INPUT_DESC_BEGIN,
         MENU_SETTINGS_INPUT_DESC_END,
         menu_action_setting_disp_set_label_input_desc
      },
      {
         MENU_SETTINGS_CHEAT_BEGIN,
         MENU_SETTINGS_CHEAT_END,
         menu_action_setting_disp_set_label_cheat
      },
      {
         MENU_SETTINGS_PERF_COUNTERS_BEGIN,
         MENU_SETTINGS_PERF_COUNTERS_END,
         menu_action_setting_disp_set_label_perf_counters
      },
      {
         MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN,
         MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END,
         menu_action_setting_disp_set_label_libretro_perf_counters
      },
      {
         MENU_SETTINGS_INPUT_DESC_KBD_BEGIN,
         MENU_SETTINGS_INPUT_DESC_KBD_END,
         menu_action_setting_disp_set_label_input_desc_kbd
      },
   };

   for (i = 0; i < ARRAY_SIZE(info_list); i++)
   {
      if (type >= info_list[i].min && type <= info_list[i].max)
      {
         BIND_ACTION_GET_VALUE(cbs, info_list[i].cb);
         return 0;
      }
   }

   switch (type)
   {
      case MENU_SETTINGS_CORE_OPTION_CREATE:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_core_option_create);
         break;
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_core);
         break;
      case FILE_TYPE_PLAIN:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_plain);
         break;
      case FILE_TYPE_MOVIE:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_movie);
         break;
      case FILE_TYPE_MUSIC:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_music);
         break;
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_imageviewer);
         break;
      case FILE_TYPE_USE_DIRECTORY:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_use_directory);
         break;
      case FILE_TYPE_DIRECTORY:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_directory);
         break;
      case FILE_TYPE_PARENT_DIRECTORY:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_parent_directory);
         break;
      case FILE_TYPE_CARCHIVE:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_carchive);
         break;
      case FILE_TYPE_OVERLAY:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_overlay);
         break;
#ifdef HAVE_VIDEO_LAYOUT
      case FILE_TYPE_VIDEO_LAYOUT:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_video_layout);
         break;
#endif
      case FILE_TYPE_FONT:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_font);
         break;
      case FILE_TYPE_SHADER:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_shader);
         break;
      case FILE_TYPE_SHADER_PRESET:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_shader_preset);
         break;
      case FILE_TYPE_CONFIG:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_config);
         break;
      case FILE_TYPE_IN_CARCHIVE:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_in_carchive);
         break;
      case FILE_TYPE_VIDEOFILTER:
      case FILE_TYPE_AUDIOFILTER:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_filter);
         break;
      case FILE_TYPE_DOWNLOAD_CORE:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_url_core);
         break;
      case FILE_TYPE_RDB:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_rdb);
         break;
      case FILE_TYPE_CURSOR:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_cursor);
         break;
      case FILE_TYPE_CHEAT:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_cheat);
         break;
      case MENU_SETTINGS_CHEAT_MATCH:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_cheat_match);
         break;
      case MENU_SETTING_SUBGROUP:
      case MENU_SETTINGS_CUSTOM_BIND_ALL:
      case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
      case MENU_SETTING_ACTION:
      case MENU_SETTING_ACTION_LOADSTATE:
      case 7:   /* Run */
      case MENU_SETTING_ACTION_DELETE_ENTRY:
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_more);
         break;
      case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_disk_index);
         break;
      case 31: /* Database entry */
         BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label_db_entry);
         break;
      case 25: /* URL directory entries */
      case 26: /* URL entries */
         BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label_entry_url);
         break;
      case MENU_SETTING_DROPDOWN_SETTING_INT_ITEM:
      case MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM:
      case MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM:
      case MENU_SETTING_DROPDOWN_ITEM:
      case MENU_SETTING_NO_ITEM:
         BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label_no_items);
         break;
      case 32: /* Recent history entry */
      case 65535: /* System info entry */
         BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label_entry);
         break;
      default:
#if 0
         RARCH_LOG("type: %d\n", type);
#endif
         BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label);
         break;
   }

   return 0;
}

int menu_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   if (strstr(label, "joypad_index") && strstr(label, "input_player"))
   {
      BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label);
      return 0;
   }

#if 0
   RARCH_LOG("MENU_SETTINGS_NONE: %d\n", MENU_SETTINGS_NONE);
   RARCH_LOG("MENU_SETTINGS_LAST: %d\n", MENU_SETTINGS_LAST);
#endif

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_cheevos_unlocked_entry);
            return 0;
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY_HARDCORE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_cheevos_unlocked_entry_hardcore);
            return 0;
         case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_cheevos_locked_entry);
            return 0;
         case MENU_ENUM_LABEL_CHEEVOS_UNSUPPORTED_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_cheevos_unsupported_entry);
            return 0;
         case MENU_ENUM_LABEL_CHEEVOS_UNOFFICIAL_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_cheevos_unofficial_entry);
            return 0;
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_menu_more);
            return 0;
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_achievement_information);
            return 0;
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_achievement_information);
            return 0;
         case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
#ifdef HAVE_NETWORKING
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_netplay_mitm_server);
#endif
            return 0;
         default:
            break;
      }
   }

   if (cbs->setting)
   {
      switch (setting_get_type(cbs->setting))
      {
         case ST_BOOL:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_setting_bool);
            return 0;
         case ST_STRING:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_setting_string);
            return 0;
         case ST_PATH:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_setting_path);
            return 0;
         default:
            break;
      }
   }

   if ((type >= MENU_SETTINGS_CORE_OPTION_START) &&
       (type < MENU_SETTINGS_CHEEVOS_START))
   {
      BIND_ACTION_GET_VALUE(cbs,
         menu_action_setting_disp_set_label_core_options);
      return 0;
   }

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_GET_VALUE(cbs,
         menu_action_setting_disp_set_label_shader_parameter);
      return 0;
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_GET_VALUE(cbs,
         menu_action_setting_disp_set_label_shader_preset_parameter);
      return 0;
   }
#endif

   if (menu_cbs_init_bind_get_string_representation_compare_label(cbs) == 0)
      return 0;

   if (menu_cbs_init_bind_get_string_representation_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
