/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __MENU_ENTRIES_H__
#define __MENU_ENTRIES_H__

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

#include "menu_setting.h"
#include "menu_entry.h"
#include "menu_displaylist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   MENU_LIST_PLAIN = 0,
   MENU_LIST_HORIZONTAL,
   MENU_LIST_TABS
} menu_list_type_t;

typedef struct menu_file_list_cbs
{
   rarch_setting_t *setting;

   int (*action_iterate)(const char *label, unsigned action);
   const char *action_iterate_ident;

   int (*action_deferred_push)(menu_displaylist_info_t *info);
   const char *action_deferred_push_ident;

   int (*action_select)(const char *path, const char *label, unsigned type,
         size_t idx);
   const char *action_select_ident;

   int (*action_get_title)(const char *path, const char *label,
         unsigned type, char *s, size_t len);
   const char *action_get_title_ident;

   int (*action_ok)(const char *path, const char *label, unsigned type,
         size_t idx, size_t entry_idx);
   const char *action_ok_ident;

   int (*action_cancel)(const char *path, const char *label, unsigned type,
         size_t idx);
   const char *action_cancel_ident;

   int (*action_scan)(const char *path, const char *label, unsigned type,
         size_t idx);
   const char *action_scan_ident;

   int (*action_start)(unsigned type,  const char *label);
   const char *action_start_ident;

   int (*action_info)(unsigned type,  const char *label);
   const char *action_info_ident;

   int (*action_content_list_switch)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   const char *action_content_list_switch_ident;

   int (*action_left)(unsigned type, const char *label, bool wraparound);
   const char *action_left_ident;

   int (*action_right)(unsigned type, const char *label, bool wraparound);
   const char *action_right_ident;

   int (*action_refresh)(file_list_t *list, file_list_t *menu_list);
   const char *action_refresh_ident;

   int (*action_up)(unsigned type, const char *label);
   const char *action_up_ident;

   int (*action_down)(unsigned type, const char *label);
   const char *action_down_ident;

   void (*action_get_value)(file_list_t* list,
         unsigned *w, unsigned type, unsigned i,
         const char *label, char *s, size_t len,
         const char *entry_label,
         const char *path,
         char *path_buf, size_t path_buf_size);
   const char *action_get_value_ident;

} menu_file_list_cbs_t;

enum menu_entries_ctl_state
{
   MENU_ENTRIES_CTL_NONE = 0,
   MENU_ENTRIES_CTL_DEINIT,
   MENU_ENTRIES_CTL_INIT,
   MENU_ENTRIES_CTL_LIST_GET,
   MENU_ENTRIES_CTL_LIST_DEINIT,
   MENU_ENTRIES_CTL_LIST_INIT,
   MENU_ENTRIES_CTL_SETTINGS_GET,
   MENU_ENTRIES_CTL_SETTINGS_DEINIT,
   MENU_ENTRIES_CTL_SETTINGS_INIT,
   MENU_ENTRIES_CTL_SET_REFRESH,
   MENU_ENTRIES_CTL_UNSET_REFRESH,
   MENU_ENTRIES_CTL_NEEDS_REFRESH,
   /* Sets the starting index of the menu entry list. */
   MENU_ENTRIES_CTL_SET_START,
   /* Returns the starting index of the menu entry list. */
   MENU_ENTRIES_CTL_START_GET,
   MENU_ENTRIES_CTL_REFRESH,
   MENU_ENTRIES_CTL_SHOW_BACK
};

typedef struct menu_list menu_list_t;

size_t menu_entries_get_end(void);

void menu_entries_get(size_t i, menu_entry_t *entry);

int menu_entries_get_title(char *title, size_t title_len);

int menu_entries_get_core_title(char *title_msg, size_t title_msg_len);

file_list_t *menu_entries_get_selection_buf_ptr(size_t idx);

file_list_t *menu_entries_get_menu_stack_ptr(size_t idx);

void menu_entries_push(file_list_t *list, const char *path, const char *label,
      unsigned type, size_t directory_ptr, size_t entry_idx);

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, size_t *entry_idx);

menu_file_list_cbs_t *menu_entries_get_last_stack_actiondata(void);

void menu_entries_pop_stack(size_t *ptr, size_t idx);

void menu_entries_flush_stack(const char *needle, unsigned final_type);

size_t menu_entries_get_stack_size(size_t idx);

size_t menu_entries_get_size(void);

void menu_entries_get_at_offset(const file_list_t *list, size_t idx,
      const char **path, const char **label, unsigned *file_type,
      size_t *entry_idx, const char **alt);

void *menu_entries_get_userdata_at_offset(
      const file_list_t *list, size_t idx);

menu_file_list_cbs_t *menu_entries_get_actiondata_at_offset(
      const file_list_t *list, size_t idx);

void menu_entries_get_last(const file_list_t *list,
      const char **path, const char **label,
      unsigned *file_type, size_t *entry_idx);

void menu_entries_clear(file_list_t *list);

void menu_entries_set_alt_at_offset(file_list_t *list, size_t idx,
      const char *alt);

bool menu_entries_increment_selection_buf(void);

bool menu_entries_increment_menu_stack(void);

void menu_entries_push_selection_buf(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr, size_t entry_idx);

rarch_setting_t *menu_entries_get_setting(uint32_t i);

bool menu_entries_ctl(enum menu_entries_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
