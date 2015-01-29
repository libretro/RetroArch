/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2013-2015 - Jason Fetters
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

#include "xmb_entry.h"
#include "general.h"
#include <file/file_path.h>
#include <file/dir_list.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

xmb_entry_list_t *xmb_entry_list_new(const char *xmb_entry_path)
{
   size_t i;
   xmb_entry_t *entry = NULL;
   xmb_entry_list_t *entry_list = NULL;
   struct string_list *contents = (struct string_list*)
      dir_list_new(xmb_entry_path, "xe", false);

   if (!contents)
      return NULL;

   entry_list = (xmb_entry_list_t*)calloc(1, sizeof(*entry_list));
   if (!entry_list)
      goto error;

   entry = (xmb_entry_t*)calloc(contents->size, sizeof(*entry));
   if (!entry)
      goto error;

   entry_list->list = entry;
   entry_list->count = contents->size;

   for (i = 0; i < contents->size; i++)
   {
      char xe_path_base[PATH_MAX_LENGTH], xe_path[PATH_MAX_LENGTH];
      entry[i].path = strdup(contents->elems[i].data);

      if (!entry[i].path)
         break;

      fill_pathname_base(xe_path_base, contents->elems[i].data,
            sizeof(xe_path_base));
      path_remove_extension(xe_path_base);

#if defined(RARCH_MOBILE) || defined(RARCH_CONSOLE)
      char *substr = strrchr(xe_path_base, '_');
      if (substr)
         *substr = '\0';
#endif

      strlcat(xe_path_base, ".xe", sizeof(xe_path_base));

      fill_pathname_join(xe_path, (*g_settings.xmb_entry_path) ?
            g_settings.xmb_entry_path : xmb_entry_path,
            xe_path_base, sizeof(xe_path));

      entry[i].data = config_file_new(xe_path);

      if (entry[i].data)
      {
         unsigned count = 0;
         config_get_string(entry[i].data, "display_name",
               &entry[i].display_name);
         config_get_string(entry[i].data, "core_name",
               &entry[i].core_name);
         config_get_string(entry[i].data, "icon_name",
               &entry[i].icon_name);
         config_get_string(entry[i].data, "content_icon_name",
               &entry[i].content_icon_name);
         config_get_string(entry[i].data, "content_subdirectory",
               &entry[i].content_subdirectory);
      }

      if (!entry[i].display_name)
         entry[i].display_name = strdup(path_basename(entry[i].path));
   }

   dir_list_free(contents);
   return entry_list;

error:
   if (contents)
      dir_list_free(contents);
   xmb_entry_list_free(entry_list);
   return NULL;
}

void xmb_entry_list_free(xmb_entry_list_t *xmb_entry_list)
{
   size_t i, j;

   if (!xmb_entry_list)
      return;

   for (i = 0; i < xmb_entry_list->count; i++)
   {
      xmb_entry_t *info = (xmb_entry_t*)&xmb_entry_list->list[i];

      if (!info)
         continue;

      free(info->path);
      free(info->display_name);
      free(info->core_name);
      free(info->icon_name);
      free(info->content_icon_name);
      free(info->content_subdirectory);
   }

   free(xmb_entry_list->list);
   free(xmb_entry_list);
}
