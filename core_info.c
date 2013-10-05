/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "core_info.h"
#include "general.h"
#include "file.h"
#include "file_ext.h"
#include "config.def.h"

core_info_list_t *get_core_info_list(const char *modules_path)
{
   struct string_list *contents = dir_list_new(modules_path, EXT_EXECUTABLES, false);

   core_info_t *core_info = NULL;
   core_info_list_t *core_info_list = NULL;

   if (!contents)
      return NULL;

   core_info_list = (core_info_list_t*)calloc(1, sizeof(*core_info_list));
   if (!core_info_list)
      goto error;

   core_info = (core_info_t*)calloc(contents->size, sizeof(*core_info));
   if (!core_info)
      goto error;

   core_info_list->list = core_info;
   core_info_list->count = contents->size;

   for (size_t i = 0; i < contents->size; i++)
   {
      char buffer[PATH_MAX];
      char info_path[PATH_MAX];
   
      core_info[i].path = strdup(contents->elems[i].data);

      // FIXME: Need to do something about this logic.
      // fill_pathname() *should* be sufficient.
      //
      // NOTE: This assumes all modules are named module_name_{tag}.ext
      //       {tag} must not contain an underscore. (This isn't true for PC versions)
      strlcpy(buffer, contents->elems[i].data, sizeof(buffer));
      char *substr = strrchr(buffer, '_');
      if (substr)
         *substr = '\0';

      // NOTE: Can't just use fill_pathname on iOS as it will cut at RetroArch.app;
      //       perhaps fill_pathname shouldn't cut before the last path element.
      if (substr)
         snprintf(info_path, PATH_MAX, "%s.info", buffer);
      else
         fill_pathname(info_path, buffer, ".info", sizeof(info_path));

      core_info[i].data = config_file_new(info_path);

      if (core_info[i].data)
      {
         config_get_string(core_info[i].data, "display_name", &core_info[i].display_name);
         if (config_get_string(core_info[i].data, "supported_extensions", &core_info[i].supported_extensions) &&
               core_info[i].supported_extensions)
            core_info[i].supported_extensions_list = string_split(core_info[i].supported_extensions, "|");
      }

      if (!core_info[i].display_name)
         core_info[i].display_name = strdup(path_basename(core_info[i].path));
   }

   dir_list_free(contents);
   return core_info_list;

error:
   if (contents)
      dir_list_free(contents);
   free_core_info_list(core_info_list);
   return NULL;
}

void free_core_info_list(core_info_list_t *core_info_list)
{
   if (!core_info_list)
      return;

   for (size_t i = 0; i < core_info_list->count; i++)
   {
      free(core_info_list->list[i].path);
      free(core_info_list->list[i].display_name);
      free(core_info_list->list[i].supported_extensions);
      string_list_free(core_info_list->list[i].supported_extensions_list);
      config_file_free(core_info_list->list[i].data);
   }

   free(core_info_list->list);
   free(core_info_list);
}

bool does_core_support_file(core_info_t *core, const char *path)
{
   if (!path || !core || !core->supported_extensions_list)
      return false;

   return string_list_find_elem_prefix(core->supported_extensions_list, ".", path_get_extension(path));
}

