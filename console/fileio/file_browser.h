/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FILEBROWSER_H_
#define FILEBROWSER_H_

#define MAXJOLIET	255
#define MAX_PATH_LENGTH	1024

#include <stdint.h>
#include <stdlib.h>

#ifdef __CELLOS_LV2__
#include <stdbool.h>
#include <cell/cell_fs.h>
#include <sys/types.h>
#define FS_MAX_PATH 256
#define FS_MAX_FS_PATH_LENGTH 255
#define MAX_FILE_LIMIT 30000
#elif defined(_XBOX)
#define FS_MAX_PATH MAX_PATH
#define FS_MAX_FS_PATH_LENGTH 2048
#define MAX_FILE_LIMIT 4096
#endif

typedef struct {
   uint8_t d_type;
   uint8_t d_namlen;
   char d_name[FS_MAX_PATH];
} DirectoryEntry;

typedef struct
{
   uint32_t file_count;				/* amount of files in current dir*/
   uint32_t currently_selected;			/* currently select browser entry*/
   uint32_t directory_stack_size;
   char dir[128][FS_MAX_FS_PATH_LENGTH];	/* info of the current directory*/
   DirectoryEntry cur[MAX_FILE_LIMIT];	/* current file listing*/
   char extensions[512];			/* allowed extensions*/
} filebrowser_t;

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, const char * extensions);
void filebrowser_reset_start_directory(filebrowser_t * filebrowser, const char * start_dir, const char * extensions);
void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path, bool with_extension);
void filebrowser_pop_directory (filebrowser_t * filebrowser);

#define FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(filebrowser) (filebrowser.dir[filebrowser.directory_stack_size])
#define FILEBROWSER_GET_CURRENT_DIRECTORY_FILE_COUNT(filebrowser) (filebrowser.file_count)
#define FILEBROWSER_GOTO_ENTRY(filebrowser, i)	filebrowser.currently_selected = i;

#define FILEBROWSER_INCREMENT_ENTRY(filebrowser) \
{ \
   filebrowser.currently_selected++; \
   if (filebrowser.currently_selected >= filebrowser.file_count) \
      filebrowser.currently_selected = 0; \
}

#define FILEBROWSER_INCREMENT_ENTRY_POINTER(filebrowser) \
{ \
   filebrowser->currently_selected++; \
   if (filebrowser->currently_selected >= filebrowser->file_count) \
      filebrowser->currently_selected = 0; \
}

#define FILEBROWSER_DECREMENT_ENTRY(filebrowser) \
{ \
   filebrowser.currently_selected--; \
   if (filebrowser.currently_selected >= filebrowser.file_count) \
      filebrowser.currently_selected = filebrowser.file_count - 1; \
}

#define FILEBROWSER_DECREMENT_ENTRY_POINTER(filebrowser) \
{ \
   filebrowser->currently_selected--; \
   if (filebrowser->currently_selected >= filebrowser->file_count) \
      filebrowser->currently_selected = filebrowser->file_count - 1; \
}

#define FILEBROWSER_GET_CURRENT_FILENAME(filebrowser) (filebrowser.cur[filebrowser.currently_selected].d_name)
#define FILEBROWSER_GET_CURRENT_ENTRY_INDEX(filebrowser) (filebrowser.currently_selected)
#define FILEBROWSER_IS_CURRENT_A_FILE(filebrowser)	(filebrowser.cur[filebrowser.currently_selected].d_type == CELL_FS_TYPE_REGULAR)
#define FILEBROWSER_IS_CURRENT_A_DIRECTORY(filebrowser)	(filebrowser.cur[filebrowser.currently_selected].d_type == CELL_FS_TYPE_DIRECTORY)

#endif /* FILEBROWSER_H_ */
