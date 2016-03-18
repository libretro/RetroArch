/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <compat/strl.h>
#include <string/string_list.h>

#include "menu_driver.h"
#include "menu_display.h"
#include "menu_navigation.h"

/* This file provides an abstraction of the currently displayed
 * menu.
 *
 * It is organized into an event-based system where the UI companion
 * calls this functions and RetroArch responds by changing the global
 * state (including arranging for these functions to return different
 * values).
 *
 * Its only interaction back to the UI is to arrange for
 * notify_list_loaded on the UI companion.
 */

/* Clicks the back button */
int menu_entry_go_back(void)
{
   size_t new_selection_ptr;

   menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &new_selection_ptr);
   menu_entries_pop_stack(&new_selection_ptr, 0);
   menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &new_selection_ptr);

   return 0;
}

enum menu_entry_type menu_entry_get_type(uint32_t i)
{
   rarch_setting_t *setting  = menu_entries_get_setting(i);

   /* XXX Really a special kind of ST_ACTION, but this should be changed */
   if (menu_setting_ctl(MENU_SETTING_CTL_IS_OF_PATH_TYPE, (void*)setting))
      return MENU_ENTRY_PATH;

   if (setting)
   {
      enum setting_type setting_type = 
         menu_setting_get_type(setting);

      switch (setting_type)
      {
         case ST_BOOL:
            return MENU_ENTRY_BOOL;
         case ST_BIND:
            return MENU_ENTRY_BIND;
         case ST_INT:
            return MENU_ENTRY_INT;
         case ST_UINT:
            return MENU_ENTRY_UINT;
         case ST_FLOAT:
            return MENU_ENTRY_FLOAT;
         case ST_PATH:
            return MENU_ENTRY_PATH;
         case ST_DIR:
            return MENU_ENTRY_DIR;
         case ST_STRING_OPTIONS:
            return MENU_ENTRY_ENUM;
         case ST_STRING:
            return MENU_ENTRY_STRING;
         case ST_HEX:
            return MENU_ENTRY_HEX;

         default:
            break;
      }
   }

   return MENU_ENTRY_ACTION;
}

void menu_entry_get_path(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, 0, i, NULL, true);

   strlcpy(s, entry.path, len);
}

void menu_entry_get_label(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, 0, i, NULL, true);

   strlcpy(s, entry.label, len);
}

unsigned menu_entry_get_spacing(uint32_t i)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, 0, i, NULL, true);

   return entry.spacing;
}

unsigned menu_entry_get_type_new(uint32_t i)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, 0, i, NULL, true);

   return entry.type;
}

uint32_t menu_entry_get_bool_value(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   bool *ptr                = (bool*)setting_get_ptr(setting);
   if (!ptr)
      return 0;
   return *ptr;
}

struct string_list *menu_entry_enum_values(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   const char      *values  = menu_setting_get_values(setting);

   if (!values)
      return NULL;
   return string_split(values, "|");
}

void menu_entry_enum_set_value_with_string(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   menu_setting_set_with_string_representation(setting, s);
}

int32_t menu_entry_bind_index(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   uint32_t          index  = menu_setting_get_index(setting);

   if (index)
      return index - 1;
   return 0;
}

void menu_entry_bind_key_set(uint32_t i, int32_t value)
{
   rarch_setting_t      *setting = menu_entries_get_setting(i);
   struct retro_keybind *keybind = (struct retro_keybind*)
      setting_get_ptr(setting);
   if (keybind)
      keybind->key = (enum retro_key)value;
}

void menu_entry_bind_joykey_set(uint32_t i, int32_t value)
{
   rarch_setting_t      *setting = menu_entries_get_setting(i);
   struct retro_keybind *keybind = (struct retro_keybind*)
      setting_get_ptr(setting);
   if (keybind)
      keybind->joykey = value;
}

void menu_entry_bind_joyaxis_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   struct retro_keybind *keybind = (struct retro_keybind*)
      setting_get_ptr(setting);
   if (keybind)
      keybind->joyaxis = value;
}

void menu_entry_pathdir_selected(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);

   if (menu_setting_ctl(MENU_SETTING_CTL_IS_OF_PATH_TYPE, (void*)setting))
      menu_setting_ctl(MENU_SETTING_CTL_ACTION_RIGHT, setting);
}

bool menu_entry_pathdir_allow_empty(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   uint64_t           flags = menu_setting_get_flags(setting);

   return flags & SD_FLAG_ALLOW_EMPTY;
}

uint32_t menu_entry_pathdir_for_directory(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   uint64_t           flags = menu_setting_get_flags(setting);

   return flags & SD_FLAG_PATH_DIR;
}

void menu_entry_pathdir_get_value(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, 0, i, NULL, true);
   strlcpy(s, entry.value, len);
}

int menu_entry_pathdir_set_value(uint32_t i, const char *s)
{
   const char *menu_path     = NULL;
   menu_file_list_cbs_t *cbs = menu_entries_get_last_stack_actiondata();

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL);

   if (!cbs || !cbs->setting)
      return -1;

   if (menu_setting_get_type(cbs->setting) != ST_DIR)
      return -1;

   menu_setting_set_with_string_representation(cbs->setting, menu_path);
   menu_setting_generic(cbs->setting, false);

   menu_entries_flush_stack(NULL, 49);

   return 0;
}

