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
#include "../menu_entries.h"
#include "../menu_cbs.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../menu_shader.h"
#endif

#include "../../configuration.h"
#include "../../core.h"
#include "../../core_info.h"
#ifdef HAVE_CHEATS
#include "../../cheat_manager.h"
#endif
#include "../../file_path_special.h"
#include "../../driver.h"
#include "../../retroarch.h"
#include "../../audio/audio_driver.h"
#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#endif
#include "../../playlist.h"
#include "../../manual_content_scan.h"
#include "../misc/cpufreq/cpufreq.h"

#ifndef BIND_ACTION_LEFT
#define BIND_ACTION_LEFT(cbs, name) (cbs)->action_left = (name)
#endif

/* Forward declarations */
int action_ok_core_lock(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx);
int action_ok_core_set_standalone_exempt(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx);

extern struct key_desc key_descriptors[RARCH_MAX_KEYS];

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static int generic_shader_action_parameter_left(
      struct video_shader_parameter *param,
      unsigned type, const char *label, bool wraparound)
{
   param->current -= param->step;
   param->current  = MIN(MAX(param->minimum, param->current),
         param->maximum);
   return 0;
}

static int shader_action_parameter_left_internal(unsigned type, const char *label, bool wraparound,
      unsigned offset)
{
   video_shader_ctx_t shader_info;
   struct video_shader *shader          = menu_shader_get();
   struct video_shader_parameter *param_menu = NULL;
   struct video_shader_parameter *param_prev = NULL;

   int ret = 0;

   video_shader_driver_get_current_shader(&shader_info);

   param_prev = &shader_info.data->parameters[type - offset];
   param_menu = shader ? &shader->parameters [type - offset] : NULL;

   if (!param_prev || !param_menu)
      return -1;
   ret = generic_shader_action_parameter_left(param_prev, type, label, wraparound);

   param_menu->current = param_prev->current;

   shader->flags      |= SHDR_FLAG_MODIFIED;

   return ret;
}

static int shader_action_parameter_left(unsigned type, const char *label, bool wraparound)
{
   return shader_action_parameter_left_internal(type, label, wraparound, MENU_SETTINGS_SHADER_PARAMETER_0);
}

static int shader_action_preset_parameter_left(unsigned type, const char *label, bool wraparound)
{
   return shader_action_parameter_left_internal(type, label, wraparound, MENU_SETTINGS_SHADER_PRESET_PARAMETER_0);
}
#endif

#ifdef HAVE_AUDIOMIXER
static int audio_mixer_stream_volume_left(unsigned type, const char *label,
      bool wraparound)
{
   unsigned         offset      = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN);
   float orig_volume            = 0.0f;

   if (offset >= AUDIO_MIXER_MAX_STREAMS)
      return 0;

   orig_volume                  = audio_driver_mixer_get_stream_volume(offset);
   orig_volume                  = orig_volume - 1.00f;

   audio_driver_mixer_set_stream_volume(offset, orig_volume);

   return 0;
}
#endif

#ifdef HAVE_CHEATS
static int action_left_cheat(unsigned type, const char *label,
      bool wraparound)
{
   size_t idx             = type - MENU_SETTINGS_CHEAT_BEGIN;
   return generic_action_cheat_toggle(idx, type, label,
         wraparound);
}
#endif

static int action_left_input_desc(unsigned type, const char *label,
   bool wraparound)
{
   unsigned btn_idx;
   unsigned user_idx;
   unsigned remap_idx;
   unsigned bind_idx;
   unsigned mapped_port;
   settings_t *settings                  = config_get_ptr();
   rarch_system_info_t *sys_info         = &runloop_state_get_ptr()->system;
   if (!settings || !sys_info)
      return 0;

   user_idx    = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) / (RARCH_FIRST_CUSTOM_BIND + 8);
   btn_idx     = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) - (RARCH_FIRST_CUSTOM_BIND + 8) * user_idx;
   mapped_port = settings->uints.input_remap_ports[user_idx];

   if (settings->uints.input_remap_ids[user_idx][btn_idx] == RARCH_UNMAPPED)
      settings->uints.input_remap_ids[user_idx][btn_idx] = RARCH_CUSTOM_BIND_LIST_END - 1;

   remap_idx = settings->uints.input_remap_ids[user_idx][btn_idx];
   for (bind_idx = 0; bind_idx < RARCH_ANALOG_BIND_LIST_END; bind_idx++)
   {
      if (input_config_bind_order[bind_idx] == remap_idx)
         break;
   }

   if (bind_idx > 0)
   {
      if (bind_idx > RARCH_ANALOG_BIND_LIST_END)
         settings->uints.input_remap_ids[user_idx][btn_idx]--;
      else
      {
         bind_idx--;
         bind_idx = input_config_bind_order[bind_idx];
         settings->uints.input_remap_ids[user_idx][btn_idx] = bind_idx;
      }
   }
   else if (bind_idx == 0)
      settings->uints.input_remap_ids[user_idx][btn_idx] = RARCH_UNMAPPED;
   else
      settings->uints.input_remap_ids[user_idx][btn_idx] = RARCH_CUSTOM_BIND_LIST_END - 1;

   remap_idx = settings->uints.input_remap_ids[user_idx][btn_idx];

   /* skip the not used buttons (unless they are at the end by calling the right desc function recursively
      also skip all the axes until analog remapping is implemented */
   if (remap_idx != RARCH_UNMAPPED)
   {
      if ((string_is_empty(sys_info->input_desc_btn[mapped_port][remap_idx]) && remap_idx < RARCH_CUSTOM_BIND_LIST_END) /*||
          (remap_idx >= RARCH_FIRST_CUSTOM_BIND && remap_idx < RARCH_CUSTOM_BIND_LIST_END)*/)
         action_left_input_desc(type, label, wraparound);
   }

   return 0;
}

