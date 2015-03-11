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

#include "menu.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"
#include "menu_entries.h"
#include "menu_shader.h"

#include "../retroarch.h"
#include "../performance.h"

#include "../input/input_remapping.h"

static int action_start_remap_file_load(unsigned type, const char *label,
      unsigned action)
{
   g_settings.input.remapping_path[0] = '\0';
   input_remapping_set_defaults();
   return 0;
}

static int action_start_video_filter_file_load(unsigned type, const char *label,
      unsigned action)
{
   g_settings.video.softfilter_plugin[0] = '\0';
   rarch_main_command(RARCH_CMD_REINIT);
   return 0;
}

static int action_start_performance_counters_core(unsigned type, const char *label,
      unsigned action)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   (void)label;
   (void)action;

   if (counters[offset])
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_input_desc(unsigned type, const char *label,
      unsigned action)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / RARCH_FIRST_CUSTOM_BIND;
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * RARCH_FIRST_CUSTOM_BIND);

   (void)label;
   (void)action;

   g_settings.input.remap_ids[inp_desc_user][inp_desc_button_index_offset] = 
      g_settings.input.binds[inp_desc_user][inp_desc_button_index_offset].id;

   return 0;
}

static int action_start_shader_action_parameter(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader_parameter *param = NULL;
   struct video_shader *shader = video_shader_driver_get_current_shader();

   if (!shader)
      return 0;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
   param->current = param->initial;
   param->current = min(max(param->minimum, param->current), param->maximum);

#endif

   return 0;
}

static int action_start_shader_action_preset_parameter(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_parameter *param = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!(shader = menu->shader))
      return 0;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0];
   param->current = param->initial;
   param->current = min(max(param->minimum, param->current), param->maximum);
#endif

   return 0;
}

static int action_start_shader_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   hack_shader_pass = type - MENU_SETTINGS_SHADER_PASS_0;
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   shader = menu->shader;

   if (shader)
      shader_pass = &shader->pass[hack_shader_pass];

   if (shader_pass)
      *shader_pass->source.path = '\0';
#endif

   return 0;
}


static int action_start_shader_scale_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   shader      = menu->shader;

   if (shader)
   {
      shader_pass = &shader->pass[pass];

      if (shader_pass)
      {
         shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = 0;
         shader_pass->fbo.valid = false;
      }
   }
#endif

   return 0;
}

static int action_start_shader_filter_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   
   shader = menu->shader;
   if (!shader)
      return -1;
   shader_pass = &shader->pass[pass];
   if (!shader_pass)
      return -1;

   shader_pass->filter = RARCH_FILTER_UNSPEC;
#endif

   return 0;
}

static int action_start_shader_num_passes(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   
   shader = menu->shader;
   if (!shader)
      return -1;
   if (shader->passes)
      shader->passes = 0;
   menu->need_refresh = true;

   video_shader_resolve_parameters(NULL, menu->shader);
#endif
   return 0;
}

static int action_start_cheat_num_passes(unsigned type, const char *label,
      unsigned action)
{
   cheat_manager_t *cheat = g_extern.cheat;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!cheat)
      return -1;

   if (cheat->size)
   {
      cheat_manager_realloc(cheat, 0);
      menu->need_refresh = true;
   }

   return 0;
}

static int action_start_performance_counters_frontend(unsigned type, const char *label,
      unsigned action)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;

   (void)label;

   if (counters[offset])
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_core_setting(unsigned type,
      const char *label, unsigned action)
{
   unsigned idx = type - MENU_SETTINGS_CORE_OPTION_START;

   (void)label;

   core_option_set_default(g_extern.system.core_options, idx);

   return 0;
}

static int action_start_lookup_setting(unsigned type, const char *label,
      unsigned action)
{
   int ret = menu_setting_set(type, label, MENU_ACTION_START, false);

   return ret;
}

void menu_entries_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_start = action_start_lookup_setting;

   if (!strcmp(label, "remap_file_load"))
      cbs->action_start = action_start_remap_file_load;
   if (!strcmp(label, "video_filter"))
      cbs->action_start = action_start_video_filter_file_load;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_start = action_start_shader_pass;
   else if (!strcmp(label, "video_shader_scale_pass"))
      cbs->action_start = action_start_shader_scale_pass;
   else if (!strcmp(label, "video_shader_filter_pass"))
      cbs->action_start = action_start_shader_filter_pass;
   else if (!strcmp(label, "video_shader_num_passes"))
      cbs->action_start = action_start_shader_num_passes;
   else if (!strcmp(label, "cheat_num_passes"))
      cbs->action_start = action_start_cheat_num_passes;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_start = action_start_shader_action_parameter;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_start = action_start_shader_action_preset_parameter;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_core;
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_start = action_start_input_desc;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_frontend;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_start = action_start_core_setting;
}
