/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
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

#include <stdint.h>

#define FATX_MAX_FILE_LIMIT 4096
#define MAX_FILE_LIMIT FATX_MAX_FILE_LIMIT

typedef struct {
	unsigned d_type;
	unsigned d_namlen;
	CHAR d_name[MAX_PATH];
} DirectoryEntry;

typedef struct {
	unsigned file_count;			// amount of files in current directory
	unsigned currently_selected;	// currently selected browser entry
	uint32_t directory_stack_size;
	char dir[128][2048];					/* info on the current directory */
	DirectoryEntry cur[MAX_FILE_LIMIT];		// current file listing
	char extensions[512];			// allowed file extensions
} filebrowser_t;

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, const char * extensions);
void filebrowser_parse_directory(filebrowser_t * filebrowser, const char * path, const char *extensions);
void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path, bool with_extension);

#define FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(filebrowser) (filebrowser.dir[filebrowser.directory_stack_size])