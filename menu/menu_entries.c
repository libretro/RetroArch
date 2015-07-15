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
#include "menu_hash.h"
#include "menu_display.h"
#include "menu_entry.h"
#include "menu_navigation.h"
#include "menu_setting.h"
#include "menu_input.h"

#include "menu_entries.h"

#include "../general.h"
#include "../system.h"

menu_entries_t *menu_entries_get_ptr(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return NULL;

   return &menu->entries;
}

/* Sets the starting index of the menu entry list. */
void menu_entries_set_start(size_t i)
{
   menu_entries_t *entries   = menu_entries_get_ptr();
   
   if (!entries)
     return;

   entries->begin = i;
}

/* Returns the starting index of the menu entry list. */
size_t menu_entries_get_start(void)
{
   menu_entries_t *entries   = menu_entries_get_ptr();
   
   if (!entries)
     return 0;

   return entries->begin;
}

/* Returns the last index (+1) of the menu entry list. */
size_t menu_entries_get_end(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   return menu_list_get_size(entries->menu_list);
}

/* Get an entry from the top of the menu stack */
void menu_entries_get(size_t i, menu_entry_t *entry)
{
   const char *label         = NULL;
   const char *path          = NULL;
   const char *entry_label   = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_entries_t   *entries = menu_entries_get_ptr();

   if (!entries || !entries->menu_list)
      return;

   menu_list_get_last_stack(entries->menu_list, NULL, &label, NULL, NULL);

   entry->path[0] = entry->value[0] = entry->label[0] = '\0';

   menu_list_get_at_offset(entries->menu_list->selection_buf, i,
         &path, &entry_label, &entry->type, &entry->entry_idx);

   cbs = menu_list_get_actiondata_at_offset(entries->menu_list->selection_buf, i);

   if (cbs && cbs->action_get_value)
      cbs->action_get_value(entries->menu_list->selection_buf,
            &entry->spacing, entry->type, i, label,
            entry->value,  sizeof(entry->value),
            entry_label, path,
            entry->path, sizeof(entry->path));

   entry->idx = i;

   if (entry_label)
      strlcpy(entry->label, entry_label, sizeof(entry->label));
}

/* Sets title to what the name of the current menu should be. */
int menu_entries_get_title(char *s, size_t len)
{
   const char *path          = NULL;
   const char *label         = NULL;
   unsigned menu_type        = 0;
   menu_file_list_cbs_t *cbs = NULL;
   menu_entries_t *entries = menu_entries_get_ptr();
   
   if (!entries->menu_list)
      return -1;

   cbs = (menu_file_list_cbs_t*)menu_list_get_last_stack_actiondata(entries->menu_list);

   menu_list_get_last_stack(entries->menu_list, &path, &label, &menu_type, NULL);

   if (cbs && cbs->action_get_title)
      return cbs->action_get_title(path, label, menu_type, s, len);
   return 0;
}

/* Returns true if a Back button should be shown (i.e. we are at least
 * one level deep in the menu hierarchy). */
bool menu_entries_show_back(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();

   if (!entries->menu_list)
      return false;

   return (menu_list_get_stack_size(entries->menu_list) > 1);
}

/* Sets 's' to the name of the current core 
 * (shown at the top of the UI). */
void menu_entries_get_core_title(char *s, size_t len)
{
   global_t *global               = global_get_ptr();
   const char *core_name          = global ? global->menu.info.library_name    : NULL;
   const char *core_version       = global ? global->menu.info.library_version : NULL;
   rarch_system_info_t      *info = rarch_system_info_get_ptr();

   if (!core_name || core_name[0] == '\0')
      core_name = info->info.library_name;
   if (!core_name || core_name[0] == '\0')
      core_name = menu_hash_to_str(MENU_VALUE_NO_CORE);

   if (!core_version)
      core_version = info->info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(s, len, "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
}

static bool menu_entries_get_nonblocking_refresh(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   if (!entries)
      return false;
   return entries->nonblocking_refresh;
}

int menu_entries_refresh(unsigned action)
{
   if (menu_entries_get_nonblocking_refresh())
      return -1;
   if (!menu_entries_needs_refresh())
      return -1;
   return menu_entry_iterate(MENU_ACTION_REFRESH);
}

bool menu_entries_needs_refresh(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   if (!entries)
      return false;
   return entries->need_refresh;
}

void menu_entries_set_nonblocking_refresh(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   if (!entries)
      return;
   entries->nonblocking_refresh = true;
}

void menu_entries_unset_nonblocking_refresh(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   if (!entries)
      return;
   entries->nonblocking_refresh = false;
}

void menu_entries_set_refresh(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   if (!entries)
      return;
   entries->need_refresh = true;
}

void menu_entries_unset_refresh(void)
{
   menu_entries_t *entries = menu_entries_get_ptr();
   if (!entries)
      return;
   entries->need_refresh = false;
}
