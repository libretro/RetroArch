/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <retro_common_api.h>

#include "widgets/menu_list.h"

#include "menu_setting.h"
#include "menu_displaylist.h"

RETRO_BEGIN_DECLS

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
   MENU_ENTRIES_CTL_CLEAR,
   MENU_ENTRIES_CTL_SHOW_BACK
};

typedef struct menu_file_list_cbs
{
   enum msg_hash_enums enum_idx;
   const char *action_iterate_ident;
   const char *action_deferred_push_ident;
   const char *action_select_ident;
   const char *action_get_title_ident;
   const char *action_ok_ident;
   const char *action_cancel_ident;
   const char *action_scan_ident;
   const char *action_right_ident;
   const char *action_start_ident;
   const char *action_info_ident;
   const char *action_content_list_switch_ident;
   const char *action_left_ident;
   const char *action_refresh_ident;
   const char *action_up_ident;
   const char *action_label_ident;
   const char *action_sublabel_ident;
   const char *action_down_ident;
   const char *action_get_value_ident;

   rarch_setting_t *setting;

   int (*action_iterate)(const char *label, unsigned action);
   int (*action_deferred_push)(menu_displaylist_info_t *info);
   int (*action_select)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_get_title)(const char *path, const char *label,
         unsigned type, char *s, size_t len);
   int (*action_ok)(const char *path, const char *label, unsigned type,
         size_t idx, size_t entry_idx);
   int (*action_cancel)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_scan)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_start)(unsigned type,  const char *label);
   int (*action_info)(unsigned type,  const char *label);
   int (*action_content_list_switch)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   int (*action_left)(unsigned type, const char *label, bool wraparound);
   int (*action_right)(unsigned type, const char *label, bool wraparound);
   int (*action_refresh)(file_list_t *list, file_list_t *menu_list);
   int (*action_up)(unsigned type, const char *label);
   int (*action_label)(file_list_t *list,
         unsigned type, unsigned i,
         const char *label, const char *path,
         char *s, size_t len);
   int (*action_sublabel)(file_list_t *list,
         unsigned type, unsigned i,
         const char *label, const char *path,
         char *s, size_t len);
   int (*action_down)(unsigned type, const char *label);
   void (*action_get_value)(file_list_t* list,
         unsigned *w, unsigned type, unsigned i,
         const char *label, char *s, size_t len,
         const char *entry_label,
         const char *path,
         char *path_buf, size_t path_buf_size);
} menu_file_list_cbs_t;

size_t menu_entries_get_end(void);

void menu_entries_get(size_t i, void *data_entry);

int menu_entries_get_title(char *title, size_t title_len);

bool menu_entries_current_core_is_no_core(void);

int menu_entries_get_core_title(char *title_msg, size_t title_msg_len);

int menu_entries_get_core_name(char *s, size_t len);

file_list_t *menu_entries_get_selection_buf_ptr(size_t idx);

file_list_t *menu_entries_get_menu_stack_ptr(size_t idx);

void menu_entries_append(file_list_t *list, const char *path, const char *label,
      unsigned type, size_t directory_ptr, size_t entry_idx);

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, enum msg_hash_enums *enum_idx, size_t *entry_idx);

menu_file_list_cbs_t *menu_entries_get_last_stack_actiondata(void);

void menu_entries_pop_stack(size_t *ptr, size_t idx, bool animate);

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

void menu_entries_set_alt_at_offset(file_list_t *list, size_t idx,
      const char *alt);

rarch_setting_t *menu_entries_get_setting(uint32_t i);

void menu_entries_prepend(file_list_t *list, const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx);

void menu_entries_append_enum(file_list_t *list, const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx);

bool menu_entries_ctl(enum menu_entries_ctl_state state, void *data);

RETRO_END_DECLS

#endif
