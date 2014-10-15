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
#include "menu_shader.h"


static int menu_action_setting_apply(rarch_setting_t *setting)
{
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

int menu_action_setting_boolean(
      rarch_setting_t *setting, unsigned action)
{
   if (setting->action_ok)
      setting->action_ok(setting, action);

   return menu_action_setting_apply(setting);
}

int menu_action_setting_unsigned_integer(
      rarch_setting_t *setting, unsigned action)
{
   if (setting->action_ok)
      setting->action_ok(setting, action);

   return menu_action_setting_apply(setting);
}

int menu_action_setting_fraction(
      rarch_setting_t *setting, unsigned action)
{
   if (setting->action_ok)
      setting->action_ok(setting, action);

   return menu_action_setting_apply(setting);
}

void menu_action_setting_driver(
      rarch_setting_t *setting, unsigned action)
{
   if (!strcmp(setting->name, "audio_resampler_driver"))
   {
      switch (action)
      {
         case MENU_ACTION_LEFT:
            find_prev_resampler_driver();
            break;
         case MENU_ACTION_RIGHT:
            find_next_resampler_driver();
            break;
      }
   }
   else if (setting->flags & SD_FLAG_IS_DRIVER)
   {
      const char *label    = setting->name;
      char *drv            = (char*)setting->value.string;
      size_t sizeof_driver = setting->size;

      switch (action)
      {
         case MENU_ACTION_LEFT:
            find_prev_driver(label, drv, sizeof_driver);
            break;
         case MENU_ACTION_RIGHT:
            find_next_driver(label, drv, sizeof_driver);
            break;
      }
   }
}

int menu_action_setting_set_current_string(
      rarch_setting_t *setting, const char *str)
{
   strlcpy(setting->value.string, str, setting->size);

   return menu_action_setting_apply(setting);
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

   return menu_action_setting_apply(setting);
}

static int menu_entries_set_current_path_selection(
      rarch_setting_t *setting, const char *start_path,
      const char *label, unsigned type,
      unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_OK:
         menu_entries_push(driver.menu->menu_stack,
               start_path, label, type,
               driver.menu->selection_ptr);

         if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
            setting->cmd_trigger.triggered = true;
         break;
      case MENU_ACTION_START:
         *setting->value.string = '\0';
         break;
   }

   return menu_action_setting_apply(setting);
}

static int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action)
{
   if (!setting)
      return -1;

   if (setting->type == ST_BOOL)
      return menu_action_setting_boolean(setting, action);
   if (setting->type == ST_UINT)
      return menu_action_setting_unsigned_integer(setting, action);
   if (setting->type == ST_FLOAT)
      return menu_action_setting_fraction(setting, action);
   if (setting->type == ST_PATH)
      return menu_entries_set_current_path_selection(setting,
            setting->default_value.string, setting->name, type, action);

   if (setting->type == ST_DIR)
   {
      if (action == MENU_ACTION_START)
      {
         *setting->value.string = '\0';
         return menu_action_setting_apply(setting);
      }
      return 0;
   }

   if (setting->type == ST_STRING)
   {
      if (setting->flags & SD_FLAG_ALLOW_INPUT)
      {
         switch (action)
         {
            case MENU_ACTION_OK:
               menu_key_start_line(driver.menu, setting->short_description,
                     setting->name, st_string_callback);
               break;
            case MENU_ACTION_START:
               *setting->value.string = '\0';
               break;
         }
      }
      else
         menu_action_setting_driver(setting, action);
   }

   return 0;
}

int menu_action_setting_set(unsigned type, const char *label,
      unsigned action)
{
   const file_list_t *list = (const file_list_t*)driver.menu->selection_buf;

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
