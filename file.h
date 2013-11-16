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

#ifndef __RARCH_FILE_H
#define __RARCH_FILE_H

#include "boolean.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "general.h"
#include "file_path.h"

#ifdef __cplusplus
extern "C" {
#endif

// Handles files related to libretro.

bool load_state(const char *path);
bool save_state(const char *path);

void load_ram_file(const char *path, int type);
void save_ram_file(const char *path, int type);

bool init_rom_file(enum rarch_game_type type);

#ifdef __cplusplus
}
#endif

#endif
