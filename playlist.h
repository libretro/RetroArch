/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#ifndef CONTENT_HISTORY_H__
#define CONTENT_HISTORY_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct content_playlist_entry content_playlist_entry_t;
typedef struct content_playlist       content_playlist_t;

typedef int (content_playlist_sort_fun_t)(
      const content_playlist_entry_t *a,
      const content_playlist_entry_t *b);

/**
 * content_playlist_init:
 * @path            	   : Path to playlist contents file.
 * @size                : Maximum capacity of playlist size.
 *
 * Creates and initializes a playlist.
 *
 * Returns: handle to new playlist if successful, otherwise NULL
 **/
content_playlist_t *content_playlist_init(const char *path, size_t size);

/**
 * content_playlist_free:
 * @playlist        	   : Playlist handle.
 *
 * Frees playlist handle.
 */
void content_playlist_free(content_playlist_t *playlist);

/**
 * content_playlist_clear:
 * @playlist        	   : Playlist handle.
 *
 * Clears all playlist entries in playlist.
 **/
void content_playlist_clear(content_playlist_t *playlist);

/**
 * content_playlist_size:
 * @playlist        	   : Playlist handle.
 *
 * Gets size of playlist.
 * Returns: size of playlist.
 **/
size_t content_playlist_size(content_playlist_t *playlist);

const char *content_playlist_entry_get_label(
      const content_playlist_entry_t *entry);

/**
 * content_playlist_get_index:
 * @playlist        	   : Playlist handle.
 * @idx                 : Index of playlist entry.
 * @path                : Path of playlist entry.
 * @core_path           : Core path of playlist entry.
 * @core_name           : Core name of playlist entry.
 * 
 * Gets values of playlist index: 
 **/
void content_playlist_get_index(content_playlist_t *playlist,
      size_t idx,
      const char **path, const char **label,
      const char **core_path, const char **core_name,
      const char **db_name,
      const char **crc32);

/**
 * content_playlist_push:
 * @playlist        	   : Playlist handle.
 * @path                : Path of new playlist entry.
 * @core_path           : Core path of new playlist entry.
 * @core_name           : Core name of new playlist entry.
 *
 * Push entry to top of playlist.
 **/
void content_playlist_push(content_playlist_t *playlist,
      const char *path, const char *label,
      const char *core_path, const char *core_name,
      const char *db_name,
      const char *crc32);

void content_playlist_update(content_playlist_t *playlist, size_t idx,
      const char *path, const char *label,
      const char *core_path, const char *core_name,
      const char *db_name,
      const char *crc32);

void content_playlist_get_index_by_path(content_playlist_t *playlist,
      const char *search_path,
      char **path, char **label,
      char **core_path, char **core_name,
      char **db_name,
      char **crc32);

bool content_playlist_entry_exists(content_playlist_t *playlist,
      const char *path,
      const char *crc32);

void content_playlist_write_file(content_playlist_t *playlist);

void content_playlist_qsort(content_playlist_t *playlist,
      content_playlist_sort_fun_t *fn);

#ifdef __cplusplus
}
#endif

#endif

