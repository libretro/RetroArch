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

#ifndef __RARCH_CHEATS_H
#define __RARCH_CHEATS_H

typedef struct cheat_manager cheat_manager_t;

cheat_manager_t* cheat_manager_new(const char *path);
void cheat_manager_free(cheat_manager_t *handle);

void cheat_manager_index_next(cheat_manager_t *handle);
void cheat_manager_index_prev(cheat_manager_t *handle);
void cheat_manager_toggle(cheat_manager_t *handle);

#endif
