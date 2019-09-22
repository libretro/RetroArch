/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <file/archive_file.h>
#include <streams/file_stream.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "retroarch.h"
#include "verbosity.h"

#include "core_info.h"
#include "file_path_special.h"

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#include "uwp/uwp_func.h"
#endif

#ifdef HAVE_COMPRESSION
static const struct string_list *core_info_tmp_list = NULL;
#endif
static const char *core_info_tmp_path               = NULL;
static core_info_t *core_info_current               = NULL;
static core_info_list_t *core_info_curr_list        = NULL;

enum compare_op
{
   COMPARE_OP_EQUAL,
   COMPARE_OP_NOT_EQUAL,
   COMPARE_OP_LESS,
   COMPARE_OP_LESS_EQUAL,
   COMPARE_OP_GREATER,
   COMPARE_OP_GREATER_EQUAL
};

static void core_info_list_resolve_all_extensions(
      core_info_list_t *core_info_list)
{
   size_t i              = 0;
   size_t all_ext_len    = 0;
   char *all_ext         = NULL;

   for (i = 0; i < core_info_list->count; i++)
   {
      if (core_info_list->list[i].supported_extensions)
         all_ext_len +=
            (strlen(core_info_list->list[i].supported_extensions) + 2);
   }

   all_ext_len += STRLEN_CONST("7z|") + STRLEN_CONST("zip|");

   all_ext      = (char*)calloc(1, all_ext_len);

   if (!all_ext)
      return;

   core_info_list->all_ext = all_ext;

   for (i = 0; i < core_info_list->count; i++)
   {
      if (!core_info_list->list[i].supported_extensions)
         continue;

      strlcat(core_info_list->all_ext,
            core_info_list->list[i].supported_extensions, all_ext_len);
      strlcat(core_info_list->all_ext, "|", all_ext_len);
   }
#ifdef HAVE_7ZIP
   strlcat(core_info_list->all_ext, "7z|", all_ext_len);
#endif
#ifdef HAVE_ZLIB
   strlcat(core_info_list->all_ext, "zip|", all_ext_len);
#endif
}

