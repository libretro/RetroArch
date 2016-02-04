/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifdef __cplusplus
extern "C" {
#endif

enum menu_content_ctl_state
{
   MENU_CONTENT_CTL_NONE = 0,
   /* Loads content into currently selected core.
    * Will also optionally push the content entry 
    * to the history playlist. */
   MENU_CONTENT_CTL_LOAD
};

bool menu_content_ctl(enum menu_content_ctl_state state, void *data);

/**
 * menu_content_playlist_load:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
void menu_content_playlist_load(void *data, unsigned index);

/**
 * menu_content_defer_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @s                    : Deferred core path. Will be filled in
 *                         by function.
 * @len                  : Size of @s.
 *
 * Gets deferred core.
 *
 * Returns: 0 if there are multiple deferred cores and a
 * selection needs to be made from a list, otherwise
 * returns -1 and fills in @s with path to core.
 **/
int menu_content_defer_core(void *data,
      const char *dir, const char *path,
      const char *menu_label,
      char *s, size_t len);

#ifdef __cplusplus
}
#endif

#endif
