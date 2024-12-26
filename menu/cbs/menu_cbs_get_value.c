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
#include "../../gfx/gfx_animation.h"
#include "../menu_cbs.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../menu_shader.h"
#endif

#include "../../tasks/tasks_internal.h"

#include "../../core.h"
#include "../../core_info.h"
#include "../../configuration.h"
#include "../../file_path_special.h"
#include "../../core_option_manager.h"
#ifdef HAVE_CHEATS
#include "../../cheat_manager.h"
#endif
#include "../../performance_counters.h"
#include "../../paths.h"
#include "../../verbosity.h"
#ifdef HAVE_BLUETOOTH
#include "../../bluetooth/bluetooth_driver.h"
#endif
#include "../../playlist.h"
#include "../../manual_content_scan.h"
#include "../misc/cpufreq/cpufreq.h"
#include "../../audio/audio_driver.h"

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#include "../../network/wifi_driver.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../../cheevos/cheevos_menu.h"
#endif

#ifdef HAVE_MIST
#include "../../steam/steam.h"
#endif

#ifndef BIND_ACTION_GET_VALUE
#define BIND_ACTION_GET_VALUE(cbs, name) (cbs)->action_get_value = (name)
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
   unsigned offset      = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN);
   *w                   = 19;

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
   size_t _len;
   unsigned offset = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN);
   *w              = 19;
   strlcpy(s2, path, len2);

   if (offset >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   _len = snprintf(s, len, "%.2f", audio_driver_mixer_get_stream_volume(offset));
   strlcpy(s + _len, " dB", len - _len);
}
#endif

#ifdef HAVE_CHEATS
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
#endif

#ifdef HAVE_CHEEVOS
static void menu_action_setting_disp_set_label_cheevos_entry(
   file_list_t* list,
   unsigned *w, unsigned type, unsigned i,
   const char *label,
   char *s, size_t len,
   const char *path,
   char *s2, size_t len2)
{
   *w = 19;
   strlcpy(s2, path, len2);

   rcheevos_menu_get_state(type - MENU_SETTINGS_CHEEVOS_START, s, len);
}
#endif

static void menu_action_setting_disp_set_label_remap_file_info(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *remap_path      = runloop_st->name.remapfile;
   const char *remap_file      = NULL;

   *w = 19;

   if (!string_is_empty(remap_path))
      remap_file = path_basename_nocompression(remap_path);

   if (!string_is_empty(remap_file))
      strlcpy(s, remap_file, len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_override_file_info(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *override_path   = path_get(RARCH_PATH_CONFIG_OVERRIDE);
   const char *override_file   = NULL;

   *w = 19;

   if (!string_is_empty(override_path))
      override_file = path_basename_nocompression(override_path);

   if (!string_is_empty(override_file))
      strlcpy(s, override_file, len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);

   strlcpy(s2, path, len2);
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

   if (shader_pass)
   {
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
}

static void menu_action_setting_disp_set_label_shader_watch_for_changes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      list->list[i].actiondata;
   bool val                  = *cbs->setting->value.target.boolean;

   *w = 19;
   strlcpy(s2, path, len2);

   if (val)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FALSE), len);
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
   *w = 19;
   strlcpy(s2, path, len2);
   if (shader_pass && !string_is_empty(shader_pass->source.path))
      fill_pathname_base(s, shader_pass->source.path, len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
}

static void menu_action_setting_disp_set_label_shader_default_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      list->list[i].actiondata;
   bool val                  = *cbs->setting->value.target.boolean;
   *w = 19;
   if (val)
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
   *w = 19;
   strlcpy(s2, path, len2);
   video_shader_driver_get_current_shader(&shader_info);
   if (shader_info.data && (param = &shader_info.data->parameters[type - offset]))
      snprintf(s, len, "%.2f [%.2f %.2f]",
            param->current, param->minimum, param->maximum);
   else
      *s = '\0';
}

static void menu_action_setting_disp_set_label_shader_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_disp_set_label_shader_parameter_internal(
         list, w, type, i,
         label, s, len, path, s2, len2,
         MENU_SETTINGS_SHADER_PARAMETER_0);
}

static void menu_action_setting_disp_set_label_shader_preset_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_disp_set_label_shader_parameter_internal(
         list, w, type, i,
         label, s, len, path, s2, len2,
         MENU_SETTINGS_SHADER_PRESET_PARAMETER_0);
}

