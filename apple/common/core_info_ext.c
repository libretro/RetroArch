/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include "core_info_ext.h"

static core_info_list_t* global_core_list = 0;
static char core_config_path[PATH_MAX];

void apple_core_info_set_core_path(const char* core_path)
{
   if (global_core_list)
      core_info_list_free(global_core_list);
   
   global_core_list = core_path ? core_info_list_new(core_path) : 0;

   if (!global_core_list)
      RARCH_WARN("No cores were found at %s", core_path ? core_path : "(null");
}

void apple_core_info_set_config_path(const char* config_path)
{
   if (!config_path || strlcpy(core_config_path, config_path, sizeof(core_config_path)) >= PATH_MAX)
      *core_config_path = '\0';
}

core_info_list_t* apple_core_info_list_get(void)
{
   if (!global_core_list)
      RARCH_WARN("apple_core_info_list_get() called before apple_core_info_set_core_path()");

   return global_core_list;
}

const core_info_t* apple_core_info_list_get_by_id(const char* core_id)
{
   if (core_id)
   {
      const core_info_list_t* cores = apple_core_info_list_get();

      for (int i = 0; i != cores->count; i ++)
         if (cores->list[i].path && strcmp(core_id, cores->list[i].path) == 0)
            return &cores->list[i];
   }

   return 0;
}

const char* apple_core_info_get_id(const core_info_t* info, char* buffer, size_t buffer_length)
{
   if (!buffer || !buffer_length)
      return "";

   if (info && info->path && strlcpy(buffer, info->path, buffer_length) < buffer_length)
      return buffer;

   *buffer = 0;
   return buffer;
}

const char* apple_core_info_get_custom_config(const char* core_id, char* buffer, size_t buffer_length)
{
   if (!core_id || !buffer || !buffer_length)
      return 0;

   snprintf(buffer, buffer_length, "%s/%s", core_config_path, path_basename(core_id));
   fill_pathname(buffer, buffer, ".cfg", buffer_length);
   return buffer;
}

bool apple_core_info_has_custom_config(const char* core_id)
{
   if (!core_id)
      return false;
   
   char path[PATH_MAX];
   apple_core_info_get_custom_config(core_id, path, sizeof(path));
   return path_file_exists(path);
}

// ROM HISTORY EXTENSIONS
const char* apple_rom_history_get_path(rom_history_t* history, uint32_t index)
{
   const char *path, *core_path, *core_name;
   rom_history_get_index(history, index, &path, &core_path, &core_name);
   return path ? path : "";
}

const char* apple_rom_history_get_core_path(rom_history_t* history, uint32_t index)
{
   const char *path, *core_path, *core_name;
   rom_history_get_index(history, index, &path, &core_path, &core_name);
   return core_path ? core_path : "";
}

const char* apple_rom_history_get_core_name(rom_history_t* history, uint32_t index)
{
   const char *path, *core_path, *core_name;
   rom_history_get_index(history, index, &path, &core_path, &core_name);
   return core_name ? core_name : "";
}