static void core_info_list_resolve_all_firmware(
      core_info_list_t *core_info_list)
{
   size_t i;
   unsigned c;

   for (i = 0; i < core_info_list->count; i++)
   {
      unsigned count                  = 0;
      core_info_firmware_t *firmware  = NULL;
      core_info_t *info               = (core_info_t*)&core_info_list->list[i];
      config_file_t *config           = (config_file_t*)info->config_data;

      if (!config || !config_get_uint(config, "firmware_count", &count))
         continue;

      firmware = (core_info_firmware_t*)calloc(count, sizeof(*firmware));

      if (!firmware)
         continue;

      info->firmware = firmware;

      for (c = 0; c < count; c++)
      {
         char path_key[64];
         char desc_key[64];
         char opt_key[64];
         bool tmp_bool     = false;
         char *tmp         = NULL;
         path_key[0]       = desc_key[0] = opt_key[0] = '\0';

         snprintf(path_key, sizeof(path_key), "firmware%u_path", c);
         snprintf(desc_key, sizeof(desc_key), "firmware%u_desc", c);
         snprintf(opt_key,  sizeof(opt_key),  "firmware%u_opt",  c);

         if (config_get_string(config, path_key, &tmp) && !string_is_empty(tmp))
         {
            info->firmware[c].path = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }
         if (config_get_string(config, desc_key, &tmp) && !string_is_empty(tmp))
         {
            info->firmware[c].desc = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }
         if (tmp)
            free(tmp);
         tmp = NULL;
         if (config_get_bool(config, opt_key , &tmp_bool))
            info->firmware[c].optional = tmp_bool;
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

      free(info->path);
      free(info->core_name);
      free(info->systemname);
      free(info->system_id);
      free(info->system_manufacturer);
      free(info->display_name);
      free(info->display_version);
      free(info->supported_extensions);
      free(info->authors);
      free(info->permissions);
      free(info->licenses);
      free(info->categories);
      free(info->databases);
      free(info->notes);
      free(info->required_hw_api);
      string_list_free(info->supported_extensions_list);
      string_list_free(info->authors_list);
      string_list_free(info->note_list);
      string_list_free(info->permissions_list);
      string_list_free(info->licenses_list);
      string_list_free(info->categories_list);
      string_list_free(info->databases_list);
      string_list_free(info->required_hw_api_list);
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

static config_file_t *core_info_list_iterate(
      const char *current_path,
      const char *path_basedir)
{
   size_t info_path_base_size = PATH_MAX_LENGTH * sizeof(char);
   char *info_path_base       = NULL;
   char *info_path            = NULL;
   config_file_t *conf        = NULL;

   if (!current_path)
      return NULL;

   info_path_base             = (char*)malloc(info_path_base_size);

   info_path_base[0] = '\0';

   fill_pathname_base_noext(info_path_base,
         current_path,
         info_path_base_size);

#if defined(RARCH_MOBILE) || (defined(RARCH_CONSOLE) && !defined(PSP) && !defined(_3DS) && !defined(VITA) && !defined(PS2) && !defined(HW_WUP))
   {
      char *substr = strrchr(info_path_base, '_');
      if (substr)
         *substr = '\0';
   }
#endif

   strlcat(info_path_base, ".info", info_path_base_size);

   info_path = (char*)malloc(info_path_base_size);
   fill_pathname_join(info_path,
         path_basedir,
         info_path_base, info_path_base_size);
   free(info_path_base);
   info_path_base = NULL;

   if (path_is_valid(info_path))
      conf = config_file_new_from_path_to_string(info_path);
   free(info_path);

   return conf;
}

static core_info_list_t *core_info_list_new(const char *path,
      const char *libretro_info_dir,
      const char *exts,
      bool dir_show_hidden_files)
{
   size_t i;
   core_info_t *core_info           = NULL;
   core_info_list_t *core_info_list = NULL;
   const char       *path_basedir   = libretro_info_dir;
   struct string_list *contents     = string_list_new();
   bool                          ok = dir_list_append(contents, path, exts,
         false, dir_show_hidden_files, false, false);

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   /* UWP: browse the optional packages for additional cores */
   struct string_list *core_packages = string_list_new();
   uwp_fill_installed_core_packages(core_packages);
   for (i = 0; i < core_packages->size; i++)
      dir_list_append(contents, core_packages->elems[i].data, exts,
            false, dir_show_hidden_files, false, false);
   string_list_free(core_packages);
#else
   /* Keep the old 'directory not found' behavior */
   if (!ok)
   {
      string_list_free(contents);
      return NULL;
   }
#endif

   if (!contents)
      return NULL;

   core_info_list = (core_info_list_t*)calloc(1, sizeof(*core_info_list));
   if (!core_info_list)
   {
      string_list_free(contents);
      return NULL;
   }

   core_info = (core_info_t*)calloc(contents->size, sizeof(*core_info));
   if (!core_info)
   {
      core_info_list_free(core_info_list);
      string_list_free(contents);
      return NULL;
   }

   core_info_list->list  = core_info;
   core_info_list->count = contents->size;

   for (i = 0; i < contents->size; i++)
   {
      const char *base_path = contents->elems[i].data;
      config_file_t *conf   = core_info_list_iterate(base_path,
            path_basedir);

      if (conf)
      {
         char *tmp           = NULL;

         if (config_get_string(conf, "display_name", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].display_name = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }
         if (config_get_string(conf, "display_version", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].display_version = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }
         if (config_get_string(conf, "corename", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].core_name = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "systemname", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].systemname = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "systemid", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].system_id = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "manufacturer", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].system_manufacturer = strdup(tmp);
            free(tmp);
            tmp = NULL;
         }

         {
            unsigned count      = 0;
            config_get_uint(conf, "firmware_count", &count);
            core_info[i].firmware_count = count;
         }

         if (config_get_string(conf, "supported_extensions", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].supported_extensions      = strdup(tmp);
            core_info[i].supported_extensions_list =
               string_split(core_info[i].supported_extensions, "|");

            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "authors", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].authors      = strdup(tmp);
            core_info[i].authors_list =
               string_split(core_info[i].authors, "|");

            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "permissions", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].permissions      = strdup(tmp);
            core_info[i].permissions_list =
               string_split(core_info[i].permissions, "|");

            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "license", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].licenses      = strdup(tmp);
            core_info[i].licenses_list =
               string_split(core_info[i].licenses, "|");

            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "categories", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].categories      = strdup(tmp);
            core_info[i].categories_list =
               string_split(core_info[i].categories, "|");

            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "database", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].databases      = strdup(tmp);
            core_info[i].databases_list =
               string_split(core_info[i].databases, "|");

            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "notes", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].notes     = strdup(tmp);
            core_info[i].note_list = string_split(core_info[i].notes, "|");

            free(tmp);
            tmp = NULL;
         }

         if (config_get_string(conf, "required_hw_api", &tmp)
               && !string_is_empty(tmp))
         {
            core_info[i].required_hw_api = strdup(tmp);
            core_info[i].required_hw_api_list = string_split(core_info[i].required_hw_api, "|");

            free(tmp);
            tmp = NULL;
         }

         if (tmp)
            free(tmp);
         tmp    = NULL;

         {
            bool tmp_bool       = false;
            if (config_get_bool(conf, "supports_no_game",
                     &tmp_bool))
               core_info[i].supports_no_game = tmp_bool;

            if (config_get_bool(conf, "database_match_archive_member",
                     &tmp_bool))
               core_info[i].database_match_archive_member = tmp_bool;
         }

         core_info[i].config_data = conf;
      }

      if (!string_is_empty(base_path))
         core_info[i].path = strdup(base_path);

      if (!core_info[i].display_name)
         core_info[i].display_name =
            strdup(path_basename(core_info[i].path));
   }

   if (core_info_list)
   {
      core_info_list_resolve_all_extensions(core_info_list);
      core_info_list_resolve_all_firmware(core_info_list);
   }

   string_list_free(contents);
   return core_info_list;
}