static void menu_action_setting_disp_set_label_shader_scale_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned scale_value                  = 0;
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[type - MENU_SETTINGS_SHADER_PASS_SCALE_0] : NULL;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);
   if (!shader_pass)
      return;
   if (!(scale_value = shader_pass->fbo.scale_x))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE), len);
   else
      snprintf(s, len, "%ux", scale_value);
}
#endif


#ifdef HAVE_NETWORKING
static void menu_action_setting_disp_set_label_netplay_mitm_server(
      file_list_t *list, unsigned *w, unsigned type, unsigned i,
      const char *label, char *s, size_t len,
      const char *path, char *path_buf, size_t path_buf_size)
{
   size_t j;
   const char *netplay_mitm_server;
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;

   *w = 19;
   *s = '\0';
   strlcpy(path_buf, path, path_buf_size);

   if (!cbs || !cbs->setting)
      return;

   netplay_mitm_server = cbs->setting->value.target.string;
   if (string_is_empty(netplay_mitm_server))
      return;

   for (j = 0; j < ARRAY_SIZE(netplay_mitm_server_list); j++)
   {
      const mitm_server_t *server = &netplay_mitm_server_list[j];

      if (string_is_equal(server->name, netplay_mitm_server))
      {
         strlcpy(s, msg_hash_to_str(server->description), len);
         break;
      }
   }
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
   const char *alt = list->list[i].alt
      ? list->list[i].alt
      : list->list[i].path;
   s[0] = '(';
   s[1] = 'C';
   s[2] = 'O';
   s[3] = 'R';
   s[4] = 'E';
   s[5] = ')';
   s[6] = '\0';
   *w   = (unsigned)STRLEN_CONST("(CORE)");
   if (alt)
      strlcpy(s2, alt, len2);
}

#ifdef HAVE_NETWORKING
static void menu_action_setting_disp_set_label_core_updater_entry(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   core_updater_list_t *core_list         = core_updater_list_get_cached();
   const core_updater_list_entry_t *entry = NULL;
   const char *alt                        = list->list[i].alt
      ? list->list[i].alt
      : list->list[i].path;
   *s                                     = '\0';
   *w                                     = 0;

   if (alt)
      strlcpy(s2, alt, len2);

   /* Search for specified core */
   if (core_list &&
       core_updater_list_get_filename(core_list, path, &entry) &&
       !string_is_empty(entry->local_core_path))
   {
      core_info_t *core_info = NULL;

      /* Check whether core is installed
       * > Note: We search core_info here instead
       *   of calling path_is_valid() since we don't
       *   want to perform disk access every frame */
      if (core_info_find(entry->local_core_path, &core_info))
      {
         /* Highlight locked cores */
         if (core_info->is_locked)
         {
            s[0] = '[';
            s[1] = '#';
            s[2] = '!';
            s[3] = ']';
            s[4] = '\0';
            *w   = (unsigned)STRLEN_CONST("[#!]");
         }
         else
         {
            s[0] = '[';
            s[1] = '#';
            s[2] = ']';
            s[3] = '\0';
            *w   = (unsigned)STRLEN_CONST("[#]");
         }
      }
   }
}
#endif

static void menu_action_setting_disp_set_label_core_manager_entry(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   core_info_t *core_info = NULL;
   const char *alt        = list->list[i].alt
         ? list->list[i].alt
         : list->list[i].path;

   if (alt)
      strlcpy(s2, alt, len2);

   /* Check whether core is locked
    * > Note: We search core_info here instead of
    *   calling core_info_get_core_lock() since we
    *   don't want to perform disk access every frame */
   if (   core_info_find(path, &core_info)
       && core_info->is_locked)
   {
      s[0] = '[';
      s[1] = '!';
      s[2] = ']';
      s[3] = '\0';
      *w   = (unsigned)STRLEN_CONST("[!]");
   }
   else
   {
      *s   = '\0';
      *w   = 0;
   }
}

