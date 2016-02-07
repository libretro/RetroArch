/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <file/file_path.h>
#include <file/dir_list.h>
#include <file/file_archive.h>
#include <compat/strl.h>

#include <string/stdstring.h>

#include "core_info.h"
#include "configuration.h"
#include "dir_list_special.h"
#include "config.def.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const char *core_info_tmp_path = NULL;
static const struct string_list *core_info_tmp_list = NULL;

static void core_info_list_resolve_all_extensions(
      core_info_list_t *core_info_list)
{
   size_t i, all_ext_len = 0;

   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      if (core_info_list->list[i].supported_extensions)
         all_ext_len += 
            (strlen(core_info_list->list[i].supported_extensions) + 2);
   }

   if (all_ext_len)
      core_info_list->all_ext = (char*)calloc(1, all_ext_len);

   if (!core_info_list->all_ext)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      if (!core_info_list->list[i].supported_extensions)
         continue;

      strlcat(core_info_list->all_ext,
            core_info_list->list[i].supported_extensions, all_ext_len);
      strlcat(core_info_list->all_ext, "|", all_ext_len);
   }
}

static void core_info_list_resolve_all_firmware(
      core_info_list_t *core_info_list)
{
   size_t i;
   unsigned c;

   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      unsigned count        = 0;
      core_info_t *info     = (core_info_t*)&core_info_list->list[i];
      config_file_t *config = (config_file_t*)info->config_data;

      if (!info || !config)
         continue;

      if (!config_get_uint(config, "firmware_count", &count))
         continue;

      info->firmware = (core_info_firmware_t*)
         calloc(count, sizeof(*info->firmware));

      if (!info->firmware)
         continue;

      for (c = 0; c < count; c++)
      {
         char path_key[64] = {0};
         char desc_key[64] = {0};
         char opt_key[64]  = {0};

         snprintf(path_key, sizeof(path_key), "firmware%u_path", c);
         snprintf(desc_key, sizeof(desc_key), "firmware%u_desc", c);
         snprintf(opt_key, sizeof(opt_key), "firmware%u_opt", c);

         config_get_string(config, path_key, &info->firmware[c].path);
         config_get_string(config, desc_key, &info->firmware[c].desc);
         config_get_bool(config, opt_key , &info->firmware[c].optional);
      }
   }
}

static void core_info_list_free(core_info_list_t *core_info_list)
{
   size_t i, j;

   if (!core_info_list)
      return;

   for (i = 0; i < core_info_list->count; i++)
   {
      core_info_t *info = (core_info_t*)&core_info_list->list[i];

      if (!info)
         continue;

      free(info->path);
      free(info->core_name);
      free(info->systemname);
      free(info->system_manufacturer);
      free(info->display_name);
      free(info->supported_extensions);
      free(info->authors);
      free(info->permissions);
      free(info->licenses);
      free(info->categories);
      free(info->databases);
      free(info->notes);
      if (info->supported_extensions_list)
         string_list_free(info->supported_extensions_list);
      string_list_free(info->authors_list);
      string_list_free(info->note_list);
      string_list_free(info->permissions_list);
      string_list_free(info->licenses_list);
      string_list_free(info->categories_list);
      string_list_free(info->databases_list);
      config_file_free((config_file_t*)info->config_data);

      for (j = 0; j < info->firmware_count; j++)
      {
         free(info->firmware[j].path);
         free(info->firmware[j].desc);
      }
      free(info->firmware);
   }

   free(core_info_list->all_ext);
   free(core_info_list->list);
   free(core_info_list);
}