void menu_entry_pathdir_extensions(uint32_t i, char *s, size_t len)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   const char      *values  = menu_setting_get_values(setting);

   if (!values)
      return;

   strlcpy(s, values, len);
}

void menu_entry_reset(uint32_t i)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, 0, i, NULL, true);

   menu_entry_action(&entry, i, MENU_ACTION_START);
}

void menu_entry_get_value(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, 0, i, NULL, true);
   strlcpy(s, entry.value, len);
}

void menu_entry_set_value(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   menu_setting_set_with_string_representation(setting, s);
}

uint32_t menu_entry_num_has_range(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   uint64_t           flags = menu_setting_get_flags(setting);

   return (flags & SD_FLAG_HAS_RANGE);
}

float menu_entry_num_min(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   double               min = menu_setting_get_min(setting);
   return (float)min;
}

float menu_entry_num_max(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   double               max = menu_setting_get_max(setting);
   return (float)max;
}

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation)
{
   const char *path           = NULL;
   const char *entry_label    = NULL;
   menu_file_list_cbs_t *cbs  = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(stack_idx);
   file_list_t *list          = selection_buf;

   if (userdata)
      list = (file_list_t*)userdata;

   if (!list)
      return;

   menu_entries_get_at_offset(list, i, &path, &entry_label, &entry->type,
         &entry->entry_idx, NULL);

   cbs = menu_entries_get_actiondata_at_offset(list, i);

   if (cbs && cbs->action_get_value && use_representation)
   {
      const char *label         = NULL;
      menu_entries_get_last_stack(NULL, &label, NULL, NULL);

      cbs->action_get_value(list,
            &entry->spacing, entry->type, i, label,
            entry->value,  sizeof(entry->value), 
            entry_label, path,
            entry->path, sizeof(entry->path));
   }

   entry->idx         = i;

   if (path && !use_representation)
      strlcpy(entry->path,  path,        sizeof(entry->path));
   if (entry_label)
      strlcpy(entry->label, entry_label, sizeof(entry->label));
}

bool menu_entry_is_currently_selected(unsigned id)
{
   size_t selection;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
         return false;

   return (id == selection);
}

/* Performs whatever actions are associated with menu entry 'i'.
 *
 * This is the most important function because it does all the work
 * associated with clicking on things in the UI.
 *
 * This includes loading cores and updating the 
 * currently displayed menu. */
int menu_entry_select(uint32_t i)
{
   menu_entry_t     entry = {{0}};
    
   menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &i);

   menu_entry_get(&entry, 0, i, NULL, false);

   return menu_entry_action(&entry, i, MENU_ACTION_SELECT);
}

int menu_entry_action(menu_entry_t *entry, unsigned i, enum menu_action action)
{
   int ret                    = 0;
   file_list_t *selection_buf = 
      menu_entries_get_selection_buf_ptr(0);
   menu_file_list_cbs_t *cbs  = 
      menu_entries_get_actiondata_at_offset(selection_buf, i);

   switch (action)
   {
      case MENU_ACTION_UP:
         if (cbs && cbs->action_up)
            ret = cbs->action_up(entry->type, entry->label);
         break;
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_down)
            ret = cbs->action_down(entry->type, entry->label);
         break;
      case MENU_ACTION_SCROLL_UP:
         menu_navigation_ctl(MENU_NAVIGATION_CTL_DESCEND_ALPHABET, NULL);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_navigation_ctl(MENU_NAVIGATION_CTL_ASCEND_ALPHABET, NULL);
         break;
      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            ret = cbs->action_cancel(entry->path,
                  entry->label, entry->type, i);
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            ret = cbs->action_ok(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            ret = cbs->action_start(entry->type, entry->label);
         break;
      case MENU_ACTION_LEFT:
         if (cbs && cbs->action_left)
            ret = cbs->action_left(entry->type, entry->label, false);
         break;
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_right)
            ret = cbs->action_right(entry->type, entry->label, false);
         break;
      case MENU_ACTION_INFO:
         if (cbs && cbs->action_info)
            ret = cbs->action_info(entry->type, entry->label);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(entry->path,
                  entry->label, entry->type, i);
         break;
      case MENU_ACTION_SEARCH:
         menu_input_ctl(MENU_INPUT_CTL_SEARCH_START, NULL);
         break;

      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path,
                  entry->label, entry->type, i);
         break;

      default:
         break;
   }

   cbs = menu_entries_get_actiondata_at_offset(selection_buf, i);

   if (menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL))
   {
      if (cbs && cbs->action_refresh)
      {
         bool refresh               = false;
         file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);

         cbs->action_refresh(selection_buf, menu_stack);
         menu_entries_ctl(MENU_ENTRIES_CTL_UNSET_REFRESH, &refresh);
      }
   }

   return ret;
}