#ifdef HAVE_MIST
static void menu_action_setting_disp_set_label_core_manager_steam_entry(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MistResult result;
   steam_core_dlc_list_t *core_dlc_list = NULL;
   steam_core_dlc_t *core_dlc           = NULL;
   bool dlc_installed                   = false;

   *s = '\0';
   *w = 0;

   if (MIST_IS_ERROR(steam_get_core_dlcs(&core_dlc_list, true))) return;

   strlcpy(s2, path, len2);

   if (!(core_dlc = steam_get_core_dlc_by_name(core_dlc_list, path)))
      return;

   result = mist_steam_apps_is_dlc_installed(core_dlc->app_id, &dlc_installed);

   if (MIST_IS_ERROR(result))
   {
      RARCH_ERR("[Steam]: Failed to get dlc install status (%d-%d)\n", MIST_UNPACK_RESULT(result));
      return;
   }

   if (dlc_installed)
   {
      s[0] = '[';
      s[1] = '#';
      s[2] = ']';
      s[3] = '\0';
      *w = (unsigned)STRLEN_CONST("[#]");
   }
}
#endif

static void menu_action_setting_disp_set_label_contentless_core(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *alt = list->list[i].alt
      ? list->list[i].alt
      : list->list[i].path;

   *s = '\0';
   *w = 0;

   if (alt)
      strlcpy(s2, alt, len2);
}

#ifdef HAVE_LAKKA
static void menu_action_setting_disp_cpu_gov_mode(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *alt        = list->list[i].alt
         ? list->list[i].alt
         : list->list[i].path;
   enum cpu_scaling_mode mode = get_cpu_scaling_mode(NULL);

   if (alt)
      strlcpy(s2, alt, len2);

   strlcpy(s, msg_hash_to_str(
      MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF + (int)mode), len);
}

static void menu_action_setting_disp_cpu_gov_choose(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   cpu_scaling_opts_t opts;
   const char *alt            = list->list[i].alt
         ? list->list[i].alt
         : list->list[i].path;
   int fnum                   = atoi(list->list[i].label);
   enum cpu_scaling_mode mode = get_cpu_scaling_mode(&opts);

   if (alt)
      strlcpy(s2, alt, len2);

   if (!fnum)
      strlcpy(s, opts.main_policy, len);
   else
      strlcpy(s, opts.menu_policy, len);
}

static void menu_action_setting_disp_set_label_cpu_policy(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned policyid              = atoi(path);
   cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(false);
   cpu_scaling_driver_t *d        = drivers[policyid];
   size_t _len                    = snprintf(s2, len2, "%s %d",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY),
         policyid);

   *s   = '\0';
   *w   = 0;

   if (d->affected_cpus)
   {
      _len += strlcpy(s2 + _len, " [CPU(s) ",      len2 - _len);
      _len += strlcpy(s2 + _len, d->affected_cpus, len2 - _len);
      s2[  _len] = ']' ;
      s2[++_len] = '\0';
   }
}

static void menu_action_cpu_managed_freq_label(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   cpu_scaling_opts_t opts;
   uint32_t freq              = 0;
   enum cpu_scaling_mode mode = get_cpu_scaling_mode(&opts);

   switch (type)
   {
      case MENU_SETTINGS_CPU_MANAGED_SET_MINFREQ:
         strlcpy(s2, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ), len2);
         freq = opts.min_freq;
         break;
      case MENU_SETTINGS_CPU_MANAGED_SET_MAXFREQ:
         strlcpy(s2, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ), len2);
         freq = opts.max_freq;
         break;
   };

   if (freq == 1)
      strlcpy(s, "Min.", len);
   else if (freq == ~0U)
      strlcpy(s, "Max.", len);
   else
      snprintf(s, len, "%u MHz", freq / 1000);
}

static void menu_action_cpu_freq_label(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned              policyid = atoi(path);
   cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(false);
   cpu_scaling_driver_t        *d = drivers[policyid];

   switch (type)
   {
      case MENU_SETTINGS_CPU_POLICY_SET_MINFREQ:
         strlcpy(s2, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ), len2);
         snprintf(s, len, "%u MHz", d->min_policy_freq / 1000);
         break;
      case MENU_SETTINGS_CPU_POLICY_SET_MAXFREQ:
         strlcpy(s2, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ), len2);
         snprintf(s, len, "%u MHz", d->max_policy_freq / 1000);
         break;
      case MENU_SETTINGS_CPU_POLICY_SET_GOVERNOR:
         strlcpy(s2, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR), len2);
         strlcpy(s, d->scaling_governor, len);
         break;
   };
}

