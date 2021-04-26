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

static uint32_t core_info_hash_string(const char *str)
{
   unsigned char c;
   uint32_t hash = (uint32_t)0x811c9dc5;
   while ((c = (unsigned char)*(str++)) != '\0')
      hash = ((hash * (uint32_t)0x01000193) ^ (uint32_t)c);
   return (hash ? hash : 1);
}

static bool core_info_get_file_id(const char *core_filename,
      char *core_file_id, size_t len)
{
   char *last_underscore = NULL;

   if (string_is_empty(core_filename))
      return false;

   /* Core file 'id' is filename without extension
    * or platform-specific suffix */

   /* > Remove extension */
   strlcpy(core_file_id, core_filename, len);
   path_remove_extension(core_file_id);

   /* > Remove suffix */
   last_underscore = (char*)strrchr(core_file_id, '_');

   if (!string_is_empty(last_underscore) &&
       !string_is_equal(last_underscore, "_libretro"))
      *last_underscore = '\0';

   return !string_is_empty(core_file_id);
}

static core_info_t *core_info_find_internal(
      core_info_list_t *list,
      const char *core_path)
{
   char core_file_id[256];
   uint32_t hash;
   size_t i;

   core_file_id[0] = '\0';

   if (!list ||
       string_is_empty(core_path) ||
       !core_info_get_file_id(path_basename_nocompression(core_path),
            core_file_id, sizeof(core_file_id)))
      return NULL;

   hash = core_info_hash_string(core_file_id);

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = &list->list[i];

      if (info->core_file_id.hash == hash)
         return info;
   }

   return NULL;
}

static void core_info_resolve_firmware(
      core_info_t *info, config_file_t *conf)
{
   unsigned i;
   unsigned firmware_count        = 0;
   core_info_firmware_t *firmware = NULL;

   if (!config_get_uint(conf, "firmware_count", &firmware_count))
      return;

   firmware = (core_info_firmware_t*)calloc(firmware_count, sizeof(*firmware));

   if (!firmware)
      return;

   for (i = 0; i < firmware_count; i++)
   {
      char path_key[64];
      char desc_key[64];
      char opt_key[64];
      struct config_entry_list *entry = NULL;
      bool tmp_bool                   = false;

      path_key[0] = '\0';
      desc_key[0] = '\0';
      opt_key[0]  = '\0';

      snprintf(path_key, sizeof(path_key), "firmware%u_path", i);
      snprintf(desc_key, sizeof(desc_key), "firmware%u_desc", i);
      snprintf(opt_key,  sizeof(opt_key),  "firmware%u_opt",  i);

      entry = config_get_entry(conf, path_key);

      if (entry && !string_is_empty(entry->value))
      {
         firmware[i].path = entry->value;
         entry->value     = NULL;
      }

      entry = config_get_entry(conf, desc_key);

      if (entry && !string_is_empty(entry->value))
      {
         firmware[i].desc = entry->value;
         entry->value     = NULL;
      }

      if (config_get_bool(conf, opt_key , &tmp_bool))
         firmware[i].optional = tmp_bool;
   }

   info->firmware_count = firmware_count;
   info->firmware       = firmware;
}

static config_file_t *core_info_get_config_file(
      const char *core_file_id,
      const char *info_dir)
{
   char info_path[PATH_MAX_LENGTH];

   info_path[0] = '\0';

   if (string_is_empty(info_dir))
      strlcpy(info_path, core_file_id, sizeof(info_path));
   else
      fill_pathname_join(info_path, info_dir, core_file_id,
            sizeof(info_path));

   strlcat(info_path, ".info", sizeof(info_path));

   return config_file_new_from_path_to_string(info_path);
}

