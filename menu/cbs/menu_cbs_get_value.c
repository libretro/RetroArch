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

#include "../menu.h"
#include "../menu_cbs.h"
#include "../menu_shader.h"
#include "../menu_hash.h"

#include "../../general.h"
#include "../../performance.h"
#include "../../system.h"
#include "../../intl/intl.h"

const char axis_labels[4][128] = {
   RETRO_LBL_ANALOG_LEFT_X,
   RETRO_LBL_ANALOG_LEFT_Y,
   RETRO_LBL_ANALOG_RIGHT_X,
   RETRO_LBL_ANALOG_RIGHT_Y
};

static void menu_action_setting_disp_set_label_cheat_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global = global_get_ptr();

   *w = 19;
   strlcpy(s2, path, len2);
   if (global && global->cheat)
      snprintf(s, len, "%u", global->cheat->buf_size);
}

static void menu_action_setting_disp_set_label_remap_file_load(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *w = 19;
   strlcpy(s2, path, len2);
   if (settings)
      fill_pathname_base(s, settings->input.remapping_path,
            len);
}

static void menu_action_setting_disp_set_label_configurations(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global = global_get_ptr();

   *w = 19;
   strlcpy(s2, path, len2);
   if (global && *global->config_path)
      fill_pathname_base(s, global->config_path,
            len);
   else
      strlcpy(s, menu_hash_to_str(MENU_VALUE_DIRECTORY_DEFAULT), len);
}

static void menu_action_setting_disp_set_label_shader_filter_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned pass = 0;
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   (void)pass;
   (void)menu;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu || !menu->shader)
      return;

  pass = (type - MENU_SETTINGS_SHADER_PASS_FILTER_0);

  switch (menu->shader->pass[pass].filter)
  {
     case 0:
        strlcpy(s, menu_hash_to_str(MENU_VALUE_DONT_CARE),
              len);
        break;
     case 1:
        strlcpy(s, menu_hash_to_str(MENU_VALUE_LINEAR),
              len);
        break;
     case 2:
        strlcpy(s, menu_hash_to_str(MENU_VALUE_NEAREST),
              len);
        break;
  }
#endif
}

static void menu_action_setting_disp_set_label_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s, menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), len);

   if (settings && *settings->video.softfilter_plugin)
      fill_short_pathname_representation(s,
            settings->video.softfilter_plugin, len);
}

static void menu_action_setting_disp_set_label_shader_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   (void)menu;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (menu && menu->shader)
      snprintf(s, len, "%u", menu->shader->passes);
#endif
}

static void menu_action_setting_disp_set_label_shader_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned pass = (type - MENU_SETTINGS_SHADER_PASS_0);
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   (void)pass;
   (void)menu;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);
   strlcpy(s, menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), len);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;
   if (*menu->shader->pass[pass].source.path)
      fill_pathname_base(s,
            menu->shader->pass[pass].source.path, len);
#endif
}

static void menu_action_setting_disp_set_label_shader_default_filter(

      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();

   *s = '\0';
   *w = 19;

   if (!settings)
      return;

   if (settings->video.smooth)
      strlcpy(s, menu_hash_to_str(MENU_VALUE_LINEAR), len); 
   else
      strlcpy(s, menu_hash_to_str(MENU_VALUE_NEAREST), len);
}

static void menu_action_setting_disp_set_label_shader_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   const struct video_shader_parameter *param = NULL;
   struct video_shader *shader = NULL;
#endif
   driver_t *driver = driver_get_ptr();

   if (!driver->video_poke || !driver->video_data)
      return;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   shader = video_shader_driver_get_current_shader();

   if (!shader)
      return;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];

   if (!param)
      return;

   snprintf(s, len, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
#endif
}

static void menu_action_setting_disp_set_label_shader_preset_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   const struct video_shader_parameter *param = NULL;
#endif
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   (void)menu;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

   param = &menu->shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0];

   if (!param)
      return;

   snprintf(s, len, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
#endif
}