static int action_left_input_desc_kbd(unsigned type, const char *label,
   bool wraparound)
{
   unsigned remap_id;
   unsigned key_id, user_idx, btn_idx;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return 0;

   user_idx = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) / RARCH_ANALOG_BIND_LIST_END;
   btn_idx  = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) - RARCH_ANALOG_BIND_LIST_END * user_idx;

   remap_id =
      settings->uints.input_keymapper_ids[user_idx][btn_idx];

   for (key_id = 0; key_id < RARCH_MAX_KEYS; key_id++)
   {
      if (remap_id == key_descriptors[key_id].key)
         break;
   }

   if (key_id > 0)
      key_id--;
   else
      key_id = RARCH_MAX_KEYS - 1;

   settings->uints.input_keymapper_ids[user_idx][btn_idx] = key_descriptors[key_id].key;

   return 0;
}

static int action_left_scroll(unsigned type, const char *label,
      bool wraparound)
{
   struct menu_state *menu_st    = menu_state_get_ptr();
   size_t selection              = menu_st->selection_ptr;
   size_t scroll_accel           = menu_st->scroll.acceleration;
   unsigned scroll_speed         = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);
   unsigned fast_scroll_speed    = 10 * scroll_speed;

   if (selection > fast_scroll_speed)
   {
      size_t idx                 = selection - fast_scroll_speed;
      menu_st->selection_ptr     = idx;
      if (menu_st->driver_ctx->navigation_set)
         menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
   }
   else
   {
      bool pending_push          = false;
      menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
   }
#ifdef HAVE_AUDIOMIXER
   if (selection != menu_st->selection_ptr) /* Changed? */
      audio_driver_mixer_play_scroll_sound(true);
#endif
   return 0;
}

static int action_left_mainmenu(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_XMB
   struct menu_state    *menu_st       = menu_state_get_ptr();
   const menu_ctx_driver_t *driver_ctx = menu_st->driver_ctx;
   size_t size                         = (driver_ctx && driver_ctx->list_get_size) ? driver_ctx->list_get_size(menu_st->userdata, MENU_LIST_PLAIN) : 0;
   const char *menu_ident              = (driver_ctx && driver_ctx->ident) ? driver_ctx->ident : NULL;
   /* Tab switching functionality only applies
    * to XMB */
   if (  (size == 1)
       && string_is_equal(menu_ident, "xmb"))
   {
      settings_t            *settings  = config_get_ptr();
      bool menu_nav_wraparound_enable  = settings->bools.menu_navigation_wraparound_enable;
      size_t selection                 = (driver_ctx && driver_ctx->list_get_selection) ? driver_ctx->list_get_selection(menu_st->userdata) : 0;
      if ((selection != 0) || menu_nav_wraparound_enable)
      {
         menu_list_t *menu_list        = menu_st->entries.list;
         file_list_t *selection_buf    = menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;

         if (menu_st->driver_ctx && menu_st->driver_ctx->list_cache)
            menu_st->driver_ctx->list_cache(menu_st->userdata,
                  MENU_LIST_HORIZONTAL, MENU_ACTION_LEFT);
         return menu_driver_deferred_push_content_list(selection_buf);
      }
   }
   else
#endif
      action_left_scroll(0, "", false);

   return 0;
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static int action_left_shader_scale_pass(unsigned type, const char *label,
      bool wraparound)
{
   unsigned current_scale, delta;
   unsigned pass                         = type -
      MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[pass] : NULL;

   if (!shader_pass)
      return -1;

   /* A 20x scale is used to support scaling handheld border shaders up to 8K resolutions */
   current_scale              = shader_pass->fbo.scale_x;
   delta                      = 20;
   current_scale              = (current_scale + delta) % 21;

   if (current_scale)
      shader_pass->fbo.flags |=  FBO_SCALE_FLAG_VALID;
   else
      shader_pass->fbo.flags &= ~FBO_SCALE_FLAG_VALID;
   shader_pass->fbo.scale_x   = current_scale;
   shader_pass->fbo.scale_y   = current_scale;

   shader->flags             |= SHDR_FLAG_MODIFIED;

   return 0;
}

static int action_left_shader_filter_pass(unsigned type, const char *label,
      bool wraparound)
{
   unsigned delta                        = 2;
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[pass] : NULL;

   if (!shader_pass)
      return -1;

   shader_pass->filter                   = ((shader_pass->filter + delta) % 3);
   shader->flags                        |= SHDR_FLAG_MODIFIED;

   return 0;
}

static int action_left_shader_filter_default(unsigned type, const char *label,
      bool wraparound)
{
   rarch_setting_t *setting = menu_setting_find_enum(
         MENU_ENUM_LABEL_VIDEO_SMOOTH);
   if (!setting)
      return -1;
   return menu_action_handle_setting(setting,
         setting->type, MENU_ACTION_LEFT, wraparound);
}
#endif

#ifdef HAVE_CHEATS
static int action_left_cheat_num_passes(unsigned type, const char *label,
      bool wraparound)
{
   unsigned new_size          = 0;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned cheat_size        = cheat_manager_get_size();

   if (cheat_size)
      new_size         = cheat_size - 1;
   menu_st->flags     |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                       | MENU_ST_FLAG_PREVENT_POPULATE;
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_EMU);

   return 0;
}
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static int action_left_shader_num_passes(unsigned type, const char *label,
      bool wraparound)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   struct video_shader *shader = menu_shader_get();
   unsigned pass_count         = shader ? shader->passes : 0;

   if (!shader)
      return -1;

   if (pass_count > 0)
      shader->passes--;

   menu_st->flags     |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                       | MENU_ST_FLAG_PREVENT_POPULATE;
   video_shader_resolve_parameters(shader);

   shader->flags                        |= SHDR_FLAG_MODIFIED;

   return 0;
}
#endif

