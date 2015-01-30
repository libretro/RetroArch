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

#ifndef FILE_EXTRACT_H__
#define FILE_EXTRACT_H__

#include <boolean.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HAVE_7ZIP
#include "decompress/7zip_support.h"
#endif

#ifdef HAVE_ZLIB
#include "decompress/zip_support.h"
#endif

/* Returns true when parsing should continue. False to stop. */
typedef bool (*zlib_file_cb)(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata);

/**
 * zlib_parse_file:
 * @file                        : filename path of archive
 * @valid_exts                  : Valid extensions of archive to be parsed. 
 *                                If NULL, allow all.
 * @file_cb                     : file_cb function pointer
 * @userdata                    : userdata to pass to file_cb function pointer.
 *
 * Low-level file parsing. Enumerates over all files and calls 
 * file_cb with userdata.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool zlib_parse_file(const char *file, const char *valid_exts,
      zlib_file_cb file_cb, void *userdata);

/**
 * zlib_extract_first_content_file:
 * @zip_path                    : filename path to ZIP archive.
 * @zip_path_size               : size of ZIP archive.
 * @valid_exts                  : valid extensions for a content file.
 * @extraction_directory        : the directory to extract temporary
 *                                unzipped content to.
 *
 * Extract first content file from archive.
 *
 * Returns : true (1) on success, otherwise false (0).
 **/
bool zlib_extract_first_content_file(char *zip_path, size_t zip_path_size, 
      const char *valid_exts, const char *extraction_dir);

/**
 * zlib_get_file_list:
 * @path                        : filename path of archive
 * @valid_exts                  : Valid extensions of archive to be parsed. 
 *                                If NULL, allow all.
 *
 * Returns: string listing of files from archive on success, otherwise NULL.
 **/
struct string_list *zlib_get_file_list(const char *path, const char *valid_exts);

/**
 * zlib_inflate_data_to_file:
 * @path                        : filename path of archive.
 * @cdata                       : input data.
 * @csize                       : size of input data.
 * @size                        : output file size
 * @checksum                    : CRC32 checksum from input data.
 *
 * Decompress data to file.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool zlib_inflate_data_to_file(const char *path, const char *valid_exts,
      const uint8_t *data, uint32_t csize, uint32_t size, uint32_t crc32);

struct string_list *compressed_file_list_new(const char *filename,
      const char* ext);

#endif