static void menu_action_setting_disp_set_label_shader_scale_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned pass = 0;
   unsigned scale_value = 0;
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   (void)pass;
   (void)scale_value;
   (void)menu;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

   pass        = (type - MENU_SETTINGS_SHADER_PASS_SCALE_0);
   scale_value = menu->shader->pass[pass].fbo.scale_x;

   if (!scale_value)
      strlcpy(s, menu_hash_to_str(MENU_VALUE_DONT_CARE), len);
   else
      snprintf(s, len, "%ux", scale_value);
#endif
}

static void menu_action_setting_disp_set_label_menu_file_core(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   const char *alt = NULL;
   strlcpy(s, "(CORE)", len);
   menu_list_get_alt_at_offset(list, i, &alt);
   *w = strlen(s);
   if (alt)
      strlcpy(s2, alt, len2);
}

static void menu_action_setting_disp_set_label_input_desc(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings = config_get_ptr();
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset /
      (RARCH_FIRST_CUSTOM_BIND + 4);
   unsigned inp_desc_button_index_offset = inp_desc_index_offset -
      (inp_desc_user * (RARCH_FIRST_CUSTOM_BIND + 4));
   unsigned remap_id = 0;

   if (!settings)
      return;
   
   remap_id = settings->input.remap_ids
      [inp_desc_user][inp_desc_button_index_offset];

   if (inp_desc_button_index_offset < RARCH_FIRST_CUSTOM_BIND)
      strlcpy(s, 
            settings->input.binds[inp_desc_user][remap_id].desc,
            len);
   else
      strlcpy(s, axis_labels[remap_id], len);

   *w = 19;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   global_t *global     = global_get_ptr();
   unsigned cheat_index = type - MENU_SETTINGS_CHEAT_BEGIN;

   if (!global)
      return;

   if (cheat_index < global->cheat->buf_size)
      snprintf(s, len, "%s : (%s)",
            (global->cheat->cheats[cheat_index].code != NULL)
            ? global->cheat->cheats[cheat_index].code : 
            menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE),
            global->cheat->cheats[cheat_index].state ? 
            menu_hash_to_str(MENU_VALUE_ON) :
            menu_hash_to_str(MENU_VALUE_OFF)
            );
   *w = 19;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_perf_counters_common(
      const struct retro_perf_counter **counters,
      unsigned offset, char *s, size_t len
      )
{
   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   snprintf(s, len,
#ifdef _WIN32
         "%I64u ticks, %I64u runs.",
#else
         "%llu ticks, %llu runs.",
#endif
         ((unsigned long long)counters[offset]->total /
          (unsigned long long)counters[offset]->call_cnt),
         (unsigned long long)counters[offset]->call_cnt);
}

static void menu_action_setting_disp_set_label_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_animation_t *anim = menu_animation_get_ptr();
   const struct retro_perf_counter **counters =
      (const struct retro_perf_counter **)perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   menu_action_setting_disp_set_label_perf_counters_common(
         counters, offset, s, len);

   if (anim)
      anim->label.is_updated = true;
}

static void menu_action_setting_disp_set_label_libretro_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_animation_t *anim = menu_animation_get_ptr();
   const struct retro_perf_counter **counters =
      (const struct retro_perf_counter **)perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   *s = '\0';
   *w = 19;
   strlcpy(s2, path, len2);

   menu_action_setting_disp_set_label_perf_counters_common(
         counters, offset, s, len);

   if (anim)
      anim->label.is_updated = true;
}