static void menu_action_cpu_governor_label(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned policyid = atoi(path);
   cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(false);
   cpu_scaling_driver_t *d = drivers[policyid];

   strlcpy(s2, msg_hash_to_str(
      MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR), len2);
   strlcpy(s, d->scaling_governor, len);
}
#endif

static void menu_action_setting_disp_set_label_core_lock(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   core_info_t *core_info = NULL;
   const char *alt        = list->list[i].alt
         ? list->list[i].alt
         : list->list[i].path;

   if (alt)
      strlcpy(s2, alt, len2);

   /* Check whether core is locked
    * > Note: We search core_info here instead of
    *   calling core_info_get_core_lock() since we
    *   don't want to perform disk access every frame */
   if (   core_info_find(path, &core_info)
       && core_info->is_locked)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);

   *w  = (unsigned)strlen(s);
}

static void menu_action_setting_disp_set_label_core_set_standalone_exempt(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   core_info_t *core_info = NULL;
   const char *alt        = list->list[i].alt
         ? list->list[i].alt
         : list->list[i].path;

   if (alt)
      strlcpy(s2, alt, len2);

   /* Check whether core is excluded from the
    * contentless cores menu
    * > Note: We search core_info here instead of
    *   calling core_info_get_core_standalone_exempt()
    *   since we don't want to perform disk access
    *   every frame */
   if (   core_info_find(path, &core_info)
       && core_info->supports_no_game
       && core_info->is_standalone_exempt)
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);

   *w  = (unsigned)strlen(s);
}

static void menu_action_setting_disp_set_label_input_desc(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned remap_idx;
   settings_t *settings   = config_get_ptr();
   unsigned user_idx      = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) / (RARCH_FIRST_CUSTOM_BIND + 8);
   unsigned btn_idx       = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) - (RARCH_FIRST_CUSTOM_BIND + 8) * user_idx;

   if (!settings)
      return;

   *w = 19;
   strlcpy(s2, path, len2);

   if ((remap_idx   = settings->uints.input_remap_ids[user_idx][btn_idx]) !=
         RARCH_UNMAPPED)
   {
      unsigned mapped_port   = settings->uints.input_remap_ports[user_idx];
      const char *descriptor = runloop_state_get_ptr()->system.input_desc_btn[mapped_port][remap_idx];
      if (!string_is_empty(descriptor))
      {
         size_t _len = strlcpy(s, descriptor, len);
         if (remap_idx < RARCH_FIRST_CUSTOM_BIND) { }
         else if (remap_idx % 2 == 0)
         {
            s[  _len] = ' ';
            s[++_len] = '+';
            s[++_len] = '\0';
         }
         else
         {
            s[  _len] = ' ';
            s[++_len] = '-';
            s[++_len] = '\0';
         }
         return;
      }
   }

   /* If descriptor was not found, set this instead */
   s[0] = '-';
   s[1] = '-';
   s[2] = '-';
   s[3] = '\0';
}

static void menu_action_setting_disp_set_label_input_desc_kbd(
   file_list_t* list,
   unsigned *w, unsigned type, unsigned i,
   const char *label,
   char *s, size_t len,
   const char *path,
   char *s2, size_t len2)
{
   unsigned key_id, btn_idx;
   unsigned remap_id;
   unsigned user_idx;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   user_idx = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) / RARCH_ANALOG_BIND_LIST_END;
   btn_idx  = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) - RARCH_ANALOG_BIND_LIST_END * user_idx;
   remap_id = settings->uints.input_keymapper_ids[user_idx][btn_idx];

   for (key_id = 0; key_id < RARCH_MAX_KEYS - 1; key_id++)
   {
      if (remap_id == key_descriptors[key_id].key)
         break;
   }

   if (key_descriptors[key_id].key != RETROK_FIRST)
   {
      /* TODO/FIXME - Localize */
      size_t _len = strlcpy(s, "Keyboard ", len);
      strlcpy(s + _len, key_descriptors[key_id].desc, len - _len);
   }
   else
   {
      s[0] = '-';
      s[1] = '-';
      s[2] = '-';
      s[3] = '\0';
   }

   *w = 19;
   strlcpy(s2, path, len2);
}

