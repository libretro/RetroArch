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
#include <streams/file_stream.h>
#include <lists/dir_list.h>
#include <file/archive_file.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "retroarch.h"

#include "core_info.h"
#include "file_path_special.h"

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#include "uwp/uwp_func.h"
#endif

#if defined(ANDROID)
#include "play_feature_delivery/play_feature_delivery.h"
#endif

enum compare_op
{
   COMPARE_OP_EQUAL = 0,
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
         struct config_entry_list 
            *entry         = NULL;
         bool tmp_bool     = false;
         path_key[0]       = desc_key[0] = opt_key[0] = '\0';

         snprintf(path_key, sizeof(path_key), "firmware%u_path", c);
         snprintf(desc_key, sizeof(desc_key), "firmware%u_desc", c);
         snprintf(opt_key,  sizeof(opt_key),  "firmware%u_opt",  c);

         entry             = config_get_entry(config, path_key);

         if (entry && !string_is_empty(entry->value))
            info->firmware[c].path = strdup(entry->value);

         entry             = config_get_entry(config, desc_key);

         if (entry && !string_is_empty(entry->value))
            info->firmware[c].desc     = strdup(entry->value);

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
      free(info->description);
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

      free(info->core_file_id.str);
   }

   free(core_info_list->all_ext);
   free(core_info_list->list);
   free(core_info_list);
}

static config_file_t *core_info_list_iterate(
      const char *current_path,
      const char *path_basedir)
{
   char info_path[PATH_MAX_LENGTH];
   char info_path_base[PATH_MAX_LENGTH];

   if (!current_path)
      return NULL;

   info_path     [0]          = '\0';
   info_path_base[0]          = '\0';

   fill_pathname_base_noext(info_path_base,
         current_path,
         sizeof(info_path_base));

#if defined(RARCH_MOBILE) || (defined(RARCH_CONSOLE) && !defined(PSP) && !defined(_3DS) && !defined(VITA) && !defined(HW_WUP))
   {
      char *substr = strrchr(info_path_base, '_');
      if (substr)
         *substr = '\0';
   }
#endif

   strlcat(info_path_base, ".info", sizeof(info_path_base));

   fill_pathname_join(info_path,
         path_basedir,
         info_path_base, sizeof(info_path_base));

   if (path_is_valid(info_path))
      return config_file_new_from_path_to_string(info_path);
   return NULL;
}