/* Shallow-copies internal state.
 *
 * Data in *info is invalidated when the
 * core_info_list is freed. */
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

#ifdef HAVE_COMPRESSION
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
#endif

static bool core_info_does_support_file(
      const core_info_t *core, const char *path)
{
   if (!core || !core->supported_extensions_list)
      return false;
   if (string_is_empty(path))
      return false;

   return string_list_find_elem_prefix(
         core->supported_extensions_list, ".", path_get_extension(path));
}

/* qsort_r() is not in standard C, sadly. */

static int core_info_qsort_cmp(const void *a_, const void *b_)
{
   const core_info_t *a = (const core_info_t*)a_;
   const core_info_t *b = (const core_info_t*)b_;
   int support_a        = core_info_does_support_file(a, core_info_tmp_path);
   int support_b        = core_info_does_support_file(b, core_info_tmp_path);
#ifdef HAVE_COMPRESSION
   support_a            = support_a ||
      core_info_does_support_any_file(a, core_info_tmp_list);
   support_b            = support_b ||
      core_info_does_support_any_file(b, core_info_tmp_list);
#endif

   if (support_a != support_b)
      return support_b - support_a;
   return strcasecmp(a->display_name, b->display_name);
}

static core_info_t *core_info_find_internal(
      core_info_list_t *list,
      const char *core)
{
   size_t i;
   const char *core_path_basename = path_basename(core);

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = core_info_get(list, i);

      if (!info || !info->path)
         continue;
      if (string_is_equal(path_basename(info->path), core_path_basename))
         return info;
   }

   return NULL;
}

static bool core_info_list_update_missing_firmware_internal(
      core_info_list_t *core_info_list,
      const char *core,
      const char *systemdir,
      bool *set_missing_bios)
{
   size_t i;
   core_info_t      *info = NULL;
   char             *path = NULL;
   size_t       path_size = PATH_MAX_LENGTH * sizeof(char);

   if (!core_info_list || !core)
      return false;

   info                   = core_info_find_internal(core_info_list, core);

   if (!info)
      return false;

   path                   = (char*)malloc(path_size);

   if (!path)
      return false;

   path[0]                = '\0';

   for (i = 0; i < info->firmware_count; i++)
   {
      if (string_is_empty(info->firmware[i].path))
         continue;

      fill_pathname_join(path, systemdir,
            info->firmware[i].path, path_size);
      info->firmware[i].missing = !path_is_valid(path);
      if (info->firmware[i].missing && !info->firmware[i].optional)
      {
         *set_missing_bios = true;
         RARCH_WARN("Firmware missing: %s\n", info->firmware[i].path);
      }
   }

   free(path);
   return true;
}

