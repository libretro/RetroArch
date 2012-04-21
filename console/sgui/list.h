/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef SGUI_LIST_H__
#define SGUI_LIST_H__

#include "sgui.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sgui_list sgui_list_t;

sgui_list_t *sgui_list_new(void);
void sgui_list_free(sgui_list_t *list);

void sgui_list_push(sgui_list_t *list, const char *path, sgui_file_type_t type);
void sgui_list_pop(sgui_list_t *list);
void sgui_list_clear(sgui_list_t *list);

bool sgui_list_empty(const sgui_list_t *list);
void sgui_list_back(const sgui_list_t *list,
      const char **path, sgui_file_type_t *type);

size_t sgui_list_size(const sgui_list_t *list);
void sgui_list_at(const sgui_list_t *list, size_t index,
      const char **path, sgui_file_type_t *type);

void sgui_list_sort(sgui_list_t *list);

#ifdef __cplusplus
}
#endif
#endif

