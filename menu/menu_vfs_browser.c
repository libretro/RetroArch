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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_common_api.h>
#include <boolean.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <vfs/vfs_implementation.h>

#include "menu_vfs_browser.h"
#include "menu_driver.h"
#include "../retroarch.h"
#include "../verbosity.h"

/* VFS Browser state */
typedef struct
{
   libretro_vfs_implementation_dir *dir;
   char current_path[PATH_MAX_LENGTH];
   char **entry_names;
   bool *entry_is_dir;
   uint64_t *entry_sizes;
   size_t entry_count;
   size_t entry_capacity;
   bool initialized;
   enum vfs_scheme scheme;
} vfs_browser_state_t;

static vfs_browser_state_t g_vfs_browser = {0};

/**
 * Internal function to read directory entries using VFS
 */
static void vfs_browser_read_dir(void)
{
   libretro_vfs_implementation_dir *vfs_dir;
   size_t i;
   const char *name;
   bool is_dir;
   uint64_t size;
   char full_path[PATH_MAX_LENGTH];
   size_t new_capacity;
   char **new_names;
   bool *new_is_dir;
   uint64_t *new_sizes;
   
   /* Clean up old entries */
   if (g_vfs_browser.entry_names)
   {
      for (i = 0; i < g_vfs_browser.entry_count; i++)
      {
         if (g_vfs_browser.entry_names[i])
            free(g_vfs_browser.entry_names[i]);
      }
      free(g_vfs_browser.entry_names);
      g_vfs_browser.entry_names = NULL;
   }
   
   if (g_vfs_browser.entry_is_dir)
   {
      free(g_vfs_browser.entry_is_dir);
      g_vfs_browser.entry_is_dir = NULL;
   }
   
   if (g_vfs_browser.entry_sizes)
   {
      free(g_vfs_browser.entry_sizes);
      g_vfs_browser.entry_sizes = NULL;
   }
   
   g_vfs_browser.entry_count = 0;
   g_vfs_browser.entry_capacity = 64;
   
   /* Allocate arrays */
   g_vfs_browser.entry_names = (char**)calloc(g_vfs_browser.entry_capacity, sizeof(char*));
   g_vfs_browser.entry_is_dir = (bool*)calloc(g_vfs_browser.entry_capacity, sizeof(bool));
   g_vfs_browser.entry_sizes = (uint64_t*)calloc(g_vfs_browser.entry_capacity, sizeof(uint64_t));
   
   if (!g_vfs_browser.entry_names || !g_vfs_browser.entry_is_dir || !g_vfs_browser.entry_sizes)
   {
      RARCH_ERR("[VFS Browser] Failed to allocate entry arrays\n");
      return;
   }
   
   /* Open directory using VFS */
   vfs_dir = retro_vfs_opendir_impl(g_vfs_browser.current_path, true);
   if (!vfs_dir)
   {
      RARCH_ERR("[VFS Browser] Failed to open directory: %s\n", g_vfs_browser.current_path);
      return;
   }
   
   g_vfs_browser.dir = vfs_dir;
   
   /* Read entries */
   while (retro_vfs_readdir_impl(vfs_dir))
   {
      name = retro_vfs_dirent_get_name_impl(vfs_dir);
      is_dir = retro_vfs_dirent_is_dir_impl(vfs_dir);
      size = 0;
      
      if (!name)
         continue;
      
      /* Skip . and .. */
      if (string_is_equal(name, ".") || string_is_equal(name, ".."))
         continue;
      
      /* Expand capacity if needed */
      if (g_vfs_browser.entry_count >= g_vfs_browser.entry_capacity)
      {
         new_capacity = g_vfs_browser.entry_capacity * 2;
         new_names = (char**)realloc(g_vfs_browser.entry_names, 
                                            new_capacity * sizeof(char*));
         new_is_dir = (bool*)realloc(g_vfs_browser.entry_is_dir, 
                                           new_capacity * sizeof(bool));
         new_sizes = (uint64_t*)realloc(g_vfs_browser.entry_sizes, 
                                                  new_capacity * sizeof(uint64_t));
         
         if (!new_names || !new_is_dir || !new_sizes)
         {
            RARCH_ERR("[VFS Browser] Failed to expand entry arrays\n");
            break;
         }
         
         g_vfs_browser.entry_names = new_names;
         g_vfs_browser.entry_is_dir = new_is_dir;
         g_vfs_browser.entry_sizes = new_sizes;
         g_vfs_browser.entry_capacity = new_capacity;
      }
      
      /* Store entry info */
      g_vfs_browser.entry_names[g_vfs_browser.entry_count] = strdup(name);
      g_vfs_browser.entry_is_dir[g_vfs_browser.entry_count] = is_dir;
      
      /* Get file size if not a directory */
      if (!is_dir)
      {
         fill_pathname_join(full_path, g_vfs_browser.current_path, name, 
                           sizeof(full_path));
         size = retro_vfs_stat_impl(full_path, NULL);
      }
      
      g_vfs_browser.entry_sizes[g_vfs_browser.entry_count] = size;
      g_vfs_browser.entry_count++;
   }
   
   retro_vfs_closedir_impl(vfs_dir);
   g_vfs_browser.dir = NULL;
   
   RARCH_LOG("[VFS Browser] Read %zu entries from %s\n", 
            g_vfs_browser.entry_count, g_vfs_browser.current_path);
}

