/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef ROM_HISTORY_H__
#define ROM_HISTORY_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rom_history rom_history_t;

rom_history_t *rom_history_init(const char *path, size_t size);
void rom_history_free(void *data);

void rom_history_clear(void *data);

size_t rom_history_size(void *data);

void rom_history_get_index(void *data,
      size_t index,
      const char **path, const char **core_path,
      const char **core_name);

void rom_history_push(void *data,
      const char *path, const char *core_path,
      const char *core_name);

const char* rom_history_get_path(void *data,
      unsigned index);
const char* rom_history_get_core_path(void *data,
      unsigned index);
const char* rom_history_get_core_name(void *data,
      unsigned index);

#ifdef __cplusplus
}
#endif

#endif

