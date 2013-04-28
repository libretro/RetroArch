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

#ifndef ROM_HISTORY_H__
#define ROM_HISTORY_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rom_history rom_history_t;

rom_history_t *rom_history_init(const char *path, size_t size);
void rom_history_free(rom_history_t *hist);

size_t rom_history_size(rom_history_t *hist);

void rom_history_get_index(rom_history_t *hist,
      size_t index,
      const char **path, const char **core_path,
      const char **core_name);

void rom_history_push(rom_history_t *hist,
      const char *path, const char *core_path,
      const char *core_name);

#ifdef __cplusplus
}
#endif

#endif

