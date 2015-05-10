/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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
#include "menu_entry.h"
#include "menu_navigation.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "../settings.h"

void get_core_title(char *title_msg, size_t title_msg_len)
{
   global_t *global          = global_get_ptr();
   const char *core_name     = global->menu.info.library_name;
   const char *core_version  = global->menu.info.library_version;

   if (!core_name)
      core_name = global->system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   if (!core_version)
      core_version = global->system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, title_msg_len, "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
}

rarch_setting_t *menu_entry_get_setting(uint32_t i)
{
   rarch_setting_t *setting;
   const char *path = NULL, *entry_label = NULL;
   unsigned type = 0;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir, &label, &menu_type);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type);

   setting = menu_setting_find(
         menu_list->selection_buf->list[i].label);

   return setting;
}

enum menu_entry_type menu_entry_get_type(uint32_t i)
{
   rarch_setting_t *setting;
   const char *path = NULL, *entry_label = NULL;
   unsigned type = 0;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir, &label, &menu_type);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type);

   setting = menu_entry_get_setting(i);

   // XXX Really a special kind of ST_ACTION, but this should be
   // changed
   if (setting_is_of_path_type(setting))
      return MENU_ENTRY_PATH;
   else if (setting && setting->type == ST_BOOL )
      return MENU_ENTRY_BOOL;
   else if (setting && setting->type == ST_BIND )
      return MENU_ENTRY_BIND;
   else if (setting_is_of_enum_type(setting))
      return MENU_ENTRY_ENUM;
   else if (setting && setting->type == ST_INT )
      return MENU_ENTRY_INT;
   else if (setting && setting->type == ST_UINT )
      return MENU_ENTRY_UINT;
   else if (setting && setting->type == ST_FLOAT )
      return MENU_ENTRY_FLOAT;
   else if (setting && setting->type == ST_PATH )
      return MENU_ENTRY_PATH;
   else if (setting && setting->type == ST_DIR )
      return MENU_ENTRY_DIR;
   else if (setting && setting->type == ST_STRING )
      return MENU_ENTRY_STRING;
   else if (setting && setting->type == ST_HEX )
      return MENU_ENTRY_HEX;
   else
      return MENU_ENTRY_ACTION;
}

void menu_entry_get_label(uint32_t i, char *label, size_t sizeof_label)
{
   menu_entry_t entry;
   menu_entry_get(&entry, i, NULL, true);

   strlcpy(label, entry.path, sizeof_label);
}

uint32_t menu_entry_get_bool_value(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return *setting->value.boolean;
}

void menu_entry_set_bool_value(uint32_t i, uint32_t new_val)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   *setting->value.boolean = new_val;
}

struct string_list *menu_entry_enum_values(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return string_split(setting->values, "|");
}

void menu_entry_enum_set_value_with_string(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

int32_t menu_entry_bind_index(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   if (setting->index)
      return setting->index - 1;
   return 0;
}

void menu_entry_bind_key_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   BINDFOR(*setting).key = value;
}

void menu_entry_bind_joykey_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   BINDFOR(*setting).joykey = value;
}

void menu_entry_bind_joyaxis_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   BINDFOR(*setting).joyaxis = value;
}

void menu_entry_pathdir_selected(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   if (setting_is_of_path_type(setting))
      setting->action_toggle( setting, MENU_ACTION_RIGHT, false);
}

uint32_t menu_entry_pathdir_allow_empty(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->flags & SD_FLAG_ALLOW_EMPTY;
}

uint32_t menu_entry_pathdir_for_directory(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->flags & SD_FLAG_PATH_DIR;
}

const char *menu_entry_pathdir_get_value(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->value.string;
}

void menu_entry_pathdir_set_value(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

const char *menu_entry_pathdir_extensions(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->values;
}

void menu_entry_reset(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_reset_setting(setting);
}

void menu_entry_get_value(uint32_t i, char *s, size_t len)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_get_string_representation(setting, s, len);
}

void menu_entry_set_value(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

uint32_t menu_entry_num_has_range(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return (setting->flags & SD_FLAG_HAS_RANGE);
}

float menu_entry_num_min(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->min;
}

float menu_entry_num_max(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->max;
}

void menu_entry_get(menu_entry_t *entry, size_t i,
      void *userdata, bool use_representation)
{
   const char *label         = NULL;
   const char *path          = NULL;
   const char *entry_label   = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();

   if (!menu_list)
      return;

   menu_list_get_last_stack(menu_list, NULL, &label, NULL);
   
   list = userdata ? (file_list_t*)userdata : menu_list->selection_buf;

   if (!list)
      return;

   menu_list_get_at_offset(list, i, &path, &entry_label, &entry->type);

   cbs = (menu_file_list_cbs_t*)menu_list_get_actiondata_at_offset(list, i);

   if (cbs && cbs->action_get_representation && use_representation)
      cbs->action_get_representation(list,
            &entry->spacing, entry->type, i, label,
            entry->value,  sizeof(entry->value), 
            entry_label, path,
            entry->path, sizeof(entry->path));

   entry->id         = i;

   if (path)
      strlcpy(entry->path,  path,        sizeof(entry->path));
   if (entry_label)
      strlcpy(entry->label, entry_label, sizeof(entry->label));
}

bool menu_entry_is_currently_selected(menu_entry_t *entry)
{
   menu_navigation_t *nav = menu_navigation_get_ptr();
   if (!entry || !nav)
      return false;
   return (entry->id == nav->selection_ptr);
}

int menu_entry_get_current_id(bool use_representation)
{
   size_t i;
   menu_list_t   *menu_list = menu_list_get_ptr();
   size_t               end = menu_list_get_size(menu_list);

   for (i = 0; i < end; i++)
   {
      menu_entry_t entry;
      menu_entry_get(&entry, i, NULL, use_representation);

      if (menu_entry_is_currently_selected(&entry))
         return i;
   }

   return -1;
}

/* Returns true if the menu should reload */
uint32_t menu_entry_select(uint32_t i)
{
   menu_entry_t entry;
   menu_file_list_cbs_t *cbs = NULL;
   menu_navigation_t *nav    = menu_navigation_get_ptr();
   menu_list_t    *menu_list = menu_list_get_ptr();
   rarch_setting_t *setting  = menu_setting_find(
         menu_list->selection_buf->list[i].label);

   menu_entry_get(&entry, i, NULL, false);

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(menu_list->selection_buf, i);

   if (setting_is_of_path_type(setting))
      return false;
   if (setting_is_of_general_type(setting))
   {
      nav->selection_ptr = i;
      if (cbs && cbs->action_ok)
         cbs->action_ok(entry.path, entry.label, entry.type, i);

      return false;
   }

   nav->selection_ptr = i;
   if (cbs && cbs->action_ok)
      cbs->action_ok(entry.path, entry.label, entry.type, i);
   else
   {
      if (cbs && cbs->action_start)
         cbs->action_start(entry.type, entry.label, MENU_ACTION_START);
      if (cbs && cbs->action_toggle)
         cbs->action_toggle(entry.type, entry.label, MENU_ACTION_RIGHT, true);
      menu_list_push(menu_list->menu_stack, "",
            "info_screen", 0, i);
   }
   return true;
}