#ifdef HAVE_CHEATS
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
      size_t _len = 
         snprintf(s, len, "(%s) : ",
                 cheat_manager_get_code_state(cheat_index)
               ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)
               : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF));

      if (cheat_manager_state.cheats[cheat_index].handler == CHEAT_HANDLER_TYPE_EMU)
      {
         const char *code = cheat_manager_get_code(cheat_index);
         strlcpy(s + _len, 
                 code 
               ? code
               : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               len - _len);
      }
      else
         snprintf(s + _len, len - _len, "%08X",
               cheat_manager_state.cheats[cheat_index].address);
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
   unsigned int address      = 0;
   unsigned int address_mask = 0;
   unsigned int prev_val     = 0;
   unsigned int curr_val     = 0;
   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_VIEW, cheat_manager_state.match_idx, &address, &address_mask, &prev_val, &curr_val);

   /* TODO/FIXME - localize */
   snprintf(s, len, "Prev: %u Curr: %u", prev_val, curr_val);
   *w = 19;
   strlcpy(s2, path, len2);
}
#endif

static void menu_action_setting_disp_set_label_perf_counters_common(
      struct retro_perf_counter **counters,
      unsigned offset, char *s, size_t len
      )
{
   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   /* TODO/FIXME - localize */
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
   gfx_animation_t *p_anim     = anim_get_ptr();
   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   menu_action_setting_disp_set_label_perf_counters_common(
         counters, offset, s, len);
   GFX_ANIMATION_CLEAR_ACTIVE(p_anim);
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

#ifdef HAVE_BLUETOOTH
static void menu_action_setting_disp_set_label_bluetooth_is_connected(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   strlcpy(s2, path, len2);
   *w = 19;

   if (driver_bluetooth_device_is_connected(i))
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BT_CONNECTED), len);
}
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_WIFI)
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
#endif

static void menu_action_setting_disp_set_label_menu_disk_index(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   unsigned images               = 0;
   unsigned current              = 0;
   rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;

   if (!sys_info)
      return;

   if (!disk_control_enabled(&sys_info->disk_control))
      return;

   *w = 19;
   *s = '\0';
   strlcpy(s2, path, len2);

   images  = disk_control_get_num_images(&sys_info->disk_control);
   current = disk_control_get_image_index(&sys_info->disk_control);

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
   char desc[64] = {0};
   *w = 19;
   *s = '\0';

   strlcpy(s2, path, len2);

   if (video_driver_get_video_output_size(&width, &height, desc, sizeof(desc)))
   {
#ifdef GEKKO
      if (width == 0 || height == 0)
         snprintf(s, len, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE));
      else
#endif
      {
         if (!string_is_empty(desc))
            snprintf(s, len, msg_hash_to_str(MSG_SCREEN_RESOLUTION_FORMAT_DESC), 
               width, height, desc);
         else
            snprintf(s, len, msg_hash_to_str(MSG_SCREEN_RESOLUTION_FORMAT_NO_DESC), 
               width, height);
      }
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
}

#define MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len, path, label, label_size, s2, len2) \
   *s = '\0'; \
   strlcpy(s, label, len); \
   *w = label_size; \
   strlcpy(s2, path, len2)

