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

#define MENU_SEARCH_FILTER_MAX_TERMS  8
#define MENU_SEARCH_FILTER_MAX_LENGTH 64

enum menu_entries_ctl_state
{
   MENU_ENTRIES_CTL_NONE = 0,
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


typedef struct menu_ctx_list
{
   const char  *path;
   char        *fullpath;
   const char  *label;
   file_list_t *list;
   void        *entry;
   size_t idx;
   size_t selection;
   size_t size;
   size_t list_size;
   unsigned entry_type;
   unsigned action;
   enum menu_list_type type;
} menu_ctx_list_t;

typedef struct menu_search_terms
{
   size_t size;
   char terms[MENU_SEARCH_FILTER_MAX_TERMS][MENU_SEARCH_FILTER_MAX_LENGTH];
} menu_search_terms_t;

typedef struct menu_file_list_cbs
{
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
   int (*action_left)(unsigned type, const char *label, bool wraparound);
   int (*action_right)(unsigned type, const char *label, bool wraparound);
   int (*action_label)(file_list_t *list,
         unsigned type, unsigned i,
         const char *label, const char *path,
         char *s, size_t len);
   int (*action_sublabel)(file_list_t *list,
         unsigned type, unsigned i,
         const char *label, const char *path,
         char *s, size_t len);
   void (*action_get_value)(file_list_t* list,
         unsigned *w, unsigned type, unsigned i,
         const char *label, char *s, size_t len,
         const char *path,
         char *path_buf, size_t path_buf_size);
   menu_search_terms_t search;
   enum msg_hash_enums enum_idx;
   char action_sublabel_cache[MENU_SUBLABEL_MAX_LENGTH];
   char action_title_cache   [512];
   bool checked;
} menu_file_list_cbs_t;

enum menu_entry_flags
{
   MENU_ENTRY_FLAG_PATH_ENABLED       = (1 << 0),
   MENU_ENTRY_FLAG_LABEL_ENABLED      = (1 << 1),
   MENU_ENTRY_FLAG_RICH_LABEL_ENABLED = (1 << 2),
   MENU_ENTRY_FLAG_VALUE_ENABLED      = (1 << 3),
   MENU_ENTRY_FLAG_SUBLABEL_ENABLED   = (1 << 4),
   MENU_ENTRY_FLAG_CHECKED            = (1 << 5)
};

typedef struct menu_entry
{
   size_t entry_idx;
   unsigned idx;
   unsigned type;
   unsigned spacing;
   enum msg_hash_enums enum_idx;
   uint8_t setting_type;
   uint8_t flags;
   char path[255];
   char label[255];
   char sublabel[MENU_SUBLABEL_MAX_LENGTH];
   char rich_label[255];
   char value[255];
   char password_value[255];
} menu_entry_t;

int menu_entries_get_title(char *title, size_t title_len);

int menu_entries_get_label(char *label, size_t label_len);

int menu_entries_get_core_title(char *title_msg, size_t title_msg_len);

file_list_t *menu_entries_get_selection_buf_ptr(size_t idx);

file_list_t *menu_entries_get_menu_stack_ptr(size_t idx);

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, enum msg_hash_enums *enum_idx, size_t *entry_idx);

menu_file_list_cbs_t *menu_entries_get_last_stack_actiondata(void);

void menu_entries_pop_stack(size_t *ptr, size_t idx, bool animate);

void menu_entries_flush_stack(const char *needle, unsigned final_type);

size_t menu_entries_get_stack_size(size_t idx);

size_t menu_entries_get_size(void);

void menu_entries_prepend(file_list_t *list,
      const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx);

bool menu_entries_append(file_list_t *list,
      const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx,
      rarch_setting_t *setting);

bool menu_entries_ctl(enum menu_entries_ctl_state state, void *data);

bool menu_entries_search_push(const char *search_term);

bool menu_entries_search_pop(void);

menu_search_terms_t *menu_entries_search_get_terms(void);

/* Convenience function: Appends list of current
 * search terms to specified string */
void menu_entries_search_append_terms_string(char *s, size_t len);

menu_search_terms_t *menu_entries_search_get_terms_internal(void);

/* Searches current menu list for specified 'needle'
 * string. If string is found, returns true and sets
 * 'idx' to the matching list entry index. */
bool menu_entries_list_search(const char *needle, size_t *idx);

/* Menu entry interface -
 *
 * This provides an abstraction of the currently displayed
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

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation);

int menu_entry_action(
      menu_entry_t *entry, size_t i, enum menu_action action);

#define MENU_ENTRY_INITIALIZE(entry) \
   entry.path[0]            = '\0'; \
   entry.label[0]           = '\0'; \
   entry.sublabel[0]        = '\0'; \
   entry.rich_label[0]      = '\0'; \
   entry.value[0]           = '\0'; \
   entry.password_value[0]  = '\0'; \
   entry.enum_idx           = MSG_UNKNOWN; \
   entry.entry_idx          = 0; \
   entry.idx                = 0; \
   entry.type               = 0; \
   entry.spacing            = 0; \
   entry.flags              = 0

RETRO_END_DECLS

#endif
