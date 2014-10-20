/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "menu_entries_cbs.h"
#include "menu_action.h"
#include "menu_common.h"
#include "menu_list.h"
#include "menu_input_line_cb.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "backend/menu_backend.h"

#include "../../file_ext.h"
#include "../../file_path.h"
#include "../../config.def.h"
#include "../../performance.h"

#ifdef GEKKO
enum
{
   GX_RESOLUTIONS_512_192 = 0,
   GX_RESOLUTIONS_598_200,
   GX_RESOLUTIONS_640_200,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_448_224,
   GX_RESOLUTIONS_480_224,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_340_232,
   GX_RESOLUTIONS_512_232,
   GX_RESOLUTIONS_512_236,
   GX_RESOLUTIONS_336_240,
   GX_RESOLUTIONS_352_240,
   GX_RESOLUTIONS_384_240,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_530_240,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_512_384,
   GX_RESOLUTIONS_598_400,
   GX_RESOLUTIONS_640_400,
   GX_RESOLUTIONS_384_448,
   GX_RESOLUTIONS_448_448,
   GX_RESOLUTIONS_480_448,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_576_448,
   GX_RESOLUTIONS_608_448,
   GX_RESOLUTIONS_640_448,
   GX_RESOLUTIONS_340_464,
   GX_RESOLUTIONS_512_464,
   GX_RESOLUTIONS_512_472,
   GX_RESOLUTIONS_352_480,
   GX_RESOLUTIONS_384_480,
   GX_RESOLUTIONS_512_480,
   GX_RESOLUTIONS_530_480,
   GX_RESOLUTIONS_608_480,
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST,
};

unsigned menu_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
   { 512, 192 },
   { 598, 200 },
   { 640, 200 },
   { 384, 224 },
   { 448, 224 },
   { 480, 224 },
   { 512, 224 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 340, 232 },
   { 512, 232 },
   { 512, 236 },
   { 336, 240 },
   { 352, 240 },
   { 384, 240 },
   { 512, 240 },
   { 530, 240 },
   { 640, 240 },
   { 512, 384 },
   { 598, 400 },
   { 640, 400 },
   { 384, 448 },
   { 448, 448 },
   { 480, 448 },
   { 512, 448 },
   { 576, 448 },
   { 608, 448 },
   { 640, 448 },
   { 340, 464 },
   { 512, 464 },
   { 512, 472 },
   { 352, 480 },
   { 384, 480 },
   { 512, 480 },
   { 530, 480 },
   { 608, 480 },
   { 640, 480 },
};

unsigned menu_current_gx_resolution = GX_RESOLUTIONS_640_480;
#endif

static void common_load_content(void)
{
   rarch_main_command(RARCH_CMD_LOAD_CONTENT);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   driver.menu->msg_force = true;
}

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.menu_content_directory,
         label,
         MENU_FILE_DIRECTORY,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   rarch_playlist_load_content(g_defaults.history,
         driver.menu->selection_ptr);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   return -1;
}

static int action_ok_push_history_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         "",
         label,
         type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_push_path_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         "",
         label,
         type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx)
{
   rarch_main_command(RARCH_CMD_SHADERS_APPLY_CHANGES);
   return 0;
}

// FIXME: Ugly hack, nees to be refactored badly
size_t hack_shader_pass = 0;

static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   if (!driver.menu)
      return -1;
   (void)menu_path;

#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(driver.menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(driver.menu->shader->pass[hack_shader_pass].source.path,
         menu_path, path,
         sizeof(driver.menu->shader->pass[hack_shader_pass].source.path));

   /* This will reset any changed parameters. */
   gfx_shader_resolve_parameters(NULL, driver.menu->shader);
   menu_list_flush_stack_by_needle(driver.menu->menu_list, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char shader_path[PATH_MAX];
   if (!driver.menu)
      return -1;

   (void)shader_path;
   (void)menu_path;
#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(driver.menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(shader_path, menu_path, path, sizeof(shader_path));
   menu_shader_manager_set_preset(driver.menu->shader,
         gfx_shader_parse_type(shader_path, RARCH_SHADER_NONE),
         shader_path);
   menu_list_flush_stack_by_needle(driver.menu->menu_list, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_key_start_line(driver.menu, "Preset Filename",
         label, st_string_callback);
   return 0;
}

static int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   setting = (rarch_setting_t*)
      setting_data_find_setting(driver.menu->list_settings, menu_label);

   if (!setting)
      return -1;

   if (setting->type == ST_DIR)
   {
      menu_action_setting_set_current_string(setting, menu_path);
      menu_list_pop_stack_by_needle(driver.menu->menu_list, setting->name);
   }

   return 0;
}

static int action_ok_core_load_deferred(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
   strlcpy(g_extern.fullpath, driver.menu->deferred_path,
         sizeof(g_extern.fullpath));

   rarch_main_command(RARCH_CMD_LOAD_CONTENT);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   driver.menu->msg_force = true;

   return -1;
}

static int action_ok_core_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(g_settings.libretro, menu_path, path,
         sizeof(g_settings.libretro));
   rarch_main_command(RARCH_CMD_LOAD_CORE);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
#if defined(HAVE_DYNAMIC)
   /* No content needed for this core, load core immediately. */
   if (driver.menu->load_no_content)
   {
      *g_extern.fullpath = '\0';
      rarch_main_command(RARCH_CMD_LOAD_CONTENT);
      menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
      driver.menu->msg_force = true;
      return -1;
   }

   return 0;
   /* Core selection on non-console just updates directory listing.
    * Will take effect on new content load. */
#elif defined(RARCH_CONSOLE)
   rarch_main_command(RARCH_CMD_RESTART_RETROARCH);
   return -1;
#endif
}