void core_info_get_name(const char *path, char *s, size_t len)
{
   size_t i;
   core_info_t *core_info = NULL;
   core_info_list_t *core_info_list = NULL;
   settings_t *settings = config_get_ptr();
   struct string_list *contents = dir_list_new_special(
         NULL, DIR_LIST_CORES, NULL);

   if (!contents)
      return;

   core_info_list = (core_info_list_t*)calloc(1, sizeof(*core_info_list));
   if (!core_info_list)
      goto error;

   core_info = (core_info_t*)calloc(contents->size, sizeof(*core_info));
   if (!core_info)
      goto error;

   core_info_list->list  = core_info;
   core_info_list->count = 0;

   for (i = 0; i < contents->size; i++)
   {
      config_file_t *conf                  = NULL;
      char info_path_base[PATH_MAX_LENGTH] = {0};
      char info_path[PATH_MAX_LENGTH]      = {0};

      core_info[i].path = strdup(contents->elems[i].data);

      if (!core_info[i].path)
         break;

      if (!string_is_equal(core_info[i].path, path))
            continue;

      fill_pathname_base(info_path_base, contents->elems[i].data,
            sizeof(info_path_base));
      path_remove_extension(info_path_base);

#if defined(RARCH_MOBILE) || (defined(RARCH_CONSOLE) && !defined(PSP))
      char *substr = strrchr(info_path_base, '_');
      if (substr)
         *substr = '\0';
#endif

      strlcat(info_path_base, ".info", sizeof(info_path_base));

      fill_pathname_join(info_path, (*settings->libretro_info_path) ?
            settings->libretro_info_path : settings->libretro_directory,
            info_path_base, sizeof(info_path));

      conf = config_file_new(info_path);

      if (conf)
      {
         config_get_string(conf, "corename",
               &core_info[i].core_name);
         core_info[i].config_data = (void*)conf;
      }

      strlcpy(s, core_info[i].core_name, len);
   }

   for (i = 0; i < contents->size; i++)
   {
      config_file_t *conf = (config_file_t*)
         core_info_list->list[i].config_data;
      core_info_list->count += !!conf;
   }

error:
   if (contents)
      dir_list_free(contents);
   contents = NULL;
   core_info_list_free(core_info_list);
}

static core_info_list_t *core_info_list_new(void)
{
   size_t i;
   core_info_t *core_info = NULL;
   core_info_list_t *core_info_list = NULL;
   settings_t *settings = config_get_ptr();
   struct string_list *contents = 
      dir_list_new_special(NULL, DIR_LIST_CORES, NULL);

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
      config_file_t *conf                  = NULL;
      char info_path_base[PATH_MAX_LENGTH] = {0};
      char info_path[PATH_MAX_LENGTH]      = {0};
      core_info[i].path = strdup(contents->elems[i].data);

      if (!core_info[i].path)
         break;

      fill_pathname_base(info_path_base, contents->elems[i].data,
            sizeof(info_path_base));
      path_remove_extension(info_path_base);

#if defined(RARCH_MOBILE) || (defined(RARCH_CONSOLE) && !defined(PSP))
      char *substr = strrchr(info_path_base, '_');
      if (substr)
         *substr = '\0';
#endif

      strlcat(info_path_base, ".info", sizeof(info_path_base));

      fill_pathname_join(info_path, (*settings->libretro_info_path) ?
            settings->libretro_info_path : settings->libretro_directory,
            info_path_base, sizeof(info_path));

      conf = config_file_new(info_path);

      if (conf)
      {
         unsigned count = 0;
         config_get_string(conf, "display_name",
               &core_info[i].display_name);
         config_get_string(conf, "corename",
               &core_info[i].core_name);
         config_get_string(conf, "systemname",
               &core_info[i].systemname);
         config_get_string(conf, "manufacturer",
               &core_info[i].system_manufacturer);
         config_get_uint(conf, "firmware_count", &count);
         core_info[i].firmware_count = count;
         if (config_get_string(conf, "supported_extensions",
                  &core_info[i].supported_extensions) &&
               core_info[i].supported_extensions)
            core_info[i].supported_extensions_list =
               string_split(core_info[i].supported_extensions, "|");

         if (config_get_string(conf, "authors",
                  &core_info[i].authors) &&
               core_info[i].authors)
            core_info[i].authors_list =
               string_split(core_info[i].authors, "|");

         if (config_get_string(conf, "permissions",
                  &core_info[i].permissions) &&
               core_info[i].permissions)
            core_info[i].permissions_list =
               string_split(core_info[i].permissions, "|");

         if (config_get_string(conf, "license",
                  &core_info[i].licenses) &&
               core_info[i].licenses)
            core_info[i].licenses_list =
               string_split(core_info[i].licenses, "|");

         if (config_get_string(conf, "categories",
                  &core_info[i].categories) &&
               core_info[i].categories)
            core_info[i].categories_list =
               string_split(core_info[i].categories, "|");

         if (config_get_string(conf, "database",
                  &core_info[i].databases) &&
               core_info[i].databases)
            core_info[i].databases_list =
               string_split(core_info[i].databases, "|");

         if (config_get_string(conf, "notes",
                  &core_info[i].notes) &&
               core_info[i].notes)
            core_info[i].note_list = string_split(core_info[i].notes, "|");

         config_get_bool(conf, "supports_no_game",
               &core_info[i].supports_no_game);

         core_info[i].config_data = conf;
      }

      if (!core_info[i].display_name)
         core_info[i].display_name = 
            strdup(path_basename(core_info[i].path));
   }

   core_info_list_resolve_all_extensions(core_info_list);
   core_info_list_resolve_all_firmware(core_info_list);

   dir_list_free(contents);
   return core_info_list;

