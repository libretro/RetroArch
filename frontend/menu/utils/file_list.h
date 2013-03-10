/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef RGUI_LIST_H__
#define RGUI_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rgui_list rgui_list_t;

rgui_list_t *rgui_list_new(void);
void rgui_list_free(rgui_list_t *list);

void rgui_list_push(rgui_list_t *list,
      const char *path, unsigned type, size_t directory_ptr);
void rgui_list_pop(rgui_list_t *list);
void rgui_list_clear(rgui_list_t *list);

bool rgui_list_empty(const rgui_list_t *list);
void rgui_list_back(const rgui_list_t *list,
      const char **path, unsigned *type, size_t *directory_ptr);

size_t rgui_list_size(const rgui_list_t *list);
void rgui_list_at(const rgui_list_t *list, size_t index,
      const char **path, unsigned *type, size_t *directory_ptr);

void rgui_list_sort(rgui_list_t *list);

#ifdef __cplusplus
}
#endif
#endif

