/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "menu_input_dialog.h"

#include "../menu_driver.h"

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

enum menu_entry_type menu_entry_get_type(uint32_t i)
{
   rarch_setting_t *setting  = menu_entries_get_setting(i);

   /* XXX Really a special kind of ST_ACTION, but this should be changed */
   if (menu_setting_ctl(MENU_SETTING_CTL_IS_OF_PATH_TYPE, (void*)setting))
      return MENU_ENTRY_PATH;

   if (setting)
   {
      switch (setting_get_type(setting))
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

void menu_entry_free(menu_entry_t *entry)
{
   if (!entry)
      return;
   if (!string_is_empty(entry->label))
      free(entry->label);
   if (!string_is_empty(entry->rich_label))
      free(entry->rich_label);
   if (!string_is_empty(entry->sublabel))
      free(entry->sublabel);
   if (!string_is_empty(entry->path))
      free(entry->path);
   if (!string_is_empty(entry->value))
      free(entry->value);
   entry->path          = NULL;
   entry->label         = NULL;
   entry->value         = NULL;
   entry->sublabel      = NULL;
   entry->rich_label    = NULL;
}

void menu_entry_init(menu_entry_t *entry)
{
   entry->path          = NULL;
   entry->label         = NULL;
   entry->value         = NULL;
   entry->sublabel      = NULL;
   entry->rich_label    = NULL;
   entry->enum_idx      = MSG_UNKNOWN;
   entry->entry_idx     = 0;
   entry->idx           = 0;
   entry->type          = 0;
   entry->spacing       = 0;
}

char *menu_entry_get_path(menu_entry_t *entry)
{
   if (!entry || string_is_empty(entry->path))
      return NULL;
   return strdup(entry->path);
}

char *menu_entry_get_rich_label(menu_entry_t *entry)
{
   if (!entry)
      return NULL;
   if (!string_is_empty(entry->rich_label))
      return strdup(entry->rich_label);
   if (!string_is_empty(entry->path))
      return strdup(entry->path);
   return NULL;
}

char *menu_entry_get_sublabel(menu_entry_t *entry)
{
   if (!entry || string_is_empty(entry->sublabel))
      return NULL;
   return strdup(entry->sublabel);
}

void menu_entry_get_label(menu_entry_t *entry, char *s, size_t len)
{
   if (!entry || string_is_empty(entry->label))
      return;
   strlcpy(s, entry->label, len);
}

unsigned menu_entry_get_spacing(menu_entry_t *entry)
{
   if (!entry)
      return 0;
   return entry->spacing;
}

unsigned menu_entry_get_type_new(menu_entry_t *entry)
{
   if (!entry)
      return 0;
   return entry->type;
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
   const char      *values  = setting->values;

   if (!values)
      return NULL;
   return string_split(values, "|");
}

void menu_entry_enum_set_value_with_string(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

int32_t menu_entry_bind_index(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);

   if (setting)
      return setting->index - 1;
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
   uint64_t           flags = setting->flags;

   return flags & SD_FLAG_ALLOW_EMPTY;
}

uint32_t menu_entry_pathdir_for_directory(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   uint64_t           flags = setting->flags;

   return flags & SD_FLAG_PATH_DIR;
}

void menu_entry_pathdir_extensions(uint32_t i, char *s, size_t len)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   const char      *values  = setting->values;

   if (!values)
      return;

   strlcpy(s, values, len);
}

void menu_entry_reset(uint32_t i)
{
   menu_entry_t entry;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, i, NULL, true);

   menu_entry_action(&entry, i, MENU_ACTION_START);
}

void menu_entry_get_value(menu_entry_t *entry, char *s, size_t len)
{
   if (!entry || string_is_empty(entry->value))
      return;
   strlcpy(s, entry->value, len);
}

void menu_entry_set_value(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

uint32_t menu_entry_num_has_range(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   uint64_t           flags = setting->flags;

   return (flags & SD_FLAG_HAS_RANGE);
}

float menu_entry_num_min(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   double               min = setting->min;
   return (float)min;
}

float menu_entry_num_max(uint32_t i)
{
   rarch_setting_t *setting = menu_entries_get_setting(i);
   double               max = setting->max;
   return (float)max;
}

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation)
{
   char newpath[255];
   const char *path           = NULL;
   const char *entry_label    = NULL;
   menu_file_list_cbs_t *cbs  = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(stack_idx);
   file_list_t *list          = (userdata) ? (file_list_t*)userdata : selection_buf;

   newpath[0]                 = '\0';

   if (!list)
      return;

   file_list_get_at_offset(list, i, &path, &entry_label, &entry->type,
         &entry->entry_idx);

   cbs = (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(list, i);

   if (cbs)
   {
      const char *label             = NULL;
      enum msg_hash_enums enum_idx  = MSG_UNKNOWN;

      entry->enum_idx               = cbs->enum_idx;

      menu_entries_get_last_stack(NULL, &label, NULL, &enum_idx, NULL);

      if (cbs->action_get_value && use_representation)
      {
         char tmp[255];
         tmp[0] = '\0';

         cbs->action_get_value(list,
               &entry->spacing, entry->type,
               (unsigned)i, label,
               tmp,
               sizeof(tmp),
               entry_label, path,
               newpath,
               sizeof(newpath)
               );

         if (!string_is_empty(tmp))
            entry->value = strdup(tmp);
      }

      if (cbs->action_label)
      {
         char tmp[255];
         tmp[0] = '\0';

         cbs->action_label(list,
               entry->type, (unsigned)i,
               label, path,
               tmp,
               sizeof(tmp));

         if (!string_is_empty(tmp))
            entry->rich_label = strdup(tmp);
      }

      if (cbs->action_sublabel)
      {
         char tmp[255];
         tmp[0] = '\0';

         cbs->action_sublabel(list,
               entry->type, (unsigned)i,
               label, path,
               tmp,
               sizeof(tmp));

         if (!string_is_empty(tmp))
            entry->sublabel = strdup(tmp);
      }
   }

   entry->idx         = (unsigned)i;

   if (!string_is_empty(path) && !use_representation)
      strlcpy(newpath,  path, sizeof(newpath));
   else if (cbs && cbs->setting && cbs->setting->enum_value_idx != MSG_UNKNOWN
         && !cbs->setting->dont_use_enum_idx_representation)
      strlcpy(newpath,
            msg_hash_to_str(cbs->setting->enum_value_idx),
            sizeof(newpath));

   if (!string_is_empty(newpath))
      entry->path = strdup(newpath);

   if (!string_is_empty(entry_label))
      entry->label = strdup(entry_label);
}

bool menu_entry_is_currently_selected(unsigned id)
{
   return (id == menu_navigation_get_selection());
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
   menu_entry_t     entry;

   menu_navigation_set_selection(i);

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, i, NULL, false);

   return menu_entry_action(&entry, i, MENU_ACTION_SELECT);
}

int menu_entry_action(menu_entry_t *entry, unsigned i, enum menu_action action)
{
   int ret                    = 0;
   file_list_t *selection_buf =
      menu_entries_get_selection_buf_ptr(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(selection_buf, i) : NULL;

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
         menu_driver_ctl(MENU_NAVIGATION_CTL_DESCEND_ALPHABET, NULL);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_driver_ctl(MENU_NAVIGATION_CTL_ASCEND_ALPHABET, NULL);
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
         menu_input_dialog_start_search();
         break;

      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path,
                  entry->label, entry->type, i);
         break;

      default:
         break;
   }

   cbs = selection_buf ? (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(selection_buf, i) : NULL;

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