static void menu_action_setting_disp_set_label_menu_file_plain(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(FILE)", STRLEN_CONST("(FILE)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_imageviewer(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(IMAGE)", STRLEN_CONST("(IMAGE)"), s2, len2);
}

static void menu_action_setting_disp_set_label_movie(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(MOVIE)", STRLEN_CONST("(MOVIE)"), s2, len2);
}

static void menu_action_setting_disp_set_label_music(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(MUSIC)", STRLEN_CONST("(MUSIC)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(DIR)", STRLEN_CONST("(DIR)"), s2, len2);
}

static void menu_action_setting_disp_set_label_generic(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   *s = '\0';
   *w = 0;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_menu_file_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(COMP)", STRLEN_CONST("(COMP)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_shader(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(SHADER)", STRLEN_CONST("(SHADER)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_shader_preset(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(PRESET)", STRLEN_CONST("(PRESET)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_in_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(CFILE)", STRLEN_CONST("(CFILE)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_overlay(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(OVERLAY)", STRLEN_CONST("(OVERLAY)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_config(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(CONFIG)", STRLEN_CONST("(CONFIG)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_font(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(FONT)", STRLEN_CONST("(FONT)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(FILTER)", STRLEN_CONST("(FILTER)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_rdb(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(RDB)", STRLEN_CONST("(RDB)"), s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   MENU_ACTION_SETTING_GENERIC_DISP_SET_LABEL_2(w, s, len,
         path, "(CHEAT)", STRLEN_CONST("(CHEAT)"), s2, len2);
}

static void menu_action_setting_disp_set_label_core_option_override_info(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *override_path       = path_get(RARCH_PATH_CORE_OPTIONS);
   core_option_manager_t *coreopts = NULL;
   const char *options_file        = NULL;

   *s = '\0';
   *w = 19;

   if (!string_is_empty(override_path))
      options_file = path_basename_nocompression(override_path);
   else if (retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
   {
      const char *options_path = coreopts->conf_path;
      if (!string_is_empty(options_path))
         options_file = path_basename_nocompression(options_path);
   }

   if (!string_is_empty(options_file))
      strlcpy(s, options_file, len);
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);

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

   *w = 19;

   strlcpy(s2, path, len2);

   if (playlist)
      strlcpy(
            s,
            get_playlist_thumbnail_mode_value(playlist, PLAYLIST_THUMBNAIL_RIGHT),
            len);
   else
      *s = '\0';
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

   *w = 19;

   strlcpy(s2, path, len2);

   if (playlist)
      strlcpy(
            s,
            get_playlist_thumbnail_mode_value(playlist, PLAYLIST_THUMBNAIL_LEFT),
            len);
   else
      *s = '\0';
}

static void menu_action_setting_disp_set_label_playlist_sort_mode(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   enum playlist_sort_mode sort_mode;
   playlist_t *playlist  = playlist_get_cached();

   if (!playlist)
      return;

   sort_mode = playlist_get_sort_mode(playlist);
   *w        = 19;

   strlcpy(s2, path, len2);

   switch (sort_mode)
   {
      case PLAYLIST_SORT_MODE_ALPHABETICAL:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL), len);
         break;
      case PLAYLIST_SORT_MODE_OFF:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF), len);
         break;
      case PLAYLIST_SORT_MODE_DEFAULT:
      default:
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT), len);
         break;
   }
}

static void menu_action_setting_disp_set_label_core_options(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *category = path;
   const char *desc     = NULL;

   /* Add 'more' value text */
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MORE), len);
   *w = 19;

   /* If this is an options subcategory, fetch
    * the category description */
   if (!string_is_empty(category))
   {
      core_option_manager_t *coreopts = NULL;

      if (retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
         desc = core_option_manager_get_category_desc(
               coreopts, category);
   }

   /* If this isn't a subcategory (or something
    * went wrong...), use top level core options
    * menu label */
   if (string_is_empty(desc))
      desc = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS);

   strlcpy(s2, desc, len2);
}