static int action_ok_compressed_archive_push(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   char cat_path[PATH_MAX];

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (!strcmp(menu_label, "detect_core_list"))
   {
      menu_list_push_stack(
            driver.menu->menu_list,
            path,
            "load_open_zip",
            0,
            driver.menu->selection_ptr);
      return 0;
   }

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         cat_path,
         menu_label,
         type,
         driver.menu->selection_ptr);

   return 0;
}

static int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   char cat_path[PATH_MAX];

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         cat_path,
         menu_label,
         type,
         driver.menu->selection_ptr);

   return 0;
}

static int action_ok_config_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path  = NULL;
   char config[PATH_MAX];

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(config, menu_path, path, sizeof(config));
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   driver.menu->msg_force = true;
   if (rarch_replace_config(config))
   {
      menu_navigation_clear(driver.menu, false);
      return -1;
   }

   return 0;
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   char image[PATH_MAX];

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(image, menu_path, path, sizeof(image));
   rarch_disk_control_append_image(image);

   rarch_main_command(RARCH_CMD_RESUME);

   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   return -1;
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   int ret;

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   ret = rarch_defer_core(g_extern.core_info,
         menu_path, path, driver.menu->deferred_path,
         sizeof(driver.menu->deferred_path));

   if (ret == -1)
   {
      rarch_main_command(RARCH_CMD_LOAD_CORE);
      common_load_content();
      return -1;
   }
   else if (ret == 0)
      menu_list_push_stack_refresh(
            driver.menu->menu_list,
            g_settings.libretro_directory,
            "deferred_core_list",
            0, driver.menu->selection_ptr);

   return ret;
}

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   menu_list_get_last(driver.menu->menu_list->menu_stack,
         &menu_path, &menu_label, NULL);

   setting = (rarch_setting_t*)
      setting_data_find_setting(driver.menu->list_settings, menu_label);

   if (setting && setting->type == ST_PATH)
   {
      menu_action_setting_set_current_string_path(setting, menu_path, path);
      menu_list_pop_stack_by_needle(driver.menu->menu_list, setting->name);
   }
   else
   {
      if (type == MENU_FILE_IN_CARCHIVE)
         fill_pathname_join_delim(g_extern.fullpath, menu_path, path,
               '#',sizeof(g_extern.fullpath));
      else
         fill_pathname_join(g_extern.fullpath, menu_path, path,
               sizeof(g_extern.fullpath));

      common_load_content();
      rarch_main_command(RARCH_CMD_LOAD_CONTENT_PERSIST);
      menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
      driver.menu->msg_force = true;

      return -1;
   }

   return 0;
}

static int action_ok_set_path(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   const char *menu_label   = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   setting = (rarch_setting_t*)
      setting_data_find_setting(driver.menu->list_settings, menu_label);

   if (!setting)
      return -1;

   menu_action_setting_set_current_string_path(setting, menu_path, path);
   menu_list_pop_stack_by_needle(driver.menu->menu_list, setting->name);

   return 0;
}

static int action_ok_bind_all(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   driver.menu->binds.target = &g_settings.input.binds
      [driver.menu->current_pad][0];
   driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
   driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;

   menu_list_push_stack(
         driver.menu->menu_list,
         "",
         "",
         driver.menu->bind_mode_keyboard ?
         MENU_SETTINGS_CUSTOM_BIND_KEYBOARD :
         MENU_SETTINGS_CUSTOM_BIND,
         driver.menu->selection_ptr);
   if (driver.menu->bind_mode_keyboard)
   {
      driver.menu->binds.timeout_end =
         rarch_get_time_usec() + 
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      input_keyboard_wait_keys(driver.menu,
            menu_custom_bind_keyboard_cb);
   }
   else
   {
      menu_poll_bind_get_rested_axes(&driver.menu->binds);
      menu_poll_bind_state(&driver.menu->binds);
   }
   return 0;
}

static int action_ok_bind_default_all(const char *path,
      const char *label, unsigned type, size_t idx)
{
   unsigned i;
   const struct retro_keybind *def_binds;
   struct retro_keybind *target = (struct retro_keybind*)
      &g_settings.input.binds[driver.menu->current_pad][0];

   def_binds = driver.menu->current_pad ? retro_keybinds_rest : retro_keybinds_1;

   driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
   driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;

   for (i = MENU_SETTINGS_BIND_BEGIN;
         i <= MENU_SETTINGS_BIND_LAST; i++, target++)
   {
      if (driver.menu->bind_mode_keyboard)
         target->key = def_binds[i - MENU_SETTINGS_BIND_BEGIN].key;
      else
      {
         target->joykey = NO_BTN;
         target->joyaxis = AXIS_NONE;
      }
   }
   return 0;
}

static int action_ok_custom_viewport(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_list_push_stack(
         driver.menu->menu_list,
         "",
         "",
         MENU_SETTINGS_CUSTOM_VIEWPORT,
         driver.menu->selection_ptr);

   /* Start with something sane. */
   rarch_viewport_t *custom = (rarch_viewport_t*)
      &g_extern.console.screen.viewports.custom_vp;

   if (driver.video_data && driver.video &&
         driver.video->viewport_info)
      driver.video->viewport_info(driver.video_data, custom);
   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;

   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   return 0;
}

static int action_ok_core_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *dir = g_settings.libretro_directory;

   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         dir, label, type,
         driver.menu->selection_ptr);

   return 0;
}

static int action_ok_disk_image_append_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *dir = g_settings.menu_content_directory;

   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         dir, label, type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_configurations_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *dir = g_settings.menu_config_directory;
   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         dir ? dir : label, label, type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         label, label, type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_disk_cycle_tray_status(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   rarch_main_command(RARCH_CMD_DISK_EJECT_TOGGLE);
   return 0;
}

static int action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack(
         driver.menu->menu_list,
         "",
         "help",
         0,
         0);
   driver.menu->push_start_screen = false;

   return 0;
}

