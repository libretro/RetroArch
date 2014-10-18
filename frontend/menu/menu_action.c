/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include "menu_action.h"
#include "menu_entries.h"
#include "menu_list.h"
#include "menu_shader.h"

int setting_handler(
      rarch_setting_t *setting, unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         if (setting->action_toggle)
            setting->action_toggle(setting, action);
         break;
      case MENU_ACTION_OK:
         if (setting->action_ok)
            setting->action_ok(setting, action);
         break;
      case MENU_ACTION_START:
         if (setting->action_start)
            setting->action_start(setting);
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   if (setting->flags & SD_FLAG_EXIT
         && setting->cmd_trigger.triggered)
   {
      setting->cmd_trigger.triggered = false;
      return -1;
   }

   return 0;
}

int menu_action_setting_set_current_string(
      rarch_setting_t *setting, const char *str)
{
   strlcpy(setting->value.string, str, setting->size);

   if (setting->change_handler)
      setting->change_handler(setting);

   if (setting->flags & SD_FLAG_EXIT
         && setting->cmd_trigger.triggered)
   {
      setting->cmd_trigger.triggered = false;
      return -1;
   }

   return 0;
}

int menu_action_set_current_string_based_on_label(
      const char *label, const char *str)
{
   if (!strcmp(label, "video_shader_preset_save_as"))
      menu_shader_manager_save_preset(str, false);

   return 0;
}

int menu_action_setting_set_current_string_path(
      rarch_setting_t *setting, const char *dir, const char *path)
{
   fill_pathname_join(setting->value.string, dir, path, setting->size);

   if (setting->change_handler)
      setting->change_handler(setting);

   if (setting->flags & SD_FLAG_EXIT
         && setting->cmd_trigger.triggered)
   {
      setting->cmd_trigger.triggered = false;
      return -1;
   }

   return 0;
}

static int menu_entries_set_current_path_selection(
      rarch_setting_t *setting, const char *start_path,
      const char *label, unsigned type,
      unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_OK:
         menu_list_push_stack_refresh(
               driver.menu->menu_list,
               start_path,
               label,
               type,
               driver.menu->selection_ptr);

         if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
            setting->cmd_trigger.triggered = true;
         break;
      case MENU_ACTION_START:
         if (setting->action_start)
            setting->action_start(setting);
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   if (setting->flags & SD_FLAG_EXIT
         && setting->cmd_trigger.triggered)
   {
      setting->cmd_trigger.triggered = false;
      return -1;
   }

   return 0;
}

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action)
{
   if (!setting)
      return -1;

   switch (setting->type)
   {
      case ST_BOOL:
      case ST_INT:
      case ST_UINT:
      case ST_FLOAT:
      case ST_STRING:
      case ST_DIR:
      case ST_BIND:
         return setting_handler(setting, action);
      case ST_PATH:
         return menu_entries_set_current_path_selection(setting,
               setting->default_value.string, setting->name, type, action);
      default:
         break;
   }

   return 0;
}

int menu_action_setting_set(unsigned type, const char *label,
      unsigned action)
{
   const file_list_t *list = (const file_list_t*)driver.menu->menu_list->selection_buf;

   /* Check if setting belongs to settings menu. */

   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
         driver.menu->list_settings, list->list[driver.menu->selection_ptr].label);

   if (setting)
      return menu_action_handle_setting(setting, type, action);

   /* Check if setting belongs to main menu. */

   setting = (rarch_setting_t*)setting_data_find_setting(
         driver.menu->list_mainmenu, list->list[driver.menu->selection_ptr].label);

   if (setting)
      return menu_action_handle_setting(setting, type, action);

   return 0;
}
