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
#include "config.h"
#include "configuration.h"
#include "dirs.h"
#include "paths.h"
#include "verbosity.h"

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include <string.h>

#ifndef MIN
#  define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

/* TODO: move to URL-parsing module */
bool get_hostname(const char *url, char *hostname, size_t hostname_max_size)
{
   if (hostname_max_size == 0)
      return false;

   const char *scheme_separator = strchr(url, ':');
   if (!scheme_separator)
      return false;

   const char *hostname_start = scheme_separator + 1;
   while (*hostname_start == '/' && *hostname_start != '\0')
   {
      /* Don't proceed past two slashes */
      if (hostname_start - scheme_separator >= 2)
         break;

      hostname_start++;
   }

   const char *hostname_end = hostname_start;
   while (*hostname_end != '/' && *hostname_end != '\0')
      hostname_end++;

   unsigned int hostname_length = hostname_end - hostname_start;

   strlcpy(hostname, hostname_start, MIN(hostname_length, hostname_max_size - 1));

   return true;
}

bool has_protocol(const char* path)
{
   return strstr(path, "://") != NULL;
}

void set_protocol(const char* path, const char* protocol, char* target_path, size_t target_size)
{
   // Truncate path of existing protocol
   if (has_protocol(path))
      path = strstr(path, "://") + strlen("://");

   strlcpy(target_path, protocol, target_size - 1);
   size_t protocol_len = strlen(target_path);

   strlcpy(target_path + protocol_len, "://", target_size - protocol_len - 1);
   protocol_len += strlen("://");

   strlcpy(target_path + protocol_len, path, target_size - protocol_len - 1);
}

void get_assets_dir(char* target_dir, size_t target_dir_size)
{
   settings_t *settings = config_get_ptr();

   if (!string_is_empty(settings->directory.core_assets))
   {
      const char* assets_dir = settings->directory.core_assets;
      if (has_protocol(assets_dir))
      {
         strlcpy(target_dir, assets_dir, target_dir_size - 1);
      }
      else
      {
         char prefixed_assets_dir[PATH_MAX];
         set_protocol(assets_dir, "file", prefixed_assets_dir, sizeof(prefixed_assets_dir));
         strlcpy(target_dir, prefixed_assets_dir, target_dir_size - 1);
      }
   }
}

void get_core_dir(char* target_dir, size_t target_dir_size)
{
#ifdef HAVE_DYNAMIC
   const char* core_dir = path_get(RARCH_PATH_CORE);
   if (!string_is_empty(core_dir))
   {
      if (has_protocol(core_dir))
      {
         strlcpy(target_dir, core_dir, target_dir_size - 1);
      }
      else
      {
         char prefixed_core_dir[PATH_MAX];
         set_protocol(core_dir, "file", prefixed_core_dir, sizeof(prefixed_core_dir));
         strlcpy(target_dir, prefixed_core_dir, target_dir_size - 1);
      }

      // Remove basename
      const char* basename = path_basename(target_dir);
      if (target_dir != basename)
         target_dir[strlen(target_dir) - strlen(basename)] = '\0';
   }
#endif
}

void get_game_dir(char* target_dir, size_t target_dir_size)
{
   // TODO
}

void get_save_dir(char* target_dir, size_t target_dir_size)
{
   const char* save_dir = dir_get(RARCH_DIR_CURRENT_SAVEFILE);
   if (!string_is_empty(save_dir))
   {
      if (has_protocol(save_dir))
      {
         strlcpy(target_dir, save_dir, target_dir_size - 1);
      }
      else
      {
         char prefixed_save_dir[PATH_MAX];
         set_protocol(save_dir, "file", prefixed_save_dir, sizeof(prefixed_save_dir));
         strlcpy(target_dir, prefixed_save_dir, target_dir_size - 1);
      }
   }
}

void get_system_dir(char* target_dir, size_t target_dir_size)
{
   settings_t *settings = config_get_ptr();

   if (!string_is_empty(settings->directory.system))
   {
      const char* system_dir = dir_get(RARCH_DIR_CURRENT_SAVEFILE);
      if (has_protocol(system_dir))
      {
         strlcpy(target_dir, system_dir, target_dir_size - 1);
      }
      else
      {
         char prefixed_system_dir[PATH_MAX];
         set_protocol(system_dir, "file", prefixed_system_dir, sizeof(prefixed_system_dir));
         strlcpy(target_dir, prefixed_system_dir, target_dir_size - 1);
      }
   }
}

bool retro_vfs_translate_path(const char *path, char* target_dir, size_t target_dir_size)
{
   if (!path || strcmp(path, "retro://") != 0)
      return false;

   *target_dir = '\0';

   char hostname[16];
   if (get_hostname(path, hostname, sizeof(hostname)))
   {
      if (strcmp(hostname, "assets") == 0)
         get_assets_dir(target_dir, target_dir_size - 1);

      else if (strcmp(hostname, "core") == 0)
         get_core_dir(target_dir, target_dir_size - 1);

      else if (strcmp(hostname, "game") == 0)
         get_game_dir(target_dir, target_dir_size - 1);

      else if (strcmp(hostname, "save") == 0)
         get_save_dir(target_dir, target_dir_size - 1);

      else if (strcmp(hostname, "system") == 0)
         get_system_dir(target_dir, target_dir_size - 1);
   }

   if (*target_dir == '\0')
      return false;

   /* Append remaining path */
   const char *remaining_path = path + strlen("retro://") + strlen(hostname);
   if (*remaining_path == '/')
      remaining_path++;

   strlcpy(target_dir + strlen(target_dir), remaining_path, target_dir_size - strlen(path));

   return true;
}

static void retro_vfs_init(void)
{
}

static void retro_vfs_deinit()
{
}

bool retro_vfs_stat_file(const char *path, struct retro_file_info *buffer)
{
  char translated_path[PATH_MAX];

  if (retro_vfs_translate_path(path, translated_path, sizeof(translated_path)))
     return vfs_stat_file(translated_path, buffer);

   return false;
}

bool retro_vfs_remove_file(const char *path)
{
   char translated_path[PATH_MAX];

   if (retro_vfs_translate_path(path, translated_path, sizeof(translated_path)))
      return vfs_remove_file(translated_path);

   return false;
}

bool retro_vfs_create_directory(const char *path)
{
   char translated_path[PATH_MAX];

   if (retro_vfs_translate_path(path, translated_path, sizeof(translated_path)))
      return vfs_create_directory(translated_path);

   return false;
}

bool retro_vfs_remove_directory(const char *path)
{
   char translated_path[PATH_MAX];

   if (retro_vfs_translate_path(path, translated_path, sizeof(translated_path)))
      return vfs_remove_directory(translated_path);

   return false;
}

bool retro_vfs_list_directory(const char *path, char ***items, unsigned int *item_count)
{
   char translated_path[PATH_MAX];

   if (retro_vfs_translate_path(path, translated_path, sizeof(translated_path)))
      return vfs_list_directory(translated_path, items, item_count);

   return false;
}

struct vfs_driver_t vfs_retro_driver = {
   retro_vfs_init,
   retro_vfs_deinit,
   retro_vfs_stat_file,
   retro_vfs_remove_file,
   retro_vfs_create_directory,
   retro_vfs_remove_directory,
   retro_vfs_list_directory,
};