static int action_start_performance_counters_core(unsigned type, const char *label,
      unsigned action)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   (void)label;

   if (counters[offset])
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_shader_action_parameter(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct gfx_shader *shader = NULL;
   struct gfx_shader_parameter *param = NULL;

   if (!(shader = (struct gfx_shader*)driver.menu->parameter_shader))
      return 0;

   if (!(param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0]))
      return 0;

   param->current = param->initial;

   param->current = min(max(param->minimum, param->current), param->maximum);

   if (!strcmp(label, "video_shader_parameters"))
      rarch_main_command(RARCH_CMD_SHADERS_APPLY_CHANGES);
#endif

   return 0;
}

static int shader_action_parameter_toggle(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   bool apply_changes = false;
   struct gfx_shader *shader = NULL;
   struct gfx_shader_parameter *param = NULL;

   if (!(shader = (struct gfx_shader*)driver.menu->parameter_shader))
      return 0;

   if (!(param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0]))
      return 0;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         param->current -= param->step;
         apply_changes = true;
         break;

      case MENU_ACTION_RIGHT:
         param->current += param->step;
         apply_changes = true;
         break;

      default:
         break;
   }

   param->current = min(max(param->minimum, param->current), param->maximum);

   if (apply_changes 
         && !strcmp(label, "video_shader_parameters"))
      rarch_main_command(RARCH_CMD_SHADERS_APPLY_CHANGES);

#endif
   return 0;
}

#ifdef HAVE_SHADER_MANAGER
extern size_t hack_shader_pass;
#endif

static int action_ok_shader_pass(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.video.shader_dir, 
         "video_shader_pass",
         type,
         driver.menu->selection_ptr);

   return 0;
}

static int action_start_shader_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   hack_shader_pass = type - MENU_SETTINGS_SHADER_PASS_0;
   struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
   struct gfx_shader_pass *shader_pass = NULL;

   if (shader)
      shader_pass = (struct gfx_shader_pass*)&shader->pass[hack_shader_pass];

   if (shader_pass)
      *shader_pass->source.path = '\0';
#endif

   return 0;
}

static int action_ok_shader_preset(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.video.shader_dir, 
         "video_shader_preset",
         type,
         driver.menu->selection_ptr);

   return 0;
}

static int action_start_shader_scale_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
   struct gfx_shader_pass *shader_pass = (struct gfx_shader_pass*)
      &shader->pass[pass];

   if (shader)
   {
      shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = 0;
      shader_pass->fbo.valid = false;
   }
#endif

   return 0;
}

static int action_toggle_shader_scale_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
   struct gfx_shader_pass *shader_pass = (struct gfx_shader_pass*)
      &shader->pass[pass];

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         {
            unsigned current_scale = shader_pass->fbo.scale_x;
            unsigned delta = action == MENU_ACTION_LEFT ? 5 : 1;
            current_scale = (current_scale + delta) % 6;

            if (shader_pass)
            {
               shader_pass->fbo.valid = current_scale;
               shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = current_scale;
            }
         }
         break;
   }
#endif
   return 0;
}

static int action_start_shader_filter_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
   struct gfx_shader_pass *shader_pass = (struct gfx_shader_pass*)
      &shader->pass[pass];

   if (shader && shader_pass)
      shader_pass->filter = RARCH_FILTER_UNSPEC;
#endif

   return 0;
}

static int action_toggle_shader_filter_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;
   struct gfx_shader_pass *shader_pass = (struct gfx_shader_pass*)
      &shader->pass[pass];

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         {
            unsigned delta = (action == MENU_ACTION_LEFT) ? 2 : 1;
            if (shader_pass)
               shader_pass->filter = ((shader_pass->filter + delta) % 3);
         }
         break;
   }
#endif
   return 0;
}

static int action_toggle_shader_filter_default(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   rarch_setting_t *current_setting = NULL;
   if ((current_setting = setting_data_find_setting(
               driver.menu->list_settings, "video_smooth")))
      setting_handler(current_setting, action);
#endif
   return 0;
}

static int action_start_shader_num_passes(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;

   if (!shader)
      return -1;

   if (shader && shader->passes)
      shader->passes = 0;
   driver.menu->need_refresh = true;

   gfx_shader_resolve_parameters(NULL, driver.menu->shader);
#endif
   return 0;
}

static int action_toggle_shader_num_passes(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct gfx_shader *shader = (struct gfx_shader*)driver.menu->shader;

   if (!shader)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (shader && shader->passes)
            shader->passes--;
         driver.menu->need_refresh = true;
         break;

      case MENU_ACTION_RIGHT:
         if (shader && (shader->passes < GFX_MAX_SHADERS))
            shader->passes++;
         driver.menu->need_refresh = true;
         break;
   }

   if (driver.menu->need_refresh)
      gfx_shader_resolve_parameters(NULL, driver.menu->shader);

#endif
   return 0;
}

static int action_ok_shader_parameters(const char *path,
      const char *label, unsigned type, size_t idx)
{
#ifdef HAVE_SHADER_MANAGER
   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         "",
         "video_shader_parameters",
         MENU_FILE_PUSH,
         driver.menu->selection_ptr);
#endif

   return 0;
}

static int action_start_input_bind_analog_dpad_mode(unsigned type, const char *label,
      unsigned action)
{
   unsigned port = 0;
   
   if (!driver.menu)
      return -1;

   port = driver.menu->current_pad;

   g_settings.input.analog_dpad_mode[port] = 0;

   return 0;
}

