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

#ifndef FILEBROWSER_H_
#define FILEBROWSER_H_

#define MAX_DIR_STACK   25

#include <stdint.h>
#include <stdlib.h>

typedef struct
{
   uint32_t directory_stack_size;
   char dir[MAX_DIR_STACK][512]; 
   struct {
	   char **elems;
	   size_t size;
	   size_t ptr;
   } current_dir;
   char extensions[PATH_MAX];
} filebrowser_t;

void filebrowser_new(filebrowser_t *filebrowser, const char * start_dir, const char * extensions);
void filebrowser_free(filebrowser_t *filebrowser);
void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path, bool with_extension);
void filebrowser_pop_directory (filebrowser_t * filebrowser);
const char * filebrowser_get_current_dir (filebrowser_t *filebrowser);
const char * filebrowser_get_current_path (filebrowser_t *filebrowser);
size_t filebrowser_get_current_index (filebrowser_t *filebrowser);
void filebrowser_set_current_at (filebrowser_t *filebrowser, size_t pos);
void filebrowser_set_current_increment (filebrowser_t *filebrowser, bool allow_wraparound);
void filebrowser_set_current_decrement (filebrowser_t *filebrowser, bool allow_wraparound);

#endif /* FILEBROWSER_H_ */
