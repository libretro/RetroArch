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

#include <file/file_path.h>
#include "menu.h"
#include "menu_entries_cbs.h"
#include "menu_entries.h"
#include "menu_shader.h"

#include "../performance.h"

static void menu_action_setting_disp_set_label_cheat_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   global_t *global = global_get_ptr();

   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   snprintf(type_str, type_str_size, "%u", global->cheat->buf_size);
}

static void menu_action_setting_disp_set_label_remap_file_load(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   settings_t *settings = config_get_ptr();

   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   fill_pathname_base(type_str, settings->input.remapping_path,
         type_str_size);
}

static void menu_action_setting_disp_set_label_configurations(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   global_t *global = global_get_ptr();

   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   if (*global->config_path)
      fill_pathname_base(type_str, global->config_path,
            type_str_size);
   else
      strlcpy(type_str, "<default>", type_str_size);
}

static void menu_action_setting_disp_set_label_shader_filter_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned pass = 0;
   static const char *modes[] = {
      "Don't care",
      "Linear",
      "Nearest"
   };
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)pass;
   (void)modes;
   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

  pass = (type - MENU_SETTINGS_SHADER_PASS_FILTER_0);

  strlcpy(type_str, modes[menu->shader->pass[pass].filter],
        type_str_size);
#endif
}

static void menu_action_setting_disp_set_label_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   settings_t *settings = config_get_ptr();

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   strlcpy(type_str, "N/A", type_str_size);

   if (*settings->video.softfilter_plugin)
   strlcpy(type_str, path_basename(settings->video.softfilter_plugin),
         type_str_size);
}

static void menu_action_setting_disp_set_label_shader_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   snprintf(type_str, type_str_size, "%u", menu->shader->passes);
#endif
}

static void menu_action_setting_disp_set_label_shader_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned pass = (type - MENU_SETTINGS_SHADER_PASS_0);
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)pass;
   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   strlcpy(type_str, "N/A", type_str_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (*menu->shader->pass[pass].source.path)
      fill_pathname_base(type_str,
            menu->shader->pass[pass].source.path, type_str_size);
#endif
}

static void menu_action_setting_disp_set_label_shader_default_filter(

      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   settings_t *settings = config_get_ptr();

   *type_str = '\0';
   *w = 19;
   snprintf(type_str, type_str_size, "%s",
         settings->video.smooth ? "Linear" : "Nearest");
}

static void menu_action_setting_disp_set_label_shader_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   const struct video_shader_parameter *param = NULL;
   struct video_shader *shader = NULL;
#endif
   driver_t *driver = driver_get_ptr();

   if (!driver->video_poke)
      return;
   if (!driver->video_data)
      return;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   shader = video_shader_driver_get_current_shader();

   if (!shader)
      return;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];

   if (!param)
      return;

   snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
#endif
}

static void menu_action_setting_disp_set_label_shader_preset_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   const struct video_shader_parameter *param = NULL;
#endif
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

   param = &menu->shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0];

   if (!param)
      return;

   snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
#endif
}

static void menu_action_setting_disp_set_label_shader_scale_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned pass = 0;
   unsigned scale_value = 0;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

   (void)pass;
   (void)scale_value;
   (void)menu;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

   pass        = (type - MENU_SETTINGS_SHADER_PASS_SCALE_0);
   scale_value = menu->shader->pass[pass].fbo.scale_x;

   if (!scale_value)
      strlcpy(type_str, "Don't care", type_str_size);
   else
      snprintf(type_str, type_str_size, "%ux", scale_value);
#endif
}

static void menu_action_setting_disp_set_label_menu_file_core(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   const char *alt = NULL;
   strlcpy(type_str, "(CORE)", type_str_size);
   menu_list_get_alt_at_offset(list, i, &alt);
   *w = strlen(type_str);
   if (alt)
      strlcpy(path_buf, alt, path_buf_size);
}

static void menu_action_setting_disp_set_label_input_desc(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   settings_t *settings = config_get_ptr();
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / 
      RARCH_FIRST_CUSTOM_BIND;
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - 
      (inp_desc_user * RARCH_FIRST_CUSTOM_BIND);
   unsigned remap_id = settings->input.remap_ids
      [inp_desc_user][inp_desc_button_index_offset];

   snprintf(type_str, type_str_size, "%s",
         settings->input.binds[inp_desc_user][remap_id].desc);
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   global_t *global     = global_get_ptr();
   unsigned cheat_index = type - MENU_SETTINGS_CHEAT_BEGIN;

   if (cheat_index < global->cheat->buf_size)
      snprintf(type_str, type_str_size, "%s : (%s)",
            (global->cheat->cheats[cheat_index].code != NULL)
            ? global->cheat->cheats[cheat_index].code : "N/A",
            global->cheat->cheats[cheat_index].state ? "ON" : "OFF"
            );
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   const struct retro_perf_counter **counters = 
      (const struct retro_perf_counter **)perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;
   runloop_t *runloop = rarch_main_get_ptr();

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   snprintf(type_str, type_str_size,
#ifdef _WIN32
         "%I64u ticks, %I64u runs.",
#else
         "%llu ticks, %llu runs.",
#endif
         ((unsigned long long)counters[offset]->total /
          (unsigned long long)counters[offset]->call_cnt),
         (unsigned long long)counters[offset]->call_cnt);

   runloop->frames.video.current.menu.label.is_updated = true;
}

