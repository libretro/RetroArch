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

#ifndef FILE_EXTRACT_H__
#define FILE_EXTRACT_H__

#include "boolean.h"
#include "file.h"
#include <stddef.h>
#include <stdint.h>

// Returns true when parsing should continue. False to stop.
typedef bool (*zlib_file_cb)(const char *name,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata);

// Low-level file parsing. Enumerates over all files and calls file_cb with userdata.
bool zlib_parse_file(const char *file, zlib_file_cb file_cb, void *userdata);

// Built with zlib_parse_file.
bool zlib_extract_first_rom(char *zip_path, size_t zip_path_size, const char *valid_exts);
struct string_list *zlib_get_file_list(const char *path);

#endif

