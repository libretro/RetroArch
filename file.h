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


#ifndef __RARCH_FILE_H
#define __RARCH_FILE_H

#include "boolean.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "general.h"

// Generic file, path and directory handling.

ssize_t read_file(const char *path, void **buf);

bool load_state(const char *path);
bool save_state(const char *path);

void load_ram_file(const char *path, int type);
void save_ram_file(const char *path, int type);

bool init_rom_file(enum rarch_game_type type);

// Returns a NULL-terminated list of files in a directory with full paths.
// If ext is NULL, any file will be picked.
// If non-NULL, only files with extension ext are added.
char **dir_list_new(const char *dir, const char *ext, bool include_dirs);
size_t dir_list_size(char * const *dir_list);
void dir_list_sort(char **dir_list, bool dir_first);
void dir_list_free(char **dir_list);

bool path_is_directory(const char *path);
bool path_file_exists(const char *path);

// Path-name operations.
// If any of these operation are detected to overflow, the application will be terminated.

// Replaces filename extension with 'replace' and outputs result to out_path.
// The extension here is considered to be the string from the last '.' to the end.
// If no '.' is present, in_path and replace will simply be concatenated.
// 'size' is buffer size of 'out_path'.
// I.e.: in_path = "/foo/bar/baz/boo.c", replace = ".asm" => out_path = "/foo/bar/baz/boo.asm" 
// I.e.: in_path = "/foo/bar/baz/boo.c", replace = ""     => out_path = "/foo/bar/baz/boo" 
void fill_pathname(char *out_path, const char *in_path, const char *replace, size_t size);

// Appends a filename extension 'replace' to 'in_path', and outputs result in 'out_path'.
// Assumes in_path has no extension. If an extension is still present in 'in_path', it will be ignored.
// 'size' is buffer size of 'out_path'.
void fill_pathname_noext(char *out_path, const char *in_path, const char *replace, size_t size);

// Appends basename of 'in_basename', to 'in_dir', along with 'replace'.
// Basename of in_basename is the string after the last '/' or '\\', i.e the filename without directories.
// If in_basename has no '/' or '\\', the whole 'in_basename' will be used.
// 'size' is buffer size of 'in_dir'.
// I.e.: in_dir = "/tmp/some_dir", in_basename = "/some_roms/foo.c", replace = ".asm" =>
//    in_dir = "/tmp/some_dir/foo.c.asm"
void fill_pathname_dir(char *in_dir, const char *in_basename, const char *replace, size_t size);

// Copies basename of in_path into out_path.
void fill_pathname_base(char *out_path, const char *in_path, size_t size);

#endif
