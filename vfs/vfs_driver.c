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
#include <stdlib.h>
#include <string.h>

static struct vfs_driver_t *vfs_drivers[] = {
   &vfs_local_driver,
   &vfs_retro_driver,
   NULL,
};

struct vfs_driver_t *vfs_get_driver(const char* path)
{
   if (strcmp("file://", path) == 0)
      return &vfs_local_driver;

   if (strcmp("retro://", path) == 0)
      return &vfs_retro_driver;

   return NULL;
}

void init_vfs(void)
{
   for (unsigned int i = 0; vfs_drivers[i] != NULL; i++)
      vfs_drivers[i]->init();
}

void deinit_vfs(void)
{
   for (unsigned int i = 0; vfs_drivers[i] != NULL; i++)
      vfs_drivers[i]->deinit();
}

bool vfs_translate_path(const char *path, char* target_dir, size_t target_dir_size)
{
   struct retro_file_info file_info;

   if (vfs_stat_file(path, &file_info))
   {
      strlcpy(target_dir, file_info.path, target_dir_size - 1);
      return *target_dir != '\0';
   }

   return false;
}

bool vfs_stat_file(const char *path, struct retro_file_info *buffer)
{
   struct vfs_driver_t *driver = vfs_get_driver(path);
   if (driver)
      return driver->stat_file(path, buffer);

   return false;
}

bool vfs_remove_file(const char *path)
{
   struct vfs_driver_t *driver = vfs_get_driver(path);
   if (driver)
      return driver->remove_file(path);

   return false;
}

bool vfs_create_directory(const char *path)
{
   struct vfs_driver_t *driver = vfs_get_driver(path);
   if (driver)
      return driver->create_directory(path);

   return false;
}

bool vfs_remove_directory(const char *path)
{
   struct vfs_driver_t *driver = vfs_get_driver(path);
   if (driver)
      return driver->remove_directory(path);

   return false;
}

bool vfs_list_directory(const char *path, char ***items, unsigned int *item_count)
{
   struct vfs_driver_t *driver = vfs_get_driver(path);
   if (driver)
      return driver->list_directory(path, items, item_count);

   return false;
}

void vfs_free_directory(char **items, unsigned int item_count)
{
   if (items != NULL && item_count > 0)
   {
      for (unsigned int i = 0; i < item_count; i++)
         free(items[i]);
      free(items);
   }
}
