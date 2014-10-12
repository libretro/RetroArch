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

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_key_start_line(driver.menu, "Preset Filename",
         label, st_string_callback);
   return 0;
}

/* Bind the OK callback function */

static int menu_entries_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   if (type == MENU_FILE_PLAYLIST_ENTRY)
      cbs->action_ok = action_ok_playlist_entry;
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
