/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef _MENU_WIDGETS_LIST_H
#define _MENU_WIDGETS_LIST_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>

#include <retro_common_api.h>
#include <lists/file_list.h>

RETRO_BEGIN_DECLS

enum menu_list_type
{
   MENU_LIST_PLAIN = 0,
   MENU_LIST_HORIZONTAL,
   MENU_LIST_TABS
};

typedef struct menu_ctx_list
{
   file_list_t *list;
   size_t list_size;
   const char *path;
   char *fullpath;
   const char *label;
   size_t idx;
   unsigned entry_type;
   enum menu_list_type type;
   unsigned action;
   size_t selection;
   size_t size;
   void *entry;
} menu_ctx_list_t;

typedef struct menu_list menu_list_t;

void menu_list_free_list(file_list_t *list);

void menu_list_free(menu_list_t *menu_list);

menu_list_t *menu_list_new(void);

file_list_t *menu_list_get(menu_list_t *list, unsigned idx);

file_list_t *menu_list_get_selection(menu_list_t *list, unsigned idx);

size_t menu_list_get_stack_size(menu_list_t *list, size_t idx);

void menu_list_flush_stack(menu_list_t *list,
      size_t idx, const char *needle, unsigned final_type);

bool menu_list_pop_stack(menu_list_t *list,
      size_t idx, size_t *directory_ptr, bool animate);

RETRO_END_DECLS

#endif
