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

#define NOD3D
#define NONET
#include <xtl.h>
#include "file_browser.h"
#include "../general.h"
#include "../file.h"

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
	for(uint32_t i = 0; i < MAX_FILE_LIMIT; i++)
	{
		filebrowser->cur[filebrowser->file_count].d_type = 0;
		filebrowser->cur[filebrowser->file_count].d_namlen = 0;
		strcpy(filebrowser->cur[filebrowser->file_count].d_name, "\0");
	}
}

void filebrowser_parse_directory(filebrowser_t * filebrowser, const char * path, const char *extensions)
{
	int error = FALSE;
   filebrowser->file_count = 0;

   WIN32_FIND_DATA ffd;
   HANDLE hFind = INVALID_HANDLE_VALUE;

   char path_buf[PATH_MAX];

   if (strlcpy(path_buf, path, sizeof(path_buf)) >= sizeof(path_buf))
   {
	  error = TRUE;
      goto error;
   }
   if (strlcat(path_buf, "\\*", sizeof(path_buf)) >= sizeof(path_buf))
   {
	  error = TRUE;
      goto error;
   }

   hFind = FindFirstFile(path_buf, &ffd);
   if (hFind == INVALID_HANDLE_VALUE)
   {
	  error = TRUE;
      goto error;
   }

   do
   {
	   strcpy(filebrowser->dir[filebrowser->directory_stack_size], path);
	    bool found_dir = false;
		if(!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			char tmp_extensions[512];
			strncpy(tmp_extensions, extensions, sizeof(tmp_extensions));
			const char * current_extension = filebrowser_get_extension(ffd.cFileName);
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
		else if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			found_dir = true;

	  filebrowser->cur[filebrowser->file_count].d_type = found_dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
      sprintf(filebrowser->cur[filebrowser->file_count].d_name, ffd.cFileName);

	  filebrowser->file_count++;
   }while (FindNextFile(hFind, &ffd) != 0 && (filebrowser->file_count + 1) < FATX_MAX_FILE_LIMIT);

error:
   if(error)
   {
	   SSNES_ERR("Failed to open directory: \"%s\"\n", path);
   }
   FindClose(hFind);
}

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, const char * extensions)
{
	filebrowser_clear_current_entries(filebrowser);
	filebrowser->directory_stack_size = 0;
	strcpy(filebrowser->extensions, extensions);

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