static int action_left_video_resolution(unsigned type, const char *label,
      bool wraparound)
{
   video_driver_get_prev_video_out();
   return 0;
}

static int playlist_association_left(unsigned type, const char *label,
      bool wraparound)
{
   char core_filename[PATH_MAX_LENGTH];
   size_t i, current                = 0;
   size_t next                      = 0;
   playlist_t *playlist             = playlist_get_cached();
   const char *default_core_path    = playlist_get_default_core_path(playlist);
   bool default_core_set            = false;
   core_info_list_t *core_info_list = NULL;
   core_info_t *core_info           = NULL;

   core_filename[0] = '\0';

   if (!playlist)
      return -1;

   core_info_get_list(&core_info_list);
   if (!core_info_list)
      return -1;

   /* Get current core path association */
   if (!string_is_empty(default_core_path) &&
       !string_is_equal(default_core_path, "DETECT"))
   {
      const char *default_core_filename = path_basename(default_core_path);
      if (!string_is_empty(default_core_filename))
      {
         strlcpy(core_filename, default_core_filename, sizeof(core_filename));
         default_core_set = true;
      }
   }

   /* Sort cores alphabetically */
   core_info_qsort(core_info_list, CORE_INFO_LIST_SORT_DISPLAY_NAME);

   /* If a core is currently associated... */
   if (default_core_set)
   {
      /* ...get its index */
      for (i = 0; i < core_info_list->count; i++)
      {
         core_info = NULL;
         core_info = core_info_get(core_info_list, i);
         if (!core_info)
            continue;
         if (string_starts_with(core_filename, core_info->core_file_id.str))
            current = i;
      }

      /* ...then decrement it */
      if (current == 0)
         /* Unset core association (DETECT) */
         default_core_set = false;
      else
         next = current - 1;
   }
   /* If a core is *not* currently associated and
    * wraparound is enabled, select last core in
    * the list */
   else if (wraparound && (core_info_list->count > 1))
   {
      next             = core_info_list->count - 1;
      default_core_set = true;
   }

   /* If a core is now associated, get new core info */
   core_info = NULL;
   if (default_core_set)
      core_info = core_info_get(core_info_list, next);

   /* Update playlist */
   playlist_set_default_core_path(playlist, core_info ? core_info->path         : "DETECT");
   playlist_set_default_core_name(playlist, core_info ? core_info->display_name : "DETECT");
   playlist_write_file(playlist);

   return 0;
}

static int playlist_label_display_mode_left(unsigned type, const char *label,
      bool wraparound)
{
   enum playlist_label_display_mode label_display_mode;
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   label_display_mode = playlist_get_label_display_mode(playlist);

   if (label_display_mode != LABEL_DISPLAY_MODE_DEFAULT)
      label_display_mode = (enum playlist_label_display_mode)((int)label_display_mode - 1);
   else if (wraparound)
      label_display_mode = LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX;

   playlist_set_label_display_mode(playlist, label_display_mode);
   playlist_write_file(playlist);

   return 0;
}

static void playlist_thumbnail_mode_left(playlist_t *playlist, enum playlist_thumbnail_id thumbnail_id,
      bool wraparound)
{
   enum playlist_thumbnail_mode thumbnail_mode =
         playlist_get_thumbnail_mode(playlist, thumbnail_id);

   if (thumbnail_mode > PLAYLIST_THUMBNAIL_MODE_DEFAULT)
      thumbnail_mode = (enum playlist_thumbnail_mode)((unsigned)thumbnail_mode - 1);
   else if (wraparound)
      thumbnail_mode = PLAYLIST_THUMBNAIL_MODE_BOXARTS;

   playlist_set_thumbnail_mode(playlist, thumbnail_id, thumbnail_mode);
   playlist_write_file(playlist);
}