void core_info_free_current_core(void)
{
   if (core_info_current)
      free(core_info_current);
   core_info_current = NULL;
}

bool core_info_init_current_core(void)
{
   core_info_current = (core_info_t*)calloc(1, sizeof(core_info_t));
   if (!core_info_current)
      return false;
   return true;
}

bool core_info_get_current_core(core_info_t **core)
{
   if (!core)
      return false;
   *core = core_info_current;
   return true;
}

void core_info_deinit_list(void)
{
   if (core_info_curr_list)
      core_info_list_free(core_info_curr_list);
   core_info_curr_list = NULL;
}

bool core_info_init_list(const char *path_info, const char *dir_cores,
      const char *exts, bool dir_show_hidden_files)
{
   if (!(core_info_curr_list = core_info_list_new(dir_cores,
               !string_is_empty(path_info) ? path_info : dir_cores,
               exts,
               dir_show_hidden_files)))
      return false;
   return true;
}

bool core_info_get_list(core_info_list_t **core)
{
   if (!core)
      return false;
   *core = core_info_curr_list;
   return true;
}

bool core_info_list_update_missing_firmware(core_info_ctx_firmware_t *info,
      bool *set_missing_bios)
{
   if (!info)
      return false;
   return core_info_list_update_missing_firmware_internal(
         core_info_curr_list,
         info->path, info->directory.system,
         set_missing_bios);
}

bool core_info_load(core_info_ctx_find_t *info)
{
   core_info_t *core_info     = NULL;

   if (!info)
      return false;

   if (!core_info_current)
      core_info_init_current_core();

   core_info_get_current_core(&core_info);

   if (!core_info_curr_list)
      return false;

   if (!core_info_list_get_info(core_info_curr_list,
            core_info, info->path))
      return false;

   return true;
}

bool core_info_find(core_info_ctx_find_t *info, const char *core_path)
{
   if (!info || !core_info_curr_list)
      return false;
   info->inf = core_info_find_internal(core_info_curr_list, core_path);
   if (!info->inf)
      return false;
   return true;
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

void core_info_list_get_supported_cores(core_info_list_t *core_info_list,
      const char *path, const core_info_t **infos, size_t *num_infos)
{
   size_t i;
   size_t supported         = 0;
#ifdef HAVE_COMPRESSION
   struct string_list *list = NULL;
#endif

   if (!core_info_list)
      return;

   core_info_tmp_path = path;

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(path))
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

      if (core_info_does_support_file(core, path))
         continue;

#ifdef HAVE_COMPRESSION
      if (core_info_does_support_any_file(core, list))
         continue;
#endif

      break;
   }

#ifdef HAVE_COMPRESSION
   if (list)
      string_list_free(list);
#endif

   *infos     = core_info_list->list;
   *num_infos = supported;
}

void core_info_get_name(const char *path, char *s, size_t len,
      const char *path_info, const char *dir_cores,
      const char *exts, bool dir_show_hidden_files,
      bool get_display_name)
{
   size_t i;
   const char       *path_basedir   = !string_is_empty(path_info) ?
      path_info : dir_cores;
   struct string_list *contents     = dir_list_new(
         dir_cores, exts, false, dir_show_hidden_files, false, false);
   const char *core_path_basename   = path_basename(path);

   if (!contents)
      return;

   for (i = 0; i < contents->size; i++)
   {
      config_file_t *conf             = NULL;
      char *new_core_name             = NULL;
      const char *current_path        = contents->elems[i].data;

      if (!string_is_equal(path_basename(current_path), core_path_basename))
         continue;

      conf = core_info_list_iterate(contents->elems[i].data,
               path_basedir);

      if (!conf)
         continue;

      if (config_get_string(conf, get_display_name 
               ? "display_name" : "corename",
            &new_core_name))
      {
         strlcpy(s, new_core_name, len);
         free(new_core_name);
      }

      config_file_free(conf);
      break;
   }

   dir_list_free(contents);
   contents = NULL;
}

