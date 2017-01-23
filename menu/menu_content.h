/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __MENU_CONTENT_H__
#define __MENU_CONTENT_H__

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct menu_content_ctx_playlist_info
{
   void *data;
   unsigned idx;
} menu_content_ctx_playlist_info_t;

typedef struct menu_content_ctx_defer_info
{
   void *data;
   const char *dir;
   const char *path;
   const char *menu_label;
   char *s;
   size_t len;
} menu_content_ctx_defer_info_t;

/**
 * menu_content_playlist_load:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
bool menu_content_playlist_load(menu_content_ctx_playlist_info_t *info);

bool menu_content_playlist_find_associated_core(const char *path,
      char *s, size_t len);

bool menu_content_find_first_core(menu_content_ctx_defer_info_t *def_info,
      bool load_content_with_current_core,
      char *new_core_path, size_t len);

RETRO_END_DECLS

#endif
