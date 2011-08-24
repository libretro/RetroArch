/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SSNES_FILE_H
#define __SSNES_FILE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "general.h"

ssize_t read_file(const char *path, void **buf);

bool load_state(const char *path);
bool save_state(const char *path);

void load_ram_file(const char *path, int type);
void save_ram_file(const char *path, int type);

bool init_rom_file(enum ssnes_game_type type);

// Returns a NULL-terminated list of files in a directory with full paths.
char** dir_list_new(const char *dir, const char *ext);
void dir_list_free(char **dir_list);

bool path_is_directory(const char *path);
bool path_file_exists(const char *path);

// Path-name operations.
// Replaces filename extension with replace.
void fill_pathname(char *out_path, const char *in_path, const char *replace, size_t size);
// Sets filename extension. Assumes in_path has no extension.
void fill_pathname_noext(char *out_path, const char *in_path, const char *replace, size_t size);
// Concatenates in_basename and replace to in_dir.
void fill_pathname_dir(char *in_dir, const char *in_basename, const char *replace, size_t size);


#endif
