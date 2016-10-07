/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __VFS_DRIVER__H
#define __VFS_DRIVER__H

#include <libretro.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* VFS Driver API function table. */
typedef struct vfs_driver_t
{
   void (*init)(void);
   void (*deinit)(void);
   bool (*stat_file)(const char *path, struct retro_file_info *buffer);
   bool (*remove_file)(const char *path);
   bool (*create_directory)(const char *path);
   bool (*remove_directory)(const char *path);
   bool (*list_directory)(const char *path, char ***items, unsigned int *item_count);
} vfs_driver_t;

/* VFS Driver API implementations. */
extern struct vfs_driver_t vfs_local_driver;
extern struct vfs_driver_t vfs_retro_driver;

/* VFS Driver API operations. */
void init_vfs(void);
void deinit_vfs(void);
bool vfs_translate_path(const char *path, char* target_dir, size_t target_dir_size);
bool vfs_stat_file(const char *path, struct retro_file_info *buffer);
bool vfs_remove_file(const char *path);
bool vfs_create_directory(const char *path);
bool vfs_remove_directory(const char *path);
bool vfs_list_directory(const char *path, char ***items, unsigned int *item_count);
void vfs_free_directory(char **items, unsigned int item_count);

RETRO_END_DECLS

#endif
