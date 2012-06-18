/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifdef _XBOX
#include <xtl.h>
#endif
#include "file_browser.h"


static void filebrowser_parse_directory(filebrowser_t * filebrowser, 
const char * path, const char * extensions)
{
   strlcpy(filebrowser->dir[filebrowser->directory_stack_size], path, sizeof(filebrowser->dir[filebrowser->directory_stack_size]));

   filebrowser->current_dir.elems = dir_list_new(path, extensions, true);
   filebrowser->current_dir.size  = dir_list_size(filebrowser->current_dir.elems);
   filebrowser->current_dir.ptr   = 0;

   dir_list_sort(filebrowser->current_dir.elems);
}

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, 
const char * extensions)
{
   filebrowser->directory_stack_size = 0;
   strlcpy(filebrowser->extensions, extensions, sizeof(filebrowser->extensions));

   filebrowser_parse_directory(filebrowser, start_dir, extensions);
}

void filebrowser_free(filebrowser_t * filebrowser)
{
   dir_list_free(filebrowser->current_dir.elems);

   filebrowser->current_dir.elems = NULL;
   filebrowser->current_dir.size  = 0;
   filebrowser->current_dir.ptr   = 0;
}

void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path,
bool with_extension)
{
   filebrowser->directory_stack_size++;
   if(with_extension)
      filebrowser_parse_directory(filebrowser, path, filebrowser->extensions);
   else
      filebrowser_parse_directory(filebrowser, path, "empty");
}

void filebrowser_pop_directory (filebrowser_t * filebrowser)
{
   if (filebrowser->directory_stack_size > 0)
      filebrowser->directory_stack_size--;

   filebrowser_parse_directory(filebrowser, filebrowser->dir[filebrowser->directory_stack_size],
   filebrowser->extensions);
}

const char * filebrowser_get_current_path (filebrowser_t *filebrowser)
{
   return filebrowser->current_dir.elems[filebrowser->current_dir.ptr];
}