static int playlist_right_thumbnail_mode_left(unsigned type, const char *label,
      bool wraparound)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   playlist_thumbnail_mode_left(playlist, PLAYLIST_THUMBNAIL_RIGHT, wraparound);

   return 0;
}

static int playlist_left_thumbnail_mode_left(unsigned type, const char *label,
      bool wraparound)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   playlist_thumbnail_mode_left(playlist, PLAYLIST_THUMBNAIL_LEFT, wraparound);

   return 0;
}

static int playlist_sort_mode_left(unsigned type, const char *label,
      bool wraparound)
{
   enum playlist_sort_mode sort_mode;
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   sort_mode = playlist_get_sort_mode(playlist);

   if (sort_mode > PLAYLIST_SORT_MODE_DEFAULT)
      sort_mode = (enum playlist_sort_mode)((int)sort_mode - 1);
   else if (wraparound)
      sort_mode = PLAYLIST_SORT_MODE_OFF;

   playlist_set_sort_mode(playlist, sort_mode);
   playlist_write_file(playlist);

   return 0;
}

static int manual_content_scan_system_name_left(
      unsigned type, const char *label,
      bool wraparound)
{
   settings_t *settings              = config_get_ptr();
   bool show_hidden_files            = settings->bools.show_hidden_files;
#ifdef HAVE_LIBRETRODB
   const char *path_content_database = settings->paths.path_content_database;
   struct string_list *system_name_list                            =
      manual_content_scan_get_menu_system_name_list(path_content_database, show_hidden_files);
#else
   struct string_list *system_name_list                            =
      manual_content_scan_get_menu_system_name_list(NULL, show_hidden_files);
#endif
   const char *current_system_name                                 = NULL;
   enum manual_content_scan_system_name_type next_system_name_type =
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE;
   const char *next_system_name                                    = NULL;
   unsigned current_index                                          = 0;
   unsigned next_index                                             = 0;
   unsigned i;

   if (!system_name_list)
      return -1;

   /* Get currently selected system name */
   if (manual_content_scan_get_menu_system_name(&current_system_name))
   {
      /* Get index of currently selected system name */
      for (i = 0; i < system_name_list->size; i++)
      {
         const char *system_name = system_name_list->elems[i].data;

         if (string_is_equal(current_system_name, system_name))
         {
            current_index = i;
            break;
         }
      }

      /* Decrement index */
      if (current_index > 0)
         next_index = current_index - 1;
      else if (wraparound && (system_name_list->size > 1))
         next_index = (unsigned)(system_name_list->size - 1);
   }

   /* Get new system name parameters */
   if (next_index == (unsigned)MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR)
      next_system_name_type = MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR;
   else if (next_index == (unsigned)MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM)
      next_system_name_type = MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM;

   next_system_name = system_name_list->elems[next_index].data;

   /* Set system name */
   manual_content_scan_set_menu_system_name(
         next_system_name_type, next_system_name);

   /* Clean up */
   string_list_free(system_name_list);

   return 0;
}

static int manual_content_scan_core_name_left(unsigned type, const char *label,
      bool wraparound)
{
   struct string_list *core_name_list                =
         manual_content_scan_get_menu_core_name_list();
   const char *current_core_name                     = NULL;
   enum manual_content_scan_core_type next_core_type =
         MANUAL_CONTENT_SCAN_CORE_SET;
   const char *next_core_name                        = NULL;
   unsigned current_index                            = 0;
   unsigned next_index                               = 0;
   unsigned i;

   if (!core_name_list)
      return -1;

   /* Get currently selected core name */
   if (manual_content_scan_get_menu_core_name(&current_core_name))
   {
      /* Get index of currently selected core name */
      for (i = 0; i < core_name_list->size; i++)
      {
         const char *core_name = core_name_list->elems[i].data;

         if (string_is_equal(current_core_name, core_name))
         {
            current_index = i;
            break;
         }
      }

      /* Decrement index */
      if (current_index > 0)
         next_index = current_index - 1;
      else if (wraparound && (core_name_list->size > 1))
         next_index = (unsigned)(core_name_list->size - 1);
   }

   /* Get new core name parameters */
   if (next_index == (unsigned)MANUAL_CONTENT_SCAN_CORE_DETECT)
      next_core_type = MANUAL_CONTENT_SCAN_CORE_DETECT;

   next_core_name = core_name_list->elems[next_index].data;

   /* Set core name */
   manual_content_scan_set_menu_core_name(
         next_core_type, next_core_name);

   /* Clean up */
   string_list_free(core_name_list);

   return 0;
}

#ifdef HAVE_LAKKA
static int cpu_policy_mode_change(unsigned type, const char *label,
      bool wraparound)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   enum cpu_scaling_mode mode = get_cpu_scaling_mode(NULL);
   if (mode != CPUSCALING_MANAGED_PERFORMANCE)
      mode--;
   set_cpu_scaling_mode(mode, NULL);
   menu_st->flags |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   return 0;
}