static void core_info_parse_config_file(
      core_info_list_t *list, core_info_t *info,
      config_file_t *conf)
{
   struct config_entry_list *entry = NULL;
   bool tmp_bool                   = false;

   entry = config_get_entry(conf, "display_name");

   if (entry && !string_is_empty(entry->value))
   {
      info->display_name = entry->value;
      entry->value       = NULL;
   }

   entry = config_get_entry(conf, "display_version");

   if (entry && !string_is_empty(entry->value))
   {
      info->display_version = entry->value;
      entry->value          = NULL;
   }

   entry = config_get_entry(conf, "corename");

   if (entry && !string_is_empty(entry->value))
   {
      info->core_name = entry->value;
      entry->value    = NULL;
   }

   entry = config_get_entry(conf, "systemname");

   if (entry && !string_is_empty(entry->value))
   {
      info->systemname = entry->value;
      entry->value     = NULL;
   }

   entry = config_get_entry(conf, "systemid");

   if (entry && !string_is_empty(entry->value))
   {
      info->system_id = entry->value;
      entry->value    = NULL;
   }

   entry = config_get_entry(conf, "manufacturer");

   if (entry && !string_is_empty(entry->value))
   {
      info->system_manufacturer = entry->value;
      entry->value              = NULL;
   }

   entry = config_get_entry(conf, "supported_extensions");

   if (entry && !string_is_empty(entry->value))
   {
      info->supported_extensions      = entry->value;
      entry->value                    = NULL;

      info->supported_extensions_list =
            string_split(info->supported_extensions, "|");
   }

   entry = config_get_entry(conf, "authors");

   if (entry && !string_is_empty(entry->value))
   {
      info->authors      = entry->value;
      entry->value       = NULL;

      info->authors_list =
            string_split(info->authors, "|");
   }

   entry = config_get_entry(conf, "permissions");

   if (entry && !string_is_empty(entry->value))
   {
      info->permissions      = entry->value;
      entry->value           = NULL;

      info->permissions_list =
            string_split(info->permissions, "|");
   }

   entry = config_get_entry(conf, "license");

   if (entry && !string_is_empty(entry->value))
   {
      info->licenses      = entry->value;
      entry->value        = NULL;

      info->licenses_list =
            string_split(info->licenses, "|");
   }

   entry = config_get_entry(conf, "categories");

   if (entry && !string_is_empty(entry->value))
   {
      info->categories      = entry->value;
      entry->value          = NULL;

      info->categories_list =
            string_split(info->categories, "|");
   }

   entry = config_get_entry(conf, "database");

   if (entry && !string_is_empty(entry->value))
   {
      info->databases      = entry->value;
      entry->value         = NULL;

      info->databases_list =
            string_split(info->databases, "|");
   }

   entry = config_get_entry(conf, "notes");

   if (entry && !string_is_empty(entry->value))
   {
      info->notes     = entry->value;
      entry->value    = NULL;

      info->note_list =
            string_split(info->notes, "|");
   }

   entry = config_get_entry(conf, "required_hw_api");

   if (entry && !string_is_empty(entry->value))
   {
      info->required_hw_api      = entry->value;
      entry->value               = NULL;

      info->required_hw_api_list =
            string_split(info->required_hw_api, "|");
   }

   entry = config_get_entry(conf, "description");

   if (entry && !string_is_empty(entry->value))
   {
      info->description = entry->value;
      entry->value      = NULL;
   }

   if (config_get_bool(conf, "supports_no_game",
            &tmp_bool))
      info->supports_no_game = tmp_bool;

   if (config_get_bool(conf, "database_match_archive_member",
            &tmp_bool))
      info->database_match_archive_member = tmp_bool;

   if (config_get_bool(conf, "is_experimental",
            &tmp_bool))
      info->is_experimental = tmp_bool;

   core_info_resolve_firmware(info, conf);

   info->has_info = true;
   list->info_count++;
}

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