error:
   if (contents)
      dir_list_free(contents);
   core_info_list_free(core_info_list);
   return NULL;
}

bool core_info_list_get_display_name(core_info_list_t *core_info_list,
      const char *path, char *s, size_t len)
{
   size_t i;

   if (!core_info_list)
      return false;

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];
      if (string_is_equal(path_basename(info->path), path_basename(path))
            && info->display_name)
      {
         strlcpy(s, info->display_name, len);
         return true;
      }
   }

   return false;
}

bool core_info_get_display_name(const char *path, char *s, size_t len)
{
   char       *core_name = NULL;
   char       *display_name = NULL;
   config_file_t *conf   = NULL;

   if (!path_file_exists(path))
      return false;

   conf = config_file_new(path);

   if (!conf)
      goto error;

   config_get_string(conf, "corename",
         &core_name);

   config_get_string(conf, "display_name",
         &display_name);

   config_file_free(conf);

   if (!core_name)
      goto error;
   if (!conf)
      goto error;

   snprintf(s, len,"%s",display_name);

   free(core_name);
   free(display_name);

   return true;

error:
   if (core_name)
      free(core_name);
   return false;
}

bool core_info_list_get_info(core_info_list_t *core_info_list,
      core_info_t *out_info, const char *path)
{
   size_t i;
   if (!core_info_list || !out_info)
      return false;

   memset(out_info, 0, sizeof(*out_info));

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];

      if (string_is_equal(path_basename(info->path), 
               path_basename(path)))
      {
         *out_info = *info;
         return true;
      }
   }

   return false;
}

static bool core_info_does_support_any_file(const core_info_t *core,
      const struct string_list *list)
{
   size_t i;
   if (!list || !core || !core->supported_extensions_list)
      return false;

   for (i = 0; i < list->size; i++)
      if (string_list_find_elem_prefix(core->supported_extensions_list,
               ".", path_get_extension(list->elems[i].data)))
         return true;
   return false;
}

static bool core_info_does_support_file(
      const core_info_t *core, const char *path)
{
   if (!path || !core || !core->supported_extensions_list)
      return false;
   return string_list_find_elem_prefix(
         core->supported_extensions_list, ".", path_get_extension(path));
}

const char *core_info_list_get_all_extensions(void)
{
   core_info_list_t *list = NULL;
   core_info_ctl(CORE_INFO_CTL_LIST_GET, &list);
   if (!list)
      return NULL;
   return list->all_ext;
}

/* qsort_r() is not in standard C, sadly. */

static int core_info_qsort_cmp(const void *a_, const void *b_)
{
   const core_info_t *a = (const core_info_t*)a_;
   const core_info_t *b = (const core_info_t*)b_;
   int support_a        = 
         core_info_does_support_any_file(a, core_info_tmp_list)
      || core_info_does_support_file(a, core_info_tmp_path);
   int support_b        = 
         core_info_does_support_any_file(b, core_info_tmp_list)
      || core_info_does_support_file(b, core_info_tmp_path);

   if (support_a != support_b)
      return support_b - support_a;
   return strcasecmp(a->display_name, b->display_name);
}

void core_info_list_get_supported_cores(core_info_list_t *core_info_list,
      const char *path, const core_info_t **infos, size_t *num_infos)
{
   struct string_list *list = NULL;
   size_t supported = 0, i;

   if (!core_info_list)
      return;

   (void)list;

   core_info_tmp_path = path;

#ifdef HAVE_ZLIB
   if (string_is_equal_noncase(path_get_extension(path), "zip"))
      list = file_archive_get_file_list(path, NULL);
   core_info_tmp_list = list;
#endif

   /* Let supported core come first in list so we can return 
    * a pointer to them. */
   qsort(core_info_list->list, core_info_list->count,
         sizeof(core_info_t), core_info_qsort_cmp);

   for (i = 0; i < core_info_list->count; i++, supported++)
   {
      const core_info_t *core = &core_info_list->list[i];

      if (!core)
         continue;

      if (core_info_does_support_file(core, path))
         continue;

#ifdef HAVE_ZLIB
      if (core_info_does_support_any_file(core, list))
         continue;
#endif

      break;
   }

#ifdef HAVE_ZLIB
   if (list)
      string_list_free(list);
#endif

   *infos = core_info_list->list;
   *num_infos = supported;
}

