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

#include "menu_action.h"
#include "menu_common.h"
#include "menu_input_line_cb.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "backend/menu_backend.h"

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         g_settings.menu_content_directory, label, MENU_FILE_DIRECTORY,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   rarch_playlist_load_content(g_defaults.history,
         driver.menu->selection_ptr);
   menu_flush_stack_type(driver.menu->menu_stack, MENU_SETTINGS);
   return -1;
}

static int action_ok_push_history_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         "", label, type, driver.menu->selection_ptr);
   menu_clear_navigation(driver.menu);
   menu_entries_push_list(driver.menu, driver.menu->selection_buf, 
         path, label, type);
   return 0;
}

static int action_ok_push_path_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         "", label, type, driver.menu->selection_ptr);
   return 0;
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t index)
{
   rarch_main_command(RARCH_CMD_SHADERS_APPLY_CHANGES);
   return 0;
}

// FIXME: Ugly hack, nees to be refactored badly
size_t hack_shader_pass = 0;

static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path = NULL;
   if (!driver.menu)
      return -1;
   (void)menu_path;

#ifdef HAVE_SHADER_MANAGER
   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL,
         NULL);
   fill_pathname_join(driver.menu->shader->pass[hack_shader_pass].source.path,
         menu_path, path,
         sizeof(driver.menu->shader->pass[hack_shader_pass].source.path));

   /* This will reset any changed parameters. */
   gfx_shader_resolve_parameters(NULL, driver.menu->shader);
   menu_flush_stack_label(driver.menu->menu_stack, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path = NULL;
   char shader_path[PATH_MAX];
   if (!driver.menu)
      return -1;

   (void)shader_path;
   (void)menu_path;
#ifdef HAVE_SHADER_MANAGER
   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL,
         NULL);
   fill_pathname_join(shader_path, menu_path, path, sizeof(shader_path));
   menu_shader_manager_set_preset(driver.menu->shader,
         gfx_shader_parse_type(shader_path, RARCH_SHADER_NONE),
         shader_path);
   menu_flush_stack_label(driver.menu->menu_stack, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_key_start_line(driver.menu, "Preset Filename",
         label, st_string_callback);
   return 0;
}

static int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, &menu_label, NULL);
   setting = (rarch_setting_t*)
      setting_data_find_setting(driver.menu->list_settings, menu_label);

   if (!setting)
      return -1;

   if (setting->type == ST_DIR)
   {
      menu_action_setting_set_current_string(setting, menu_path);
      menu_entries_pop_stack(driver.menu->menu_stack, setting->name);
   }

   return 0;
}

static int action_ok_core_load_deferred(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
   strlcpy(g_extern.fullpath, driver.menu->deferred_path,
         sizeof(g_extern.fullpath));

   rarch_main_command(RARCH_CMD_LOAD_CONTENT);
   menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
   driver.menu->msg_force = true;

   return -1;
}

static int action_ok_core_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path    = NULL;
   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL, NULL);

   fill_pathname_join(g_settings.libretro, menu_path, path,
         sizeof(g_settings.libretro));
   rarch_main_command(RARCH_CMD_LOAD_CORE);
   menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
#if defined(HAVE_DYNAMIC)
   /* No content needed for this core, load core immediately. */
   if (driver.menu->load_no_content)
   {
      *g_extern.fullpath = '\0';
      rarch_main_command(RARCH_CMD_LOAD_CONTENT);
      menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
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

/* Bind the OK callback function */

static int menu_entries_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   const char *menu_label = NULL;

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, NULL, &menu_label, NULL);

   if (type == MENU_FILE_PLAYLIST_ENTRY)
      cbs->action_ok = action_ok_playlist_entry;
   else if (type == MENU_FILE_SHADER_PRESET)
      cbs->action_ok = action_ok_shader_preset_load;
   else if (type == MENU_FILE_SHADER)
      cbs->action_ok = action_ok_shader_pass_load;
   else if (type == MENU_FILE_USE_DIRECTORY)
      cbs->action_ok = action_ok_path_use_directory;
   else if (type == MENU_FILE_CORE)
   {
      if (!strcmp(menu_label, "deferred_core_list"))
         cbs->action_ok = action_ok_core_load_deferred;
      else if (!strcmp(menu_label, "core_list"))
         cbs->action_ok = action_ok_core_load;
      else
         return -1;
   }
   else
      return -1;

   return 0;
}

static void menu_entries_cbs_init_bind_ok_toggle(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   if (!cbs)
      return;

   cbs->action_ok = NULL;

   if (menu_entries_cbs_init_bind_ok(cbs, path, label, type, index) == 0)
      return;
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
}

void menu_entries_cbs_init(void *data,
      const char *path, const char *label,
      unsigned type, size_t index)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   cbs = (menu_file_list_cbs_t*)list->list[index].actiondata;

   if (cbs)
   {
      menu_entries_cbs_init_bind_ok_toggle(cbs, path, label, type, index);
   }
}