static core_info_list_t *core_info_list_new(const char *path,
      const char *libretro_info_dir,
      const char *exts,
      bool dir_show_hidden_files)
{
   size_t i;
   struct string_list contents      = {0};
   core_info_t *core_info           = NULL;
   core_info_list_t *core_info_list = NULL;
   const char *info_dir             = libretro_info_dir;
   bool ok                          = false;

   string_list_initialize(&contents);

   ok = dir_list_append(&contents, path, exts,
         false, dir_show_hidden_files, false, false);

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
   /* Keep the old 'directory not found' behaviour */
   if (!ok)
      goto error;
#endif

   core_info_list = (core_info_list_t*)malloc(sizeof(*core_info_list));
   if (!core_info_list)
      goto error;

   core_info_list->list       = NULL;
   core_info_list->count      = 0;
   core_info_list->info_count = 0;
   core_info_list->all_ext    = NULL;

   core_info = (core_info_t*)calloc(contents.size, sizeof(*core_info));

   if (!core_info)
   {
      core_info_list_free(core_info_list);
      goto error;
   }

   core_info_list->list  = core_info;
   core_info_list->count = contents.size;

   for (i = 0; i < contents.size; i++)
   {
      core_info_t *info         = &core_info[i];
      const char *base_path     = contents.elems[i].data;
      const char *core_filename = NULL;
      config_file_t *conf       = NULL;
      char core_file_id[256];

      core_file_id[0] = '\0';

      if (string_is_empty(base_path) ||
          !(core_filename = path_basename_nocompression(base_path)) ||
          !core_info_get_file_id(core_filename, core_file_id,
               sizeof(core_file_id)))
         continue;

      /* Cache core path */
      info->path = strdup(base_path);

      /* Get core lock status */
      info->is_locked = core_info_get_core_lock(info->path, false);

      /* Cache core file 'id' */
      info->core_file_id.str  = strdup(core_file_id);
      info->core_file_id.hash = core_info_hash_string(core_file_id);

      /* Parse core info file */
      conf = core_info_get_config_file(core_file_id, info_dir);

      if (conf)
      {
         core_info_parse_config_file(core_info_list, info, conf);
         config_file_free(conf);
      }

      /* Get fallback display name, if required */
      if (!info->display_name)
         info->display_name = strdup(core_filename);
   }

   core_info_list_resolve_all_extensions(core_info_list);

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
      core_info_t *out_info, const char *core_path)
{
   core_info_t *info = core_info_find_internal(
         core_info_list, core_path);

   if (!out_info)
      return false;

   memset(out_info, 0, sizeof(*out_info));

   if (info)
   {
      *out_info = *info;
      return true;
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

static bool core_info_list_update_missing_firmware_internal(
      core_info_list_t *core_info_list,
      const char *core_path,
      const char *systemdir,
      bool *set_missing_bios)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   core_info_t      *info = NULL;

   if (!core_info_list)
      return false;

   info                   = core_info_find_internal(core_info_list, core_path);

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
   current->has_info                      = false;
   current->supports_no_game              = false;
   current->database_match_archive_member = false;
   current->is_experimental               = false;
   current->is_locked                     = false;
   current->firmware_count                = 0;
   current->path                          = NULL;
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
   current->core_file_id.hash             = 0;

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

bool core_info_load(const char *core_path,
      core_info_state_t *p_coreinfo)
{
   core_info_t    *core_info     = NULL;

   if (!p_coreinfo->current)
      core_info_init_current_core();

   core_info_get_current_core(&core_info);

   if (!p_coreinfo->curr_list)
      return false;

   if (!core_info_list_get_info(p_coreinfo->curr_list,
            core_info, core_path))
      return false;

   return true;
}

bool core_info_find(const char *core_path,
      core_info_t **core_info)
{
   core_info_state_t *p_coreinfo = coreinfo_get_ptr();
   core_info_t *info             = NULL;

   if (!core_info || !p_coreinfo->curr_list)
      return false;

   info = core_info_find_internal(p_coreinfo->curr_list, core_path);

   if (!info)
      return false;

   *core_info = info;
   return true;
}

core_info_t *core_info_get(core_info_list_t *list, size_t i)
{
   core_info_t *info = NULL;

   if (!list || (i >= list->count))
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
 * Matches core A and B file IDs
 *
 * e.g.:
 *   snes9x_libretro.dll and snes9x_libretro_android.so are matched
 *   snes9x__2005_libretro.dll and snes9x_libretro_android.so are 
 *   NOT matched
 */
bool core_info_core_file_id_is_equal(const char *core_path_a,
      const char *core_path_b)
{
   char core_file_id_a[256];
   char core_file_id_b[256];

   core_file_id_a[0] = '\0';
   core_file_id_b[0] = '\0';

   if (string_is_empty(core_path_a) ||
       string_is_empty(core_path_b) ||
       !core_info_get_file_id(path_basename_nocompression(core_path_a),
            core_file_id_a, sizeof(core_file_id_a)) ||
       !core_info_get_file_id(path_basename_nocompression(core_path_b),
            core_file_id_b, sizeof(core_file_id_b)))
      return false;

   return string_is_equal(core_file_id_a, core_file_id_b);
}

bool core_info_database_match_archive_member(const char *database_path)
{
   char      *database           = NULL;
   const char      *new_path     = path_basename_nocompression(database_path);
   core_info_state_t *p_coreinfo = NULL;

   if (string_is_empty(new_path))
      return false;
   if (!(database = strdup(new_path)))
      return false;

   path_remove_extension(database);

   p_coreinfo               = coreinfo_get_ptr();

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

   free(database);
   return false;
}

bool core_info_database_supports_content_path(
      const char *database_path, const char *path)
{
   char      *database           = NULL;
   const char      *new_path     = path_basename(database_path);
   core_info_state_t *p_coreinfo = NULL;

   if (string_is_empty(new_path))
      return false;
   if (!(database = strdup(new_path)))
      return false;

   path_remove_extension(database);

   p_coreinfo                    = coreinfo_get_ptr();

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

   free(database);
   return false;
}

bool core_info_list_get_display_name(core_info_list_t *core_info_list,
      const char *core_path, char *s, size_t len)
{
   core_info_t *info = core_info_find_internal(
         core_info_list, core_path);

   if (s &&
       info &&
       !string_is_empty(info->display_name))
   {
      strlcpy(s, info->display_name, len);
      return true;
   }

   return false;
}

/* Returns core_info parameters required for
 * core updater tasks, read from specified file.
 * Returned core_updater_info_t object must be
 * freed using core_info_free_core_updater_info().
 * Returns NULL if 'path' is invalid. */
core_updater_info_t *core_info_get_core_updater_info(const char *info_path)
{
   struct config_entry_list 
      *entry                 = NULL;
   bool tmp_bool             = false;
   core_updater_info_t *info = NULL;
   config_file_t *conf       = NULL;

   if (string_is_empty(info_path))
      return NULL;

   /* Read config file */
   conf = config_file_new_from_path_to_string(info_path);

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
   {
      info->display_name     = entry->value;
      entry->value           = NULL;
   }

   /* > description */
   entry                     = config_get_entry(conf, "description");

   if (entry && !string_is_empty(entry->value))
   {
      info->description      = entry->value;
      entry->value           = NULL;
   }

   /* > licenses */
   entry                     = config_get_entry(conf, "license");

   if (entry && !string_is_empty(entry->value))
   {
      info->licenses         = entry->value;
      entry->value           = NULL;
   }

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
   core_info_t *core_info = NULL;
   bool lock_file_exists  = false;
   char lock_file_path[PATH_MAX_LENGTH];

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
   if (!core_info_find(core_path, &core_info))
      return false;

   if (string_is_empty(core_info->path))
      return false;

   /* Get lock file path */
   strlcpy(lock_file_path, core_info->path,
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
   core_info->is_locked = lock;

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
   core_info_t *core_info     = NULL;
   const char *core_file_path = NULL;
   bool is_locked             = false;
   char lock_file_path[PATH_MAX_LENGTH];

   lock_file_path[0] = '\0';

#if defined(ANDROID)
   /* Play Store builds do not support
    * core locking */
   if (play_feature_delivery_enabled())
      return false;
#endif

   if (string_is_empty(core_path))
      return false;

   /* Check whether core path is to be validated */
   if (validate_path)
   {
      if (core_info_find(core_path, &core_info))
         core_file_path = core_info->path;
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
   if (validate_path && core_info)
      core_info->is_locked = is_locked;

   return is_locked;
}
