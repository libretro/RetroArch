/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <string.h>
#include <formats/image.h>
#include "menu_media_browser.h"
#include "../configuration.h"

bool menu_media_open_selected_movie(struct menu_state *menu_st,
      gfx_thumbnail_t *primary, gfx_thumbnail_t *secondary)
{
   file_list_t *list, *stack;
   const char *name, *directory;
   enum image_type_enum type;
   gfx_thumbnail_path_data_t *paths = menu_st ? menu_st->thumbnail_path_data : NULL;
   if (!menu_st || !menu_st->entries.list || !paths || !primary || !secondary) return false;
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   if (!list || menu_st->selection_ptr >= list->size || !stack || !stack->size) return false;
   name = list->list[menu_st->selection_ptr].path;
   directory = stack->list[stack->size - 1].path;
   type = image_texture_get_type(name);
   if (type != IMAGE_TYPE_MP4 && type != IMAGE_TYPE_WEBM) return false;
   if (!gfx_thumbnail_set_content_image(paths, directory, name)) return false;
   if (gfx_thumbnail_has_file(primary, paths->content_path))
      gfx_thumbnail_reset(secondary);
   else if (gfx_thumbnail_has_file(secondary, paths->content_path))
      gfx_thumbnail_reset(primary);
   else
   {
      gfx_thumbnail_reset(primary);
      gfx_thumbnail_reset(secondary);
      gfx_thumbnail_request_file(paths->content_path, primary,
            config_get_ptr()->uints.gfx_thumbnail_upscale_threshold);
   }
   return true;
}