/**
 * Initialize the VFS browser
 */
bool menu_vfs_browser_init(void)
{
   memset(&g_vfs_browser, 0, sizeof(g_vfs_browser));
   strlcpy(g_vfs_browser.current_path, "/", sizeof(g_vfs_browser.current_path));
   g_vfs_browser.scheme = VFS_SCHEME_NONE;
   g_vfs_browser.initialized = true;
   
   RARCH_LOG("[VFS Browser] Initialized\n");
   return true;
}

/**
 * Deinitialize the VFS browser
 */
void menu_vfs_browser_deinit(void)
{
   size_t i;
   
   if (!g_vfs_browser.initialized)
      return;
   
   /* Clean up entries */
   if (g_vfs_browser.entry_names)
   {
      for (i = 0; i < g_vfs_browser.entry_count; i++)
      {
         if (g_vfs_browser.entry_names[i])
            free(g_vfs_browser.entry_names[i]);
      }
      free(g_vfs_browser.entry_names);
   }
   
   if (g_vfs_browser.entry_is_dir)
      free(g_vfs_browser.entry_is_dir);
   
   if (g_vfs_browser.entry_sizes)
      free(g_vfs_browser.entry_sizes);
   
   memset(&g_vfs_browser, 0, sizeof(g_vfs_browser));
   
   RARCH_LOG("[VFS Browser] Deinitialized\n");
}

/**
 * Open VFS browser at specified path
 */
bool menu_vfs_browser_open(const char *path)
{
   if (!g_vfs_browser.initialized)
   {
      RARCH_ERR("[VFS Browser] Not initialized\n");
      return false;
   }
   
   if (path && !string_is_empty(path))
   {
      strlcpy(g_vfs_browser.current_path, path, sizeof(g_vfs_browser.current_path));
   }
   else
   {
      strlcpy(g_vfs_browser.current_path, "/", sizeof(g_vfs_browser.current_path));
   }
   
   vfs_browser_read_dir();
   return true;
}

/**
 * Navigate to parent directory
 */
bool menu_vfs_browser_parent(void)
{
   char *last_slash;
   
   if (!g_vfs_browser.initialized)
      return false;
   
   /* Don't go above root */
   if (string_is_equal(g_vfs_browser.current_path, "/"))
      return false;
   
   /* Find last slash and truncate */
   last_slash = strrchr(g_vfs_browser.current_path, '/');
   if (last_slash && last_slash != g_vfs_browser.current_path)
   {
      *last_slash = '\0';
      if (string_is_empty(g_vfs_browser.current_path))
         strlcpy(g_vfs_browser.current_path, "/", sizeof(g_vfs_browser.current_path));
   }
   
   vfs_browser_read_dir();
   return true;
}

