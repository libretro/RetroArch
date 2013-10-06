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

#ifdef __cplusplus
extern "C" {
#endif

// Generic file, path and directory handling.

ssize_t read_file(const char *path, void **buf);
bool write_file(const char *path, const void *buf, size_t size);

bool load_state(const char *path);
bool save_state(const char *path);

void load_ram_file(const char *path, int type);
void save_ram_file(const char *path, int type);

bool init_rom_file(enum rarch_game_type type);

// Yep, this is C alright ;)
union string_list_elem_attr
{
   bool  b;
   int   i;
   void *p;
};

struct string_list_elem
{
   char *data;
   union string_list_elem_attr attr;
};

struct string_list
{
   struct string_list_elem *elems;
   size_t size;
   size_t cap;
};

struct string_list *dir_list_new(const char *dir, const char *ext, bool include_dirs);
void dir_list_sort(struct string_list *list, bool dir_first);
void dir_list_free(struct string_list *list);
bool string_list_find_elem(const struct string_list *list, const char *elem);
bool string_list_find_elem_prefix(const struct string_list *list, const char *prefix, const char *elem);
struct string_list *string_split(const char *str, const char *delim);
struct string_list *string_list_new(void);
bool string_list_append(struct string_list *list, const char *elem, union string_list_elem_attr attr);
void string_list_free(struct string_list *list);

bool path_is_directory(const char *path);
bool path_file_exists(const char *path);

// Gets extension of file. Only '.'s after the last slash are considered.
const char *path_get_extension(const char *path);

bool path_mkdir(const char *dir);

// Removes all text after and including the last '.'.
// Only '.'s after the last slash are considered.
char *path_remove_extension(char *path);

// Returns basename from path.
const char *path_basename(const char *path);

// Extracts base directory by mutating path. Keeps trailing '/'.
void path_basedir(char *path);

// Extracts parent directory by mutating path.
// Assumes that path is a directory. Keeps trailing '/'.
void path_parent_dir(char *path);

// Turns relative paths into absolute path.
// If relative, rebases on current working dir.
void path_resolve_realpath(char *buf, size_t size);

bool path_is_absolute(const char *path);

// Path-name operations.
// If any of these operation are detected to overflow, the application will be terminated.

// Replaces filename extension with 'replace' and outputs result to out_path.
// The extension here is considered to be the string from the last '.' to the end.
// Only '.'s after the last slash are considered as extensions.
// If no '.' is present, in_path and replace will simply be concatenated.
// 'size' is buffer size of 'out_path'.
// E.g.: in_path = "/foo/bar/baz/boo.c", replace = ".asm" => out_path = "/foo/bar/baz/boo.asm" 
// E.g.: in_path = "/foo/bar/baz/boo.c", replace = ""     => out_path = "/foo/bar/baz/boo" 
void fill_pathname(char *out_path, const char *in_path, const char *replace, size_t size);

void fill_dated_filename(char *out_filename, const char *ext, size_t size);

// Appends a filename extension 'replace' to 'in_path', and outputs result in 'out_path'.
// Assumes in_path has no extension. If an extension is still present in 'in_path', it will be ignored.
// 'size' is buffer size of 'out_path'.
void fill_pathname_noext(char *out_path, const char *in_path, const char *replace, size_t size);

// Appends basename of 'in_basename', to 'in_dir', along with 'replace'.
// Basename of in_basename is the string after the last '/' or '\\', i.e the filename without directories.
// If in_basename has no '/' or '\\', the whole 'in_basename' will be used.
// 'size' is buffer size of 'in_dir'.
// E.g..: in_dir = "/tmp/some_dir", in_basename = "/some_roms/foo.c", replace = ".asm" =>
//    in_dir = "/tmp/some_dir/foo.c.asm"
void fill_pathname_dir(char *in_dir, const char *in_basename, const char *replace, size_t size);

// Copies basename of in_path into out_path.
void fill_pathname_base(char *out_path, const char *in_path, size_t size);

// Copies base directory of in_path into out_path.
// If in_path is a path without any slashes (relative current directory), out_path will get path "./".
void fill_pathname_basedir(char *out_path, const char *in_path, size_t size);

// Copies parent directory of in_dir into out_dir.
// Assumes in_dir is a directory. Keeps trailing '/'.
void fill_pathname_parent_dir(char *out_dir, const char *in_dir, size_t size);

// Joins basedir of in_refpath together with in_path.
// If in_path is an absolute path, out_path = in_path.
// E.g.: in_refpath = "/foo/bar/baz.a", in_path = "foobar.cg", out_path = "/foo/bar/foobar.cg".
void fill_pathname_resolve_relative(char *out_path, const char *in_refpath, const char *in_path, size_t size);

// Joins a directory and path together. Makes sure not to get two consecutive slashes between dir and path.
void fill_pathname_join(char *out_path, const char *dir, const char *path, size_t size);

#ifndef RARCH_CONSOLE
void fill_pathname_application_path(char *buf, size_t size);
#endif

#ifdef __cplusplus
}
#endif

#endif