static int cpu_policy_freq_managed_tweak(unsigned type, const char *label,
      bool wraparound)
{
   bool refresh = false;
   cpu_scaling_opts_t opts;
   enum cpu_scaling_mode mode = get_cpu_scaling_mode(&opts);

   switch (type) {
   case MENU_SETTINGS_CPU_MANAGED_SET_MINFREQ:
      opts.min_freq = get_cpu_scaling_next_frequency_limit(
         opts.min_freq, -1);
      set_cpu_scaling_mode(mode, &opts);
      break;
   case MENU_SETTINGS_CPU_MANAGED_SET_MAXFREQ:
      opts.max_freq = get_cpu_scaling_next_frequency_limit(
         opts.max_freq, -1);
      set_cpu_scaling_mode(mode, &opts);
      break;
   };

   return 0;
}

static int cpu_policy_freq_managed_gov(unsigned type, const char *label,
      bool wraparound)
{
   int pidx;
   bool refresh = false;
   cpu_scaling_opts_t opts;
   enum cpu_scaling_mode mode = get_cpu_scaling_mode(&opts);
   cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(false);

   /* Using drivers[0] governors, should be improved */
   if (!drivers || !drivers[0])
      return -1;

   switch (atoi(label)) {
   case 0:
      pidx = string_list_find_elem(drivers[0]->available_governors,
         opts.main_policy);
      if (pidx > 1)
      {
         strlcpy(opts.main_policy,
            drivers[0]->available_governors->elems[pidx-2].data,
            sizeof(opts.main_policy));
         set_cpu_scaling_mode(mode, &opts);
      }
      break;
   case 1:
      pidx = string_list_find_elem(drivers[0]->available_governors,
         opts.menu_policy);
      if (pidx > 1)
      {
         strlcpy(opts.menu_policy,
            drivers[0]->available_governors->elems[pidx-2].data,
            sizeof(opts.menu_policy));
         set_cpu_scaling_mode(mode, &opts);
      }
      break;
   };

   return 0;
}

static int cpu_policy_freq_tweak(unsigned type, const char *label,
      bool wraparound)
{
   bool refresh = false;
   cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(false);
   unsigned policyid = atoi(label);
   uint32_t next_freq;
   if (!drivers)
     return 0;

   switch (type) {
   case MENU_SETTINGS_CPU_POLICY_SET_MINFREQ:
      next_freq = get_cpu_scaling_next_frequency(drivers[policyid],
         drivers[policyid]->min_policy_freq, -1);
      set_cpu_scaling_min_frequency(drivers[policyid], next_freq);
      break;
   case MENU_SETTINGS_CPU_POLICY_SET_MAXFREQ:
      next_freq = get_cpu_scaling_next_frequency(drivers[policyid],
         drivers[policyid]->max_policy_freq, -1);
      set_cpu_scaling_max_frequency(drivers[policyid], next_freq);
      break;
   case MENU_SETTINGS_CPU_POLICY_SET_GOVERNOR:
   {
      int pidx = string_list_find_elem(drivers[policyid]->available_governors,
         drivers[policyid]->scaling_governor);
      if (pidx > 1)
      {
         set_cpu_scaling_governor(drivers[policyid],
            drivers[policyid]->available_governors->elems[pidx-2].data);
      }
      break;
   }
   };

   return 0;
}
#endif

static int core_setting_left(unsigned type, const char *label,
      bool wraparound)
{
   unsigned idx     = type - MENU_SETTINGS_CORE_OPTION_START;

   retroarch_ctl(RARCH_CTL_CORE_OPTION_PREV, &idx);

   return 0;
}

static int action_left_core_lock(unsigned type, const char *label,
      bool wraparound)
{
   return action_ok_core_lock(label, label, type, 0, 0);
}

static int action_left_core_set_standalone_exempt(unsigned type, const char *label,
      bool wraparound)
{
   return action_ok_core_set_standalone_exempt(label, label, type, 0, 0);
}

static int disk_options_disk_idx_left(unsigned type, const char *label,
      bool wraparound)
{
   /* Note: Menu itself provides visual feedback - no
    * need to print info message to screen */
   bool print_log = false;

   command_event(CMD_EVENT_DISK_PREV, &print_log);

   return 0;
}

