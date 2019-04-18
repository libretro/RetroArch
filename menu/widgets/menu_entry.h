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

#ifndef MENU_ENTRY_H__
#define MENU_ENTRY_H__

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include "../../msg_hash.h"

#include "../menu_input.h"

RETRO_BEGIN_DECLS

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
   char *path;
   char *label;
   char *sublabel;
   char *rich_label;
   char *value;
   bool checked;
} menu_entry_t;

enum menu_entry_type menu_entry_get_type(uint32_t i);

char *menu_entry_get_path(menu_entry_t *entry);

void menu_entry_get_label(menu_entry_t *entry, char *s, size_t len);

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

char *menu_entry_get_rich_label(menu_entry_t *entry);

char *menu_entry_get_sublabel(menu_entry_t *entry);

void menu_entry_get_value(menu_entry_t *entry, char *s, size_t len);

void menu_entry_set_value(uint32_t i, const char *s);

bool menu_entry_is_password(menu_entry_t *entry);

uint32_t menu_entry_num_has_range(uint32_t i);

float menu_entry_num_min(uint32_t i);

float menu_entry_num_max(uint32_t i);

bool menu_entry_is_currently_selected(unsigned id);

void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation);

int menu_entry_select(uint32_t i);

int menu_entry_action(menu_entry_t *entry,
                      unsigned i, enum menu_action action);

void menu_entry_free(menu_entry_t *entry);

void menu_entry_init(menu_entry_t *entry);

RETRO_END_DECLS

#endif