static core_info_list_t *core_info_list_new(const char *path,
      const char *libretro_info_dir,
      const char *exts,
      bool dir_show_hidden_files)
{
   size_t i;
   struct string_list contents      = {0};
   core_info_t *core_info           = NULL;
   core_info_list_t *core_info_list = NULL;
   const char       *path_basedir   = libretro_info_dir;
   bool                          ok = false;

   string_list_initialize(&contents);

   if (dir_list_append(&contents, path, exts,
         false, dir_show_hidden_files, false, false))
      ok                            = true;

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   {
      /* UWP: browse the optional packages for additional cores */
      struct string_list core_packages = {0};
      
      if (string_list_initialize(&core_packages))
      {
         uwp_fill_installed_core_packages(&core_packages);
         for (i = 0; i < core_packages.size; i++)
            dir_list_append(&contents, core_packages.elems[i].data, exts,
                  false, dir_show_hidden_files, false, false);
         string_list_deinitialize(&core_packages);
      }
   }
#else
   /* Keep the old 'directory not found' behavior */
   if (!ok)
      goto error;
#endif

   core_info_list = (core_info_list_t*)malloc(sizeof(*core_info_list));
   if (!core_info_list)
      goto error;

   core_info_list->list    = NULL;
   core_info_list->count   = 0;
   core_info_list->all_ext = NULL;

   core_info               = (core_info_t*)
      calloc(contents.size, sizeof(*core_info));

   if (!core_info)
   {
      core_info_list_free(core_info_list);
      goto error;
   }

   core_info_list->list    = core_info;
   core_info_list->count   = contents.size;

   for (i = 0; i < contents.size; i++)
   {
      const char *base_path = contents.elems[i].data;
      config_file_t *conf   = core_info_list_iterate(base_path,
            path_basedir);

      if (conf)
      {
         bool tmp_bool      = false;
         unsigned tmp_uint  = 0;
         struct config_entry_list 
            *entry = config_get_entry(conf, "display_name");

         if (entry && !string_is_empty(entry->value))
            core_info[i].display_name = strdup(entry->value);

         entry = config_get_entry(conf, "display_version");

         if (entry && !string_is_empty(entry->value))
            core_info[i].display_version = strdup(entry->value);

         entry = config_get_entry(conf, "corename");

         if (entry && !string_is_empty(entry->value))
            core_info[i].core_name = strdup(entry->value);

         entry = config_get_entry(conf, "systemname");

         if (entry && !string_is_empty(entry->value))
               core_info[i].systemname = strdup(entry->value);

         entry = config_get_entry(conf, "systemid");

         if (entry && !string_is_empty(entry->value))
            core_info[i].system_id = strdup(entry->value);

         entry = config_get_entry(conf, "manufacturer");

         if (entry && !string_is_empty(entry->value))
            core_info[i].system_manufacturer = strdup(entry->value);

         config_get_uint(conf, "firmware_count", &tmp_uint);
         core_info[i].firmware_count = tmp_uint;

         entry = config_get_entry(conf, "supported_extensions");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].supported_extensions      = strdup(entry->value);
            core_info[i].supported_extensions_list =
               string_split(core_info[i].supported_extensions, "|");
         }

         entry = config_get_entry(conf, "authors");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].authors      = strdup(entry->value);
            core_info[i].authors_list =
               string_split(core_info[i].authors, "|");
         }

         entry = config_get_entry(conf, "permissions");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].permissions      = strdup(entry->value);
            core_info[i].permissions_list =
               string_split(core_info[i].permissions, "|");
         }

         entry = config_get_entry(conf, "license");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].licenses      = strdup(entry->value);
            core_info[i].licenses_list =
               string_split(core_info[i].licenses, "|");
         }

         entry = config_get_entry(conf, "categories");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].categories      = strdup(entry->value);
            core_info[i].categories_list =
               string_split(core_info[i].categories, "|");
         }

         entry = config_get_entry(conf, "database");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].databases      = strdup(entry->value);
            core_info[i].databases_list =
               string_split(core_info[i].databases, "|");
         }

         entry = config_get_entry(conf, "notes");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].notes     = strdup(entry->value);
            core_info[i].note_list =
               string_split(core_info[i].notes, "|");
         }

         entry = config_get_entry(conf, "required_hw_api");

         if (entry && !string_is_empty(entry->value))
         {
            core_info[i].required_hw_api      = strdup(entry->value);
            core_info[i].required_hw_api_list =
               string_split(core_info[i].required_hw_api, "|");
         }

         entry = config_get_entry(conf, "description");

         if (entry && !string_is_empty(entry->value))
            core_info[i].description = strdup(entry->value);

         if (config_get_bool(conf, "supports_no_game",
                  &tmp_bool))
            core_info[i].supports_no_game = tmp_bool;

         if (config_get_bool(conf, "database_match_archive_member",
                  &tmp_bool))
            core_info[i].database_match_archive_member = tmp_bool;

         if (config_get_bool(conf, "is_experimental",
                  &tmp_bool))
            core_info[i].is_experimental = tmp_bool;

         core_info[i].config_data = conf;
      }

      if (!string_is_empty(base_path))
      {
         const char *core_filename = path_basename(base_path);

         /* Cache core path */
         core_info[i].path = strdup(base_path);

         /* Cache core file 'id'
          * > Filename without extension or platform-specific suffix */
         if (!string_is_empty(core_filename))
         {
            char *core_file_id = strdup(core_filename);
            path_remove_extension(core_file_id);

            if (!string_is_empty(core_file_id))
            {
#if defined(RARCH_MOBILE) || (defined(RARCH_CONSOLE) && !defined(PSP) && !defined(_3DS) && !defined(VITA) && !defined(HW_WUP))
               char *last_underscore = strrchr(core_file_id, '_');
               if (last_underscore)
                  *last_underscore = '\0';
#endif
               core_info[i].core_file_id.str = core_file_id;
               core_info[i].core_file_id.len = strlen(core_file_id);

               core_file_id = NULL;
            }

            if (core_file_id)
            {
               free(core_file_id);
               core_file_id = NULL;
            }

            /* Get fallback display name, if required */
            if (!core_info[i].display_name)
               core_info[i].display_name = strdup(core_filename);
         }
      }

      /* Get core lock status */
      core_info[i].is_locked = core_info_get_core_lock(core_info[i].path, false);
   }

   core_info_list_resolve_all_extensions(core_info_list);
   core_info_list_resolve_all_firmware(core_info_list);

   string_list_deinitialize(&contents);
   return core_info_list;