static void menu_action_setting_disp_set_label_core_option(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   core_option_manager_t *coreopts = NULL;

   *s = '\0';
   *w = 19;

   if (retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
   {
      const char *coreopt_label    = core_option_manager_get_val_label(coreopts,
            type - MENU_SETTINGS_CORE_OPTION_START);
      if (!string_is_empty(coreopt_label))
         strlcpy(s, coreopt_label, len);
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
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      list->list[i].actiondata;
   rarch_setting_t *setting  = cbs->setting;
   *w                        = 2;

   if (setting && setting->get_string_representation)
      setting->get_string_representation(setting, s, len);
   else
      *s                     = '\0';

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_manual_content_scan_dir(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *content_dir = NULL;
   *w = 19;

   strlcpy(s2, path, len2);

   if (manual_content_scan_get_menu_content_dir(&content_dir))
      strlcpy(s, content_dir, len);
   else
      *s = '\0';
}

static void menu_action_setting_disp_set_label_manual_content_scan_system_name(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *system_name = NULL;

   *w = 19;

   strlcpy(s2, path, len2);

   if (manual_content_scan_get_menu_system_name(&system_name))
      strlcpy(s, system_name, len);
   else
      *s = '\0';
}

static void menu_action_setting_disp_set_label_manual_content_scan_core_name(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   const char *core_name = NULL;

   *w = 19;

   strlcpy(s2, path, len2);

   if (manual_content_scan_get_menu_core_name(&core_name))
      strlcpy(s, core_name, len);
   else
      *s = '\0';
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
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      list->list[i].actiondata;
   rarch_setting_t *setting  = cbs->setting;

   *w                        = 19;

   if (setting && setting->get_string_representation)
      setting->get_string_representation(setting, s, len);
   else
      *s                     = '\0';

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_setting_bool(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      list->list[i].actiondata;
   rarch_setting_t *setting  = cbs->setting;

   *w = 19;

   if (setting)
   {
      if (*setting->value.target.boolean)
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON), len);
      else
         strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
   }
   else
      *s = '\0';

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_setting_string(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *path,
      char *s2, size_t len2)
{
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      list->list[i].actiondata;
   rarch_setting_t *setting  = cbs->setting;
   *w                        = 19;

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
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      list->list[i].actiondata;
   rarch_setting_t *setting  = cbs->setting;
   const char *basename      = setting ? path_basename(setting->value.target.string) : NULL;
   *w                        = 19;

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
#ifdef HAVE_MICROPHONE
         case MENU_ENUM_LABEL_MICROPHONE_DRIVER:
         case MENU_ENUM_LABEL_MICROPHONE_RESAMPLER_DRIVER:
#endif
         case MENU_ENUM_LABEL_INPUT_DRIVER:
         case MENU_ENUM_LABEL_JOYPAD_DRIVER:
         case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         case MENU_ENUM_LABEL_RECORD_DRIVER:
         case MENU_ENUM_LABEL_MIDI_DRIVER:
         case MENU_ENUM_LABEL_LOCATION_DRIVER:
         case MENU_ENUM_LABEL_CAMERA_DRIVER:
         case MENU_ENUM_LABEL_BLUETOOTH_DRIVER:
         case MENU_ENUM_LABEL_WIFI_DRIVER:
         case MENU_ENUM_LABEL_MENU_DRIVER:
         case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
         case MENU_ENUM_LABEL_UPDATE_ASSETS:
         case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
         case MENU_ENUM_LABEL_UPDATE_CHEATS:
         case MENU_ENUM_LABEL_UPDATE_DATABASES:
         case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
         case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
         case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
         case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
         case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
         case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
         case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
         case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
         case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
         case MENU_ENUM_LABEL_CHEAT_ADD_MATCHES:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME:
         case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         case MENU_ENUM_LABEL_OVERRIDE_UNLOAD:
         case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
         case MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN:
         case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
         case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
#ifdef HAVE_LAKKA
         case MENU_ENUM_LABEL_TIMEZONE:
#endif
            BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label);
            break;
         case MENU_ENUM_LABEL_CONNECT_BLUETOOTH:
#ifdef HAVE_BLUETOOTH
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_bluetooth_is_connected);
#endif
            break;
         case MENU_ENUM_LABEL_CONNECT_WIFI:
#if defined(HAVE_NETWORKING) && defined(HAVE_WIFI)
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_wifi_is_online);
#endif
            break;
         case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
#ifdef HAVE_CHEATS
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_cheat_num_passes);
#endif
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_INFO:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_remap_file_info);
            break;
         case MENU_ENUM_LABEL_OVERRIDE_FILE_INFO:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_override_file_info);
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
         case MENU_ENUM_LABEL_CORE_OPTIONS:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_core_options);
            break;
         case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         case MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY:
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         case MENU_ENUM_LABEL_FAVORITES:
         case MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_LIST:
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
         case MENU_ENUM_LABEL_SHADER_OPTIONS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE:
         case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
         case MENU_ENUM_LABEL_CORE_COUNTERS:
         case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
         case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         case MENU_ENUM_LABEL_CORE_INFORMATION:
         case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
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
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_playlist_sort_mode);
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
#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_CORE_UPDATER_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_core_updater_entry);
            break;
#endif
         case MENU_ENUM_LABEL_CORE_MANAGER_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_core_manager_entry);
            break;
#ifdef HAVE_MIST
         case MENU_ENUM_LABEL_CORE_MANAGER_STEAM_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_core_manager_steam_entry);
            break;