/**
 * Navigate to subdirectory
 */
bool menu_vfs_browser_subdir(const char *name)
{
   char new_path[PATH_MAX_LENGTH];
   
   if (!g_vfs_browser.initialized || !name)
      return false;
   
   fill_pathname_join(new_path, g_vfs_browser.current_path, name, 
                     sizeof(new_path));
   strlcpy(g_vfs_browser.current_path, new_path, sizeof(g_vfs_browser.current_path));
   
   vfs_browser_read_dir();
   return true;
}

/**
 * Get current VFS path
 */
const char* menu_vfs_browser_get_path(void)
{
   return g_vfs_browser.current_path;
}

/**
 * Perform file operation
 */
bool menu_vfs_browser_operation(unsigned operation, const char *name, const char *new_name)
{
   char full_path[PATH_MAX_LENGTH];
   char new_path[PATH_MAX_LENGTH];
   
   if (!g_vfs_browser.initialized || !name)
      return false;
   
   fill_pathname_join(full_path, g_vfs_browser.current_path, name, 
                     sizeof(full_path));
   
   switch (operation)
   {
      case 0: /* Info - just return success, info is already loaded */
         return true;
         
      case 1: /* Open - would need to integrate with file viewer */
         RARCH_LOG("[VFS Browser] Open file: %s\n", full_path);
         /* TODO: Integrate with file viewer */
         return true;
         
      case 2: /* Delete */
         RARCH_LOG("[VFS Browser] Delete: %s\n", full_path);
         if (retro_vfs_file_remove_impl(full_path) == 0)
         {
            vfs_browser_read_dir(); /* Refresh */
            return true;
         }
         return false;
         
      case 3: /* Rename */
         if (!new_name)
            return false;
         fill_pathname_join(new_path, g_vfs_browser.current_path, new_name,
                           sizeof(new_path));
         RARCH_LOG("[VFS Browser] Rename: %s -> %s\n", full_path, new_path);
         if (retro_vfs_file_rename_impl(full_path, new_path) == 0)
         {
            vfs_browser_read_dir(); /* Refresh */
            return true;
         }
         return false;
         
      case 4: /* Create directory */
         RARCH_LOG("[VFS Browser] Create directory: %s\n", full_path);
         if (retro_vfs_mkdir_impl(full_path))
         {
            vfs_browser_read_dir(); /* Refresh */
            return true;
         }
         return false;
         
      default:
         return false;
   }
}

/**
 * Refresh current directory listing
 */
void menu_vfs_browser_refresh(void)
{
   if (g_vfs_browser.initialized)
      vfs_browser_read_dir();
}

/**
 * Get file count in current directory
 */
size_t menu_vfs_browser_get_count(void)
{
   return g_vfs_browser.entry_count;
}

/**
 * Get entry name at index
 */
const char* menu_vfs_browser_get_name(size_t index)
{
   if (index >= g_vfs_browser.entry_count)
      return NULL;
   return g_vfs_browser.entry_names[index];
}

/**
 * Check if entry at index is a directory
 */
bool menu_vfs_browser_is_directory(size_t index)
{
   if (index >= g_vfs_browser.entry_count)
      return false;
   return g_vfs_browser.entry_is_dir[index];
}

/**
 * Get entry size
 */
uint64_t menu_vfs_browser_get_size(size_t index)
{
   if (index >= g_vfs_browser.entry_count)
      return 0;
   return g_vfs_browser.entry_sizes[index];
}

/**
 * Get VFS scheme for current location
 */
enum vfs_scheme menu_vfs_browser_get_scheme(void)
{
   return g_vfs_browser.scheme;
}
