/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - The RetroArch Team
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

#ifndef __MENU_VFS_BROWSER_H
#define __MENU_VFS_BROWSER_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <vfs/vfs.h>

RETRO_BEGIN_DECLS

/**
 * Initialize the VFS browser
 * @return true on success, false on failure
 */
bool menu_vfs_browser_init(void);

/**
 * Deinitialize the VFS browser
 */
void menu_vfs_browser_deinit(void);

/**
 * Open VFS browser at specified path
 * @param path Path to browse (NULL for root)
 * @return true on success
 */
bool menu_vfs_browser_open(const char *path);

/**
 * Navigate to parent directory
 * @return true on success
 */
bool menu_vfs_browser_parent(void);

/**
 * Navigate to subdirectory
 * @param name Directory name
 * @return true on success
 */
bool menu_vfs_browser_subdir(const char *name);

/**
 * Get current VFS path
 * @return Current path string
 */
const char* menu_vfs_browser_get_path(void);

/**
 * Perform file operation
 * @param operation Operation type (0=info, 1=open, 2=delete, 3=rename, 4=mkdir)
 * @param name File/directory name
 * @param new_name New name (for rename)
 * @return true on success
 */
bool menu_vfs_browser_operation(unsigned operation, const char *name, const char *new_name);

/**
 * Refresh current directory listing
 */
void menu_vfs_browser_refresh(void);

/**
 * Get file count in current directory
 * @return Number of entries
 */
size_t menu_vfs_browser_get_count(void);

/**
 * Get entry name at index
 * @param index Entry index
 * @return Entry name or NULL
 */
const char* menu_vfs_browser_get_name(size_t index);

/**
 * Check if entry at index is a directory
 * @param index Entry index
 * @return true if directory
 */
bool menu_vfs_browser_is_directory(size_t index);

/**
 * Get entry size
 * @param index Entry index
 * @return File size in bytes
 */
uint64_t menu_vfs_browser_get_size(size_t index);

/**
 * Get VFS scheme for current location
 * @return VFS scheme enum value
 */
enum vfs_scheme menu_vfs_browser_get_scheme(void);

RETRO_END_DECLS

#endif /* __MENU_VFS_BROWSER_H */