#endif
         case MENU_ENUM_LABEL_CONTENTLESS_CORE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_contentless_core);
            break;
         case MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_INFO:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_core_option_override_info);
            break;
         #ifdef HAVE_LAKKA
         case MENU_ENUM_LABEL_CPU_PERF_MODE:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_cpu_gov_mode);
            break;
         case MENU_ENUM_LABEL_CPU_POLICY_CORE_GOVERNOR:
         case MENU_ENUM_LABEL_CPU_POLICY_MENU_GOVERNOR:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_cpu_gov_choose);
            break;
         case MENU_ENUM_LABEL_CPU_POLICY_ENTRY:
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_cpu_policy);
            break;
         case MENU_ENUM_LABEL_CPU_POLICY_MIN_FREQ:
         case MENU_ENUM_LABEL_CPU_POLICY_MAX_FREQ:
            BIND_ACTION_GET_VALUE(cbs, menu_action_cpu_freq_label);
            break;
         case MENU_ENUM_LABEL_CPU_MANAGED_MIN_FREQ:
         case MENU_ENUM_LABEL_CPU_MANAGED_MAX_FREQ:
            BIND_ACTION_GET_VALUE(cbs, menu_action_cpu_managed_freq_label);
            break;
         case MENU_ENUM_LABEL_CPU_POLICY_GOVERNOR:
            BIND_ACTION_GET_VALUE(cbs, menu_action_cpu_governor_label);
            break;
         #endif
         default:
            return -1;
      }
   }
   else
      return -1;

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
#ifdef HAVE_CHEATS
      {
         MENU_SETTINGS_CHEAT_BEGIN,
         MENU_SETTINGS_CHEAT_END,
         menu_action_setting_disp_set_label_cheat
      },
#endif
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
      {
         MENU_SETTINGS_REMAPPING_PORT_BEGIN,
         MENU_SETTINGS_REMAPPING_PORT_END,
         menu_action_setting_disp_set_label_menu_more
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
      case FILE_TYPE_DIRECTORY:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_directory);
         break;
      case FILE_TYPE_PARENT_DIRECTORY:
      case FILE_TYPE_USE_DIRECTORY:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_generic);
         break;
      case FILE_TYPE_CARCHIVE:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_carchive);
         break;
      case FILE_TYPE_OVERLAY:
      case FILE_TYPE_OSK_OVERLAY:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_overlay);
         break;
      case FILE_TYPE_FONT:
      case FILE_TYPE_VIDEO_FONT:
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
      case FILE_TYPE_RDB:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_rdb);
         break;
      case FILE_TYPE_CHEAT:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_menu_file_cheat);
         break;
      case MENU_SETTINGS_CHEAT_MATCH:
#ifdef HAVE_CHEATS
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_cheat_match);
#endif
         break;
      case MENU_SETTING_SUBGROUP:
      case MENU_SETTING_ACTION:
      case MENU_SETTING_ACTION_REMAP_FILE_MANAGER_LIST:
      case MENU_SETTING_ACTION_REMAP_FILE_LOAD:
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
      case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND:
      case MENU_EXPLORE_TAB:
      case MENU_CONTENTLESS_CORES_TAB:
      case MENU_PLAYLISTS_TAB:
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
      case MENU_SETTING_ACTION_CORE_LOCK:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_core_lock);
         break;
      case MENU_SETTING_ACTION_CORE_SET_STANDALONE_EXEMPT:
         BIND_ACTION_GET_VALUE(cbs,
               menu_action_setting_disp_set_label_core_set_standalone_exempt);
         break;
      case 32: /* Recent history entry */
      case 65535: /* System info entry */
         BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label_entry);
         break;
      default:
         BIND_ACTION_GET_VALUE(cbs, menu_action_setting_disp_set_label);
         break;
   }

   return 0;
}

int menu_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, size_t lbl_len,
      unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY:
#ifdef HAVE_CHEEVOS
            BIND_ACTION_GET_VALUE(cbs,
                  menu_action_setting_disp_set_label_cheevos_entry);
#endif
            return 0;
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
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
         case MENU_ENUM_LABEL_RESTART_RETROARCH:
         case MENU_ENUM_LABEL_QUIT_RETROARCH:
         case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
         case MENU_ENUM_LABEL_REBOOT:
         case MENU_ENUM_LABEL_SHUTDOWN:
         case MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG:
         case MENU_ENUM_LABEL_START_NET_RETROPAD:
         case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
            return 0;
         default:
            break;
      }
   }

   if (cbs->setting && !cbs->setting->get_string_representation)
   {
      switch (cbs->setting->type)
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
         menu_action_setting_disp_set_label_core_option);
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
