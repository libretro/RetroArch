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

#ifndef CONTENT_HISTORY_H__
#define CONTENT_HISTORY_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct content_playlist content_playlist_t;

content_playlist_t *content_playlist_init(const char *path, size_t size);
void content_playlist_free(content_playlist_t *playlist);

void content_playlist_clear(content_playlist_t *playlist);

size_t content_playlist_size(content_playlist_t *playlist);

void content_playlist_get_index(content_playlist_t *playlist,
      size_t index,
      const char **path, const char **core_path,
      const char **core_name);

void content_playlist_push(content_playlist_t *playlist,
      const char *path, const char *core_path,
      const char *core_name);

const char* content_playlist_get_path(content_playlist_t *playlist,
      unsigned index);
const char* content_playlist_get_core_path(content_playlist_t *playlist,
      unsigned index);
const char* content_playlist_get_core_name(content_playlist_t *playlist,
      unsigned index);

#ifdef __cplusplus
}
#endif

#endif

