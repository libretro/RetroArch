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

#define MAXJOLIET	255
#define MAX_DIR_STACK   25

#include <stdint.h>
#include <stdlib.h>

#ifdef __CELLOS_LV2__
#include <stdbool.h>
#include <sys/types.h>
#define FS_MAX_PATH 256
#define FS_MAX_FS_PATH_LENGTH 255
#elif defined(_XBOX)
#define FS_MAX_PATH MAX_PATH
#define FS_MAX_FS_PATH_LENGTH 2048
#endif

typedef struct
{
   uint32_t directory_stack_size;
   char dir[MAX_DIR_STACK][FS_MAX_FS_PATH_LENGTH]; 
   struct {
	   char **elems;
	   size_t size;
	   size_t ptr;
   } current_dir;
   char extensions[FS_MAX_PATH];
} filebrowser_t;

void filebrowser_new(filebrowser_t *filebrowser, const char * start_dir, const char * extensions);
void filebrowser_free(filebrowser_t *filebrowser);
void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path, bool with_extension);
void filebrowser_pop_directory (filebrowser_t * filebrowser);

#define FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(filebrowser) (filebrowser.dir[filebrowser.directory_stack_size])
#define FILEBROWSER_GET_CURRENT_DIRECTORY_FILE_COUNT(filebrowser) (filebrowser.current_dir.size)
#define FILEBROWSER_GOTO_ENTRY(filebrowser, i)	filebrowser.current_dir.ptr = i;

#define FILEBROWSER_INCREMENT_ENTRY(filebrowser) \
{ \
   filebrowser.current_dir.ptr++; \
   if (filebrowser.current_dir.ptr >= filebrowser.current_dir.size) \
      filebrowser.current_dir.ptr = 0; \
}

#define FILEBROWSER_INCREMENT_ENTRY_POINTER(filebrowser) \
{ \
   filebrowser->current_dir.ptr++; \
   if (filebrowser->current_dir.ptr >= filebrowser->current_dir.size) \
      filebrowser->current_dir.ptr = 0; \
}

#define FILEBROWSER_DECREMENT_ENTRY(filebrowser) \
{ \
   filebrowser.current_dir.ptr--; \
   if (filebrowser.current_dir.ptr >= filebrowser.current_dir.size) \
      filebrowser.current_dir.ptr = filebrowser.current_dir.size - 1; \
}

#define FILEBROWSER_DECREMENT_ENTRY_POINTER(filebrowser) \
{ \
   filebrowser->current_dir.ptr--; \
   if (filebrowser->current_dir.ptr >= filebrowser->current_dir.size) \
      filebrowser->current_dir.ptr = filebrowser->current_dir.size - 1; \
}

#define FILEBROWSER_GET_CURRENT_FILENAME(filebrowser)    (filebrowser.current_dir.elems[filebrowser.current_dir.ptr])
#define FILEBROWSER_GET_CURRENT_ENTRY_INDEX(filebrowser) (filebrowser.current_dir.ptr)
#define FILEBROWSER_IS_CURRENT_A_FILE(filebrowser)       (path_file_exists(filebrowser.current_dir.elems[filebrowser.current_dir.ptr]))
#define FILEBROWSER_IS_CURRENT_A_DIRECTORY(filebrowser)  (path_is_directory(filebrowser.current_dir.elems[filebrowser.current_dir.ptr]))

#endif /* FILEBROWSER_H_ */