error:
   string_list_deinitialize(&contents);
   return NULL;
}

/* Shallow-copies internal state.
 *
 * Data in *info is invalidated when the
 * core_info_list is freed. */
bool core_info_list_get_info(core_info_list_t *core_info_list,
      core_info_t *out_info, const char *path)
{
   size_t i;
   const char *core_filename = NULL;

   if (!core_info_list || !out_info || string_is_empty(path))
      return false;

   core_filename = path_basename(path);
   if (string_is_empty(core_filename))
      return false;

   memset(out_info, 0, sizeof(*out_info));

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];

      if (!info || (info->core_file_id.len == 0))
         continue;

      if (!strncmp(info->core_file_id.str, core_filename, info->core_file_id.len))
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
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   const core_info_t          *a = (const core_info_t*)a_;
   const core_info_t          *b = (const core_info_t*)b_;
   int support_a                 = core_info_does_support_file(a,
         p_coreinfo->tmp_path);
   int support_b                 = core_info_does_support_file(b,
         p_coreinfo->tmp_path);
#ifdef HAVE_COMPRESSION
   support_a            = support_a ||
      core_info_does_support_any_file(a, p_coreinfo->tmp_list);
   support_b            = support_b ||
      core_info_does_support_any_file(b, p_coreinfo->tmp_list);
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
   const char *core_filename = NULL;

   if (!list || string_is_empty(core))
      return NULL;

   core_filename = path_basename(core);
   if (string_is_empty(core_filename))
      return NULL;

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = core_info_get(list, i);

      if (!info || (info->core_file_id.len == 0))
         continue;

      if (!strncmp(info->core_file_id.str, core_filename, info->core_file_id.len))
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
   char path[PATH_MAX_LENGTH];
   core_info_t      *info = NULL;

   if (!core_info_list || !core)
      return false;

   info                   = core_info_find_internal(core_info_list, core);

   if (!info)
      return false;

   path[0]                = '\0';

   for (i = 0; i < info->firmware_count; i++)
   {
      if (string_is_empty(info->firmware[i].path))
         continue;

      fill_pathname_join(path, systemdir,
            info->firmware[i].path, sizeof(path));
      info->firmware[i].missing = !path_is_valid(path);
      if (info->firmware[i].missing && !info->firmware[i].optional)
         *set_missing_bios = true;
   }

   return true;
}

void core_info_free_current_core(core_info_state_t *p_coreinfo)
{
   if (p_coreinfo->current)
      free(p_coreinfo->current);
   p_coreinfo->current = NULL;
}

bool core_info_init_current_core(void)
{
   core_info_state_t *p_coreinfo          = coreinfo_get_ptr();
   core_info_t *current                   = (core_info_t*)
      malloc(sizeof(*current));
   if (!current)
      return false;
   current->supports_no_game              = false;
   current->database_match_archive_member = false;
   current->is_experimental               = false;
   current->is_locked                     = false;
   current->firmware_count                = 0;
   current->path                          = NULL;
   current->config_data                   = NULL;
   current->display_name                  = NULL;
   current->display_version               = NULL;
   current->core_name                     = NULL;
   current->system_manufacturer           = NULL;
   current->systemname                    = NULL;
   current->system_id                     = NULL;
   current->supported_extensions          = NULL;
   current->authors                       = NULL;
   current->permissions                   = NULL;
   current->licenses                      = NULL;
   current->categories                    = NULL;
   current->databases                     = NULL;
   current->notes                         = NULL;
   current->required_hw_api               = NULL;
   current->description                   = NULL;
   current->categories_list               = NULL;
   current->databases_list                = NULL;
   current->note_list                     = NULL;
   current->supported_extensions_list     = NULL;
   current->authors_list                  = NULL;
   current->permissions_list              = NULL;
   current->licenses_list                 = NULL;
   current->required_hw_api_list          = NULL;
   current->firmware                      = NULL;
   current->core_file_id.str              = NULL;
   current->core_file_id.len              = 0;
   current->userdata                      = NULL;

   p_coreinfo->current                    = current;
   return true;
}

bool core_info_get_current_core(core_info_t **core)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   if (!core)
      return false;
   *core = p_coreinfo->current;
   return true;
}

void core_info_deinit_list(void)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   if (p_coreinfo->curr_list)
      core_info_list_free(p_coreinfo->curr_list);
   p_coreinfo->curr_list = NULL;
}

