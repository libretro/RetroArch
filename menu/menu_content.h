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
   MENU_CONTENT_CTL_LOAD,

   /* Initializes core and loads content 
    * (based on playlist entry). */
   MENU_CONTENT_CTL_LOAD_PLAYLIST,

   /* Find first core that is compatible with the
    * content.
    *
    * Returns false if there are multiple compatible
    * cores and a selection needs to be made from
    * a list. 
    *
    * Returns true and fills in @s with path to core.
    */
   MENU_CONTENT_CTL_FIND_FIRST_CORE
};

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

bool menu_content_ctl(enum menu_content_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