static void menu_action_setting_disp_set_label_libretro_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   const struct retro_perf_counter **counters = 
      (const struct retro_perf_counter **)perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;
   runloop_t *runloop = rarch_main_get_ptr();

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   snprintf(type_str, type_str_size,
#ifdef _WIN32
         "%I64u ticks, %I64u runs.",
#else
         "%llu ticks, %llu runs.",
#endif
         ((unsigned long long)counters[offset]->total /
          (unsigned long long)counters[offset]->call_cnt),
         (unsigned long long)counters[offset]->call_cnt);

   runloop->frames.video.current.menu.label.is_updated = true;
}

static void menu_action_setting_disp_set_label_menu_more(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "...", type_str_size);
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
}


static void menu_action_setting_disp_set_label_menu_disk_index(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned images = 0, current = 0;
   global_t *global     = global_get_ptr();
   const struct retro_disk_control_callback *control =
      (const struct retro_disk_control_callback*)
      &global->system.disk_control;

   *w = 19;
   *type_str = '\0';
   strlcpy(path_buf, path, path_buf_size);
   if (!control)
      return;

   images = control->get_num_images();
   current = control->get_image_index();

   if (current >= images)
      strlcpy(type_str, "No Disk", type_str_size);
   else
      snprintf(type_str, type_str_size, "%u", current + 1);
}

static void menu_action_setting_disp_set_label_menu_video_resolution(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned width = 0, height = 0;
   driver_t *driver = driver_get_ptr();

   *w = 19;
   *type_str = '\0';

   (void)width;
   (void)height;

   strlcpy(path_buf, path, path_buf_size);

   if (driver->video_data && driver->video_poke &&
         driver->video_poke->get_video_output_size)
   {
      driver->video_poke->get_video_output_size(driver->video_data,
            &width, &height);
      snprintf(type_str, type_str_size, "%ux%u", width, height);
   }
   else
      strlcpy(type_str, "N/A", type_str_size);
}

static void menu_action_setting_generic_disp_set_label(
      unsigned *w, char *type_str, size_t type_str_size,
      const char *path, const char *label,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';

   if (label)
      strlcpy(type_str, label, type_str_size);
   *w = strlen(type_str);

   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_plain(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(FILE)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_use_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, NULL, path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(DIR)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(COMP)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_shader(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(SHADER)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_shader_preset(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(PRESET)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_in_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CFILE)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_overlay(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(OVERLAY)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_config(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CONFIG)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_font(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(FONT)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(FILTER)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_url(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(URL)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_rdb(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(RDB)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_cursor(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CURSOR)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CHEAT)", path_buf, path_buf_size);
}

static void menu_action_setting_disp_set_label(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   global_t *global     = global_get_ptr();

   *type_str = '\0';
   *w = 19;

   if (!strcmp(label, "performance_counters"))
      *w = strlen(label);

   if (!strcmp(label, "history_list"))
      *w = strlen(label);

   if (type >= MENU_SETTINGS_CORE_OPTION_START)
      strlcpy(
            type_str,
            core_option_get_val(global->system.core_options,
               type - MENU_SETTINGS_CORE_OPTION_START),
            type_str_size);
   else
      setting_get_label(list, type_str,
            type_str_size, w, type, label, entry_label, i);

   strlcpy(path_buf, path, path_buf_size);
}

void menu_entries_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_input_desc;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_cheat;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_perf_counters;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_libretro_perf_counters;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_shader_preset_parameter;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_shader_parameter;
   else if (!strcmp(label, "cheat_num_passes"))
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_cheat_num_passes;
   else if (!strcmp(label, "remap_file_load"))
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_remap_file_load;
   else if (!strcmp(label, "video_shader_filter_pass"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_filter_pass;
   else if (!strcmp(label, "video_shader_scale_pass"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_scale_pass;
   else if (!strcmp(label, "video_shader_num_passes"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_num_passes;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_pass;
   else if (!strcmp(label, "video_shader_default_filter"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_default_filter;
   else if (!strcmp(label, "video_filter"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_filter;
   else if (!strcmp(label, "configurations"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_configurations;
   else
   {
      switch (type)
      {
         case MENU_FILE_CORE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_core;
            break;
         case MENU_FILE_PLAIN:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_plain;
            break;
         case MENU_FILE_USE_DIRECTORY:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_use_directory;
            break;
         case MENU_FILE_DIRECTORY:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_directory;
            break;
         case MENU_FILE_CARCHIVE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_carchive;
            break;
         case MENU_FILE_OVERLAY:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_overlay;
            break;
         case MENU_FILE_FONT:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_font;
            break;
         case MENU_FILE_SHADER:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_shader;
            break;
         case MENU_FILE_SHADER_PRESET:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_shader_preset;
            break;
         case MENU_FILE_CONFIG:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_config;
            break;
         case MENU_FILE_IN_CARCHIVE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_in_carchive;
            break;
         case MENU_FILE_VIDEOFILTER:
         case MENU_FILE_AUDIOFILTER:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_filter;
            break;
         case MENU_FILE_DOWNLOAD_CORE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_url;
            break;
         case MENU_FILE_RDB:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_rdb;
            break;
         case MENU_FILE_CURSOR:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_cursor;
            break;
         case MENU_FILE_CHEAT:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_cheat;
            break;
         case MENU_SETTING_SUBGROUP:
         case MENU_SETTINGS_CUSTOM_VIEWPORT:
         case MENU_SETTINGS_CUSTOM_BIND_ALL:
         case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_more;
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_disk_index;
            break;
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_video_resolution;
            break;
         default:
            cbs->action_get_representation = menu_action_setting_disp_set_label;
            break;
      }
   }
}