bool core_info_init_list(const char *path_info, const char *dir_cores,
      const char *exts, bool dir_show_hidden_files)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   if (!(p_coreinfo->curr_list = core_info_list_new(dir_cores,
               !string_is_empty(path_info) ? path_info : dir_cores,
               exts,
               dir_show_hidden_files)))
      return false;
   return true;
}

bool core_info_get_list(core_info_list_t **core)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   if (!core)
      return false;
   *core = p_coreinfo->curr_list;
   return true;
}

/* Returns number of installed cores */
size_t core_info_count(void)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   if (!p_coreinfo || !p_coreinfo->curr_list)
      return 0;
   return p_coreinfo->curr_list->count;
}

bool core_info_list_update_missing_firmware(core_info_ctx_firmware_t *info,
      bool *set_missing_bios)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   if (!info)
      return false;
   return core_info_list_update_missing_firmware_internal(
         p_coreinfo->curr_list,
         info->path, info->directory.system,
         set_missing_bios);
}

bool core_info_load(
      core_info_ctx_find_t *info,
      core_info_state_t *p_coreinfo)
{
   core_info_t    *core_info     = NULL;

   if (!info)
      return false;

   if (!p_coreinfo->current)
      core_info_init_current_core();

   core_info_get_current_core(&core_info);

   if (!p_coreinfo->curr_list)
      return false;

   if (!core_info_list_get_info(p_coreinfo->curr_list,
            core_info, info->path))
      return false;

   return true;
}

bool core_info_find(core_info_ctx_find_t *info)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();

   if (!info || !p_coreinfo->curr_list)
      return false;

   info->inf = core_info_find_internal(p_coreinfo->curr_list, info->path);

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
   size_t supported              = 0;
#ifdef HAVE_COMPRESSION
   struct string_list *list      = NULL;
#endif
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();

   if (!core_info_list)
      return;

   p_coreinfo->tmp_path          = path;

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(path))
      list = file_archive_get_file_list(path, NULL);
   p_coreinfo->tmp_list = list;
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

/*
 * Matches core path A and B "base" filename (ignoring everything after _libretro)
 *
 * Ex:
 *   snes9x_libretro.dll and snes9x_libretro_android.so are matched
 *   snes9x__2005_libretro.dll and snes9x_libretro_android.so are NOT matched
 */
bool core_info_core_file_id_is_equal(const char* core_path_a, const char* core_path_b)
{
   const char *core_path_basename_a = NULL;
   const char *extension_pos        = NULL;
   const char *underscore_pos       = NULL;

   if (!core_path_a || !core_path_b)
      return false;

   core_path_basename_a = path_basename(core_path_a);

   if (core_path_basename_a)
   {
      extension_pos = strrchr(core_path_basename_a, '.');

      if (extension_pos)
      {
         /* Remove extension */
         *((char*)extension_pos) = '\0';

         underscore_pos = strrchr(core_path_basename_a, '_');

         /* Restore extension */
         *((char*)extension_pos) = '.';

         if (underscore_pos)
         {
            size_t core_base_file_id_length  = underscore_pos - core_path_basename_a;
            const char* core_path_basename_b = path_basename(core_path_b);

            if (string_starts_with_size(core_path_basename_a, core_path_basename_b,
                  core_base_file_id_length))
               return true;
         }
      }
   }

   return false;
}

