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

#include <stdint.h>
#include <stdlib.h>
#include "../../../file.h"
#include "file_browser.h"

static bool directory_parse(void *data, unsigned stack_size, 
const char *path, const char * extensions)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;

   struct string_list *list = dir_list_new(path, extensions, true);

   if(list != NULL)
   {
      strlcpy(filebrowser->directory_path, path, sizeof(filebrowser->directory_path));

      if(filebrowser->current_dir.list != NULL)
         dir_list_free(filebrowser->current_dir.list);

      filebrowser->current_dir.list = list;
      filebrowser->current_dir.ptr   = 0;
      filebrowser->directory_stack_size = stack_size;
      strlcpy(filebrowser->extensions, extensions, sizeof(filebrowser->extensions));

      dir_list_sort(filebrowser->current_dir.list, true);
      
      return true;
   }
   else
      return false;
}

void filebrowser_free(void *data)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;

   dir_list_free(filebrowser->current_dir.list);
   filebrowser->current_dir.list = NULL;
   filebrowser->current_dir.ptr   = 0;
   free(filebrowser);
}

void filebrowser_set_root_and_ext(void *data, const char *ext, const char *root_dir)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   
   if (ext)
      strlcpy(filebrowser->extensions, ext, sizeof(filebrowser->extensions));

   strlcpy(filebrowser->root_dir, root_dir, sizeof(filebrowser->root_dir));
   filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_RESET);
}


static bool filebrowser_pop_directory (void *data)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   bool ret = true;
   char previous_dir[PATH_MAX], directory_path_tmp[PATH_MAX];
   unsigned pop_dir = filebrowser->directory_stack_size;

   if (filebrowser->directory_stack_size > 0)
      pop_dir -= 1;

   fill_pathname_parent_dir(previous_dir, filebrowser->directory_path, sizeof(previous_dir));
   strlcpy(directory_path_tmp, filebrowser->directory_path, sizeof(directory_path_tmp));

   //test first if previous directory can be accessed
   ret = directory_parse(filebrowser, pop_dir, previous_dir,
   filebrowser->extensions);

   if(!ret)
   {
      //revert to previous directory
      strlcpy(filebrowser->directory_path, directory_path_tmp, sizeof(filebrowser->directory_path));
      ret = directory_parse(filebrowser, pop_dir, filebrowser->directory_path,
      filebrowser->extensions);
   }

   return ret;
}

const char *filebrowser_get_current_path (void *data)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   return filebrowser->current_dir.list->elems[filebrowser->current_dir.ptr].data;
}

bool filebrowser_iterate(void *data, unsigned action)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   bool ret = true;
   unsigned entries_to_scroll = 19;

   switch(action)
   {
      case FILEBROWSER_ACTION_UP:
         filebrowser->current_dir.ptr--;
         if (filebrowser->current_dir.ptr >= filebrowser->current_dir.list->size)
            filebrowser->current_dir.ptr = filebrowser->current_dir.list->size - 1;
         break;
      case FILEBROWSER_ACTION_DOWN:
         filebrowser->current_dir.ptr++;
         if (filebrowser->current_dir.ptr >= filebrowser->current_dir.list->size)
            filebrowser->current_dir.ptr = 0;
         break;
      case FILEBROWSER_ACTION_LEFT:
         if (filebrowser->current_dir.ptr <= 5)
            filebrowser->current_dir.ptr = 0;
         else
            filebrowser->current_dir.ptr -= 5;
         break;
      case FILEBROWSER_ACTION_RIGHT:
         filebrowser->current_dir.ptr = (min(filebrowser->current_dir.ptr + 5, 
         filebrowser->current_dir.list->size-1));
         break;
      case FILEBROWSER_ACTION_SCROLL_UP:
         if (filebrowser->current_dir.ptr <= entries_to_scroll)
            filebrowser->current_dir.ptr= 0;
         else
            filebrowser->current_dir.ptr -= entries_to_scroll;
         break;
      case FILEBROWSER_ACTION_SCROLL_DOWN:
         filebrowser->current_dir.ptr = (min(filebrowser->current_dir.ptr + 
         entries_to_scroll, filebrowser->current_dir.list->size-1));
         break;
      case FILEBROWSER_ACTION_OK:
         ret = directory_parse(filebrowser, filebrowser->directory_stack_size + 1,
               filebrowser_get_current_path(filebrowser), filebrowser->extensions);
         break;
      case FILEBROWSER_ACTION_CANCEL:
         ret = filebrowser_pop_directory(filebrowser);
         break;
      case FILEBROWSER_ACTION_RESET:
         ret = directory_parse(filebrowser, 0, filebrowser->root_dir, filebrowser->extensions);
         break;
      case FILEBROWSER_ACTION_PATH_ISDIR:
         ret = filebrowser->current_dir.list->elems[filebrowser->current_dir.ptr].attr.b;
         break;
      case FILEBROWSER_ACTION_NOOP:
      default:
         break;
   }

   return ret;
}
