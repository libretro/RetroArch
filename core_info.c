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
#include "file_extract.h"
#include "config.def.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

core_info_list_t *core_info_list_new(const char *modules_path)
{
   struct string_list *contents = dir_list_new(modules_path, EXT_EXECUTABLES, false);
   size_t all_ext_len, i;

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

   for (i = 0; i < contents->size; i++)
   {
      char info_path_base[PATH_MAX], info_path[PATH_MAX];
      core_info[i].path = strdup(contents->elems[i].data);

      if (!core_info[i].path)
         break;

      fill_pathname_base(info_path_base, contents->elems[i].data, sizeof(info_path_base));
      path_remove_extension(info_path_base);

#if defined(RARCH_MOBILE) || defined(RARCH_CONSOLE)
      char *substr = strrchr(info_path_base, '_');
      if (substr)
         *substr = '\0';
#endif

      strlcat(info_path_base, ".info", sizeof(info_path_base));

      fill_pathname_join(info_path, (*g_settings.libretro_info_path) ? g_settings.libretro_info_path : modules_path,
            info_path_base, sizeof(info_path));

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

   all_ext_len = 0;
   for (i = 0; i < core_info_list->count; i++)
   {
      all_ext_len += core_info_list->list[i].supported_extensions ?
         (strlen(core_info_list->list[i].supported_extensions) + 2) : 0;
   }

   if (all_ext_len)
   {
      all_ext_len += strlen("|zip");
      core_info_list->all_ext = (char*)calloc(1, all_ext_len);
   }

   if (core_info_list->all_ext)
   {
      for (i = 0; i < core_info_list->count; i++)
      {
         if (core_info_list->list[i].supported_extensions)
         {
            strlcat(core_info_list->all_ext, core_info_list->list[i].supported_extensions, all_ext_len);
            strlcat(core_info_list->all_ext, "|", all_ext_len);
         }
      }
      strlcat(core_info_list->all_ext, "|zip", all_ext_len);
   }

   dir_list_free(contents);
   return core_info_list;

error:
   if (contents)
      dir_list_free(contents);
   core_info_list_free(core_info_list);
   return NULL;
}

void core_info_list_free(core_info_list_t *core_info_list)
{
   size_t i;
   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      free(core_info_list->list[i].path);
      free(core_info_list->list[i].display_name);
      free(core_info_list->list[i].supported_extensions);
      string_list_free(core_info_list->list[i].supported_extensions_list);
      config_file_free(core_info_list->list[i].data);
   }

   free(core_info_list->all_ext);
   free(core_info_list->list);
   free(core_info_list);
}

size_t core_info_list_num_info_files(core_info_list_t *core_info_list)
{
   size_t i, num;
   num = 0;
   for (i = 0; i < core_info_list->count; i++)
      num += !!core_info_list->list[i].data;
   return num;
}

bool core_info_list_get_display_name(core_info_list_t *core_info_list, const char *path, char *buf, size_t size)
{
   size_t i;
   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (!strcmp(info->path, path) && info->display_name)
      {
         strlcpy(buf, info->display_name, size);
         return true;
      }
   }

   return false;
}

bool core_info_does_support_any_file(const core_info_t *core, const struct string_list *list)
{
   size_t i;
   if (!list || !core || !core->supported_extensions_list)
      return false;

   for (i = 0; i < list->size; i++)
      if (string_list_find_elem_prefix(core->supported_extensions_list, ".", path_get_extension(list->elems[i].data)))
         return true;
   return false;
}

bool core_info_does_support_file(const core_info_t *core, const char *path)
{
   if (!path || !core || !core->supported_extensions_list)
      return false;

   return string_list_find_elem_prefix(core->supported_extensions_list, ".", path_get_extension(path));
}

const char *core_info_list_get_all_extensions(core_info_list_t *core_info_list)
{
   return core_info_list->all_ext;
}

// qsort_r() is not in standard C, sadly.
static const char *core_info_tmp_path;
static const struct string_list *core_info_tmp_list;

static int core_info_qsort_cmp(const void *a_, const void *b_)
{
   const core_info_t *a = (const core_info_t*)a_;
   const core_info_t *b = (const core_info_t*)b_;

   int support_a = core_info_does_support_any_file(a, core_info_tmp_list) ||
      core_info_does_support_file(a, core_info_tmp_path);
   int support_b = core_info_does_support_any_file(b, core_info_tmp_list) ||
      core_info_does_support_file(b, core_info_tmp_path);

   if (support_a != support_b)
      return support_b - support_a;
   else
      return strcasecmp(a->display_name, b->display_name);
}

void core_info_list_get_supported_cores(core_info_list_t *core_info_list, const char *path,
      const core_info_t **infos, size_t *num_infos)
{
   core_info_tmp_path = path;

#ifdef HAVE_ZLIB
   struct string_list *list = NULL;
   if (!strcasecmp(path_get_extension(path), "zip"))
      list = zlib_get_file_list(path);
   core_info_tmp_list = list;
#endif

   // Let supported core come first in list so we can return a pointer to them.
   qsort(core_info_list->list, core_info_list->count, sizeof(core_info_t), core_info_qsort_cmp);

   size_t supported, i;
   supported = 0;
   for (i = 0; i < core_info_list->count; i++, supported++)
   {
      const core_info_t *core = &core_info_list->list[i];
      if (!core_info_does_support_file(core, path)
#ifdef HAVE_ZLIB
            && !core_info_does_support_any_file(core, list)
#endif
            )
         break;
   }

#ifdef HAVE_ZLIB
   if (list)
      string_list_free(list);
#endif

   *infos = core_info_list->list;
   *num_infos = supported;
}

