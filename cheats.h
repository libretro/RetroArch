/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __RARCH_CHEATS_H
#define __RARCH_CHEATS_H

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

struct item_cheat
{
   char *desc;
   bool state;
   char *code;
};

struct cheat_manager
{
   struct item_cheat *cheats;
   unsigned ptr;
   unsigned size;
   unsigned buf_size;
};

typedef struct cheat_manager cheat_manager_t;

cheat_manager_t *cheat_manager_new(unsigned size);

cheat_manager_t *cheat_manager_load(const char *path);

/**
 * cheat_manager_save:
 * @path                      : Path to cheats file (absolute path).
 *
 * Saves cheats to file on disk.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool cheat_manager_save(cheat_manager_t *handle, const char *path);

bool cheat_manager_realloc(cheat_manager_t *handle, unsigned new_size);

void cheat_manager_free(cheat_manager_t *handle);

void cheat_manager_index_next(cheat_manager_t *handle);

void cheat_manager_index_prev(cheat_manager_t *handle);

void cheat_manager_toggle(cheat_manager_t *handle);

void cheat_manager_apply_cheats(cheat_manager_t *handle);

void cheat_manager_update(cheat_manager_t *handle, unsigned handle_idx);

#ifdef __cplusplus
}
#endif

#endif
