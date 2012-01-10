/*************************************************************************************
 *  -- Cellframework Mk.II -  Open framework to abstract the common tasks related to
 *                            PS3 application development.
 *
 *  Copyright (C) 2010-2012
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ********************************************************************************/

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

#include <stdlib.h>
#include "file_browser.h"

static int less_than_key(const void * a, const void * b)
{
	DirectoryEntry * a_dir = (DirectoryEntry*)a;
	DirectoryEntry * b_dir = (DirectoryEntry*)b;

	/* compare a directory to a file directory is always lesser than*/
	if ((a_dir->d_type == CELL_FS_TYPE_DIRECTORY && b_dir->d_type == CELL_FS_TYPE_REGULAR))
		return -1;
	else if (a_dir->d_type == CELL_FS_TYPE_REGULAR && b_dir->d_type == CELL_FS_TYPE_DIRECTORY)
		return 1;

	return strcasecmp(a_dir->d_name, b_dir->d_name);
}

static const char * filebrowser_get_extension(const char * filename)
{
	const char * ext = strrchr(filename, '.');
	if (ext)
		return ext+1;
	else
		return "";
}

static void filebrowser_clear_current_entries(filebrowser_t * filebrowser)
{
	for(uint32_t i = 0; i < MAX_FILE_LIMIT_CFS; i++)
	{
		filebrowser->cur[filebrowser->file_count].d_type = 0;
		filebrowser->cur[filebrowser->file_count].d_namlen = 0;
		strcpy(filebrowser->cur[filebrowser->file_count].d_name, "\0");
	}
}

static bool filebrowser_parse_directory(filebrowser_t * filebrowser, const char * path, const char * extensions)
{
	int fd;

	/* bad path*/
	if (strcmp(path,"") == 0)
		return false;

	/* delete old path*/
	filebrowser_clear_current_entries(filebrowser);

	if (cellFsOpendir(path, &fd) == CELL_FS_SUCCEEDED)
	{
		uint64_t nread = 0;

		strcpy(filebrowser->dir[filebrowser->directory_stack_size], path);

		filebrowser->file_count = 0;

		filebrowser->currently_selected = 0;

		CellFsDirent dirent;
		while (cellFsReaddir(fd, &dirent, &nread) == CELL_FS_SUCCEEDED)
		{
			if (nread == 0)
				break;

			if ((dirent.d_type != CELL_FS_TYPE_REGULAR) && (dirent.d_type != CELL_FS_TYPE_DIRECTORY))
				continue;

			if (dirent.d_type == CELL_FS_TYPE_DIRECTORY && !(strcmp(dirent.d_name, ".")))
				continue;

			if (dirent.d_type == CELL_FS_TYPE_REGULAR)
			{
				char tmp_extensions[512];
				strncpy(tmp_extensions, extensions, sizeof(tmp_extensions));
				const char * current_extension = filebrowser_get_extension(dirent.d_name);
				bool found_rom = false;

				if(current_extension)
				{
					char * pch = strtok(tmp_extensions, "|");
					while (pch != NULL)
					{
						if(strcmp(current_extension, pch) == 0)
						{
							found_rom = true;
							break;
						}
						pch = strtok(NULL, "|");
					}
				}

				if(!found_rom)
					continue;
			}


			filebrowser->cur[filebrowser->file_count].d_type = dirent.d_type;
			filebrowser->cur[filebrowser->file_count].d_namlen = dirent.d_namlen;
			strcpy(filebrowser->cur[filebrowser->file_count].d_name, dirent.d_name);

			++filebrowser->file_count;
		}

		cellFsClosedir(fd);
	}
	else
		return false;

	qsort(filebrowser->cur, filebrowser->file_count, sizeof(DirectoryEntry), less_than_key);

	return true;
}

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, const char * extensions)
{
	filebrowser->directory_stack_size = 0;
	strncpy(filebrowser->extensions, extensions, sizeof(filebrowser->extensions));

	filebrowser_parse_directory(filebrowser, start_dir, filebrowser->extensions);
}


void filebrowser_reset_start_directory(filebrowser_t * filebrowser, const char * start_dir, const char * extensions)
{
	filebrowser_clear_current_entries(filebrowser);
	filebrowser->directory_stack_size = 0;
	strncpy(filebrowser->extensions, extensions, sizeof(filebrowser->extensions));

	filebrowser_parse_directory(filebrowser, start_dir, filebrowser->extensions);
}

void filebrowser_push_directory(filebrowser_t * filebrowser, const char * path, bool with_extension)
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

	filebrowser_parse_directory(filebrowser, filebrowser->dir[filebrowser->directory_stack_size], filebrowser->extensions);
}
