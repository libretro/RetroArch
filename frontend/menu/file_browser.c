/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "file_browser.h"

static bool directory_parse(void *data, const char *path)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;

   struct string_list *list = dir_list_new(path,
         filebrowser->current_dir.extensions, true);
   if(!list)
      return false;
   
   if (list->size)
      dir_list_sort(list, true);

   filebrowser->current_dir.ptr   = 0;
   strlcpy(filebrowser->current_dir.directory_path,
         path, sizeof(filebrowser->current_dir.directory_path));

   if (filebrowser->list)
      dir_list_free(filebrowser->list);

   filebrowser->list = list;

   return true;

}

void filebrowser_free(void *data)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;

   dir_list_free(filebrowser->list);
   filebrowser->list = NULL;
   filebrowser->current_dir.ptr   = 0;
   free(filebrowser);
}

void filebrowser_set_root_and_ext(void *data, const char *ext, const char *root_dir)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   
   if (ext)
      strlcpy(filebrowser->current_dir.extensions, ext,
            sizeof(filebrowser->current_dir.extensions));

   strlcpy(filebrowser->current_dir.root_dir,
         root_dir, sizeof(filebrowser->current_dir.root_dir));
   filebrowser_iterate(filebrowser, RGUI_ACTION_START);
}

#define GET_CURRENT_PATH(browser) (browser->list->elems[browser->current_dir.ptr].data)

bool filebrowser_iterate(void *data, unsigned action)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   bool ret = true;
   unsigned entries_to_scroll = 19;

   switch(action)
   {
      case RGUI_ACTION_UP:
         if (filebrowser->list->size)
         {
            filebrowser->current_dir.ptr--;
            if (filebrowser->current_dir.ptr >= filebrowser->list->size)
               filebrowser->current_dir.ptr = filebrowser->list->size - 1;
         }
         break;
      case RGUI_ACTION_DOWN:
         if (filebrowser->list->size)
         {
            filebrowser->current_dir.ptr++;
            if (filebrowser->current_dir.ptr >= filebrowser->list->size)
               filebrowser->current_dir.ptr = 0;
         }
         break;
      case RGUI_ACTION_LEFT:
         if (filebrowser->list->size)
         {
            if (filebrowser->current_dir.ptr <= 5)
               filebrowser->current_dir.ptr = 0;
            else
               filebrowser->current_dir.ptr -= 5;
         }
         break;
      case RGUI_ACTION_RIGHT:
         if (filebrowser->list->size)
            filebrowser->current_dir.ptr = (min(filebrowser->current_dir.ptr + 5, 
                     filebrowser->list->size-1));
         break;
      case RGUI_ACTION_SCROLL_UP:
         if (filebrowser->list->size)
         {
            if (filebrowser->current_dir.ptr <= entries_to_scroll)
               filebrowser->current_dir.ptr= 0;
            else
               filebrowser->current_dir.ptr -= entries_to_scroll;
         }
         break;
      case RGUI_ACTION_SCROLL_DOWN:
         if (filebrowser->list->size)
            filebrowser->current_dir.ptr = (min(filebrowser->current_dir.ptr + 
                     entries_to_scroll, filebrowser->list->size-1));
         break;
      case RGUI_ACTION_OK:
         ret = directory_parse(filebrowser, GET_CURRENT_PATH(filebrowser));
         break;
      case RGUI_ACTION_CANCEL:
         {
            char tmp_str[PATH_MAX];
            fill_pathname_parent_dir(tmp_str, rgui->browser->current_dir.directory_path, sizeof(tmp_str));

            if (tmp_str[0] != '\0')
            {
               fill_pathname_parent_dir(filebrowser->current_dir.directory_path,
                     filebrowser->current_dir.directory_path,
                     sizeof(filebrowser->current_dir.directory_path));

               ret = directory_parse(filebrowser, filebrowser->current_dir.directory_path);
            }
            else
               ret = false;
         }
         break;
      case RGUI_ACTION_START:
         filebrowser_set_root_and_ext(rgui->browser, NULL, default_paths.filesystem_root_dir);
#ifdef HAVE_RMENU_XUI
         filebrowser_fetch_directory_entries(RGUI_ACTION_OK);
#endif
         ret = directory_parse(filebrowser, filebrowser->current_dir.root_dir);
         break;
      default:
         break;
   }

   if (ret)
      strlcpy(filebrowser->current_dir.path, GET_CURRENT_PATH(filebrowser),
         sizeof(filebrowser->current_dir.path));

   return ret;
}

bool filebrowser_is_current_entry_dir(void *data)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   return filebrowser->list->elems[filebrowser->current_dir.ptr].attr.b;
}

bool filebrowser_reset_current_dir(void *data)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   return directory_parse(filebrowser, filebrowser->current_dir.directory_path);
}
