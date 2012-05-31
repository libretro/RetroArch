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

static int less_than_key(const void * a, const void * b)
{
   DirectoryEntry * a_dir = (DirectoryEntry*)a;
   DirectoryEntry * b_dir = (DirectoryEntry*)b;

   /* compare a directory to a file directory is always lesser than*/
   if ((a_dir->d_type == FS_TYPES_DIRECTORY && b_dir->d_type == FS_TYPES_FILE))
      return -1;
   else if (a_dir->d_type == FS_TYPES_FILE && b_dir->d_type == FS_TYPES_DIRECTORY)
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
   for(uint32_t i = 0; i < MAX_FILE_LIMIT; i++)
   {
      filebrowser->cur[filebrowser->file_count].d_type = 0;
      filebrowser->cur[filebrowser->file_count].d_namlen = 0;
      strlcpy(filebrowser->cur[filebrowser->file_count].d_name, "\0", sizeof(filebrowser->cur[filebrowser->file_count].d_name));
   }
}

static void filebrowser_parse_directory(filebrowser_t * filebrowser, 
const char * path, const char * extensions)
{
   int error = 0;
#if defined(_XBOX)
   filebrowser->file_count = 0;

   WIN32_FIND_DATA ffd;
   HANDLE hFind = INVALID_HANDLE_VALUE;

   char path_buf[PATH_MAX];

   if (strlcpy(path_buf, path, sizeof(path_buf)) >= sizeof(path_buf))
   {
      error = 1;
      goto error;
   }
   if (strlcat(path_buf, "\\*", sizeof(path_buf)) >= sizeof(path_buf))
   {
      error = 1;
      goto error;
   }

   hFind = FindFirstFile(path_buf, &ffd);
   if (hFind == INVALID_HANDLE_VALUE)
   {
      error = 1;
      goto error;
   }

   do
   {
      strlcpy(filebrowser->dir[filebrowser->directory_stack_size], path, sizeof(filebrowser->dir[filebrowser->directory_stack_size]));
      bool found_dir = false;

      if(!(ffd.dwFileAttributes & FS_TYPES_DIRECTORY))
      {
         char tmp_extensions[512];
	 strlcpy(tmp_extensions, extensions, sizeof(tmp_extensions));
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
      else if (ffd.dwFileAttributes & FS_TYPES_DIRECTORY)
         found_dir = true;

      filebrowser->cur[filebrowser->file_count].d_type = found_dir ? FS_TYPES_DIRECTORY : FS_TYPES_FILE;
      snprintf(filebrowser->cur[filebrowser->file_count].d_name, sizeof(filebrowser->cur[filebrowser->file_count].d_name), ffd.cFileName);

      filebrowser->file_count++;
   }while (FindNextFile(hFind, &ffd) != 0 && (filebrowser->file_count + 1) < MAX_FILE_LIMIT);
#elif defined(__CELLOS_LV2__)
   int fd;

   /* bad path*/
   if (strcmp(path,"") == 0)
   {
      error = 1;
      goto error;
   }

   /* delete old path*/
   filebrowser_clear_current_entries(filebrowser);

   if (cellFsOpendir(path, &fd) == CELL_FS_SUCCEEDED)
   {
      uint64_t nread = 0;

      strlcpy(filebrowser->dir[filebrowser->directory_stack_size], path, sizeof(filebrowser->dir[filebrowser->directory_stack_size]));

      filebrowser->file_count = 0;
      filebrowser->currently_selected = 0;

      CellFsDirent dirent;

      while (cellFsReaddir(fd, &dirent, &nread) == CELL_FS_SUCCEEDED)
      {
         if (nread == 0)
            break;

         if ((dirent.d_type != FS_TYPES_FILE) && (dirent.d_type != FS_TYPES_DIRECTORY))
            continue;

         if (dirent.d_type == FS_TYPES_DIRECTORY && !(strcmp(dirent.d_name, ".")))
            continue;

         if (dirent.d_type == FS_TYPES_FILE)
         {
            char tmp_extensions[512];
            strlcpy(tmp_extensions, extensions, sizeof(tmp_extensions));
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
         strlcpy(filebrowser->cur[filebrowser->file_count].d_name, dirent.d_name, sizeof(filebrowser->cur[filebrowser->file_count].d_name));

         ++filebrowser->file_count;
      }

      cellFsClosedir(fd);
   }
   else
   {
      error = 1;
      goto error;
   }
#endif
   qsort(filebrowser->cur, filebrowser->file_count, sizeof(DirectoryEntry), less_than_key);
   error:
   if(error)
   {
      RARCH_ERR("Failed to open directory: \"%s\"\n", path);
   }
#ifdef _XBOX
   FindClose(hFind);
#endif
}

void filebrowser_new(filebrowser_t * filebrowser, const char * start_dir, 
const char * extensions)
{
   filebrowser_clear_current_entries(filebrowser);
   filebrowser->directory_stack_size = 0;
   strlcpy(filebrowser->extensions, extensions, sizeof(filebrowser->extensions));

   filebrowser_parse_directory(filebrowser, start_dir, filebrowser->extensions);
}


void filebrowser_reset_start_directory(filebrowser_t * filebrowser, const char * start_dir, 
const char * extensions)
{
   filebrowser->directory_stack_size = 0;
   strlcpy(filebrowser->extensions, extensions, sizeof(filebrowser->extensions));

   filebrowser_parse_directory(filebrowser, start_dir, filebrowser->extensions);
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
