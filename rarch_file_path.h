/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef __RARCH_FILE_PATH_H
#define __RARCH_FILE_PATH_H

#include <boolean.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <string/string_list.h>
#include "file_path.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_COMPRESSION
long read_compressed_file(const char * path, void **buf,
      const char* optional_filename);
#endif

long read_file(const char *path, void **buf);

bool read_file_string(const char *path, char **buf);

bool write_file(const char *path, const void *buf, size_t size);

bool write_empty_file(const char *path);

struct string_list *compressed_file_list_new(const char *filename,
      const char* ext);

#ifdef __cplusplus
}
#endif

#endif