size_t core_info_list_num_info_files(core_info_list_t *core_info_list)
{
   size_t i, num = 0;

   if (!core_info_list)
      return 0;

   for (i = 0; i < core_info_list->count; i++)
   {
      config_file_t *conf = (config_file_t*)
         core_info_list->list[i].config_data;
      num += !!conf;
   }

   return num;
}

bool core_info_database_match_archive_member(const char *database_path)
{
   char *database           = NULL;
   const char *new_path     = path_basename(database_path);

   if (string_is_empty(new_path))
      return false;

   database                 = strdup(new_path);

   if (string_is_empty(database))
      goto error;

   path_remove_extension(database);

   if (core_info_curr_list)
   {
      size_t i;

      for (i = 0; i < core_info_curr_list->count; i++)
      {
         const core_info_t *info = &core_info_curr_list->list[i];

         if (!info->database_match_archive_member)
             continue;

         if (!string_list_find_elem(info->databases_list, database))
             continue;

         free(database);
         return true;
      }
   }

error:
   if (database)
      free(database);
   return false;
}

bool core_info_database_supports_content_path(
      const char *database_path, const char *path)
{
   char *database           = NULL;
   const char *new_path     = path_basename(database_path);

   if (string_is_empty(new_path))
      return false;

   database                 = strdup(new_path);

   if (string_is_empty(database))
      goto error;

   path_remove_extension(database);

   if (core_info_curr_list)
   {
      size_t i;

      for (i = 0; i < core_info_curr_list->count; i++)
      {
         const core_info_t *info = &core_info_curr_list->list[i];

         if (!string_list_find_elem(info->supported_extensions_list,
                  path_get_extension(path)))
            continue;

         if (!string_list_find_elem(info->databases_list, database))
            continue;

         free(database);
         return true;
      }
   }

error:
   if (database)
      free(database);
   return false;
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

      if (!string_is_equal(path_basename(info->path), path_basename(path)))
         continue;

      if (!info->display_name)
         continue;

      strlcpy(s, info->display_name, len);
      return true;
   }

   return false;
}

bool core_info_get_display_name(const char *path, char *s, size_t len)
{
   char       *tmp     = NULL;
   config_file_t *conf = config_file_new_from_path_to_string(path);

   if (!conf)
      return false;

   if (config_get_string(conf, "display_name", &tmp))
   {
      strlcpy(s, tmp, len);
      free(tmp);
   }

   config_file_free(conf);
   return true;
}

static int core_info_qsort_func_path(const core_info_t *a,
      const core_info_t *b)
{
   if (!a || !b)
      return 0;

   if (string_is_empty(a->path) || string_is_empty(b->path))
      return 0;

   return strcasecmp(a->path, b->path);
}

static int core_info_qsort_func_display_name(const core_info_t *a,
      const core_info_t *b)
{
   if (!a || !b)
      return 0;

   if (string_is_empty(a->display_name) || string_is_empty(b->display_name))
      return 0;

   return strcasecmp(a->display_name, b->display_name);
}

static int core_info_qsort_func_core_name(const core_info_t *a,
      const core_info_t *b)
{
   if (!a || !b)
      return 0;

   if (string_is_empty(a->core_name) || string_is_empty(b->core_name))
      return 0;

   return strcasecmp(a->core_name, b->core_name);
}

static int core_info_qsort_func_system_name(const core_info_t *a,
      const core_info_t *b)
{
   if (!a || !b)
      return 0;

   if (string_is_empty(a->systemname) || string_is_empty(b->systemname))
      return 0;

   return strcasecmp(a->systemname, b->systemname);
}

void core_info_qsort(core_info_list_t *core_info_list,
      enum core_info_list_qsort_type qsort_type)
{
   if (!core_info_list)
      return;

   if (core_info_list->count < 2)
      return;