static int action_toggle_input_bind_analog_dpad_mode(unsigned type, const char *label,
      unsigned action)
{
   unsigned port = 0;
   
   if (!driver.menu)
      return -1;

   port = driver.menu->current_pad;

   switch (action)
   {
      case MENU_ACTION_RIGHT:
         g_settings.input.analog_dpad_mode[port] =
            (g_settings.input.analog_dpad_mode[port] + 1)
            % ANALOG_DPAD_LAST;
         break;

      case MENU_ACTION_LEFT:
         g_settings.input.analog_dpad_mode[port] =
            (g_settings.input.analog_dpad_mode
             [port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST;
         break;
   }

   return 0;
}

static int action_start_input_bind_device_type(unsigned type, const char *label,
      unsigned action)
{
   unsigned current_device, i, devices[128];
   const struct retro_controller_info *desc;
   unsigned types = 0, port = 0;

   if (!driver.menu)
      return -1;

   port = driver.menu->current_pad;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   /* Only push RETRO_DEVICE_ANALOG as default if we use an 
    * older core which doesn't use SET_CONTROLLER_INFO. */
   if (!g_extern.system.num_ports)
      devices[types++] = RETRO_DEVICE_ANALOG;

   desc = port < g_extern.system.num_ports ?
      &g_extern.system.ports[port] : NULL;
   if (desc)
   {
      for (i = 0; i < desc->num_types; i++)
      {
         unsigned id = desc->types[i].id;
         if (types < ARRAY_SIZE(devices) &&
               id != RETRO_DEVICE_NONE &&
               id != RETRO_DEVICE_JOYPAD)
            devices[types++] = id;
      }
   }

   current_device = g_settings.input.libretro_device[port];

   current_device = RETRO_DEVICE_JOYPAD;

   g_settings.input.libretro_device[port] = current_device;
   pretro_set_controller_port_device(port, current_device);

   return 0;
}

static int action_toggle_input_bind_device_type(unsigned type, const char *label,
      unsigned action)
{
   unsigned current_device, current_idx, i, devices[128];
   const struct retro_controller_info *desc;
   unsigned types = 0, port = 0;

   if (!driver.menu)
      return -1;

   port = driver.menu->current_pad;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   /* Only push RETRO_DEVICE_ANALOG as default if we use an 
    * older core which doesn't use SET_CONTROLLER_INFO. */
   if (!g_extern.system.num_ports)
      devices[types++] = RETRO_DEVICE_ANALOG;

   desc = port < g_extern.system.num_ports ?
      &g_extern.system.ports[port] : NULL;
   if (desc)
   {
      for (i = 0; i < desc->num_types; i++)
      {
         unsigned id = desc->types[i].id;
         if (types < ARRAY_SIZE(devices) &&
               id != RETRO_DEVICE_NONE &&
               id != RETRO_DEVICE_JOYPAD)
            devices[types++] = id;
      }
   }

   current_device = g_settings.input.libretro_device[port];
   current_idx = 0;
   for (i = 0; i < types; i++)
   {
      if (current_device == devices[i])
      {
         current_idx = i;
         break;
      }
   }

   switch (action)
   {
      case MENU_ACTION_LEFT:
         current_device = devices
            [(current_idx + types - 1) % types];

         g_settings.input.libretro_device[port] = current_device;
         pretro_set_controller_port_device(port, current_device);
         break;

      case MENU_ACTION_RIGHT:
         current_device = devices
            [(current_idx + 1) % types];

         g_settings.input.libretro_device[port] = current_device;
         pretro_set_controller_port_device(port, current_device);
         break;
   }

   return 0;
}

static int action_ok_video_resolution(const char *path,
      const char *label, unsigned type, size_t idx)
{
#ifdef GEKKO
   if (driver.video_data)
      gx_set_video_mode(driver.video_data, menu_gx_resolutions
            [menu_current_gx_resolution][0],
            menu_gx_resolutions[menu_current_gx_resolution][1]);
#elif defined(__CELLOS_LV2__)
   if (g_extern.console.screen.resolutions.list[
         g_extern.console.screen.resolutions.current.idx] == 
         CELL_VIDEO_OUT_RESOLUTION_576)
   {
      if (g_extern.console.screen.pal_enable)
         g_extern.console.screen.pal60_enable = true;
   }
   else
   {
      g_extern.console.screen.pal_enable = false;
      g_extern.console.screen.pal60_enable = false;
   }

   rarch_main_command(RARCH_CMD_REINIT);
#endif
   return 0;
}

static int action_toggle_video_resolution(unsigned type, const char *label,
      unsigned action)
{
#ifdef GEKKO
   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (menu_current_gx_resolution > 0)
            menu_current_gx_resolution--;
         break;
      case MENU_ACTION_RIGHT:
         if (menu_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
         {
#ifdef HW_RVL
            if ((menu_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
               if (CONF_GetVideo() != CONF_VIDEO_PAL)
                  return 0;
#endif

            menu_current_gx_resolution++;
         }
         break;
   }
#elif defined(__CELLOS_LV2__)
   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (g_extern.console.screen.resolutions.current.idx)
         {
            g_extern.console.screen.resolutions.current.idx--;
            g_extern.console.screen.resolutions.current.id =
               g_extern.console.screen.resolutions.list
               [g_extern.console.screen.resolutions.current.idx];
         }
         break;
      case MENU_ACTION_RIGHT:
         if (g_extern.console.screen.resolutions.current.idx + 1 <
               g_extern.console.screen.resolutions.count)
         {
            g_extern.console.screen.resolutions.current.idx++;
            g_extern.console.screen.resolutions.current.id =
               g_extern.console.screen.resolutions.list
               [g_extern.console.screen.resolutions.current.idx];
         }
         break;
   }
#endif

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

static int core_setting_toggle(unsigned type, const char *label,
      unsigned action)
{
   unsigned idx = type - MENU_SETTINGS_CORE_OPTION_START;

   (void)label;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         core_option_prev(g_extern.system.core_options, idx);
         break;

      case MENU_ACTION_RIGHT:
         core_option_next(g_extern.system.core_options, idx);
         break;
   }

   return 0;
}

static int disk_options_disk_idx_toggle(unsigned type, const char *label,
      unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
         rarch_main_command(RARCH_CMD_DISK_PREV);
         break;
      case MENU_ACTION_RIGHT:
         rarch_main_command(RARCH_CMD_DISK_NEXT);
         break;
   }

   return 0;
}

static int custom_bind_mode_toggle(unsigned type, const char *label,
      unsigned action)
{
   if (!driver.menu)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         driver.menu->bind_mode_keyboard = !driver.menu->bind_mode_keyboard;
         break;
   }

   return 0;
}

static int action_start_bind(unsigned type, const char *label,
      unsigned action)
{
   struct retro_keybind *def_binds = (struct retro_keybind *)retro_keybinds_1;
   struct retro_keybind *bind = (struct retro_keybind*)
      &g_settings.input.binds[driver.menu->current_pad]
      [type - MENU_SETTINGS_BIND_BEGIN];

   if (!bind)
      return -1;

   (void)label;
   (void)action;

   if (!driver.menu->bind_mode_keyboard)
   {
      bind->joykey = NO_BTN;
      bind->joyaxis = AXIS_NONE;
      return 0;
   }

   if (driver.menu->current_pad)
      def_binds = (struct retro_keybind*)retro_keybinds_rest;

   if (!def_binds)
      return -1;

   bind->key = def_binds[type - MENU_SETTINGS_BIND_BEGIN].key;

   return 0;
}

static int deferred_push_core_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int i;
   size_t list_size = 0;
   const core_info_t *info = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);
   core_info_list_get_supported_cores(g_extern.core_info,
         driver.menu->deferred_path, &info, &list_size);

   for (i = 0; i < list_size; i++)
   {
      menu_list_push(list, info[i].path, "",
            MENU_FILE_CORE, 0);
      menu_list_set_alt_at_offset(list, i,
            info[i].display_name);
   }

   menu_list_sort_on_alt(list);

   driver.menu->scroll_indices_size = 0;
   menu_entries_build_scroll_indices(list);
   menu_entries_refresh(list);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_core_information(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int i;
   core_info_t *info      = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   info = (core_info_t*)g_extern.core_info_current;
   menu_list_clear(list);

   if (info->data)
   {
      char tmp[PATH_MAX];

      snprintf(tmp, sizeof(tmp), "Core name: %s",
            info->display_name ? info->display_name : "");
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      if (info->authors_list)
      {
         strlcpy(tmp, "Authors: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->authors_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->permissions_list)
      {
         strlcpy(tmp, "Permissions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->permissions_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->licenses_list)
      {
         strlcpy(tmp, "License(s): ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->licenses_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->supported_extensions_list)
      {
         strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->supported_extensions_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->firmware_count > 0)
      {
         core_info_list_update_missing_firmware(
               g_extern.core_info, info->path,
               g_settings.system_directory);

         menu_list_push(list, "Firmware: ", "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
         for (i = 0; i < info->firmware_count; i++)
         {
            if (info->firmware[i].desc)
            {
               snprintf(tmp, sizeof(tmp), "	name: %s",
                     info->firmware[i].desc ? info->firmware[i].desc : "");
               menu_list_push(list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);

               snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                     info->firmware[i].missing ?
                     "missing" : "present",
                     info->firmware[i].optional ?
                     "optional" : "required");
               menu_list_push(list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);
            }
         }
      }

      if (info->notes)
      {
         snprintf(tmp, sizeof(tmp), "Core notes: ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         for (i = 0; i < info->note_list->size; i++)
         {
            snprintf(tmp, sizeof(tmp), " %s",
                  info->note_list->elems[i].data);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }
   }
   else
      menu_list_push(list,
            "No information available.", "",
            MENU_SETTINGS_CORE_OPTION_NONE, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_performance_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);
   menu_list_push(list, "Frontend Counters", "frontend_counters",
         MENU_FILE_PUSH, 0);
   menu_list_push(list, "Core Counters", "core_counters",
         MENU_FILE_PUSH, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static inline struct gfx_shader *shader_manager_get_current_shader(
      menu_handle_t *menu, const char *label, unsigned type)
{
   if (!strcmp(label, "video_shader_preset_parameters") ||
         !strcmp(label, "video_shader_parameters"))
      return menu->shader;
   else if (driver.video_poke && driver.video_data &&
         driver.video_poke->get_current_shader)
      return driver.video_poke->get_current_shader(driver.video_data);
   return NULL;
}

static int deferred_push_video_shader_preset_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   struct gfx_shader *shader = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   shader = (struct gfx_shader*)
      shader_manager_get_current_shader(driver.menu, label, type);

   if (shader)
      for (i = 0; i < shader->num_parameters; i++)
         menu_list_push(list,
               shader->parameters[i].desc, label,
               MENU_SETTINGS_SHADER_PARAMETER_0 + i, 0);
   driver.menu->parameter_shader = shader;

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_video_shader_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   struct gfx_shader *shader = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   shader = (struct gfx_shader*)
      shader_manager_get_current_shader(driver.menu, label, type);

   if (shader)
      for (i = 0; i < shader->num_parameters; i++)
         menu_list_push(list,
               shader->parameters[i].desc, label,
               MENU_SETTINGS_SHADER_PARAMETER_0 + i, 0);
   driver.menu->parameter_shader = shader;

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_settings(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   settings_list_free(driver.menu->list_settings);
   driver.menu->list_settings = (rarch_setting_t *)setting_data_new(SL_FLAG_ALL_SETTINGS);
   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(driver.menu->list_settings,
         "Driver Options");

   menu_list_clear(list);

   for (; setting->type != ST_NONE; setting++)
   {
      if (setting->type == ST_GROUP)
         menu_list_push(list, setting->short_description,
               setting->name, setting_set_flags(setting), 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_category(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   settings_list_free(driver.menu->list_settings);
   driver.menu->list_settings = (rarch_setting_t *)setting_data_new(SL_FLAG_ALL_SETTINGS);
   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
         driver.menu->list_settings, label);

   menu_list_clear(list);

   if (!strcmp(label, "Video Options"))
   {
#if defined(GEKKO) || defined(__CELLOS_LV2__)
      menu_list_push(list, "Screen Resolution", "",
            MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
      menu_list_push(list, "Custom Ratio", "",
            MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
   }

   for (; setting->type != ST_END_GROUP; setting++)
   {
      if (
            setting->type == ST_GROUP ||
            setting->type == ST_SUB_GROUP ||
            setting->type == ST_END_SUB_GROUP
         )
         continue;

      menu_list_push(list, setting->short_description,
            setting->name, setting_set_flags(setting), 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_input_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   settings_list_free(driver.menu->list_settings);
   driver.menu->list_settings = (rarch_setting_t *)setting_data_new(SL_FLAG_ALL_SETTINGS);

   menu_list_clear(list);
   menu_list_push(list, "Device Type", "input_bind_device_type", 0, 0);
   menu_list_push(list, "Analog D-pad Mode", "input_bind_analog_dpad_mode", 0, 0);
   menu_list_push(list, "Bind Mode", "",
         MENU_SETTINGS_CUSTOM_BIND_MODE, 0);
   menu_list_push(list, "Configure All (RetroPad)", "",
         MENU_SETTINGS_CUSTOM_BIND_ALL, 0);
   menu_list_push(list, "Default All (RetroPad)", "",
         MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_shader_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int i;
   struct gfx_shader *shader = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   shader = (struct gfx_shader*)driver.menu->shader;

   if (!shader)
      return -1;

   menu_list_clear(list);
   menu_list_push(list, "Apply Shader Changes", "shader_apply_changes",
         MENU_FILE_PUSH, 0);
   menu_list_push(list, "Default Filter", "video_shader_default_filter",
         0, 0);
   menu_list_push(list, "Load Shader Preset", "video_shader_preset",
         MENU_FILE_PUSH, 0);
   menu_list_push(list, "Shader Preset Save As",
         "video_shader_preset_save_as", MENU_FILE_LINEFEED_SWITCH, 0);
   menu_list_push(list, "Parameters (Current)",
         "video_shader_parameters", MENU_FILE_PUSH, 0);
   menu_list_push(list, "Parameters (Menu)",
         "video_shader_preset_parameters", MENU_FILE_PUSH, 0);
   menu_list_push(list, "Shader Passes", "video_shader_num_passes",
         0, 0);

   for (i = 0; i < shader->passes; i++)
   {
      char buf[64];

      snprintf(buf, sizeof(buf), "Shader #%u", i);
      menu_list_push(list, buf, "video_shader_pass",
            MENU_SETTINGS_SHADER_PASS_0 + i, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
      menu_list_push(list, buf, "video_shader_filter_pass",
            MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
      menu_list_push(list, buf, "video_shader_scale_pass",
            MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static void push_perfcounter(menu_handle_t *menu,
      file_list_t *list,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
      return;

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_list_push(list, counters[i]->ident, "",
               id + i, 0);
}

static int deferred_push_core_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);
   push_perfcounter(driver.menu, list, perf_counters_libretro,
         perf_ptr_libretro, MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_frontend_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;

   if (!list)
      return -1;

   menu_list_clear(list);
   push_perfcounter(driver.menu, list, perf_counters_rarch,
         perf_ptr_rarch, MENU_SETTINGS_PERF_COUNTERS_BEGIN);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_core_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int i;
   file_list_t *list      = (file_list_t*)data;

   if (!list)
      return -1;

   menu_list_clear(list);
   if (g_extern.system.core_options)
   {
      size_t opts = core_option_size(g_extern.system.core_options);
      for (i = 0; i < opts; i++)
         menu_list_push(list,
               core_option_get_desc(g_extern.system.core_options, i), "",
               MENU_SETTINGS_CORE_OPTION_START + i, 0);
   }
   else
      menu_list_push(list, "No options available.", "",
               MENU_SETTINGS_CORE_OPTION_NONE, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_disk_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;

   if (!list)
      return -1;

   menu_list_clear(list);
   menu_list_push(list, "Disk Index", "disk_idx",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0);
   menu_list_push(list, "Disk Cycle Tray Status", "disk_cycle_tray_status",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0);
   menu_list_push(list, "Disk Image Append", "disk_image_append",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_PLAIN, EXT_EXECUTABLES);

   return 0;
}

static int deferred_push_history_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   size_t list_size = 0;
   file_list_t *list      = (file_list_t*)data;

   if (!list || !driver.menu)
      return -1;

   menu_list_clear(list);
   list_size = content_playlist_size(g_defaults.history);

   for (i = 0; i < list_size; i++)
   {
      char fill_buf[PATH_MAX];
      const char *core_name = NULL;

      content_playlist_get_index(g_defaults.history, i,
            &path, NULL, &core_name);
      strlcpy(fill_buf, core_name, sizeof(fill_buf));

      if (path)
      {
         char path_short[PATH_MAX];
         fill_short_pathname_representation(path_short,path,sizeof(path_short));
         snprintf(fill_buf,sizeof(fill_buf),"%s (%s)",
               path_short,core_name);
      }

      menu_list_push(list, fill_buf, "",
            MENU_FILE_PLAYLIST_ENTRY, 0);
   }

   driver.menu->scroll_indices_size = 0;
   menu_entries_build_scroll_indices(list);
   menu_entries_refresh(list);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_configurations(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_CONFIG, "cfg");

   return 0;
}

static int deferred_push_video_shader_preset(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_SHADER_PRESET, "cgp|glslp");

   return 0;
}

static int deferred_push_video_shader_pass(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_SHADER, "cg|glsl");

   return 0;
}

static int deferred_push_video_filter(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_VIDEOFILTER, "filt");

   return 0;
}

static int deferred_push_audio_dsp_plugin(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_AUDIOFILTER, "dsp");

   return 0;
}

static int deferred_push_input_overlay(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_OVERLAY, "cfg");

   return 0;
}

static int deferred_push_video_font_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_FONT, "ttf");

   return 0;
}

static int deferred_push_content_history_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_PLAIN, "cfg");

   return 0;
}

static int deferred_push_detect_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   const char *exts = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   exts = g_extern.core_info ? core_info_list_get_all_extensions(
         g_extern.core_info) : "";

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_PLAIN, exts);

   return 0;
}

static int deferred_push_default(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   char ext_buf[PATH_MAX];
   const char *exts = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   if (menu_common_type_is(label, type) == MENU_FILE_DIRECTORY)
      exts = ""; /* we ignore files anyway */
   else if (g_extern.menu.info.valid_extensions)
   {
      exts = ext_buf;
      if (*g_extern.menu.info.valid_extensions)
         snprintf(ext_buf, sizeof(ext_buf), "%s",
               g_extern.menu.info.valid_extensions);
      else
         *ext_buf = '\0';
   }
   else
      exts = g_extern.system.valid_extensions;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_PLAIN, exts);

   return 0;
}

/* Bind the OK callback function */

static int menu_entries_cbs_init_bind_ok_first(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   const char *menu_label = NULL;

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         NULL, &menu_label, NULL);

   switch (type)
   {
      case MENU_SETTINGS_VIDEO_RESOLUTION:
         cbs->action_ok = action_ok_video_resolution;
         break;
      case MENU_FILE_PLAYLIST_ENTRY:
         cbs->action_ok = action_ok_playlist_entry;
         break;
      case MENU_FILE_SHADER_PRESET:
         cbs->action_ok = action_ok_shader_preset_load;
         break;
      case MENU_FILE_SHADER:
         cbs->action_ok = action_ok_shader_pass_load;
         break;
      case MENU_FILE_USE_DIRECTORY:
         cbs->action_ok = action_ok_path_use_directory;
         break;
      case MENU_FILE_CONFIG:
         cbs->action_ok = action_ok_config_load;
         break;
      case MENU_FILE_DIRECTORY:
         cbs->action_ok = action_ok_directory_push;
         break;
      case MENU_FILE_CARCHIVE:
         cbs->action_ok = action_ok_compressed_archive_push;
         break;
      case MENU_FILE_CORE:
         if (!strcmp(menu_label, "deferred_core_list"))
            cbs->action_ok = action_ok_core_load_deferred;
         else if (!strcmp(menu_label, "core_list"))
            cbs->action_ok = action_ok_core_load;
         else
            return -1;
         break;
      case MENU_FILE_FONT:
      case MENU_FILE_OVERLAY:
      case MENU_FILE_AUDIOFILTER:
      case MENU_FILE_VIDEOFILTER:
         cbs->action_ok = action_ok_set_path;
         break;
#ifdef HAVE_COMPRESSION
      case MENU_FILE_IN_CARCHIVE:
#endif
      case MENU_FILE_PLAIN:
         if (!strcmp(menu_label, "detect_core_list"))
            cbs->action_ok = action_ok_file_load_with_detect_core;
         else if (!strcmp(menu_label, "disk_image_append"))
            cbs->action_ok = action_ok_disk_image_append;
         else
            cbs->action_ok = action_ok_file_load;
         break;
      case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
         cbs->action_ok = action_ok_bind_default_all;
         break;
      case MENU_SETTINGS_CUSTOM_BIND_ALL:
         cbs->action_ok = action_ok_bind_all;
         break;
      case MENU_SETTINGS_CUSTOM_VIEWPORT:
         cbs->action_ok = action_ok_custom_viewport;
         break;
      case MENU_SETTINGS:
      case MENU_FILE_CATEGORY:
         cbs->action_ok = action_ok_push_default;
         break;
      case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
         cbs->action_ok = action_ok_disk_cycle_tray_status;
      default:
         return -1;
   }

   return 0;
}

static void menu_entries_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return;

   cbs->action_start = NULL;

   if (!strcmp(label, "video_shader_pass"))
      cbs->action_start = action_start_shader_pass;
   else if (!strcmp(label, "video_shader_scale_pass"))
      cbs->action_start = action_start_shader_scale_pass;
   else if (!strcmp(label, "video_shader_filter_pass"))
      cbs->action_start = action_start_shader_filter_pass;
   else if (!strcmp(label, "video_shader_num_passes"))
      cbs->action_start = action_start_shader_num_passes;
   else if (!strcmp(label, "input_bind_analog_dpad_mode"))
      cbs->action_start = action_start_input_bind_analog_dpad_mode;
   else if (!strcmp(label, "input_bind_device_type"))
      cbs->action_start = action_start_input_bind_device_type;
   else if (type >= MENU_SETTINGS_BIND_BEGIN &&
         type <= MENU_SETTINGS_BIND_ALL_LAST)
      cbs->action_start = action_start_bind;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_start = action_start_shader_action_parameter;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_core;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_frontend;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_start = action_start_core_setting;
}

static void menu_entries_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return;

   cbs->action_ok = NULL;

   if (menu_entries_cbs_init_bind_ok_first(cbs, path, label, type, idx) == 0)
      return;
   else if (!strcmp(label, "help"))
      cbs->action_ok = action_ok_help;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_ok = action_ok_shader_pass;
   else if (!strcmp(label, "video_shader_preset"))
      cbs->action_ok = action_ok_shader_preset;
   else if ((!strcmp(label, "video_shader_parameters") ||
            !strcmp(label, "video_shader_preset_parameters")))
      cbs->action_ok = action_ok_shader_parameters;
   else if (
         !strcmp(label, "Shader Options") ||
         !strcmp(label, "Input Options") ||
         !strcmp(label, "core_options") ||
         !strcmp(label, "core_information") ||
         !strcmp(label, "video_shader_parameters") ||
         !strcmp(label, "video_shader_preset_parameters") ||
         !strcmp(label, "disk_options") ||
         !strcmp(label, "settings") ||
         !strcmp(label, "performance_counters") ||
         !strcmp(label, "frontend_counters") ||
         !strcmp(label, "core_counters")
         )
      cbs->action_ok = action_ok_push_default;
   else if (
         !strcmp(label, "load_content") ||
         !strcmp(label, "detect_core_list")
         )
      cbs->action_ok = action_ok_push_content_list;
   else if (!strcmp(label, "history_list"))
      cbs->action_ok = action_ok_push_history_list;
   else if (menu_common_type_is(label, type) == MENU_FILE_DIRECTORY)
      cbs->action_ok = action_ok_push_path_list;
   else if (!strcmp(label, "shader_apply_changes"))
      cbs->action_ok = action_ok_shader_apply_changes;
   else if (!strcmp(label, "video_shader_preset_save_as"))
      cbs->action_ok = action_ok_shader_preset_save_as;
   else if (!strcmp(label, "core_list"))
      cbs->action_ok = action_ok_core_list;
   else if (!strcmp(label, "disk_image_append"))
      cbs->action_ok = action_ok_disk_image_append_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_ok = action_ok_configurations_list;
}

static void menu_entries_cbs_init_bind_toggle(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return;

   cbs->action_toggle = menu_action_setting_set;

   if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_toggle = shader_action_parameter_toggle;
   else if (!strcmp(label, "video_shader_scale_pass"))
      cbs->action_toggle = action_toggle_shader_scale_pass;
   else if (!strcmp(label, "video_shader_filter_pass"))
      cbs->action_toggle = action_toggle_shader_filter_pass;
   else if (!strcmp(label, "video_shader_default_filter"))
      cbs->action_toggle = action_toggle_shader_filter_default;
   else if (!strcmp(label, "video_shader_num_passes"))
      cbs->action_toggle = action_toggle_shader_num_passes;
   else if (!strcmp(label, "shader_apply_changes"))
      cbs->action_toggle = menu_action_setting_set;
   else if (!strcmp(label, "input_bind_analog_dpad_mode"))
      cbs->action_toggle = action_toggle_input_bind_analog_dpad_mode;
   else if (!strcmp(label, "input_bind_device_type"))
      cbs->action_toggle = action_toggle_input_bind_device_type;
   else if (type == MENU_SETTINGS_VIDEO_RESOLUTION)
      cbs->action_toggle = action_toggle_video_resolution;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_toggle = core_setting_toggle;

   switch (type)
   {
      case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
         cbs->action_toggle = disk_options_disk_idx_toggle;
         break;
      case MENU_SETTINGS_CUSTOM_BIND_MODE:
         cbs->action_toggle = custom_bind_mode_toggle;
         break;
   }

}

static void menu_entries_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   const char *menu_label = NULL;
   if (!cbs || !driver.menu)
      return;

   menu_list_get_last_stack(driver.menu->menu_list,
         NULL, &menu_label, NULL);

   cbs->action_deferred_push = deferred_push_default;

   if (!strcmp(label, "history_list"))
      cbs->action_deferred_push = deferred_push_history_list;
   else if (type == MENU_FILE_CATEGORY)
      cbs->action_deferred_push = deferred_push_category;
   else if (!strcmp(label, "deferred_core_list"))
      cbs->action_deferred_push = deferred_push_core_list_deferred;
   else if (!strcmp(label, "Input Options"))
      cbs->action_deferred_push = deferred_push_input_options;
   else if (!strcmp(label, "Shader Options"))
      cbs->action_deferred_push = deferred_push_shader_options;
   else if (!strcmp(label, "core_information"))
      cbs->action_deferred_push = deferred_push_core_information;
   else if (!strcmp(label, "performance_counters"))
      cbs->action_deferred_push = deferred_push_performance_counters;
   else if (!strcmp(label, "core_counters"))
      cbs->action_deferred_push = deferred_push_core_counters;
   else if (!strcmp(label, "video_shader_preset_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_preset_parameters;
   else if (!strcmp(label, "video_shader_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_parameters;
   else if (!strcmp(label, "settings"))
      cbs->action_deferred_push = deferred_push_settings;
   else if (!strcmp(label, "frontend_counters"))
      cbs->action_deferred_push = deferred_push_frontend_counters;
   else if (!strcmp(label, "core_options"))
      cbs->action_deferred_push = deferred_push_core_options;
   else if (!strcmp(label, "disk_options"))
      cbs->action_deferred_push = deferred_push_disk_options;
   else if (!strcmp(label, "core_list"))
      cbs->action_deferred_push = deferred_push_core_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_deferred_push = deferred_push_configurations;
   else if (!strcmp(label, "video_shader_preset"))
      cbs->action_deferred_push = deferred_push_video_shader_preset;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_deferred_push = deferred_push_video_shader_pass;
   else if (!strcmp(label, "video_filter"))
      cbs->action_deferred_push = deferred_push_video_filter;
   else if (!strcmp(label, "audio_dsp_plugin"))
      cbs->action_deferred_push = deferred_push_audio_dsp_plugin;
   else if (!strcmp(label, "input_overlay"))
      cbs->action_deferred_push = deferred_push_input_overlay;
   else if (!strcmp(label, "video_font_path"))
      cbs->action_deferred_push = deferred_push_video_font_path;
   else if (!strcmp(label, "game_history_path"))
      cbs->action_deferred_push = deferred_push_content_history_path;
   else if (!strcmp(label, "detect_core_list"))
      cbs->action_deferred_push = deferred_push_detect_core_list;
}

void menu_entries_cbs_init(void *data,
      const char *path, const char *label,
      unsigned type, size_t idx)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   cbs = (menu_file_list_cbs_t*)list->list[idx].actiondata;

   if (cbs)
   {
      menu_entries_cbs_init_bind_ok(cbs, path, label, type, idx);
      menu_entries_cbs_init_bind_start(cbs, path, label, type, idx);
      menu_entries_cbs_init_bind_toggle(cbs, path, label, type, idx);
      menu_entries_cbs_init_bind_deferred_push(cbs, path, label, type, idx);
   }
}
