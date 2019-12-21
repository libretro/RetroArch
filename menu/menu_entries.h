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

#include <lists/file_list.h>

#include "menu_setting.h"
#include "menu_input.h"
#include "menu_displaylist.h"

RETRO_BEGIN_DECLS

#define MENU_SUBLABEL_MAX_LENGTH 1024

enum menu_entries_ctl_state
{
   MENU_ENTRIES_CTL_NONE = 0,
   MENU_ENTRIES_CTL_LIST_GET,
   MENU_ENTRIES_CTL_LIST_DEINIT,
   MENU_ENTRIES_CTL_SETTINGS_GET,
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

enum menu_list_type
{
   MENU_LIST_PLAIN = 0,
   MENU_LIST_HORIZONTAL,
   MENU_LIST_TABS
};

typedef struct menu_list menu_list_t;

typedef struct menu_ctx_list
{
   enum menu_list_type type;
   const char *path;
   char       *fullpath;
   const char *label;
   unsigned entry_type;
   unsigned action;
   size_t idx;
   size_t selection;
   size_t size;
   size_t list_size;
   void *entry;
   file_list_t *list;
} menu_ctx_list_t;

typedef struct menu_file_list_cbs
{
   char action_sublabel_cache[MENU_SUBLABEL_MAX_LENGTH];
   char action_title_cache   [512];

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

   bool checked;

   rarch_setting_t *setting;

   int (*action_iterate)(const char *label, unsigned action);
   int (*action_deferred_push)(menu_displaylist_info_t *info);
   int (*action_select)(const char *path, const char *label, unsigned type,
         size_t idx, size_t entry_idx);
   int (*action_get_title)(const char *path, const char *label,
         unsigned type, char *s, size_t len);
   int (*action_ok)(const char *path, const char *label, unsigned type,
         size_t idx, size_t entry_idx);
   int (*action_cancel)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_scan)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_start)(const char *path, const char *label, unsigned type,
         size_t idx, size_t entry_idx);
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
         const char *path,
         char *path_buf, size_t path_buf_size);
} menu_file_list_cbs_t;

int menu_entries_get_title(char *title, size_t title_len);

int menu_entries_get_core_title(char *title_msg, size_t title_msg_len);

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

void menu_entries_prepend(file_list_t *list, const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx);

bool menu_entries_append_enum(file_list_t *list,
      const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx);

bool menu_entries_ctl(enum menu_entries_ctl_state state, void *data);

void menu_entries_set_checked(file_list_t *list, size_t entry_idx,
      bool checked);

enum menu_entry_type
{
   MENU_ENTRY_ACTION = 0,
   MENU_ENTRY_BOOL,
   MENU_ENTRY_INT,
   MENU_ENTRY_UINT,
   MENU_ENTRY_FLOAT,
   MENU_ENTRY_PATH,
   MENU_ENTRY_DIR,
   MENU_ENTRY_STRING,
   MENU_ENTRY_HEX,
   MENU_ENTRY_BIND,
   MENU_ENTRY_ENUM,
   MENU_ENTRY_SIZE
};

typedef struct menu_entry
{
   enum msg_hash_enums enum_idx;
   unsigned idx;
   unsigned type;
   unsigned spacing;
   size_t entry_idx;
   char path[255];
   char label[255];
   char sublabel[MENU_SUBLABEL_MAX_LENGTH];
   char rich_label[255];
   char value[255];
   char password_value[255];
   bool checked;
   bool path_enabled;
   bool label_enabled;
   bool rich_label_enabled;
   bool value_enabled;
   bool sublabel_enabled;
} menu_entry_t;

enum menu_entry_type menu_entry_get_type(uint32_t i);

void menu_entry_get_path(menu_entry_t *entry, const char **path);

void menu_entry_get_label(menu_entry_t *entry, const char **label);

unsigned menu_entry_get_spacing(menu_entry_t *entry);

unsigned menu_entry_get_type_new(menu_entry_t *entry);

uint32_t menu_entry_get_bool_value(uint32_t i);

struct string_list *menu_entry_enum_values(uint32_t i);

void menu_entry_enum_set_value_with_string(uint32_t i, const char *s);

int32_t menu_entry_bind_index(uint32_t i);

void menu_entry_bind_key_set(uint32_t i, int32_t value);

void menu_entry_bind_joykey_set(uint32_t i, int32_t value);

void menu_entry_bind_joyaxis_set(uint32_t i, int32_t value);

void menu_entry_pathdir_selected(uint32_t i);

bool menu_entry_pathdir_allow_empty(uint32_t i);

uint32_t menu_entry_pathdir_for_directory(uint32_t i);

void menu_entry_pathdir_extensions(uint32_t i, char *s, size_t len);

void menu_entry_reset(uint32_t i);

void menu_entry_get_rich_label(menu_entry_t *entry, const char **rich_label);

void menu_entry_get_sublabel(menu_entry_t *entry, const char **sublabel);

void menu_entry_get_value(menu_entry_t *entry, const char **value);

void menu_entry_set_value(uint32_t i, const char *s);

bool menu_entry_is_password(menu_entry_t *entry);

uint32_t menu_entry_num_has_range(uint32_t i);

float menu_entry_num_min(uint32_t i);

float menu_entry_num_max(uint32_t i);

bool menu_entry_is_currently_selected(unsigned id);

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation);

int menu_entry_select(uint32_t i);

int menu_entry_action(
      menu_entry_t *entry, size_t i, enum menu_action action);

void menu_entry_init(menu_entry_t *entry);

void get_current_menu_value(char* retstr, size_t max);
void get_current_menu_label(char* retstr, size_t max);
void get_current_menu_sublabel(char* retstr, size_t max);

RETRO_END_DECLS

#endif
