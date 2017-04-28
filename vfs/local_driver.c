/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "vfs_driver.h"

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <retro_stat.h>
#include <string.h>

// TODO
void retro_rewinddir(struct RDIR** entry, const char* path)
{
   retro_closedir(*entry);

   *entry = retro_opendir(path);

   if (!*entry)
      return;

   if (retro_dirent_error(*entry))
   {
      retro_closedir(*entry);
      *entry = NULL;
   }
}

void add_file_to_folder(const char* folder_path, const char* file, char* target_path, size_t target_path_size)
{
   strlcpy(target_path, folder_path, target_path_size - 1);

   // Trim trailing slash
   size_t path_len = strlen(target_path);
   if (path_len > 0)
   {
      char end_char = target_path[path_len - 1];
      if (end_char == '/' || end_char == '\\')
         target_path[path_len - 1] = '\0';
   }

   // Add interstertial slash
#if defined(_WIN32)
   strlcat(target_path, "\\", sizeof(target_path) - 1);
#else
   strlcat(target_path, "/", sizeof(target_path) - 1);
#endif

   // Append filename
   strlcat(target_path, file, sizeof(target_path) - 1);
}

bool local_vfs_translate_path(const char *path, char* target_dir, size_t target_dir_size)
{
   const size_t protocol_len = strlen("file://");

   if (strncmp(path, "file://", protocol_len) == 0 && path[protocol_len] != '\0')
   {
      strlcpy(target_dir, path + protocol_len, target_dir_size - 1);
      return true;
   }

   return false;
}

void local_vfs_init(void)
{
}

void local_vfs_deinit()
{
}

bool local_vfs_stat_file(const char *path, struct retro_file_info *buffer)
{
   if (!buffer)
      return false;

   char local_path[PATH_MAX];
   if (local_vfs_translate_path(path, local_path, sizeof(local_path)))
   {
      if (path_is_valid(local_path))
      {
         /* path */
         strlcpy(buffer->path, path, sizeof(buffer->path) - 1);

         /* directory flag */
         buffer->is_directory = path_is_directory(local_path);

         /* size */
         buffer->size = path_get_size(local_path);

         return true;
      }
   }

   return false;
}

bool local_vfs_remove_file(const char *path)
{
   return false; // TODO
}

bool local_vfs_create_directory(const char *path)
{
   char local_path[PATH_MAX];
   if (local_vfs_translate_path(path, local_path, sizeof(local_path)))
      return path_mkdir(local_path);

   return false;
}

bool local_vfs_remove_directory(const char *path)
{
   return false; // TODO
}

bool local_vfs_list_directory(const char *path, char ***items, unsigned int *item_count)
{
   if (!items || !item_count)
      return false;

   *items = NULL;
   *item_count = 0;

   char local_path[PATH_MAX];
   if (local_vfs_translate_path(path, local_path, sizeof(local_path)))
   {
      struct RDIR *entry = retro_opendir(local_path);

      if (!entry)
         return false;

      if (retro_dirent_error(entry))
      {
         retro_closedir(entry);
         return false;
      }

      unsigned int read_count = 0;
      while (retro_readdir(entry))
      {
         const char* name = retro_dirent_get_name(entry);

         if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

         read_count++;
      }

      if (read_count > 0)
      {
         // TODO: rewinddir()
         retro_rewinddir(&entry, local_path);
         if (!entry)
            return false;

         *items = (char**)malloc(read_count * sizeof(char**));

         unsigned int i = 0;
         while (retro_readdir(entry) && i < read_count)
         {
            const char* name = retro_dirent_get_name(entry);

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
               continue;

            char item_path[PATH_MAX];
            add_file_to_folder(path, name, item_path, sizeof(item_path));

            char* item = (char*)malloc(strlen(item_path) + 1);
            strcpy(item, item_path);

            *items[i] = item;
            i++;
         }

         *item_count = i;
      }

      retro_closedir(entry);

      return true;
   }

   return false;
}

struct vfs_driver_t vfs_local_driver = {
   local_vfs_init,
   local_vfs_deinit,
   local_vfs_stat_file,
   local_vfs_remove_file,
   local_vfs_create_directory,
   local_vfs_remove_directory,
   local_vfs_list_directory,
};