static int action_left_video_gpu_index(unsigned type, const char *label,
      bool wraparound)
{
   enum gfx_ctx_api api = video_context_driver_get_api();

   switch (api)
   {
#ifdef HAVE_VULKAN
      case GFX_CTX_VULKAN_API:
      {
         struct string_list *list = video_driver_get_gpu_api_devices(api);

         if (list)
         {
            settings_t *settings = config_get_ptr();
            int vulkan_gpu_index = settings->ints.vulkan_gpu_index;

            if (vulkan_gpu_index > 0)
            {
               configuration_set_int(settings,
                     settings->ints.vulkan_gpu_index,
                     vulkan_gpu_index - 1);
            }
            else
            {
               configuration_set_int(settings,
                     settings->ints.vulkan_gpu_index,
                     (int)list->size - 1);
            }
         }

         break;
      }
#endif
#ifdef HAVE_D3D10
      case GFX_CTX_DIRECT3D10_API:
      {
         struct string_list *list = video_driver_get_gpu_api_devices(api);

         if (list)
         {
            settings_t *settings = config_get_ptr();
            int d3d10_gpu_index  = settings->ints.d3d10_gpu_index;
            if (d3d10_gpu_index > 0)
            {
               configuration_set_int(settings,
                     settings->ints.d3d10_gpu_index,
                     settings->ints.d3d10_gpu_index - 1);
            }
            else
            {
               configuration_set_int(settings,
                     settings->ints.d3d10_gpu_index,
                     list->size - 1);
            }
         }

         break;
      }
#endif
#ifdef HAVE_D3D11
      case GFX_CTX_DIRECT3D11_API:
      {
         struct string_list *list = video_driver_get_gpu_api_devices(api);

         if (list)
         {
            settings_t *settings = config_get_ptr();
            int d3d11_gpu_index  = settings->ints.d3d11_gpu_index;
            if (d3d11_gpu_index > 0)
            {
               configuration_set_int(settings,
                     settings->ints.d3d11_gpu_index,
                     settings->ints.d3d11_gpu_index - 1);
            }
            else
            {
               configuration_set_int(settings,
                     settings->ints.d3d11_gpu_index,
                     list->size - 1);
            }
         }

         break;
      }
#endif
#ifdef HAVE_D3D12
      case GFX_CTX_DIRECT3D12_API:
      {
         struct string_list *list = video_driver_get_gpu_api_devices(api);

         if (list)
         {
            settings_t *settings = config_get_ptr();
            int d3d12_gpu_index  = settings->ints.d3d12_gpu_index;
            if (d3d12_gpu_index > 0)
            {
               configuration_set_int(settings,
                     settings->ints.d3d12_gpu_index,
                     settings->ints.d3d12_gpu_index - 1);
            }
            else
            {
               configuration_set_int(settings,
                     settings->ints.d3d12_gpu_index,
                     list->size - 1);
            }
         }

         break;
      }
#endif
      default:
         break;
   }

   return 0;
}

static int action_left_state_slot(unsigned type, const char *label,
      bool wraparound)
{
   struct menu_state *menu_st     = menu_state_get_ptr();
   settings_t           *settings = config_get_ptr();

   settings->ints.state_slot--;
   if (settings->ints.state_slot < -1)
      settings->ints.state_slot = 999;

   if (menu_st->driver_ctx)
   {
      size_t selection            = menu_st->selection_ptr;
      if (menu_st->driver_ctx->update_savestate_thumbnail_path)
         menu_st->driver_ctx->update_savestate_thumbnail_path(
               menu_st->userdata, (unsigned)selection);
      if (menu_st->driver_ctx->update_savestate_thumbnail_image)
         menu_st->driver_ctx->update_savestate_thumbnail_image(menu_st->userdata);
   }

   return 0;
}
static int action_left_replay_slot(unsigned type, const char *label,
      bool wraparound)
{
   struct menu_state *menu_st     = menu_state_get_ptr();
   settings_t           *settings = config_get_ptr();

   settings->ints.replay_slot--;
   if (settings->ints.replay_slot < -1)
      settings->ints.replay_slot = 999;

   if (menu_st->driver_ctx)
   {
      size_t selection            = menu_st->selection_ptr;
      if (menu_st->driver_ctx->update_savestate_thumbnail_path)
         menu_st->driver_ctx->update_savestate_thumbnail_path(
               menu_st->userdata, (unsigned)selection);
      if (menu_st->driver_ctx->update_savestate_thumbnail_image)
         menu_st->driver_ctx->update_savestate_thumbnail_image(menu_st->userdata);
   }

   return 0;
}

static int bind_left_generic(unsigned type, const char *label,
      bool wraparound)
{
   return menu_setting_set(type, MENU_ACTION_LEFT, wraparound);
}