void core_info_get_name(const char *path, char *s, size_t len,
      const char *path_info, const char *dir_cores,
      const char *exts, bool dir_show_hidden_files,
      bool get_display_name)
{
   size_t i;
   struct string_list contents      = {0};
   const char       *path_basedir   = !string_is_empty(path_info) ?
      path_info : dir_cores;
   const char *core_path_basename   = path_basename(path);

   if (!dir_list_initialize(&contents,
            dir_cores, exts, false, dir_show_hidden_files, false, false))
      return;

   for (i = 0; i < contents.size; i++)
   {
      struct config_entry_list 
         *entry                       = NULL;
      config_file_t *conf             = NULL;
      const char *current_path        = contents.elems[i].data;

      if (!string_is_equal(path_basename(current_path), core_path_basename))
         continue;

      conf = core_info_list_iterate(contents.elems[i].data,
               path_basedir);

      if (!conf)
         continue;

      if (get_display_name)
         entry = config_get_entry(conf, "display_name");
      else
         entry = config_get_entry(conf, "corename");

      if (entry && !string_is_empty(entry->value))
         strlcpy(s, entry->value, len);

      config_file_free(conf);
      break;
   }

   dir_list_deinitialize(&contents);
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
   char      *database           = NULL;
   const char      *new_path     = path_basename(database_path);
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();

   if (string_is_empty(new_path))
      return false;

   database                 = strdup(new_path);

   if (string_is_empty(database))
      goto error;

   path_remove_extension(database);

   if (p_coreinfo->curr_list)
   {
      size_t i;

      for (i = 0; i < p_coreinfo->curr_list->count; i++)
      {
         const core_info_t *info = &p_coreinfo->curr_list->list[i];

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
   char      *database           = NULL;
   const char      *new_path     = path_basename(database_path);
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();

   if (string_is_empty(new_path))
      return false;

   database                 = strdup(new_path);

   if (string_is_empty(database))
      goto error;

   path_remove_extension(database);

   if (p_coreinfo->curr_list)
   {
      size_t i;

      for (i = 0; i < p_coreinfo->curr_list->count; i++)
      {
         const core_info_t *info = &p_coreinfo->curr_list->list[i];

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
   const char *core_filename = NULL;

   if (!core_info_list || string_is_empty(path))
      return false;

   core_filename = path_basename(path);
   if (string_is_empty(core_filename))
      return false;

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *info = &core_info_list->list[i];

      if (!info || (info->core_file_id.len == 0))
         continue;

      if (!strncmp(info->core_file_id.str, core_filename, info->core_file_id.len))
      {
         if (string_is_empty(info->display_name))
            break;

         strlcpy(s, info->display_name, len);
         return true;
      }
   }

   return false;
}

bool core_info_get_display_name(const char *path, char *s, size_t len)
{
   struct config_entry_list 
      *entry           = NULL;
   config_file_t *conf = config_file_new_from_path_to_string(path);

   if (!conf)
      return false;

   entry               = config_get_entry(conf, "display_name");

   if (entry && !string_is_empty(entry->value))
      strlcpy(s, entry->value, len);

   config_file_free(conf);
   return true;
}

/* Returns core_info parameters required for
 * core updater tasks, read from specified file.
 * Returned core_updater_info_t object must be
 * freed using core_info_free_core_updater_info().
 * Returns NULL if 'path' is invalid. */
core_updater_info_t *core_info_get_core_updater_info(const char *path)
{
   struct config_entry_list 
      *entry                 = NULL;
   bool tmp_bool             = false;
   core_updater_info_t *info = NULL;
   config_file_t *conf       = NULL;

   if (string_is_empty(path))
      return NULL;

   /* Read config file */
   conf = config_file_new_from_path_to_string(path);

   if (!conf)
      return NULL;

   /* Create info struct */
   info                      = (core_updater_info_t*)malloc(sizeof(*info));

   if (!info)
      return NULL;

   info->is_experimental     = false;
   info->display_name        = NULL;
   info->description         = NULL;
   info->licenses            = NULL;

   /* Fetch required parameters */

   /* > is_experimental */
   if (config_get_bool(conf, "is_experimental", &tmp_bool))
      info->is_experimental  = tmp_bool;

   /* > display_name */
   entry                     = config_get_entry(conf, "display_name");

   if (entry && !string_is_empty(entry->value))
      info->display_name     = strdup(entry->value);

   /* > description */
   entry                     = config_get_entry(conf, "description");

   if (entry && !string_is_empty(entry->value))
      info->description      = strdup(entry->value);

   /* > licenses */
   entry                     = config_get_entry(conf, "license");

   if (entry && !string_is_empty(entry->value))
      info->licenses         = strdup(entry->value);

   /* Clean up */
   config_file_free(conf);

   return info;
}

void core_info_free_core_updater_info(core_updater_info_t *info)
{
   if (!info)
      return;

   if (info->display_name)
      free(info->display_name);

   if (info->description)
      free(info->description);

   if (info->licenses)
      free(info->licenses);

   free(info);
   info = NULL;
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
   int sys_api_version_major       = 0;
   int sys_api_version_minor       = 0;
   const char *sys_api_version_str = video_driver_get_gpu_api_version_string();
   gfx_ctx_flags_t sys_flags       = video_driver_get_flags_wrapper();

   enum api_parse_state
   {
      STATE_API_NAME,
      STATE_API_COMPARE_OP,
      STATE_API_VERSION
   };

   if (!info || !info->required_hw_api_list || info->required_hw_api_list->size == 0)
      return true;

   sys_api = video_context_driver_get_api();

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
      int major                  = 0;
      int minor                  = 0;
      unsigned cur_api_len       = 0;
      unsigned j                 = 0;
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
               if (  ISUPPER((unsigned char)cur_api[j]) || 
                     ISLOWER((unsigned char)cur_api[j]))
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
               if (    !found_minor 
                     && cur_api[j] >= '0'
                     && cur_api[j] <= '9'
                     && cur_api[j] != '.')
               {
                  found_major = true;

                  if (major_str_pos < sizeof(major_str) - 1)
                     major_str[major_str_pos++] = cur_api[j];
               }
               else if (
                        found_major 
                     && found_minor
                     && cur_api[j] >= '0'
                     && cur_api[j] <= '9')
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

/* Sets 'locked' status of specified core
 * > Returns true if successful
 * > Like all functions that access the cached
 *   core info list this is *not* thread safe */
bool core_info_set_core_lock(const char *core_path, bool lock)
{
   core_info_ctx_find_t core_info;
   char lock_file_path[PATH_MAX_LENGTH];
   bool lock_file_exists = false;

   lock_file_path[0] = '\0';

#if defined(ANDROID)
   /* Play Store builds do not support
    * core locking */
   if (play_feature_delivery_enabled())
      return false;
#endif

   if (string_is_empty(core_path))
      return false;

   /* Search for specified core */
   core_info.inf  = NULL;
   core_info.path = core_path;

   if (!core_info_find(&core_info))
      return false;

   if (string_is_empty(core_info.inf->path))
      return false;

   /* Get lock file path */
   strlcpy(lock_file_path, core_info.inf->path,
         sizeof(lock_file_path));
   strlcat(lock_file_path, FILE_PATH_LOCK_EXTENSION,
         sizeof(lock_file_path));

   /* Check whether lock file exists */
   lock_file_exists = path_is_valid(lock_file_path);

   /* Create or delete lock file, as required */
   if (lock && !lock_file_exists)
   {
      RFILE *lock_file = filestream_open(
            lock_file_path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!lock_file)
         return false;

      /* We have to write something - just output
       * a single character */
      if (filestream_putc(lock_file, 0) != 0)
      {
         filestream_close(lock_file);
         return false;
      }

      filestream_close(lock_file);
   }
   else if (!lock && lock_file_exists)
      if (filestream_delete(lock_file_path) != 0)
         return false;

   /* File operations were successful - update
    * core info entry */
   core_info.inf->is_locked = lock;

   return true;
}

/* Fetches 'locked' status of specified core
 * > If 'validate_path' is 'true', will search
 *   cached core info list for a corresponding
 *   'sanitised' core file path. This is *not*
 *   thread safe
 * > If 'validate_path' is 'false', performs a
 *   direct filesystem check. This *is* thread
 *   safe, but validity of specified core path
 *   must be checked externally */
bool core_info_get_core_lock(const char *core_path, bool validate_path)
{
   char lock_file_path[PATH_MAX_LENGTH];
   const char *core_file_path = NULL;
   bool is_locked             = false;
   core_info_ctx_find_t core_info;

   lock_file_path[0] = '\0';

#if defined(ANDROID)
   /* Play Store builds do not support
    * core locking */
   if (play_feature_delivery_enabled())
      return false;
#endif

   if (string_is_empty(core_path))
      return false;

   core_info.inf  = NULL;
   core_info.path = NULL;

   /* Check whether core path is to be validated */
   if (validate_path)
   {
      core_info.path = core_path;

      if (core_info_find(&core_info))
         core_file_path = core_info.inf->path;
   }
   else
      core_file_path = core_path;

   /* A core cannot be locked if it does not exist... */
   if (string_is_empty(core_file_path) ||
       !path_is_valid(core_file_path))
      return false;

   /* Get lock file path */
   strlcpy(lock_file_path, core_file_path,
         sizeof(lock_file_path));
   strlcat(lock_file_path, FILE_PATH_LOCK_EXTENSION,
         sizeof(lock_file_path));

   /* Check whether lock file exists */
   is_locked = path_is_valid(lock_file_path);

   /* If core path has been validated (and a
    * core info object is available), ensure
    * that core info 'is_locked' field is
    * up to date */
   if (validate_path && core_info.inf)
      core_info.inf->is_locked = is_locked;

   return is_locked;
}
