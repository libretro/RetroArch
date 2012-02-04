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

#define FATX_MAX_FILE_LIMIT 4096

typedef struct {
	unsigned d_type;
	char d_nam[XCONTENT_MAX_FILENAME_LENGTH];
} DirectoryEntry;

typedef struct {
	unsigned file_count;			// amount of files in current directory
	unsigned currently_selected;	// currently selected browser entry
	DirectoryEntry cur[FATX_MAX_FILE_LIMIT];		// current file listing
	char extensions[512];			// allowed file extensions
} filebrowser_t;

void filebrowser_parse_directory(filebrowser_t * filebrowser, const char * path, const char *extensions);