static int menu_cbs_init_bind_left_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, size_t lbl_len, const char *menu_label, size_t menu_lbl_len)
{

   if (     string_starts_with_size(label, "input_player", STRLEN_CONST("input_player"))
         && string_ends_with_size(label, "_joypad_index", lbl_len,
            STRLEN_CONST("_joypad_index")))
   {
      unsigned i;
      char lbl_setting[128];
      size_t _len = strlcpy(lbl_setting, "input_player", sizeof(lbl_setting));
      for (i = 0; i < MAX_USERS; i++)
      {
         snprintf(lbl_setting + _len, sizeof(lbl_setting) - _len, "%d_joypad_index", i + 1);

         if (!string_is_equal(label, lbl_setting))
            continue;

         BIND_ACTION_LEFT(cbs, bind_left_generic);
         return 0;
      }
   }

   if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
   {
      BIND_ACTION_LEFT(cbs, action_left_mainmenu);
      return 0;
   }

   if (strstr(label, "rdb_entry") || string_starts_with_size(label, "content_info", STRLEN_CONST("content_info")))
   {
      BIND_ACTION_LEFT(cbs, action_left_scroll);
   }
   else
   {
      if (cbs->enum_idx != MSG_UNKNOWN)
      {
         switch (cbs->enum_idx)
         {
            case MENU_ENUM_LABEL_SUBSYSTEM_ADD:
            case MENU_ENUM_LABEL_SUBSYSTEM_LOAD:
            case MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM:
            case MENU_ENUM_LABEL_EXPLORE_ITEM:
            case MENU_ENUM_LABEL_CONTENTLESS_CORE:
            case MENU_ENUM_LABEL_NO_SETTINGS_FOUND:
               BIND_ACTION_LEFT(cbs, action_left_mainmenu);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
               BIND_ACTION_LEFT(cbs, action_left_shader_scale_pass);
#endif
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
               BIND_ACTION_LEFT(cbs, action_left_shader_filter_pass);
#endif
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
               BIND_ACTION_LEFT(cbs, action_left_shader_filter_default);
#endif
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
               BIND_ACTION_LEFT(cbs, action_left_shader_num_passes);
#endif
               break;
            case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
#ifdef HAVE_CHEATS
               BIND_ACTION_LEFT(cbs, action_left_cheat_num_passes);
#endif
               break;
            case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
               BIND_ACTION_LEFT(cbs, action_left_video_resolution);
               break;
            case MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE:
            case MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE:
               BIND_ACTION_LEFT(cbs, action_left_scroll);
               break;
            case MENU_ENUM_LABEL_NO_ITEMS:
            case MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE:
            case MENU_ENUM_LABEL_NO_CORES_AVAILABLE:
            case MENU_ENUM_LABEL_EXPLORE_INITIALISING_LIST:
               if (
                        string_ends_with_size(menu_label, "_tab",
                           menu_lbl_len,
                           STRLEN_CONST("_tab")
                           )
                     || string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))
                     || string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU))
                  )
               {
                  BIND_ACTION_LEFT(cbs, action_left_mainmenu);
               }
               else
               {
                  BIND_ACTION_LEFT(cbs, action_left_scroll);
               }
               break;
            case MENU_ENUM_LABEL_VIDEO_GPU_INDEX:
               BIND_ACTION_LEFT(cbs, action_left_video_gpu_index);
               break;
            case MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE:
               BIND_ACTION_LEFT(cbs, playlist_association_left);
               break;
            case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE:
               BIND_ACTION_LEFT(cbs, playlist_label_display_mode_left);
               break;
            case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE:
               BIND_ACTION_LEFT(cbs, playlist_right_thumbnail_mode_left);
               break;
            case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE:
               BIND_ACTION_LEFT(cbs, playlist_left_thumbnail_mode_left);
               break;
            case MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE:
               BIND_ACTION_LEFT(cbs, playlist_sort_mode_left);
               break;
            case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
               BIND_ACTION_LEFT(cbs, manual_content_scan_system_name_left);
               break;
            case MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME:
               BIND_ACTION_LEFT(cbs, manual_content_scan_core_name_left);
               break;
            #ifdef HAVE_LAKKA
            case MENU_ENUM_LABEL_CPU_PERF_MODE:
               BIND_ACTION_LEFT(cbs, cpu_policy_mode_change);
               break;
            case MENU_ENUM_LABEL_CPU_POLICY_MAX_FREQ:
            case MENU_ENUM_LABEL_CPU_POLICY_MIN_FREQ:
            case MENU_ENUM_LABEL_CPU_POLICY_GOVERNOR:
               BIND_ACTION_LEFT(cbs, cpu_policy_freq_tweak);
               break;
            case MENU_ENUM_LABEL_CPU_MANAGED_MIN_FREQ:
            case MENU_ENUM_LABEL_CPU_MANAGED_MAX_FREQ:
               BIND_ACTION_LEFT(cbs, cpu_policy_freq_managed_tweak);
               break;
            case MENU_ENUM_LABEL_CPU_POLICY_CORE_GOVERNOR:
            case MENU_ENUM_LABEL_CPU_POLICY_MENU_GOVERNOR:
               BIND_ACTION_LEFT(cbs, cpu_policy_freq_managed_gov);
               break;
            #endif
            default:
               return -1;
         }
      }
      else
      {
         return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_left_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type, const char *menu_label, size_t menu_lbl_len)
{
#ifdef HAVE_CHEATS
   if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_LEFT(cbs, action_left_cheat);
   } else
#endif
#ifdef HAVE_AUDIOMIXER
   if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END)
   {
      BIND_ACTION_LEFT(cbs, audio_mixer_stream_volume_left);
   } else
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_LEFT(cbs, shader_action_parameter_left);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_LEFT(cbs, shader_action_preset_parameter_left);
   } else
