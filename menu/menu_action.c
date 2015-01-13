/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "menu_action.h"
#include "menu_entries.h"
#include "../retroarch.h"

static int setting_extra_handler(rarch_setting_t *setting)
{
   if (!setting)
      return -1;

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
      setting->cmd_trigger.triggered = true;

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

static void process_setting_handler(
      rarch_setting_t *setting,
      unsigned action)
{
   if (!setting)
      return;

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
      case MENU_ACTION_CANCEL:
         if (setting->action_cancel)
            setting->action_cancel(setting, action);
         break;
      case MENU_ACTION_START:
         if (setting->action_start)
            setting->action_start(setting);
         break;
   }
}

int menu_action_setting_handler(
      rarch_setting_t *setting, unsigned action)
{
   process_setting_handler(setting, action);

   return setting_extra_handler(setting);
}

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action)
{
   if (!setting)
      return -1;

   switch (setting->type)
   {
      case ST_PATH:
         if (action == MENU_ACTION_OK)
            menu_list_push_stack_refresh(
                  driver.menu->menu_list,
                  setting->default_value.string,
                  setting->name,
                  type,
                  driver.menu->selection_ptr);
         /* fall-through. */
      case ST_BOOL:
      case ST_INT:
      case ST_UINT:
      case ST_FLOAT:
      case ST_STRING:
      case ST_DIR:
      case ST_BIND:
      case ST_ACTION:
         process_setting_handler(setting, action);
         break;
      default:
         break;
   }

   return setting_extra_handler(setting);
}

static rarch_setting_t *find_setting(void)
{
   const file_list_t *list = (const file_list_t*)driver.menu->menu_list->selection_buf;

   /* Check if setting belongs to settings menu. */

   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
         driver.menu->list_settings, list->list[driver.menu->selection_ptr].label);

   /* Check if setting belongs to main menu. */
   if (!setting)
      setting = (rarch_setting_t*)setting_data_find_setting(
            driver.menu->list_mainmenu, list->list[driver.menu->selection_ptr].label);

   return setting;
}

int menu_action_setting_set(unsigned type, const char *label,
      unsigned action)
{
   rarch_setting_t *setting = find_setting();

   if (setting)
      return menu_action_handle_setting(setting, type, action);

   return 0;
}
