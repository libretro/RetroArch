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

#include "verbosity.h"

#include "core_info.h"
#include "file_path_special.h"

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#include "uwp/uwp_func.h"
#endif

static const char *core_info_tmp_path               = NULL;
static const struct string_list *core_info_tmp_list = NULL;
static core_info_t *core_info_current               = NULL;
static core_info_list_t *core_info_curr_list        = NULL;

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

   all_ext_len += strlen("7z|") + strlen("zip|");

   if (all_ext_len)
      all_ext = (char*)calloc(1, all_ext_len);

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

static bool core_info_list_iterate(
      char *s, size_t len,
      const char *path_basedir,
      struct string_list *contents, size_t i)
{
   size_t info_path_base_size = PATH_MAX_LENGTH * sizeof(char);
   char *info_path_base       = NULL;
   char             *substr   = NULL;
   const char *current_path   = contents ? contents->elems[i].data : NULL;

   (void)substr;

   if (!current_path)
      return false;

   info_path_base             = (char*)malloc(info_path_base_size);

   info_path_base[0] = '\0';

   fill_pathname_base_noext(info_path_base,
         current_path,
         info_path_base_size);

#if defined(RARCH_MOBILE) || (defined(RARCH_CONSOLE) && !defined(PSP) && !defined(_3DS) && !defined(VITA) && !defined(PS2) && !defined(HW_WUP))
   substr = strrchr(info_path_base, '_');
   if (substr)
      *substr = '\0';
#endif

   strlcat(info_path_base,
         file_path_str(FILE_PATH_CORE_INFO_EXTENSION),
         info_path_base_size);

   fill_pathname_join(s,
         path_basedir,
         info_path_base, len);

   free(info_path_base);
   return true;
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
      contents = NULL;
   }
#endif

   if (!contents)
      return NULL;

   core_info_list = (core_info_list_t*)calloc(1, sizeof(*core_info_list));
   if (!core_info_list)
      goto error;

   core_info = (core_info_t*)calloc(contents->size, sizeof(*core_info));
   if (!core_info)
      goto error;

   core_info_list->list  = core_info;
   core_info_list->count = contents->size;

   for (i = 0; i < contents->size; i++)
   {
      size_t info_path_size = PATH_MAX_LENGTH * sizeof(char);
      char *info_path       = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      info_path[0]          = '\0';

      if (
            core_info_list_iterate(info_path, info_path_size,
               path_basedir, contents, i)
            && path_is_valid(info_path))
      {
         char *tmp           = NULL;
         bool tmp_bool       = false;
         unsigned count      = 0;
         config_file_t *conf = config_file_new(info_path);

         free(info_path);

         if (!conf)
            continue;

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

         config_get_uint(conf, "firmware_count", &count);

         core_info[i].firmware_count = count;

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

         if (tmp)
            free(tmp);
         tmp    = NULL;

         if (config_get_bool(conf, "supports_no_game",
               &tmp_bool))
            core_info[i].supports_no_game = tmp_bool;

         if (config_get_bool(conf, "database_match_archive_member",
               &tmp_bool))
            core_info[i].database_match_archive_member = tmp_bool;

         core_info[i].config_data = conf;
      }
      else
         free(info_path);

      if (!string_is_empty(contents->elems[i].data))
         core_info[i].path = strdup(contents->elems[i].data);

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

error:
   if (contents)
      string_list_free(contents);
   core_info_list_free(core_info_list);
   return NULL;
}

/* Shallow-copies internal state.
 *
 * Data in *info is invalidated when the
 * core_info_list is freed. */
static bool core_info_list_get_info(core_info_list_t *core_info_list,
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
      info->firmware[i].missing = !filestream_exists(path);
      if (info->firmware[i].missing && !info->firmware[i].optional)
      {
         *set_missing_bios = true;
         RARCH_WARN("Firmware missing: %s\n", info->firmware[i].path);
      }
   }

   free(path);
   return true;
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
   char path[PATH_MAX_LENGTH];
   core_info_t          *info = NULL;

   if (!core_info_list || !core)
      return;

   path[0]       = '\0';
   *firmware     = NULL;
   *num_firmware = 0;
   info          = core_info_find_internal(core_info_list, core);

   if (!info)
      return;

   *firmware = info->firmware;

   for (i = 1; i < info->firmware_count; i++)
   {
      fill_pathname_join(path, systemdir,
            info->firmware[i].path, sizeof(path));
      info->firmware[i].missing = !filestream_exists(path);
      *num_firmware += info->firmware[i].missing;
   }

   qsort(info->firmware, info->firmware_count, sizeof(*info->firmware),
         core_info_firmware_cmp);
}
#endif

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
   struct string_list *list = NULL;
   size_t supported         = 0;

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

   if (list)
      string_list_free(list);

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
      size_t path_size                = PATH_MAX_LENGTH * sizeof(char);
      char *info_path                 = NULL;
      config_file_t *conf             = NULL;
      char *new_core_name             = NULL;
      const char *current_path        = contents->elems[i].data;

      if (!string_is_equal(path_basename(current_path), core_path_basename))
         continue;

      info_path                       = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      info_path[0]                    = '\0';

      if (!core_info_list_iterate(info_path,
               path_size, path_basedir, contents, i)
            && path_is_valid(info_path))
      {
         free(info_path);
         continue;
      }

      conf                            = config_file_new(info_path);

      if (!conf)
      {
         free(info_path);
         continue;
      }

      if (config_get_string(conf, get_display_name ? "display_name" : "corename",
            &new_core_name))
      {
         strlcpy(s, new_core_name, len);
         free(new_core_name);
      }

      config_file_free(conf);
      free(info_path);
      break;
   }

   if (contents)
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

bool core_info_database_supports_content_path(const char *database_path, const char *path)
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
   bool               ret   = true;
   char       *tmp          = NULL;
   config_file_t *conf      = config_file_new(path);

   if (!conf)
   {
      ret = false;
      goto error;
   }

   if (config_get_string(conf, "display_name", &tmp))
   {
      strlcpy(s, tmp, len);
      free(tmp);
   }

error:
   config_file_free(conf);

   return ret;
}