#endif
   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_LEFT(cbs, action_left_input_desc);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
      && type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
   {
      BIND_ACTION_LEFT(cbs, action_left_input_desc_kbd);
   }
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START) &&
            (type < MENU_SETTINGS_CHEEVOS_START))
   {
      BIND_ACTION_LEFT(cbs, core_setting_left);
   }
   else
   {
      switch (type)
      {
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            BIND_ACTION_LEFT(cbs, disk_options_disk_idx_left);
            break;
         case FILE_TYPE_PLAIN:
         case FILE_TYPE_DIRECTORY:
         case FILE_TYPE_CARCHIVE:
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_CORE:
         case FILE_TYPE_RDB:
         case FILE_TYPE_RDB_ENTRY:
         case FILE_TYPE_RPL_ENTRY:
         case FILE_TYPE_SHADER:
         case FILE_TYPE_SHADER_PRESET:
         case FILE_TYPE_IMAGE:
         case FILE_TYPE_OVERLAY:
         case FILE_TYPE_OSK_OVERLAY:
         case FILE_TYPE_VIDEOFILTER:
         case FILE_TYPE_AUDIOFILTER:
         case FILE_TYPE_CONFIG:
         case FILE_TYPE_USE_DIRECTORY:
         case FILE_TYPE_PLAYLIST_ENTRY:
         case MENU_INFO_MESSAGE:
         case FILE_TYPE_DOWNLOAD_CORE:
         case FILE_TYPE_CHEAT:
         case FILE_TYPE_REMAP:
         case FILE_TYPE_MOVIE:
         case FILE_TYPE_MUSIC:
         case FILE_TYPE_IMAGEVIEWER:
         case FILE_TYPE_PLAYLIST_COLLECTION:
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
         case FILE_TYPE_DOWNLOAD_CORE_SYSTEM_FILES:
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
         case FILE_TYPE_DOWNLOAD_URL:
         case FILE_TYPE_SCAN_DIRECTORY:
         case FILE_TYPE_MANUAL_SCAN_DIRECTORY:
         case FILE_TYPE_FONT:
         case FILE_TYPE_VIDEO_FONT:
         case MENU_SETTING_GROUP:
         case MENU_SETTINGS_CORE_INFO_NONE:
            if (  
                  string_ends_with_size(menu_label, "_tab",
                     menu_lbl_len, STRLEN_CONST("_tab"))
                  || string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU))
               )
            {
               BIND_ACTION_LEFT(cbs, action_left_mainmenu);
               break;
            }
         case MENU_SETTING_ACTION_RUN:
         case MENU_SETTING_ACTION_CLOSE:
         case MENU_SETTING_ACTION_CLOSE_HORIZONTAL:
         case MENU_SETTING_ACTION_DELETE_ENTRY:
         case MENU_SETTING_ACTION_CORE_OPTIONS:
         case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         case MENU_SETTING_ACTION_SCREENSHOT:
         case MENU_SETTING_ACTION_FAVORITES_DIR:
         case MENU_SETTING_ACTION_CORE_MANAGER_OPTIONS:
         case MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION:
         case MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION_KBD:
            BIND_ACTION_LEFT(cbs, action_left_scroll);
            break;
         case MENU_SETTING_ACTION:
         case FILE_TYPE_CONTENTLIST_ENTRY:
            BIND_ACTION_LEFT(cbs, action_left_mainmenu);
            break;
         case MENU_SETTING_ACTION_CORE_LOCK:
            BIND_ACTION_LEFT(cbs, action_left_core_lock);
            break;
         case MENU_SETTING_ACTION_CORE_SET_STANDALONE_EXEMPT:
            BIND_ACTION_LEFT(cbs, action_left_core_set_standalone_exempt);
            break;
         case MENU_SETTING_ACTION_SAVESTATE:
         case MENU_SETTING_ACTION_LOADSTATE:
            BIND_ACTION_LEFT(cbs, action_left_state_slot);
            break;
         case MENU_SETTING_ACTION_RECORDREPLAY:
         case MENU_SETTING_ACTION_PLAYREPLAY:
         case MENU_SETTING_ACTION_HALTREPLAY:
            BIND_ACTION_LEFT(cbs, action_left_replay_slot);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_left(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, size_t lbl_len,
      unsigned type, size_t idx,
      const char *menu_label, size_t menu_lbl_len)
{
   if (!cbs)
      return -1;

   BIND_ACTION_LEFT(cbs, bind_left_generic);

   if (type == MENU_SETTING_NO_ITEM)
   {
      if (  
               string_ends_with_size(menu_label, "_tab",
                  menu_lbl_len, STRLEN_CONST("_tab"))
            || string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))
            || string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU))
         )
      {
            BIND_ACTION_LEFT(cbs, action_left_mainmenu);
            return 0;
      }
   }

   if (cbs->setting)
   {
      const char *parent_group   = cbs->setting->parent_group;

      if (string_is_equal(parent_group,
               msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))
               && (cbs->setting->type == ST_GROUP))
      {
         BIND_ACTION_LEFT(cbs, action_left_mainmenu);
         return 0;
      }
   }

   if (menu_cbs_init_bind_left_compare_label(cbs, label, lbl_len, menu_label, menu_lbl_len) == 0)
      return 0;

   if (menu_cbs_init_bind_left_compare_type(cbs, type, menu_label, menu_lbl_len) == 0)
      return 0;

   return -1;
}
