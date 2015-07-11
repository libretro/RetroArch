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

#include <compat/strl.h>
#include <string/string_list.h>

#include "menu.h"
#include "menu_display.h"
#include "menu_entry.h"
#include "menu_navigation.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "../runloop_data.h"

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
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   menu_setting_apply_deferred();
   menu_list_pop_stack(menu_list);

   if (menu_entries_needs_refresh())
      menu_entries_refresh(MENU_ACTION_CANCEL);

   rarch_main_data_iterate();

   return 0;
}


static rarch_setting_t *menu_entry_get_setting(uint32_t i)
{
   const char          *path = NULL;
   const char   *entry_label = NULL;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned             type = 0;
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir,
         &label, &menu_type, NULL);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type, NULL);

   return menu_setting_find(
         menu_list->selection_buf->list[i].label);
}

enum menu_entry_type menu_entry_get_type(uint32_t i)
{
   rarch_setting_t *setting  = NULL;
   const char *path          = NULL;
   const char *entry_label   = NULL;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned type             = 0;
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir,
         &label, &menu_type, NULL);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type, NULL);

   setting = menu_entry_get_setting(i);

   /* XXX Really a special kind of ST_ACTION, but this should be changed */
   if (menu_setting_is_of_path_type(setting))
      return MENU_ENTRY_PATH;

   if (menu_setting_is_of_enum_type(setting))
      return MENU_ENTRY_ENUM;

   if (setting)
   {
      switch (setting->type)
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
         case ST_STRING:
            return MENU_ENTRY_STRING;
         case ST_HEX:
            return MENU_ENTRY_HEX;
           
         case ST_NONE:
         case ST_ACTION:
         case ST_GROUP:
         case ST_SUB_GROUP:
         case ST_END_GROUP:
         case ST_END_SUB_GROUP:
            break;
      }
   }

   return MENU_ENTRY_ACTION;
}

void menu_entry_get_path(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, i, NULL, true);

   strlcpy(s, entry.path, len);
}

void menu_entry_get_label(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, i, NULL, true);

   strlcpy(s, entry.label, len);
}

unsigned menu_entry_get_spacing(uint32_t i)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, i, NULL, true);

   return entry.spacing;
}

unsigned menu_entry_get_type_new(uint32_t i)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, i, NULL, true);

   return entry.type;
}

uint32_t menu_entry_get_bool_value(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return *setting->value.boolean;
}

void menu_entry_set_bool_value(uint32_t i, bool value)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_set_with_string_representation(setting, value ? "true" : "false");
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
   BINDFOR(*setting).key = (enum retro_key)value;
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
   if (menu_setting_is_of_path_type(setting))
      setting->action_right(setting, false);
}

bool menu_entry_pathdir_allow_empty(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   if (!setting)
      return false;
   return setting->flags & SD_FLAG_ALLOW_EMPTY;
}

uint32_t menu_entry_pathdir_for_directory(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->flags & SD_FLAG_PATH_DIR;
}

void menu_entry_pathdir_get_value(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, i, NULL, true);
   strlcpy(s, entry.value, len);
}

int menu_entry_pathdir_set_value(uint32_t i, const char *s)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;
   menu_list_t *menu_list   = menu_list_get_ptr();

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL, NULL);

   setting = menu_setting_find(menu_label);

   if (!setting)
      return -1;

   if (setting->type != ST_DIR)
      return -1;

   (void)s;
   setting_set_with_string_representation(setting, menu_path);

   menu_setting_generic(setting, false);

   menu_list_pop_stack_by_needle(menu_list, setting->name);

   return 0;
}

void menu_entry_pathdir_extensions(uint32_t i, char *s, size_t len)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   const char *extensions   = setting ? setting->values : NULL;
   if (setting && extensions)
      strlcpy(s, extensions, len);
}

void menu_entry_reset(uint32_t i)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, i, NULL, true);

   menu_entry_action(&entry, i, MENU_ACTION_START);
}

void menu_entry_get_value(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry = {{0}};
   menu_entry_get(&entry, i, NULL, true);
   strlcpy(s, entry.value, len);
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

   menu_list_get_last_stack(menu_list, NULL, &label, NULL, NULL);
   
   list = userdata ? (file_list_t*)userdata : menu_list->selection_buf;

   if (!list)
      return;

   menu_list_get_at_offset(list, i, &path, &entry_label, &entry->type,
         &entry->entry_idx);

   cbs = menu_list_get_actiondata_at_offset(list, i);

   if (cbs && cbs->action_get_value && use_representation)
      cbs->action_get_value(list,
            &entry->spacing, entry->type, i, label,
            entry->value,  sizeof(entry->value), 
            entry_label, path,
            entry->path, sizeof(entry->path));

   entry->idx         = i;

   if (path && !use_representation)
      strlcpy(entry->path,  path,        sizeof(entry->path));
   if (entry_label)
      strlcpy(entry->label, entry_label, sizeof(entry->label));
}

bool menu_entry_is_currently_selected(unsigned id)
{
   menu_navigation_t *nav = menu_navigation_get_ptr();
   if (!nav)
      return false;
   return (id == nav->selection_ptr);
}

int menu_entry_get_current_id(bool use_representation)
{
   size_t i;
   menu_list_t   *menu_list = menu_list_get_ptr();
   size_t               end = menu_list_get_size(menu_list);

   for (i = 0; i < end; i++)
   {
      menu_entry_t entry = {{0}};
      menu_entry_get(&entry, i, NULL, use_representation);

      if (menu_entry_is_currently_selected(entry.idx))
         return i;
   }

   return -1;
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
   menu_navigation_t *nav = menu_navigation_get_ptr();
    
   nav->selection_ptr = i;
   menu_entry_get(&entry, i, NULL, false);

   return menu_entry_action(&entry, i, MENU_ACTION_SELECT);
}

int menu_entry_iterate(unsigned action)
{
   const char *label         = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   if (action != MENU_ACTION_NOOP || menu_entries_needs_refresh() || menu_display_update_pending())
      menu_display_fb_set_dirty();

   cbs = (menu_file_list_cbs_t*)menu_list_get_last_stack_actiondata(menu_list);

   menu_list_get_last_stack(menu_list, NULL, &label, NULL, NULL);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);

   return -1;
}

int menu_entry_action(menu_entry_t *entry, unsigned i, enum menu_action action)
{
   int ret                   = 0;
   menu_navigation_t *nav    = menu_navigation_get_ptr();
   menu_display_t *disp      = menu_display_get_ptr();
   menu_list_t *menu_list    = menu_list_get_ptr();
   menu_file_list_cbs_t *cbs = menu_list_get_actiondata_at_offset(menu_list->selection_buf, i);

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
         menu_navigation_descend_alphabet(nav, &nav->selection_ptr);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_navigation_ascend_alphabet(nav, &nav->selection_ptr);
         break;

      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            ret = cbs->action_cancel(entry->path, entry->label, entry->type, i);
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            ret = cbs->action_ok(entry->path, entry->label, entry->type, i, entry->entry_idx);
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
            ret = cbs->action_select(entry->path, entry->label, entry->type, i);
         break;

      case MENU_ACTION_REFRESH:
         if (cbs && cbs->action_refresh)
         {
            ret = cbs->action_refresh(menu_list->selection_buf, menu_list->menu_stack);
            menu_entries_unset_refresh();
         }
         break;

      case MENU_ACTION_MESSAGE:
         if (disp)
            disp->msg_force = true;
         break;

      case MENU_ACTION_SEARCH:
         menu_input_search_start();
         break;

      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path, entry->label, entry->type, i);
         break;

      default:
         break;
   }

   return ret;
}