   switch (qsort_type)
   {
      case CORE_INFO_LIST_SORT_PATH:
         qsort(core_info_list->list,
               core_info_list->count,
               sizeof(core_info_t),
               (int (*)(const void *, const void *))
               core_info_qsort_func_path);
         break;
      case CORE_INFO_LIST_SORT_DISPLAY_NAME:
         qsort(core_info_list->list,
               core_info_list->count,
               sizeof(core_info_t),
               (int (*)(const void *, const void *))
               core_info_qsort_func_display_name);
         break;
      case CORE_INFO_LIST_SORT_CORE_NAME:
         qsort(core_info_list->list,
               core_info_list->count,
               sizeof(core_info_t),
               (int (*)(const void *, const void *))
               core_info_qsort_func_core_name);
         break;
      case CORE_INFO_LIST_SORT_SYSTEM_NAME:
         qsort(core_info_list->list,
               core_info_list->count,
               sizeof(core_info_t),
               (int (*)(const void *, const void *))
               core_info_qsort_func_system_name);
         break;
      default:
         return;
   }
}

static bool core_info_compare_api_version(int sys_major, int sys_minor, int major, int minor, enum compare_op op)
{
   switch (op)
   {
      case COMPARE_OP_EQUAL:
         if (sys_major == major && sys_minor == minor)
            return true;
         break;
      case COMPARE_OP_NOT_EQUAL:
         if (!(sys_major == major && sys_minor == minor))
            return true;
         break;
      case COMPARE_OP_LESS:
         if (sys_major < major || (sys_major == major && sys_minor < minor))
            return true;
         break;
      case COMPARE_OP_LESS_EQUAL:
         if (sys_major < major || (sys_major == major && sys_minor <= minor))
            return true;
         break;
      case COMPARE_OP_GREATER:
         if (sys_major > major || (sys_major == major && sys_minor > minor))
            return true;
         break;
      case COMPARE_OP_GREATER_EQUAL:
         if (sys_major > major || (sys_major == major && sys_minor >= minor))
            return true;
         break;
      default:
         break;
   }

   return false;
}