static void menu_action_setting_disp_set_label_menu_more(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   strlcpy(s, "...", len);
   *w = 19;
   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_state(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings        = config_get_ptr();

   if (!settings)
      return;

   strlcpy(s2, path, len2);
   *w = 16;
   snprintf(s, len, "%d", settings->state_slot);
   if (settings->state_slot == -1)
      strlcat(s, " (Auto)", len);
}

static void menu_action_setting_disp_set_label_menu_toggle_gamepad_combo(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   settings_t *settings        = config_get_ptr();

   if (!settings)
      return;

   strlcpy(s2, path, len2);
   *w = 19;
   switch (settings->input.menu_toggle_gamepad_combo)
   {
      case 0:
         snprintf(s, len, "%s", "None");
         break;
      case 1:
         snprintf(s, len, "%s", "Down + L1 + R1 + Y");
         break;
      case 2:
         snprintf(s, len, "%s", "L3 + R3");
         break;
   }
}


static void menu_action_setting_disp_set_label_menu_disk_index(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned images = 0, current = 0;
   rarch_system_info_t *system = rarch_system_info_get_ptr();
   struct retro_disk_control_callback *control = NULL;

   if (!system)
      return;

   control = &system->disk_control;

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
      strlcpy(s, menu_hash_to_str(MENU_VALUE_NO_DISK), len);
   else
      snprintf(s, len, "%u", current + 1);
}

static void menu_action_setting_disp_set_label_menu_video_resolution(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   unsigned width = 0, height = 0;

   *w = 19;
   *s = '\0';

   (void)width;
   (void)height;

   strlcpy(s2, path, len2);

   if (video_driver_get_video_output_size(&width, &height))
      snprintf(s, len, "%ux%u", width, height);
   else
      strlcpy(s, menu_hash_to_str(MENU_VALUE_NOT_AVAILABLE), len);
}

static void menu_action_setting_generic_disp_set_label(
      unsigned *w, char *s, size_t len,
      const char *path, const char *label,
      char *s2, size_t len2)
{
   *s = '\0';

   if (label)
      strlcpy(s, label, len);
   *w = strlen(s);

   strlcpy(s2, path, len2);
}

static void menu_action_setting_disp_set_label_menu_file_plain(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(FILE)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_image(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(IMG)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_imageviewer(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(DIR)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(OVERLAY)", s2, len2);
}

static void menu_action_setting_disp_set_label_menu_file_config(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   const char *alt = NULL;
   strlcpy(s, "(CORE)", len);
   menu_list_get_alt_at_offset(list, i, &alt);
   *w = strlen(s);
   if (alt)
      strlcpy(s2, alt, len2);
}

static void menu_action_setting_disp_set_label_menu_file_rdb(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
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
      const char *entry_label,
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
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   menu_action_setting_generic_disp_set_label(w, s, len,
         path, "(CHEAT)", s2, len2);
}

static void menu_action_setting_disp_set_label(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *s, size_t len,
      const char *entry_label,
      const char *path,
      char *s2, size_t len2)
{
   rarch_system_info_t *system = rarch_system_info_get_ptr();
   uint32_t hash_label  = menu_hash_calculate(label);

   *s = '\0';
   *w = 19;

   switch (hash_label)
   {
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         *w = strlen(label);
         break;
   }

   if (type >= MENU_SETTINGS_CORE_OPTION_START)
   {
      const char *core_opt = NULL;
      if (!system)
         return;

      core_opt = core_option_get_val(system->core_options,
               type - MENU_SETTINGS_CORE_OPTION_START);

      strlcpy(s, core_opt ? core_opt : "", len);
   }
   else
      setting_get_label(list, s,
            len, w, type, label, entry_label, i);

   strlcpy(s2, path, len2);
}

static int menu_cbs_init_bind_get_string_representation_compare_label(
      menu_file_list_cbs_t *cbs, uint32_t label_hash)
{
   switch (label_hash)
   {
      case MENU_LABEL_SAVE_STATE:
      case MENU_LABEL_LOAD_STATE:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_state;
         break;
      case MENU_LABEL_INPUT_MENU_TOGGLE_GAMEPAD_COMBO:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_menu_toggle_gamepad_combo;
         break;
      case MENU_LABEL_CHEAT_NUM_PASSES:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_cheat_num_passes;
         break;
      case MENU_LABEL_REMAP_FILE_LOAD:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_remap_file_load;
         break;
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_filter_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_scale_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_num_passes;
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_shader_default_filter;
         break;
      case MENU_LABEL_VIDEO_FILTER:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_filter;
         break;
      case MENU_LABEL_CONFIGURATIONS:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_configurations;
         break;
      case MENU_LABEL_SCREEN_RESOLUTION:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_menu_video_resolution;
         break;
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
      case MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
      case MENU_LABEL_DETECT_CORE_LIST:
      case MENU_LABEL_LOAD_CONTENT:
      case MENU_LABEL_CORE_OPTIONS:
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
      case MENU_LABEL_SHADER_OPTIONS:
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
      case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_LABEL_CHEAT_FILE_SAVE_AS:
      case MENU_LABEL_FRONTEND_COUNTERS:
      case MENU_LABEL_CORE_COUNTERS:
      case MENU_LABEL_DATABASE_MANAGER_LIST:
      case MENU_LABEL_CURSOR_MANAGER_LIST:
      case MENU_LABEL_RESTART_CONTENT:
      case MENU_LABEL_CLOSE_CONTENT:
      case MENU_LABEL_RESUME_CONTENT:
      case MENU_LABEL_TAKE_SCREENSHOT:
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
      case MENU_LABEL_CORE_INFORMATION:
      case MENU_LABEL_SYSTEM_INFORMATION:
         cbs->action_get_value =
            menu_action_setting_disp_set_label_menu_more;
         break;
      default:
         return - 1;
   }

   return 0;
}

static int menu_cbs_init_bind_get_string_representation_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_input_desc;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_cheat;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_perf_counters;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_get_value =
         menu_action_setting_disp_set_label_libretro_perf_counters;
   else
   {
      switch (type)
      {
         case MENU_FILE_CORE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_core;
            break;
         case MENU_FILE_PLAIN:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_plain;
            break;
         case MENU_FILE_MOVIE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_movie;
            break;
         case MENU_FILE_MUSIC:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_music;
            break;
         case MENU_FILE_IMAGE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_image;
            break;
         case MENU_FILE_IMAGEVIEWER:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_imageviewer;
            break;
         case MENU_FILE_USE_DIRECTORY:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_use_directory;
            break;
         case MENU_FILE_DIRECTORY:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_directory;
            break;
         case MENU_FILE_CARCHIVE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_carchive;
            break;
         case MENU_FILE_OVERLAY:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_overlay;
            break;
         case MENU_FILE_FONT:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_font;
            break;
         case MENU_FILE_SHADER:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_shader;
            break;
         case MENU_FILE_SHADER_PRESET:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_shader_preset;
            break;
         case MENU_FILE_CONFIG:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_config;
            break;
         case MENU_FILE_IN_CARCHIVE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_in_carchive;
            break;
         case MENU_FILE_VIDEOFILTER:
         case MENU_FILE_AUDIOFILTER:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_filter;
            break;
         case MENU_FILE_DOWNLOAD_CORE:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_url_core;
            break;
         case MENU_FILE_RDB:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_rdb;
            break;
         case MENU_FILE_CURSOR:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_cursor;
            break;
         case MENU_FILE_CHEAT:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_file_cheat;
            break;
         case MENU_SETTING_SUBGROUP:
         case MENU_SETTINGS_CUSTOM_VIEWPORT:
         case MENU_SETTINGS_CUSTOM_BIND_ALL:
         case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_more;
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            cbs->action_get_value =
               menu_action_setting_disp_set_label_menu_disk_index;
            break;
         default:
            cbs->action_get_value = menu_action_setting_disp_set_label;
            break;
      }
   }

   return 0;
}

int menu_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      cbs->action_get_value =
         menu_action_setting_disp_set_label_shader_parameter;
      return 0;
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      cbs->action_get_value =
         menu_action_setting_disp_set_label_shader_preset_parameter;
      return 0;
   }

   if (menu_cbs_init_bind_get_string_representation_compare_label(cbs, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_get_string_representation_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
