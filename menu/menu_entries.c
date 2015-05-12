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

#include "menu_displaylist.h"
#include "menu_entries.h"
#include "menu_setting.h"
#include "menu_navigation.h"
#include <file/file_list.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <file/dir_list.h>

int menu_entries_setting_set_flags(rarch_setting_t *setting)
{
   if (!setting)
      return 0;

   if (setting->flags & SD_FLAG_IS_DRIVER)
      return MENU_SETTING_DRIVER;

   switch (setting->type)
   {
      case ST_ACTION:
         return MENU_SETTING_ACTION;
      case ST_PATH:
         return MENU_FILE_PATH;
      case ST_GROUP:
         return MENU_SETTING_GROUP;
      case ST_SUB_GROUP:
         return MENU_SETTING_SUBGROUP;
      default:
         break;
   }

   return 0;
}

static void menu_entries_content_list_push(
      file_list_t *list, core_info_t *info, const char* path)
{
   unsigned j;
   struct string_list *str_list = NULL;

   if (!info)
      return;

   str_list = (struct string_list*)dir_list_new(path,
         info->supported_extensions, true);

   if (!str_list)
      return;

   dir_list_sort(str_list, true);

   for (j = 0; j < str_list->size; j++)
   {
      const char *name = str_list->elems[j].data;
      
      if (!name)
         continue;

      if (str_list->elems[j].attr.i == RARCH_DIRECTORY)
         menu_entries_content_list_push(list, info, name);
      else
         menu_list_push(
               list, name,
               "content_actions",
               MENU_FILE_CONTENTLIST_ENTRY, 0);
   }

   string_list_free(str_list);
}

static int menu_entries_push_cores_list(file_list_t *list, core_info_t *info,
      const char *path, bool push_databases_enable)
{
   size_t i;
   settings_t *settings     = config_get_ptr();

   if (!info->supports_no_game)
      menu_entries_content_list_push(list, info, path);
   else
      menu_list_push(list, info->display_name, "content_actions",
            MENU_FILE_CONTENTLIST_ENTRY, 0);

   if (!push_databases_enable)
      return 0;
   if (!info->databases_list)
      return 0;

   for (i = 0; i < info->databases_list->size; i++)
   {
      char db_path[PATH_MAX_LENGTH];
      struct string_list *str_list = (struct string_list*)info->databases_list;

      if (!str_list)
         continue;

      fill_pathname_join(db_path, settings->content_database,
            str_list->elems[i].data, sizeof(db_path));
      strlcat(db_path, ".rdb", sizeof(db_path));

      if (!path_file_exists(db_path))
         continue;

      menu_list_push(list, path_basename(db_path), "core_database",
            MENU_FILE_RDB, 0);
   }

   return 0;
}

int menu_entries_push_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned type, unsigned setting_flags)
{
   rarch_setting_t *setting = NULL;
   settings_t *settings     = config_get_ptr();
   
   if (menu && menu->list_settings)
      settings_list_free(menu->list_settings);

   menu->list_settings      = (rarch_setting_t *)setting_new(setting_flags);
   setting                  = (rarch_setting_t*)menu_setting_find(label);

   if (!setting)
      return -1;

   menu_list_clear(list);

   for (; setting->type != ST_END_GROUP; setting++)
   {
      if (
            setting->type == ST_GROUP 
            || setting->type == ST_SUB_GROUP
            || setting->type == ST_END_SUB_GROUP
            || (setting->flags & SD_FLAG_ADVANCED && 
               !settings->menu.show_advanced_settings)
         )
         continue;

      menu_list_push(list, setting->short_description,
            setting->name, menu_entries_setting_set_flags(setting), 0);
   }

   menu_driver_populate_entries(path, label, type);

   return 0;
}

int menu_entries_push_horizontal_menu_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned type)
{
   core_info_t           *info = NULL;
   global_t            *global = global_get_ptr();
   core_info_list_t *info_list = (core_info_list_t*)global->core_info;
   settings_t *settings        = config_get_ptr();

   if (!info_list)
      return -1;

   info = (core_info_t*)&info_list->list[menu->categories.selection_ptr - 1];

   if (!info)
      return -1;

   strlcpy(settings->libretro, info->path, sizeof(settings->libretro));

   menu_list_clear(list);

   menu_entries_push_cores_list(list, info, settings->core_assets_directory, true);

   menu_list_populate_generic(list, path, label, type);

   return 0;
}

/**
 * menu_entries_init:
 * @menu                     : Menu handle.
 *
 * Creates and initializes menu entries.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_entries_init(menu_handle_t *menu)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   menu_displaylist_info_t info = {0};
   if (!menu || !menu_list)
      return false;

   info.list  = menu_list->selection_buf;
   info.type  = MENU_SETTINGS;
   info.flags = SL_FLAG_MAIN_MENU;
   strlcpy(info.label, "Main Menu", sizeof(info.label));

   menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);

   return true;
}