bool core_info_hw_api_supported(core_info_t *info)
{
#ifdef RARCH_INTERNAL
   unsigned i;
   enum gfx_ctx_api sys_api;
   gfx_ctx_flags_t sys_flags       = {0};
   const char *sys_api_version_str = video_driver_get_gpu_api_version_string();
   int sys_api_version_major       = 0;
   int sys_api_version_minor       = 0;

   enum api_parse_state
   {
      STATE_API_NAME,
      STATE_API_COMPARE_OP,
      STATE_API_VERSION
   };

   if (!info || !info->required_hw_api_list || info->required_hw_api_list->size == 0)
      return true;

   sys_api = video_context_driver_get_api();
   video_context_driver_get_flags(&sys_flags);

   for (i = 0; i < info->required_hw_api_list->size; i++)
   {
      char api_str[32]           = {0};
      char version[16]           = {0};
      char major_str[16]         = {0};
      char minor_str[16]         = {0};
      const char *cur_api        = info->required_hw_api_list->elems[i].data;
      int api_pos                = 0;
      int major_str_pos          = 0;
      int minor_str_pos          = 0;
      int cur_api_len            = 0;
      int j                      = 0;
      int major                  = 0;
      int minor                  = 0;
      bool found_major           = false;
      bool found_minor           = false;
      enum compare_op op         = COMPARE_OP_GREATER_EQUAL;
      enum api_parse_state state = STATE_API_NAME;

      if (string_is_empty(cur_api))
         continue;

      cur_api_len                = (int)strlen(cur_api);

      for (j = 0; j < cur_api_len; j++)
      {
         if (cur_api[j] == ' ')
            continue;

         switch (state)
         {
            case STATE_API_NAME:
            {
               if (isupper(cur_api[j]) || islower(cur_api[j]))
                  api_str[api_pos++] = cur_api[j];
               else
               {
                  j--;
                  state = STATE_API_COMPARE_OP;
                  break;
               }

               break;
            }
            case STATE_API_COMPARE_OP:
            {
               if (j < cur_api_len - 1 && !(cur_api[j] >= '0' && cur_api[j] <= '9'))
               {
                  if (cur_api[j] == '=' && cur_api[j + 1] == '=')
                  {
                     op = COMPARE_OP_EQUAL;
                     j++;
                  }
                  else if (cur_api[j] == '=')
                     op = COMPARE_OP_EQUAL;
                  else if (cur_api[j] == '!' && cur_api[j + 1] == '=')
                  {
                     op = COMPARE_OP_NOT_EQUAL;
                     j++;
                  }
                  else if (cur_api[j] == '<' && cur_api[j + 1] == '=')
                  {
                     op = COMPARE_OP_LESS_EQUAL;
                     j++;
                  }
                  else if (cur_api[j] == '>' && cur_api[j + 1] == '=')
                  {
                     op = COMPARE_OP_GREATER_EQUAL;
                     j++;
                  }
                  else if (cur_api[j] == '<')
                     op = COMPARE_OP_LESS;
                  else if (cur_api[j] == '>')
                     op = COMPARE_OP_GREATER;
               }

               state = STATE_API_VERSION;

               break;
            }
            case STATE_API_VERSION:
            {
               if (!found_minor && cur_api[j] >= '0' && cur_api[j] <= '9' && cur_api[j] != '.')
               {
                  found_major = true;

                  if (major_str_pos < sizeof(major_str) - 1)
                     major_str[major_str_pos++] = cur_api[j];
               }
               else if (found_major && found_minor && cur_api[j] >= '0' && cur_api[j] <= '9')
               {
                  if (minor_str_pos < sizeof(minor_str) - 1)
                     minor_str[minor_str_pos++] = cur_api[j];
               }
               else if (cur_api[j] == '.')
                  found_minor = true;
               break;
            }
            default:
               break;
         }
      }

      sscanf(major_str, "%d", &major);
      sscanf(minor_str, "%d", &minor);
      snprintf(version, sizeof(version), "%d.%d", major, minor);
#if 0
      printf("Major: %d\n", major);
      printf("Minor: %d\n", minor);
      printf("API: %s\n", api_str);
      printf("Version: %s\n", version);
      fflush(stdout);
#endif

      if ((string_is_equal_noncase(api_str, "opengl") && sys_api == GFX_CTX_OPENGL_API) ||
            (string_is_equal_noncase(api_str, "openglcompat") && sys_api == GFX_CTX_OPENGL_API) ||
            (string_is_equal_noncase(api_str, "openglcompatibility") && sys_api == GFX_CTX_OPENGL_API))
      {
         /* system is running a core context while compat is requested */
         if (sys_flags.flags & (1 << GFX_CTX_FLAGS_GL_CORE_CONTEXT))   
            return false;

         sscanf(sys_api_version_str, "%d.%d", &sys_api_version_major, &sys_api_version_minor);

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "openglcore") && sys_api == GFX_CTX_OPENGL_API)
      {
         sscanf(sys_api_version_str, "%d.%d", &sys_api_version_major, &sys_api_version_minor);

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "opengles") && sys_api == GFX_CTX_OPENGL_ES_API)
      {
         sscanf(sys_api_version_str, "OpenGL ES %d.%d", &sys_api_version_major, &sys_api_version_minor);

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "direct3d8") && sys_api == GFX_CTX_DIRECT3D8_API)
      {
         sys_api_version_major = 8;
         sys_api_version_minor = 0;

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "direct3d9") && sys_api == GFX_CTX_DIRECT3D9_API)
      {
         sys_api_version_major = 9;
         sys_api_version_minor = 0;

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "direct3d10") && sys_api == GFX_CTX_DIRECT3D10_API)
      {
         sys_api_version_major = 10;
         sys_api_version_minor = 0;

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "direct3d11") && sys_api == GFX_CTX_DIRECT3D11_API)
      {
         sys_api_version_major = 11;
         sys_api_version_minor = 0;

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "direct3d12") && sys_api == GFX_CTX_DIRECT3D12_API)
      {
         sys_api_version_major = 12;
         sys_api_version_minor = 0;

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "vulkan") && sys_api == GFX_CTX_VULKAN_API)
      {
         sscanf(sys_api_version_str, "%d.%d", &sys_api_version_major, &sys_api_version_minor);

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
      else if (string_is_equal_noncase(api_str, "metal") && sys_api == GFX_CTX_METAL_API)
      {
         sscanf(sys_api_version_str, "%d.%d", &sys_api_version_major, &sys_api_version_minor);

         if (core_info_compare_api_version(sys_api_version_major, sys_api_version_minor, major, minor, op))
            return true;
      }
   }

   return false;
#else
   return true;
#endif
}