static core_info_t *core_info_find(core_info_list_t *list,
      const char *core)
{
   size_t i;

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = (core_info_t*)&list->list[i];

      if (!info || !info->path)
         continue;
      if (string_is_equal(info->path, core))
         return info;
   }

   return NULL;
}

core_info_t *core_info_get(core_info_list_t *list, size_t i)
{
   core_info_t *info = NULL;

   if (!list)
      return NULL;
   info = (core_info_t*)&list->list[i];
   if (!info || !info->path)
      return NULL;

   return info;
}


void core_info_list_update_missing_firmware(
      core_info_list_t *core_info_list,
      const char *core, const char *systemdir)
{
   size_t i;
   char path[PATH_MAX_LENGTH] = {0};
   core_info_t          *info = NULL;

   if (!core_info_list || !core)
      return;

   if (!(info = core_info_find(core_info_list, core)))
      return;

   for (i = 0; i < info->firmware_count; i++)
   {
      if (!info->firmware[i].path)
         continue;

      fill_pathname_join(path, systemdir,
            info->firmware[i].path, sizeof(path));
      info->firmware[i].missing = !path_file_exists(path);
   }
}

#if 0
static int core_info_firmware_cmp(const void *a_, const void *b_)
{
   const core_info_firmware_t *a = (const core_info_firmware_t*)a_;
   const core_info_firmware_t *b = (const core_info_firmware_t*)b_;
   int                     order = b->missing - a->missing;

   if (order)
      return order;
   return strcasecmp(a->path, b->path);
}

/* Non-reentrant, does not allocate. Returns pointer to internal state. */

static void core_info_list_get_missing_firmware(
      core_info_list_t *core_info_list,
      const char *core, const char *systemdir,
      const core_info_firmware_t **firmware, size_t *num_firmware)
{
   size_t i;
   char path[PATH_MAX_LENGTH] = {0};
   core_info_t          *info = NULL;

   if (!core_info_list || !core)
      return;

   *firmware     = NULL;
   *num_firmware = 0;

   if (!(info = core_info_find(core_info_list, core)))
      return;

   *firmware = info->firmware;

   for (i = 1; i < info->firmware_count; i++)
   {
      fill_pathname_join(path, systemdir,
            info->firmware[i].path, sizeof(path));
      info->firmware[i].missing = !path_file_exists(path);
      *num_firmware += info->firmware[i].missing;
   }

   qsort(info->firmware, info->firmware_count, sizeof(*info->firmware),
         core_info_firmware_cmp);
}
#endif

bool core_info_ctl(enum core_info_state state, void *data)
{
   static core_info_t *core_info_current            = NULL;
   static core_info_list_t *core_info_curr_list     = NULL;

   switch (state)
   {
      case CORE_INFO_CTL_CURRENT_CORE_FREE:
         if (core_info_current)
            free(core_info_current);
         core_info_current = NULL;
         break;
      case CORE_INFO_CTL_CURRENT_CORE_INIT:
         core_info_current = (core_info_t*)calloc(1, sizeof(core_info_t));
         if (!core_info_current)
            return false;
         break;
      case CORE_INFO_CTL_CURRENT_CORE_GET:
         {
            core_info_t **core = (core_info_t**)data;
            if (!core)
               return false;
            *core = core_info_current;
         }
         break;
      case CORE_INFO_CTL_LIST_DEINIT:
         if (core_info_curr_list)
            core_info_list_free(core_info_curr_list);
         core_info_curr_list = NULL;
         break;
      case CORE_INFO_CTL_LIST_INIT:
         core_info_curr_list = core_info_list_new();
         break;
      case CORE_INFO_CTL_LIST_GET:
         {
            core_info_list_t **core = (core_info_list_t**)data;
            if (!core)
               return false;
            *core = core_info_curr_list;
         }
         break;
      case CORE_INFO_CTL_FIND:
         {
            core_info_ctx_find_t *info = (core_info_ctx_find_t*)data;
            if (!info || !core_info_curr_list)
               return false;
            if (!(info->inf = core_info_find(core_info_curr_list, info->path)))
               return false;
         }
         break;
      case CORE_INFO_CTL_NONE:
      default:
         break;
   }

   return true;
}
